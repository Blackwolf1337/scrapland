/*
 * Missiles (original source: Entities/Vehicle/Weaps/): creation on network clients, started by SWeap.NetExec.
 */
#ifndef SCRAP_WEAPS_H
#define SCRAP_WEAPS_H

#include "scrap/mathlib.h"

void Swarm_NetCreate(const char *name, vec3_t pos, vec3_t dir);
void Swarm_NetSetUp(const char *name);
void Inferno_NetCreate(const char *name, vec3_t pos, vec3_t dir);
void Inferno_NetSetUp(const char *name);

#endif
