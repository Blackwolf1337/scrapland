/*
 * The console view (src/game/Scorer/ConsoleView.c, not in the original): the console as idTech 3 engines have it,
 * over the scorer. The key next to 1 opens and closes it. While it is open the platform layer gives it the keys and
 * the typed text instead of the game: an input line with a cursor, the history, Tab completion of commands,
 * variables and arguments (Console_Complete), the scrollback of the console text (Con_BackLine).
 */
#ifndef SCRAP_CONSOLEVIEW_H
#define SCRAP_CONSOLEVIEW_H

#include "scrap/shared.h"

/* the keys the platform layer hands over (Plat_Key), besides the letters 'A'..'Z' for the Ctrl shortcuts */
enum {
	CVK_NONE = 0, CVK_ENTER = 0x100, CVK_ESC, CVK_TAB, CVK_BACKSPACE, CVK_DELETE, CVK_LEFT, CVK_RIGHT, CVK_UP, CVK_DOWN,
	CVK_HOME, CVK_END, CVK_PGUP, CVK_PGDN
};
#define CVK_CTRL 1
#define CVK_SHIFT 2

/* the key next to 1: open or close */
void ConsoleView_Toggle(void);
byte ConsoleView_IsOpen(void);
/* a key pressed while the console is open (key a CVK_ or a letter, mods CVK_CTRL and CVK_SHIFT) */
void ConsoleView_Key(int key, int mods);
/* a typed character, Windows-1252 */
void ConsoleView_Char(int c);
/* the mouse wheel, notches up (positive) or down */
void ConsoleView_Wheel(int notches);
/* Scorer_SetConsole closed it (the key, Escape, a change of the input set or of the level): its image goes */
void ConsoleView_Closed(void);
/* each frame after the scorer: the console sliding in or out, the text, the input line */
void ConsoleView_Draw(void);

#endif
