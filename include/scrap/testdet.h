/*
 * The deterministic test mode (src/game/Global/TestDet.c). Not in the original, test tools only. On with the
 * environment variable SCRAP_TEST_DETERMINISTIC=1 (read once, at the first question), off without it: then nothing
 * here changes the game.
 *
 * The mode makes the inputs of the game repeat, it does not change what the game computes from them:
 *   the clock     Plat_Ticks and Plat_Counter (all time of the game comes from them: the app time and the frame
 *                 delta, the world timers, the engine timers, the texts, the music fades, the net timers) give a clock
 *                 of the mode that starts at a fixed value and goes on by one fixed step per pass of the main loop
 *                 (TESTDET_FPS steps a second; SCRAP_TEST_DETERMINISTIC=<n> with n > 1: n steps a second), whatever
 *                 the real clock does. A level load takes no time on it.
 *   the random    crt_rand gets a fixed seed at the start and at the end of every level load (TestDet_Seed)
 *   the sound     the mixer is not run by the device thread but by the clock of the mode (Snd_MixTo), so a sound ends
 *                 after the same number of frames in every run
 *   the channel   the pace of the command files of the test channel is counted once per step of the clock; with
 *                 SCRAP_TEST_CHANNEL=step the clock stands while no command file waits (TestChannel_Hold), so a
 *                 command line of a test lands in the same frame in every run. The answers show the heap addresses
 *                 as numbers in the order of their first appearance
 *   the rest      on Linux the program starts itself again without the random layout of its address space (main.c);
 *                 frames are presented without waiting for the display; mouse motion from before the first step
 *                 does not count; the two loops of the net code that wait for a goodbye step the clock
 */
#ifndef SCRAP_TESTDET_H
#define SCRAP_TESTDET_H

#include <stdint.h>

#define TESTDET_FPS 60

/* the mode is on */
int TestDet_On(void);
/* steps a second */
int TestDet_Fps(void);
/* the clock of the mode in 100 ns ticks (the unit of Plat_Counter) */
int64_t TestDet_Counter(void);
/* the steps the clock made */
uint32_t TestDet_Frame(void);
/* one step of the clock (once per pass of the main loop) */
void TestDet_Tick(void);
/* the fixed seed for crt_rand; counts the level loads when load is set */
void TestDet_Seed(int load);
/* the level loads so far and TestDet_Frame() at the end of the last one */
uint32_t TestDet_Loads(void);
uint32_t TestDet_LoadFrame(void);

/* SCRAP_TEST_DET_TRACE=<file> with the mode on: what the game takes in and draws at random, to find where two runs
 * part (tools/test/regress/dettrace.py compares two such files). A line "<step> <what>" per step of the clock, per
 * crt_rand call (the return address of its caller and the state before), per window or input event, per command file
 * of the test channel and per level load. g_TestDetTrace is set while the file is open */
extern int g_TestDetTrace;
void TestDet_Trace(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#endif
