/*
 * The deterministic test mode, see include/scrap/testdet.h. Not in the original, test tools only.
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "scrap/crt.h"
#include "scrap/snd.h"
#include "scrap/testdet.h"

#define TESTDET_TICKS 10000000		/* ticks of Plat_Counter a second */
#define TESTDET_START 1000			/* the clock at the start, in seconds (never 0: a time of 0 means "not set" to
									 * some of the game's timers) */
#define TESTDET_SEED 1				/* the seed of the MSVC runtime at its start */

static int det_fps = -1;			/* -1: not asked yet, 0: off */
static uint32_t det_frame;
static uint32_t det_loads;
static uint32_t det_loadFrame;
static FILE *det_trace;				/* the file of SCRAP_TEST_DET_TRACE */
int g_TestDetTrace;

int TestDet_Fps(void) {
	if (det_fps < 0) {
		const char *e = getenv("SCRAP_TEST_DETERMINISTIC");
		int n = e ? atoi(e) : 0;

		det_fps = n == 1 ? TESTDET_FPS : n > 1 && n <= 1000 ? n : 0;
		e = getenv("SCRAP_TEST_DET_TRACE");
		if (det_fps && e && *e && (det_trace = fopen(e, "w")) != NULL) {
			g_TestDetTrace = 1;
		}
	}
	return det_fps;
}

int TestDet_On(void) {
	return TestDet_Fps() != 0;
}

int64_t TestDet_Counter(void) {
	return (int64_t)TESTDET_START * TESTDET_TICKS + (int64_t)det_frame * TESTDET_TICKS / TestDet_Fps();
}

uint32_t TestDet_Frame(void) {
	return det_frame;
}

void TestDet_Trace(const char *fmt, ...) {
	va_list ap;

	if (!det_trace) {
		return;
	}
	fprintf(det_trace, "%u ", det_frame);
	va_start(ap, fmt);
	vfprintf(det_trace, fmt, ap);
	va_end(ap);
	fputc('\n', det_trace);
	/* the test tools end the game with a kill */
	fflush(det_trace);
}

void TestDet_Tick(void) {
	det_frame++;
	TestDet_Trace("step");
	Snd_MixTo(det_frame, (uint32_t)TestDet_Fps());
}

void TestDet_Seed(int load) {
	if (!TestDet_On()) {
		return;
	}
	crt_srand(TESTDET_SEED);
	if (load) {
		det_loads++;
		det_loadFrame = det_frame;
		TestDet_Trace("level loaded");
	}
}

uint32_t TestDet_Loads(void) {
	return det_loads;
}

uint32_t TestDet_LoadFrame(void) {
	return det_loadFrame;
}
