/*
 * Entities/Vehicle/Weaps/Inferno.cpp of the game: the Inferno missile, see include/scrap/weaps2.h. The missile
 * carries one bee per mounted Inferno bay; it explodes in a sphere whose range grows with the bees, hurts and pushes
 * the entities in it (InfernoXplosionCall) and shakes the cameras of the players near it.
 */
#include <string.h>

#include "scrap/crt.h"
#include "scrap/weaps2.h"

static const char *const inferno_types[] = {
	"Misil Inferno Uranio\n", "Misil Inferno Hidrogeno\n", "Misil Inferno Napalm\n"
};

/* the reload sound, the delay of the next shot and the shoot effect at every mounted bay */
void Inferno_Shoot(const char *name) {
	cEntity *v = Weap_Vehicle(name);
	if (cTargetLock_GetAmmo(&Veh(v)->weapons, g_InfernoIDAmmo) >= (float)g_InfernoAmmoCost) {
		Weap_PlaySound(v, 0x15);
		Veh(v)->weapons.fireStamp = -g_InfernoTimeDelay;
		for (int i = 0; i < 4; i++) {
			if (cTargetLock_HasSlot(&Veh(v)->weapons, g_InfernoIDWeap, i)) {
				cVehicleEffects_Shoot(Veh_Effects(v), g_InfernoIDWeap, i);
			}
		}
	}
}

/* name, position, orientation */
void Inferno_NetCreateNative(cAbstractFile *f) {
	char name[0x80];
	vec3_t pos, dir;

	cAbstractFile_ReadString(f, name, 0x80);
	Weap_ReadVec(f, &pos);
	Weap_ReadVec(f, &dir);
	Inferno_NetCreate(name, pos, dir);
}

cEntity *Inferno_NetCreate(const char *name, vec3_t pos, vec3_t dir) {
	cEntity *m = cWorld_CreateEntity(g_pWorld, name, pos.x, pos.y, pos.z, TYEN_MISSILE);
	CHECK(m);
	CHECK(Entity_IsType(m, TYEN_MISSILE));
	cMissileEntity_SetHiddenSwarm((cMissileEntity *)m, 0);
	cMissileEntity_SetHideMain((cMissileEntity *)m, 1);
	Obj_Vtbl(m)->SetAng(m, &dir);
	cMissileEntity_SetSwarmSpeed((cMissileEntity *)m, g_InfernoBeesSpeed);
	cMissileEntity_SetLifeTime((cMissileEntity *)m, g_InfernoLifeTime);
	cPy_Function_SetNative(&Missile(m)->onHit, (nativefunc_t)GAME_FUNC(Inferno_HitNative));
	cEntity_SetNetResource(m, "Inferno");
	return m;
}

/*
 * the cursor of the HUD item: it turns and shrinks for a new target (time 1 to 0), then shows the level of the
 * weapon; shared with the Swarm (its own buffers and levels). Ship name, player, item name.
 */
void Weap_UpdateHud(cAbstractFile *f, char (*targets)[0x80], float *times, int levelRot, int levelSize) {
	char name[0x80], itemName[0x80];
	int player;

	cAbstractFile_ReadString(f, name, 0x80);
	f->vftable->Read(f, &player, 4);
	cAbstractFile_ReadString(f, itemName, 0x80);
	cEntity *v = Weap_Vehicle(name);
	cCannonScorer *item = (cCannonScorer *)cScorer_FindItem(Scorer_Get(player), itemName);
	if (!item) {
		return;
	}
	float dt = Scorer_Get(player)->frameTime;
	if (strcmp(Nto1Node_TargetName(&Veh(v)->autoAim.target), targets[player])) {
		Str_Copy(targets[player], Nto1Node_TargetName(&Veh(v)->autoAim.target), 0x80);
		times[player] = Veh(v)->autoAim.target.target ? 1.0f : 0.0f;
	}
	float t = times[player];
	if (0.0f >= t) {
		times[player] = t - dt;
		int level = cWeaponManager_GetUpgrade(&Veh(v)->weapons, Veh(v)->weapons.curWeap);
		if (Veh(v)->autoAim.target.target && (level == levelRot || level == levelSize)) {
			if (-0.5f >= times[player]) {
				times[player] = 0.0f;
			}
			float x = (float)__builtin_fabs((double)(times[player] * 4.0f + 1.0f));
			uint32_t a = (uint32_t)(-1 - (int)(x * 64.0f));
			item->color = ((((a << 8) | g_CannonScorerColorRed) << 8 | g_CannonScorerColorGreen) << 8) |
				g_CannonScorerColorBlue;
			if (level == levelRot) {
				item->size[0] = 20.0f;
				item->size[1] = 20.0f;
				item->angle = (1.0f - x) * 3.14159274f * 0.25f;
			} else {
				float s = (1.0f - x) * 7.0f + 20.0f;
				item->size[0] = s;
				item->size[1] = s;
				item->angle = 0.0f;
			}
			return;
		}
		item->size[0] = 20.0f;
		item->size[1] = 20.0f;
		item->color = (((g_CannonScorerColorRed | 0xffffbf00u) << 8 | g_CannonScorerColorGreen) << 8) |
			g_CannonScorerColorBlue;
		item->angle = 0.0f;
		return;
	}
	t = t - dt * 3.0f;
	times[player] = t;
	item->angle = t * 1.57079637f;
	float s = times[player] * 50.0f + 20.0f;
	item->size[0] = s;
	item->size[1] = s;
	uint32_t a = (uint32_t)(-64 - (int)(times[player] * 192.0f));
	item->color = ((((a << 8) | g_CannonScorerColorRed) << 8 | g_CannonScorerColorGreen) << 8) |
		g_CannonScorerColorBlue;
}

void Inferno_HudNative(cAbstractFile *f) {
	Weap_UpdateHud(f, g_InfernoHudTarget, g_InfernoHudTime, 11, 12);
}

/* the ship shoots: a missile with one bee per Inferno ammo cost (up to the bays) */
void Inferno_FireNative(cAbstractFile *f) {
	char name[0x80], obj[0x80];
	vec3_t pos, ang, vel;

	cAbstractFile_ReadString(f, name, 0x80);
	Inferno_Shoot(name);
	cEntity *v = Weap_Vehicle(name);
	float ammo = cTargetLock_GetAmmo(&Veh(v)->weapons, g_InfernoIDAmmo);
	if (!(ammo >= (float)g_InfernoAmmoCost)) {
		return;
	}
	v->vftable->GetPos(v, &pos);
	Obj_Vtbl(v)->GetAng(v, &ang);
	Obj_Vtbl(v)->GetVel(v, &vel);
	Net_GetObjName(obj);
	cEntity *m = Inferno_NetCreate(obj, pos, ang);
	if (!m) {
		return;
	}
	Controls_RumbleA(&Veh(v)->controls, 1.0f, 0.3f);
	Controls_RumbleB(&Veh(v)->controls, 1.0f, 0.5f);
	cPy_Function_SetNative(&Missile(m)->onHit, (nativefunc_t)GAME_FUNC(Inferno_ServerHitNative));
	cPy_Function_SetNative(&WLife(m)->onDeath, (nativefunc_t)GAME_FUNC(Inferno_OnDeathNative));
	cPy_Function_SetNative(&WLife(m)->onDamage, (nativefunc_t)GAME_FUNC(Inferno_OnDamageNative));
	cMissileEntity_SetBeesHit((cMissileEntity *)m, 1);
	if (!WLife(m)->god) {
		WLife(m)->life = g_InfernoArmor;
	}
	cPrimitive_SetSize(Mis_Size(m), 50.0f);
	cMissileEntity_SetParent((cMissileEntity *)m, name);
	cMissileEntity_SetTarget((cMissileEntity *)m, Nto1Node_TargetName(&Veh(v)->autoAim.target));
	Inferno_NetSetUp(m->name);
	int n = (int)ammo / g_InfernoAmmoCost;
	if (Missile(m)->swarmCount > n) {
		Missile_Vtbl(Missile(m))->SetSwarm(Missile(m), n);
	}
	cPrimitive_SetSize(Mis_Size(m), Missile(m)->topRadius * 0.6f);
	if (!WLife(m)->god) {
		WLife(m)->life = Missile(m)->swarmCount * g_InfernoArmor;
	}
	cMissileEntity_SetLifeTime((cMissileEntity *)m, Missile(m)->swarmCount ? g_InfernoLifeTime - 0.2f : 0.1f);
	cWeaponManager_SetAmmo(&Veh(v)->weapons, g_InfernoIDAmmo,
		(float)(int)ammo - (float)(Missile(m)->swarmCount * g_InfernoAmmoCost));
}

/* missile name, two names not used */
void Inferno_HitNative(cAbstractFile *f) {
	char name[0x80], a[0x80], b[0x80];

	cAbstractFile_ReadString(f, name, 0x80);
	cAbstractFile_ReadString(f, a, 0x80);
	cAbstractFile_ReadString(f, b, 0x80);
	Inferno_Hit(name);
}

/* the explosion effect, the camera shake and flash of the players in range, the hit sound; returns the missile */
cEntity *Inferno_Hit(const char *name) {
	vec3_t pos, op, tmp, rnd, nrm;

	cEntity *m = Weap_Missile(name);
	m->vftable->GetPos(m, &pos);
	float range = (float)Missile(m)->swarmCount * g_InfernoRangeBay + g_InfernoRangeMin;
	uint32_t type = (uint32_t)Missile(m)->level;
	Con_Print(0x606060, type < 3 ? inferno_types[type] : "Misil Inferno Desconocido\n", 0);
	if (!FX_IsVisible(pos.x, pos.y, pos.z, range, g_FXInfernoExplosionMaxDist, NULL, 1)) {
		return m;
	}
	cDebrisSys *sys = cWorld_GetDebrisSys(g_pWorld, g_InfernoExplosionFile);
	cDebris *d = cDebrisSys_NewElement(sys);
	CGameModel_SetPos(Debris_Model(d), &pos);
	d->life = g_FXInfernoExplosionTime;
	cDebrisSys_Drop(sys, d);
	cObjElementFXInfernoExplosion *fx = Debris_Element(d);
	if (fx) {
		if (!fx->on) {
			fx->on = 1;
		}
		cObjElementFXInfernoExplosion_Explode(fx, pos.x, pos.y, pos.z, range, Missile(m)->swarmCount, 0);
	}
	int i = 0;
	do {
		cEntity *o = Weap_Owner(g_pWorld, i);
		if (o && Obj(o)->cam) {
			byte hit = Engine_TestLine(&pos, o->vftable->GetPos(o, &tmp), NULL, NULL);
			if (Obj(o)->cam && !hit) {
				const vec3_t *p = o->vftable->GetPos(o, &op);
				float dy = p->y - pos.y;
				float dx = p->x - pos.x;
				float dz = p->z - pos.z;
				float dist = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
				if (1.0f > dist) {
					double r0 = (double)(crt_rand() & 0x7fff) * g_WeapRandScale;
					double r1 = (double)(crt_rand() & 0x7fff) * g_WeapRandScale;
					double r2 = (double)(crt_rand() & 0x7fff) * g_WeapRandScale;
					rnd.x = (float)((r2 + r2) - 1.0);
					rnd.y = (float)((r1 + r1) - 1.0);
					rnd.z = (float)((r0 + r0) - 1.0);
					Vec3_NormalizeTo(&nrm, &rnd);
					dist = 1.0f;
				}
				dist = Float_MaxSS(1.0f, dist - WLife(o)->prim.radius);
				if (range >= dist) {
					float w = (range - dist) / range;
					w = w * w * (2.0f - w);
					cScorer *sc = NULL;
					int found = 1;
					if (g_SplitScreenGame) {
						int k;
						for (k = 0; k < 4; k++) {
							const char *cam = Obj(o)->cam->name;
							if (!strcmp(cam, cWorld_GetCamName(g_pWorld, k))) {
								break;
							}
						}
						found = k < 4;
						if (found) {
							sc = Scorer_Get(k);
						}
					} else {
						sc = Scorer_Get(0);
					}
					if (found && sc) {
						uint32_t a = (uint32_t)(-64 - (int)(w * -63.0f));
						ScorerFade_Start(&sc->fade, (a << 24) | 0xffbe3c, 0xffffff, g_InfernoLifeTime * w * 0.5f);
					}
					cEntity *cam = Obj(o)->cam;
					if (cam) {
						Cam_Shake(cam) = w * 0x1.c0f2eep-5f;
						Cam_ShakeTime(cam) = g_InfernoLifeTime * w * 0.25f;
					}
				}
			}
		}
	} while (++i < (g_SplitScreenGame != 0) + 1);
	cSoundDesc desc;
	cSoundDesc_Init(&desc);
	desc.param3 = 0.0f;
	desc.param1 = g_InfernoHitSoundAttIniRadius * range;
	desc.param2 = g_InfernoHitSoundAttEndRadius * range;
	cSoundDesc_SetPos(&desc, &pos);
	desc.volume += Snd_Volume(g_InfernoHitSoundVol);
	g_pSound->vftable->PlaySound(g_pSound, g_InfernoHitSound, &desc, NULL);
	return m;
}

/* missile name, attacker name, damage: one bee lost per InfernoArmor */
void Inferno_OnDamageNative(cAbstractFile *f) {
	char name[0x80], other[0x80];
	int damage;

	cAbstractFile_ReadString(f, name, 0x80);
	cAbstractFile_ReadString(f, other, 0x80);
	f->vftable->Read(f, &damage, 4);
	cEntity *m = Weap_Missile(name);
	int bees;
	if (Ent_Active(m) && (bees = Missile(m)->swarmCount) > 0) {
		int armor = g_InfernoArmor;
		if (damage >= armor) {
			int n = damage / armor;
			Missile_Vtbl(Missile(m))->Hit(Missile(m), n > bees ? bees : n, 0);
		}
	}
}

void Inferno_OnDeathNative(cAbstractFile *f) {
	char name[0x80], other[0x80];

	cAbstractFile_ReadString(f, name, 0x80);
	cAbstractFile_ReadString(f, other, 0x80);
	cEntity *m = Weap_Missile(name);
	if (Ent_Active(m)) {
		m->vftable->SetActive(m, 0);
		Inferno_Explode(name);
		Missile_Vtbl(Missile(m))->SetSwarm(Missile(m), 0);
	}
}

void Inferno_AimOffNative(cAbstractFile *f) {
	Weap_SetAimMode(f, 0);
}

/* missile name, a number not used: the speed back towards InfernoNormSpeed */
void Inferno_SlowNative(cAbstractFile *f) {
	char name[0x80];
	int unused;

	cAbstractFile_ReadString(f, name, 0x80);
	f->vftable->Read(f, &unused, 4);
	cEntity *m = Weap_Missile(name);
	float speed = Missile(m)->speed;
	float norm = g_InfernoNormSpeed;
	if (speed > norm) {
		cMissileEntity_SetSpeed((cMissileEntity *)m, (speed - norm) * 0.9f + norm);
	}
}

void Inferno_AimOnNative(cAbstractFile *f) {
	cEntity *v = Weap_SetAimMode(f, 2);
	cAutoAim_SetRange(Veh_Aim(v), g_InfernoDistLimit);
	cAutoAim_SetAngle(Veh_Aim(v), 0x1.1df46ap-3f);
}

void Inferno_AimMode3Native(cAbstractFile *f) {
	Weap_SetAimMode(f, 3);
}

void Inferno_AimMode2Native(cAbstractFile *f) {
	Weap_SetAimMode(f, 2);
}

void Inferno_ServerHitNative(cAbstractFile *f) {
	char name[0x80];

	cAbstractFile_ReadString(f, name, 0x80);
	Inferno_Explode(name);
}

/* the explosion: the effect, then (not on clients) InfernoXplosionCall on the entities in the sphere */
void Inferno_Explode(const char *name) {
	vec3_t tmp;

	cEntity *m = g_InfernoMissile = Inferno_Hit(name);
	if (!m) {
		return;
	}
	const vec3_t *p = m->vftable->GetPos(m, &tmp);
	g_InfernoPos.x = p->x;
	g_InfernoPos.y = p->y;
	g_InfernoPos.z = p->z;
	m = g_InfernoMissile;
	const char *owner = Missile(m)->parent;
	g_InfernoRange = (float)Missile(m)->swarmCount * g_InfernoRangeBay + g_InfernoRangeMin;
	if (*owner) {
		HashTable *t = &g_pWorld->entities;
		HashNode *node = HashTable_Find(t, owner, HashTable_Hash(t, owner));
		cEntity *e = node ? node->value : NULL;
		g_InfernoOwner = e;
		if (!e || !Entity_IsType(e, TYEN_VEHICLE)) {
			g_InfernoOwner = NULL;
		}
	} else {
		g_InfernoOwner = NULL;
	}
	if (!(g_pApp->flags & APPF_CLIENT)) {
		cPy_Function_SetNative(g_InfernoSphereFunc, g_InfernoXplosionCall);
		cWorld_SphereCallF(g_pWorld, 8, &g_InfernoPos, g_InfernoRange, g_InfernoSphereFunc, 0);
	}
}

void Inferno_NetSetUpNative(cAbstractFile *f) {
	char name[0x80];

	cAbstractFile_ReadString(f, name, 0x80);
	Inferno_NetSetUp(name);
}

/* the model, speeds, bees (one per mounted bay of the ship), the upgrade type and the engine sound of a missile */
void Inferno_NetSetUp(const char *name) {
	vec3_t tmp, ang, vel;
	matrix_t mtx;

	cEntity *m = Weap_Missile(name);
	m->vftable->GetPos(m, &tmp);
	m->vftable->SetModel(m, g_InfernoMissileFile, 1.0f, 1.0f, 1.0f, 0);
	cMissileEntity_SetRadius((cMissileEntity *)m, 100.0f);
	cMissileEntity_SetTopRadius((cMissileEntity *)m, 400.0f);
	cMissileEntity_SetRadiusGrow((cMissileEntity *)m, 200.0f);
	cMissileEntity_SetSpeed((cMissileEntity *)m, g_InfernoNormSpeed);
	cMissileEntity_SetTurnSpeed((cMissileEntity *)m, (float)(((double)g_InfernoTurnSpeed * WEAP_PI) / 180.0));
	cMissileEntity_SetBeesGather((cMissileEntity *)m, 0);
	cEntity *v = Weap_FindVehicle(Missile(m)->parent);
	if (v) {
		float top = g_InfernoUpTopSpeed;
		if (Veh(v)->speed > g_InfernoNormSpeed - top) {
			cMissileEntity_SetSpeed((cMissileEntity *)m, top + Veh(v)->speed);
		}
		cTargetLock *wm = &Veh(v)->weapons;
		for (int i = 0; i < 4; i++) {
			if (!cTargetLock_HasSlot(wm, g_InfernoIDWeap, i)) {
				continue;
			}
			Weap_DropAtNode(v, Veh(v)->weapons.nodes[g_InfernoIDAmmo][i], &mtx, &ang, &vel);
			Missile_Vtbl(Missile(m))->AddToSwarm(Missile(m), 1);
			if (i == 0 && Missile(m)->swarm && Debris_Element(Missile(m)->swarm)) {
				cObjElementFXInfernoMissile_SetFlare(Debris_Element(Missile(m)->swarm), 1);
			}
		}
		int level = cWeaponManager_GetUpgrade(wm, g_InfernoIDWeap);
		Missile(m)->level = level;
		if ((g_SplitScreenGame || g_pApp->flags) && (level == 11 || level == 12)) {
			int bees = Missile(m)->swarmCount;
			Missile(m)->level = level - 10;
			float left = cWeaponManager_GetUAmmo(wm, g_InfernoIDWeap) - (float)(bees * 51);
			cWeaponManager_SetUAmmo(wm, g_InfernoIDWeap, left);
			if (0.0f >= left) {
				cPy_Function_AddParamI(g_RestoreUpgradeFunc, Missile(m)->level + 10);
				cPy_Function_Execute(g_RestoreUpgradeFunc, 0);
				cWeaponManager_SetUpgrade(wm, g_InfernoIDWeap, 0);
			}
		}
	}
	Missile_Vtbl(Missile(m))->SetSwarm(Missile(m), cMissileEntity_GetSwarmCount((cMissileEntity *)m));
	WithLife_Vtbl(m)->SetMass(m, 6000.0f);
	Missile(m)->fallOut = 1;
	Weap_EngineSound(m, g_InfernoEngineSound, g_InfernoEngineSoundVol, g_InfernoEngineSoundAttIni,
		g_InfernoEngineSoundAttEnd, g_InfernoEngineSoundDoppler);
}

/* InfernoXplosionCall, for each entity in the sphere of the explosion: damage, push and burn by the distance */
void Inferno_SphereHit(cAbstractFile *f) {
	vec3_t tmp, rnd, nrm, vel;

	cAbstractFile_ReadString(f, g_InfernoHitName, 0x80);
	if (!*g_InfernoHitName) {
		return;
	}
	HashTable *t = &g_pWorld->entities;
	HashNode *node = HashTable_Find(t, g_InfernoHitName, HashTable_Hash(t, g_InfernoHitName));
	if (!node) {
		return;
	}
	cEntity *e = node->value;
	if (!e || !Entity_IsType(e, TYEN_WITH_LIFE)) {
		return;
	}
	int life = WLife(e)->life;
	const vec3_t *p = e->vftable->GetPos(e, &tmp);
	float dy = p->y - g_InfernoPos.y;
	float dx = p->x - g_InfernoPos.x;
	float dz = p->z - g_InfernoPos.z;
	float dist = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
	if (dist != 0.0f) {
		float inv = 1.0f / dist;
		dx *= inv;
		dy *= inv;
		dz *= inv;
	}
	if (1.0f > dist) {
		double r0 = (double)(crt_rand() & 0x7fff) * g_WeapRandScale;
		double r1 = (double)(crt_rand() & 0x7fff) * g_WeapRandScale;
		double r2 = (double)(crt_rand() & 0x7fff) * g_WeapRandScale;
		rnd.x = (float)((r2 + r2) - 1.0);
		rnd.y = (float)((r1 + r1) - 1.0);
		rnd.z = (float)((r0 + r0) - 1.0);
		const vec3_t *n = Vec3_NormalizeTo(&nrm, &rnd);
		dx = n->x;
		dy = n->y;
		dz = n->z;
		dist = 1.0f;
	}
	float d = Float_MaxSS(1.0f, dist - WLife(e)->prim.radius);
	float k = (g_InfernoRange - d) / g_InfernoRange;
	if (Missile(g_InfernoMissile)->level == 1) {
		float inv = g_InfernoInvUpgExtraDamage;
		if (k > inv) {
			k = 1.0f;
		} else {
			k *= 1.0f / inv;
		}
	}
	float w = k * k * (2.0f - k);
	if (!WLife(e)->god) {
		WLife(e)->life -= (int)((g_InfernoMaxDamage - g_InfernoMinDamage) * w);
	}
	byte isWalk = Entity_IsType(e, TYEN_WALKCHAR);
	byte isCar = Entity_IsType(e, TYEN_VEHICLE);
	if (isWalk || isCar) {
		if (g_InfernoOwner && life > WLife(e)->life) {
			cVehicleAttack_ShotAt(Veh_HitInfo(g_InfernoOwner), g_InfernoHitName, 0);
		}
	}
	if (!isWalk) {
		float px = dx * w;
		float py = dy * w;
		float pz = dz * w;
		float force = g_InfernoHitForce * 1000.0f / WLife(e)->mass;
		px *= force;
		py *= force;
		pz *= force;
		const vec3_t *ov = Obj_Vtbl(e)->GetVel(e, &tmp);
		vel.x = px + ov->x;
		vel.y = ov->y + py;
		vel.z = ov->z + pz;
		Obj_Vtbl(e)->SetVel(e, &vel);
	}
	if (isCar && Veh(e)->netCtrl) {
		Veh(e)->netCtrl->hit = 2;
	}
	if (WLife(e)->life > 0 && Missile(g_InfernoMissile)->level == 2) {
		WLife(e)->fireRemain = g_InfernoUpgFireTime;
		WLife(e)->fireAtt = 0.0f;
		WLife(e)->fireDelay = ((1.0f - w) + 0.1f) / g_InfernoUpgFireFactor;
		cWithLifeEntity_SetBurner(e, g_InfernoOwner);
	}
}
