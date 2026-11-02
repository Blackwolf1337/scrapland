/*
 * Sound/Wav.cpp of the game: the header of a RIFF .wav file (PCM, 8 or 16 bit, mono or stereo). Errors are
 * reported in errno like the C library; E_FRMT_NO_VAL marks a file that is no such wav.
 */
#include "scrap/filesys.h"
#include "scrap/sound.h"

#define E_FRMT_NO_VAL 0x32

#define WAV_RIFF 0x46464952	/* "RIFF" */
#define WAV_WAVE 0x45564157	/* "WAVE" */
#define WAV_FMT 0x20746d66	/* "fmt " */
#define WAV_DATA 0x61746164	/* "data" */

/* the "fmt " chunk with its header */
typedef struct cWavFmt {
	uint32_t id;			/* 0x00 */
	uint32_t size;			/* 0x04 */
	uint16_t format;		/* 0x08 1 PCM */
	uint16_t channels;		/* 0x0a */
	uint32_t rate;			/* 0x0c */
	uint32_t byteRate;		/* 0x10 */
	uint16_t blockAlign;		/* 0x14 */
	uint16_t bits;			/* 0x16 */
} cWavFmt;

