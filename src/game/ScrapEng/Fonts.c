/*
 * Fonts.cpp of the game: the seven fonts of Fonts.fnt, see include/scrap/scrapfont.h.
 */
#include <stdio.h>

#include "scrap/console.h"
#include "scrap/crt.h"
#include "scrap/e3dutil.h"
#include "scrap/scrapfont.h"

static const struct {
	const char *name;
	uint32_t rva;
} fontTab[7] = {
	{ "Terminal8", 0x4a8908 },
	{ "Terminal8Fixed", 0x4a890c },
	{ "BigNumbers", 0x4a8910 },
	{ "ScrapBig", 0x4a8914 },
	{ "ScrapMedium", 0x4a8918 },
	{ "Horatio", 0x4a891c },
	{ "HoratioSmall", 0x4a8920 },
};

static CFont **Font_Slot(int i) {
	return (CFont **)Game_Ptr(fontTab[i].rva);
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
		if (!crt_stricmp(name, fontTab[i].name)) {
			return *Font_Slot(i);
		}
	}
	return NULL;
}

void Fonts_Load(void) {
	CArchINI ini;
	char path[128];

	CArchINI_ctor(&ini);
	snprintf(path, sizeof(path), "%sFonts.fnt", g_FontsDir);
	if (CArchINI_Load(&ini, path, NULL, 0)) {
		Con_FatalErrorf("CFont: File not found: '%s'", path);
	}
	for (int i = 0; i < 7; i++) {
		CFont **slot = Font_Slot(i);
		if (*slot) {
			CheckFailed("(*fnt) == NULL", __FILE__, __TIMESTAMP__, __LINE__);
		}
		CFont *f = operator_new(sizeof(CFont));
		*slot = f ? CFont_ctor(f, NULL, NULL) : NULL;
		CFont_Load(*slot, fontTab[i].name, NULL, &ini);
	}
	CUnifont_Load(&g_Unifont);
	CArchINI_dtor(&ini);
}

byte Fonts_Unifont(void) {
	return g_Unifont.grf && g_Unifont.sprites;
}
