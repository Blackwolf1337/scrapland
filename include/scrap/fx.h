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
/* the effects of the characters' actions between two entities (the freeroam factions show them on the clients) */
void FX_BankerSteal(cEntity *thief, cEntity *victim);
void FX_BankMasterSuction(cEntity *master, cEntity *victim);
void FX_BertoFlash(cEntity *from, cEntity *to);
/* D-Tritus' kill effect at the character killed by his punch (sent to the players of a -mp2 instance, every one: a
 * client's own punch hurts nobody, so it does not predict the kill) */
void FX_DtritusKill(cEntity *dtritus, cEntity *killed);
void FX_BishopRay(cEntity *bishop, cEntity *target, const vec3_t *hit);
void FX_MessengerTake(cEntity *messenger, cEntity *dataPack);
/* the lightning of a messenger's shot at target (NULL none) or the point hit, with its sound */
void FX_MessengerAttack(cEntity *messenger, cEntity *target, const vec3_t *hit);
void FX_PoliceSteal(cEntity *attacker, cEntity *attacked);

#endif
