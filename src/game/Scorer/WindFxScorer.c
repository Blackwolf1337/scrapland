/*
 * WindFxScorer.cpp of the game (Menu/): cWindFxScorer, the wind noise of the ship by the free space around it. See
 * include/scrap/scorerhud.h.
 */
#include "scrap/gvar.h"
#include "scrap/objentity.h"
#include "scrap/scorerhud.h"
#include "scrap/scrapcollision.h"
#include "scrap/sound.h"
#include "scrap/vehicleentity.h"
#include "scrap/staticinit.h"

#define CWINDFXSCORER_VFTABLE (&cWindFxScorer_vftable)
#define g_WindFxSound (GVar_WindFxScorerSound.value.s)
static const cScorerItemVtbl cWindFxScorer_vftable;
static cGVar GVar_WindFxScorerSound;

cWindFxScorer *cWindFxScorer_ctor(cWindFxScorer *self) {
	cScorerItem_ctor(&self->base);
	self->base.vftable = CWINDFXSCORER_VFTABLE;
	cSound_Init(&self->sound);
	CPsaImage *img = operator_new(sizeof(CPsaImage));
	/* the color stays the one of the image */
	self->img = img ? CPsaImage_ctor(img, "2D/white.tga", 0x24) : NULL;
	cSound_SetSound(&self->sound, g_WindFxSound, 0);
	self->sound.desc.loop = 1;
	self->wind[1] = 0.0f;
	self->wind[0] = 0.0f;
	self->wind[2] = 0.0f;
	self->wind[3] = self->wind[1];
	return self;
}

static void cWindFxScorer_dtor(cWindFxScorer *self) {
	self->base.vftable = CWINDFXSCORER_VFTABLE;
	ScorerImg_Free(self->img);
	cSound_Stop(&self->sound);
	cScorerItem_dtor(&self->base);
}

static void cWindFxScorer_Free(cScorerItem *base) {
	cWindFxScorer *self = (cWindFxScorer *)base;
	cWindFxScorer_dtor(self);
	operator_delete(self);
}

static cGVar GVar_WindFxScorerFadeFactor;

/* the engine variables of the wind (WindFxScorer*) */
#define g_WindFxFadeFactor (GVar_WindFxScorerFadeFactor.value.f)
static cGVar GVar_WindFxScorerFwAngle;
#define g_WindFxFwAngle (GVar_WindFxScorerFwAngle.value.f)	/* degrees, the side tests turned by it */
static cGVar GVar_WindFxScorerHorizontal;
#define g_WindFxHorizontal (GVar_WindFxScorerHorizontal.value.f)	/* the length of the side tests at full speed */
static cGVar GVar_WindFxScorerVertical;
#define g_WindFxVertical (GVar_WindFxScorerVertical.value.f)
static cGVar GVar_WindFxScorerShow;
#define g_WindFxShow (GVar_WindFxScorerShow.value.i)
static cGVar GVar_WindFxScorerSoundVol;
#define g_WindFxSoundVol (GVar_WindFxScorerSoundVol.value.f)
static cGVar GVar_WindFxScorerSpeedPitch;
#define g_WindFxSpeedPitch (GVar_WindFxScorerSpeedPitch.value.f)
static cGVar GVar_WindFxScorerSpeedVol;
#define g_WindFxSpeedVol (GVar_WindFxScorerSpeedVol.value.f)

#define WINDFX_MAX_SPEED 28000.0f	/* the speed of the ship to 0..1 */
#define WINDFX_BAR 100.0f		/* the debug bars: the scale of 1 */

/* the free part 0..1 (below 0 when the way is shorter) of the test from pos along (x, y, 0) turned by ax, ay;
 * back tests the opposite way */
static float WindFx_Free(const vec3_t *pos, float ax, float ay, float x, float y, float range, float speed,
	byte back) {
	vec3_t in, r, to, hit;

	in.x = x;
	in.y = y;
	in.z = 0.0f;
	Vec3_RotateXY(&r, &in, ax, ay);
	if (back) {
		to.x = pos->x - r.x;
		to.y = pos->y - r.y;
		to.z = pos->z - r.z;
	} else {
		to.x = pos->x + r.x;
		to.y = r.y + pos->y;
		to.z = pos->z + r.z;
	}
	Engine_TestLine(pos, &to, &hit, NULL);
	float dy = hit.y - pos->y, dx = hit.x - pos->x, dz = hit.z - pos->z;
	float d = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
	return (range * speed - d) / range;
}

static void WindFx_Bar(CPsaImage *img, float sx, float sy, float x, float y) {
	img->scaleX = sx;
	img->scaleY = sy;
	img->color = 0xff0000ff;
	CPsaImage_Draw(img, x, y);
}

/* vf00: the free space around the ship of the camera (four line tests ahead to the sides, up and down) sets the
 * volume, pan and pitch of the wind */
static void cWindFxScorer_Draw(cScorerItem *base, int player) {
	cWindFxScorer *self = (cWindFxScorer *)base;
	cEntity *ship;
	vec3_t pos;
	float to[4];

	if (!ScorerHud_CamShip(player, &ship)) {
		return;
	}
	cVehicleEntity *v = Veh(ship);
	float speed = v->smoothSpeed;
	float now = Scorer_Get(player)->time;
	float dt = now - Scorer_Get(player)->lastTime;
	speed = speed / WINDFX_MAX_SPEED;
	Obj_GetViewPos(ship, &pos);
	float pan = 0.0f;
	to[0] = to[1] = to[2] = to[3] = 0.0f;
	if (pos.y > g_AtmosphereLimit) {
		speed = 0.0f;
	} else if (speed > 0.001f) {
		float ax = v->smoothAng.x, ay = v->smoothAng.y;
		float fw = (float)((double)g_WindFxFwAngle * 3.141592653589793 / 180.0);
		float h = g_WindFxHorizontal, vt = g_WindFxVertical;
		float f0 = WindFx_Free(&pos, ax, ay - fw, -(h * speed), 0.0f, h, speed, 0);
		float f1 = WindFx_Free(&pos, ax, ay + fw, -(h * speed), 0.0f, h, speed, 1);
		float f2 = WindFx_Free(&pos, ax + fw, ay, 0.0f, -(vt * speed), vt, speed, 0);
		float f3 = WindFx_Free(&pos, ax - fw, ay, 0.0f, vt * speed, vt, speed, 0);
		to[0] = Float_MaxSS(0.0f, f1);
		to[1] = Float_MaxSS(0.0f, f0);
		to[2] = Float_MaxSS(0.0f, f3);
		to[3] = Float_MaxSS(0.0f, f2);
	}
	for (int k = 0; k < 4; k++) {
		Float_Approach(&self->wind[k], to[k], g_WindFxFadeFactor * dt);
	}
	float *w = self->wind;
	float sv = g_WindFxSpeedVol * speed;
	float a = ((w[1] + w[1]) + (w[3] + w[2])) * (1.0f - sv) * 0.25f + sv;
	float b = ((w[0] + w[0]) + (w[3] + w[2])) * (1.0f - sv) * 0.25f + sv;
	float sum = a + b;
	float vol = sum * g_WindFxSoundVol * 0.5f;
	if (!(0.001 >= (double)sum)) {
		pan = (b - a) / sum;
	}
	self->sound.desc.volume = Snd_Volume(vol);
	self->sound.desc.pan = (int)(pan * -10000.0f);
	self->sound.desc.freq = 1.0f;
	cSound_SetPitch(&self->sound, g_WindFxSpeedPitch * speed);
	cSound_Update(&self->sound);
	if (!g_WindFxShow) {
		return;
	}
	WindFx_Bar(self->img, w[1] * WINDFX_BAR, 20.0f, 340.0f, 230.0f);
	WindFx_Bar(self->img, w[0] * WINDFX_BAR, 20.0f, 300.0f - w[0] * WINDFX_BAR, 230.0f);
	WindFx_Bar(self->img, 20.0f, w[3] * WINDFX_BAR, 310.0f, 260.0f);
	WindFx_Bar(self->img, 20.0f, w[2] * WINDFX_BAR, 310.0f, 220.0f - w[2] * WINDFX_BAR);
	float l = (pan + 1.0f) * vol * WINDFX_BAR;
	WindFx_Bar(self->img, 20.0f, l, 200.0f, 220.0f - l);
	float r = (1.0f - pan) * vol * WINDFX_BAR;
	WindFx_Bar(self->img, 20.0f, r, 400.0f, 220.0f - r);
}

/* the vtable of cWindFxScorer */
static const cScorerItemVtbl cWindFxScorer_vftable = {
	.Draw = cWindFxScorer_Draw,
	.SetParam = cScorerItem_SetParam,
	.Var = cScorerItem_Var,
	.Free = cWindFxScorer_Free,
};

static cGVar GVar_WindFxScorerSound;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
FINI_GVAR_STRING(Fini_GVar_WindFxScorerSound, &GVar_WindFxScorerSound)
INIT_GVAR_STRING(Init_GVar_WindFxScorerSound, Fini_GVar_WindFxScorerSound, &GVar_WindFxScorerSound, "WindFxScorerSound", 0, "Sound name for Wind FX", "FastFXWind")
INIT_GVAR_FLOAT(Init_GVar_WindFxScorerSoundVol, &GVar_WindFxScorerSoundVol, "WindFxScorerSoundVol", 0, "Sound Volume for Wind FX", 1.0f)
INIT_GVAR_FLOAT(Init_GVar_WindFxScorerSpeedVol, &GVar_WindFxScorerSpeedVol, "WindFxScorerSpeedVol", 0, "Sound Volume allways if speed factor", 3.0f)
INIT_GVAR_FLOAT(Init_GVar_WindFxScorerSpeedPitch, &GVar_WindFxScorerSpeedPitch, "WindFxScorerSpeedPitch", 0, "semitones for speed", 5.0f)
INIT_GVAR_FLOAT(Init_GVar_WindFxScorerHorizontal, &GVar_WindFxScorerHorizontal, "WindFxScorerHorizontal", 0, "Horizontal check for Wind FX", 10000.0f)
INIT_GVAR_FLOAT(Init_GVar_WindFxScorerVertical, &GVar_WindFxScorerVertical, "WindFxScorerVertical", 0, "Vertical   check for Wind FX", 5000.0f)
INIT_GVAR_FLOAT(Init_GVar_WindFxScorerFadeFactor, &GVar_WindFxScorerFadeFactor, "WindFxScorerFadeFactor", 0, "Fade in and out for Wind FX", 5.0f)
INIT_GVAR_FLOAT(Init_GVar_WindFxScorerFwAngle, &GVar_WindFxScorerFwAngle, "WindFxScorerFwAngle", 0, "Foward angle Wind FX tests", 45.0f)
INIT_GVAR_INT(Init_GVar_WindFxScorerShow, &GVar_WindFxScorerShow, "WindFxScorerShow", 0, "Shows Wind FX info", 0)
