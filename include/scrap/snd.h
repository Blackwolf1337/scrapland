/*
 * The sound mixer of the game (src/game/Sound/Snd.c, on miniaudio for the output): the channels, samples and streams
 * that the game used FMOD 3 for, with the same model: SND_CHANNELS channels, each playing a sample or a stream with
 * a volume (0..255), a pan (0..255, 128 the middle, SND_PAN_STEREO for a stereo source as it is) and a frequency in
 * Hz; a channel can be reserved (the music), loop or be paused. The game does the 3D part itself (pan, volume and the
 * doppler frequency per channel).
 */
#ifndef SCRAP_SND_H
#define SCRAP_SND_H

#include "scrap/shared.h"

#define SND_CHANNELS 32
#define SND_RATE 44100			/* the output */
#define SND_PAN_STEREO (-1)
#define SND_PRIORITY 128		/* of every channel (FMOD's default) */

/* the error codes of Snd_GetError, numbered as FMOD 3's (Fmod_ErrorString has their texts) */
#define SND_ERR_NONE 0
#define SND_ERR_UNINITIALIZED 2
#define SND_ERR_INIT 3
#define SND_ERR_FILE_NOTFOUND 9
#define SND_ERR_FILE_FORMAT 10
#define SND_ERR_FILE_BAD 11
#define SND_ERR_MEMORY 12
#define SND_ERR_INVALID_PARAM 14
#define SND_ERR_CHANNEL_ALLOC 16

typedef struct SndSample SndSample;
typedef struct SndStream SndStream;
/* fills buf with bytes of 16 bit PCM (interleaved when stereo), on the mixer thread */
typedef void (*SndPcmCallback)(void *buf, int bytes, void *user);

/* the output (the null device when there is none, so the channels still run); 0 on failure */
int Snd_Init(void);
void Snd_Close(void);
int Snd_IsInit(void);
int Snd_GetError(void);
/* the name of the output backend and device, for the log */
const char *Snd_DeviceName(void);

/* a sample from a .wav file in memory (16 bit PCM, mono or stereo); NULL on failure */
SndSample *Snd_SampleLoad(const void *wav, int size);
void Snd_SampleFree(SndSample *s);
byte Snd_SampleStereo(const SndSample *s);
int Snd_SampleRate(const SndSample *s);
uint32_t Snd_SampleBytes(const SndSample *s);

/* channels: Snd_Play returns the lowest free channel that is not reserved, or -1 */
int Snd_Play(SndSample *s, byte paused);
byte Snd_Stop(int ch);
byte Snd_IsPlaying(int ch);
byte Snd_SetPaused(int ch, byte paused);
byte Snd_SetVolume(int ch, int volume);
byte Snd_SetPan(int ch, int pan);
byte Snd_SetFrequency(int ch, int freq);
byte Snd_SetLoop(int ch, byte loop);
byte Snd_SetReserved(int ch, byte reserved);
int Snd_GetVolume(int ch);
int Snd_GetPan(int ch);
int Snd_GetFrequency(int ch);
byte Snd_GetLoop(int ch);
byte Snd_GetReserved(int ch);
/* the sample a channel plays, NULL for none or a stream */
SndSample *Snd_ChannelSample(int ch);
int Snd_ChannelsPlaying(void);
/* the bytes of the loaded samples now and at most */
void Snd_MemoryStats(uint32_t *cur, uint32_t *max);

/* a stream of an .ogg (or .wav) file, or of size bytes from offset in it (a file in a pack); NULL on failure */
SndStream *Snd_StreamOpen(const char *path, uint32_t offset, uint32_t size, byte loop);
/* a stream that cb fills with 16 bit PCM of channels at rate */
SndStream *Snd_StreamPcm(int rate, int channels, SndPcmCallback cb, void *user);
/* plays the stream on a free channel (from its start), the channel or -1 */
int Snd_StreamPlay(SndStream *s, byte paused);
void Snd_StreamStop(SndStream *s);
void Snd_StreamClose(SndStream *s);
int Snd_StreamRate(const SndStream *s);
/* the length of a file stream and how much of it has played, ms */
int Snd_StreamLengthMs(const SndStream *s);
int Snd_StreamTimeMs(const SndStream *s);

#endif
