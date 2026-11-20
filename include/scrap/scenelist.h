/*
 * The cutscenes of the game (src/game/Script/SceneList.c): each scene with its level, the mission that plays it, the
 * function of the level's scripts that loads its classes (Scene.Load, the actors, their actions and timed events: a
 * data section or a call table) and its actors. The debug menu, the test verb "scene" and the client of the second
 * multiplayer design (include/scrap/netscene.h: a -mp2 client loads the classes of the server's scene itself, tag
 * SCENE of tools/script/netclass/module.tsv) use it. Not in the original.
 */
#ifndef SCRAP_SCENELIST_H
#define SCRAP_SCENELIST_H

#include "scrap/shared.h"

typedef struct sceneInfo {
	const char *scene;				/* the class without "Scene" (Scene.Play's argument) */
	const char *level;
	const char *mission;			/* the mission that plays it ("" a scene of the level's own scripts) */
	const char *module, *load;		/* the function that loads its classes */
	byte atStart;					/* the mission plays it at its start (the debug menu starts the mission alone) */
	const char *actors;
} sceneInfo_t;

int SceneList_Count(void);
const sceneInfo_t *SceneList_At(int i);
/* by the scene's name (case ignored), NULL */
const sceneInfo_t *SceneList_Find(const char *scene);
/* the class Scene<scene> is there */
byte SceneList_Loaded(const char *scene);
/* a module of the current level's scripts imported (a mission's: its level pack's Scripts folder preloaded first, as
 * the level start preloads it) */
byte SceneList_Import(const char *module);
/* the classes of the scene loaded by its function when they are not there yet (a class loaded twice is a fatal
 * check of the actions); 0 when it could not */
byte SceneList_Load(const sceneInfo_t *s);
/* the scene loaded and played (Scene.Play) now; 0 for an unknown scene or a failed load or play */
byte SceneList_Play(const char *scene);

#endif
