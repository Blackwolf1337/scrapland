/*
 * Entities/Char/Type/PoliceBoss.cpp of the game: the police boss, whose burp throws debris around and makes the
 * police near him laugh (include/scrap/chartype.h).
 */
#include <string.h>

#include "scrap/chartype.h"
#include "scrap/fxelements.h"
#include "scrap/staticinit.h"

#define PB_SYSTEMS 5		/* the debris systems <PoliceBossBrupBaseFile>A.M3D .. E.M3D */
#define PB_Started(el) (((FXElemHead *)(el))->started)

/* params: the boss, an event and the life factor of the debris: PoliceBossBrupDebrisMin..Max debris out of the
 * mouth in random directions */
void ChType_FXPoliceBossBrup(cAbstractFile *params) {
	char name[0x80], event[0x80], file[0x100];
	cDebrisSys *sys[PB_SYSTEMS];
	vec3_t pos, ang, down, rot, dir, one, rot2, t1, off, t2;
	float scale;

	cAbstractFile_ReadString(params, name, 0x80);
	cAbstractFile_ReadString(params, event, 0x80);
	params->vftable->Read(params, &scale, 4);
	cEntity *v = World_FindEntity(name);
	CHECK(v);
	CHECK(Entity_IsType(v, TYEN_WITH_LIFE));
	Obj_Vtbl(v)->GetViewPos(v, &pos);
	Obj_Vtbl(v)->GetViewAng(v, &ang);
	for (int i = 0; i < PB_SYSTEMS; i++) {
		Str_Printf(file, 0xff, "%s%c.M3D", g_PoliceBossBrupBaseFile, i + 'A');
		cDebrisSys *s = cWorld_GetDebrisSys(g_pWorld, file);
		sys[i] = s;
		s->radiusSq = 100000000.0f;
		s->flag = 1;
		s->d = 980.0f;
		s->b = 0.75f;
		s->onHit = (DebrisHit_t)ChType_PoliceBossBrupBounce;
	}
	int r = crt_rand();
	int n = g_PoliceBossBrupDebrisMin + r % (g_PoliceBossBrupDebrisMax - g_PoliceBossBrupDebrisMin);
	for (; n > 0; n--) {
		cDebrisSys *s = sys[crt_rand() % PB_SYSTEMS];
		cDebris *d = cDebrisSys_NewElement(s);
		d->life = FxElem_Rand(0.3500000238418579, 0.75) * scale;
		cDebrisSys_Drop(s, d);
		/* the direction: down along the view turned by up to DirRand half turns */
		float f = FxElem_Rand(0.05000000074505806, -0.02500000037252903) + g_PoliceBossBrupDirRand;
		float hi = (float)((double)f * 3.141592653589793);
		float lo = (float)((double)f * -3.141592653589793);
		double range = (double)(hi - lo);
		double base = (double)lo;
		float az = FxElem_Rand(range, base);
		float ay = FxElem_Rand(range, base);
		float ax = FxElem_Rand(range, base);
		down.x = 0.0f;
		down.y = 0.0f;
		down.z = -1.0f;
		rot.x = ang.x + ax;
		rot.y = ang.y + ay;
		rot.z = ang.z + az;
		Vec3_Rotate(&dir, &down, &rot);
		/* the start: off the mouth by up to 8 in a random direction */
		float len = FxElem_Rand(8.0, 0.0);
		float bz = FxElem_Rand(6.2831854820251465, 0.0);
		float by = FxElem_Rand(6.2831854820251465, 0.0);
		float bx = FxElem_Rand(6.2831854820251465, 0.0);
		one.x = 1.0f;
		one.y = 1.0f;
		one.z = 1.0f;
		rot2.x = bx;
		rot2.y = by;
		rot2.z = bz;
		const vec3_t *rv = Vec3_Rotate(&t1, &one, &rot2);
		float ox = len * rv->x;
		float oy = rv->y * len;
		float oz = rv->z * len;
		off.x = g_PoliceBossBrupPosX;
		off.y = g_PoliceBossBrupPosY;
		off.z = g_PoliceBossBrupPosZ;
		const vec3_t *ro = Vec3_Rotate(&t2, &off, &ang);
		d->drop[0].x = ro->x + pos.x + ox;
		d->drop[0].y = ro->y + pos.y + oy;
		d->drop[0].z = ro->z + pos.z + oz;
		float sp = FxElem_Rand(650.0, 100.0);
		d->drop[2].x = sp * dir.x;
		d->drop[2].y = sp * dir.y;
		d->drop[2].z = sp * dir.z;
		float qz = FxElem_Rand(12.566370964050293, -6.2831854820251465);
		float qy = FxElem_Rand(12.566370964050293, -6.2831854820251465);
		d->drop[1].x = FxElem_Rand(12.566370964050293, -6.2831854820251465);
		d->drop[1].y = qy;
		d->drop[1].z = qz;
		float m = FxElem_Rand(0.5, 0.10000000149011612);
		float pz = FxElem_Rand(12.566370964050293, -6.2831854820251465);
		float py = FxElem_Rand(12.566370964050293, -6.2831854820251465);
		d->drop[3].y = py * m;
		d->drop[3].z = pz * m;
		d->drop[3].x = FxElem_Rand(12.566370964050293, -6.2831854820251465) * m;
		void *el = Debris_Element(d);
		if (el) {
			if (!PB_Started(el)) {
				PB_Started(el) = 1;
			}
			FXMisc_SetF8(el, 0.0500000007f);
			FXMisc_SetFC(el, 0.25f);
		}
	}
}

/* the bounce of a debris of the burp on hit (its normal at 0x24): slower, the spin turned, the life short once it
 * is slow, a random bounce sound */
void ChType_PoliceBossBrupBounce(cDebris *d, const byte *hit) {
	float k = FxElem_Rand(0.15000003576278687, 0.6499999761581421);
	vec3_t *vel = &d->drop[2];
	float vx = -vel->x;
	float vz = -vel->z;
	float vy = -vel->y;
	vel->y = k * vy;
	vel->x = k * vx;
	vel->z = k * vz;
	d->drop[1].x = -d->drop[1].x;
	d->drop[1].y = -d->drop[1].y;
	d->drop[1].z = -d->drop[1].z;
	Vec3_Reflect((const vec3_t *)(hit + 0x24), vel, vel);
	if (d->life > 0.25f && 10000.0f > vel->x * vel->x + vel->y * vel->y + vel->z * vel->z) {
		d->life = 0.25f;
	}
	if (!(0.0f >= g_PoliceBossBrupBounceSoundVol)) {
		int r = crt_rand();
		CSoundSys *snd = g_pSound;
		const char *s = (r & 1) ? g_PoliceBossBrupBounce1Sound : g_PoliceBossBrupBounce2Sound;
		int vol = Snd_Volume(g_PoliceBossBrupBounceSoundVol);
		snd->vftable->PlayAt(g_pSound, s, &d->drop[0], vol);
	}
}

cEntity *g_PoliceBossEnt;

/* params: the boss: Action when it does not fall and no action of priority runs */
void ChType_PoliceBossAction(cAbstractFile *params) {
	char name[0x80];

	cAbstractFile_ReadString(params, name, 0x80);
	g_PoliceBossEnt = World_FindEntity(name);
	CHECK(g_PoliceBossEnt);
	CHECK(Entity_IsType(g_PoliceBossEnt, TYEN_WALKCHAR));
	ChType_PoliceBossTryAction(g_PoliceBossEnt);
}

void ChType_PoliceBossTryAction(cEntity *v) {
	if (!WChar(v)->falling && !ChType_ActPriority(ChType_ActCtrl(v))) {
		cActCtrl_Play(ChType_ActCtrl(v), "Action", 0, 1.0f);
	}
}

/* params: the boss, an event, a float: ChType_PoliceBossSphereHit in PoliceBossActionRadius */
void ChType_PoliceBossBrup(cAbstractFile *params) {
	char name[0x80], event[0x80];
	vec3_t tmp;

	ChType_ReadParams(params, name, event);
	g_PoliceBossEnt = World_FindEntity(name);
	CHECK(g_PoliceBossEnt);
	CHECK(Entity_IsType(g_PoliceBossEnt, TYEN_WALKCHAR));
	cPy_Function_SetNative(&g_ChTypeSphereFunc, (nativefunc_t)ChType_PoliceBossSphereHit);
	cWorld_SphereCallF(g_pWorld, 0x10, g_PoliceBossEnt->vftable->GetPos(g_PoliceBossEnt, &tmp),
		g_PoliceBossActionRadius, &g_ChTypeSphereFunc, 0);
}

/* the sphere test of the burp: params a character. A police with a logic that runs no action of priority laughs
 * at the boss */
void ChType_PoliceBossSphereHit(cAbstractFile *params) {
	char name[0x80];

	CHECK(g_PoliceBossEnt);
	cAbstractFile_ReadString(params, name, 0x80);
	cEntity *e = World_FindEntity(name);
	CHECK(g_PoliceBossEnt);
	CHECK(Entity_IsType(g_PoliceBossEnt, TYEN_WALKCHAR));
	if (e && !strcmp(ChType_ClassName(e), "Police") && !ChType_ActPriority(ChType_ActCtrl(e)) &&
		Logic_GetLocalLogic(e)) {
		cActCtrl_Play(ChType_ActCtrl(e), "Laugh", 0, 1.0f);
		cEntity_AddLink(g_PoliceBossEnt, &WChar(e)->listen);
	}
}

cGVar GVar_PoliceBossBrupBaseFile;
cGVar GVar_PoliceBossBrupDebrisMax;
cGVar GVar_PoliceBossBrupBounce1Sound;
cGVar GVar_PoliceBossBrupDirRand;
cGVar GVar_PoliceBossActionRadius;
cGVar GVar_PoliceBossBrupPosY;
cGVar GVar_PoliceBossBrupPosX;
cGVar GVar_PoliceBossBrupBounceSoundVol;
cGVar GVar_PoliceBossBrupBounce2Sound;
cGVar GVar_PoliceBossBrupPosZ;
cGVar GVar_PoliceBossBrupDebrisMin;
cPy_NativeFunc NativeFunc_PoliceBossAction;
cPy_NativeFunc NativeFunc_PoliceBossBrup;
cPy_NativeFunc NativeFunc_FXPoliceBossBrup;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_FLOAT(Init_GVar_PoliceBossActionRadius, &GVar_PoliceBossActionRadius, "PoliceBossActionRadius", 0, "Radius of PoliceBoss fun FX to the polices.", 1000.0f)
FINI_GVAR_STRING(Fini_GVar_PoliceBossBrupBaseFile, &GVar_PoliceBossBrupBaseFile)
INIT_GVAR_STRING(Init_GVar_PoliceBossBrupBaseFile, Fini_GVar_PoliceBossBrupBaseFile, &GVar_PoliceBossBrupBaseFile, "PoliceBossBrupBaseFile", 0, "Default Model for Police Boss Brup Effect", "Models/Misc/NutsAndScrew/NutsAndScrew")
INIT_GVAR_FLOAT(Init_GVar_PoliceBossBrupPosX, &GVar_PoliceBossBrupPosX, "PoliceBossBrupPosX", 0, "Police Boss Brup Displacement from center", 5.0f)
INIT_GVAR_FLOAT(Init_GVar_PoliceBossBrupPosY, &GVar_PoliceBossBrupPosY, "PoliceBossBrupPosY", 0, "Police Boss Brup Displacement from center", 5.0f)
INIT_GVAR_FLOAT(Init_GVar_PoliceBossBrupPosZ, &GVar_PoliceBossBrupPosZ, "PoliceBossBrupPosZ", 0, "Police Boss Brup Displacement from center", -100.0f)
INIT_GVAR_FLOAT(Init_GVar_PoliceBossBrupDirRand, &GVar_PoliceBossBrupDirRand, "PoliceBossBrupDirRand", 0, "Police Boss Brup Random Direction factor", 0.15f)
INIT_GVAR_INT(Init_GVar_PoliceBossBrupDebrisMin, &GVar_PoliceBossBrupDebrisMin, "PoliceBossBrupDebrisMin", 0, "Police Boss Brup Min debris number", 4)
INIT_GVAR_INT(Init_GVar_PoliceBossBrupDebrisMax, &GVar_PoliceBossBrupDebrisMax, "PoliceBossBrupDebrisMax", 0, "Police Boss Brup Max debris number", 8)
FINI_GVAR_STRING(Fini_GVar_PoliceBossBrupBounce1Sound, &GVar_PoliceBossBrupBounce1Sound)
INIT_GVAR_STRING(Init_GVar_PoliceBossBrupBounce1Sound, Fini_GVar_PoliceBossBrupBounce1Sound, &GVar_PoliceBossBrupBounce1Sound, "PoliceBossBrupBounce1Sound", 0, "Police Boss Brup debris bounce sound1 name", "PoliceBossBrupBounce1Sound")
FINI_GVAR_STRING(Fini_GVar_PoliceBossBrupBounce2Sound, &GVar_PoliceBossBrupBounce2Sound)
INIT_GVAR_STRING(Init_GVar_PoliceBossBrupBounce2Sound, Fini_GVar_PoliceBossBrupBounce2Sound, &GVar_PoliceBossBrupBounce2Sound, "PoliceBossBrupBounce2Sound", 0, "Police Boss Brup debris bounce sound2 name", "PoliceBossBrupBounce2Sound")
INIT_GVAR_FLOAT(Init_GVar_PoliceBossBrupBounceSoundVol, &GVar_PoliceBossBrupBounceSoundVol, "PoliceBossBrupBounceSoundVol", 0, "Police Boss Brup debris bounce sound volume", 0.0f)
INIT_NATIVE_FUNC(Init_NativeFunc_PoliceBossAction, &NativeFunc_PoliceBossAction, "PoliceBossAction", ChType_PoliceBossAction)
INIT_NATIVE_FUNC(Init_NativeFunc_PoliceBossBrup, &NativeFunc_PoliceBossBrup, "PoliceBossBrup", ChType_PoliceBossBrup)
INIT_NATIVE_FUNC(Init_NativeFunc_FXPoliceBossBrup, &NativeFunc_FXPoliceBossBrup, "FXPoliceBossBrup", ChType_FXPoliceBossBrup)
