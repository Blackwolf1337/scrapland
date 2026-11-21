/*
 * The feel log of the multiplayer tests (stage M10 of issue #1, tools/test/parity/feel.py). Not in the original.
 * SCRAP_FEELLOG=<file>: a CSV line per frame of a rendered game, single player or client alike: the monotonic clock
 * of the system (ns, CLOCK_MONOTONIC on Linux: the tool's clock for its key presses) when the frame's state was drawn
 * (the engine draws a frame's update at the start of the next frame, App_Render before App_Update), the time from the
 * drawing before, the own entity's kind (C character, S ship, - none), its drawn position, the camera's
 * position and yaw, the walk/accelerate input of the frame, the corrections of the prediction so far and the shots
 * of the own ship's weapons so far (the fire callbacks it ran).
 */
#ifndef SCRAP_FEELLOG_H
#define SCRAP_FEELLOG_H

struct cEntity;

/* after App_Frame of the main loop; nothing without SCRAP_FEELLOG */
void FeelLog_Frame(void);
/* the frame drawn now (before App_Render): the time of the line of the frame before */
void FeelLog_Render(void);
/* a weapon of the ship e fired (its fire callback ran): counted when e is the own entity */
void FeelLog_Fire(struct cEntity *e);

#endif
