/*
 * The video player of the engine3d: a .avi or .vds file (AVI with XviD video and PCM sound) decoded by FFmpeg
 * (libavformat, libavcodec, libswscale; tools/ffmpeg.sh builds them), its frames drawn through the render device and
 * its sound played by a stream of the sound mixer, until a key, the pad or the end of the file. The message loop drives it, one
 * Video_Step per pass, which never waits: it decodes up to the frame that is due and draws that.
 */
#include <string.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

#include "scrap/crt.h"
#include "scrap/app.h"
#include "scrap/console.h"
#include "scrap/d3d8.h"
#include "scrap/e3dutil.h"
#include "scrap/filesys.h"
#include "scrap/mathlib.h"
#include "scrap/misc.h"
#include "scrap/rdev.h"
#include "scrap/render.h"
#include "scrap/scrapmain.h"
#include "scrap/snd.h"
#include "scrap/video.h"
#include "scrap/platform.h"

#define VK_ESCAPE 0x1b
#define VK_SPACE 0x20
#define VK_RETURN 0xd
#define DIK_ESCAPE 0x01
#define DIK_RETURN 0x1c
#define DIK_SPACE 0x39
#define DIK_NUMPADENTER 0x9c
#define XINPUT_GAMEPAD_A_B 0x3000


#define VIDEO_PACKETS 32		/* packets read per Video_Step at most */
#define VIDEO_RING (1u << 20)		/* bytes of decoded sound ahead of the mixer, about 6 s of 44.1 kHz stereo */

extern int g_VideoPadHeld;
#define g_VideoMirror (*(uint32_t *)&GAME_GVAR(GVar_PlayVideoFX)->value)	/* 1: up-down, 2: left-right */

byte Video_PadPressed(void) {
	uint32_t state[4] = { 0, 0, 0, 0 };

	if (Plat_PadState(0, state)) {
		return 0;
	}
	if (state[1] & XINPUT_GAMEPAD_A_B) {
		if (!g_VideoPadHeld) {
			g_VideoPadHeld = 1;
		}
		return 0;
	}
	if (g_VideoPadHeld == 1) {
		g_VideoPadHeld = 0;
		return 1;
	}
	return 0;
}

/* the hold of Video_WaitKeysUp: on until its end (timeGetTime) or until Escape, Space and Enter are all up */
static byte video_hold;
static uint32_t video_holdEnd;

byte Video_KeysHeld(void) {
	if (video_hold && (Plat_Ticks() >= video_holdEnd || (!Plat_KeyDown(VK_ESCAPE)
		&& !Plat_KeyDown(VK_SPACE) && !Plat_KeyDown(VK_RETURN)))) {
		video_hold = 0;
	}
	return video_hold;
}

void Video_WaitKeysUp(int ms) {
	video_holdEnd = Plat_Ticks() + (uint32_t)ms;
	video_hold = 1;
	Video_KeysHeld();
}

void Video_MaskHeldKeys(char *keys) {
	if (Video_KeysHeld()) {
		keys[DIK_ESCAPE] = 0;
		keys[DIK_RETURN] = 0;
		keys[DIK_SPACE] = 0;
		keys[DIK_NUMPADENTER] = 0;
	}
}

static struct {
	byte busy;			/* a video plays, Video_Step goes on with it */
	byte stream;			/* the file is open and its frames run */
	byte played;			/* stopped by a key or the pad */
	byte eof;			/* every packet read */
	byte pending;			/* frame holds a decoded picture not yet shown */
	byte started;			/* the first picture is shown: t0 and the sound run */
	AVFormatContext *fmt;
	AVCodecContext *vdec, *adec;
	int vidx, aidx;
	AVPacket *pkt;
	AVFrame *frame, *aframe;
	struct SwsContext *sws;
	void *tex;			/* the picture as an X8R8G8B8 texture of the render device */
	int w, h;
	uint32_t t0;			/* timeGetTime of pts 0 */
	double pts;			/* of the pending picture, seconds */
	SndStream *sound;		/* the sound stream, NULL without sound */
	const char *rest;		/* the name after the '|' of Video_PlayFile, played after the first one */
	const char *restDir;
	int result;			/* of the Video_PlayFile that ended last */
} video;

/*
 * The decoded sound between Video_Step (the only writer of head) and the thread of the sound mixer (the only writer of
 * tail): the bytes from tail to head are unplayed.
 */
static struct {
	byte data[VIDEO_RING];
	uint32_t head, tail;
} video_ring;

static void Video_RingPut(const byte *p, uint32_t n) {
	uint32_t head = video_ring.head;
	uint32_t room = VIDEO_RING - (head - __atomic_load_n(&video_ring.tail, __ATOMIC_ACQUIRE));
	if (n > room) {
		n = room;		/* the sound runs too far ahead: the rest is lost */
	}
	for (uint32_t i = 0; i < n; i++) {
		video_ring.data[(head + i) & (VIDEO_RING - 1)] = p[i];
	}
	__atomic_store_n(&video_ring.head, head + n, __ATOMIC_RELEASE);
}

/* the mixer asks for len bytes (on its thread); silence where the ring has none */
static void Video_SoundCallback(void *buf, int len, void *user) {
	uint32_t tail = video_ring.tail;
	uint32_t have = __atomic_load_n(&video_ring.head, __ATOMIC_ACQUIRE) - tail;
	uint32_t n = (uint32_t)len < have ? (uint32_t)len : have;
	byte *out = buf;

	(void)user;
	for (uint32_t i = 0; i < n; i++) {
		out[i] = video_ring.data[(tail + i) & (VIDEO_RING - 1)];
	}
	memset(out + n, 0, (size_t)len - n);
	__atomic_store_n(&video_ring.tail, tail + n, __ATOMIC_RELEASE);
}

static AVCodecContext *Video_OpenCodec(int idx) {
	const AVCodecParameters *par = video.fmt->streams[idx]->codecpar;
	const AVCodec *codec = avcodec_find_decoder(par->codec_id);
	AVCodecContext *c = codec ? avcodec_alloc_context3(codec) : NULL;

	if (!c || avcodec_parameters_to_context(c, par) < 0 || avcodec_open2(c, codec, NULL) < 0) {
		avcodec_free_context(&c);
	}
	return c;
}

/* everything of the file closed, the sound stopped */
static void Video_Close(void) {
	if (video.sound) {
		Snd_StreamClose(video.sound);
		video.sound = NULL;
	}
	if (video.tex) {
		Rd_Release(video.tex);
		video.tex = NULL;
	}
	sws_freeContext(video.sws);
	video.sws = NULL;
	av_frame_free(&video.frame);
	av_frame_free(&video.aframe);
	av_packet_free(&video.pkt);
	avcodec_free_context(&video.vdec);
	avcodec_free_context(&video.adec);
	avformat_close_input(&video.fmt);
	video.stream = 0;
}

/* the file opened with its decoders, the texture and the sound stream; 0 when it cannot play */
static int Video_Open(const char *path) {
	void *dev = g_Engine ? g_Engine->device : NULL;

	if (!dev) {
		E3D_LogBlue(g_FileLog, "ERROR: no render device for the video");
		return 0;
	}
	char sys[0x400];
	crt_syspath(path, sys, sizeof(sys));	/* FFmpeg opens the file itself */
	if (avformat_open_input(&video.fmt, sys, NULL, NULL) < 0 || avformat_find_stream_info(video.fmt, NULL) < 0) {
		E3D_LogBlue(g_FileLog, "ERROR: the video does not open: %s", path);
		return 0;
	}
	video.vidx = av_find_best_stream(video.fmt, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
	video.aidx = av_find_best_stream(video.fmt, AVMEDIA_TYPE_AUDIO, -1, video.vidx, NULL, 0);
	if (video.vidx < 0 || !(video.vdec = Video_OpenCodec(video.vidx))) {
		E3D_LogBlue(g_FileLog, "ERROR: no video stream that decodes in %s", path);
		return 0;
	}
	video.w = video.vdec->width;
	video.h = video.vdec->height;
	video.pkt = av_packet_alloc();
	video.frame = av_frame_alloc();
	video.aframe = av_frame_alloc();
	video.sws = sws_getContext(video.w, video.h, video.vdec->pix_fmt, video.w, video.h, AV_PIX_FMT_BGRA,
		SWS_POINT, NULL, NULL, NULL);
	if (!video.pkt || !video.frame || !video.aframe || !video.sws
		|| Rd_DevCreateTexture(dev, (uint32_t)video.w, (uint32_t)video.h, 1, 0, D3DFMT_X8R8G8B8, D3DPOOL_MANAGED,
			&video.tex) < 0) {
		return 0;
	}
	E3D_LogDebug(g_FileLog, "  video: %d x %d, %s", video.w, video.h, avcodec_get_name(video.vdec->codec_id));
	/* the sound: 16 bit PCM through a mixer stream that reads the ring; none without the mixer */
	video_ring.head = video_ring.tail = 0;
	if (video.aidx >= 0 && (video.adec = Video_OpenCodec(video.aidx)) != NULL) {
		int ch = video.adec->ch_layout.nb_channels;
		if (video.adec->sample_fmt == AV_SAMPLE_FMT_S16 && (ch == 1 || ch == 2)) {
			/* the intro videos play before the sound system starts: the mixer starts here then (Snd_Init once) */
			video.sound = Snd_Init() ? Snd_StreamPcm(video.adec->sample_rate, ch, Video_SoundCallback, NULL) : NULL;
		}
		if (!video.sound) {
			avcodec_free_context(&video.adec);
		}
	}
	video.eof = 0;
	video.pending = 0;
	video.started = 0;
	return 1;
}

/* the picture in frame into the texture, mirrored by PlayVideoFX */
static void Video_Upload(void) {
	d3dLockedRect_t lr;

	if (Rd_TexLockRect(video.tex, 0, &lr, NULL, 0) < 0) {
		return;
	}
	byte *dst = lr.pBits;
	int pitch = lr.Pitch;
	if (g_VideoMirror & 1) {
		dst += (size_t)(video.h - 1) * (size_t)pitch;
		pitch = -pitch;
	}
	sws_scale(video.sws, (const uint8_t *const *)video.frame->data, video.frame->linesize, 0, video.h, &dst, &pitch);
	if (g_VideoMirror & 2) {
		for (int y = 0; y < video.h; y++) {
			uint32_t *row = (uint32_t *)((byte *)lr.pBits + (size_t)y * (size_t)lr.Pitch);
			for (int a = 0, b = video.w - 1; a < b; a++, b--) {
				uint32_t t = row[a];
				row[a] = row[b];
				row[b] = t;
			}
		}
	}
	Rd_TexUnlockRect(video.tex, 0);
}

/* the texture on the back buffer with bars that keep the ratio of the video, and the present */
static void Video_Draw(void) {
	void *dev = g_Engine->device, *back = NULL;
	d3dSurfaceDesc_t desc;

	if (Rd_DevGetBackBuffer(dev, 0, 0, &back) < 0) {
		return;
	}
	int r = Rd_SurfGetDesc(back, &desc);
	Rd_Release(back);
	if (r < 0 || !desc.Width || !desc.Height) {
		return;
	}
	float sx = (float)desc.Height * (float)video.w / ((float)video.h * (float)desc.Width);
	float bx = sx < 1.0f ? (1.0f - sx) * 0.5f : 0.0f;
	float by = sx > 1.0f ? (1.0f - 1.0f / sx) * 0.5f : 0.0f;
	const box_t rect = { { bx, by, 0.0f }, { 1.0f - bx, 1.0f - by, 0.0f } };
	Rd_DevBeginScene(dev);
	Rd_DevClear(dev, 0, NULL, D3DCLEAR_TARGET, 0, 1.0f, 0);
	Render_DrawTexRect(&rect, 0xffffffffu, 0xffffffffu, video.tex);
	Rd_DevEndScene(dev);
	Rd_DevPresent(dev, NULL, NULL, NULL, NULL);
}

/* the decoded sound of the packet into the ring */
static void Video_DecodeSound(void) {
	if (avcodec_send_packet(video.adec, video.pkt) < 0) {
		return;
	}
	while (avcodec_receive_frame(video.adec, video.aframe) >= 0) {
		int bytes = video.aframe->nb_samples * video.adec->ch_layout.nb_channels * 2;
		Video_RingPut(video.aframe->data[0], (uint32_t)bytes);
	}
}

/*
 * The next picture in frame (pending): the decoded ones first, then up to VIDEO_PACKETS packets, whose sound goes
 * into the ring. 0 when none is there yet in this pass, or at the end of the file (eof then).
 */
static int Video_NextPicture(void) {
	for (int n = 0;;) {
		int r = avcodec_receive_frame(video.vdec, video.frame);
		if (r >= 0) {
			int64_t ts = video.frame->best_effort_timestamp;
			video.pts = ts == AV_NOPTS_VALUE ? 0.0 : (double)ts * av_q2d(video.fmt->streams[video.vidx]->time_base);
			video.pending = 1;
			return 1;
		}
		if (r == AVERROR_EOF || video.eof || n++ == VIDEO_PACKETS) {
			return 0;
		}
		if (av_read_frame(video.fmt, video.pkt) < 0) {
			video.eof = 1;
			avcodec_send_packet(video.vdec, NULL);	/* the frames the decoder still holds */
			continue;
		}
		if (video.pkt->stream_index == video.vidx) {
			avcodec_send_packet(video.vdec, video.pkt);
		} else if (video.pkt->stream_index == video.aidx && video.adec) {
			Video_DecodeSound();
		}
		av_packet_unref(video.pkt);
	}
}

/* the end of the stream: the hold of the keys, the file closed */
static void Video_StreamStop(byte skipped) {
	if (skipped) {
		video.played = 1;
	}
	Video_WaitKeysUp(1000);
	Video_Close();
}

/* one pass: the keys and the pad, the pictures that are due decoded, the last of them drawn */
static void Video_StreamStep(void) {
	if (!Video_KeysHeld() && (Plat_KeyDown(VK_ESCAPE) || Plat_KeyDown(VK_SPACE) || Plat_KeyDown(VK_RETURN))) {
		Video_StreamStop(1);
		return;
	}
	if (Video_PadPressed()) {
		Video_StreamStop(1);
		return;
	}
	byte show = 0;
	for (;;) {
		if (!video.pending && !Video_NextPicture()) {
			break;
		}
		if (!video.started) {
			/* the clock and the sound start with the first picture */
			video.t0 = Plat_Ticks() - (uint32_t)(video.pts * 1000.0);
			video.started = 1;
			if (video.sound) {
				Snd_StreamPlay(video.sound, 0);
			}
		}
		if (video.pts * 1000.0 > (double)(Plat_Ticks() - video.t0)) {
			break;			/* not due yet */
		}
		/* due: only the last due picture of this pass is drawn */
		video.pending = 0;
		show = 1;
		Video_Upload();
	}
	if (show) {
		Video_Draw();
	}
	if (video.eof && !video.pending && !show && !Video_NextPicture()) {
		Video_StreamStop(0);
	}
}

/* name in the language directory of dir, else as it is (with the warning when neither is found) */
static int Video_PlayName(const char *name, const char *dir) {
	char buf[MAX_FILEPATH];

	Bprintf(buf, sizeof(buf), "&U&D%s/&N&E", *g_LangName);
	Path_Format(buf, name, buf);
	int r = Video_PlayPath(buf, dir, 0);
	if (!r) {
		r = Video_PlayPath(name, dir, 1);
	}
	return r;
}

int Video_PlayFile(const char *name, const char *dir) {
	char buf[MAX_FILEPATH];

	CHECK(!video.busy);
	/* "a|b": a first, then b */
	if (strchr(name, '|')) {
		Bprintf(buf, sizeof(buf), "%s", name);
		*strchr(buf, '|') = 0;
		int r = Video_PlayFile(buf, dir);
		name = strchr(name, '|') + 1;
		while (*name && *name == ' ') {
			name++;
		}
		if (r == VIDEO_PENDING) {
			/* b plays after the video of a, from Video_Step */
			video.rest = name;
			video.restDir = dir;
			return VIDEO_PENDING;
		}
	}
	return Video_PlayName(name, dir);
}

int Video_PlayPath(const char *name, const char *dir, byte warn) {
	char path[MAX_FILEPATH];

	Bprintf(path, sizeof(path), "%s%s", dir ? dir : "", name);
	if (File_Access(path, 0)) {
		Path_Format(path, path, "&U&D&N.vds");
		if (File_Access(path, 0)) {
			if (warn) {
				Con_Warnf("ERROR: video file not found [%s]\n", path);
			}
			return 0;
		}
	}
	E3D_LogPrint(g_FileLog, "VIDEO: %s", path);
	video.played = 0;
	if (!Video_Open(path)) {
		Video_Close();
		return 1;
	}
	Video_WaitKeysUp(1000);
	video.stream = 1;
	video.busy = 1;
	return VIDEO_PENDING;
}

void Video_Step(void) {
	if (!video.busy) {
		return;
	}
	if (video.stream) {
		Video_StreamStep();
		if (video.stream) {
			return;
		}
	}
	int r = video.played ? 2 : 1;
	if (video.rest) {
		const char *name = video.rest;
		video.rest = NULL;
		r = Video_PlayName(name, video.restDir);
		if (r == VIDEO_PENDING) {
			return;
		}
	}
	video.result = r;
	video.busy = 0;
}

byte Video_Busy(void) {
	return video.busy;
}

int Video_Result(void) {
	return video.result;
}
