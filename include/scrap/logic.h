/*
 * Game logic and AI used by the SLogic script module (original sources: Logic/, AI/). The objects at
 * g_pWorld->taunts, shipRegen and aiCtrl are only known by their methods so far.
 */
#ifndef SCRAP_LOGIC_H
#define SCRAP_LOGIC_H

#include "scrap/entity.h"

typedef struct cLocalLogic cLocalLogic;

typedef struct cLocalLogicVtbl {
	void *vf00_18[19];
	void(__thiscall *SetState)(cLocalLogic *self, int state, cEntity *target);	/* 0x4c vf19 */
} cLocalLogicVtbl;
SD_OFFSET(cLocalLogicVtbl, SetState, 0x4c);

struct cLocalLogic {
	const cLocalLogicVtbl *vftable;
};

/* the logic of a character or NULL */
cLocalLogic *Logic_GetLocalLogic(cEntity *e);
/* puts the entity on the floor; 0 if not possible */
byte Logic_SetOnFloor(cEntity *e);
/* flash at pos with radius; the last argument is -1 from the script module */
int Logic_Flash(const vec3_t *pos, float radius, int arg);

void __thiscall Taunts_UpdateEndTime(void *taunts);
void __thiscall ShipRegen_Set(void *shipRegen, cEntity *ship, float speed, float endSpeed);
void __thiscall AICtrl_SendSentinelToWatch(void *aiCtrl, const vec3_t *pos);
void __thiscall AICtrl_GearAttack(void *aiCtrl, cEntity *target, int arg);

/* domination mode */
void Domination_AddZone(cEntity *zone);
void Domination_SetZoneState(int zone, int state);
void Domination_SetBatonState(const vec3_t *pos, int state);

#endif
