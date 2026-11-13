/*
 * The Scene module in C (scripts/missions/scene.py, issue #2): the cut scenes. A scene is an action class
 * "Scene<name>" whose model holds the camera path and the dummies the actors link to; the level scripts build it
 * (Load, the actors, the timed events of the actors, texts, sounds and generic lines) and Play starts it: the
 * characters hide, the camera links to the scene, the input stops until EndOfScene (the last event of the scene, or
 * Escape through AbortScene). InitOutDoorScene and EndOutDoorScene prepare an outdoor map for a scene the mission
 * moves itself.
 *
 * The state stays in the module (Actors, LocalActors, SceneName, EndOfSceneCallback, AcActionSet, VoiceChannel,
 * HidenChars, usr, LastDopplerLevel, PrevInput, InvulnerableChars), the functions read and write it as their
 * globals, because the level scripts set EndOfSceneCallback and read the rest.
 *
 * The module is native C (docs/objlib-plan.md, stage 2): its functions work on C values, read the globals where the
 * script reads them and write them back at once, the lists of Actors and LocalActors changed in place.
 * The entities are handle values, the functions of the module are called through their refs as globals (PORT_OWN:
 * a level may replace one) and the callback as the value it holds. Scrap.Set, GetEntity, UsrEntity, CreateEntity,
 * ConsoleError, OpenPack, ClosePack, GetAlarm, the dummy search and the scheduler, the SAct calls, the voices,
 * SScorer.SetLabelText, SetMsgText, SetSpeechText and SetSpeechCallback and SInput.SetActionSet go through their seams
 * (OBJLIB 7: the cases record most of them). MakeChar, Menu, CharScorer, Speech and string.atoi and atof go through
 * refs. The lines the game execs (the generic lines of a scene, the sound lines of SetDefSounds and Load) go through
 * the exec reader on native values (ScriptExec.c).
 */
#include <limits.h>
#include <string.h>

#include "scrap/script.h"
#include "scrap/scriptmodes.h"
#include "game/Script/levels/Levels.h"

#define SC "Scene"

/* a global of the module as its functions read it */
static byte Sc_G(const char *name, nativeVal_t *out) {
	return Mode_G(SC, name, out);
}

/* Time = SAct.GetAct("TimeOfFrame" + `frame`); SAct.SetAct("AddEventAtTime", Time) */
static byte Sc_EventAt(const nativeVal_t *frame) {
	nativeVal_t r, pre, key = { 0 }, t = { 0 };
	Mode_Repr(frame, &r);
	Native_SetText(&pre, "TimeOfFrame", 11);
	byte ok = Native_Arith('+', &pre, &r, &key) && Mode_SActV("GetAct", &key, &t) && LvN_SetAct("AddEventAtTime", &t);
	Native_Free(&r);
	Native_Free(&pre);
	Native_Free(&key);
	Native_Free(&t);
	return ok;
}

/* SAct.SetAct(name, "text") */
static byte Sc_ActS(const char *name, const char *s) {
	return Mode_SActSet(0, name, s, 0);
}

/* SAct.SetAct(name, i), SAct.SetAct(name, d) */
static byte Sc_ActI(const char *name, long i) {
	return Mode_SActSet(0, name, NULL, i);
}

static byte Sc_ActF(const char *name, double d) {
	nativeVal_t v;
	Native_SetFloat(&v, d);
	return LvN_SetAct(name, &v);
}

/* the motion blur off for a scene */
static byte Sc_NoBlur(void) {
	return Mode_GVarSetI("MotionBlurFactorBegin", 0) && Mode_GVarSetN("MotionBlurFactorEnd", 0.0) &&
		Mode_GVarSetN("MotionBlurTime", 0.0) && Mode_GVarSetN("MotionBlurEndTime", 0.0) &&
		Mode_GVarSetN("MotionBlurOffVar", 0.0) && Mode_GVarSetI("R_SceneMotionBlurValue", 0);
}

/* SScorer.SetLabelText("", 0), SetMsgText("", 0), SetSpeechText("", 0, 255, 255, 255) */
static byte Sc_NoText(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *empty = Mode_S(&t, "");
	const nativeVal_t *zero = Mode_I(&t, 0), *full = Mode_I(&t, 255);
	return Mode_Done(&t, Mode_LabelTextV(empty, zero) && Mode_MsgTextV(empty, zero) &&
		Mode_SpeechText(empty, zero, full, full, full));
}

/* BaseAddActor(ActorName, ActorClass, pathfile): an actor of the scene (a tuple of its dummy, entity and class in
 * Actors[SceneName]) and its class */
static byte Sc_BaseAddActor(const nativeVal_t *name, const nativeVal_t *cls, const nativeVal_t *path) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *one = Mode_V(&t), *scene = Mode_V(&t), *actor = Mode_V(&t), *dummy = Mode_V(&t), *r = Mode_V(&t);
	Native_SetFloat(one, 1.0);
	if (!Mode_Import("MakeChar") || !Mode_SetModVar("MakeChar", "MaxSpeed", one) || !Mode_SetModVar("MakeChar", "MinSpeed", one) ||
		!Sc_G("SceneName", scene)) {
		return Mode_Done(&t, 0);
	}
	/* the list of the scene in Actors, held by reference and appended in place (append of three values makes
	 * a tuple in 1.5.2) */
	nativeVal_t *all = Mode_V(&t), *the = Mode_V(&t), *append = Mode_V(&t);
	byte ok = Mode_RefG(SC, "Actors", all) && Mode_RefSub(all, scene, the) && Mode_Read(SC, "SceneName", scene) &&
		Native_Arith('+', scene, name, actor) && Mode_RefAttr(the, "append", append) &&
		Mode_Cat(&t, dummy, 2, Mode_S(&t, "Actor_"), name);
	if (ok) {
		const nativeVal_t args[3] = { *dummy, *actor, *cls };
		ok = Mode_RefApply(append, args, 3, NULL);
	}
	return Mode_Done(&t, ok && Mode_SActV("CreateClass", actor, r) && LvN_SetCls("ModelFilePath", path));
}

/* AddCharActor(ActorName, ActorClass): an actor with the model of a character, its relax and morph actions */
static byte Sc_AddCharActor(const nativeVal_t *name, const nativeVal_t *cls) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *path = Mode_V(&t);
	/* the global BaseAddActor is looked up after the path is made (the script looked it up first) */
	return Mode_Done(&t, Mode_Import("MakeChar") &&
		Mode_Cat(&t, path, 5, Mode_S(&t, "Models/Chars/"), cls, Mode_S(&t, "/"), cls, Mode_S(&t, ".M3D")) &&
		PORT_OWN(Scene, BaseAddActor, NULL, "vvv", name, cls, path) &&
		PORT_CALL(MakeChar, CreateRelaxAction, NULL, "vissi", cls, 0L, "Relax", "Relax", 1L) &&
		PORT_CALL(MakeChar, AddDefMorphActions, NULL, "v", cls) && Mode_SActSet(1, "DefAction", "Relax", 0));
}

/* LocalActors[SceneName].method(arg) on the list itself; in: `arg in LocalActors[SceneName]` instead */
static int Sc_LocalList(const nativeVal_t *scene, const char *method, const nativeVal_t *arg) {
	nativeVal_t all = { 0 }, l = { 0 };
	int r = -1;
	if (!Mode_RefG(SC, "LocalActors", &all) || !Mode_RefSub(&all, scene, &l)) {
		Native_Free(&all);
		return -1;
	}
	if (!method) {
		nativeVal_t v;
		Mode_RefPlain(&l, &v);
		r = Mode_In(&v, arg);
		Native_Free(&v);
	} else {
		/* the method as the plain attribute of the list (no error is added to an object that sets none) */
		nativeVal_t m;
		r = Mode_RefAttr(&l, method, &m) && Mode_RefApply(&m, arg, 1, NULL) ? 1 : -1;
		Native_Free(&m);
	}
	Native_Free(&l);
	Native_Free(&all);
	return r;
}

/* AddLocalActor(ActorName, ActorClass): an actor with a model of the scene's folder; its class goes with the scene
 * (LocalActors) */
static byte Sc_AddLocalActor(const nativeVal_t *name, const nativeVal_t *cls) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *ipath = Mode_V(&t), *scene = Mode_V(&t), *empty = Mode_V(&t), *path = Mode_V(&t);
	/* the global BaseAddActor is looked up after Init.Path, SceneName and the path (the script looked it up first) */
	if (!Mode_Import("Init") || !LvN_ModGet("Init", "Path", ipath) || !Sc_G("SceneName", scene)) {
		return Mode_Done(&t, 0);
	}
	if (!Mode_Cat(&t, path, 6, ipath, Mode_S(&t, "/Map/"), scene, Mode_S(&t, "/"), cls, Mode_S(&t, ".M3D")) ||
		!PORT_OWN(Scene, BaseAddActor, NULL, "vvv", name, cls, path) || !Mode_SActSet(1, "Discardable", NULL, 1) ||
		!Mode_Read(SC, "SceneName", scene)) {
		return Mode_Done(&t, 0);
	}
	int has = Mode_HasKey(SC, "LocalActors", 1, scene);
	Native_SetEmpty(empty, NV_LIST);
	if (has < 0 || (!has && (!Mode_Read(SC, "SceneName", scene) || !Mode_DictPut(SC, "LocalActors", NULL, scene, empty)))) {
		return Mode_Done(&t, 0);
	}
	int in = Mode_Read(SC, "SceneName", scene) ? Sc_LocalList(scene, NULL, cls) : -1;
	return Mode_Done(&t, in >= 0 && (in || (Mode_Read(SC, "SceneName", scene) && Sc_LocalList(scene, "append", cls) > 0)));
}

/* EventName "'" + Actor + "','" + Action + "','" + DefAction + "'" and EventFunc func at the frame (ActorAction,
 * ObjAction) */
static byte Sc_Action(const nativeVal_t *frame, const nativeVal_t *actor, const nativeVal_t *action, const nativeVal_t *def,
	const char *func) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *name = Mode_V(&t);
	return Mode_Done(&t, Sc_EventAt(frame) && Mode_Cat(&t, name, 7, Mode_S(&t, "'"), actor, Mode_S(&t, "','"), action,
		Mode_S(&t, "','"), def, Mode_S(&t, "'")) && LvN_SetAct("EventName", name) && Sc_ActS("EventFunc", func));
}

/* Actor, Action, DefAction = eval(EventName); the entity of the scene's actor (actor 1) or the object forces the
 * action */
static byte Sc_ActEvent(const nativeVal_t *ename, const nativeVal_t *event, byte actor) {
	modeVals_t t = { .n = 0 };
	/* v takes the three values of the unpack: the two slots after it stay free for v[1] and v[2] */
	nativeVal_t *val = Mode_V(&t), *v = Mode_V(&t);
	Mode_V(&t);
	Mode_V(&t);
	nativeVal_t *tail = Mode_V(&t), *name = Mode_V(&t), *e = Mode_V(&t);
	if (!Native_EvalText(SC, event, val) || !Mode_Unpack(val, 3, v)) {
		return Mode_Done(&t, 0);
	}
	if (actor) {
		if (!Native_Slice(ename, 9, INT_MAX, tail) || !Native_Arith('+', tail, &v[0], name)) {
			return Mode_Done(&t, 0);
		}
	} else {
		Native_Copy(name, &v[0]);
	}
	return Mode_Done(&t, Mode_EntitySeam(name, e) && Mode_VSet(e, "ForceAction", &v[1]) && Mode_VSet(e, "ForceDefLoop", &v[2]));
}

/* ActorBreakChannel(frame, Actor, Channel): at the frame the actor stops a secondary channel */
static byte Sc_ActorBreakChannel(const nativeVal_t *frame, const nativeVal_t *actor, const nativeVal_t *channel) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *r = Mode_V(&t), *name = Mode_V(&t);
	Mode_Repr(channel, r);
	return Mode_Done(&t, Sc_EventAt(frame) && Mode_Cat(&t, name, 4, Mode_S(&t, "'"), actor, Mode_S(&t, "',"), r) &&
		LvN_SetAct("EventName", name) && Sc_ActS("EventFunc", "Scene.ActorActBreakChanne"));
}

/* ActorActBreakChanne(EntityName, EventName, Time) */
static byte Sc_ActorActBreakChanne(const nativeVal_t *ename, const nativeVal_t *event) {
	modeVals_t t = { .n = 0 };
	/* v takes the two values of the unpack: the slot after it stays free for v[1] */
	nativeVal_t *val = Mode_V(&t), *v = Mode_V(&t);
	Mode_V(&t);
	nativeVal_t *tail = Mode_V(&t), *name = Mode_V(&t), *e = Mode_V(&t);
	return Mode_Done(&t, Native_EvalText(SC, event, val) && Mode_Unpack(val, 2, v) && Native_Slice(ename, 9, INT_MAX, tail) &&
		Native_Arith('+', tail, &v[0], name) && Mode_EntitySeam(name, e) && Mode_VSet(e, "StopSecondary", &v[1]));
}

/* exec(ActorType + "Sound.SetSounds('" + SceneName + ActorName + "')") of SetDefSounds: the call made directly when
 * the line is that call (the sound module imported into the variables before) */
static byte Sc_SetSounds(execScope_t *x, const nativeVal_t *type, const nativeVal_t *scene, const nativeVal_t *name) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *mod = Mode_V(&t), *arg = Mode_V(&t), *code = Mode_V(&t);
	if (type->type == NV_STR && scene->type == NV_STR && name->type == NV_STR) {
		if (!Exec_Cat(mod, 2, type, Mode_S(&t, "Sound")) || !Exec_Cat(arg, 2, scene, name)) {
			return Mode_Done(&t, 0);
		}
		if (Script_IsName(Native_Text(mod), mod->n) && Script_IsQuotableText(Native_Text(arg), arg->n) &&
			Exec_Bound(x, Native_Text(mod))) {
			return Mode_Done(&t, Exec_Call(x, Native_Text(mod), "SetSounds", arg, 1));
		}
	}
	return Mode_Done(&t, Exec_Cat(code, 5, type, Mode_S(&t, "Sound.SetSounds('"), scene, name, Mode_S(&t, "')")) &&
		Exec_Text(x, code));
}

/* the try of SetDefSounds and Load: exec("import MapSnd;MapSnd." + a + b + "()") (b NULL: a alone), the call made
 * directly when the function name is a text */
static byte Sc_MapSnd(execScope_t *x, const nativeVal_t *a, const nativeVal_t *b) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *func = Mode_V(&t), *code = Mode_V(&t), *head = Mode_S(&t, "import MapSnd;MapSnd."), *tail = Mode_S(&t, "()");
	if (a->type == NV_STR && (!b || b->type == NV_STR)) {
		return Mode_Done(&t, (!b || Exec_Cat(func, 2, a, b)) && Exec_ImportCall(x, "MapSnd", b ? func : a));
	}
	return Mode_Done(&t, (b ? Exec_Cat(code, 4, head, a, b, tail) : Exec_Cat(code, 3, head, a, tail)) &&
		Exec_Text(x, code));
}

/* in the bare except of that try: print "Sound warning: MapSnd." + a + b + "() not found\n" (b NULL: a alone) */
static byte Sc_MapSndWarning(const nativeVal_t *a, const nativeVal_t *b) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *text = Mode_V(&t), *head = Mode_S(&t, "Sound warning: MapSnd."), *tail = Mode_S(&t, "() not found\n");
	return Mode_Done(&t, (b ? Exec_Cat(text, 4, head, a, b, tail) : Exec_Cat(text, 3, head, a, tail)) &&
		Mode_PrintV(text));
}

/* SetDefSounds(ActorName, ActorType): the sounds of the character's sound script on the actor's class, then the
 * MapSnd function of the actor when the level has one. The function is its exec lines (an import, a call of the
 * sound module, the MapSnd call in a try), run by the exec reader with the variables of the function */
static byte Sc_SetDefSounds(const nativeVal_t *name, const nativeVal_t *type) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *mod = Mode_V(&t), *scene = Mode_V(&t);
	/* the function has exec, so its variables are what the exec'd lines share */
	execScope_t x = { .module = SC };
	Exec_Var(&x, "ActorName", name);
	Exec_Var(&x, "ActorType", type);
	if (type->type == NV_STR) {
		if (!Exec_Cat(mod, 2, type, Mode_S(&t, "Sound")) || !Exec_ImportText(&x, mod)) {
			return Mode_Done(&t, 0);
		}
	} else if (!Exec_Cat(mod, 3, Mode_S(&t, "import "), type, Mode_S(&t, "Sound")) || !Exec_Text(&x, mod)) {
		return Mode_Done(&t, 0);
	}
	if (!Sc_G("SceneName", scene) || !Sc_SetSounds(&x, type, scene, name)) {
		return Mode_Done(&t, 0);
	}
	if (Mode_Read(SC, "SceneName", scene) && Sc_MapSnd(&x, scene, name)) {
		return Mode_Done(&t, 1);
	}
	Native_ErrClear();
	return Mode_Done(&t, Mode_Read(SC, "SceneName", scene) && Sc_MapSndWarning(scene, name));
}

/* CreateLocalAction(CharName, turnspeed, Name="Relax", Anm="Relax", Loop=0): an action of an actor with an
 * animation of the scene's folder */
static byte Sc_CreateLocalAction(const nativeVal_t *ch, const nativeVal_t *speed, const nativeVal_t *name,
	const nativeVal_t *anm, const nativeVal_t *loop) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *r = Mode_V(&t), *ipath = Mode_V(&t), *scene = Mode_V(&t), *path = Mode_V(&t);
	if (!Mode_Import("Init") || !Mode_SActV("CreateAction", name, r) || !LvN_ModGet("Init", "Path", ipath) ||
		!Sc_G("SceneName", scene) || !Mode_Cat(&t, path, 10, ipath, Mode_S(&t, "/Map/"), scene, Mode_S(&t, "/"), scene,
		Mode_S(&t, "Anm/"), ch, Mode_S(&t, "_"), anm, Mode_S(&t, ".M3D"))) {
		return Mode_Done(&t, 0);
	}
	return Mode_Done(&t, LvN_SetAct("AnmFilePath", path) && LvN_SetAct("isLoop", loop) && LvN_SetAct("RotSpeed", speed) &&
		Sc_ActI("AngRotLimit", 90) && Sc_ActI("UseDisp", 0) && Sc_ActI("NodeModifier", -1) && Sc_ActF("AngleModifierSpeed", 5.0));
}

/* ExecGeneric(EntityName, EventName, Time): the event is a line of Python (the forms of the scene lines run in C,
 * Exec_Text, with the arguments as the variables of the line) */
static byte Sc_ExecGeneric(const nativeVal_t *ename, const nativeVal_t *event, const nativeVal_t *time) {
	execScope_t x = { .module = SC };
	Exec_Var(&x, "EntityName", ename);
	Exec_Var(&x, "EventName", event);
	Exec_Var(&x, "Time", time);
	return Exec_Text(&x, event);
}

/* AddGeneric(Frame, Event, Discardable=1): a line of Python at the frame */
static byte Sc_AddGeneric(const nativeVal_t *frame, const nativeVal_t *event, const nativeVal_t *disc) {
	return Sc_EventAt(frame) && Sc_ActS("EventFunc", "Scene.ExecGeneric") && LvN_SetAct("EventName", event) &&
		LvN_SetAct("EventDiscardable", disc);
}

/* AddStream(frame, filename, vol=1.0): a stream of the stream folder at the frame */
static byte Sc_AddStream(const nativeVal_t *frame, const nativeVal_t *file, const nativeVal_t *vol) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *r = Mode_V(&t), *name = Mode_V(&t);
	Mode_Repr(vol, r);
	return Mode_Done(&t, Sc_EventAt(frame) && Mode_Cat(&t, name, 3, file, Mode_S(&t, "\001"), r) &&
		LvN_SetAct("EventName", name) && Sc_ActS("EventFunc", "Scene.PlayStream"));
}

/* PlayStream(EntityName, EventName, Time): opens the stream on voice channel 2 and plays it half a second later at
 * its volume */
static byte Sc_PlayStream(const nativeVal_t *event) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *cads = Mode_V(&t), *path = Mode_V(&t), *c0 = Mode_V(&t), *file = Mode_V(&t), *time = Mode_V(&t);
	nativeVal_t *c1 = Mode_V(&t), *args = Mode_V(&t);
	Mode_V(&t);
	if (!Mode_Import("string") || !Native_Split(event, "\001", cads) || !Native_GVarGet("StreamPath", path) ||
		!LvN_Item(cads, 0, c0) || !Mode_Cat(&t, file, 3, path, c0, Mode_S(&t, ".ogg")) ||
		!Mode_OpenVoice(file, Mode_I(&t, 2), Mode_I(&t, 0))) {
		return Mode_Done(&t, 0);
	}
	Native_SetFloat(time, Mode_Time() + 0.5);
	/* the function as the global holds it (OBJLIB 7: by reference; the scheduler takes the entry of the port's own) */
	nativeVal_t *fn = Mode_V(&t);
	Native_SetInt(&args[0], 2);
	return Mode_Done(&t, Mode_RefG(SC, "PlayVoiceVol", fn) && LvN_Item(cads, 1, c1) && PORT_CALL(string, atof, &args[1], "v", c1) &&
		Mode_ScheduleV(time, fn, args, 2, NULL));
}

/* PlayText(EntityName, EventName, Time): the subtitle of the event ("time\001key") */
static byte Sc_PlayText(const nativeVal_t *event) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *cads = Mode_V(&t), *c0 = Mode_V(&t), *key = Mode_V(&t), *time = Mode_V(&t);
	/* the global SetText is looked up after its arguments (the script looked it up first) */
	return Mode_Done(&t, Mode_Import("string") && Native_Split(event, "\001", cads) && LvN_Item(cads, 1, key) &&
		LvN_Item(cads, 0, c0) && PORT_CALL(string, atoi, time, "v", c0) &&
		PORT_OWN(Scene, SetText, NULL, "vv", key, time));
}

/* SetText(Cad, Time): the voice of the language key on the next voice channel and its text (Time 0: the time the
 * text takes to read) */
static byte Sc_SetText(const nativeVal_t *cad, const nativeVal_t *timeArg) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *time = Mode_V(&t), *lang = Mode_V(&t), *path = Mode_V(&t), *file = Mode_V(&t), *vc = Mode_V(&t);
	if (!Mode_Import("Speech")) {
		return Mode_Done(&t, 0);
	}
	Native_Copy(time, timeArg);
	if (Mode_CmpI(time, "==", 0) > 0) {
		Native_Free(time);
		if (!Mode_LangV(cad, lang) || !PORT_CALL(Speech, CalculateSpeechTime, time, "v", lang)) {
			return Mode_Done(&t, 0);
		}
	}
	if (!Native_GVarGet("VoicePath", path) || !Native_Arith('+', path, cad, file) || !Sc_G("VoiceChannel", vc) ||
		!Mode_OpenVoice(file, vc, NULL) || !Mode_Read(SC, "VoiceChannel", vc) ||
		!PORT_CALL(Speech, PlayVoiceScheduled, NULL, "vdi", vc, 0.25, 0L) || !Mode_Read(SC, "VoiceChannel", vc) ||
		!LvN_PutI(SC, "VoiceChannel", !Native_True(vc)) || !Mode_SpeechCallback("Scene.StopSpeech")) {
		return Mode_Done(&t, 0);
	}
	Native_Free(lang);
	return Mode_Done(&t, Mode_LangV(cad, lang) && Mode_SpeechText(lang, time, Mode_I(&t, 220), Mode_I(&t, 235), Mode_I(&t, 255)));
}

/* StopSpeech(id=0): the voice that plays */
static byte Sc_StopSpeech(void) {
	nativeVal_t vc = { 0 }, ch;
	if (!Sc_G("VoiceChannel", &vc)) {
		return 0;
	}
	Native_SetInt(&ch, !Native_True(&vc));
	Native_Free(&vc);
	return Mode_StopVoice(&ch);
}

/* AddText(frame, lngtext, time=86400): a subtitle at the frame */
static byte Sc_AddText(const nativeVal_t *frame, const nativeVal_t *text, const nativeVal_t *time) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *r = Mode_V(&t), *name = Mode_V(&t);
	Mode_Repr(time, r);
	return Mode_Done(&t, Sc_EventAt(frame) && Mode_Cat(&t, name, 3, r, Mode_S(&t, "\001"), text) &&
		LvN_SetAct("EventName", name) && Sc_ActS("EventFunc", "Scene.PlayText"));
}

/* AbortScene(id): Escape ends the scene */
static byte Sc_AbortScene(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *scene = Mode_V(&t);
	/* the global EndOfScene is looked up after SceneName (the script looked it up first) */
	return Mode_Done(&t, Sc_G("SceneName", scene) && PORT_OWN(Scene, EndOfScene, NULL, "vsi", scene, "EndOfScene", 0L));
}

/* Scrap.GetEntity(name).Del() */
static byte Sc_Del(const nativeVal_t *name) {
	nativeVal_t e = { 0 };
	byte ok = Mode_EntitySeam(name, &e) && Mode_VDel(&e);
	Native_Free(&e);
	return ok;
}

/* EndOfScene(EntityName, EventName, Time): the scene and its actors go, the characters come back, the camera, the
 * HUD and the input of the player return; EndOfSceneCallback(usr) runs last */
static byte Sc_EndOfScene(const nativeVal_t *ename) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *none = Mode_V(&t), *cam = Mode_V(&t), *sname = Mode_V(&t), *actors = Mode_V(&t), *list = Mode_V(&t);
	nativeVal_t *actr = Mode_V(&t), *name = Mode_V(&t), *v = Mode_V(&t), *time = Mode_V(&t), *cb = Mode_V(&t);
	Native_SetNone(none);
	if (!Mode_Put(SC, "SceneName", NULL, none) || !Mode_StopAllSounds() || !Mode_GVarSetI("ForceNoActionMusic", 0) ||
		!Mode_StopVoice(Mode_I(&t, 0)) || !Mode_StopVoice(Mode_I(&t, 1)) || !Mode_StopVoice(Mode_I(&t, 2)) || !Mode_Cam0(cam) ||
		!LvN_SetS(cam, "EntityLink", "") || !PORT_OWN(Scene, UnhideChars, NULL, "") || !Sc_Del(ename) ||
		!Native_Slice(ename, 9, INT_MAX, sname) || !Sc_G("Actors", actors) || !Mode_Sub(actors, sname, list)) {
		return Mode_Done(&t, 0);
	}
	int n = Mode_Loop(list);
	for (int i = 0; i < n; i++) {
		Native_Free(actr);
		Native_Free(name);
		if (!Native_SeqItem(list, i, actr) || !LvN_Item(actr, 1, name) || !Sc_Del(name)) {
			return Mode_Done(&t, 0);
		}
	}
	if (n < 0 || !Mode_Import("CharScorer") || !PORT_CALL(CharScorer, Create, NULL, "i", 0L) || !Sc_NoText() ||
		!Sc_G("AcActionSet", v) || !Mode_ActionSetV(v) || !Mode_GVarSetS("EscapeEvent", "Menu.Init") ||
		!Mode_Read(SC, "LastDopplerLevel", v) || !Mode_GVarSet("SoundDopplerLevel", v)) {
		return Mode_Done(&t, 0);
	}
	Native_SetFloat(time, Mode_Time() + 0.2);
	nativeVal_t *fn = Mode_V(&t);
	byte ok = Mode_RefG(SC, "DelClass", fn) && Mode_ScheduleV(time, fn, sname, 1, NULL);
	if (!ok || !Mode_GVarSetI("DisableSkipSlot", 0) || !Mode_GVarSetI("PauseInactivityTime", 0) ||
		!Mode_GVarSetI("CameraScene", 0) || !Sc_G("EndOfSceneCallback", cb)) {
		return Mode_Done(&t, 0);
	}
	if (cb->type == NV_NONE) {
		return Mode_Done(&t, 1);
	}
	return Mode_Done(&t, Mode_Put(SC, "EndOfSceneCallback", NULL, none) && Mode_Read(SC, "usr", v) && Mode_CallArgs(cb, NULL, v, 1));
}

/* DelClass(SName): the class of the scene and those of its local actors */
static byte Sc_DelClass(const nativeVal_t *sname) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *name = Mode_V(&t), *r = Mode_V(&t), *local = Mode_V(&t), *list = Mode_V(&t), *anam = Mode_V(&t);
	if (!Mode_Cat(&t, name, 2, Mode_S(&t, "Scene"), sname) || !Mode_SActV("DelClass", name, r)) {
		return Mode_Done(&t, 0);
	}
	int has = Mode_HasKey(SC, "LocalActors", 1, sname);
	if (has <= 0) {
		return Mode_Done(&t, has == 0);
	}
	int n = Sc_G("LocalActors", local) && Mode_Sub(local, sname, list) ? Mode_Loop(list) : -1;
	for (int i = 0; i < n; i++) {
		Native_Free(anam);
		Native_Free(name);
		Native_Free(r);
		if (!Native_SeqItem(list, i, anam) || !Native_Arith('+', sname, anam, name) || !Mode_SActV("DelClass", name, r)) {
			return Mode_Done(&t, 0);
		}
	}
	return Mode_Done(&t, n >= 0 && Mode_DictDel(SC, "LocalActors", NULL, sname));
}

/* AddSound(Frame, SoundName, Vol, Pitch=0, Pan=0): a 2D sound at the frame (the engine's !AnmSound2DEvent) */
static byte Sc_AddSound(const nativeVal_t *frame, const nativeVal_t *sound, const nativeVal_t *vol, const nativeVal_t *pitch,
	const nativeVal_t *pan) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *rv = Mode_V(&t), *rp = Mode_V(&t), *rn = Mode_V(&t), *name = Mode_V(&t);
	if (!Sc_EventAt(frame)) {
		return Mode_Done(&t, 0);
	}
	Mode_Repr(vol, rv);
	Mode_Repr(pitch, rp);
	Mode_Repr(pan, rn);
	nativeVal_t *colon = Mode_S(&t, ":");
	return Mode_Done(&t, Mode_Cat(&t, name, 7, sound, colon, rv, colon, rp, colon, rn) && LvN_SetAct("EventName", name) &&
		Sc_ActS("EventFunc", "!AnmSound2DEvent"));
}

/* the try of Load: exec("import MapSnd;MapSnd." + SceneClass + "()") with the warning of a scene without sounds
 * (the exec reader with the variables of Load) */
static byte Sc_LoadSounds(const nativeVal_t *cls, const nativeVal_t *path) {
	if (!Mode_Import("Init")) {
		return 0;
	}
	/* the function has exec, so its variables (the import of Init too) are what the exec'd line shares */
	execScope_t x = { .module = SC };
	Exec_Var(&x, "SceneClass", cls);
	Exec_Var(&x, "Init", NULL);
	Exec_Var(&x, "path", path);
	if (Sc_MapSnd(&x, cls, NULL)) {
		return 1;
	}
	Native_ErrClear();
	return Sc_MapSndWarning(cls, NULL);
}

/* Load(SceneClass): the class of the scene from the level's Map/<scene> folder, its Play action ending in
 * EndOfScene, the MapSnd sounds of the scene; a new list of actors */
static byte Sc_Load(const nativeVal_t *cls) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *ipath = Mode_V(&t), *path = Mode_V(&t), *sname = Mode_V(&t), *r = Mode_V(&t), *file = Mode_V(&t);
	nativeVal_t *total = Mode_V(&t), *end = Mode_V(&t), *tenth = Mode_V(&t), *pack = Mode_V(&t), *scene = Mode_V(&t);
	nativeVal_t *empty = Mode_V(&t);
	if (!Mode_Import("Init") || !LvN_ModGet("Init", "Path", ipath) ||
		!Mode_Cat(&t, path, 4, ipath, Mode_S(&t, "/Map/"), cls, Mode_S(&t, "/")) ||
		!Mode_Cat(&t, sname, 2, Mode_S(&t, "Scene"), cls) || !Mode_SActV("GetClass", sname, r)) {
		return Mode_Done(&t, 0);
	}
	if (Native_True(r)) {
		return Mode_Done(&t, 1);
	}
	Native_Free(sname);
	Native_Free(r);
	if (!Mode_Cat(&t, sname, 2, Mode_S(&t, "Scene"), cls) || !Mode_SActV("CreateClass", sname, r) ||
		!Mode_SActSet(1, "Discardable", NULL, 1) || !Mode_Cat(&t, file, 3, path, cls, Mode_S(&t, ".m3d")) ||
		!LvN_SetCls("ModelFilePath", file) || !Mode_SAct("CreateAction", "Play")) {
		return Mode_Done(&t, 0);
	}
	Native_Free(file);
	Native_SetFloat(tenth, 0.1);
	if (!Mode_Cat(&t, file, 3, path, cls, Mode_S(&t, "Anm/Play.m3d")) || !LvN_SetAct("AnmFilePath", file) ||
		!Sc_ActI("InterpTime", 0) || !Sc_ActI("isLoop", 0) || !Mode_SActV("GetAct", Mode_S(&t, "AnmTotalTime"), total) ||
		!Mode_Arith('-', total, tenth, end) || !LvN_SetAct("AddEventAtTime", end) || !Sc_ActS("EventName", "EndOfScene") ||
		!Sc_ActS("EventFunc", "Scene.EndOfScene") || !LvN_ModGet("Init", "Path", ipath) ||
		!Mode_Cat(&t, pack, 2, ipath, Mode_S(&t, "/Sounds")) || !Mode_Pack(pack) || !Sc_LoadSounds(cls, path) ||
		!Mode_Pack(NULL)) {
		return Mode_Done(&t, 0);
	}
	Native_Free(sname);
	Native_Free(r);
	Native_SetEmpty(empty, NV_LIST);
	return Mode_Done(&t, Mode_Cat(&t, sname, 2, Mode_S(&t, "Scene"), cls) && Mode_SActV("GetClass", sname, r) &&
		Mode_SActSet(1, "DefAction", "Play", 0) && Mode_Put(SC, "SceneName", NULL, cls) && Sc_G("SceneName", scene) &&
		Mode_DictPut(SC, "Actors", NULL, scene, empty));
}

/* UnhideChars(): the entities HideThis hid are active again */
static byte Sc_UnhideChars(void) {
	nativeVal_t list = { 0 }, ch = { 0 }, e = { 0 };
	int n = Sc_G("HidenChars", &list) ? Mode_Loop(&list) : -1;
	byte ok = n >= 0;
	for (int i = 0; ok && i < n; i++) {
		ok = Native_SeqItem(&list, i, &ch) && Mode_EntitySeam(&ch, &e) &&
			(!Native_True(&e) || (LvN_SetI(&e, "Active", 1) && LvN_SetI(&e, "TimeSpeed", 1)));
		Native_Free(&ch);
		Native_Free(&e);
	}
	Native_Free(&list);
	return ok;
}

/* HideThis(e): an active entity goes inactive and frozen, its name into HidenChars */
static byte Sc_HideThis(const nativeVal_t *e) {
	nativeVal_t active = { 0 }, name = { 0 };
	byte ok = !Native_True(e) || (Mode_VObj(e, "Active", &active) && (!Native_True(&active) ||
		(Mode_VObj(e, "Name", &name) && Mode_Append(SC, "HidenChars", &name) && LvN_SetI(e, "Active", 0) &&
		LvN_SetI(e, "TimeSpeed", 0))));
	Native_Free(&active);
	Native_Free(&name);
	return ok;
}

/* the entities of the slots from the first one: HideThis (the global of the module) for those of the type */
static byte Sc_HideType(const char *type) {
	nativeVal_t e = { 0 }, next = { 0 };
	byte ok = Mode_First(&e);
	while (ok && Native_True(&e)) {
		int is = Mode_VIsType(&e, type);
		ok = is >= 0 && (!is || PORT_OWN(Scene, HideThis, NULL, "v", &e)) && Mode_VObj(&e, "NextInSlot", &next);
		Native_Free(&e);
		ok = ok && Mode_EntitySeam(&next, &e);
		Native_Free(&next);
	}
	Native_Free(&e);
	return ok;
}

/* HideChars(): the walking characters hide (the player does not select anything during the scene) */
static byte Sc_HideChars(void) {
	nativeVal_t empty, e = { 0 };
	Native_SetEmpty(&empty, NV_LIST);
	int walk = Mode_Put(SC, "HidenChars", NULL, &empty) && Mode_UsrHandle(&e) ? Native_True(&e) : -1;
	if (walk > 0) {
		walk = Mode_VIsType(&e, "WalkChar");
	}
	byte ok = walk >= 0 && (!walk || LvN_SetS(&e, "SelectedEntity", "")) && Sc_HideType("WalkChar");
	Native_Free(&empty);
	Native_Free(&e);
	return ok;
}

/* the entity of an actor of the list at the scene entity l: linked to its dummy, the morph eyes of its class */
static byte Sc_SetActor(const nativeVal_t *actr, const nativeVal_t *l) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *name = Mode_V(&t), *a = Mode_V(&t), *lname = Mode_V(&t), *dummy = Mode_V(&t), *link = Mode_V(&t);
	nativeVal_t *zero = Mode_V(&t), *cls = Mode_V(&t), *eyes = Mode_V(&t);
	Mode_Ints3(zero, 0, 0, 0);
	if (!LvN_Item(actr, 1, name) || !Mode_EntitySeam(name, a) || !Mode_VSet(a, "ActCtrl", name) || !Mode_VObj(l, "Name", lname) ||
		!LvN_Item(actr, 0, dummy) || !Mode_Cat(&t, link, 3, lname, Mode_S(&t, ","), dummy) || !Mode_VSet(a, "LinkModel", link) ||
		!Mode_VSet(a, "OffModPos", zero) || !LvN_SetI(a, "DeltaOffMul", 1) || !LvN_SetI(a, "OffMul", 1) ||
		!LvN_SetI(a, "UseLinkAng", 1) || !LvN_Item(actr, 2, cls) || !LvN_ModGet("MakeChar", "MorphEyesList", eyes)) {
		return Mode_Done(&t, 0);
	}
	int morph = Mode_In(eyes, cls);
	return Mode_Done(&t, morph >= 0 && (!morph || LvN_SetS(a, "Action", "MorphEyes")));
}

/* Play(SceneClass): loads the scene, creates its actors and the scene entity at the dummy DM_Scene_<name>, links
 * the camera, hides the characters (and outdoors the ships), the cinema HUD, no input, no doppler, no music */
static byte Sc_Play(const nativeVal_t *cls) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *actors = Mode_V(&t), *list = Mode_V(&t), *actr = Mode_V(&t), *name = Mode_V(&t), *e = Mode_V(&t);
	nativeVal_t *dm = Mode_V(&t), *dummy = Mode_V(&t), *msg = Mode_V(&t), *pos = Mode_V(&t), *x = Mode_V(&t);
	nativeVal_t *y = Mode_V(&t), *z = Mode_V(&t), *l = Mode_V(&t), *ang = Mode_V(&t), *a = Mode_V(&t), *cam = Mode_V(&t);
	nativeVal_t *lname = Mode_V(&t), *outdoor = Mode_V(&t), *usr = Mode_V(&t), *v = Mode_V(&t), *time = Mode_V(&t);
	nativeVal_t *args = Mode_V(&t);
	Mode_V(&t);
	const nativeVal_t *zero = Mode_I(&t, 0);
	if (!Mode_Import("MakeChar") || !Mode_Import("Init") || !Sc_NoBlur() || !Mode_GVarSetI("DrawNextFrame", 0) ||
		!PORT_OWN(Scene, Load, NULL, "v", cls) || !Sc_G("Actors", actors) || !Mode_Sub(actors, cls, list)) {
		return Mode_Done(&t, 0);
	}
	int n = Mode_Loop(list);
	for (int i = 0; i < n; i++) {
		Native_Free(actr);
		Native_Free(name);
		Native_Free(e);
		if (!Native_SeqItem(list, i, actr) || !LvN_Item(actr, 1, name) || !LvN_Create(name, zero, zero, zero, "WithLife", e)) {
			return Mode_Done(&t, 0);
		}
	}
	if (n < 0 || !Mode_Cat(&t, dm, 2, Mode_S(&t, "DM_Scene_"), cls) || !Mode_DummyStartSeam(Native_Text(dm), 1) ||
		!Mode_DummyNextSeam(dummy)) {
		return Mode_Done(&t, 0);
	}
	if (dummy->type == NV_NONE) {
		return Mode_Done(&t, Mode_Cat(&t, msg, 3, Mode_S(&t, "TERROR : DM_Scene_"), cls, Mode_S(&t, " Dummy never exists!\n")) &&
			Mode_ConsoleError(msg));
	}
	Native_SetEmpty(ang, NV_TUPLE);
	if (!LvN_Item(dummy, 1, pos) || !LvN_Item(dummy, 0, name) || !LvN_Item(pos, 0, x) || !LvN_Item(pos, 1, y) ||
		!LvN_Item(pos, 2, z) || !LvN_Create(name, x, y, z, "WithLife", l) || !LvN_Item(dummy, 2, a) ||
		!LvN_Item(a, 0, Native_Append(ang)) || !LvN_Item(a, 1, Native_Append(ang))) {
		return Mode_Done(&t, 0);
	}
	Native_SetInt(Native_Append(ang), 0);
	Native_Free(name);
	if (!Mode_VSet(l, "Ang", ang) || !Mode_Cat(&t, name, 2, Mode_S(&t, "Scene"), cls) || !Mode_VSet(l, "ActCtrl", name) ||
		!Mode_Cam0(cam) || !Mode_VObj(l, "Name", lname) || !Mode_VSet(cam, "EntityLink", lname) ||
		!PORT_OWN(Scene, HideChars, NULL, "") ||
		!LvN_ModGet("Init", "Outdoor", outdoor) || (Native_True(outdoor) && !PORT_OWN(Scene, HideVehiclesToo, NULL, "")) ||
		!Mode_UsrHandle(usr) || !Mode_Put(SC, "usr", NULL, usr) || !Mode_GVarSetI("PauseInactivityTime", 1) ||
		!Mode_GVarSetI("CameraScene", 1) || !Mode_Import("Menu") || !PORT_CALL(Menu, MovieScorer, NULL, "i", 0L) ||
		!Sc_NoText()) {
		return Mode_Done(&t, 0);
	}
	Mode_ActionSetName(v);
	Native_Free(lname);
	if (!Mode_Put(SC, "AcActionSet", NULL, v) || !Mode_ActionSet("Inactive") || !Mode_GVarSetS("EscapeEvent", "Scene.AbortScene") ||
		!Mode_VObj(l, "Name", lname) || !Mode_Put(SC, "SceneName", NULL, lname)) {
		return Mode_Done(&t, 0);
	}
	Native_Free(v);
	if (!Native_GVarGet("SoundDopplerLevel", v) || !Mode_Put(SC, "LastDopplerLevel", NULL, v) ||
		!Mode_GVarSetN("SoundDopplerLevel", 0.0) || !Mode_StopAllSounds() || !Mode_GVarSetI("ForceNoActionMusic", 1)) {
		return Mode_Done(&t, 0);
	}
	Native_SetFloat(time, Mode_Time());
	Native_SetStr(&args[0], "DisableSkipSlot");
	Native_SetInt(&args[1], 1);
	/* Scrap.Set as the module holds it: the scheduler takes its entry (Port_Scheduled) */
	nativeVal_t *set = Mode_V(&t);
	byte ok = Mode_RefMod("Scrap", "Set", set) && Mode_ScheduleV(time, set, args, 2, NULL);
	Native_Free(actors);
	Native_Free(list);
	n = ok && Sc_G("Actors", actors) && Mode_Sub(actors, cls, list) ? Mode_Loop(list) : -1;
	for (int i = 0; i < n; i++) {
		Native_Free(actr);
		if (!Native_SeqItem(list, i, actr) || !Sc_SetActor(actr, l)) {
			return Mode_Done(&t, 0);
		}
	}
	return Mode_Done(&t, n >= 0);
}

/* HideThis(o) of the global of the module, the value read before the global as the script reads it */
static byte Sc_HideOf(const nativeVal_t *o) {
	return PORT_OWN(Scene, HideThis, NULL, "v", o);
}

/* module.var.Invulnerable = i (a handle of another module, Scrap.UsrEntity(0) for module NULL) */
static byte Sc_Invulnerable(const char *module, const char *var, long i) {
	nativeVal_t e = { 0 };
	byte ok = (module ? LvN_ModGet(module, var, &e) : Mode_UsrHandle(&e)) && LvN_SetI(&e, "Invulnerable", i);
	Native_Free(&e);
	return ok;
}

/* InitOutDoorScene(HideChars=1): the police (during an alarm) and the enemies hide; the ship of the player, the
 * player and Sputnik hide too, or (HideChars 0) stay and cannot be hurt; no input, the cinema HUD */
static byte Sc_InitOutDoorScene(const nativeVal_t *hide) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *e = Mode_V(&t), *empty = Mode_V(&t), *list = Mode_V(&t), *next = Mode_V(&t), *o = Mode_V(&t), *v = Mode_V(&t);
	if (!Mode_Import("Parking") || !Mode_Import("Menu") || !Mode_Import("Sputnik") || !Sc_NoBlur() || !Mode_First(e)) {
		return Mode_Done(&t, 0);
	}
	Native_SetEmpty(empty, NV_LIST);
	if (!Mode_Put(SC, "HidenChars", NULL, empty)) {
		return Mode_Done(&t, 0);
	}
	while (Native_True(e)) {
		int police = Mode_AlarmOn();
		if (police > 0) {
			police = Mode_VObj(e, "EntityList", list) ? Native_IsStr(list, "Police") : -1;
			Native_Free(list);
		}
		int enemy = 0;
		if (police == 0) {
			enemy = Mode_VObj(e, "EntityList", list) ? Native_IsStr(list, "EnemiesTraffic") : -1;
			Native_Free(list);
		}
		if (police < 0 || enemy < 0 || ((police > 0 || enemy > 0) && !Sc_HideOf(e)) || !Mode_VObj(e, "NextInSlot", next)) {
			return Mode_Done(&t, 0);
		}
		Native_Free(e);
		byte ok = Mode_EntitySeam(next, e);
		Native_Free(next);
		if (!ok) {
			return Mode_Done(&t, 0);
		}
	}
	byte on = Native_True(hide);
	if (!LvN_PutI(SC, "InvulnerableChars", !on)) {
		return Mode_Done(&t, 0);
	}
	if (on) {
		if (!LvN_ModGet("Parking", "PlayerShip", o) || !Sc_HideOf(o) || !Mode_UsrHandle(v) || !Sc_HideOf(v)) {
			return Mode_Done(&t, 0);
		}
		Native_Free(o);
		if (!LvN_ModGet("Sputnik", "spt", o) || !Sc_HideOf(o)) {
			return Mode_Done(&t, 0);
		}
	} else if (!Sc_Invulnerable("Parking", "PlayerShip", 1) || !Sc_Invulnerable(NULL, NULL, 1) ||
		!Sc_Invulnerable("Sputnik", "spt", 1)) {
		return Mode_Done(&t, 0);
	}
	Native_Free(v);
	Mode_ActionSetName(v);
	return Mode_Done(&t, Mode_Put(SC, "PrevInput", NULL, v) && Mode_ActionSet("Inactive") &&
		PORT_CALL(Menu, MovieScorer, NULL, "i", 0L));
}

/* EndOutDoorScene(): back from InitOutDoorScene, the camera on its main target, the input and the HUD of before */
static byte Sc_EndOutDoorScene(void) {
	modeVals_t t = { .n = 0 };
	nativeVal_t *inv = Mode_V(&t), *cam = Mode_V(&t), *target = Mode_V(&t), *prev = Mode_V(&t);
	if (!Mode_Import("Menu") || !Mode_Import("Parking") || !Mode_Import("Sputnik") || !Sc_G("InvulnerableChars", inv)) {
		return Mode_Done(&t, 0);
	}
	if (Native_True(inv)) {
		if (!Sc_Invulnerable("Parking", "PlayerShip", 0) || !Sc_Invulnerable(NULL, NULL, 0) || !Sc_Invulnerable("Sputnik", "spt", 0)) {
			return Mode_Done(&t, 0);
		}
	} else if (!PORT_OWN(Scene, UnhideChars, NULL, "")) {
		return Mode_Done(&t, 0);
	}
	return Mode_Done(&t, Mode_Cam0(cam) && Mode_VObj(cam, "MainTarget", target) && Mode_VSet(cam, "Target", target) &&
		LvN_SetI(cam, "AddTurn", 0) && Sc_G("PrevInput", prev) && Mode_ActionSetV(prev) && Mode_Read(SC, "PrevInput", prev) &&
		PORT_CALL(Menu, DrawScorer, NULL, "iv", 0L, prev));
}

/* ---- entries ---- */

MODE_FN(Scene, BaseAddActor, 3, Sc_BaseAddActor(&a[0], &a[1], &a[2]))
MODE_FN(Scene, AddCharActor, 2, Sc_AddCharActor(&a[0], &a[1]))
MODE_FN(Scene, AddLocalActor, 2, Sc_AddLocalActor(&a[0], &a[1]))
MODE_FN(Scene, ActorActEvent, 3, Sc_ActEvent(&a[0], &a[1], 1))
MODE_FN(Scene, ActorBreakChannel, 3, Sc_ActorBreakChannel(&a[0], &a[1], &a[2]))
MODE_FN(Scene, ActorActBreakChanne, 3, Sc_ActorActBreakChanne(&a[0], &a[1]))
MODE_FN(Scene, ObjActEvent, 3, Sc_ActEvent(&a[0], &a[1], 0))
MODE_FN(Scene, SetDefSounds, 2, Sc_SetDefSounds(&a[0], &a[1]))
MODE_FN(Scene, ExecGeneric, 3, Sc_ExecGeneric(&a[0], &a[1], &a[2]))
MODE_FN(Scene, PlayStream, 3, Sc_PlayStream(&a[1]))
MODE_FN(Scene, PlayVoiceVol, 2, Mode_PlayVoice(&a[0], NULL) && Mode_VoiceVolume(&a[0], &a[1]))
MODE_FN(Scene, PlayText, 3, Sc_PlayText(&a[1]))
MODE_FN(Scene, SetText, 2, Sc_SetText(&a[0], &a[1]))
MODE_FN(Scene, AbortScene, 1, Sc_AbortScene())
MODE_FN(Scene, EndOfScene, 3, Sc_EndOfScene(&a[0]))
MODE_FN(Scene, DelClass, 1, Sc_DelClass(&a[0]))
MODE_FN(Scene, Load, 1, Sc_Load(&a[0]))
MODE_FN(Scene, UnhideChars, 0, Sc_UnhideChars())
MODE_FN(Scene, HideThis, 1, Sc_HideThis(&a[0]))
MODE_FN(Scene, HideChars, 0, Sc_HideChars())
MODE_FN(Scene, HideVehiclesToo, 0, Sc_HideType("Car"))
MODE_FN(Scene, Play, 1, Sc_Play(&a[0]))
MODE_FN(Scene, EndOutDoorScene, 0, Sc_EndOutDoorScene())

/* the keyword functions: the arguments by getargs.c Arg_ParseTupleAndKeywords, the defaults of the script (MODE_KW) */
MODE_KW(Scene, ActorAction, "OOO|O", "frame", "Actor", "Action", "DefAction") {
	Mode_KwDefS(v, given, 3, "Relax");
	return Sc_Action(&v[0], &v[1], &v[2], &v[3], "Scene.ActorActEvent");
}

MODE_KW(Scene, ObjAction, "OOO|O", "frame", "Actor", "Action", "DefAction") {
	Mode_KwDefS(v, given, 3, "Relax");
	return Sc_Action(&v[0], &v[1], &v[2], &v[3], "Scene.ObjActEvent");
}

MODE_KW(Scene, CreateLocalAction, "OO|OOO", "CharName", "turnspeed", "Name", "Anm", "Loop") {
	Mode_KwDefS(v, given, 2, "Relax");
	Mode_KwDefS(v, given, 3, "Relax");
	Mode_KwDefI(v, given, 4, 0);
	return Sc_CreateLocalAction(&v[0], &v[1], &v[2], &v[3], &v[4]);
}

MODE_KW(Scene, AddGeneric, "OO|O", "Frame", "Event", "Discardable") {
	Mode_KwDefI(v, given, 2, 1);
	return Sc_AddGeneric(&v[0], &v[1], &v[2]);
}

MODE_KW(Scene, AddStream, "OO|O", "frame", "filename", "vol") {
	Mode_KwDefF(v, given, 2, 1.0);
	return Sc_AddStream(&v[0], &v[1], &v[2]);
}

MODE_KW(Scene, StopSpeech, "|O", "id") {
	return Sc_StopSpeech();
}

MODE_KW(Scene, AddText, "OO|O", "frame", "lngtext", "time") {
	Mode_KwDefI(v, given, 2, 86400);
	return Sc_AddText(&v[0], &v[1], &v[2]);
}

MODE_KW(Scene, AddSound, "OOO|OO", "Frame", "SoundName", "Vol", "Pitch", "Pan") {
	Mode_KwDefI(v, given, 3, 0);
	Mode_KwDefI(v, given, 4, 0);
	return Sc_AddSound(&v[0], &v[1], &v[2], &v[3], &v[4]);
}

MODE_KW(Scene, InitOutDoorScene, "|O", "HideChars") {
	Mode_KwDefI(v, given, 0, 1);
	return Sc_InitOutDoorScene(&v[0]);
}

const portFn_t g_SceneMethods[] = {
	PORT_FN(BaseAddActor, Scene_BaseAddActor),
	PORT_FN(AddCharActor, Scene_AddCharActor),
	PORT_FN(AddLocalActor, Scene_AddLocalActor),
	PORT_FNK(ActorAction, Scene_ActorAction),
	PORT_FN(ActorActEvent, Scene_ActorActEvent),
	PORT_FN(ActorBreakChannel, Scene_ActorBreakChannel),
	PORT_FN(ActorActBreakChanne, Scene_ActorActBreakChanne),
	PORT_FNK(ObjAction, Scene_ObjAction),
	PORT_FN(ObjActEvent, Scene_ObjActEvent),
	PORT_FN(SetDefSounds, Scene_SetDefSounds),
	PORT_FNK(CreateLocalAction, Scene_CreateLocalAction),
	PORT_FN(ExecGeneric, Scene_ExecGeneric),
	PORT_FNK(AddGeneric, Scene_AddGeneric),
	PORT_FNK(AddStream, Scene_AddStream),
	PORT_FN(PlayStream, Scene_PlayStream),
	PORT_FN(PlayVoiceVol, Scene_PlayVoiceVol),
	PORT_FN(PlayText, Scene_PlayText),
	PORT_FN(SetText, Scene_SetText),
	PORT_FNK(StopSpeech, Scene_StopSpeech),
	PORT_FNK(AddText, Scene_AddText),
	PORT_FN(AbortScene, Scene_AbortScene),
	PORT_FN(EndOfScene, Scene_EndOfScene),
	PORT_FN(DelClass, Scene_DelClass),
	PORT_FNK(AddSound, Scene_AddSound),
	PORT_FN(Load, Scene_Load),
	PORT_FN(UnhideChars, Scene_UnhideChars),
	PORT_FN(HideThis, Scene_HideThis),
	PORT_FN(HideChars, Scene_HideChars),
	PORT_FN(HideVehiclesToo, Scene_HideVehiclesToo),
	PORT_FN(Play, Scene_Play),
	PORT_FNK(InitOutDoorScene, Scene_InitOutDoorScene),
	PORT_FN(EndOutDoorScene, Scene_EndOutDoorScene),
	PORT_FNEND
};

PORTS(Scene)
