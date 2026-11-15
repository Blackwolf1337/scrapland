/*
 * The debug menu of debug builds (src/game/Script/DebugMenu.c, CMake option SCRAP_DEBUG_MENU, on for the Debug
 * build type): a page of the console view (F11, or the console command debugmenu) that starts any mission, plays any
 * cutscene and video and loads any level, through the mission maker (scrap/missionmaker.h).
 */
#ifndef SCRAP_DEBUGMENU_H
#define SCRAP_DEBUGMENU_H

#include "scrap/shared.h"

#ifdef SCRAP_DEBUG_MENU
/* the menu from its first page */
void DebugMenu_Open(void);
/* a key of the console view (CVK_ of scrap/consoleview.h, or a letter with Ctrl); 0 when the menu is left */
byte DebugMenu_Key(int key, int mods);
/* a typed character: the filter of the page */
void DebugMenu_Char(int c);
void DebugMenu_Wheel(int notches);
/* the menu in the console panel: top and bottom in pixels of the scorer */
void DebugMenu_Draw(float top, float bottom);
/* the console command scene: a cutscene of the menu by name (case ignored), 0 for an unknown one; the names by index
 * (NULL past the last) for Tab */
byte DebugMenu_Scene(const char *name);
const char *DebugMenu_SceneName(int i);
/* each frame: the cutscene a menu entry plays once its level has loaded */
void DebugMenu_Frame(void);
#endif

#endif
