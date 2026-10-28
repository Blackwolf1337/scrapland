/*
 * engine3d/mpeg/music_fmod.cpp of the game: FMOD, the music stream and the voice streams, see
 * include/scrap/musicfmod.h.
 */
#include <string.h>

#include "scrap/app.h"
#include "scrap/console.h"
#include "scrap/crt.h"
#include "scrap/e3dutil.h"
#include "scrap/filepak.h"
#include "scrap/fmod.h"
#include "scrap/mathlib.h"
#include "scrap/musicfmod.h"
#include "scrap/scrapfont.h"
#include "scrap/scrapmain.h"
#include "scrap/sound.h"
#include "scrap/varios.h"
#include "scrap/win32.h"

#define FMOD_VERSION 3.73f
#define MUSIC_MODE 0x41080132			/* FSOUND_LOOP_NORMAL | 2D | ... | NONBLOCKING */
#define FSOUND_NONBLOCKING 0x01000000
/* the same with FSOUND_LOOP_OFF; differs from the original on purpose (no blocking waits): without
 * FSOUND_NONBLOCKING, see Voice_Open */
#define VOICE_MODE (0x41080131 & ~FSOUND_NONBLOCKING)
#define MUSIC_SHOW_DEBUG 2			/* S_Show */

/* engine variables */
#define g_MusicMainVolume GAME_GLOBAL_IN(float, GVar_MusicMainVolume, 16, 0x466c70)
#define g_VoiceMainVolume GAME_GLOBAL_IN(float, GVar_VoiceMainVolume, 16, 0x466c10)
#define g_S_Show GAME_GLOBAL_IN(int, GVar_S_Show, 16, 0x466d90)
#define g_S_StreamBufferSize GAME_GLOBAL_IN(int, GVar_S_StreamBufferSize, 16, 0x4aa1e8)	/* ms */
#define g_S_VoiceoverFadein GAME_GLOBAL_IN(float, GVar_S_VoiceoverFadein, 16, 0x4a94bc)
#define g_S_VoiceoverFadeout GAME_GLOBAL_IN(float, GVar_S_VoiceoverFadeout, 16, 0x4a94d4)
#define g_S_VoiceoverVolume GAME_GLOBAL_IN(float, GVar_S_VoiceoverVolume, 16, 0x4aa1d0)
/* the frame of the engine */
#define g_RealFrameSecs GAME_GLOBAL(float, g_RealFrameSecs, 0x4a8ee4)
#define g_Fps GAME_GLOBAL(float, g_Fps, 0x431ba8)

/* the music */
#define g_MusicCurMainVolume GAME_GLOBAL(float, g_MusicCurMainVolume, 0x4a94a8)	/* approaches MusicMainVolume in 1 / fps steps */
#define g_MusicVolume GAME_GLOBAL(float, g_MusicVolume, 0x431d04)
#define g_MusicVoiceover GAME_GLOBAL(float, g_MusicVoiceover, 0x431d08)		/* the voiceover factor */
#define g_MusicChannel GAME_GLOBAL(int, g_MusicChannel, 0x431d0c)		/* -1 until it plays */
#define g_MusicVoiceoverTarget GAME_GLOBAL(float, g_MusicVoiceoverTarget, 0x431d10)
#define g_MusicVoiceoverSpeed GAME_GLOBAL(float, g_MusicVoiceoverSpeed, 0x431d14)
#define g_MusicFadeTarget GAME_GLOBAL(float, g_MusicFadeTarget, 0x431d18)
#define g_MusicFadeSpeed GAME_GLOBAL(float, g_MusicFadeSpeed, 0x431d1c)
#define g_MusicName (GAME_ARRAY(char, g_MusicName, 0x4aa1f0))		/* 0x104 */
#define g_MusicStream GAME_GLOBAL(void *, g_MusicStream, 0x4aa2f4)
#define g_MusicPaused GAME_GLOBAL(int, g_MusicPaused, 0x4aa2f8)
#define g_MusicOpenTime GAME_GLOBAL(uint32_t, g_MusicOpenTime, 0x4aa2fc)
#define g_MusicWaitTime GAME_GLOBAL(uint32_t, g_MusicWaitTime, 0x4aa300)
/* the voices */
#define g_NumVoices GAME_GLOBAL(int, g_NumVoices, 0x4aa304)
#define g_NumVoiceovers GAME_GLOBAL(int, g_NumVoiceovers, 0x4aa308)
#define g_VoicesPaused GAME_GLOBAL(byte, g_VoicesPaused, 0x4aa30c)

/* |now - since| as the original computes it (cdq, xor, sub) */
static int Time_Since(uint32_t since) {
	uint32_t d = exe_timeGetTime() - since;
	uint32_t m = (uint32_t)((int32_t)d >> 31);

	return (int)((d ^ m) - m);
}

/* the music streams Music_Stop let go while FMOD still opened them in the background: FMOD can not close a file
 * stream that opens (FSOUND_Stream_Close fails), Music_Update closes them once it has opened them */
#define MUSIC_MAX_CLOSING 8
static void *music_closing[MUSIC_MAX_CLOSING];

static void Music_CloseOpened(void) {
	for (int i = 0; i < MUSIC_MAX_CLOSING; i++) {
		if (music_closing[i] && FSOUND_Stream_GetOpenState(music_closing[i]) != FSOUND_STREAM_OPENING) {
			FSOUND_Stream_Close(music_closing[i]);
			music_closing[i] = NULL;
		}
	}
}

/* the voices of the static array */
cMusicVoice *__fastcall cMusicVoice_ctor(cMusicVoice *self) {
	self->name[0] = 0;
	self->stream = NULL;
	self->channel = -1;
	self->text[0] = 0;
	self->voiceover = 0;
	return self;
}

void __fastcall cMusicVoice_dtor(cMusicVoice *self) {
	(void)self;
}

/* the FMOD start */

int Fmod_Init(void) {
	static const char *const outputs[] = {
		"NoSound", "Windows Multimedia Waveout", "Direct Sound", "A3D", "Open Sound System",
		"Enlightenment Sound Daemon", "ALSA", "ASIO", "XBOX", "PS2", "MAC", "GC",
	};
	uint32_t caps;
	float version;
	int output;

	if (g_FmodState > 0) {
		return 1;
	}
	if (g_FmodState < 0) {
		return 0;
	}
	version = FSOUND_GetVersion();
	if (version < FMOD_VERSION) {
		Con_FatalErrorf("Error: You are using the wrong DLL version!  You should be using FMOD %.02f",
			(double)FMOD_VERSION);
		g_FmodState = -1;
		return 0;
	}
	FSOUND_SetOutput(FSOUND_OUTPUT_DSOUND);
	E3D_LogPrint(g_FileLog, "FMOD Init");
	output = FSOUND_GetOutput();
	if ((uint32_t)output < sizeof(outputs) / sizeof(outputs[0])) {
		E3D_LogPrint(g_FileLog, outputs[output]);
	}
	E3D_LogPrint(g_FileLog, " Driver list");
	/* the caps share the stack slot of the version in the original */
	memcpy(&caps, &version, sizeof(caps));
	for (int i = 0; i < FSOUND_GetNumDrivers(); i++) {
		FSOUND_GetDriverCaps(i, &caps);
		E3D_LogPrint(g_FileLog, "  %d - %s  [caps: %04xh]", i + 1, FSOUND_GetDriverName(i), caps);
	}
	FSOUND_SetDriver(0);
	if (!FSOUND_Init(44100, 32, 0)) {
		E3D_LogBlue(g_FileLog, "SOUND ERROR: %s", Fmod_ErrorString(FSOUND_GetError()));
		FSOUND_Close();
		g_FmodState = -1;
		return 0;
	}
	E3D_LogPrint(g_FileLog, "Mixer: %d", FSOUND_GetMixer());
	FSOUND_Stream_SetBufferSize(g_S_StreamBufferSize);
	g_FmodState = 1;
	return 1;
}

int Fmod_Close(void) {
	if (g_FmodState > 0) {
		Music_CloseOpened();
		memset(music_closing, 0, sizeof(music_closing));
		FSOUND_Close();
		g_FmodState = 0;
	}
	return 1;
}

int Fmod_IsInit(void) {
	return g_FmodState > 0;
}

/* the voices */

cMusicVoice *Voice_Get(uint32_t voice) {
	if (g_FmodState <= 0) {
		return NULL;
	}
	CHECK(voice < MUSIC_MAX_VOICES);
	return &g_MusicVoices[voice];
}

/* differs from the original on purpose (no blocking waits): the voices open without FSOUND_NONBLOCKING (Voice_Open),
 * there is nothing to wait for; the original polled the open state in Sleep(10) steps. Only for its rva */
void Voice_Wait(cMusicVoice *v, const char *what) {
	(void)v;
	(void)what;
}

void Voice_Close(cMusicVoice *v) {
	if (v->stream) {
		FSOUND_Stream_Close(v->stream);
		g_NumVoices--;
		g_NumVoiceovers -= v->voiceover;
		v->stream = NULL;
		v->channel = -1;
		if (g_NumVoiceovers == 0) {
			Music_Voiceover(1.0f, g_S_VoiceoverFadeout);
		}
	}
	v->text[0] = '\0';
	v->voiceover = 0;
}

/* 0..1; NaN stays NaN (comiss with jae, then minss) */
static float Volume_Clamp(float v) {
	return 0.0f >= v ? 0.0f : Float_MinSS(1.0f, v);
}

void Voice_ApplyVolume(cMusicVoice *v, float volume) {
	if (!v->stream) {
		return;
	}
	if (v->channel != -1) {
		FSOUND_SetVolume(v->channel, (int)(Volume_Clamp(volume) * g_VoiceMainVolume * 255.0f));
	}
}

int Voice_GetString(int voice, char *buf, int size) {
	cMusicVoice *v;

	*buf = '\0';
	v = Voice_Get(voice);
	if (v && v->stream) {
		Cad_Copy(buf, v->text, size);
		return 1;
	}
	return 0;
}

int Voice_Unused(int voice) {
	cMusicVoice *v;

	CHECK(0);
	v = Voice_Get(voice);
	if (!v || !v->stream) {
		return 0;
	}
	if (FSOUND_Stream_GetOpenState(v->stream) == FSOUND_STREAM_OPENING || v->channel < 0) {
		return 1;
	}
	return FSOUND_IsPlaying(v->channel) != 0;
}

int Voice_Open(const char *name, int voice, int voiceover) {
	char path[0x104];
	char pack[0x80];
	int offset, size;
	uint32_t t;
	int vo;
	cMusicVoice *v = Voice_Get(voice);

	if (!v) {
		return 0;
	}
	Ruta_Format(path, name, "&U&D&N.ogg");
	Voice_Close(v);
	/* differs from the original on purpose (no blocking waits): VOICE_MODE opens the stream here and now instead of in
	 * the background; the scripts read the length (Voice_Remain) right after PlayVoice, the original waited there */
	if (Pack_GetFileInfo(path, pack, &offset, &size)) {
		Ruta_Format(path, path, "PACK:&N&E");
		v->stream = FSOUND_Stream_Open(pack, VOICE_MODE, offset, size);
	} else {
		v->stream = FSOUND_Stream_Open(path, VOICE_MODE, 0, 0);
	}
	if (!v->stream) {
		E3D_LogBlue(g_FileLog, "Error: %s", Fmod_ErrorString(FSOUND_GetError()));
		return 0;
	}
	t = exe_timeGetTime();
	v->openTime = t;
	v->waitTime = t;
	v->playTime = t;
	vo = voiceover != 0;
	g_NumVoices++;
	g_NumVoiceovers += vo;
	v->voiceover = vo;
	if (g_NumVoiceovers != 0) {
		Music_Voiceover(g_S_VoiceoverVolume, g_S_VoiceoverFadein);
	}
	Bprintf(v->name, 0x104, "%s", path);
	return 1;
}

int Voice_SetParam(int voice, float pan) {
	cMusicVoice *v = Voice_Get(voice);
	float p;

	if (!v || !v->stream) {
		return 0;
	}
	if (v->channel == -1) {
		return 0;
	}
	p = Volume_Clamp((pan + 1.0f) * 0.5f);
	if (!g_isDolby) {
		FSOUND_SetPan(v->channel, (int)(p * 255.0f));
	}
	return 1;
}

int Voice_Play(int voice) {
	cMusicVoice *v = Voice_Get(voice);
	int freq;

	if (!v) {
		return 0;
	}
	if (v->stream && v->channel < 0) {
		if (FSOUND_Stream_GetOpenState(v->stream) != 0) {
			E3D_LogBlue(g_FileLog, "ERROR: voice file not found [%s]", v->name);
			Voice_Close(v);
			return 0;
		}
		v->channel = FSOUND_Stream_PlayEx(FSOUND_FREE, v->stream, NULL, 1);
		freq = FSOUND_GetFrequency(v->channel);
		if (g_TimerFreqMult > 1) {
			FSOUND_SetFrequency(v->channel, freq / g_TimerFreqMult);
		}
		if (!g_isDolby) {
			FSOUND_SetPan(v->channel, 0x80);
		}
		Voice_ApplyVolume(v, 1.0f);
		FSOUND_SetPaused(v->channel, 0);
		v->playTime = exe_timeGetTime();
	}
	return 1;
}

int Voice_Remain(int voice, float *remain) {
	cMusicVoice *v;
	int len, t;

	*remain = 0.0f;
	v = Voice_Get(voice);
	if (!v || !v->stream) {
		return 0;
	}
	len = FSOUND_Stream_GetLengthMs(v->stream);
	t = FSOUND_Stream_GetTime(v->stream);
	if (t >= len) {
		t = len;
	}
	*remain = (float)(len - t) * 0.001f;
	return 1;
}

int Voice_SetString(int voice, const char *text) {
	cMusicVoice *v = Voice_Get(voice);

	if (v && v->stream) {
		Cad_Copy(v->text, text, 0x80);
		return 1;
	}
	return 0;
}

int Voice_Stop(int voice) {
	cMusicVoice *v = Voice_Get(voice);

	if (!v) {
		return 0;
	}
	Voice_Close(v);
	return 1;
}

int Voice_SetVolume(int voice, float volume) {
	cMusicVoice *v = Voice_Get(voice);

	if (!v) {
		return 0;
	}
	Voice_ApplyVolume(v, volume);
	return 1;
}

void Voices_UpdatePause(void) {
	byte paused = World_TimeScale() == 0.0f || !g_pApp->active;

	if (g_VoicesPaused == paused) {
		return;
	}
	g_VoicesPaused = paused;
	for (uint32_t i = 0; i < MUSIC_MAX_VOICES; i++) {
		cMusicVoice *v = Voice_Get(i);
		if (v && v->stream && v->channel >= 0) {
			FSOUND_SetPaused(v->channel, g_VoicesPaused);
		}
	}
}

void D3DApp_OnActivate(void) {
	if (g_FmodState > 0) {
		Voices_UpdatePause();
	}
}

void Voices_DrawDebug(int x, int y) {
	float yy;
	int playingCount = 0;
	uint32_t color;
	byte playing;

	if (g_S_Show != MUSIC_SHOW_DEBUG) {
		return;
	}
	yy = (float)y;
	CFont_PrintfColorSombra(g_FontTerminal8, (float)x, yy, 0xffff00, "Voices:");
	for (uint32_t i = 0; i < MUSIC_MAX_VOICES; i++) {
		cMusicVoice *v = Voice_Get(i);
		if (!v) {
			continue;
		}
		yy = yy + 10.0f;
		CFont_PrintfColorSombra(g_FontTerminal8, (float)x, yy, 0xff00, "%2d %08xh %u", i, v->stream, v->voiceover);
		color = v->stream ? 0xffff00 : 0x808000;
		if (v->name[0]) {
			CFont_PrintfColorSombra(g_FontTerminal8, g_FontTerminal8->x, yy, color, "  [%s]",
				Ruta_FileName(v->name));
		}
		if (v->stream && FSOUND_Stream_GetOpenState(v->stream) == FSOUND_STREAM_OPENING) {
			v->waitTime = exe_timeGetTime();
		}
		CFont_PrintfColorSombra(g_FontTerminal8, g_FontTerminal8->x, yy, color, "  %d %d", v->waitTime - v->openTime,
			v->playTime - v->openTime);
		Music_DrawStream(g_FontTerminal8->x, yy, v->stream, v->channel, &playing);
		if (v->text[0]) {
			CFont_PrintfColorSombra(g_FontTerminal8, g_FontTerminal8->x, yy, color, "  [%s]", v->text);
		}
		if (playing) {
			playingCount++;
		}
	}
	double cpu = FSOUND_GetCPUUsage();
	CFont_PrintfColorSombra(g_FontTerminal8, (float)x, yy + 10.0f, 0xffff00, "channels: %d/%d  cpu %5.02f%%",
		playingCount, MUSIC_MAX_VOICES, cpu);
	uint32_t memCur, memMax;
	FSOUND_GetMemoryStats(&memCur, &memMax);
	CFont_PrintfColorSombra(g_FontTerminal8, (float)x, yy + 20.0f, 0xffff00, "memory: %d %d", memCur >> 10,
		memMax >> 10);
}

byte Voices_DrawDebugDefault(void) {
	Voices_DrawDebug(10, 0xb4);
	return 1;
}

/* the music */

int Music_GetOpenState(void) {
	int st = FSOUND_Stream_GetOpenState(g_MusicStream);

	if (st == 0 && Time_Since(g_MusicOpenTime) < 1000) {
		st = FSOUND_STREAM_OPENING;
	}
	return st;
}

byte Music_Ready(void) {
	int st;

	if (!g_MusicStream) {
		return 0;
	}
	if (g_MusicChannel != -1) {
		return 1;
	}
	st = FSOUND_Stream_GetOpenState(g_MusicStream);
	if (st == 0) {
		if (Time_Since(g_MusicOpenTime) < 1000) {
			return 0;
		}
		return Music_Start();
	}
	if (st == FSOUND_STREAM_OPENING) {
		return 0;
	}
	E3D_LogBlue(g_FileLog, "ERROR: music file not found [%s]", g_MusicName);
	FSOUND_Stream_Close(g_MusicStream);
	g_MusicStream = NULL;
	return 0;
}

byte Music_UpdateVolumes(void) {
	Music_Ready();
	Music_DrawDebug(10, 0x8c);
	if (g_MusicVoiceoverTarget != g_MusicVoiceover) {
		Float_Approach(&g_MusicVoiceover, g_MusicVoiceoverTarget, g_RealFrameSecs * g_MusicVoiceoverSpeed);
		Music_ApplyVolume();
	}
	if (g_MusicFadeTarget != g_MusicVolume) {
		Float_Approach(&g_MusicVolume, g_MusicFadeTarget, g_MusicFadeSpeed * g_RealFrameSecs);
		Music_ApplyVolume();
	}
	if (g_MusicMainVolume != g_MusicCurMainVolume) {
		float fps = Float_MaxSS(g_Fps, 10.0f);
		Float_Approach(&g_MusicCurMainVolume, g_MusicMainVolume, (float)(1.0 / (double)fps));
		Music_ApplyVolume();
	}
	return 1;
}

void Music_DrawDebug(int x, int y) {
	float yy;
	uint32_t color, t;
	byte playing;

	if (g_S_Show != MUSIC_SHOW_DEBUG) {
		return;
	}
	CFont_PrintfColorSombra(g_FontTerminal8, (float)x, (float)y, 0xffff00, "Music:");
	yy = (float)y + 10.0f;
	CFont_PrintfColorSombra(g_FontTerminal8, (float)x, yy, 0xff00, "%2d %08xh %u", 0, g_MusicStream, 0);
	color = g_MusicStream ? 0xffff00 : 0x808000;
	if (g_MusicName[0]) {
		CFont_PrintfColorSombra(g_FontTerminal8, g_FontTerminal8->x, yy, color, "  [%s]", g_MusicName);
	}
	if (g_MusicStream && FSOUND_Stream_GetOpenState(g_MusicStream) == FSOUND_STREAM_OPENING) {
		t = exe_timeGetTime();
		g_MusicWaitTime = t;
	} else {
		t = g_MusicWaitTime;
	}
	CFont_PrintfColorSombra(g_FontTerminal8, g_FontTerminal8->x, yy, color, "  %d", t - g_MusicOpenTime);
	Music_DrawStream(g_FontTerminal8->x, yy, g_MusicStream, g_MusicChannel, &playing);
	CFont_PrintfColorSombra(g_FontTerminal8, (float)x, yy + 10.0f, 0xffff00, "main_vol: %.2f  vol: %.2f",
		(double)g_MusicCurMainVolume, (double)g_MusicVolume);
}

/* differs from the original on purpose (no blocking waits): the original waited in Sleep(10) steps until FMOD had
 * opened the stream and Music_Ready had started it (one second after the open at the earliest); Music_Stop closes a
 * stream that is not ready without starting it */
byte Music_Wait(void) {
	return Music_Ready();
}

static void Music_CloseLater(void *stream) {
	Music_CloseOpened();
	for (int i = 0; i < MUSIC_MAX_CLOSING; i++) {
		if (!music_closing[i]) {
			music_closing[i] = stream;
			return;
		}
	}
	E3D_LogBlue(g_FileLog, "WARN: music stream left open");
}

byte Music_Open(const char *name) {
	char pack[0x80];
	int offset, size;
	uint32_t t;

	if (g_FmodState <= 0) {
		return 0;
	}
	CHECK(!g_MusicStream);
	if (Pack_GetFileInfo(name, pack, &offset, &size)) {
		g_MusicStream = FSOUND_Stream_Open(pack, MUSIC_MODE, offset, size);
	} else {
		g_MusicStream = FSOUND_Stream_Open(name, MUSIC_MODE, 0, 0);
	}
	if (!g_MusicStream) {
		E3D_LogBlue(g_FileLog, "Error: %s", Fmod_ErrorString(FSOUND_GetError()));
		return 0;
	}
	g_MusicChannel = -1;
	g_MusicPaused = 0;
	t = exe_timeGetTime();
	g_MusicOpenTime = t;
	g_MusicWaitTime = t;
	Bprintf(g_MusicName, 0x104, "%s", name);
	return 1;
}

void Music_SetPaused(int paused) {
	g_MusicPaused = paused;
	if (Music_Ready()) {
		FSOUND_SetPaused(g_MusicChannel, (byte)g_MusicPaused);
	}
}

byte Music_Start(void) {
	void *sample;
	int freq;

	if (!g_MusicStream) {
		return 0;
	}
	CHECK(FSOUND_Stream_GetOpenState(g_MusicStream) == 0);
	sample = FSOUND_Stream_GetSample(g_MusicStream);
	if (sample) {
		FSOUND_Sample_GetDefaults(sample, &freq, NULL, NULL, NULL);
		E3D_LogDebug2(g_FileLog, "Name      : %s", FSOUND_Sample_GetName(sample));
		E3D_LogDebug2(g_FileLog, "Frequency : %d", freq);
	}
	g_MusicChannel = FSOUND_Stream_PlayEx(FSOUND_FREE, g_MusicStream, NULL, 1);
	FSOUND_SetReserved(g_MusicChannel, 1);
	freq = FSOUND_GetFrequency(g_MusicChannel);
	if (g_TimerFreqMult > 1) {
		FSOUND_SetFrequency(g_MusicChannel, freq / g_TimerFreqMult);
	}
	if (!g_isDolby) {
		FSOUND_SetPan(g_MusicChannel, -1);	/* FSOUND_STEREOPAN */
	}
	Music_ApplyVolume();
	E3D_LogDebug2(g_FileLog, "channels  : %d", FSOUND_GetNumSubChannels(g_MusicChannel));
	if (Music_Ready()) {
		FSOUND_SetPaused(g_MusicChannel, (byte)g_MusicPaused);
	}
	return 1;
}

void Music_ApplyVolume(void) {
	if (Music_Ready()) {
		FSOUND_SetVolume(g_MusicChannel,
			(int)(g_MusicVolume * g_MusicCurMainVolume * g_MusicVoiceover * 255.0f));
	}
}

byte Music_Voiceover(float factor, float speed) {
	E3D_LogDebug2(g_FileLog, "music_voiceover: %.2f, %.2f, %.3f", (double)factor, (double)speed,
		(double)((float)__builtin_fabs((double)(factor - g_MusicVoiceover)) / Float_MaxSS(speed, 0.001f)));
	g_MusicVoiceoverTarget = factor;
	g_MusicVoiceoverSpeed = speed;
	Music_ApplyVolume();
	return 1;
}

void Music_DrawStream(float x, float y, void *stream, int channel, byte *playing) {
	int vol, pan, freq, len, t;
	byte isPlaying;
	uint32_t color;

	*playing = 0;
	if (!stream) {
		CFont_PrintfColorSombra(g_FontTerminal8, x, y, 0x8000, "  ---");
		return;
	}
	if (FSOUND_Stream_GetOpenState(stream) == FSOUND_STREAM_OPENING) {
		CFont_PrintfColorSombra(g_FontTerminal8, x, y, 0xff, "  opening...");
		return;
	}
	if (channel < 0) {
		CFont_PrintfColorSombra(g_FontTerminal8, x, y, 0xff, "  waiting...");
		return;
	}
	vol = FSOUND_GetVolume(channel);
	pan = FSOUND_GetPan(channel);
	freq = FSOUND_GetFrequency(channel);
	isPlaying = FSOUND_IsPlaying(channel);
	color = isPlaying ? 0xff00 : 0x8000;
	CFont_PrintfColorSombra(g_FontTerminal8, x, y, color, "  vpf: %6d %6d %6d", vol, pan, freq);
	len = FSOUND_Stream_GetLengthMs(stream);
	t = FSOUND_Stream_GetTime(stream);
	CFont_PrintfColorSombra(g_FontTerminal8, g_FontTerminal8->x, y, color, "  %.1f%%",
		(double)(((float)t * 100.0f) / (float)len));
	*playing = isPlaying != 0;
}

byte Music_PlayFile(const char *name, const char *dir) {
	char path[0x104];

	if (!name || !name[0]) {
		Music_SetPaused(0);
		E3D_LogDebug2(g_FileLog, "Music unpaused");
		return 1;
	}
	Bprintf(path, 0x104, "%s%s", dir ? dir : "", name);
	E3D_LogPrint(g_FileLog, "MUSIC: %s", path);
	if (g_MusicStream && crt_stricmp(g_MusicName, path) == 0) {
		Music_SetPaused(0);
		E3D_LogDebug2(g_FileLog, "Music already started");
		return 1;
	}
	Music_Stop();
	Music_Open(path);
	return 1;
}

byte Music_Pause(void) {
	Music_SetPaused(1);
	return 1;
}

byte Music_Stop(void) {
	if (Music_Wait()) {
		FSOUND_SetReserved(g_MusicChannel, 0);
		FSOUND_Stream_Close(g_MusicStream);
		g_MusicStream = NULL;
		g_MusicChannel = -1;
		return 1;
	}
	if (g_MusicStream) {
		/* differs from the original on purpose (no blocking waits): closed without the wait for the open and the
		 * start, by Music_Update while FMOD still opens it */
		if (FSOUND_Stream_GetOpenState(g_MusicStream) == FSOUND_STREAM_OPENING) {
			Music_CloseLater(g_MusicStream);
		} else {
			FSOUND_Stream_Close(g_MusicStream);
		}
		g_MusicStream = NULL;
		g_MusicChannel = -1;
	}
	CHECK(!g_MusicStream);
	return 1;
}

byte Music_SetVolume(float v) {
	g_MusicVolume = v;
	g_MusicFadeTarget = v;
	Music_ApplyVolume();
	return 1;
}

byte Music_SetMainVolume(float v) {
	g_MusicMainVolume = v;
	Music_ApplyVolume();
	return 1;
}

byte Music_FadeOut(float a, float vol) {
	g_MusicFadeTarget = a;
	g_MusicFadeSpeed = vol;
	Music_ApplyVolume();
	return 1;
}

byte Music_Update(void) {
	Music_UpdateVolumes();
	Voices_DrawDebug(10, 0xb4);
	Voices_UpdatePause();
	/* differs from the original on purpose (no blocking waits): the streams Music_Stop could not close yet */
	Music_CloseOpened();
	return 1;
}

/* the FMOD file callbacks */

int __stdcall Music_FileClose(void *f) {
	return crt_fclose(f);
}

void *__stdcall Music_FileOpen(const char *name) {
	return crt_fopen(name, "rb");
}

int __stdcall Music_FileRead(void *buf, int size, void *f) {
	return (int)crt_fread(buf, 1, size, f);
}

int __stdcall Music_FileSeek(void *f, int pos, signed char mode) {
	return crt_fseek(f, pos, mode);
}

int __stdcall Music_FileTell(void *f) {
	return crt_ftell(f);
}

/* empty callbacks of the module */

void Music_Nop(void) {
}

byte __stdcall Music_True16a(int a, int b, int c, int d) {
	(void)a;
	(void)b;
	(void)c;
	(void)d;
	return 1;
}

byte __stdcall Music_True16b(int a, int b, int c, int d) {
	(void)a;
	(void)b;
	(void)c;
	(void)d;
	return 1;
}
