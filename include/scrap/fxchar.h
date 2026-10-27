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

#include "scrap/filesys.h"
#include "scrap/fxmisc.h"

#define FXCONVERSION_TRAILS 5
#define FXCONVERSION_POINTS 75
#define FXCONVERSOR_TRAILS 3
#define FXDEATH_TRAILS 8

/* the engine variables (Gvars) */
#define g_FXCharacterConversionFile GAME_DATA(const char *, 0x466e78)
#define g_FXCharacterConversorFile GAME_DATA(const char *, 0x466e60)
#define g_FXCharacterBlaBliBlaFile GAME_DATA(const char *, 0x466ec0)
#define g_FXCharacterDazedFile GAME_DATA(const char *, 0x466ef0)
#define g_FXCharacterDeathFile GAME_DATA(const char *, 0x466f88)
#define g_FXCharacterTeleportFile GAME_DATA(const char *, 0x466fb8)
#define g_FXMoneyTransferFile GAME_DATA(const char *, 0x466f28)
#define g_FXFunctionarySpecialActionFile GAME_DATA(const char *, 0x467044)
#define g_FunctionarySpecialActionTime GAME_DATA(float, 0x44cf30)
#define g_FXCharacterDeathMinDist GAME_DATA(float, 0x466ea8)
#define g_CharacterDeathSound GAME_DATA(const char *, 0x466e90)
#define g_CharacterDeathSoundVol GAME_DATA(float, 0x466f40)
#define g_CharacterConversionSound GAME_DATA(const char *, 0x466ed8)
#define g_CharacterConversionSoundVol GAME_DATA(float, 0x466fa0)
#define g_CharacterTeleportUpSound GAME_DATA(const char *, 0x466f58)
#define g_CharacterTeleportDownSound GAME_DATA(const char *, 0x466f70)
#define g_CharacterTeleportSoundVol GAME_DATA(float, 0x466e20)
#define g_MaintenanceWorkSound GAME_DATA(const char *, 0x446b4c)
#define g_MaintenanceWorkSoundVol GAME_DATA(float, 0x446b34)
#define g_MaintenanceWorkSoundAttIni GAME_DATA(float, 0x446b74)
#define g_MaintenanceWorkSoundAttEnd GAME_DATA(float, 0x446ba4)

/* a character in the conversion: its byte at 0x48c is set while the effect runs */
#define Entity_InConversion(e) (*(byte *)((byte *)(e) + 0x48c))

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
	float f178;			/* 0x178 */
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
	float fc4;			/* 0x0c4 */
	float fc8;			/* 0x0c8 */
	float fcc;			/* 0x0cc */
	float fd0;			/* 0x0d0 */
	byte pad_d4[4];
	int nodePared[3];		/* 0x0d8 DF_Pared_01..03 of the conversor */
	int nodeSalida[3];		/* 0x0e4 DF_Salida_01..03 */
	int nodeLight[4];		/* 0x0f0 DF_Light_01..04 */
	int numTrails;			/* 0x100 */
	float trailT[FXCONVERSOR_TRAILS];	/* 0x104 */
	CGameTrail trails[FXCONVERSOR_TRAILS];	/* 0x110 */
	byte pad_11c[0x284 - 0x11c];
	float f284[FXCONVERSOR_TRAILS];	/* 0x284 */
	int n290;			/* 0x290 */
	byte started;			/* 0x294 */
} cObjElementFXCharacterConversor;
SD_OFFSET(cObjElementFXCharacterConversor, nodePared, 0xd8);
SD_OFFSET(cObjElementFXCharacterConversor, trails, 0x110);
SD_OFFSET(cObjElementFXCharacterConversor, f284, 0x284);
SD_SIZE(cObjElementFXCharacterConversor, 0x298);

typedef struct cObjElementFXCharacterDeath {
	void *vftable;			/* 0x000 */
	cEntity *ent;			/* 0x004 */
	CGameParticles particles;	/* 0x008 on Dummy01 */
	CGameParticles particles2;	/* 0x0b8 on Dummy02 */
	CGameParticles particles3;	/* 0x168 on Dummy03 */
	float f218;			/* 0x218 */
	float f21c;			/* 0x21c */
	float f220;			/* 0x220 */
	CGameTrail trails[FXDEATH_TRAILS];	/* 0x224 */
	byte pad_244[0x7e4 - 0x244];
	float f7e4[0x78];		/* 0x7e4 5 at the start */
	byte pad_9c4[0xf64 - 0x9c4];
	byte started;			/* 0xf64 */
} cObjElementFXCharacterDeath;
SD_OFFSET(cObjElementFXCharacterDeath, trails, 0x224);
SD_OFFSET(cObjElementFXCharacterDeath, f7e4, 0x7e4);
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
	float fbc;			/* 0xbc */
	float fc0;			/* 0xc0 */
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
	float fc0;			/* 0xc0 */
	int fc4;			/* 0xc4 */
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

/* World/FX/FXCharActions.cpp (rva 0x1c6b30): the start of the Functionary special action on the element of e */
void __thiscall cObjElementFXFunctionarySpecialAction_Start(void *self, cEntity *e);

/* the element methods; the same code of several classes is one function */
void __thiscall cObjElementFXCharacterConversion_Update(cObjElementFXCharacterConversion *self, cDebris *d, float dt);
byte __thiscall cObjElementFXCharacterConversion_Alive(cObjElementFXCharacterConversion *self, cDebris *d,
	cDebrisSys *sys);
void __thiscall cObjElementFXCharacterConversion_Attach(cObjElementFXCharacterConversion *self, cDebris *d);
void __thiscall cObjElementFXCharacterConversion_Reset(cObjElementFXCharacterConversion *self, int arg);
void __thiscall cObjElementFXCharacterConversion_SetAttacker(cObjElementFXCharacterConversion *self, cEntity *e);
void __thiscall cObjElementFXCharacterConversion_SetAttacked(cObjElementFXCharacterConversion *self, cEntity *e);
/* the particles of the start between the two characters */
void __fastcall cObjElementFXCharacterConversion_Emit(cObjElementFXCharacterConversion *self);
byte __thiscall cObjElementFXCharacterConversor_Alive(cObjElementFXCharacterConversor *self, cDebris *d,
	cDebrisSys *sys);
void __thiscall cObjElementFXCharacterConversor_Attach(cObjElementFXCharacterConversor *self, cDebris *d);
void __thiscall cObjElementFXCharacterConversor_Reset(cObjElementFXCharacterConversor *self, int arg);
void __thiscall cObjElementFXCharacterConversor_SetCharacter(cObjElementFXCharacterConversor *self, cEntity *e);
/* the conversor and the nodes of its model */
void __thiscall cObjElementFXCharacterConversor_SetConversor(cObjElementFXCharacterConversor *self, cEntity *e);
void __thiscall cObjElementFXCharacterDeath_Attach(cObjElementFXCharacterDeath *self, cDebris *d);
void __thiscall cObjElementFXCharacterDeath_Reset(cObjElementFXCharacterDeath *self, int arg);
void __thiscall cObjElementFXCharacterDeath_SetEntity(cObjElementFXCharacterDeath *self, cEntity *e);
/* the light column and the fade of the character, glows over its vertices, sparks and smoke */
void __thiscall cObjElementFXCharacterTeleport_Update(cObjElementFXCharacterTeleport *self, cDebris *d, float dt);
byte __thiscall cObjElementFXCharacterTeleport_Alive(cObjElementFXCharacterTeleport *self, cDebris *d,
	cDebrisSys *sys);
void __thiscall cObjElementFXCharacterTeleport_Attach(cObjElementFXCharacterTeleport *self, cDebris *d);
void __thiscall cObjElementFXCharacterTeleport_Reset(cObjElementFXCharacterTeleport *self, int arg);
byte __thiscall cObjElementFXMaintenanceWork_Alive(cObjElementFXMaintenanceWork *self, cDebris *d, cDebrisSys *sys);
void __thiscall cObjElementFXMaintenanceWork_Attach(cObjElementFXMaintenanceWork *self, cDebris *d);
void __thiscall cObjElementFXMaintenanceWork_Reset(cObjElementFXMaintenanceWork *self, int arg);
void __thiscall cObjElementFXMaintenanceWork_SetMaintenance(cObjElementFXMaintenanceWork *self, cEntity *e);
void __thiscall cObjElementFXMaintenanceWork_SetShip(cObjElementFXMaintenanceWork *self, cEntity *e);
/* the laser vertex of the ship and the work sound, at the debris d */
void __thiscall cObjElementFXMaintenanceWork_Start(cObjElementFXMaintenanceWork *self, cDebris *d);
void __thiscall cObjElementFXMoneyTransfer_Update(cObjElementFXMoneyTransfer *self, cDebris *d, float dt);
void __thiscall cObjElementFXMoneyTransfer_Attach(cObjElementFXMoneyTransfer *self, cDebris *d);
void __thiscall cObjElementFXMoneyTransfer_Reset(cObjElementFXMoneyTransfer *self, int arg);
void __thiscall cObjElementFXMoneyTransfer_SetEntities(cObjElementFXMoneyTransfer *self, cEntity *from, cEntity *to);
void __thiscall cObjElementFXSputnik_Update(cObjElementFXSputnik *self, cDebris *d, float dt);
byte __thiscall cObjElementFXSputnik_Alive(cObjElementFXSputnik *self, cDebris *d, cDebrisSys *sys);
void __thiscall cObjElementFXSputnik_Attach(cObjElementFXSputnik *self, cDebris *d);
void __thiscall cObjElementFXSputnik_Reset(cObjElementFXSputnik *self, int arg);
/* the entity and the node "Bip Spootnick MASTER" of its model */
void __thiscall FXSputnik_Start(cObjElementFXSputnik *self, cEntity *e);
void __thiscall FXSputnik_SetA(cObjElementFXSputnik *self, float v);	/* the duration */
void __thiscall FXSputnik_SetB(cObjElementFXSputnik *self, float v);	/* minMove */
/* dead code: the int at 0x4 */
void __thiscall FXChar_SetI4(void *self, int v);

/* the free functions */
/* the effects of a character by name: CharacterDeath, CharacterSputnikHello, Bye, Interface and Junkyard,
 * FunctionarySpecialAction */
void FX_Character(cEntity *e, const char *name);
/* the native functions (the character name, a second name and an int from the parameter file) */
void FX_CharacterBlaBliBla(cAbstractFile *params);
void FX_CharacterDazed(cAbstractFile *params);
void FX_CharacterTeleport(cAbstractFile *params);

#endif
