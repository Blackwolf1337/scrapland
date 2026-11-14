/*
 * The feel log of the multiplayer tests (stage M10 of issue #1, tools/test/parity/feel.py). Not in the original.
 * SCRAP_FEELLOG=<file>: a CSV line per frame of a rendered game, single player or client alike, written after the
 * frame was drawn: the monotonic clock of the system (ns, CLOCK_MONOTONIC on Linux: the tool's clock for its key
 * presses), the frame time, the own entity's kind (C character, S ship, - none), its drawn position, the camera's
 * position and yaw, the walk/accelerate input of the frame and the corrections of the prediction so far.
 */
#ifndef SCRAP_FEELLOG_H
#define SCRAP_FEELLOG_H

/* after App_Frame of the main loop; nothing without SCRAP_FEELLOG */
void FeelLog_Frame(void);

#endif
