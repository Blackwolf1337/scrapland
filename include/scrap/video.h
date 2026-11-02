/*
 * The video player of the engine3d (Engine/Video.c): FFmpeg decodes, the render device draws, FMOD plays the sound.
 */
#ifndef SCRAP_VIDEO_H
#define SCRAP_VIDEO_H

#include "scrap/platform.h"

/* Video_PlayFile and Video_PlayPath: the video plays on from the message loop (Video_Step); the result is
 * Video_Result once Video_Busy is 0. The original waited for the end of the video in them. */
#define VIDEO_PENDING (-1)

/* name, or a "a|b" list played one after the other, first in the language directory ("&U&D<lang>/&N&E") of dir;
 * 0 not found, 1 played to the end (or not opened), 2 stopped by a key or the pad, VIDEO_PENDING */
int Video_PlayFile(const char *name, const char *dir);
/* dir + name, else the .vds of it; warn: a console warning when neither is found; the results of Video_PlayFile */
int Video_PlayPath(const char *name, const char *dir, byte warn);
/* one pass of the message loop while Video_Busy: the keys, the pictures that are due and the sound; at the end of
 * the file the second name of a Video_PlayFile list */
void Video_Step(void);
/* 1 while a video plays (Video_Step goes on with it) */
byte Video_Busy(void);
/* the result of the Video_PlayFile that ended last */
int Video_Result(void);
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
