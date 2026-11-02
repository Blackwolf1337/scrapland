/*
 * The sound mixer of the game, see include/scrap/snd.h. miniaudio only provides the output device (float stereo at
 * SND_RATE, or its null device when there is none); the mixing is here: each channel reads its sample or stream at
 * its frequency (linear interpolation), with its volume and pan. The game thread and the device thread share the
 * channels under snd_lock. Streams decode .ogg with stb_vorbis on the device thread, from the whole file in memory.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scrap/snd.h"
#include "sound/ma.h"

#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"

#define SND_STREAM_FRAMES 2048		/* frames a stream decodes at a time */

struct SndSample {
	int16_t *pcm;			/* interleaved */
	uint32_t frames;
	int channels;
	int rate;
};

struct SndStream {
	byte *file;			/* the whole .ogg (owned) */
	stb_vorbis *vorbis;
	SndSample *wav;			/* a .wav file stream: played as a sample */
	SndPcmCallback cb;
	void *user;
	int channels, rate;
	byte loop;
	uint32_t totalFrames;		/* of the file, 0 unknown */
	uint64_t played;		/* frames read since the start */
	float buf[SND_STREAM_FRAMES * 2];	/* the decoded frames, interleaved */
	uint32_t bufFrames, bufPos;
	byte ended;
	int channel;			/* where it plays, -1 */
};

typedef struct {
	byte used, paused, loop, reserved;
	SndSample *sample;
	SndStream *stream;
	double pos;			/* in frames of the source (sample) or of the stream buffer */
	int volume, pan, freq;
} SndChannel;

static struct {
	byte init;
	int error;
	ma_context ctx;
	byte ctxInit;
	ma_device device;
	ma_mutex lock;
	char name[320];
	SndChannel ch[SND_CHANNELS];
	uint32_t memCur, memMax;
} snd;

/* ---- the mixer (device thread) ---- */

/* the next frame of a stream (as float), 0 at the end; refills its buffer */
static int Snd_StreamFrame(SndStream *s, float *l, float *r) {
	if (s->bufPos >= s->bufFrames) {
		if (s->ended) {
			return 0;
		}
		s->bufPos = 0;
		s->bufFrames = 0;
		if (s->cb) {
			int16_t tmp[SND_STREAM_FRAMES * 2];
			int n = SND_STREAM_FRAMES / 4;
			s->cb(tmp, n * s->channels * 2, s->user);
			for (int i = 0; i < n * s->channels; i++) {
				s->buf[i] = (float)tmp[i] / 32768.0f;
			}
			s->bufFrames = (uint32_t)n;
		} else if (s->vorbis) {
			int n = stb_vorbis_get_samples_float_interleaved(s->vorbis, s->channels, s->buf,
				SND_STREAM_FRAMES * s->channels);
			if (n <= 0 && s->loop) {
				stb_vorbis_seek_start(s->vorbis);
				s->played = 0;
				n = stb_vorbis_get_samples_float_interleaved(s->vorbis, s->channels, s->buf,
					SND_STREAM_FRAMES * s->channels);
			}
			if (n <= 0) {
				s->ended = 1;
				return 0;
			}
			s->bufFrames = (uint32_t)n;
		}
	}
	const float *f = s->buf + (size_t)s->bufPos * (uint32_t)s->channels;
	*l = f[0];
	*r = s->channels > 1 ? f[1] : f[0];
	s->bufPos++;
	s->played++;
	return 1;
}

/* the frame at pos of a sample, interpolated; 0 past its end */
static int Snd_SampleFrame(const SndSample *s, double pos, float *l, float *r) {
	uint32_t i = (uint32_t)pos;
	if (i >= s->frames) {
		return 0;
	}
	float t = (float)(pos - (double)i);
	uint32_t j = i + 1 < s->frames ? i + 1 : i;
	const int16_t *a = s->pcm + (size_t)i * (uint32_t)s->channels, *b = s->pcm + (size_t)j * (uint32_t)s->channels;
	float al = a[0], bl = b[0];
	float ar = s->channels > 1 ? a[1] : al, br = s->channels > 1 ? b[1] : bl;
	*l = (al + (bl - al) * t) / 32768.0f;
	*r = (ar + (br - ar) * t) / 32768.0f;
	return 1;
}

/* the gains of a channel: mono sources by an equal power pan, stereo ones as they are or by balance */
static void Snd_Gains(const SndChannel *c, int stereo, float *gl, float *gr) {
	float v = (float)c->volume / 255.0f;
	if (stereo && c->pan == SND_PAN_STEREO) {
		*gl = *gr = v;
		return;
	}
	float p = c->pan < 0 ? 0.5f : (float)c->pan / 255.0f;
	if (stereo) {
		*gl = v * (p < 0.5f ? 1.0f : 2.0f * (1.0f - p));
		*gr = v * (p > 0.5f ? 1.0f : 2.0f * p);
		return;
	}
	*gl = v * cosf(p * 1.5707963f);
	*gr = v * sinf(p * 1.5707963f);
}

static void Snd_MixChannel(SndChannel *c, float *out, uint32_t frames) {
	SndSample *smp = c->sample ? c->sample : c->stream && c->stream->wav ? c->stream->wav : NULL;
	SndStream *st = c->stream && !c->stream->wav ? c->stream : NULL;
	int rate = smp ? smp->rate : st ? st->rate : SND_RATE;
	int stereo = smp ? smp->channels > 1 : st ? st->channels > 1 : 0;
	float gl, gr;
	Snd_Gains(c, stereo, &gl, &gr);
	double step = (double)c->freq / (double)SND_RATE;
	for (uint32_t f = 0; f < frames; f++) {
		float l, r;
		if (smp) {
			if (!Snd_SampleFrame(smp, c->pos, &l, &r)) {
				if (!c->loop || !smp->frames) {
					c->used = 0;
					return;
				}
				c->pos -= (double)smp->frames;
				if (!Snd_SampleFrame(smp, c->pos, &l, &r)) {
					c->used = 0;
					return;
				}
			}
			c->pos += step;
		} else {
			/* a stream: whole frames at the rate of the channel (pos counts the fraction) */
			c->pos += step;
			while (c->pos >= 1.0) {
				if (!Snd_StreamFrame(st, &l, &r)) {
					c->used = 0;
					return;
				}
				c->pos -= 1.0;
			}
			const float *b = st->buf + (size_t)(st->bufPos ? st->bufPos - 1 : 0) * (uint32_t)st->channels;
			l = b[0];
			r = st->channels > 1 ? b[1] : b[0];
		}
		out[2 * f] += l * gl;
		out[2 * f + 1] += r * gr;
	}
	(void)rate;
}

static void Snd_DeviceData(ma_device *dev, void *output, const void *input, ma_uint32 frames) {
	float *out = output;

	(void)dev, (void)input;
	memset(out, 0, (size_t)frames * 2 * sizeof(float));
	ma_mutex_lock(&snd.lock);
	for (int i = 0; i < SND_CHANNELS; i++) {
		SndChannel *c = &snd.ch[i];
		if (c->used && !c->paused) {
			Snd_MixChannel(c, out, frames);
			if (!c->used && c->stream) {
				c->stream->channel = -1;
				c->stream = NULL;
			}
		}
	}
	ma_mutex_unlock(&snd.lock);
}

/* ---- the output ---- */

int Snd_Init(void) {
	if (snd.init) {
		return 1;
	}
	ma_device_config cfg = ma_device_config_init(ma_device_type_playback);
	cfg.playback.format = ma_format_f32;
	cfg.playback.channels = 2;
	cfg.sampleRate = SND_RATE;
	cfg.dataCallback = Snd_DeviceData;
	if (ma_mutex_init(&snd.lock) != MA_SUCCESS) {
		snd.error = SND_ERR_INIT;
		return 0;
	}
	/* the device of the system, else the null device: no sound, but the channels play on as with a card */
	int ok = ma_context_init(NULL, 0, NULL, &snd.ctx) == MA_SUCCESS;
	if (ok && ma_device_init(&snd.ctx, &cfg, &snd.device) != MA_SUCCESS) {
		ma_context_uninit(&snd.ctx);
		ok = 0;
	}
	if (!ok) {
		ma_backend null = ma_backend_null;
		if (ma_context_init(&null, 1, NULL, &snd.ctx) != MA_SUCCESS
			|| ma_device_init(&snd.ctx, &cfg, &snd.device) != MA_SUCCESS) {
			ma_mutex_uninit(&snd.lock);
			snd.error = SND_ERR_INIT;
			return 0;
		}
	}
	snd.ctxInit = 1;
	snprintf(snd.name, sizeof(snd.name), "%s: %s", ma_get_backend_name(snd.ctx.backend), snd.device.playback.name);
	if (ma_device_start(&snd.device) != MA_SUCCESS) {
		ma_device_uninit(&snd.device);
		ma_context_uninit(&snd.ctx);
		ma_mutex_uninit(&snd.lock);
		snd.ctxInit = 0;
		snd.error = SND_ERR_INIT;
		return 0;
	}
	memset(snd.ch, 0, sizeof(snd.ch));
	snd.init = 1;
	snd.error = SND_ERR_NONE;
	return 1;
}

void Snd_Close(void) {
	if (!snd.init) {
		return;
	}
	ma_device_uninit(&snd.device);
	ma_context_uninit(&snd.ctx);
	ma_mutex_uninit(&snd.lock);
	snd.init = 0;
	snd.ctxInit = 0;
}

int Snd_IsInit(void) {
	return snd.init;
}

int Snd_GetError(void) {
	return snd.error;
}

const char *Snd_DeviceName(void) {
	return snd.init ? snd.name : "none";
}

/* ---- samples ---- */

static uint32_t Snd_U32(const byte *p) {
	return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

SndSample *Snd_SampleLoad(const void *wav, int size) {
	const byte *p = wav, *end = p + (size > 0 ? size : 0);
	const byte *fmt = NULL, *data = NULL;
	uint32_t dataLen = 0;

	if (size < 12 || memcmp(p, "RIFF", 4) || memcmp(p + 8, "WAVE", 4)) {
		snd.error = SND_ERR_FILE_FORMAT;
		return NULL;
	}
	for (p += 12; p + 8 <= end;) {
		uint32_t len = Snd_U32(p + 4);
		if (!memcmp(p, "fmt ", 4) && len >= 16) {
			fmt = p + 8;
		} else if (!memcmp(p, "data", 4)) {
			data = p + 8;
			dataLen = len <= (uint32_t)(end - data) ? len : (uint32_t)(end - data);
		}
		p += 8 + len + (len & 1);
	}
	if (!fmt || !data || (fmt[0] | fmt[1] << 8) != 1 || (fmt[14] | fmt[15] << 8) != 16) {
		snd.error = SND_ERR_FILE_FORMAT;		/* only 16 bit PCM, which is all the game has */
		return NULL;
	}
	int channels = fmt[2] | fmt[3] << 8;
	if (channels != 1 && channels != 2) {
		snd.error = SND_ERR_FILE_FORMAT;
		return NULL;
	}
	SndSample *s = calloc(1, sizeof(*s));
	uint32_t frames = dataLen / (uint32_t)(2 * channels);
	if (!s || !(s->pcm = malloc((size_t)frames * (uint32_t)channels * 2 + 4))) {
		free(s);
		snd.error = SND_ERR_MEMORY;
		return NULL;
	}
	memcpy(s->pcm, data, (size_t)frames * (uint32_t)channels * 2);
	s->frames = frames;
	s->channels = channels;
	s->rate = (int)Snd_U32(fmt + 4);
	snd.memCur += Snd_SampleBytes(s);
	snd.memMax = snd.memCur > snd.memMax ? snd.memCur : snd.memMax;
	return s;
}

void Snd_SampleFree(SndSample *s) {
	if (!s) {
		return;
	}
	if (snd.init) {
		ma_mutex_lock(&snd.lock);
		for (int i = 0; i < SND_CHANNELS; i++) {
			if (snd.ch[i].sample == s) {
				snd.ch[i].used = 0;
				snd.ch[i].sample = NULL;
			}
		}
		ma_mutex_unlock(&snd.lock);
	}
	snd.memCur -= Snd_SampleBytes(s);
	free(s->pcm);
	free(s);
}

byte Snd_SampleStereo(const SndSample *s) {
	return s && s->channels > 1;
}

int Snd_SampleRate(const SndSample *s) {
	return s ? s->rate : SND_RATE;
}

uint32_t Snd_SampleBytes(const SndSample *s) {
	return s ? s->frames * (uint32_t)s->channels * 2 : 0;
}

/* ---- channels ---- */

static SndChannel *Snd_Ch(int ch) {
	return snd.init && ch >= 0 && ch < SND_CHANNELS ? &snd.ch[ch] : NULL;
}

/* a free channel with the source set and the defaults, under the lock; -1 when all are busy or reserved */
static int Snd_Start(SndSample *smp, SndStream *st, int rate, byte paused) {
	if (!snd.init) {
		snd.error = SND_ERR_UNINITIALIZED;
		return -1;
	}
	ma_mutex_lock(&snd.lock);
	int i;
	for (i = 0; i < SND_CHANNELS && (snd.ch[i].used || snd.ch[i].reserved); i++) {
	}
	if (i == SND_CHANNELS) {
		ma_mutex_unlock(&snd.lock);
		snd.error = SND_ERR_CHANNEL_ALLOC;
		return -1;
	}
	SndChannel *c = &snd.ch[i];
	c->used = 1;
	c->paused = paused;
	c->loop = 0;
	c->sample = smp;
	c->stream = st;
	c->pos = 0.0;
	c->volume = 255;
	c->pan = 128;
	c->freq = rate;
	ma_mutex_unlock(&snd.lock);
	return i;
}

int Snd_Play(SndSample *s, byte paused) {
	if (!s) {
		snd.error = SND_ERR_INVALID_PARAM;
		return -1;
	}
	return Snd_Start(s, NULL, s->rate, paused);
}

byte Snd_Stop(int ch) {
	SndChannel *c = Snd_Ch(ch);
	if (!c) {
		return 0;
	}
	ma_mutex_lock(&snd.lock);
	c->used = 0;
	if (c->stream) {
		c->stream->channel = -1;
		c->stream = NULL;
	}
	c->sample = NULL;
	ma_mutex_unlock(&snd.lock);
	return 1;
}

byte Snd_IsPlaying(int ch) {
	SndChannel *c = Snd_Ch(ch);
	return c && c->used;
}

/* sets a field of a channel under the lock */
#define SND_SET(ch, field, value) do { \
	SndChannel *c_ = Snd_Ch(ch); \
	if (!c_) { \
		return 0; \
	} \
	ma_mutex_lock(&snd.lock); \
	c_->field = (value); \
	ma_mutex_unlock(&snd.lock); \
	return 1; \
} while (0)

byte Snd_SetPaused(int ch, byte paused) {
	SND_SET(ch, paused, paused != 0);
}

byte Snd_SetVolume(int ch, int volume) {
	SND_SET(ch, volume, volume < 0 ? 0 : volume > 255 ? 255 : volume);
}

byte Snd_SetPan(int ch, int pan) {
	SND_SET(ch, pan, pan == SND_PAN_STEREO ? SND_PAN_STEREO : pan < 0 ? 0 : pan > 255 ? 255 : pan);
}

byte Snd_SetFrequency(int ch, int freq) {
	SND_SET(ch, freq, freq < 100 ? 100 : freq > 705600 ? 705600 : freq);
}

byte Snd_SetLoop(int ch, byte loop) {
	SND_SET(ch, loop, loop != 0);
}

byte Snd_SetReserved(int ch, byte reserved) {
	SND_SET(ch, reserved, reserved != 0);
}

int Snd_GetVolume(int ch) {
	SndChannel *c = Snd_Ch(ch);
	return c ? c->volume : 0;
}

int Snd_GetPan(int ch) {
	SndChannel *c = Snd_Ch(ch);
	return c ? c->pan : 0;
}

int Snd_GetFrequency(int ch) {
	SndChannel *c = Snd_Ch(ch);
	return c ? c->freq : 0;
}

byte Snd_GetLoop(int ch) {
	SndChannel *c = Snd_Ch(ch);
	return c && c->loop;
}

byte Snd_GetReserved(int ch) {
	SndChannel *c = Snd_Ch(ch);
	return c && c->reserved;
}

SndSample *Snd_ChannelSample(int ch) {
	SndChannel *c = Snd_Ch(ch);
	return c && c->used ? c->sample : NULL;
}

int Snd_ChannelsPlaying(void) {
	int n = 0;
	for (int i = 0; snd.init && i < SND_CHANNELS; i++) {
		n += snd.ch[i].used && !snd.ch[i].paused;
	}
	return n;
}

void Snd_MemoryStats(uint32_t *cur, uint32_t *max) {
	*cur = snd.memCur;
	*max = snd.memMax;
}

/* ---- streams ---- */

SndStream *Snd_StreamOpen(const char *path, uint32_t offset, uint32_t size, byte loop) {
	FILE *f = fopen(path, "rb");
	if (!f) {
		snd.error = SND_ERR_FILE_NOTFOUND;
		return NULL;
	}
	if (!size) {
		fseek(f, 0, SEEK_END);
		long n = ftell(f);
		size = n > 0 ? (uint32_t)n : 0;
		offset = 0;
	}
	SndStream *s = calloc(1, sizeof(*s));
	byte *data = s ? malloc(size ? size : 1) : NULL;
	if (!data || fseek(f, (long)offset, SEEK_SET) || fread(data, 1, size, f) != size) {
		fclose(f);
		free(data);
		free(s);
		snd.error = data ? SND_ERR_FILE_BAD : SND_ERR_MEMORY;
		return NULL;
	}
	fclose(f);
	s->loop = loop;
	s->channel = -1;
	if (size >= 4 && !memcmp(data, "RIFF", 4)) {
		s->wav = Snd_SampleLoad(data, (int)size);
		free(data);
		if (!s->wav) {
			free(s);
			return NULL;
		}
		s->channels = s->wav->channels;
		s->rate = s->wav->rate;
		s->totalFrames = s->wav->frames;
		return s;
	}
	int err = 0;
	s->vorbis = stb_vorbis_open_memory(data, (int)size, &err, NULL);
	if (!s->vorbis) {
		free(data);
		free(s);
		snd.error = SND_ERR_FILE_FORMAT;
		return NULL;
	}
	stb_vorbis_info info = stb_vorbis_get_info(s->vorbis);
	s->file = data;
	s->channels = info.channels > 2 ? 2 : info.channels;
	s->rate = (int)info.sample_rate;
	s->totalFrames = stb_vorbis_stream_length_in_samples(s->vorbis);
	return s;
}

SndStream *Snd_StreamPcm(int rate, int channels, SndPcmCallback cb, void *user) {
	if (!cb || channels < 1 || channels > 2 || rate <= 0) {
		snd.error = SND_ERR_INVALID_PARAM;
		return NULL;
	}
	SndStream *s = calloc(1, sizeof(*s));
	if (!s) {
		snd.error = SND_ERR_MEMORY;
		return NULL;
	}
	s->cb = cb;
	s->user = user;
	s->channels = channels;
	s->rate = rate;
	s->channel = -1;
	return s;
}

int Snd_StreamPlay(SndStream *s, byte paused) {
	if (!s) {
		snd.error = SND_ERR_INVALID_PARAM;
		return -1;
	}
	Snd_StreamStop(s);
	if (s->vorbis) {
		stb_vorbis_seek_start(s->vorbis);
	}
	s->played = 0;
	s->bufFrames = s->bufPos = 0;
	s->ended = 0;
	int ch = Snd_Start(NULL, s, s->rate, paused);
	if (ch >= 0) {
		ma_mutex_lock(&snd.lock);
		snd.ch[ch].loop = s->loop;
		s->channel = ch;
		ma_mutex_unlock(&snd.lock);
	}
	return ch;
}

void Snd_StreamStop(SndStream *s) {
	if (s && s->channel >= 0) {
		Snd_Stop(s->channel);
	}
}

void Snd_StreamClose(SndStream *s) {
	if (!s) {
		return;
	}
	Snd_StreamStop(s);
	if (s->vorbis) {
		stb_vorbis_close(s->vorbis);
	}
	Snd_SampleFree(s->wav);
	free(s->file);
	free(s);
}

int Snd_StreamRate(const SndStream *s) {
	return s ? s->rate : SND_RATE;
}

int Snd_StreamLengthMs(const SndStream *s) {
	return s && s->rate ? (int)((uint64_t)s->totalFrames * 1000 / (uint32_t)s->rate) : 0;
}

int Snd_StreamTimeMs(const SndStream *s) {
	if (!s || !s->rate) {
		return 0;
	}
	uint64_t frames = s->played;
	if (s->wav && s->channel >= 0) {
		frames = (uint64_t)snd.ch[s->channel].pos;
	}
	return (int)(frames * 1000 / (uint32_t)s->rate);
}
