/*
 * engine3d/mpeg/music_fmod.cpp of the game: the start and end of the sound mixer (include/scrap/snd.h, FMOD in the
 * original), the music stream (one .ogg, started one second after its open; its volume is MusicMainVolume * the fade
 * volume * the voiceover factor) and the 8 voice streams (&U&D&N.ogg, the dialogs; a voiceover voice turns the music down while it is
 * open). The screen debug info of both with S_Show 2. The results are bytes in al; the ones the sound code passes
 * on in eax are declared int.
 */
#ifndef SCRAP_MUSICFMOD_H
#define SCRAP_MUSICFMOD_H

#include "scrap/game.h"

#define MUSIC_MAX_VOICES 8

typedef struct cMusicVoice {
	char name[0x104];		/* 0x000 the file */
	void *stream;			/* 0x104 FSOUND_STREAM */
	int channel;			/* 0x108 -1 until it plays */
	char text[0x80];		/* 0x10c Voice_SetString */
	int voiceover;			/* 0x18c 0/1 */
	uint32_t openTime;		/* 0x190 timeGetTime when opened */
	uint32_t waitTime;		/* 0x194 the last time it was seen opening (debug info) */
	uint32_t playTime;		/* 0x198 */
} cMusicVoice;
SD_SIZE(cMusicVoice, 0x19c);

extern int g_FmodState;		/* > 0 running, < 0 failed */
extern cMusicVoice cMusicVoice_MusicVoices[8];
#define g_MusicVoices (cMusicVoice_MusicVoices)

int Fmod_Init(void);
cMusicVoice *cMusicVoice_ctor(cMusicVoice *self);
void cMusicVoice_dtor(cMusicVoice *self);
int Fmod_Close(void);
int Fmod_IsInit(void);

/* the voices; NULL without the mixer */
cMusicVoice *Voice_Get(uint32_t voice);
void Voice_Wait(cMusicVoice *v, const char *what);	/* nothing: the voices open at once (Voice_Open) */
void Voice_Close(cMusicVoice *v);
void Voice_ApplyVolume(cMusicVoice *v, float volume);
int Voice_GetString(int voice, char *buf, int size);
int Voice_Unused(int voice);			/* CHECK(0), then whether it plays */
int Voice_Open(const char *name, int voice, int voiceover);
int Voice_SetParam(int voice, float pan);		/* -1..1 */
int Voice_Play(int voice);
int Voice_Remain(int voice, float *remain);	/* in s */
int Voice_SetString(int voice, const char *text);
int Voice_Stop(int voice);
int Voice_SetVolume(int voice, float volume);
void Voices_UpdatePause(void);			/* paused with the time scale 0 or the window inactive */
void Voices_DrawDebug(int x, int y);
byte Voices_DrawDebugDefault(void);

/* the music */
byte Music_Ready(void);				/* started one second after the open */
byte Music_UpdateVolumes(void);
void Music_DrawDebug(int x, int y);
byte Music_Wait(void);					/* Music_Ready, the original waited for the open */
byte Music_Open(const char *name);
void Music_SetPaused(int paused);
byte Music_Start(void);
void Music_ApplyVolume(void);
byte Music_Voiceover(float factor, float speed);
void Music_DrawStream(float x, float y, void *stream, int channel, byte *playing);
byte Music_PlayFile(const char *name, const char *dir);
byte Music_Pause(void);
byte Music_Stop(void);
byte Music_SetVolume(float v);
byte Music_SetMainVolume(float v);
byte Music_FadeOut(float a, float vol);
byte Music_Update(void);

/* an empty callback of the module */
void Music_Nop(void);

#endif
