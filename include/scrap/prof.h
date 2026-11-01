/*
 * R_ShowInfo, the info overlay of the engine3d (Engine_DrawStats): 1 the frame rate, 2 to 4 more pages (triangles,
 * camera, lights, memory, the frame time graph). The rdtsc profiler of the original (mode 5) is gone.
 */
#ifndef SCRAP_PROF_H
#define SCRAP_PROF_H

#include "scrap/gvar.h"
#include "scrap/shared.h"

extern cGVar GVar_R_ShowInfo;
#define g_ProfMode (GVar_R_ShowInfo.value.i)

#endif
