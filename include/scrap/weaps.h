/*
 * Missiles (original source: Entities/Vehicle/Weaps/): creation on network clients, started by SWeap.NetExec.
 * They return the new missile.
 */
#ifndef SCRAP_WEAPS_H
#define SCRAP_WEAPS_H

#include "scrap/entity.h"
#include "scrap/mathlib.h"

cEntity *Swarm_NetCreate(const char *name, vec3_t pos, vec3_t dir);
void Swarm_NetSetUp(const char *name);
cEntity *Inferno_NetCreate(const char *name, vec3_t pos, vec3_t dir);
void Inferno_NetSetUp(const char *name);

#endif
