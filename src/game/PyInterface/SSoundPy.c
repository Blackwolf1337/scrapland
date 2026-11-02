/*
 * Python module SSound (method table SSound_methods, rva 0x42f558): sounds, music, voices and sound property
 * sets. The original source file is not known (no CHECK in it); the other modules are PyInterface/S*Py.cpp.
 */
#include "scrap/python.h"
#include "scrap/sound.h"

/* SetVoiceVolume(voice, volume) */
PyObject *SSound_SetVoiceVolume(PyObject *self, PyObject *args) {
	int voice;
	double volume;

	(void)self;
	if (!PyArg_ParseTuple(args, "id", &voice, &volume)) {
		return NULL;
	}
	g_pSound->vftable->SetVoiceVolume(g_pSound, voice, (float)volume);
	return Py_ReturnNone();
}

/* SetVoiceString(voice, text) */
PyObject *SSound_SetVoiceString(PyObject *self, PyObject *args) {
	int voice;
	char *text;

	(void)self;
	if (!PyArg_ParseTuple(args, "is", &voice, &text)) {
		return NULL;
	}
	g_pSound->vftable->SetVoiceString(g_pSound, voice, text);
	return Py_ReturnNone();
}

/* DeleteAllSounds() */
PyObject *SSound_DeleteAllSounds(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	g_pSound->vftable->DeleteAllSounds(g_pSound);
	return Py_ReturnNone();
}

/* LoadSound(name) */
PyObject *SSound_LoadSound(PyObject *self, PyObject *args) {
	char *name;

	(void)self;
	if (!PyArg_ParseTuple(args, "s", &name)) {
		return NULL;
	}
	g_pSound->vftable->LoadSound(g_pSound, name);
	return Py_ReturnNone();
}

/* PlaySound((x,y,z), name, volume[, p1, p2, p3]) : sound at a position; p1, p2 default 0, p3 default 1 */
PyObject *SSound_PlaySound(PyObject *self, PyObject *args) {
	double x, y, z, volume, p1 = 0.0, p2 = 0.0, p3 = 1.0;
	char *name;
	cSoundDesc desc;
	vec3_t pos;

	(void)self;
	if (!PyArg_ParseTuple(args, "(ddd)sd|ddd", &x, &y, &z, &name, &volume, &p1, &p2, &p3)) {
		return NULL;
	}
	cSoundDesc_Init(&desc);
	desc.param1 = (float)p1;
	desc.param2 = (float)p2;
	desc.param3 = (float)p3;
	pos.x = (float)x;
	pos.y = (float)y;
	pos.z = (float)z;
	cSoundDesc_SetPos(&desc, &pos);
	desc.volume += Snd_Volume((float)volume);
	g_pSound->vftable->PlaySound(g_pSound, name, &desc, 0);
	return Py_ReturnNone();
}

/* Play(name[, volume[, pan]]) : 2D sound; volume default 1, pan default 0 (-1..1, sent as pan * 10000) */
PyObject *SSound_Play(PyObject *self, PyObject *args) {
	char *name;
	double volume = 1.0, pan = 0.0;

	(void)self;
	if (!PyArg_ParseTuple(args, "s|dd", &name, &volume, &pan)) {
		return NULL;
	}
	int ipan = (int)(pan * 10000.0);
	g_pSound->vftable->Play(g_pSound, name, Snd_Volume((float)volume), ipan);
	return Py_ReturnNone();
}

/* StopAllSounds() */
PyObject *SSound_StopAllSounds(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	g_pSound->vftable->StopAllSounds(g_pSound);
	return Py_ReturnNone();
}

/*
 * SetPS(psname, var, value) : value as string ("sss") or number ("ssd", stored as double and truncated int).
 * As compiled: when the string form parses but the set does not exist, the number form is tried and fails with
 * a TypeError; an unknown set with a number returns None.
 */
PyObject *SSound_SetPS(PyObject *self, PyObject *args) {
	char *psName, *var, *s;
	double d;
	int i;
	cSoundPS *ps;

	(void)self;
	if (PyArg_ParseTuple(args, "sss", &psName, &var, &s) && (ps = SndPS_Get(psName))) {
		return Py_BuildValue("i", cSoundPS_Var(ps, var, 1, &s, NULL, NULL));
	}
	PyErr_Clear();
	if (!PyArg_ParseTuple(args, "ssd", &psName, &var, &d)) {
		return NULL;
	}
	ps = SndPS_Get(psName);
	i = (int)d;
	if (!ps) {
		return Py_ReturnNone();
	}
	return Py_BuildValue("i", cSoundPS_Var(ps, var, 1, NULL, &d, &i));
}

/* GetPS(psname, var) : value in its type; None for an unknown set or variable */
PyObject *SSound_GetPS(PyObject *self, PyObject *args) {
	char *psName, *var, *s;
	double d;
	int i;

	(void)self;
	if (!PyArg_ParseTuple(args, "ss", &psName, &var)) {
		return NULL;
	}
	cSoundPS *ps = SndPS_Get(psName);
	if (ps) {
		switch (cSoundPS_Var(ps, var, 0, &s, &d, &i)) {
		case 1:
			return Py_BuildValue("s", s);
		case 2:
			return Py_BuildValue("d", d);
		case 3:
			return Py_BuildValue("i", i);
		}
	}
	return Py_ReturnNone();
}

/* CreatePS(psname) : 1 if the set was made */
PyObject *SSound_CreatePS(PyObject *self, PyObject *args) {
	char *psName;

	(void)self;
	if (!PyArg_ParseTuple(args, "s", &psName)) {
		return NULL;
	}
	return Py_BuildValue("i", SndPS_Create(psName) != NULL);
}

/* UsePS(on) */
PyObject *SSound_UsePS(PyObject *self, PyObject *args) {
	int on;

	(void)self;
	if (!PyArg_ParseTuple(args, "i", &on)) {
		return NULL;
	}
	return Py_BuildValue("i", SndPS_Use(on != 0) != 0);
}

/* SetMusic(name[, volume]) : volume default 1, set before the music */
PyObject *SSound_SetMusic(PyObject *self, PyObject *args) {
	char *name;
	double volume = 1.0;

	(void)self;
	if (!PyArg_ParseTuple(args, "s|d", &name, &volume)) {
		return NULL;
	}
	g_pSound->vftable->SetMusicVolume(g_pSound, (float)volume);
	g_pSound->vftable->SetMusic(g_pSound, name);
	return Py_ReturnNone();
}

/* SetMusicVolume(volume) */
PyObject *SSound_SetMusicVolume(PyObject *self, PyObject *args) {
	double volume = 1.0;

	(void)self;
	if (!PyArg_ParseTuple(args, "d", &volume)) {
		return NULL;
	}
	g_pSound->vftable->SetMusicVolume(g_pSound, (float)volume);
	return Py_ReturnNone();
}

/* OpenVoice(name, voice[, arg]) : arg default 1 */
PyObject *SSound_OpenVoice(PyObject *self, PyObject *args) {
	char *name;
	int voice, arg = 1;

	(void)self;
	if (!PyArg_ParseTuple(args, "si|i", &name, &voice, &arg)) {
		return NULL;
	}
	g_pSound->vftable->OpenVoice(g_pSound, name, voice, arg);
	return Py_ReturnNone();
}

/* PlayVoice(voice[, text]) : plays and sets the subtitle text (default "") */
PyObject *SSound_PlayVoice(PyObject *self, PyObject *args) {
	int voice;
	char *text = "";

	(void)self;
	if (!PyArg_ParseTuple(args, "i|s", &voice, &text)) {
		return NULL;
	}
	g_pSound->vftable->PlayVoice(g_pSound, voice);
	g_pSound->vftable->SetVoiceString(g_pSound, voice, text);
	return Py_ReturnNone();
}

/* StopVoice(voice) */
PyObject *SSound_StopVoice(PyObject *self, PyObject *args) {
	int voice;

	(void)self;
	if (!PyArg_ParseTuple(args, "i", &voice)) {
		return NULL;
	}
	g_pSound->vftable->StopVoice(g_pSound, voice);
	return Py_ReturnNone();
}

/* VoiceRemain(voice, default) : remaining time of the voice, the default when the voice reports none */
PyObject *SSound_VoiceRemain(PyObject *self, PyObject *args) {
	int voice;
	double result;
	float remain;

	(void)self;
	if (!PyArg_ParseTuple(args, "id", &voice, &result)) {
		return NULL;
	}
	if (g_pSound->vftable->VoiceRemain(g_pSound, voice, &remain)) {
		result = (double)remain;
	}
	return Py_BuildValue("d", result);
}

SD_SYM(str_PyMethod_SSound_LoadSound);
SD_SYM(SSound_LoadSound);
SD_SYM(str_PyDoc_SSound_LoadSound);
SD_SYM(str_PyMethod_SSound_PlaySound);
SD_SYM(SSound_PlaySound);
SD_SYM(str_PyDoc_SSound_PlaySound);
SD_SYM(str_PyMethod_SSound_Play);
SD_SYM(SSound_Play);
SD_SYM(str_PyDoc_SSound_Play);
SD_SYM(str_PyMethod_SSound_StopAllSounds);
SD_SYM(SSound_StopAllSounds);
SD_SYM(str_PyDoc_SSound_StopAllSounds);
SD_SYM(str_PyMethod_SSound_SetPS);
SD_SYM(SSound_SetPS);
SD_SYM(str_PyDoc_SSound_SetPS);
SD_SYM(str_PyMethod_SSound_GetPS);
SD_SYM(SSound_GetPS);
SD_SYM(str_PyDoc_SSound_GetPS);
SD_SYM(str_PyMethod_SSound_UsePS);
SD_SYM(SSound_UsePS);
SD_SYM(str_PyDoc_SSound_UsePS);
SD_SYM(str_PyMethod_SSound_CreatePS);
SD_SYM(SSound_CreatePS);
SD_SYM(str_PyDoc_SSound_CreatePS);
SD_SYM(str_PyMethod_SSound_SetMusic);
SD_SYM(SSound_SetMusic);
SD_SYM(str_PyDoc_SSound_SetMusic);
SD_SYM(str_PyMethod_SSound_SetMusicVolume);
SD_SYM(SSound_SetMusicVolume);
SD_SYM(str_PyDoc_SSound_SetMusicVolume);
SD_SYM(str_PyMethod_SSound_OpenVoice);
SD_SYM(SSound_OpenVoice);
SD_SYM(str_PyDoc_SSound_OpenVoice);
SD_SYM(str_PyMethod_SSound_PlayVoice);
SD_SYM(SSound_PlayVoice);
SD_SYM(str_PyDoc_SSound_PlayVoice);
SD_SYM(str_PyMethod_SSound_StopVoice);
SD_SYM(SSound_StopVoice);
SD_SYM(str_PyDoc_SSound_StopVoice);
SD_SYM(str_PyMethod_SSound_VoiceRemain);
SD_SYM(SSound_VoiceRemain);
SD_SYM(str_PyDoc_SSound_VoiceRemain);
SD_SYM(str_PyMethod_SSound_SetVoiceVolume);
SD_SYM(SSound_SetVoiceVolume);
SD_SYM(str_PyDoc_SSound_SetVoiceVolume);
SD_SYM(str_PyMethod_SSound_SetVoiceString);
SD_SYM(SSound_SetVoiceString);
SD_SYM(str_PyDoc_SSound_SetVoiceString);
SD_SYM(str_PyMethod_SSound_DeleteAllSounds);
SD_SYM(SSound_DeleteAllSounds);
SD_SYM(str_PyDoc_SSound_DeleteAllSounds);
PyMethodDef SSound_methods[18] = {
	{
		.ml_name = (void *)sd_str_PyMethod_SSound_LoadSound,
		.ml_meth = (void *)sd_SSound_LoadSound,
		.ml_flags = 1,
		.ml_doc = (void *)sd_str_PyDoc_SSound_LoadSound,
	},
	{
		.ml_name = (void *)sd_str_PyMethod_SSound_PlaySound,
		.ml_meth = (void *)sd_SSound_PlaySound,
		.ml_flags = 1,
		.ml_doc = (void *)sd_str_PyDoc_SSound_PlaySound,
	},
	{
		.ml_name = (void *)sd_str_PyMethod_SSound_Play,
		.ml_meth = (void *)sd_SSound_Play,
		.ml_flags = 1,
		.ml_doc = (void *)sd_str_PyDoc_SSound_Play,
	},
	{
		.ml_name = (void *)sd_str_PyMethod_SSound_StopAllSounds,
		.ml_meth = (void *)sd_SSound_StopAllSounds,
		.ml_flags = 1,
		.ml_doc = (void *)sd_str_PyDoc_SSound_StopAllSounds,
	},
	{
		.ml_name = (void *)sd_str_PyMethod_SSound_SetPS,
		.ml_meth = (void *)sd_SSound_SetPS,
		.ml_flags = 1,
		.ml_doc = (void *)sd_str_PyDoc_SSound_SetPS,
	},
	{
		.ml_name = (void *)sd_str_PyMethod_SSound_GetPS,
		.ml_meth = (void *)sd_SSound_GetPS,
		.ml_flags = 1,
		.ml_doc = (void *)sd_str_PyDoc_SSound_GetPS,
	},
	{
		.ml_name = (void *)sd_str_PyMethod_SSound_UsePS,
		.ml_meth = (void *)sd_SSound_UsePS,
		.ml_flags = 1,
		.ml_doc = (void *)sd_str_PyDoc_SSound_UsePS,
	},
	{
		.ml_name = (void *)sd_str_PyMethod_SSound_CreatePS,
		.ml_meth = (void *)sd_SSound_CreatePS,
		.ml_flags = 1,
		.ml_doc = (void *)sd_str_PyDoc_SSound_CreatePS,
	},
	{
		.ml_name = (void *)sd_str_PyMethod_SSound_SetMusic,
		.ml_meth = (void *)sd_SSound_SetMusic,
		.ml_flags = 1,
		.ml_doc = (void *)sd_str_PyDoc_SSound_SetMusic,
	},
	{
		.ml_name = (void *)sd_str_PyMethod_SSound_SetMusicVolume,
		.ml_meth = (void *)sd_SSound_SetMusicVolume,
		.ml_flags = 1,
		.ml_doc = (void *)sd_str_PyDoc_SSound_SetMusicVolume,
	},
	{
		.ml_name = (void *)sd_str_PyMethod_SSound_OpenVoice,
		.ml_meth = (void *)sd_SSound_OpenVoice,
		.ml_flags = 1,
		.ml_doc = (void *)sd_str_PyDoc_SSound_OpenVoice,
	},
	{
		.ml_name = (void *)sd_str_PyMethod_SSound_PlayVoice,
		.ml_meth = (void *)sd_SSound_PlayVoice,
		.ml_flags = 1,
		.ml_doc = (void *)sd_str_PyDoc_SSound_PlayVoice,
	},
	{
		.ml_name = (void *)sd_str_PyMethod_SSound_StopVoice,
		.ml_meth = (void *)sd_SSound_StopVoice,
		.ml_flags = 1,
		.ml_doc = (void *)sd_str_PyDoc_SSound_StopVoice,
	},
	{
		.ml_name = (void *)sd_str_PyMethod_SSound_VoiceRemain,
		.ml_meth = (void *)sd_SSound_VoiceRemain,
		.ml_flags = 1,
		.ml_doc = (void *)sd_str_PyDoc_SSound_VoiceRemain,
	},
	{
		.ml_name = (void *)sd_str_PyMethod_SSound_SetVoiceVolume,
		.ml_meth = (void *)sd_SSound_SetVoiceVolume,
		.ml_flags = 1,
		.ml_doc = (void *)sd_str_PyDoc_SSound_SetVoiceVolume,
	},
	{
		.ml_name = (void *)sd_str_PyMethod_SSound_SetVoiceString,
		.ml_meth = (void *)sd_SSound_SetVoiceString,
		.ml_flags = 1,
		.ml_doc = (void *)sd_str_PyDoc_SSound_SetVoiceString,
	},
	{
		.ml_name = (void *)sd_str_PyMethod_SSound_DeleteAllSounds,
		.ml_meth = (void *)sd_SSound_DeleteAllSounds,
		.ml_flags = 1,
		.ml_doc = (void *)sd_str_PyDoc_SSound_DeleteAllSounds,
	},
};

SD_PY_MODULE(SSound, 0x42f558)

const char str_PyMethod_SSound_LoadSound[] = "LoadSound";

const char str_PyMethod_SSound_PlaySound[] = "PlaySound";

const char str_PyMethod_SSound_Play[] = "Play";

const char str_PyMethod_SSound_StopAllSounds[] = "StopAllSounds";

const char str_PyMethod_SSound_SetPS[] = "SetPS";

const char str_PyMethod_SSound_GetPS[] = "GetPS";

const char str_PyMethod_SSound_UsePS[] = "UsePS";

const char str_PyMethod_SSound_CreatePS[] = "CreatePS";

const char str_PyMethod_SSound_SetMusic[] = "SetMusic";

const char str_PyMethod_SSound_SetMusicVolume[] = "SetMusicVolume";

const char str_PyMethod_SSound_OpenVoice[] = "OpenVoice";

const char str_PyMethod_SSound_PlayVoice[] = "PlayVoice";

const char str_PyMethod_SSound_StopVoice[] = "StopVoice";

const char str_PyMethod_SSound_VoiceRemain[] = "VoiceRemain";

const char str_PyMethod_SSound_SetVoiceVolume[] = "SetVoiceVolume";

const char str_PyMethod_SSound_SetVoiceString[] = "SetVoiceString";

const char str_PyMethod_SSound_DeleteAllSounds[] = "DeleteAllSounds";

char str_PyDoc_SSound_SetVoiceVolume[] = "SetVoiceVolume(channel,volume) : set the volume of a voice.";

char str_PyDoc_SSound_CreatePS[] = "CreatePS(Name) : Crea un nuevo sonido posicional";

char str_PyDoc_SSound_SetVoiceString[] =
	"SetVoiceString(channel,volume) : set string user data of a voice.";

char str_PyDoc_SSound_DeleteAllSounds[60] = "SCRAP_DeleteAllSounds() : elimina todos los sonidos.";

char str_PyDoc_SSound_LoadSound[] =
	"LoadSound(soundfile) : carga un sonido desde un archivo empaquetado.";

char str_PyDoc_SSound_PlaySound[] =
	"PlaySound((x,y,z),'soundname',vol[,AttenIni,AttenEnd,Doppler]) : ejecuta un sonido en la posicion da"
	"da con el volumen deseado.";

char str_PyDoc_SSound_Play[] =
	"Play('soundname'[,vol,pan]) : ejecuta un sonido con el volumen deseado.";

char str_PyDoc_SSound_StopAllSounds[] = "SCRAP_StopAllSounds() : para todos los sonidos.";

char str_PyDoc_SSound_SetPS[] =
	"SetPS(name,varname,value) :  Modifica el valor de una variable de un sonido posicional";

char str_PyDoc_SSound_GetPS[] =
	"GetPS(name,varname) :  Obtiene el valor de una variable de un sonido posicional";

char str_PyDoc_SSound_UsePS[] = "UsePS(Name) : Crea un nuevo sonido posicional";

char str_PyDoc_SSound_SetMusic[] =
	"SetMusic('soundname'[,vol]) : set the file of the background music.";

char str_PyDoc_SSound_SetMusicVolume[] =
	"SetMusicVolume(vel) : set the volume of the background music.";

char str_PyDoc_SSound_OpenVoice[60] = "OpenVoice(name,channel) : Preload a voice on a channel.";

char str_PyDoc_SSound_PlayVoice[] =
	"PlayVoice(channel) : play a voice loaded on the specified channel.";

char str_PyDoc_SSound_StopVoice[] = "StopVoice(channel) : stop a voice on the specified channel.";

char str_PyDoc_SSound_VoiceRemain[72] =
	"VoiceRemain(channel,time) : return the remaining time of a voice.";

