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

/* the engine variables (Gvars) */
extern cGVar GVar_FXCharacterConversionFile;
#define g_FXCharacterConversionFile (GVar_FXCharacterConversionFile.value.s)
extern cGVar GVar_FXCharacterConversorFile;
#define g_FXCharacterConversorFile (GVar_FXCharacterConversorFile.value.s)
extern cGVar GVar_FXCharacterBlaBliBlaFile;
#define g_FXCharacterBlaBliBlaFile (GVar_FXCharacterBlaBliBlaFile.value.s)
extern cGVar GVar_FXCharacterDazedFile;
#define g_FXCharacterDazedFile (GVar_FXCharacterDazedFile.value.s)
extern cGVar GVar_FXCharacterDeathFile;
#define g_FXCharacterDeathFile (GVar_FXCharacterDeathFile.value.s)
extern cGVar GVar_FXCharacterTeleportFile;
#define g_FXCharacterTeleportFile (GVar_FXCharacterTeleportFile.value.s)
extern cGVar GVar_FXMoneyTransferFile;
#define g_FXMoneyTransferFile (GVar_FXMoneyTransferFile.value.s)
extern cGVar GVar_FXFunctionarySpecialActionFile;
#define g_FXFunctionarySpecialActionFile (GVar_FXFunctionarySpecialActionFile.value.s)
extern cGVar GVar_FunctionarySpecialActionTime;
#define g_FunctionarySpecialActionTime (GVar_FunctionarySpecialActionTime.value.f)
extern cGVar GVar_FXCharacterDeathMinDist;
#define g_FXCharacterDeathMinDist (GVar_FXCharacterDeathMinDist.value.f)
extern cGVar GVar_CharacterDeathSound;
#define g_CharacterDeathSound (GVar_CharacterDeathSound.value.s)
extern cGVar GVar_CharacterDeathSoundVol;
#define g_CharacterDeathSoundVol (GVar_CharacterDeathSoundVol.value.f)
extern cGVar GVar_CharacterConversionSound;
#define g_CharacterConversionSound (GVar_CharacterConversionSound.value.s)
extern cGVar GVar_CharacterConversionSoundVol;
#define g_CharacterConversionSoundVol (GVar_CharacterConversionSoundVol.value.f)
extern cGVar GVar_CharacterTeleportUpSound;
#define g_CharacterTeleportUpSound (GVar_CharacterTeleportUpSound.value.s)
extern cGVar GVar_CharacterTeleportDownSound;
#define g_CharacterTeleportDownSound (GVar_CharacterTeleportDownSound.value.s)
extern cGVar GVar_CharacterTeleportSoundVol;
#define g_CharacterTeleportSoundVol (GVar_CharacterTeleportSoundVol.value.f)
extern cGVar GVar_MaintenanceWorkSound;
#define g_MaintenanceWorkSound (GVar_MaintenanceWorkSound.value.s)
extern cGVar GVar_MaintenanceWorkSoundVol;
#define g_MaintenanceWorkSoundVol (GVar_MaintenanceWorkSoundVol.value.f)
extern cGVar GVar_MaintenanceWorkSoundAttIni;
#define g_MaintenanceWorkSoundAttIni (GVar_MaintenanceWorkSoundAttIni.value.f)
extern cGVar GVar_MaintenanceWorkSoundAttEnd;
#define g_MaintenanceWorkSoundAttEnd (GVar_MaintenanceWorkSoundAttEnd.value.f)

/* a character in the conversion: its byte at 0x48c is set while the effect runs */
#define Entity_InConversion(e) (WChar(e)->disableControl)

/* the elements */

typedef struct cObjElementFXCharacterConversion {
	void *vftable;			/* 0x000 */
	cEntity *attacker;		/* 0x004 */
	cEntity *attacked;		/* 0x008 */
	CGameParticles particles;	/* 0x00c on Dummy01 */
	CGameParticles particles2;	/* 0x0bc on Dummy02 */
	float time;			/* 0x16c since the start */
	float emitT;			/* 0x170 since the last sparks */
	byte pad_174[4];
	float unused_178;			/* 0x178 */
	CGameTrail trails[FXCONVERSION_TRAILS];	/* 0x17c */
	vec3_t dir[FXCONVERSION_POINTS];	/* 0x190 random directions of length 0.3..1 */
	float phase[FXCONVERSION_POINTS];	/* 0x514 0..1 */
	float speed[FXCONVERSION_POINTS];	/* 0x640 12..24 */
	float scale[FXCONVERSION_POINTS];	/* 0x76c 0.7..1.5 */
	byte started;			/* 0x898 */
} cObjElementFXCharacterConversion;
SD_OFFSET(cObjElementFXCharacterConversion, trails, 0x17c);
SD_OFFSET(cObjElementFXCharacterConversion, phase, 0x514);
SD_OFFSET(cObjElementFXCharacterConversion, started, 0x898);
SD_SIZE(cObjElementFXCharacterConversion, 0x89c);

typedef struct cObjElementFXCharacterConversor {
	void *vftable;			/* 0x000 */
	cEntity *conversor;		/* 0x004 */
	cEntity *character;		/* 0x008 */
	CGameParticles particles;	/* 0x00c on Dummy01 */
	int phase;			/* 0x0bc 1 in (the character disabled at the end), 2 out */
	float duration;			/* 0x0c0 */
	float timeIn;			/* 0x0c4 the fade of phase 1 */
	float timeOut;			/* 0x0c8 the fade of phase 2 */
	float time;			/* 0x0cc since the start */
	float emitT;			/* 0x0d0 since the last particles */
	byte pad_d4[4];
	int nodeWall[3];		/* 0x0d8 DF_Pared_01..03 of the conversor */
	int nodeExit[3];		/* 0x0e4 DF_Salida_01..03 */
	int nodeLight[4];		/* 0x0f0 DF_Light_01..04 */
	int numTrails;			/* 0x100 */
	float trailT[FXCONVERSOR_TRAILS];	/* 0x104 the life of the lightning, a new one at 0 */
	CGameTrail trails[FXCONVERSOR_TRAILS];	/* 0x110 */
	vec3_t pts[FXCONVERSOR_TRAILS][FXCONVERSOR_POINTS];	/* 0x11c the control points of the lightnings */
	float width[FXCONVERSOR_TRAILS];	/* 0x284 */
	int numPoints;			/* 0x290 the control points used, 5 */
	byte started;			/* 0x294 */
} cObjElementFXCharacterConversor;
SD_OFFSET(cObjElementFXCharacterConversor, nodeWall, 0xd8);
SD_OFFSET(cObjElementFXCharacterConversor, trails, 0x110);
SD_OFFSET(cObjElementFXCharacterConversor, pts, 0x11c);
SD_OFFSET(cObjElementFXCharacterConversor, width, 0x284);
SD_SIZE(cObjElementFXCharacterConversor, 0x298);

typedef struct cObjElementFXCharacterDeath {
	void *vftable;			/* 0x000 */
	cEntity *ent;			/* 0x004 */
	CGameParticles particles;	/* 0x008 on Dummy01 */
	CGameParticles particles2;	/* 0x0b8 on Dummy02 */
	CGameParticles particles3;	/* 0x168 on Dummy03 */
	float time;			/* 0x218 since the start */
	float emitT;			/* 0x21c since the last particles */
	float lastTime;			/* 0x220 time of the last Update */
	CGameTrail trails[FXDEATH_TRAILS];	/* 0x224 15 points each */
	vec3_t pos[FXDEATH_POINTS];	/* 0x244 the points of the trails, on vertices of the model */
	float phase[FXDEATH_POINTS];	/* 0x7e4 0..1 up along the trail, 5 at the start (a new point) */
	float width[FXDEATH_POINTS];	/* 0x9c4 */
	float speed[FXDEATH_POINTS];	/* 0xba4 of the phase */
	uint32_t color[FXDEATH_POINTS];	/* 0xd84 */
	byte started;			/* 0xf64 */
} cObjElementFXCharacterDeath;
SD_OFFSET(cObjElementFXCharacterDeath, trails, 0x224);
SD_OFFSET(cObjElementFXCharacterDeath, phase, 0x7e4);
SD_OFFSET(cObjElementFXCharacterDeath, color, 0xd84);
SD_SIZE(cObjElementFXCharacterDeath, 0xf68);

typedef struct cObjElementFXCharacterTeleport {
	void *vftable;			/* 0x00 */
	cEntity *teleport;		/* 0x04 */
	cEntity *character;		/* 0x08 */
	CGameParticles particles;	/* 0x0c on Dummy01 */
	int mode;			/* 0xbc 1 up, 2 down */
	float duration;			/* 0xc0 */
	float timeIn;			/* 0xc4 the fade of the character at the start (mode 1) */
	float timeOut;			/* 0xc8 the fade at the end (mode 2) */
	float t;			/* 0xcc since the start */
	float emitT;			/* 0xd0 since the last particles */
	byte started;			/* 0xd4 */
} cObjElementFXCharacterTeleport;
SD_OFFSET(cObjElementFXCharacterTeleport, mode, 0xbc);
SD_SIZE(cObjElementFXCharacterTeleport, 0xd8);

typedef struct cObjElementFXMaintenanceWork {
	void *vftable;			/* 0x00 */
	cEntity *maintenance;		/* 0x04 the robot */
	cEntity *ship;			/* 0x08 */
	CGameParticles particles;	/* 0x0c on Dummy01 */
	float emitTime;		/* 0xbc the particles are emitted every 0.04 s of it */
	float time;			/* 0xc0 since the start */
	float life;			/* 0xc4 the life of the debris at the start */
	CGameTrail trails[2];		/* 0xc8 */
	int vertex;			/* 0xd0 the vertex of the ship nearest to the laser */
	int nodeLaser;			/* 0xd4 DF_Laser of the robot */
	int sound;			/* 0xd8 the handle of the work sound */
	byte started;			/* 0xdc */
} cObjElementFXMaintenanceWork;
SD_OFFSET(cObjElementFXMaintenanceWork, trails, 0xc8);
SD_OFFSET(cObjElementFXMaintenanceWork, sound, 0xd8);
SD_SIZE(cObjElementFXMaintenanceWork, 0xe0);

typedef struct cObjElementFXMoneyTransfer {
	void *vftable;			/* 0x00 */
	cEntity *from;			/* 0x04 */
	cEntity *to;			/* 0x08 */
	CGameParticles particles;	/* 0x0c on Dummy01 */
	float emitT;			/* 0xbc the time since the last coin */
	float unused_c0;			/* 0xc0 */
	int unused_c4;			/* 0xc4 */
	byte started;			/* 0xc8 */
} cObjElementFXMoneyTransfer;
SD_OFFSET(cObjElementFXMoneyTransfer, emitT, 0xbc);
SD_SIZE(cObjElementFXMoneyTransfer, 0xcc);

typedef struct cObjElementFXSputnik {
	void *vftable;			/* 0x00 */
	cEntity *ent;			/* 0x04 */
	CGameParticles particles;	/* 0x08 on Dummy01 */
	float emitT;			/* 0xb8 the time since the last emission */
	float t;			/* 0xbc */
	float duration;			/* 0xc0 0: no end */
	int node;			/* 0xc4 "Bip Spootnick MASTER" of the model */
	float minMove;			/* 0xc8 */
	vec3_t last;			/* 0xcc the last position of the node */
	float moveT;			/* 0xd8 */
	byte started;			/* 0xdc */
} cObjElementFXSputnik;
SD_OFFSET(cObjElementFXSputnik, duration, 0xc0);
SD_OFFSET(cObjElementFXSputnik, last, 0xcc);
SD_SIZE(cObjElementFXSputnik, 0xe0);

/* World/FX/FXCharActions.cpp: the start of the Functionary special action on the element of e */
void cObjElementFXFunctionarySpecialAction_Start(void *self, cEntity *e);

/* the element methods; the same code of several classes is one function */
void cObjElementFXCharacterConversion_Update(cObjElementFX *base, cDebris *d, float dt);
byte cObjElementFXCharacterConversion_Alive(cObjElementFX *base, cDebris *d, cDebrisSys *sys);
void cObjElementFXCharacterConversion_Attach(cObjElementFX *base, cDebris *d);
void cObjElementFXCharacterConversion_Reset(cObjElementFX *base, cDebris *arg);
void cObjElementFXCharacterConversion_SetAttacker(cObjElementFXCharacterConversion *self, cEntity *e);
void cObjElementFXCharacterConversion_SetAttacked(cObjElementFXCharacterConversion *self, cEntity *e);
/* the particles of the start between the two characters */
void cObjElementFXCharacterConversion_Emit(cObjElementFXCharacterConversion *self);
byte cObjElementFXCharacterConversor_Alive(cObjElementFX *base, cDebris *d, cDebrisSys *sys);
/* the conversor machine: the character fades in or out, light from the nodes of the conversor, lightnings from its
 * walls to the exits, glows over the vertices of the character; at the end of the animation the particles go */
void cObjElementFXCharacterConversor_Update(cObjElementFX *base, cDebris *d, float dt);
void cObjElementFXCharacterConversor_Attach(cObjElementFX *base, cDebris *d);
void cObjElementFXCharacterConversor_Reset(cObjElementFX *base, cDebris *arg);
void cObjElementFXCharacterConversor_SetCharacter(cObjElementFXCharacterConversor *self, cEntity *e);
/* the conversor and the nodes of its model */
void cObjElementFXCharacterConversor_SetConversor(cObjElementFXCharacterConversor *self, cEntity *e);
/* the character dies: it slows down and fades, trails and smoke go up from its vertices, flashes and sparks */
void cObjElementFXCharacterDeath_Update(cObjElementFX *base, cDebris *d, float dt);
void cObjElementFXCharacterDeath_Attach(cObjElementFX *base, cDebris *d);
void cObjElementFXCharacterDeath_Reset(cObjElementFX *base, cDebris *arg);
void cObjElementFXCharacterDeath_SetEntity(cObjElementFXCharacterDeath *self, cEntity *e);
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
void cObjElementFXMoneyTransfer_SetEntities(cObjElementFXMoneyTransfer *self, cEntity *from, cEntity *to);
void cObjElementFXSputnik_Update(cObjElementFX *base, cDebris *d, float dt);
byte cObjElementFXSputnik_Alive(cObjElementFX *base, cDebris *d, cDebrisSys *sys);
void cObjElementFXSputnik_Attach(cObjElementFX *base, cDebris *d);
void cObjElementFXSputnik_Reset(cObjElementFX *base, cDebris *arg);
/* the entity and the node "Bip Spootnick MASTER" of its model */
void FXSputnik_Start(cObjElementFXSputnik *self, cEntity *e);
void FXSputnik_SetA(cObjElementFXSputnik *self, float v);	/* the duration */
void FXSputnik_SetB(cObjElementFXSputnik *self, float v);	/* minMove */
/* dead code: the int at 0x4 */
void FXChar_SetI4(void *self, int v);

/* the free functions */
/* the effects of a character by name: CharacterDeath, CharacterSputnikHello, Bye, Interface and Junkyard,
 * FunctionarySpecialAction */
void FX_Character(cEntity *e, const char *name);
/* the native functions (the character name, a second name and an int from the parameter file) */
void FX_CharacterBlaBliBla(cAbstractFile *params);
void FX_CharacterDazed(cAbstractFile *params);
void FX_CharacterTeleport(cAbstractFile *params);

#endif
