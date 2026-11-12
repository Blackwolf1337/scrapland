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
static const portFn_t SSound_methods[] = {
	PORT_FND(LoadSound, SSound_LoadSound, str_PyDoc_SSound_LoadSound),
	PORT_FND(PlaySound, SSound_PlaySound, str_PyDoc_SSound_PlaySound),
	PORT_FND(Play, SSound_Play, str_PyDoc_SSound_Play),
	PORT_FND(StopAllSounds, SSound_StopAllSounds, str_PyDoc_SSound_StopAllSounds),
	PORT_FND(SetPS, SSound_SetPS, str_PyDoc_SSound_SetPS),
	PORT_FND(GetPS, SSound_GetPS, str_PyDoc_SSound_GetPS),
	PORT_FND(UsePS, SSound_UsePS, str_PyDoc_SSound_UsePS),
	PORT_FND(CreatePS, SSound_CreatePS, str_PyDoc_SSound_CreatePS),
	PORT_FND(SetMusic, SSound_SetMusic, str_PyDoc_SSound_SetMusic),
	PORT_FND(SetMusicVolume, SSound_SetMusicVolume, str_PyDoc_SSound_SetMusicVolume),
	PORT_FND(OpenVoice, SSound_OpenVoice, str_PyDoc_SSound_OpenVoice),
	PORT_FND(PlayVoice, SSound_PlayVoice, str_PyDoc_SSound_PlayVoice),
	PORT_FND(StopVoice, SSound_StopVoice, str_PyDoc_SSound_StopVoice),
	PORT_FND(VoiceRemain, SSound_VoiceRemain, str_PyDoc_SSound_VoiceRemain),
	PORT_FND(SetVoiceVolume, SSound_SetVoiceVolume, str_PyDoc_SSound_SetVoiceVolume),
	PORT_FND(SetVoiceString, SSound_SetVoiceString, str_PyDoc_SSound_SetVoiceString),
	PORT_FND(DeleteAllSounds, SSound_DeleteAllSounds, str_PyDoc_SSound_DeleteAllSounds),
	PORT_FNEND
};

SD_PY_MODULE(SSound, 0x42f558)


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
