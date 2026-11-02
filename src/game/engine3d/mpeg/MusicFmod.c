/*
 * engine3d/mpeg/music_fmod.cpp of the game: the music stream and the voice streams, on the sound mixer
 * (include/scrap/snd.h) in place of FMOD; see include/scrap/musicfmod.h.
 */
#include <string.h>

#include "scrap/gvar.h"
#include "scrap/app.h"
#include "scrap/console.h"
#include "scrap/crt.h"
#include "scrap/e3dutil.h"
#include "scrap/filepak.h"
#include "scrap/mathlib.h"
#include "scrap/musicfmod.h"
#include "scrap/scrapfont.h"
#include "scrap/scrapmain.h"
#include "scrap/snd.h"
#include "scrap/sound.h"
#include "scrap/misc.h"
#include "scrap/platform.h"

#define MUSIC_SHOW_DEBUG 2			/* S_Show */

cGVar GVar_MusicMainVolume;

/* engine variables */
#define g_MusicMainVolume (GVar_MusicMainVolume.value.f)
cGVar GVar_VoiceMainVolume;

#define g_VoiceMainVolume (GVar_VoiceMainVolume.value.f)
extern cGVar GVar_S_Show;
#define g_S_Show (GVar_S_Show.value.i)
cGVar GVar_S_StreamBufferSize;

#define g_S_StreamBufferSize (GVar_S_StreamBufferSize.value.i)	/* ms */
cGVar GVar_S_VoiceoverFadein;

#define g_S_VoiceoverFadein (GVar_S_VoiceoverFadein.value.f)
cGVar GVar_S_VoiceoverFadeout;

#define g_S_VoiceoverFadeout (GVar_S_VoiceoverFadeout.value.f)
extern cGVar GVar_S_VoiceoverVolume;
#define g_S_VoiceoverVolume (GVar_S_VoiceoverVolume.value.f)
/* the frame of the engine */
extern float g_Fps;

/* the music */
/* the voices */

/* |now - since| as the original computes it (cdq, xor, sub) */
static int Time_Since(uint32_t since) {
	uint32_t d = Plat_Ticks() - since;
	uint32_t m = (uint32_t)((int32_t)d >> 31);

	return (int)((d ^ m) - m);
}

/* the voices of the static array */
cMusicVoice *cMusicVoice_ctor(cMusicVoice *self) {
	self->name[0] = 0;
	self->stream = NULL;
	self->channel = -1;
	self->text[0] = 0;
	self->voiceover = 0;
	return self;
}

void cMusicVoice_dtor(cMusicVoice *self) {
	(void)self;
}

int g_FmodState;

/* the FMOD start */

int Fmod_Init(void) {
	if (g_FmodState > 0) {
		return 1;
	}
	if (g_FmodState < 0) {
		return 0;
	}
	E3D_LogPrint((&g_FileLog), "Sound Init");
	if (!Snd_Init()) {
		E3D_LogBlue((&g_FileLog), "SOUND ERROR: %s", Fmod_ErrorString(Snd_GetError()));
		g_FmodState = -1;
		return 0;
	}
	E3D_LogPrint((&g_FileLog), "Sound output: %s", Snd_DeviceName());
	g_FmodState = 1;
	return 1;
}

int Fmod_Close(void) {
	if (g_FmodState > 0) {
		Snd_Close();
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

/* differs from the original on purpose (no blocking waits): the voices open at once (Voice_Open), there is nothing
 * to wait for; the original polled the open state in Sleep(10) steps */
void Voice_Wait(cMusicVoice *v, const char *what) {
	(void)v;
	(void)what;
}

int g_NumVoices;

int g_NumVoiceovers;

void Voice_Close(cMusicVoice *v) {
	if (v->stream) {
		Snd_StreamClose(v->stream);
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
		Snd_SetVolume(v->channel, (int)(Volume_Clamp(volume) * g_VoiceMainVolume * 255.0f));
	}
}

int Voice_GetString(int voice, char *buf, int size) {
	cMusicVoice *v;

	*buf = '\0';
	v = Voice_Get(voice);
	if (v && v->stream) {
		String_Copy(buf, v->text, size);
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
	if (v->channel < 0) {
		return 1;
	}
	return Snd_IsPlaying(v->channel) != 0;
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
	Path_Format(path, name, "&U&D&N.ogg");
	Voice_Close(v);
	/* differs from the original on purpose (no blocking waits): the stream opens here and now instead of in the
	 * background; the scripts read the length (Voice_Remain) right after PlayVoice, the original waited there */
	if (Pack_GetFileInfo(path, pack, &offset, &size)) {
		Path_Format(path, path, "PACK:&N&E");
		v->stream = Snd_StreamOpen(pack, (uint32_t)offset, (uint32_t)size, 0);
	} else {
		v->stream = Snd_StreamOpen(path, 0, 0, 0);
	}
	if (!v->stream) {
		E3D_LogBlue((&g_FileLog), "Error: %s", Fmod_ErrorString(Snd_GetError()));
		return 0;
	}
	t = Plat_Ticks();
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
		Snd_SetPan(v->channel, (int)(p * 255.0f));
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
		v->channel = Snd_StreamPlay(v->stream, 1);
		freq = Snd_GetFrequency(v->channel);
		if (g_TimerFreqMult > 1) {
			Snd_SetFrequency(v->channel, freq / g_TimerFreqMult);
		}
		if (!g_isDolby) {
			Snd_SetPan(v->channel, 0x80);
		}
		Voice_ApplyVolume(v, 1.0f);
		Snd_SetPaused(v->channel, 0);
		v->playTime = Plat_Ticks();
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
	len = Snd_StreamLengthMs(v->stream);
	t = Snd_StreamTimeMs(v->stream);
	if (t >= len) {
		t = len;
	}
	*remain = (float)(len - t) * 0.001f;
	return 1;
}

int Voice_SetString(int voice, const char *text) {
	cMusicVoice *v = Voice_Get(voice);

	if (v && v->stream) {
		String_Copy(v->text, text, 0x80);
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

byte g_VoicesPaused;

void Voices_UpdatePause(void) {
	byte paused = World_TimeScale() == 0.0f || !g_pApp->active;

	if (g_VoicesPaused == paused) {
		return;
	}
	g_VoicesPaused = paused;
	for (uint32_t i = 0; i < MUSIC_MAX_VOICES; i++) {
		cMusicVoice *v = Voice_Get(i);
		if (v && v->stream && v->channel >= 0) {
			Snd_SetPaused(v->channel, g_VoicesPaused);
		}
	}
}

void D3DApp_OnActivate(void) {
	if (g_FmodState > 0) {
		Voices_UpdatePause();
	}
}

CFont *g_FontTerminal8;

void Voices_DrawDebug(int x, int y) {
	float yy;
	int playingCount = 0;
	uint32_t color;
	byte playing;

	if (g_S_Show != MUSIC_SHOW_DEBUG) {
		return;
	}
	yy = (float)y;
	CFont_PrintfColorShadow(g_FontTerminal8, (float)x, yy, 0xffff00, "Voices:");
	for (uint32_t i = 0; i < MUSIC_MAX_VOICES; i++) {
		cMusicVoice *v = Voice_Get(i);
		if (!v) {
			continue;
		}
		yy = yy + 10.0f;
		CFont_PrintfColorShadow(g_FontTerminal8, (float)x, yy, 0xff00, "%2d %08xh %u", i, v->stream, v->voiceover);
		color = v->stream ? 0xffff00 : 0x808000;
		if (v->name[0]) {
			CFont_PrintfColorShadow(g_FontTerminal8, g_FontTerminal8->x, yy, color, "  [%s]",
				Path_FileName(v->name));
		}
		CFont_PrintfColorShadow(g_FontTerminal8, g_FontTerminal8->x, yy, color, "  %d %d", v->waitTime - v->openTime,
			v->playTime - v->openTime);
		Music_DrawStream(g_FontTerminal8->x, yy, v->stream, v->channel, &playing);
		if (v->text[0]) {
			CFont_PrintfColorShadow(g_FontTerminal8, g_FontTerminal8->x, yy, color, "  [%s]", v->text);
		}
		if (playing) {
			playingCount++;
		}
	}
	CFont_PrintfColorShadow(g_FontTerminal8, (float)x, yy + 10.0f, 0xffff00, "channels: %d/%d", playingCount,
		MUSIC_MAX_VOICES);
	uint32_t memCur, memMax;
	Snd_MemoryStats(&memCur, &memMax);
	CFont_PrintfColorShadow(g_FontTerminal8, (float)x, yy + 20.0f, 0xffff00, "memory: %d %d", memCur >> 10,
		memMax >> 10);
}

byte Voices_DrawDebugDefault(void) {
	Voices_DrawDebug(10, 0xb4);
	return 1;
}

void *g_MusicStream;

uint32_t g_MusicOpenTime;

int g_MusicChannel = -1;

/* the music */

byte Music_Ready(void) {
	if (!g_MusicStream) {
		return 0;
	}
	if (g_MusicChannel != -1) {
		return 1;
	}
	/* the original started the stream one second after the open at the earliest */
	if (Time_Since(g_MusicOpenTime) < 1000) {
		return 0;
	}
	return Music_Start();
}

float g_RealFrameSecs;

float g_MusicCurMainVolume;

float g_MusicVolume = 1.0f;

float g_MusicVoiceover = 1.0f;

float g_MusicVoiceoverTarget = 1.0f;

float g_MusicVoiceoverSpeed = 1.0f;

float g_MusicFadeTarget = 1.0f;

float g_MusicFadeSpeed = 1.0f;

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

char g_MusicName[260];

uint32_t g_MusicWaitTime;

void Music_DrawDebug(int x, int y) {
	float yy;
	uint32_t color, t;
	byte playing;

	if (g_S_Show != MUSIC_SHOW_DEBUG) {
		return;
	}
	CFont_PrintfColorShadow(g_FontTerminal8, (float)x, (float)y, 0xffff00, "Music:");
	yy = (float)y + 10.0f;
	CFont_PrintfColorShadow(g_FontTerminal8, (float)x, yy, 0xff00, "%2d %08xh %u", 0, g_MusicStream, 0);
	color = g_MusicStream ? 0xffff00 : 0x808000;
	if (g_MusicName[0]) {
		CFont_PrintfColorShadow(g_FontTerminal8, g_FontTerminal8->x, yy, color, "  [%s]", g_MusicName);
	}
	t = g_MusicWaitTime;
	CFont_PrintfColorShadow(g_FontTerminal8, g_FontTerminal8->x, yy, color, "  %d", t - g_MusicOpenTime);
	Music_DrawStream(g_FontTerminal8->x, yy, g_MusicStream, g_MusicChannel, &playing);
	CFont_PrintfColorShadow(g_FontTerminal8, (float)x, yy + 10.0f, 0xffff00, "main_vol: %.2f  vol: %.2f",
		(double)g_MusicCurMainVolume, (double)g_MusicVolume);
}

/* differs from the original on purpose (no blocking waits): the original waited in Sleep(10) steps until FMOD had
 * opened the stream and Music_Ready had started it (one second after the open at the earliest); Music_Stop closes a
 * stream that is not ready without starting it */
byte Music_Wait(void) {
	return Music_Ready();
}

int g_MusicPaused;

byte Music_Open(const char *name) {
	char pack[0x80];
	int offset, size;
	uint32_t t;

	if (g_FmodState <= 0) {
		return 0;
	}
	CHECK(!g_MusicStream);
	if (Pack_GetFileInfo(name, pack, &offset, &size)) {
		g_MusicStream = Snd_StreamOpen(pack, (uint32_t)offset, (uint32_t)size, 1);
	} else {
		g_MusicStream = Snd_StreamOpen(name, 0, 0, 1);
	}
	if (!g_MusicStream) {
		E3D_LogBlue((&g_FileLog), "Error: %s", Fmod_ErrorString(Snd_GetError()));
		return 0;
	}
	g_MusicChannel = -1;
	g_MusicPaused = 0;
	t = Plat_Ticks();
	g_MusicOpenTime = t;
	g_MusicWaitTime = t;
	Bprintf(g_MusicName, 0x104, "%s", name);
	return 1;
}

void Music_SetPaused(int paused) {
	g_MusicPaused = paused;
	if (Music_Ready()) {
		Snd_SetPaused(g_MusicChannel, (byte)g_MusicPaused);
	}
}

byte Music_Start(void) {
	int freq;

	if (!g_MusicStream) {
		return 0;
	}
	E3D_LogDebug2((&g_FileLog), "Name      : %s", g_MusicName);
	E3D_LogDebug2((&g_FileLog), "Frequency : %d", Snd_StreamRate(g_MusicStream));
	g_MusicChannel = Snd_StreamPlay(g_MusicStream, 1);
	Snd_SetReserved(g_MusicChannel, 1);
	freq = Snd_GetFrequency(g_MusicChannel);
	if (g_TimerFreqMult > 1) {
		Snd_SetFrequency(g_MusicChannel, freq / g_TimerFreqMult);
	}
	if (!g_isDolby) {
		Snd_SetPan(g_MusicChannel, SND_PAN_STEREO);
	}
	Music_ApplyVolume();
	if (Music_Ready()) {
		Snd_SetPaused(g_MusicChannel, (byte)g_MusicPaused);
	}
	return 1;
}

void Music_ApplyVolume(void) {
	if (Music_Ready()) {
		Snd_SetVolume(g_MusicChannel,
			(int)(g_MusicVolume * g_MusicCurMainVolume * g_MusicVoiceover * 255.0f));
	}
}

byte Music_Voiceover(float factor, float speed) {
	E3D_LogDebug2((&g_FileLog), "music_voiceover: %.2f, %.2f, %.3f", (double)factor, (double)speed,
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
		CFont_PrintfColorShadow(g_FontTerminal8, x, y, 0x8000, "  ---");
		return;
	}
	if (channel < 0) {
		CFont_PrintfColorShadow(g_FontTerminal8, x, y, 0xff, "  waiting...");
		return;
	}
	vol = Snd_GetVolume(channel);
	pan = Snd_GetPan(channel);
	freq = Snd_GetFrequency(channel);
	isPlaying = Snd_IsPlaying(channel);
	color = isPlaying ? 0xff00 : 0x8000;
	CFont_PrintfColorShadow(g_FontTerminal8, x, y, color, "  vpf: %6d %6d %6d", vol, pan, freq);
	len = Snd_StreamLengthMs(stream);
	t = Snd_StreamTimeMs(stream);
	CFont_PrintfColorShadow(g_FontTerminal8, g_FontTerminal8->x, y, color, "  %.1f%%",
		(double)(((float)t * 100.0f) / (float)len));
	*playing = isPlaying != 0;
}

byte Music_PlayFile(const char *name, const char *dir) {
	char path[0x104];

	if (!name || !name[0]) {
		Music_SetPaused(0);
		E3D_LogDebug2((&g_FileLog), "Music unpaused");
		return 1;
	}
	Bprintf(path, 0x104, "%s%s", dir ? dir : "", name);
	E3D_LogPrint((&g_FileLog), "MUSIC: %s", path);
	if (g_MusicStream && crt_stricmp(g_MusicName, path) == 0) {
		Music_SetPaused(0);
		E3D_LogDebug2((&g_FileLog), "Music already started");
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
		Snd_SetReserved(g_MusicChannel, 0);
		Snd_StreamClose(g_MusicStream);
		g_MusicStream = NULL;
		g_MusicChannel = -1;
		return 1;
	}
	if (g_MusicStream) {
		/* differs from the original on purpose (no blocking waits): closed without the wait for the start */
		Snd_StreamClose(g_MusicStream);
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
	return 1;
}

/* empty callbacks of the module */

void Music_Nop(void) {
}

cGVar GVar_TimerFreqMult;

cMusicVoice cMusicVoice_MusicVoices[8];

cGVar GVar_isDolby;

