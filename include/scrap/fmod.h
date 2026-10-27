/*
 * The FMOD 3 functions (fmod.dll) the sound code calls, through the import thunks of the exe (stdcall). Booleans
 * are signed char in al; the ones whose eax the original passes on are declared int.
 */
#ifndef SCRAP_FMOD_H
#define SCRAP_FMOD_H

#include "scrap/game.h"

#define FSOUND_FREE (-1)		/* any free channel */
#define FSOUND_UNMANAGED (-2)		/* a sample index not managed by FMOD */
#define FSOUND_LOOP_OFF 1
#define FSOUND_LOOP_NORMAL 2
#define FSOUND_STEREO 0x40		/* mode bit of a sample */

int __stdcall FSOUND_GetError(void);
int __stdcall FSOUND_GetMaxChannels(void);
byte __stdcall FSOUND_GetMemoryStats(uint32_t *current, uint32_t *max);
void *__stdcall FSOUND_Sample_Load(int index, const void *data, uint32_t mode, int offset, int length);
void __stdcall FSOUND_Sample_Free(void *sample);
byte __stdcall FSOUND_Sample_GetDefaults(void *sample, int *freq, int *vol, int *pan, int *pri);
uint32_t __stdcall FSOUND_Sample_GetMode(void *sample);
int __stdcall FSOUND_PlaySoundEx(int channel, void *sample, void *dsp, byte paused);
int __stdcall FSOUND_StopSound(int channel);
byte __stdcall FSOUND_SetFrequency(int channel, int freq);
byte __stdcall FSOUND_SetVolume(int channel, int vol);
byte __stdcall FSOUND_SetPan(int channel, int pan);
byte __stdcall FSOUND_SetPaused(int channel, byte paused);
byte __stdcall FSOUND_SetLoopMode(int channel, uint32_t mode);
byte __stdcall FSOUND_IsPlaying(int channel);
byte __stdcall FSOUND_GetReserved(int channel);
void *__stdcall FSOUND_GetCurrentSample(int channel);

#endif
