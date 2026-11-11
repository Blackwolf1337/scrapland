/*
 * Python module SSound (method table SSound_methods): sounds, music, voices and sound property
 * sets. The original source file is not known (no CHECK in it); the other modules are PyInterface/S*Py.cpp.
 */
#include "scrap/python.h"
#include "scrap/scriptapi.h"
#include "scrap/sound.h"

/* SetVoiceVolume(voice, volume) */
API_FN(SSound, SetVoiceVolume) {
	int voice;
	double volume;
	if (!Mode_Parse(a, n, "id", &voice, &volume)) {
		return 0;
	}
	SoundSys_SetVoiceVolume(g_pSound, voice, (float)volume);
	return 1;
}

/* SetVoiceString(voice, text) */
API_FN(SSound, SetVoiceString) {
	int voice;
	const char *text;
	if (!Mode_Parse(a, n, "is", &voice, &text)) {
		return 0;
	}
	SoundSys_SetVoiceString(g_pSound, voice, text);
	return 1;
}

/* DeleteAllSounds() */
API_FN(SSound, DeleteAllSounds) {
	if (!Mode_Parse(a, n, "")) {
		return 0;
	}
	SoundSys_DeleteAllSounds(g_pSound);
	return 1;
}

/* LoadSound(name) */
API_FN(SSound, LoadSound) {
	const char *name;
	if (!Mode_Parse(a, n, "s", &name)) {
		return 0;
	}
	SoundSys_LoadSound(g_pSound, name);
	return 1;
}

/* PlaySound((x,y,z), name, volume[, p1, p2, p3]) : sound at a position; p1, p2 default 0, p3 default 1 */
void Sound_PlayAt(const vec3_t *pos, const char *name, double volume, double p1, double p2, double p3) {
	cSoundDesc desc;
	cSoundDesc_Init(&desc);
	desc.param1 = (float)p1;
	desc.param2 = (float)p2;
	desc.param3 = (float)p3;
	cSoundDesc_SetPos(&desc, pos);
	desc.volume += Snd_Volume((float)volume);
	SoundSys_PlaySound(g_pSound, name, &desc, 0);
}

API_FN(SSound, PlaySound) {
	double x, y, z, volume, p1 = 0.0, p2 = 0.0, p3 = 1.0;
	const char *name;
	if (!Mode_Parse(a, n, "(ddd)sd|ddd", &x, &y, &z, &name, &volume, &p1, &p2, &p3)) {
		return 0;
	}
	vec3_t pos = { (float)x, (float)y, (float)z };
	Sound_PlayAt(&pos, name, volume, p1, p2, p3);
	return 1;
}

/* Play(name[, volume[, pan]]) : 2D sound; volume default 1, pan default 0 (-1..1, sent as pan * 10000) */
API_FN(SSound, Play) {
	const char *name;
	double volume = 1.0, pan = 0.0;
	if (!Mode_Parse(a, n, "s|dd", &name, &volume, &pan)) {
		return 0;
	}
	int ipan = (int)(pan * 10000.0);
	SoundSys_Play(g_pSound, name, Snd_Volume((float)volume), ipan);
	return 1;
}

/* StopAllSounds() */
API_FN(SSound, StopAllSounds) {
	if (!Mode_Parse(a, n, "")) {
		return 0;
	}
	SoundSys_StopAllSounds(g_pSound);
	return 1;
}

/*
 * SetPS(psname, var, value) : value as string ("sss") or number ("ssd", stored as double and truncated int).
 * As compiled: when the string form parses but the set does not exist, the number form is tried and fails with
 * a TypeError; an unknown set with a number returns None.
 */
API_FN(SSound, SetPS) {
	const char *psName, *var, *s;
	double d;
	cSoundPS *ps;
	if (Mode_Parse(a, n, "sss", &psName, &var, &s) && (ps = SndPS_Get(psName))) {
		char *v = (char *)s;
		return Api_Build(out, "i", cSoundPS_Var(ps, var, 1, &v, NULL, NULL));
	}
	Native_ErrClear();
	if (!Mode_Parse(a, n, "ssd", &psName, &var, &d)) {
		return 0;
	}
	ps = SndPS_Get(psName);
	int i = (int)d;
	if (!ps) {
		return 1;
	}
	return Api_Build(out, "i", cSoundPS_Var(ps, var, 1, NULL, &d, &i));
}

/* GetPS(psname, var) : value in its type; None for an unknown set or variable */
API_FN(SSound, GetPS) {
	/* differs from the original: the values start at 0, as in SScorer.Get */
	const char *psName, *var;
	char *s = NULL;
	double d = 0.0;
	int i = 0;
	if (!Mode_Parse(a, n, "ss", &psName, &var)) {
		return 0;
	}
	cSoundPS *ps = SndPS_Get(psName);
	if (ps) {
		switch (cSoundPS_Var(ps, var, 0, &s, &d, &i)) {
		case 1:
			return Api_Build(out, "s", s);
		case 2:
			return Api_Build(out, "d", d);
		case 3:
			return Api_Build(out, "i", i);
		}
	}
	return 1;
}

/* CreatePS(psname) : 1 if the set was made */
API_FN(SSound, CreatePS) {
	const char *psName;
	return Mode_Parse(a, n, "s", &psName) && Api_Build(out, "i", SndPS_Create(psName) != NULL);
}

/* UsePS(on) */
API_FN(SSound, UsePS) {
	int on;
	return Mode_Parse(a, n, "i", &on) && Api_Build(out, "i", SndPS_Use(on != 0) != 0);
}

/* SetMusic(name[, volume]) : volume default 1, set before the music */
API_FN(SSound, SetMusic) {
	const char *name;
	double volume = 1.0;
	if (!Mode_Parse(a, n, "s|d", &name, &volume)) {
		return 0;
	}
	SoundSys_SetMusicVolume(g_pSound, (float)volume);
	SoundSys_SetMusic(g_pSound, name);
	return 1;
}

/* SetMusicVolume(volume) */
API_FN(SSound, SetMusicVolume) {
	double volume = 1.0;
	if (!Mode_Parse(a, n, "d", &volume)) {
		return 0;
	}
	SoundSys_SetMusicVolume(g_pSound, (float)volume);
	return 1;
}

/* OpenVoice(name, voice[, arg]) : arg default 1 */
API_FN(SSound, OpenVoice) {
	const char *name;
	int voice, arg = 1;
	if (!Mode_Parse(a, n, "si|i", &name, &voice, &arg)) {
		return 0;
	}
	SoundSys_OpenVoice(g_pSound, name, voice, arg);
	return 1;
}

/* PlayVoice(voice[, text]) : plays and sets the subtitle text (default "") */
API_FN(SSound, PlayVoice) {
	int voice;
	const char *text = "";
	if (!Mode_Parse(a, n, "i|s", &voice, &text)) {
		return 0;
	}
	SoundSys_PlayVoice(g_pSound, voice);
	SoundSys_SetVoiceString(g_pSound, voice, text);
	return 1;
}

/* StopVoice(voice) */
API_FN(SSound, StopVoice) {
	int voice;
	if (!Mode_Parse(a, n, "i", &voice)) {
		return 0;
	}
	SoundSys_StopVoice(g_pSound, voice);
	return 1;
}

/* VoiceRemain(voice, default) : remaining time of the voice, the default when the voice reports none */
API_FN(SSound, VoiceRemain) {
	int voice;
	double result;
	float remain;
	if (!Mode_Parse(a, n, "id", &voice, &result)) {
		return 0;
	}
	if (SoundSys_VoiceRemain(g_pSound, voice, &remain)) {
		result = (double)remain;
	}
	return Api_Build(out, "d", result);
}

static char str_PyDoc_SSound_CreatePS[];
static char str_PyDoc_SSound_DeleteAllSounds[60];
static char str_PyDoc_SSound_GetPS[];
static char str_PyDoc_SSound_LoadSound[];
static char str_PyDoc_SSound_OpenVoice[60];
static char str_PyDoc_SSound_Play[];
static char str_PyDoc_SSound_PlaySound[];
static char str_PyDoc_SSound_PlayVoice[];
static char str_PyDoc_SSound_SetMusic[];
static char str_PyDoc_SSound_SetMusicVolume[];
static char str_PyDoc_SSound_SetPS[];
static char str_PyDoc_SSound_SetVoiceString[];
static char str_PyDoc_SSound_SetVoiceVolume[];
static char str_PyDoc_SSound_StopAllSounds[];
static char str_PyDoc_SSound_StopVoice[];
static char str_PyDoc_SSound_UsePS[];
static char str_PyDoc_SSound_VoiceRemain[72];
static const char str_PyMethod_SSound_CreatePS[];
static const char str_PyMethod_SSound_DeleteAllSounds[];
static const char str_PyMethod_SSound_GetPS[];
static const char str_PyMethod_SSound_LoadSound[];
static const char str_PyMethod_SSound_OpenVoice[];
static const char str_PyMethod_SSound_Play[];
static const char str_PyMethod_SSound_PlaySound[];
static const char str_PyMethod_SSound_PlayVoice[];
static const char str_PyMethod_SSound_SetMusic[];
static const char str_PyMethod_SSound_SetMusicVolume[];
static const char str_PyMethod_SSound_SetPS[];
static const char str_PyMethod_SSound_SetVoiceString[];
static const char str_PyMethod_SSound_SetVoiceVolume[];
static const char str_PyMethod_SSound_StopAllSounds[];
static const char str_PyMethod_SSound_StopVoice[];
static const char str_PyMethod_SSound_UsePS[];
static const char str_PyMethod_SSound_VoiceRemain[];
static PyMethodDef SSound_methods[18] = {
	{
		.ml_name = (void *)str_PyMethod_SSound_LoadSound,
		.ml_meth = (void *)SSound_LoadSound,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SSound_LoadSound,
	},
	{
		.ml_name = (void *)str_PyMethod_SSound_PlaySound,
		.ml_meth = (void *)SSound_PlaySound,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SSound_PlaySound,
	},
	{
		.ml_name = (void *)str_PyMethod_SSound_Play,
		.ml_meth = (void *)SSound_Play,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SSound_Play,
	},
	{
		.ml_name = (void *)str_PyMethod_SSound_StopAllSounds,
		.ml_meth = (void *)SSound_StopAllSounds,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SSound_StopAllSounds,
	},
	{
		.ml_name = (void *)str_PyMethod_SSound_SetPS,
		.ml_meth = (void *)SSound_SetPS,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SSound_SetPS,
	},
	{
		.ml_name = (void *)str_PyMethod_SSound_GetPS,
		.ml_meth = (void *)SSound_GetPS,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SSound_GetPS,
	},
	{
		.ml_name = (void *)str_PyMethod_SSound_UsePS,
		.ml_meth = (void *)SSound_UsePS,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SSound_UsePS,
	},
	{
		.ml_name = (void *)str_PyMethod_SSound_CreatePS,
		.ml_meth = (void *)SSound_CreatePS,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SSound_CreatePS,
	},
	{
		.ml_name = (void *)str_PyMethod_SSound_SetMusic,
		.ml_meth = (void *)SSound_SetMusic,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SSound_SetMusic,
	},
	{
		.ml_name = (void *)str_PyMethod_SSound_SetMusicVolume,
		.ml_meth = (void *)SSound_SetMusicVolume,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SSound_SetMusicVolume,
	},
	{
		.ml_name = (void *)str_PyMethod_SSound_OpenVoice,
		.ml_meth = (void *)SSound_OpenVoice,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SSound_OpenVoice,
	},
	{
		.ml_name = (void *)str_PyMethod_SSound_PlayVoice,
		.ml_meth = (void *)SSound_PlayVoice,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SSound_PlayVoice,
	},
	{
		.ml_name = (void *)str_PyMethod_SSound_StopVoice,
		.ml_meth = (void *)SSound_StopVoice,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SSound_StopVoice,
	},
	{
		.ml_name = (void *)str_PyMethod_SSound_VoiceRemain,
		.ml_meth = (void *)SSound_VoiceRemain,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SSound_VoiceRemain,
	},
	{
		.ml_name = (void *)str_PyMethod_SSound_SetVoiceVolume,
		.ml_meth = (void *)SSound_SetVoiceVolume,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SSound_SetVoiceVolume,
	},
	{
		.ml_name = (void *)str_PyMethod_SSound_SetVoiceString,
		.ml_meth = (void *)SSound_SetVoiceString,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SSound_SetVoiceString,
	},
	{
		.ml_name = (void *)str_PyMethod_SSound_DeleteAllSounds,
		.ml_meth = (void *)SSound_DeleteAllSounds,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SSound_DeleteAllSounds,
	},
};

SD_PY_MODULE(SSound, 0x42f558)

static const char str_PyMethod_SSound_LoadSound[] = "LoadSound";
static const char str_PyMethod_SSound_PlaySound[] = "PlaySound";
static const char str_PyMethod_SSound_Play[] = "Play";
static const char str_PyMethod_SSound_StopAllSounds[] = "StopAllSounds";
static const char str_PyMethod_SSound_SetPS[] = "SetPS";
static const char str_PyMethod_SSound_GetPS[] = "GetPS";
static const char str_PyMethod_SSound_UsePS[] = "UsePS";
static const char str_PyMethod_SSound_CreatePS[] = "CreatePS";
static const char str_PyMethod_SSound_SetMusic[] = "SetMusic";
static const char str_PyMethod_SSound_SetMusicVolume[] = "SetMusicVolume";
static const char str_PyMethod_SSound_OpenVoice[] = "OpenVoice";
static const char str_PyMethod_SSound_PlayVoice[] = "PlayVoice";
static const char str_PyMethod_SSound_StopVoice[] = "StopVoice";
static const char str_PyMethod_SSound_VoiceRemain[] = "VoiceRemain";
static const char str_PyMethod_SSound_SetVoiceVolume[] = "SetVoiceVolume";
static const char str_PyMethod_SSound_SetVoiceString[] = "SetVoiceString";
static const char str_PyMethod_SSound_DeleteAllSounds[] = "DeleteAllSounds";

static char str_PyDoc_SSound_SetVoiceVolume[] = "SetVoiceVolume(channel,volume) : set the volume of a voice.";

static char str_PyDoc_SSound_CreatePS[] = "CreatePS(Name) : Crea un nuevo sonido posicional";

static char str_PyDoc_SSound_SetVoiceString[] =
	"SetVoiceString(channel,volume) : set string user data of a voice.";

static char str_PyDoc_SSound_DeleteAllSounds[60] = "SCRAP_DeleteAllSounds() : elimina todos los sonidos.";

static char str_PyDoc_SSound_LoadSound[] =
	"LoadSound(soundfile) : carga un sonido desde un archivo empaquetado.";

static char str_PyDoc_SSound_PlaySound[] =
	"PlaySound((x,y,z),'soundname',vol[,AttenIni,AttenEnd,Doppler]) : ejecuta un sonido en la posicion da"
	"da con el volumen deseado.";

static char str_PyDoc_SSound_Play[] =
	"Play('soundname'[,vol,pan]) : ejecuta un sonido con el volumen deseado.";

static char str_PyDoc_SSound_StopAllSounds[] = "SCRAP_StopAllSounds() : para todos los sonidos.";

static char str_PyDoc_SSound_SetPS[] =
	"SetPS(name,varname,value) :  Modifica el valor de una variable de un sonido posicional";

static char str_PyDoc_SSound_GetPS[] =
	"GetPS(name,varname) :  Obtiene el valor de una variable de un sonido posicional";

static char str_PyDoc_SSound_UsePS[] = "UsePS(Name) : Crea un nuevo sonido posicional";

static char str_PyDoc_SSound_SetMusic[] =
	"SetMusic('soundname'[,vol]) : set the file of the background music.";

static char str_PyDoc_SSound_SetMusicVolume[] =
	"SetMusicVolume(vel) : set the volume of the background music.";

static char str_PyDoc_SSound_OpenVoice[60] = "OpenVoice(name,channel) : Preload a voice on a channel.";

static char str_PyDoc_SSound_PlayVoice[] =
	"PlayVoice(channel) : play a voice loaded on the specified channel.";

static char str_PyDoc_SSound_StopVoice[] = "StopVoice(channel) : stop a voice on the specified channel.";

static char str_PyDoc_SSound_VoiceRemain[72] =
	"VoiceRemain(channel,time) : return the remaining time of a voice.";

PORTS(PyInterface_SSoundPy)
