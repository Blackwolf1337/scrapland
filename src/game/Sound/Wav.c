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
	uint32_t id;
	uint32_t size;
	uint16_t format;		/* 1 PCM */
	uint16_t channels;
	uint32_t rate;
	uint32_t byteRate;
	uint16_t blockAlign;
	uint16_t bits;
} cWavFmt;

