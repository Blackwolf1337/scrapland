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
 */
#include <limits.h>

#include "scrap/script.h"

#define SC "Scene"

/* a function of the module as its functions call it, with Py_BuildValue arguments */
#define Sc_Call(f, func, ...) Sf_Call(f, Sf_Global(f, SC, func), __VA_ARGS__)

/* Scrap.Set(name, value) */
#define Sc_Set(f, ...) Sf_Ask(f, "Scrap", "Set", __VA_ARGS__)

/* SAct.SetAct(var, value) */
#define Sc_Act(f, ...) Sf_Ask(f, "SAct", "SetAct", __VA_ARGS__)

/* Time = SAct.GetAct("TimeOfFrame" + `frame`); SAct.SetAct("AddEventAtTime", Time) */
static void Sc_EventAt(scriptFrame_t *f, PyObject *frame) {
	PyObject *t = Sf_Ask(f, "SAct", "GetAct", "(O)", Sf_Cat(f, "sO", "TimeOfFrame", Sf_Repr(f, frame)));
	Sc_Act(f, "(sO)", "AddEventAtTime", t);
}

/* the motion blur off for a scene */
static void Sc_NoBlur(scriptFrame_t *f) {
	Sc_Set(f, "(si)", "MotionBlurFactorBegin", 0);
	Sc_Set(f, "(sd)", "MotionBlurFactorEnd", 0.0);
	Sc_Set(f, "(sd)", "MotionBlurTime", 0.0);
	Sc_Set(f, "(sd)", "MotionBlurEndTime", 0.0);
	Sc_Set(f, "(sd)", "MotionBlurOffVar", 0.0);
	Sc_Set(f, "(si)", "R_SceneMotionBlurValue", 0);
}

/* SScorer.SetLabelText("", 0), SetMsgText("", 0), SetSpeechText("", 0, 255, 255, 255) */
static void Sc_NoText(scriptFrame_t *f) {
	Sf_Ask(f, "SScorer", "SetLabelText", "(si)", "", 0);
	Sf_Ask(f, "SScorer", "SetMsgText", "(si)", "", 0);
	Sf_Ask(f, "SScorer", "SetSpeechText", "(siiii)", "", 0, 255, 255, 255);
}

/* BaseAddActor(ActorName, ActorClass, pathfile): an actor of the scene (a tuple of its dummy, entity and class in
 * Actors[SceneName]) and its class */
static PyObject *Scene_BaseAddActor(PyObject *self, PyObject *args) {
	PyObject *name, *cls, *path;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOO", &name, &cls, &path)) {
		return NULL;
	}
	Sf_Import(&f, "MakeChar");
	Sf_SetMod(&f, "MakeChar", "MaxSpeed", Sf_Float(&f, 1.0));
	Sf_SetMod(&f, "MakeChar", "MinSpeed", Sf_Float(&f, 1.0));
	PyObject *actors = Sf_Global(&f, SC, "Actors");
	PyObject *the = Sf_GetItem(&f, actors, Sf_Global(&f, SC, "SceneName"));
	PyObject *actor = Sf_Op(&f, Sf_Global(&f, SC, "SceneName"), '+', name);
	PyObject *append = Sf_Attr(&f, the, "append");
	PyObject *dummy = Sf_Cat(&f, "sO", "Actor_", name);
	Sf_Call(&f, append, "(OOO)", dummy, actor, cls);
	Sf_Ask(&f, "SAct", "CreateClass", "(O)", actor);
	Sf_Ask(&f, "SAct", "SetCls", "(sO)", "ModelFilePath", path);
	return Sf_None(&f);
}

/* AddCharActor(ActorName, ActorClass): an actor with the model of a character, its relax and morph actions */
static PyObject *Scene_AddCharActor(PyObject *self, PyObject *args) {
	PyObject *name, *cls;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &name, &cls)) {
		return NULL;
	}
	Sf_Import(&f, "MakeChar");
	PyObject *fn = Sf_Global(&f, SC, "BaseAddActor");
	Sf_Call(&f, fn, "(OOO)", name, cls, Sf_Cat(&f, "sOsOs", "Models/Chars/", cls, "/", cls, ".M3D"));
	Sf_Ask(&f, "MakeChar", "CreateRelaxAction", "(Oissi)", cls, 0, "Relax", "Relax", 1);
	Sf_Ask(&f, "MakeChar", "AddDefMorphActions", "(O)", cls);
	Sf_Ask(&f, "SAct", "SetCls", "(ss)", "DefAction", "Relax");
	return Sf_None(&f);
}

/* AddLocalActor(ActorName, ActorClass): an actor with a model of the scene's folder; its class goes with the scene
 * (LocalActors) */
static PyObject *Scene_AddLocalActor(PyObject *self, PyObject *args) {
	PyObject *name, *cls;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &name, &cls)) {
		return NULL;
	}
	Sf_Import(&f, "Init");
	PyObject *fn = Sf_Global(&f, SC, "BaseAddActor");
	PyObject *path = Sf_Cat(&f, "OsOsOs", Sf_Mod(&f, "Init", "Path"), "/Map/", Sf_Global(&f, SC, "SceneName"), "/", cls,
		".M3D");
	Sf_Call(&f, fn, "(OOO)", name, cls, path);
	Sf_Ask(&f, "SAct", "SetCls", "(si)", "Discardable", 1);
	PyObject *local = Sf_Global(&f, SC, "LocalActors");
	int has = Sf_True(&f, Sf_Method(&f, local, "has_key", "(O)", Sf_Global(&f, SC, "SceneName")));
	if (has == 0) {
		Sf_SetItem(&f, Sf_Global(&f, SC, "LocalActors"), Sf_Global(&f, SC, "SceneName"), Sf_Keep(&f, PyList_New(0)));
	}
	PyObject *list = Sf_GetItem(&f, Sf_Global(&f, SC, "LocalActors"), Sf_Global(&f, SC, "SceneName"));
	if (Sf_In(&f, cls, list) == 0) {
		list = Sf_GetItem(&f, Sf_Global(&f, SC, "LocalActors"), Sf_Global(&f, SC, "SceneName"));
		Sf_Method(&f, list, "append", "(O)", cls);
	}
	return Sf_None(&f);
}

/* ActorAction(frame, Actor, Action, DefAction="Relax"): at the frame the actor changes its action */
static PyObject *Scene_ActorAction(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "frame", "Actor", "Action", "DefAction", NULL };
	PyObject *frame, *actor, *action, *def = NULL;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "OOO|O", names, &frame, &actor, &action, &def)) {
		return NULL;
	}
	def = def ? def : Sf_Str(&f, "Relax");
	Sc_EventAt(&f, frame);
	Sc_Act(&f, "(sO)", "EventName", Sf_Cat(&f, "sOsOsOs", "'", actor, "','", action, "','", def, "'"));
	Sc_Act(&f, "(ss)", "EventFunc", "Scene.ActorActEvent");
	return Sf_None(&f);
}

/* Actor, Action, DefAction = eval(EventName); the entity of the scene's actor or the object forces the action */
static PyObject *Sc_ActEvent(PyObject *args, byte actor) {
	PyObject *ename, *event, *time, *v[3];
	scriptFrame_t f = { 0 };
	if (!PyArg_ParseTuple(args, "OOO", &ename, &event, &time)) {
		return NULL;
	}
	Sf_Unpack(&f, Sf_Eval(&f, event, SC), 3, v);
	PyObject *name = actor ? Sf_Op(&f, Sf_Slice(&f, ename, 9, INT_MAX), '+', v[0]) : v[0];
	PyObject *e = Sf_Entity(&f, name);
	Sf_SetAttr(&f, e, "ForceAction", v[1]);
	Sf_SetAttr(&f, e, "ForceDefLoop", v[2]);
	return Sf_None(&f);
}

/* ActorActEvent(EntityName, EventName, Time) */
static PyObject *Scene_ActorActEvent(PyObject *self, PyObject *args) {
	(void)self;
	return Sc_ActEvent(args, 1);
}

/* ActorBreakChannel(frame, Actor, Channel): at the frame the actor stops a secondary channel */
static PyObject *Scene_ActorBreakChannel(PyObject *self, PyObject *args) {
	PyObject *frame, *actor, *channel;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOO", &frame, &actor, &channel)) {
		return NULL;
	}
	Sc_EventAt(&f, frame);
	Sc_Act(&f, "(sO)", "EventName", Sf_Cat(&f, "sOsO", "'", actor, "',", Sf_Repr(&f, channel)));
	Sc_Act(&f, "(ss)", "EventFunc", "Scene.ActorActBreakChanne");
	return Sf_None(&f);
}

/* ActorActBreakChanne(EntityName, EventName, Time) */
static PyObject *Scene_ActorActBreakChanne(PyObject *self, PyObject *args) {
	PyObject *ename, *event, *time, *v[2];
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOO", &ename, &event, &time)) {
		return NULL;
	}
	Sf_Unpack(&f, Sf_Eval(&f, event, SC), 2, v);
	PyObject *e = Sf_Entity(&f, Sf_Op(&f, Sf_Slice(&f, ename, 9, INT_MAX), '+', v[0]));
	Sf_SetAttr(&f, e, "StopSecondary", v[1]);
	return Sf_None(&f);
}

/* ObjAction(frame, Actor, Action, DefAction="Relax"): at the frame an object of the level changes its action */
static PyObject *Scene_ObjAction(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "frame", "Actor", "Action", "DefAction", NULL };
	PyObject *frame, *actor, *action, *def = NULL;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "OOO|O", names, &frame, &actor, &action, &def)) {
		return NULL;
	}
	def = def ? def : Sf_Str(&f, "Relax");
	Sc_EventAt(&f, frame);
	Sc_Act(&f, "(sO)", "EventName", Sf_Cat(&f, "sOsOsOs", "'", actor, "','", action, "','", def, "'"));
	Sc_Act(&f, "(ss)", "EventFunc", "Scene.ObjActEvent");
	return Sf_None(&f);
}

/* ObjActEvent(EntityName, EventName, Time) */
static PyObject *Scene_ObjActEvent(PyObject *self, PyObject *args) {
	(void)self;
	return Sc_ActEvent(args, 0);
}

/* SetDefSounds(ActorName, ActorType): the sounds of the character's sound script on the actor's class, then the
 * MapSnd function of the actor when the level has one */
static PyObject *Scene_SetDefSounds(PyObject *self, PyObject *args) {
	PyObject *name, *type;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &name, &type)) {
		return NULL;
	}
	/* the function has exec, so its variables are a dict the exec'd lines share */
	PyObject *locals = Sf_Keep(&f, Py_BuildValue("{s:O,s:O}", "ActorName", name, "ActorType", type));
	Sf_Exec(&f, Sf_Cat(&f, "sOs", "import ", type, "Sound"), SC, locals);
	PyObject *scene = Sf_Global(&f, SC, "SceneName");
	Sf_Exec(&f, Sf_Cat(&f, "OsOOs", type, "Sound.SetSounds('", scene, name, "')"), SC, locals);
	if (f.failed) {
		return Sf_None(&f);
	}
	scene = Sf_Global(&f, SC, "SceneName");
	Sf_Exec(&f, Sf_Cat(&f, "sOOs", "import MapSnd;MapSnd.", scene, name, "()"), SC, locals);
	if (Sf_Except(&f)) {
		scene = Sf_Global(&f, SC, "SceneName");
		Sf_Print(&f, "O", Sf_Cat(&f, "sOOs", "Sound warning: MapSnd.", scene, name, "() not found\n"));
	}
	return Sf_None(&f);
}

/* CreateLocalAction(CharName, turnspeed, Name="Relax", Anm="Relax", Loop=0): an action of an actor with an
 * animation of the scene's folder */
static PyObject *Scene_CreateLocalAction(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "CharName", "turnspeed", "Name", "Anm", "Loop", NULL };
	PyObject *ch, *speed, *name = NULL, *anm = NULL, *loop = NULL;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "OO|OOO", names, &ch, &speed, &name, &anm, &loop)) {
		return NULL;
	}
	name = name ? name : Sf_Str(&f, "Relax");
	anm = anm ? anm : Sf_Str(&f, "Relax");
	loop = loop ? loop : Sf_Int(&f, 0);
	Sf_Import(&f, "Init");
	Sf_Ask(&f, "SAct", "CreateAction", "(O)", name);
	PyObject *path = Sf_Mod(&f, "Init", "Path");
	PyObject *scene = Sf_Global(&f, SC, "SceneName");
	path = Sf_Cat(&f, "OsOsOsOsOs", path, "/Map/", scene, "/", scene, "Anm/", ch, "_", anm, ".M3D");
	Sc_Act(&f, "(sO)", "AnmFilePath", path);
	Sc_Act(&f, "(sO)", "isLoop", loop);
	Sc_Act(&f, "(sO)", "RotSpeed", speed);
	Sc_Act(&f, "(si)", "AngRotLimit", 90);
	Sc_Act(&f, "(si)", "UseDisp", 0);
	Sc_Act(&f, "(si)", "NodeModifier", -1);
	Sc_Act(&f, "(sd)", "AngleModifierSpeed", 5.0);
	return Sf_None(&f);
}

/* ExecGeneric(EntityName, EventName, Time): the event is a line of Python */
static PyObject *Scene_ExecGeneric(PyObject *self, PyObject *args) {
	PyObject *ename, *event, *time;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOO", &ename, &event, &time)) {
		return NULL;
	}
	PyObject *locals = Sf_Keep(&f, Py_BuildValue("{s:O,s:O,s:O}", "EntityName", ename, "EventName", event, "Time",
		time));
	Sf_Exec(&f, event, SC, locals);
	return Sf_None(&f);
}

/* AddGeneric(Frame, Event, Discardable=1): a line of Python at the frame */
static PyObject *Scene_AddGeneric(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "Frame", "Event", "Discardable", NULL };
	PyObject *frame, *event, *disc = NULL;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "OO|O", names, &frame, &event, &disc)) {
		return NULL;
	}
	disc = disc ? disc : Sf_Int(&f, 1);
	Sc_EventAt(&f, frame);
	Sc_Act(&f, "(ss)", "EventFunc", "Scene.ExecGeneric");
	Sc_Act(&f, "(sO)", "EventName", event);
	Sc_Act(&f, "(sO)", "EventDiscardable", disc);
	return Sf_None(&f);
}

/* AddStream(frame, filename, vol=1.0): a stream of the stream folder at the frame */
static PyObject *Scene_AddStream(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "frame", "filename", "vol", NULL };
	PyObject *frame, *file, *vol = NULL;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "OO|O", names, &frame, &file, &vol)) {
		return NULL;
	}
	vol = vol ? vol : Sf_Float(&f, 1.0);
	Sc_EventAt(&f, frame);
	Sc_Act(&f, "(sO)", "EventName", Sf_Cat(&f, "OsO", file, "\001", Sf_Repr(&f, vol)));
	Sc_Act(&f, "(ss)", "EventFunc", "Scene.PlayStream");
	return Sf_None(&f);
}

/* PlayStream(EntityName, EventName, Time): opens the stream on voice channel 2 and plays it half a second later at
 * its volume */
static PyObject *Scene_PlayStream(PyObject *self, PyObject *args) {
	PyObject *ename, *event, *time;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOO", &ename, &event, &time)) {
		return NULL;
	}
	Sf_Import(&f, "string");
	PyObject *cads = Sf_Ask(&f, "string", "split", "(Os)", event, "\001");
	PyObject *path = Sf_Ask(&f, "Scrap", "Get", "(s)", "StreamPath");
	path = Sf_Cat(&f, "OOs", path, Sf_Item(&f, cads, 0), ".ogg");
	Sf_Ask(&f, "SSound", "OpenVoice", "(Oii)", path, 2, 0);
	PyObject *t = Sf_Op(&f, Sf_Time(&f), '+', Sf_Float(&f, 0.5));
	PyObject *fn = Sf_Global(&f, SC, "PlayVoiceVol");
	PyObject *vol = Sf_Ask(&f, "string", "atof", "(O)", Sf_Item(&f, cads, 1));
	Sf_Schedule(&f, t, fn, Sf_Build(&f, "(iO)", 2, vol), NULL);
	return Sf_None(&f);
}

/* PlayVoiceVol(chan, vol) */
static PyObject *Scene_PlayVoiceVol(PyObject *self, PyObject *args) {
	PyObject *chan, *vol;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &chan, &vol)) {
		return NULL;
	}
	Sf_Ask(&f, "SSound", "PlayVoice", "(O)", chan);
	Sf_Ask(&f, "SSound", "SetVoiceVolume", "(OO)", chan, vol);
	return Sf_None(&f);
}

/* PlayText(EntityName, EventName, Time): the subtitle of the event ("time\001key") */
static PyObject *Scene_PlayText(PyObject *self, PyObject *args) {
	PyObject *ename, *event, *time;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOO", &ename, &event, &time)) {
		return NULL;
	}
	Sf_Import(&f, "string");
	PyObject *cads = Sf_Ask(&f, "string", "split", "(Os)", event, "\001");
	PyObject *fn = Sf_Global(&f, SC, "SetText");
	PyObject *cad = Sf_Item(&f, cads, 1);
	Sf_Call(&f, fn, "(OO)", cad, Sf_Ask(&f, "string", "atoi", "(O)", Sf_Item(&f, cads, 0)));
	return Sf_None(&f);
}

/* SetText(Cad, Time): the voice of the language key on the next voice channel and its text (Time 0: the time the
 * text takes to read) */
static PyObject *Scene_SetText(PyObject *self, PyObject *args) {
	PyObject *cad, *time;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &cad, &time)) {
		return NULL;
	}
	Sf_Import(&f, "Speech");
	if (Sf_EqI(&f, time, 0) > 0) {
		time = Sf_Ask(&f, "Speech", "CalculateSpeechTime", "(O)", Sf_Lang(&f, cad));
	}
	PyObject *path = Sf_Cat(&f, "OO", Sf_Ask(&f, "Scrap", "Get", "(s)", "VoicePath"), cad);
	Sf_Ask(&f, "SSound", "OpenVoice", "(OO)", path, Sf_Global(&f, SC, "VoiceChannel"));
	Sf_Ask(&f, "Speech", "PlayVoiceScheduled", "(Odi)", Sf_Global(&f, SC, "VoiceChannel"), 0.25, 0);
	int on = Sf_True(&f, Sf_Global(&f, SC, "VoiceChannel"));
	if (on >= 0) {
		Sf_SetGlobal(&f, SC, "VoiceChannel", Sf_Int(&f, !on));
	}
	Sf_Ask(&f, "SScorer", "SetSpeechCallback", "(s)", "Scene.StopSpeech");
	Sf_Ask(&f, "SScorer", "SetSpeechText", "(OOiii)", Sf_Lang(&f, cad), time, 220, 235, 255);
	return Sf_None(&f);
}

/* StopSpeech(id=0): the voice that plays */
static PyObject *Scene_StopSpeech(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "id", NULL };
	PyObject *id = NULL;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "|O", names, &id)) {
		return NULL;
	}
	int on = Sf_True(&f, Sf_Global(&f, SC, "VoiceChannel"));
	if (on >= 0) {
		Sf_Ask(&f, "SSound", "StopVoice", "(i)", !on);
	}
	return Sf_None(&f);
}

/* AddText(frame, lngtext, time=86400): a subtitle at the frame */
static PyObject *Scene_AddText(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "frame", "lngtext", "time", NULL };
	PyObject *frame, *text, *time = NULL;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "OO|O", names, &frame, &text, &time)) {
		return NULL;
	}
	time = time ? time : Sf_Int(&f, 86400);
	Sc_EventAt(&f, frame);
	Sc_Act(&f, "(sO)", "EventName", Sf_Cat(&f, "OsO", Sf_Repr(&f, time), "\001", text));
	Sc_Act(&f, "(ss)", "EventFunc", "Scene.PlayText");
	return Sf_None(&f);
}

/* AbortScene(id): Escape ends the scene */
static PyObject *Scene_AbortScene(PyObject *self, PyObject *args) {
	PyObject *id;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &id)) {
		return NULL;
	}
	PyObject *fn = Sf_Global(&f, SC, "EndOfScene");
	Sf_Call(&f, fn, "(Osi)", Sf_Global(&f, SC, "SceneName"), "EndOfScene", 0);
	return Sf_None(&f);
}

/* EndOfScene(EntityName, EventName, Time): the scene and its actors go, the characters come back, the camera, the
 * HUD and the input of the player return; EndOfSceneCallback(usr) runs last */
static PyObject *Scene_EndOfScene(PyObject *self, PyObject *args) {
	PyObject *ename, *event, *time;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "OOO", &ename, &event, &time)) {
		return NULL;
	}
	Sf_SetGlobal(&f, SC, "SceneName", Py_None);
	Sf_Ask(&f, "SSound", "StopAllSounds", "()");
	Sc_Set(&f, "(si)", "ForceNoActionMusic", 0);
	Sf_Ask(&f, "SSound", "StopVoice", "(i)", 0);
	Sf_Ask(&f, "SSound", "StopVoice", "(i)", 1);
	Sf_Ask(&f, "SSound", "StopVoice", "(i)", 2);
	PyObject *cam = Sf_Entity(&f, Sf_Ask(&f, "Scrap", "GetCam", "(i)", 0));
	Sf_SetAttrS(&f, cam, "EntityLink", "");
	Sc_Call(&f, "UnhideChars", "()");
	PyObject *l = Sf_Entity(&f, ename);
	Sf_Method(&f, l, "Del", "()");
	PyObject *sname = Sf_Slice(&f, ename, 9, INT_MAX);
	PyObject *list = Sf_GetItem(&f, Sf_Global(&f, SC, "Actors"), sname);
	for (int i = 0;; i++) {
		int mark = f.n;
		PyObject *actr = Sf_Next(&f, list, i);
		if (!actr) {
			break;
		}
		Sf_Method(&f, Sf_Entity(&f, Sf_Item(&f, actr, 1)), "Del", "()");
		Sf_Release(&f, mark);
	}
	Sf_Import(&f, "CharScorer");
	Sf_Ask(&f, "CharScorer", "Create", "(i)", 0);
	Sc_NoText(&f);
	Sf_Ask(&f, "SInput", "SetActionSet", "(O)", Sf_Global(&f, SC, "AcActionSet"));
	Sc_Set(&f, "(ss)", "EscapeEvent", "Menu.Init");
	Sc_Set(&f, "(sO)", "SoundDopplerLevel", Sf_Global(&f, SC, "LastDopplerLevel"));
	PyObject *t = Sf_Op(&f, Sf_Time(&f), '+', Sf_Float(&f, 0.2));
	Sf_Schedule(&f, t, Sf_Global(&f, SC, "DelClass"), Sf_Build(&f, "(O)", sname), NULL);
	Sc_Set(&f, "(si)", "DisableSkipSlot", 0);
	Sc_Set(&f, "(si)", "PauseInactivityTime", 0);
	Sc_Set(&f, "(si)", "CameraScene", 0);
	PyObject *cb = Sf_Global(&f, SC, "EndOfSceneCallback");
	if (Sf_Compare(&f, cb, "!=", Py_None) > 0) {
		Sf_SetGlobal(&f, SC, "EndOfSceneCallback", Py_None);
		Sf_Call(&f, cb, "(O)", Sf_Global(&f, SC, "usr"));
	}
	return Sf_None(&f);
}

/* DelClass(SName): the class of the scene and those of its local actors */
static PyObject *Scene_DelClass(PyObject *self, PyObject *args) {
	PyObject *sname;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &sname)) {
		return NULL;
	}
	Sf_Ask(&f, "SAct", "DelClass", "(O)", Sf_Cat(&f, "sO", "Scene", sname));
	PyObject *local = Sf_Global(&f, SC, "LocalActors");
	if (Sf_True(&f, Sf_Method(&f, local, "has_key", "(O)", sname)) > 0) {
		PyObject *list = Sf_GetItem(&f, Sf_Global(&f, SC, "LocalActors"), sname);
		for (int i = 0;; i++) {
			int mark = f.n;
			PyObject *anam = Sf_Next(&f, list, i);
			if (!anam) {
				break;
			}
			Sf_Ask(&f, "SAct", "DelClass", "(O)", Sf_Op(&f, sname, '+', anam));
			Sf_Release(&f, mark);
		}
		Sf_DelItem(&f, Sf_Global(&f, SC, "LocalActors"), sname);
	}
	return Sf_None(&f);
}

/* AddSound(Frame, SoundName, Vol, Pitch=0, Pan=0): a 2D sound at the frame (the engine's !AnmSound2DEvent) */
static PyObject *Scene_AddSound(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "Frame", "SoundName", "Vol", "Pitch", "Pan", NULL };
	PyObject *frame, *sound, *vol, *pitch = NULL, *pan = NULL;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "OOO|OO", names, &frame, &sound, &vol, &pitch, &pan)) {
		return NULL;
	}
	pitch = pitch ? pitch : Sf_Int(&f, 0);
	pan = pan ? pan : Sf_Int(&f, 0);
	Sc_EventAt(&f, frame);
	PyObject *name = Sf_Cat(&f, "OsOsOsO", sound, ":", Sf_Repr(&f, vol), ":", Sf_Repr(&f, pitch), ":",
		Sf_Repr(&f, pan));
	Sc_Act(&f, "(sO)", "EventName", name);
	Sc_Act(&f, "(ss)", "EventFunc", "!AnmSound2DEvent");
	return Sf_None(&f);
}

/* Load(SceneClass): the class of the scene from the level's Map/<scene> folder, its Play action ending in
 * EndOfScene, the MapSnd sounds of the scene; a new list of actors */
static PyObject *Scene_Load(PyObject *self, PyObject *args) {
	PyObject *cls;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &cls)) {
		return NULL;
	}
	Sf_Import(&f, "Init");
	PyObject *path = Sf_Cat(&f, "OsOs", Sf_Mod(&f, "Init", "Path"), "/Map/", cls, "/");
	PyObject *sname = Sf_Cat(&f, "sO", "Scene", cls);
	int there = Sf_True(&f, Sf_Ask(&f, "SAct", "GetClass", "(O)", sname));
	if (there != 0) {
		return Sf_None(&f);
	}
	Sf_Ask(&f, "SAct", "CreateClass", "(O)", Sf_Cat(&f, "sO", "Scene", cls));
	Sf_Ask(&f, "SAct", "SetCls", "(si)", "Discardable", 1);
	Sf_Ask(&f, "SAct", "SetCls", "(sO)", "ModelFilePath", Sf_Cat(&f, "OOs", path, cls, ".m3d"));
	Sf_Ask(&f, "SAct", "CreateAction", "(s)", "Play");
	Sc_Act(&f, "(sO)", "AnmFilePath", Sf_Cat(&f, "OOs", path, cls, "Anm/Play.m3d"));
	Sc_Act(&f, "(si)", "InterpTime", 0);
	Sc_Act(&f, "(si)", "isLoop", 0);
	PyObject *end = Sf_Op(&f, Sf_Ask(&f, "SAct", "GetAct", "(s)", "AnmTotalTime"), '-', Sf_Float(&f, 0.1));
	Sc_Act(&f, "(sO)", "AddEventAtTime", end);
	Sc_Act(&f, "(ss)", "EventName", "EndOfScene");
	Sc_Act(&f, "(ss)", "EventFunc", "Scene.EndOfScene");
	Sf_Ask(&f, "Scrap", "OpenPack", "(O)", Sf_Cat(&f, "Os", Sf_Mod(&f, "Init", "Path"), "/Sounds"));
	if (f.failed) {
		return Sf_None(&f);
	}
	/* the function has exec, so its variables (the import of Init too) are a dict the exec'd line shares */
	PyObject *init = Sf_Keep(&f, PyImport_ImportModule("Init"));
	PyObject *locals = Sf_Keep(&f, Py_BuildValue("{s:O,s:O,s:O}", "SceneClass", cls, "Init", init, "path", path));
	Sf_Exec(&f, Sf_Cat(&f, "sOs", "import MapSnd;MapSnd.", cls, "()"), SC, locals);
	if (Sf_Except(&f)) {
		Sf_Print(&f, "O", Sf_Cat(&f, "sOs", "Sound warning: MapSnd.", cls, "() not found\n"));
	}
	Sf_Ask(&f, "Scrap", "ClosePack", "()");
	Sf_Ask(&f, "SAct", "GetClass", "(O)", Sf_Cat(&f, "sO", "Scene", cls));
	Sf_Ask(&f, "SAct", "SetCls", "(ss)", "DefAction", "Play");
	Sf_SetGlobal(&f, SC, "SceneName", cls);
	Sf_SetItem(&f, Sf_Global(&f, SC, "Actors"), Sf_Global(&f, SC, "SceneName"), Sf_Keep(&f, PyList_New(0)));
	return Sf_None(&f);
}

/* UnhideChars(): the entities HideThis hid are active again */
static PyObject *Scene_UnhideChars(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	PyObject *list = Sf_Global(&f, SC, "HidenChars");
	for (int i = 0;; i++) {
		int mark = f.n;
		PyObject *ch = Sf_Next(&f, list, i);
		if (!ch) {
			break;
		}
		PyObject *e = Sf_Entity(&f, ch);
		if (Sf_True(&f, e) > 0) {
			Sf_SetAttrI(&f, e, "Active", 1);
			Sf_SetAttrI(&f, e, "TimeSpeed", 1);
		}
		Sf_Release(&f, mark);
	}
	return Sf_None(&f);
}

/* HideThis(e): an active entity goes inactive and frozen, its name into HidenChars */
static PyObject *Scene_HideThis(PyObject *self, PyObject *args) {
	PyObject *e;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &e)) {
		return NULL;
	}
	int hide = Sf_True(&f, e);
	if (hide > 0) {
		hide = Sf_True(&f, Sf_Attr(&f, e, "Active"));
	}
	if (hide > 0) {
		Sf_Method(&f, Sf_Global(&f, SC, "HidenChars"), "append", "(O)", Sf_Attr(&f, e, "Name"));
		Sf_SetAttrI(&f, e, "Active", 0);
		Sf_SetAttrI(&f, e, "TimeSpeed", 0);
	}
	return Sf_None(&f);
}

/* the entities of the slots from the first one: HideThis for those of the type */
static void Sc_HideType(scriptFrame_t *f, const char *type) {
	int mark = f->n;
	PyObject *e = Sf_Ask(f, "Scrap", "GetFirst", "()");
	while (Sf_True(f, e) > 0) {
		if (Sf_True(f, Sf_Method(f, e, "IsType", "(s)", type)) > 0) {
			Sc_Call(f, "HideThis", "(O)", e);
		}
		e = Sf_Entity(f, Sf_Attr(f, e, "NextInSlot"));
		Sf_ReleaseKeep(f, mark, &e, 1);
	}
}

/* HideChars(): the walking characters hide (the player does not select anything during the scene) */
static PyObject *Scene_HideChars(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_SetGlobal(&f, SC, "HidenChars", Sf_Keep(&f, PyList_New(0)));
	PyObject *e = Sf_Ask(&f, "Scrap", "UsrEntity", "(i)", 0);
	int walk = Sf_True(&f, e);
	if (walk > 0) {
		walk = Sf_True(&f, Sf_Method(&f, e, "IsType", "(s)", "WalkChar"));
	}
	if (walk > 0) {
		Sf_SetAttrS(&f, e, "SelectedEntity", "");
	}
	Sc_HideType(&f, "WalkChar");
	return Sf_None(&f);
}

/* HideVehiclesToo(): the ships hide too */
static PyObject *Scene_HideVehiclesToo(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sc_HideType(&f, "Car");
	return Sf_None(&f);
}

/* Play(SceneClass): loads the scene, creates its actors and the scene entity at the dummy DM_Scene_<name>, links
 * the camera, hides the characters (and outdoors the ships), the cinema HUD, no input, no doppler, no music */
static PyObject *Scene_Play(PyObject *self, PyObject *args) {
	PyObject *cls;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &cls)) {
		return NULL;
	}
	Sf_Import(&f, "MakeChar");
	Sf_Import(&f, "Init");
	Sc_NoBlur(&f);
	Sc_Set(&f, "(si)", "DrawNextFrame", 0);
	Sc_Call(&f, "Load", "(O)", cls);
	PyObject *list = Sf_GetItem(&f, Sf_Global(&f, SC, "Actors"), cls);
	for (int i = 0;; i++) {
		int mark = f.n;
		PyObject *actr = Sf_Next(&f, list, i);
		if (!actr) {
			break;
		}
		Sf_Ask(&f, "Scrap", "CreateEntity", "(Oiiis)", Sf_Item(&f, actr, 1), 0, 0, 0, "WithLife");
		Sf_Release(&f, mark);
	}
	Sf_Ask(&f, "Scrap", "StartDummySearch", "(Oi)", Sf_Cat(&f, "sO", "DM_Scene_", cls), 1);
	PyObject *dummy = Sf_Ask(&f, "Scrap", "NextDummySearch", "()");
	if (Sf_Compare(&f, dummy, "==", Py_None) > 0) {
		Sf_Ask(&f, "Scrap", "ConsoleError", "(O)", Sf_Cat(&f, "sOs", "TERROR : DM_Scene_", cls, " Dummy never exists!\n"));
		return Sf_None(&f);
	}
	PyObject *pos = Sf_Item(&f, dummy, 1);
	PyObject *l = Sf_Ask(&f, "Scrap", "CreateEntity", "(OOOOs)", Sf_Item(&f, dummy, 0), Sf_Item(&f, pos, 0),
		Sf_Item(&f, pos, 1), Sf_Item(&f, pos, 2), "WithLife");
	PyObject *ang = Sf_Item(&f, dummy, 2);
	Sf_SetAttr(&f, l, "Ang", Sf_Build(&f, "(OOi)", Sf_Item(&f, ang, 0), Sf_Item(&f, ang, 1), 0));
	Sf_SetAttr(&f, l, "ActCtrl", Sf_Cat(&f, "sO", "Scene", cls));
	PyObject *cam = Sf_Entity(&f, Sf_Ask(&f, "Scrap", "GetCam", "(i)", 0));
	Sf_SetAttr(&f, cam, "EntityLink", Sf_Attr(&f, l, "Name"));
	Sc_Call(&f, "HideChars", "()");
	if (Sf_True(&f, Sf_Mod(&f, "Init", "Outdoor")) > 0) {
		Sc_Call(&f, "HideVehiclesToo", "()");
	}
	Sf_SetGlobal(&f, SC, "usr", Sf_Ask(&f, "Scrap", "UsrEntity", "(i)", 0));
	Sc_Set(&f, "(si)", "PauseInactivityTime", 1);
	Sc_Set(&f, "(si)", "CameraScene", 1);
	Sf_Import(&f, "Menu");
	Sf_Ask(&f, "Menu", "MovieScorer", "(i)", 0);
	Sc_NoText(&f);
	Sf_SetGlobal(&f, SC, "AcActionSet", Sf_Ask(&f, "SInput", "GetActionSet", "()"));
	Sf_Ask(&f, "SInput", "SetActionSet", "(s)", "Inactive");
	Sc_Set(&f, "(ss)", "EscapeEvent", "Scene.AbortScene");
	Sf_SetGlobal(&f, SC, "SceneName", Sf_Attr(&f, l, "Name"));
	Sf_SetGlobal(&f, SC, "LastDopplerLevel", Sf_Ask(&f, "Scrap", "Get", "(s)", "SoundDopplerLevel"));
	Sc_Set(&f, "(sd)", "SoundDopplerLevel", 0.0);
	Sf_Ask(&f, "SSound", "StopAllSounds", "()");
	Sc_Set(&f, "(si)", "ForceNoActionMusic", 1);
	PyObject *t = Sf_Time(&f);
	Sf_Schedule(&f, t, Sf_Mod(&f, "Scrap", "Set"), Sf_Build(&f, "(si)", "DisableSkipSlot", 1), NULL);
	list = Sf_GetItem(&f, Sf_Global(&f, SC, "Actors"), cls);
	for (int i = 0;; i++) {
		int mark = f.n;
		PyObject *actr = Sf_Next(&f, list, i);
		if (!actr) {
			break;
		}
		PyObject *a = Sf_Entity(&f, Sf_Item(&f, actr, 1));
		Sf_SetAttr(&f, a, "ActCtrl", Sf_Item(&f, actr, 1));
		Sf_SetAttr(&f, a, "LinkModel", Sf_Cat(&f, "OsO", Sf_Attr(&f, l, "Name"), ",", Sf_Item(&f, actr, 0)));
		Sf_SetAttr(&f, a, "OffModPos", Sf_Build(&f, "(iii)", 0, 0, 0));
		Sf_SetAttrI(&f, a, "DeltaOffMul", 1);
		Sf_SetAttrI(&f, a, "OffMul", 1);
		Sf_SetAttrI(&f, a, "UseLinkAng", 1);
		if (Sf_In(&f, Sf_Item(&f, actr, 2), Sf_Mod(&f, "MakeChar", "MorphEyesList")) > 0) {
			Sf_SetAttrS(&f, a, "Action", "MorphEyes");
		}
		Sf_Release(&f, mark);
	}
	return Sf_None(&f);
}

/* InitOutDoorScene(HideChars=1): the police (during an alarm) and the enemies hide; the ship of the player, the
 * player and Sputnik hide too, or (HideChars 0) stay and cannot be hurt; no input, the cinema HUD */
static PyObject *Scene_InitOutDoorScene(PyObject *self, PyObject *args, PyObject *kw) {
	static char *names[] = { "HideChars", NULL };
	PyObject *hide = NULL;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTupleAndKeywords(args, kw, "|O", names, &hide)) {
		return NULL;
	}
	hide = hide ? hide : Sf_Int(&f, 1);
	Sf_Import(&f, "Parking");
	Sf_Import(&f, "Menu");
	Sf_Import(&f, "Sputnik");
	Sc_NoBlur(&f);
	int mark = f.n;
	PyObject *e = Sf_Ask(&f, "Scrap", "GetFirst", "()");
	Sf_SetGlobal(&f, SC, "HidenChars", Sf_Keep(&f, PyList_New(0)));
	Sf_ReleaseKeep(&f, mark, &e, 1);
	while (Sf_True(&f, e) > 0) {
		int police = Sf_True(&f, Sf_Item(&f, Sf_Ask(&f, "Scrap", "GetAlarm", "()"), 0));
		if (police > 0) {
			police = Sf_EqS(&f, Sf_Attr(&f, e, "EntityList"), "Police");
		}
		if (police > 0 || (police == 0 && Sf_EqS(&f, Sf_Attr(&f, e, "EntityList"), "EnemiesTraffic") > 0)) {
			Sc_Call(&f, "HideThis", "(O)", e);
		}
		e = Sf_Entity(&f, Sf_Attr(&f, e, "NextInSlot"));
		Sf_ReleaseKeep(&f, mark, &e, 1);
	}
	int on = Sf_True(&f, hide);
	if (on >= 0) {
		Sf_SetGlobal(&f, SC, "InvulnerableChars", Sf_Int(&f, !on));
	}
	if (on > 0) {
		Sc_Call(&f, "HideThis", "(O)", Sf_Mod(&f, "Parking", "PlayerShip"));
		Sc_Call(&f, "HideThis", "(O)", Sf_Ask(&f, "Scrap", "UsrEntity", "(i)", 0));
		Sc_Call(&f, "HideThis", "(O)", Sf_Mod(&f, "Sputnik", "spt"));
	} else if (on == 0) {
		Sf_SetAttrI(&f, Sf_Mod(&f, "Parking", "PlayerShip"), "Invulnerable", 1);
		Sf_SetAttrI(&f, Sf_Ask(&f, "Scrap", "UsrEntity", "(i)", 0), "Invulnerable", 1);
		Sf_SetAttrI(&f, Sf_Mod(&f, "Sputnik", "spt"), "Invulnerable", 1);
	}
	Sf_SetGlobal(&f, SC, "PrevInput", Sf_Ask(&f, "SInput", "GetActionSet", "()"));
	Sf_Ask(&f, "SInput", "SetActionSet", "(s)", "Inactive");
	Sf_Ask(&f, "Menu", "MovieScorer", "(i)", 0);
	return Sf_None(&f);
}

/* EndOutDoorScene(): back from InitOutDoorScene, the camera on its main target, the input and the HUD of before */
static PyObject *Scene_EndOutDoorScene(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "Menu");
	Sf_Import(&f, "Parking");
	Sf_Import(&f, "Sputnik");
	int inv = Sf_True(&f, Sf_Global(&f, SC, "InvulnerableChars"));
	if (inv > 0) {
		Sf_SetAttrI(&f, Sf_Mod(&f, "Parking", "PlayerShip"), "Invulnerable", 0);
		Sf_SetAttrI(&f, Sf_Ask(&f, "Scrap", "UsrEntity", "(i)", 0), "Invulnerable", 0);
		Sf_SetAttrI(&f, Sf_Mod(&f, "Sputnik", "spt"), "Invulnerable", 0);
	} else if (inv == 0) {
		Sc_Call(&f, "UnhideChars", "()");
	}
	PyObject *cam = Sf_Entity(&f, Sf_Ask(&f, "Scrap", "GetCam", "(i)", 0));
	Sf_SetAttr(&f, cam, "Target", Sf_Attr(&f, cam, "MainTarget"));
	Sf_SetAttrI(&f, cam, "AddTurn", 0);
	Sf_Ask(&f, "SInput", "SetActionSet", "(O)", Sf_Global(&f, SC, "PrevInput"));
	Sf_Ask(&f, "Menu", "DrawScorer", "(iO)", 0, Sf_Global(&f, SC, "PrevInput"));
	return Sf_None(&f);
}

#define KW(fn) (PyCFunction)(void (*)(void))fn, 3

PyMethodDef g_SceneMethods[] = {
	{ "BaseAddActor", Scene_BaseAddActor, 1, NULL },
	{ "AddCharActor", Scene_AddCharActor, 1, NULL },
	{ "AddLocalActor", Scene_AddLocalActor, 1, NULL },
	{ "ActorAction", KW(Scene_ActorAction), NULL },
	{ "ActorActEvent", Scene_ActorActEvent, 1, NULL },
	{ "ActorBreakChannel", Scene_ActorBreakChannel, 1, NULL },
	{ "ActorActBreakChanne", Scene_ActorActBreakChanne, 1, NULL },
	{ "ObjAction", KW(Scene_ObjAction), NULL },
	{ "ObjActEvent", Scene_ObjActEvent, 1, NULL },
	{ "SetDefSounds", Scene_SetDefSounds, 1, NULL },
	{ "CreateLocalAction", KW(Scene_CreateLocalAction), NULL },
	{ "ExecGeneric", Scene_ExecGeneric, 1, NULL },
	{ "AddGeneric", KW(Scene_AddGeneric), NULL },
	{ "AddStream", KW(Scene_AddStream), NULL },
	{ "PlayStream", Scene_PlayStream, 1, NULL },
	{ "PlayVoiceVol", Scene_PlayVoiceVol, 1, NULL },
	{ "PlayText", Scene_PlayText, 1, NULL },
	{ "SetText", Scene_SetText, 1, NULL },
	{ "StopSpeech", KW(Scene_StopSpeech), NULL },
	{ "AddText", KW(Scene_AddText), NULL },
	{ "AbortScene", Scene_AbortScene, 1, NULL },
	{ "EndOfScene", Scene_EndOfScene, 1, NULL },
	{ "DelClass", Scene_DelClass, 1, NULL },
	{ "AddSound", KW(Scene_AddSound), NULL },
	{ "Load", Scene_Load, 1, NULL },
	{ "UnhideChars", Scene_UnhideChars, 1, NULL },
	{ "HideThis", Scene_HideThis, 1, NULL },
	{ "HideChars", Scene_HideChars, 1, NULL },
	{ "HideVehiclesToo", Scene_HideVehiclesToo, 1, NULL },
	{ "Play", Scene_Play, 1, NULL },
	{ "InitOutDoorScene", KW(Scene_InitOutDoorScene), NULL },
	{ "EndOutDoorScene", Scene_EndOutDoorScene, 1, NULL },
	{ NULL, NULL, 0, NULL },
};
