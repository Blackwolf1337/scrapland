/*
 * Logic/Domination.cpp of the game: the domination mode (include/scrap/logic.h). Up to four zones
 * (SLogic.SetDominationZones) with a state each and the baton; the ships of a side (cDominationEnemiesLogic,
 * cDominationFriendsLogic) are sent to the zones their side does not hold, fight the enemies near their zone and
 * chase the others. The table assigned keeps the zone of a ship by its name.
 */
#include <string.h>

#include "scrap/filepak.h"
#include "scrap/hashtable.h"
#include "scrap/logic.h"
#include "scrap/nodestraffic.h"
#include "scrap/scrapcollision.h"

void Domination_AddZone(cEntity *zone) {
	CHECK(g_DominNumZones < MAX_DOMIN_ZONES && "numZones < MAX_DOMIN_ZONES");
	g_DominZones[g_DominNumZones].zone = zone;
	g_DominZones[g_DominNumZones].state = 0;
	g_DominNumZones++;
}

void Domination_SetBatonState(const vec3_t *pos, int state) {
	CHECK((uint32_t)state <= 2 && "state >= 0 && state <= 2");
	g_DominBatonPos = *pos;
	g_DominBatonState = state;
}

void Domination_SetZoneState(int zone, int state) {
	CHECK((uint32_t)zone < MAX_DOMIN_ZONES && "zone >= 0 && zone < MAX_DOMIN_ZONES");
	CHECK((uint32_t)state <= 2 && "state >= 0 && state < MAX_DOMIN_ZONES - 1");
	g_DominZones[zone].state = state;
}

/* the zone the ship of ai is on its way to; one its side does not go for any more is taken away (the AI to 0) */
cEntity *cDominationLogic_GetZone(cDominationLogic *self, cAIVehicle *ai) {
	const char *key = ((cEntity *)cAIVehicle_GetVehicle(ai))->name;
	int going = self->vftable->GetTeam(self) != 1 ? 4 : 3;
	cNameItem *it = NameTable_Find(&self->assigned, key, HashTable_Slot(&self->assigned, key));

	if (!it) {
		return NULL;
	}
	cEntity *zone = (cEntity *)it->value;
	for (int i = 0; i < MAX_DOMIN_ZONES; i++) {
		if (g_DominZones[i].zone == zone && g_DominZones[i].state == going) {
			return zone;
		}
	}
	NameTable_Delete(&self->assigned, key);
	cAIVehicle_SetState(ai, 0);
	return NULL;
}

/* the squared distance of the original: (y, x, z) */
static float Domin_Dist2(const vec3_t *a, const vec3_t *b) {
	float dy = a->y - b->y, dx = a->x - b->x, dz = a->z - b->z;

	return dy * dy + dx * dx + dz * dz;
}

/* the nearest ship of the other side within maxDist of pos, other too */
cEntity *cDominationLogic_FindEnemy(cDominationLogic *self, const vec3_t *pos, cEntity *other,
	float maxDist) {
	vec3_t tmp;
	float max2 = maxDist * maxDist;
	float best = 3.4028234663852886e+38f;
	cEntity *found = NULL;

	cDominationLogic *side = self->vftable->GetTeam(self) == 1 ? &g_pWorld->logic.dominEnemies.base
		: &g_pWorld->logic.dominFriends.base;
	cEntityListData *list = side->vftable->GetList(side);
	for (Nto1Node *n = Logic_ListFirst(list); n; n = Logic_ListNext(n)) {
		cEntity *v = n->data;
		if (v && !v->vftable->IsType(v, TYEN_CAR)) {
			v = NULL;
		}
		CHECK(v && "v = AsVehicle (node->GetNode())");
		float d = Domin_Dist2(v->vftable->GetPos(v, &tmp), pos);
		if (best > d && max2 > d) {
			best = d;
			found = v;
		}
	}
	if (other) {
		float d = Domin_Dist2(other->vftable->GetPos(other, &tmp), pos);
		if (best > d && max2 > d) {
			found = other;
		}
	}
	return found;
}

static void Domin_Chase(cAIVehicle *ai, cEntity *zone) {
	cAIVehicle_SetState(ai, AIV_STATE_CHASE, zone, (double)g_MinDominDistChase, (double)g_MaxDominDistChase);
}

/* a ship of the side: on the way to its zone, the enemies near it (chase 7, line of fire 8, attack 2); without a
 * zone the baton holder, a zone of the other side or nothing held, the target or state 3 */
void cDominationLogic_UpdateShip(cDominationLogic *self, cEntity *v, cEntity *target, cEntity *baton) {
	vec3_t t1, t2, t3;

	cAIVehicle *ai = v->vftable->GetAI(v);
	CHECK(ai);
	int st = cAIVehicle_GetState(ai);
	int team = self->vftable->GetTeam(self);
	cEntity *zone = cDominationLogic_GetZone(self, ai);
	if (zone) {
		const vec3_t *zp = zone->vftable->GetPos(zone, &t1);
		float minR = g_DominMinRangeDist;
		if (Domin_Dist2(v->vftable->GetPos(v, &t2), zp) > minR * minR) {
			if (st != AIV_STATE_CHASE) {
				Domin_Chase(ai, zone);
			}
			return;
		}
		cEntity *enemy;
		if (st == AIV_STATE_CHASE || st == 0) {
			enemy = cDominationLogic_FindEnemy(self, v->vftable->GetPos(v, &t2), baton, g_DominMaxRangeDist);
		} else {
			enemy = Logic_AsCar(ai->vftable->GetTargetEnt(ai));
		}
		if (!enemy) {
			if (st != AIV_STATE_CHASE) {
				Domin_Chase(ai, zone);
			}
			return;
		}
		zp = zone->vftable->GetPos(zone, &t1);
		float d = Domin_Dist2(enemy->vftable->GetPos(enemy, &t3), zp);
		float maxR = g_DominMaxRangeDist;
		minR = g_DominMinRangeDist;
		if (d > maxR * maxR) {
			if (st != AIV_STATE_CHASE) {
				Domin_Chase(ai, zone);
			}
		} else if (d > minR * minR) {
			const vec3_t *ep = enemy->vftable->GetPos(enemy, &t3);
			if (Engine_TestLine(v->vftable->GetPos(v, &t1), ep, NULL, NULL)) {
				if (st != AIV_STATE_CHASE) {
					Domin_Chase(ai, zone);
				}
			} else if (st != 8) {
				cAIVehicle_SetState(ai, 8, enemy);
			}
		} else if (st != AIV_STATE_ATTACK) {
			cAIVehicle_SetState(ai, AIV_STATE_ATTACK, enemy);
		}
		/* also after a chase just set from state 0 */
		if (st == 0) {
			Domin_Chase(ai, zone);
		}
		return;
	}
	if (team == 0 && baton && g_DominBatonState == 2) {
		if (st != AIV_STATE_ATTACK) {
			cAIVehicle_SetState(ai, AIV_STATE_ATTACK, baton);
		}
		return;
	}
	if (g_DominNumZones > 0) {
		int want = (team != 1) + 1;
		int going = (team != 1) + 3;
		byte chasing = 0;
		if (st == AIV_STATE_CHASE) {
			if (Logic_AttackTarget(ai) != target) {
				return;
			}
			chasing = 1;
		}
		int i;
		for (i = 0; i < MAX_DOMIN_ZONES; i++) {
			if (g_DominZones[i].state == want) {
				break;
			}
		}
		if (i == MAX_DOMIN_ZONES) {
			if (chasing) {
				cEntity *e = cDominationLogic_FindEnemy(self, v->vftable->GetPos(v, &t3), NULL,
					g_DominWithinRangeDist);
				if (e) {
					cAIVehicle_SetState(ai, AIV_STATE_ATTACK, e);
				}
				return;
			}
		} else {
			if (team == 0) {
				cAIVehicle_GetParams2(ai)->aim = g_DominFightingFreezeControlAim;
				cAIVehicle_GetParams3(ai)->cadence = g_DominFightingFreezeCadenceShoot;
			}
			DominZone *z = &g_DominZones[i];
			Domin_Chase(ai, z->zone);
			z->state = going;
			byte added = NameTable_Add(&self->assigned, v->name, (const intptr_t *)&z->zone, 0);
			CHECK(added && "assignedZones.Add(v->GetName(), dominationZones[i].zone)");
		}
	}
	if (st == AIV_STATE_CHASE || st == AIV_STATE_ATTACK) {
		return;
	}
	if (target) {
		cAIVehicle_SetState(ai, AIV_STATE_CHASE, target, (double)g_AIChaseDefault1, (double)g_AIChaseDefault2);
		return;
	}
	cAIVehicle_SetState(ai, 3, NULL, 5.0, 2.0);
}

/* the base part of the constructors of the sides (inlined in the original) */
void cDominationLogic_ctor(cDominationLogic *self) {
	self->vftable = CDOMINATIONLOGIC_VFTABLE;
	self->assigned.numSlots = 4;
	self->assigned.slots = operator_new_array(4 * sizeof(HashNode *));
	memset(self->assigned.slots, 0, 4 * sizeof(HashNode *));
	self->assigned.locked = 0;
	self->assigned.caseSensitive = 1;
	g_DominNumZones = 0;
}

cDominationEnemiesLogic *cDominationEnemiesLogic_ctor(cDominationEnemiesLogic *self) {
	cDominationLogic_ctor(&self->base);
	self->base.vftable = CDOMINATIONENEMIESLOGIC_VFTABLE;
	cDominationSide *s = operator_new(sizeof(*s));
	if (s) {
		s->owner = &self->base;
		s->list = NULL;
		s->timer = 0.0f;
		s->zone = -1;
		self->side = s;
		return self;
	}
	self->side = NULL;
	return self;
}

void cDominationEnemiesLogic_dtor(cDominationEnemiesLogic *self) {
	self->base.vftable = CDOMINATIONENEMIESLOGIC_VFTABLE;
	if (self->side) {
		operator_delete_sized(self->side, sizeof(*self->side));
	}
	cDominationLogic_dtor(&self->base);
}

void cDominationLogic_dtor(cDominationLogic *self) {
	self->vftable = CDOMINATIONLOGIC_VFTABLE;
	g_DominNumZones = 0;
	NameTable_Clear(&self->assigned);
}

cDominationEnemiesLogic *cDominationEnemiesLogic_Delete(cDominationEnemiesLogic *self, byte flags) {
	cDominationEnemiesLogic_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

cDominationLogic *cDominationLogic_Delete(cDominationLogic *self, byte flags) {
	cDominationLogic_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, 0x10);
	}
	return self;
}

/* the zone nearest to pos that is free (state 0), or held by a side (1) when that one is nearer; -1 for none */
int cDominationLogic_NearestZone(cDominationLogic *self, const vec3_t *pos) {
	vec3_t tmp;
	float bestFree = 3.4028234663852886e+38f, bestHeld = 3.4028234663852886e+38f;
	int free = -1, held = -1;

	(void)self;
	for (int i = 0; i < MAX_DOMIN_ZONES; i++) {
		int s = g_DominZones[i].state;
		if (s == 2 || s == 4) {
			continue;
		}
		cEntity *z = g_DominZones[i].zone;
		float d = Domin_Dist2(z->vftable->GetPos(z, &tmp), pos);
		if (s == 0) {
			if (bestFree > d) {
				free = i;
				bestFree = d;
			}
		} else if (bestHeld > d) {
			bestHeld = d;
			held = i;
		}
	}
	return bestHeld > bestFree ? free : held;
}
