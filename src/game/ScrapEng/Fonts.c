/*
 * Fonts.cpp of the game: the seven fonts of Fonts.fnt, see include/scrap/scrapfont.h.
 */
#include <stdio.h>

#include "scrap/console.h"
#include "scrap/crt.h"
#include "scrap/e3dutil.h"
#include "scrap/scrapfont.h"

static const char *const fontNames[7] = {
	"Terminal8", "Terminal8Fixed", "BigNumbers", "ScrapBig", "ScrapMedium", "Horatio", "HoratioSmall",
};

/* the global of the font, in the order of fontNames */
static CFont **Font_Slot(int i) {
	CFont **slots[7] = {
		&g_FontTerminal8, &g_FontTerminal8Fixed, &g_FontBigNumbers,
		&g_FontScrapBig, &g_FontScrapMedium, &g_FontHoratio,
		&g_FontHoratioSmall,
	};
	return slots[i];
}

byte Font_Build(const char *name, int size) {
	CFont *f = operator_new(sizeof(CFont));
	f = f ? CFont_ctor(f, NULL, NULL) : NULL;
	CFont_Build(f, name, size);
	if (f) {
		CFont_dtor(f);
		operator_delete_sized(f, sizeof(CFont));
	}
	return 1;
}

void Fonts_Unload(void) {
	CUnifont_Free(&g_Unifont);
	for (int i = 0; i < 7; i++) {
		CFont **slot = Font_Slot(i);
		if (*slot) {
			CFont_dtor(*slot);
			operator_delete_sized(*slot, sizeof(CFont));
			*slot = NULL;
		}
	}
}

CFont *Font_Get(const char *name) {
	for (int i = 0; i < 7; i++) {
		if (!crt_stricmp(name, fontNames[i])) {
			return *Font_Slot(i);
		}
	}
	return NULL;
}

void Fonts_Load(void) {
	CIniFile ini;
	char path[128];

	CIniFile_ctor(&ini);
	snprintf(path, sizeof(path), "%sFonts.fnt", g_FontsDir);
	if (CIniFile_Load(&ini, path, NULL, 0)) {
		Con_FatalErrorf("CFont: File not found: '%s'", path);
	}
	for (int i = 0; i < 7; i++) {
		CFont **slot = Font_Slot(i);
		if (*slot) {
			CheckFailed("(*fnt) == NULL", __FILE__, __TIMESTAMP__, __LINE__);
		}
		CFont *f = operator_new(sizeof(CFont));
		*slot = f ? CFont_ctor(f, NULL, NULL) : NULL;
		CFont_Load(*slot, fontNames[i], NULL, &ini);
	}
	CUnifont_Load(&g_Unifont);
	CIniFile_dtor(&ini);
}

byte Fonts_Unifont(void) {
	return g_Unifont.graphic && g_Unifont.sprites;
}

/* the language of the fonts: nothing to do in this version */
void Font_SetLanguage(const char *lang) {
	(void)lang;
}
