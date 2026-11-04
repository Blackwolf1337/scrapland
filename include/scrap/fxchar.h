/*
 * World/FX/FXChar.cpp of the game: the effects of the characters, elements (cObjElementFX) on the debris of a debris
 * system like the ones of include/scrap/fxmisc.h: the conversion of a character by another (CharacterConversion), the
 * conversor machine (CharacterConversor), the death, the teleport, the work of the maintenance robots on a ship,
 * the money flying from one character to another and Sputnik. Also the effects started by name (FX_Character) and the
 * native functions FXCharacterBlaBliBla, FXCharacterDazed and FXCharacterTeleport.
 * The element vtable: 0 Delete, 1 Update (debris, dt), 2 Alive (debris, sys), 3 Attach (debris), 4 Reset.
 * The constructors and destructors are in src/game/World/FX/FXMisc.c.
 */
#ifndef SCRAP_FXCHAR_H
#define SCRAP_FXCHAR_H

#include "scrap/gvar.h"
#include "scrap/filesys.h"
#include "scrap/fxmisc.h"

#define FXCONVERSION_TRAILS 5
#define FXCONVERSION_POINTS 75
#define FXCONVERSOR_TRAILS 3
#define FXCONVERSOR_POINTS 10
#define FXDEATH_TRAILS 8
#define FXDEATH_TRAILPOINTS 15
#define FXDEATH_POINTS (FXDEATH_TRAILS * FXDEATH_TRAILPOINTS)

extern cGVar GVar_FunctionarySpecialActionTime;
#define g_FunctionarySpecialActionTime (GVar_FunctionarySpecialActionTime.value.f)

/* a character in the conversion: its byte at 0x48c is set while the effect runs */
#define Entity_InConversion(e) (WChar(e)->disableControl)

/* the elements */

typedef struct cObjElementFXCharacterConversion {
	const cObjElementFXVtbl *vftable;
	cEntity *attacker;
	cEntity *attacked;
	CGameParticles particles;	/* on Dummy01 */
	CGameParticles particles2;	/* on Dummy02 */
	float time;			/* since the start */
	float emitT;			/* since the last sparks */
	byte pad_174[4];
	float unused_178;
	CGameTrail trails[FXCONVERSION_TRAILS];
	vec3_t dir[FXCONVERSION_POINTS];	/* random directions of length 0.3..1 */
	float phase[FXCONVERSION_POINTS];	/* 0..1 */
	float speed[FXCONVERSION_POINTS];	/* 12..24 */
	float scale[FXCONVERSION_POINTS];	/* 0.7..1.5 */
	byte started;
} cObjElementFXCharacterConversion;

typedef struct cObjElementFXCharacterConversor {
	const cObjElementFXVtbl *vftable;
	cEntity *conversor;
	cEntity *character;
	CGameParticles particles;	/* on Dummy01 */
	int phase;			/* 1 in (the character disabled at the end), 2 out */
	float duration;
	float timeIn;			/* the fade of phase 1 */
	float timeOut;			/* the fade of phase 2 */
	float time;			/* since the start */
	float emitT;			/* since the last particles */
	byte pad_d4[4];
	int nodeWall[3];		/* DF_Pared_01..03 of the conversor */
	int nodeExit[3];		/* DF_Salida_01..03 */
	int nodeLight[4];		/* DF_Light_01..04 */
	int numTrails;
	float trailT[FXCONVERSOR_TRAILS];	/* the life of the lightning, a new one at 0 */
	CGameTrail trails[FXCONVERSOR_TRAILS];
	vec3_t pts[FXCONVERSOR_TRAILS][FXCONVERSOR_POINTS];	/* the control points of the lightnings */
	float width[FXCONVERSOR_TRAILS];
	int numPoints;			/* the control points used, 5 */
	byte started;
} cObjElementFXCharacterConversor;

typedef struct cObjElementFXCharacterDeath {
	const cObjElementFXVtbl *vftable;
	cEntity *ent;
	CGameParticles particles;	/* on Dummy01 */
	CGameParticles particles2;	/* on Dummy02 */
	CGameParticles particles3;	/* on Dummy03 */
	float time;			/* since the start */
	float emitT;			/* since the last particles */
	float lastTime;			/* time of the last Update */
	CGameTrail trails[FXDEATH_TRAILS];	/* 15 points each */
	vec3_t pos[FXDEATH_POINTS];	/* the points of the trails, on vertices of the model */
	float phase[FXDEATH_POINTS];	/* 0..1 up along the trail, 5 at the start (a new point) */
	float width[FXDEATH_POINTS];
	float speed[FXDEATH_POINTS];	/* of the phase */
	uint32_t color[FXDEATH_POINTS];
	byte started;
} cObjElementFXCharacterDeath;

typedef struct cObjElementFXCharacterTeleport {
	const cObjElementFXVtbl *vftable;
	cEntity *teleport;
	cEntity *character;
	CGameParticles particles;	/* on Dummy01 */
	int mode;			/* 1 up, 2 down */
	float duration;
	float timeIn;			/* the fade of the character at the start (mode 1) */
	float timeOut;			/* the fade at the end (mode 2) */
	float t;			/* since the start */
	float emitT;			/* since the last particles */
	byte started;
} cObjElementFXCharacterTeleport;

typedef struct cObjElementFXMaintenanceWork {
	const cObjElementFXVtbl *vftable;
	cEntity *maintenance;		/* the robot */
	cEntity *ship;
	CGameParticles particles;	/* on Dummy01 */
	float emitTime;		/* the particles are emitted every 0.04 s of it */
	float time;			/* since the start */
	float life;			/* the life of the debris at the start */
	CGameTrail trails[2];
	int vertex;			/* the vertex of the ship nearest to the laser */
	int nodeLaser;			/* DF_Laser of the robot */
	int sound;			/* the handle of the work sound */
	byte started;
} cObjElementFXMaintenanceWork;

typedef struct cObjElementFXMoneyTransfer {
	const cObjElementFXVtbl *vftable;
	cEntity *from;
	cEntity *to;
	CGameParticles particles;	/* on Dummy01 */
	float emitT;			/* the time since the last coin */
	float unused_c0;
	int unused_c4;
	byte started;
} cObjElementFXMoneyTransfer;

typedef struct cObjElementFXSputnik {
	const cObjElementFXVtbl *vftable;
	cEntity *ent;
	CGameParticles particles;	/* on Dummy01 */
	float emitT;			/* the time since the last emission */
	float t;
	float duration;			/* 0: no end */
	int node;			/* "Bip Spootnick MASTER" of the model */
	float minMove;
	vec3_t last;			/* the last position of the node */
	float moveT;
	byte started;
} cObjElementFXSputnik;

/* World/FX/FXCharActions.cpp: the start of the Functionary special action on the element of e */
void cObjElementFXFunctionarySpecialAction_Start(void *self, cEntity *e);

/* the element methods; the same code of several classes is one function */
void cObjElementFXCharacterConversion_Update(cObjElementFX *base, cDebris *d, float dt);
byte cObjElementFXCharacterConversion_Alive(cObjElementFX *base, cDebris *d, cDebrisSys *sys);
void cObjElementFXCharacterConversion_Attach(cObjElementFX *base, cDebris *d);
void cObjElementFXCharacterConversion_Reset(cObjElementFX *base, cDebris *arg);
byte cObjElementFXCharacterConversor_Alive(cObjElementFX *base, cDebris *d, cDebrisSys *sys);
/* the conversor machine: the character fades in or out, light from the nodes of the conversor, lightnings from its
 * walls to the exits, glows over the vertices of the character; at the end of the animation the particles go */
void cObjElementFXCharacterConversor_Update(cObjElementFX *base, cDebris *d, float dt);
void cObjElementFXCharacterConversor_Attach(cObjElementFX *base, cDebris *d);
void cObjElementFXCharacterConversor_Reset(cObjElementFX *base, cDebris *arg);
/* the character dies: it slows down and fades, trails and smoke go up from its vertices, flashes and sparks */
void cObjElementFXCharacterDeath_Update(cObjElementFX *base, cDebris *d, float dt);
void cObjElementFXCharacterDeath_Attach(cObjElementFX *base, cDebris *d);
void cObjElementFXCharacterDeath_Reset(cObjElementFX *base, cDebris *arg);
/* the light column and the fade of the character, glows over its vertices, sparks and smoke */
void cObjElementFXCharacterTeleport_Update(cObjElementFX *base, cDebris *d, float dt);
byte cObjElementFXCharacterTeleport_Alive(cObjElementFX *base, cDebris *d, cDebrisSys *sys);
void cObjElementFXCharacterTeleport_Attach(cObjElementFX *base, cDebris *d);
void cObjElementFXCharacterTeleport_Reset(cObjElementFX *base, cDebris *arg);
byte cObjElementFXMaintenanceWork_Alive(cObjElementFX *base, cDebris *d, cDebrisSys *sys);
void cObjElementFXMaintenanceWork_Update(cObjElementFX *base, cDebris *d, float dt);
void cObjElementFXMaintenanceWork_Attach(cObjElementFX *base, cDebris *d);
void cObjElementFXMaintenanceWork_Reset(cObjElementFX *base, cDebris *arg);
void cObjElementFXMaintenanceWork_SetMaintenance(cObjElementFXMaintenanceWork *self, cEntity *e);
void cObjElementFXMaintenanceWork_SetShip(cObjElementFXMaintenanceWork *self, cEntity *e);
/* the laser vertex of the ship and the work sound, at the debris d */
void cObjElementFXMaintenanceWork_Start(cObjElementFXMaintenanceWork *self, cDebris *d);
void cObjElementFXMoneyTransfer_Update(cObjElementFX *base, cDebris *d, float dt);
void cObjElementFXMoneyTransfer_Attach(cObjElementFX *base, cDebris *d);
void cObjElementFXMoneyTransfer_Reset(cObjElementFX *base, cDebris *arg);
void cObjElementFXSputnik_Update(cObjElementFX *base, cDebris *d, float dt);
byte cObjElementFXSputnik_Alive(cObjElementFX *base, cDebris *d, cDebrisSys *sys);
void cObjElementFXSputnik_Attach(cObjElementFX *base, cDebris *d);
void cObjElementFXSputnik_Reset(cObjElementFX *base, cDebris *arg);
/* the entity and the node "Bip Spootnick MASTER" of its model */
void FXSputnik_Start(cObjElementFXSputnik *self, cEntity *e);
void FXSputnik_SetA(cObjElementFXSputnik *self, float v);	/* the duration */
void FXSputnik_SetB(cObjElementFXSputnik *self, float v);	/* minMove */
/* dead code: the int at 0x4 */

/* the free functions */
/* the effects of a character by name: CharacterDeath, CharacterSputnikHello, Bye, Interface and Junkyard,
 * FunctionarySpecialAction */
void FX_Character(cEntity *e, const char *name);

#endif
