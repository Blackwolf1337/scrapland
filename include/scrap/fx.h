/*
 * Effects started by the SFX script module (original sources: World/FX/, Entities/).
 */
#ifndef SCRAP_FX_H
#define SCRAP_FX_H

#include "scrap/entity.h"
#include "scrap/mathlib.h"

void FX_ShipExplosion(vec3_t pos, vec3_t vel, float scale, float time, int numFlames, int collision);
void FX_CharacterConversor(cEntity *character, cEntity *conversor, int phase, float a, float b);
void FX_CharacterConversion(cEntity *attacker, cEntity *attacked, float time);
void FX_BishopSellLife(cEntity *attacker, cEntity *attacked);
void FX_MoneyTransfer(cEntity *from, cEntity *to, float time);

#endif
