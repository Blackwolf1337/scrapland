/*
 * miniaudio (third_party/miniaudio, public domain / MIT-0) as the sound mixer uses it: only the playback devices, no
 * decoders, engine or node graph. ma.c holds the implementation with the same options.
 */
#ifndef SOUND_MA_H
#define SOUND_MA_H

#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_NODE_GRAPH
#define MA_NO_ENGINE
#include "miniaudio.h"

#endif
