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

/* the system and the streams (engine3d/mpeg/music_fmod.cpp) */
#define FSOUND_OUTPUT_DSOUND 2
#define FSOUND_STREAM_OPENING (-2)	/* FSOUND_Stream_GetOpenState while the stream opens */

float __stdcall FSOUND_GetVersion(void);
byte __stdcall FSOUND_SetOutput(int type);
int __stdcall FSOUND_GetOutput(void);
int __stdcall FSOUND_GetNumDrivers(void);
const char *__stdcall FSOUND_GetDriverName(int id);
byte __stdcall FSOUND_GetDriverCaps(int id, uint32_t *caps);
byte __stdcall FSOUND_SetDriver(int driver);
byte __stdcall FSOUND_Init(int mixrate, int maxchannels, uint32_t flags);
void __stdcall FSOUND_Close(void);
int __stdcall FSOUND_GetMixer(void);
double __stdcall FSOUND_GetCPUUsage(void);	/* a float in st0, the original stores it as a double */
byte __stdcall FSOUND_SetReserved(int channel, byte reserved);
int __stdcall FSOUND_GetVolume(int channel);
int __stdcall FSOUND_GetPan(int channel);
int __stdcall FSOUND_GetFrequency(int channel);
int __stdcall FSOUND_GetNumSubChannels(int channel);
const char *__stdcall FSOUND_Sample_GetName(void *sample);
byte __stdcall FSOUND_Stream_SetBufferSize(int ms);
void *__stdcall FSOUND_Stream_Open(const char *name, uint32_t mode, int offset, int length);
byte __stdcall FSOUND_Stream_Close(void *stream);
int __stdcall FSOUND_Stream_GetOpenState(void *stream);
void *__stdcall FSOUND_Stream_GetSample(void *stream);
int __stdcall FSOUND_Stream_PlayEx(int channel, void *stream, void *dsp, byte paused);
int __stdcall FSOUND_Stream_GetLengthMs(void *stream);
int __stdcall FSOUND_Stream_GetTime(void *stream);

#endif
