/*
 * The video player of the engine3d (Engine/Video.c): DirectShow multimedia streams into DirectDraw surfaces.
 */
#ifndef SCRAP_VIDEO_H
#define SCRAP_VIDEO_H

#include "scrap/win32.h"

/* name, or a "a|b" list played one after the other, first in the language directory ("&U&D<lang>/&N&E") of dir;
 * 0 not found, 1 played up to a key, 2 to the end */
int Video_PlayFile(const char *name, const char *dir);
/* dir + name, else the .vds of it; warn: a console warning when neither is found */
int Video_PlayPath(const char *name, const char *dir, byte warn);
/* the stream of the file with its video on dd and the sound to the default renderer; the HRESULT */
int Video_OpenStream(const char *name, void *dd, void **out);
/* the stream played onto primary, or without it through GDI onto the engine window; played 1 when it ran to the
 * end or was skipped by a key */
int Video_RenderStream(void *dd, void *primary, void *ms, byte *played);
/* the source rectangle of the DirectDraw surface stretched onto the window */
void Video_BltToWindow(void *hwnd, const exe_rect *dst, void *surf, const exe_rect *src);
/* busy waiting up to ms while Escape, Space or Enter is held */
void Video_WaitKeysUp(int ms);
/* 1 once A or B of the first pad is let go */
byte Video_PadPressed(void);

#endif
