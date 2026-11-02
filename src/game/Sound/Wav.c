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

/* reads a dword, which must be id unless id is 0; 0 or -1 */
int Wav_ReadChunk(cAbstractFile *file, uint32_t *out, uint32_t id) {
	uint32_t v;

	if (file->vftable->Read(file, &v, 4) != 4) {
		CHECK(*crt_errno());
		return -1;
	}
	if (id && v != id) {
		*crt_errno() = E_FRMT_NO_VAL;
		return -1;
	}
	*out = v;
	return 0;
}

int Wav_ReadInfo(cAbstractFile *file, cWavInfo *info) {
	uint32_t v, tmp;
	cWavFmt fmt;

	int saved = *crt_errno();
	CHECK(file && info);
	*crt_errno() = E_FRMT_NO_VAL;
	if (file->vftable->Read(file, &v, 4) != 4) {
		goto readerr;
	}
	if (v != WAV_RIFF) {
		*crt_errno() = E_FRMT_NO_VAL;
		goto done;
	}
	if (file->vftable->Read(file, &v, 4) != 4) {
		goto readerr;
	}
	if (Wav_ReadChunk(file, &tmp, WAV_WAVE)) {
		goto done;
	}
	if (file->vftable->Read(file, &fmt, sizeof(fmt)) != (int)sizeof(fmt)) {
		return -1;
	}
	if (fmt.id != WAV_FMT || fmt.format != 1 || fmt.size < 0x10) {
		goto done;
	}
	if (fmt.size != 0x10) {
		int pos = file->vftable->Tell(file);
		if (file->vftable->Seek(file, pos + (int)(fmt.size - 0x10)) == -1) {
			goto done;
		}
	}
	if (Wav_ReadChunk(file, &tmp, WAV_DATA) || Wav_ReadChunk(file, &v, 0)) {
		goto done;
	}
	uint32_t samples = v / fmt.blockAlign;
	if (v % fmt.blockAlign || (fmt.channels != 1 && fmt.channels != 2) || (fmt.bits != 8 && fmt.bits != 16)) {
		goto done;
	}
	info->rate = (int)fmt.rate;
	info->channels = fmt.channels;
	info->samples = (int)samples;
	info->bits16 = fmt.bits == 16;
	CHECK(*crt_errno() == E_FRMT_NO_VAL);
	*crt_errno() = 0;
	goto done;
readerr:
	CHECK(*crt_errno());
done:
	if (*crt_errno()) {
		return -1;
	}
	*crt_errno() = saved;
	return 0;
}
