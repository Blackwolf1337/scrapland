/*
 * The video player of the engine3d (Engine/Video.c): DirectShow multimedia streams into DirectDraw surfaces.
 */
#ifndef SCRAP_VIDEO_H
#define SCRAP_VIDEO_H

#include "scrap/win32.h"

/* Video_PlayFile and Video_PlayPath: the video plays on from the message loop (Video_Step); the result is
 * Video_Result once Video_Busy is 0. The original waited for the end of the video in them. */
#define VIDEO_PENDING (-1)

/* name, or a "a|b" list played one after the other, first in the language directory ("&U&D<lang>/&N&E") of dir;
 * 0 not found, 1 played to the end (or not opened), 2 stopped by a key or the pad, VIDEO_PENDING */
int Video_PlayFile(const char *name, const char *dir);
/* dir + name, else the .vds of it; warn: a console warning when neither is found; the results of Video_PlayFile */
int Video_PlayPath(const char *name, const char *dir, byte warn);
/* the stream of the file with its video on dd and the sound to the default renderer; the HRESULT */
int Video_OpenStream(const char *name, void *dd, void **out);
/* the stream played onto primary, or without it through GDI onto the engine window; played 1 when a key or the
 * pad stopped it. When the stream runs, Video_Step plays its frames and stops it (played is written then). */
int Video_RenderStream(void *dd, void *primary, void *ms, byte *played);
/* one pass of the message loop while Video_Busy: the keys, the next frame and the blits of the stream; at its end
 * the rest of Video_PlayPath and the second name of a Video_PlayFile list */
void Video_Step(void);
/* 1 while a video plays (Video_Step goes on with it) */
byte Video_Busy(void);
/* the result of the Video_PlayFile that ended last */
int Video_Result(void);
/* Video_PlayFile up to the end of the video, waiting as the original did (synchronous Update, DDBLT_WAIT): for
 * Scrap.VideoPlayer, whose script goes on after the video */
int Video_PlayFileWait(const char *name, const char *dir);
/* the source rectangle of the DirectDraw surface stretched onto the window */
void Video_BltToWindow(void *hwnd, const exe_rect *dst, void *surf, const exe_rect *src);
/* the keys that ended or started a video do not count for up to ms: the original waited that long while Escape,
 * Space or Enter was held, the C starts a hold that Video_KeysHeld ends (no blocking wait) */
void Video_WaitKeysUp(int ms);
/* 1 while the hold of Video_WaitKeysUp is on (Escape, Space or Enter still held and its time not over) */
byte Video_KeysHeld(void);
/* the DirectInput states (256, by DIK_ code) of Escape, Space and the Enter keys cleared while the hold is on */
void Video_MaskHeldKeys(char *keys);
/* 1 once A or B of the first pad is let go */
byte Video_PadPressed(void);

#endif
