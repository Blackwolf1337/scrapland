/*
 * The reader of the engine3d command file, original source Engine/cmd.cpp (include/scrap/cmd.h).
 */
#include "scrap/cmd.h"
#include "scrap/crt.h"
#include "scrap/e3dutil.h"
#include "scrap/escena.h"
#include "scrap/lex.h"
#include "scrap/material.h"
#include "scrap/modelo.h"
#include "scrap/mundo.h"
#include "scrap/varios.h"

void Cmd_AddAnim(void) {
	CmdLex_Next();
	CModelo *m = Cmd_GetModelo();
	if (m) {
		Crear_Animacion(m, g_Cl.text, 3);
		int i = CAnimGrupos_Count(&m->anims) - 1;
		CHECK(CModelo_GetAnimName(m, i));
		CModelo_AddAnimEvents(m, CModelo_GetAnimName(m, i), -1.0f, -1.0f);
		CModelo_SetAnim(m, i, 0);
	}
	CmdLex_Next();
}

void Cmd_Load(void) {
	char name[0x40];
	char file[MAX_RUTA];
	vec3_t pos = { 0.0f, 0.0f, 0.0f };
	vec3_t scale;
	double d;

	CmdLex_Next();
	Cad_Copy(name, g_Cl.text, sizeof(name));
	CmdLex_Next();
	Ruta_Copy(file, g_Cl.text, -1);
	CmdLex_Next();
	Cmd_GetVec(0, &pos, 0);
	float s = CmdLex_GetDouble(&d) ? 1.0f : (float)d;
	scale.x = s;
	scale.y = s;
	scale.z = s;
	CModelo *m = Crear_Modelo3(file, NULL, &scale, 7);
	Cad_Set(&m->name, name);
	m->pos = pos;
	if (Cmd_GetBool(1)) {
		CModelo_BuildNodosR(m, 0);
	}
	CModelo_AddAnimEvents(m, CModelo_GetAnimName(m, 0), -1.0f, -1.0f);
	CModelo_SetAnim(m, 0, 0);
}

void Cmd_PivotPos(void) {
	CmdLex_Next();
	CModelo *m = Cmd_GetModelo();
	if (m) {
		CModelo_SetPivotNodo(m, g_Cl.text);
	}
	CmdLex_Next();
}

void Cmd_PlayAnim(void) {
	CmdLex_Next();
	CModelo *m = Cmd_GetModelo();
	if (m) {
		int i = CModelo_FindAnim(m, g_Cl.text);
		CmdLex_Next();
		CHECK(g_Cl.type == CLEX_INT);
		CModelo_SetAnim(m, i, (uint32_t)g_Cl.value.i);
	}
	CmdLex_Next();
}

void Cmd_EfecMalla(void) {
	char kind[0x40];

	CmdLex_Next();
	Cad_Copy(kind, g_Cl.text, sizeof(kind));
	CmdLex_Next();
	CModelo *m = Cmd_GetModelo();
	if (!m) {
		return;
	}
	if (!crt_stricmp(kind, "pantalla")) {
		EfecMalla_AddPantalla(m);
		CModelo_Disable(m);
		return;
	}
	if (!crt_stricmp(kind, "modelo")) {
		EfecMalla_AddModelo(m);
		CModelo_Disable(m);
		return;
	}
	Lex_Error(0, "comando no v\xe1lido: %s", kind);
}

void Cmd_FundirNodos(void) {
	CmdLex_Next();
	CModelo *m = Cmd_GetModelo();
	if (m) {
		CEscena_Join(m->escena, 1, NULL);
	}
}

CModelo *Cmd_GetModelo(void) {
	CHECK((g_Cl.type == CLEX_IDENT) || (g_Cl.type == CLEX_STRING));
	CModelo *m = Modelo_FindName(g_Cl.text);
	if (!m) {
		Lex_Error(0, "Model not found: %s", g_Cl.text);
	}
	CmdLex_Next();
	return m;
}

int Cmd_GetVec(byte next, vec3_t *out, int unused) {
	(void)unused;
	if (CmdLex_IsChar('$')) {
		CmdLex_Next();
		CHECK(g_Cl.type == CLEX_IDENT);
		CNodo3D *n = CMundo_FindNodoWild(Mundo_Get(), g_Cl.text);
		if (!n) {
			Lex_Error(0, "nodo no encontrado: %s", g_Cl.text);
			CmdLex_Next();
			return -1;
		}
		*out = *(const vec3_t *)&n->m.m[12];
		CmdLex_Next();
		return 0;
	}
	if (CmdLex_NeedChar('(')) {
		return -1;
	}
	if (Cmd_GetFloat(next, &out->x, ',') || Cmd_GetFloat(0, &out->y, ',') || Cmd_GetFloat(0, &out->z, ')')) {
		return -1;
	}
	return 0;
}

byte Cmd_GetBool(byte def) {
	CHECK(g_Cl.type == CLEX_IDENT);
	if (!crt_stricmp(g_Cl.text, "false")) {
		CmdLex_Next();
		return 0;
	}
	if (!crt_stricmp(g_Cl.text, "true")) {
		CmdLex_Next();
		return 1;
	}
	Lex_Error(0, "Invalid boolean: %s", g_Cl.text);
	CmdLex_Next();
	return def;
}

int Cmd_GetFloat(byte next, float *out, int end) {
	double d;

	CHECK(out);
	if (next) {
		CmdLex_Next();
	}
	if (CmdLex_GetDouble(&d)) {
		return -1;
	}
	*out = (float)d;
	if (end && CmdLex_NeedChar(end)) {
		return -1;
	}
	return 0;
}

void Cmd_SkipToIdent(void) {
	while (g_Cl.type != CLEX_IDENT && g_Cl.type != CLEX_ERR) {
		Lex_Error(0, "no es un comando: %s", g_Cl.text);
		CmdLex_Next();
	}
}

int Cmd_NextInt(void) {
	CmdLex_Next();
	CHECK(g_Cl.type == CLEX_INT);
	return g_Cl.value.i;
}

void Cmd_Out(const char *text) {
	E3D_DebugPrintf("%s", text);
}

void Cmd_ReadFile(const char *name) {
	char path[MAX_RUTA];

	Ruta_Normalize(path, name, -1);
	/* the exe's address of Cmd_Out, as the original stores it */
	Lex_SetOut((lexout_t)Game_Ptr(0x280af0));
	Lex_SetDir(path);
	if (crt_access(path, 0) || Lex_Open(path, 0)) {
		return;
	}
	g_Cl.type = CLEX_IDENT;
	CmdLex_Next();
	for (;;) {
		Cmd_SkipToIdent();
		if (g_Cl.type == CLEX_ERR) {
			break;
		}
		if (!crt_stricmp(g_Cl.text, "Load")) {
			Cmd_Load();
		} else if (!crt_stricmp(g_Cl.text, "AddAnim")) {
			Cmd_AddAnim();
		} else if (!crt_stricmp(g_Cl.text, "PlayAnim")) {
			Cmd_PlayAnim();
		} else if (!crt_stricmp(g_Cl.text, "PivotPos")) {
			Cmd_PivotPos();
		} else if (!crt_stricmp(g_Cl.text, "FUNDIR_NODOS")) {
			Cmd_FundirNodos();
		} else if (!crt_stricmp(g_Cl.text, "EFEC_MALLA")) {
			Cmd_EfecMalla();
		} else {
			Lex_Error(0, "comando desconocido: '%s'", g_Cl.text);
			CmdLex_Next();
		}
	}
	Lex_Close(2);
	if (g_LexErrors) {
		Err_Printf(1, "lee_cmd(): error procesando '%s'", path);
	}
}
