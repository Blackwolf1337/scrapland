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
	char name[0x104];		/* the file */
	void *stream;			/* FSOUND_STREAM */
	int channel;			/* -1 until it plays */
	char text[0x80];		/* Voice_SetString */
	int voiceover;			/* 0/1 */
	uint32_t openTime;		/* timeGetTime when opened */
	uint32_t waitTime;		/* the last time it was seen opening (debug info) */
	uint32_t playTime;
} cMusicVoice;

int Fmod_Init(void);
int Fmod_Close(void);

int Voice_GetString(int voice, char *buf, int size);
int Voice_Unused(int voice);			/* CHECK(0), then whether it plays */
int Voice_Open(const char *name, int voice, int voiceover);
int Voice_SetParam(int voice, float pan);		/* -1..1 */
int Voice_Play(int voice);
int Voice_Remain(int voice, float *remain);	/* in s */
int Voice_SetString(int voice, const char *text);
int Voice_Stop(int voice);
int Voice_SetVolume(int voice, float volume);

byte Music_PlayFile(const char *name, const char *dir);
byte Music_Pause(void);
byte Music_Stop(void);
byte Music_SetVolume(float v);
byte Music_SetMainVolume(float v);
byte Music_FadeOut(float a, float vol);
byte Music_Update(void);

#endif
