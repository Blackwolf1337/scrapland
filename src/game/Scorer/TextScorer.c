/*
 * TextScorer.cpp of the game (Menu/): cTextScorer, a text the player can edit with the virtual keyboard, and
 * cTextTypingScorer, a text typed in letter by letter. With the edit start of Input/ActionSet.h inlined here. See
 * include/scrap/scorerhud.h.
 */
#include <string.h>

#include "scrap/e3dutil.h"
#include "scrap/input.h"
#include "scrap/scorerhud.h"
#include "scrap/sound.h"

/* the OnAccept of the item is the native edit start */
static byte Text_IsEdit(cTextScorer *self) {
	cPy_Function *f = &self->base.base.onAccept;
	void *func = f->flags & PYF_NATIVE ? f->func : NULL;
	return func == Game_Ptr(TEXTEDIT_NATIVE_FUNC);
}

/* the text of the item, a camera variable of the player when it starts with a line break */
static void Text_Get(cTextScorer *self, int player, char *buf, int size) {
	const char *src = self->base.text;

	buf[0] = 0;
	if (src && *src == '\n') {
		Scorer_GetCamVarStr(player, buf, src + 1, size);
	} else {
		Str_Copy(buf, src, size);
	}
}

cTextScorer *__fastcall cTextScorer_ctor(cTextScorer *self) {
	cAbstractTextScorer_ctor(&self->base);
	self->base.base.vftable = Game_Ptr(CTEXTSCORER_VFTABLE);
	cString_ctor(&self->editHint);
	CPsaImage *img = operator_new(sizeof(CPsaImage));
	self->img = img ? CPsaImage_ctorTex(img, "2D/white.tga", 0, 0x24) : NULL;
	self->cellSize = 26.0f;
	self->editing = 0;
	self->centerEdit = 0;
	self->maxInput = 0x10;
	self->isNumeric = 0;
	return self;
}

cTextTypingScorer *__fastcall cTextTypingScorer_ctor(cTextTypingScorer *self) {
	cTextScorer_ctor(&self->base);
	self->base.base.base.vftable = Game_Ptr(CTEXTTYPINGSCORER_VFTABLE);
	self->typingTime = 0.0f;
	self->typingSpeed = 0.04f;
	self->lastTime = 0.0f;
	self->cursorAtInit = 1;
	self->cursorAtEnd = 1;
	self->pos = -1;
	self->typingSound = 1;
	return self;
}

/* also the destructor of cTextTypingScorer (rva 0x182bf0, the same code) */
void __fastcall cTextScorer_dtor(cTextScorer *self) {
	self->base.base.vftable = Game_Ptr(CTEXTSCORER_VFTABLE);
	ScorerImg_Free(self->img);
	cString_dtor(&self->editHint);
	cAbstractTextScorer_dtor(&self->base);
}

cTextScorer *__thiscall cTextScorer_Delete(cTextScorer *self, byte flags) {
	cTextScorer_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(cTextScorer));
	}
	return self;
}

cTextTypingScorer *__thiscall cTextTypingScorer_Delete(cTextTypingScorer *self, byte flags) {
	cTextScorer_dtor(&self->base);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(cTextTypingScorer));
	}
	return self;
}

void TextEdit_End(int player) {
	cTextScorer *item;

	g_TextEditItem[player]->editing = 0;
	Scorer_Get(player)->cursor = g_TextEditCursor[player];
	item = g_TextEditItem[player];
	Scorer_Get(player)->skipFrames = 2;
	g_PlayerInput[player].onDone = NULL;
	cString_Assign(&Scorer_Get(player)->hint, &item->base.base.hint);
}

void TextEdit_Done(int player, const char *text) {
	(void)text;
	cString_Set(&g_TextEditItem[player]->base.text, g_PlayerInput[player].text);
	TextEdit_End(player);
}

void TextEdit_Start(cAbstractFile *params) {
	cTextScorer *item;
	cScorerItem *cursor;
	char name[0x80];
	float dim[2];
	int player, max;

	params->vftable->Read(params, &player, 4);
	cAbstractFile_ReadString(params, name, 0x7f);
	item = (cTextScorer *)cScorer_FindItem(Scorer_Get(player), name);
	cVirtualKeyboard_Set(&g_VirtualKeyboard, item->isNumeric ? "Numbers" : "Console");
	g_TextEditItem[player] = item;
	cPlayerInput_SetString(&g_PlayerInput[player], item->base.text);
	g_TextEditItem[player]->editing = 1;
	g_TextEditCursor[player] = Scorer_Get(player)->cursor;
	Scorer_Get(player)->cursor = NULL;
	/* the exe's function, as the original stores it */
	g_PlayerInput[player].onDone = (void (*)(int, const char *))Game_Ptr(0x182d60);
	max = g_TextEditItem[player]->maxInput;
	CHECK(max < 0x80);
	g_PlayerInput[player].maxLen = max;
	cursor = g_TextEditCursor[player];
	if (cursor) {
		cursor->visible = 0;
	}
	Font_TextSize(g_TextEditItem[player]->base.font, NULL, dim, 0.0f, 0.0f, "W");
	g_TextEditItem[player]->cellSize = Float_MaxSS(dim[0], dim[1]);
	cString_Assign(&Scorer_Get(player)->hint, &g_TextEditItem[player]->editHint);
}

int __thiscall cTextScorer_Var(cTextScorer *self, const char *name, byte set, char **s, double *d, int *i) {
	if (s && !crt_stricmp(name, "EditHint")) {
		if (set) {
			cString_Set(&self->editHint, *s);
		} else {
			*s = self->editHint;
		}
		return 1;
	}
	if (i) {
		if (!crt_stricmp(name, "CenterEdit")) {
			return ScorerVar_Bool(&self->centerEdit, set, i);
		}
		if (!crt_stricmp(name, "IsNumeric")) {
			return ScorerVar_Bool(&self->isNumeric, set, i);
		}
		if (!crt_stricmp(name, "MaxInput")) {
			char buf[0x100];
			int n;

			if (!set) {
				*i = self->maxInput;
				return 3;
			}
			n = *i;
			if (n > 0xff) {
				return 0;
			}
			/* the item is as big as n "W" */
			self->maxInput = n;
			for (int k = 0; k < n; k++) {
				buf[k] = 'W';
			}
			if ((uint32_t)n >= sizeof(buf)) {
				crt_report_rangecheckfailure();
			}
			buf[n] = 0;
			Font_TextSize(self->base.font, NULL, &self->base.base.w, self->base.base.x, self->base.base.y, "%s",
				buf);
			self->base.base.w = self->base.base.w + 2.0f;
			self->base.base.h = self->base.base.h + 2.0f;
			return 3;
		}
	}
	return cAbstractTextScorer_Var(&self->base, name, set, s, d, i);
}

int __thiscall cTextTypingScorer_Var(cTextTypingScorer *self, const char *name, byte set, char **s, double *d,
	int *i) {
	if (d) {
		/* the floats return 3 here */
		if (!crt_stricmp(name, "TypingSpeed")) {
			ScorerVar_Float(&self->typingSpeed, set, d);
			return 3;
		}
		if (!crt_stricmp(name, "TypingTime")) {
			if (set) {
				double v = *d;
				self->lastTime = 0.0f;
				self->typingTime = (float)v;
			} else {
				*d = (double)self->typingTime;
			}
			return 3;
		}
	}
	if (i) {
		if (!crt_stricmp(name, "CursorAtInit")) {
			return ScorerVar_Bool(&self->cursorAtInit, set, i);
		}
		if (!crt_stricmp(name, "CursorAtEnd")) {
			return ScorerVar_Bool(&self->cursorAtEnd, set, i);
		}
		if (!crt_stricmp(name, "TypingSound")) {
			return ScorerVar_Bool(&self->typingSound, set, i);
		}
	}
	return cTextScorer_Var(&self->base, name, set, s, d, i);
}

void __thiscall cTextScorer_Draw(cTextScorer *self, int player) {
	cAbstractTextScorer *t = &self->base;
	char buf[0x200];
	cScorer *sc;

	if (self->editing) {
		/* the virtual keyboard (vf04) */
		((void(__thiscall *)(cTextScorer *, int))t->base.vftable->vf04)(self, player);
		return;
	}
	Text_Get(self, player, buf, sizeof(buf));
	sc = Scorer_Get(player);
	if (!cString_ICmp(&t->base.name, &sc->actual)) {
		/* the cursor item blinks */
		int v = Scorer_Abs((int)((double)sc->time * 680.0) % 0xff - 0x7f);
		if (!Text_IsEdit(self)) {
			uint32_t c = t->color;
			int a = v + 0x80;
			uint32_t col = (uint32_t)((int)((c >> 16) & 0xff) * a / 0xff) | (c >> 24) << 8;
			col = col << 8 | (uint32_t)((int)((c >> 8) & 0xff) * a / 0xff);
			col = col << 8 | (uint32_t)((int)(c & 0xff) * a / 0xff);
			t->font->align = (uint32_t)t->align;
			t->font->color = col;
			t->font->efectos = TEXTSCORER_OUTLINE;
			CFont_PrintfFlags(t->font, t->base.x, t->base.y, 0xb, "%s", buf);
			return;
		}
		g_ScorerTabs->color = (((uint32_t)v | 0x4000) << 8 | (uint32_t)v) << 8 | (uint32_t)v;
		Menu_DrawFrame(g_ScorerTabs, 1, t->base.x - 3.0f, t->base.y - 1.0f, t->base.w + 6.0f, t->base.h + 2.0f);
	} else {
		t->font->align = (uint32_t)t->align;
		t->font->color = t->color;
		t->font->efectos = (uint32_t)t->effect;
		if (!Text_IsEdit(self)) {
			CFont_PrintfFlags(t->font, t->base.x, t->base.y, 0xb, "%s", buf);
			return;
		}
	}
	if (self->centerEdit) {
		CFont_PrintfAlignColor(t->font, t->base.w * 0.5f + t->base.x, t->base.h * 0.5f + t->base.y, 4, t->color, "%s",
			buf);
	} else {
		CFont_PrintfAlignColor(t->font, t->base.x, t->base.y, 0, t->color, "%s", buf);
	}
}

void __thiscall cTextTypingScorer_Draw(cTextTypingScorer *self, int player) {
	cAbstractTextScorer *t = &self->base.base;
	char buf[0x280];
	float now, dt, time;
	int prev, len, pos;
	uint32_t n;

	Text_Get(&self->base, player, buf, sizeof(buf));
	now = Scorer_Get(player)->time;
	if (0.0f >= self->lastTime) {
		self->lastTime = now;
	}
	prev = self->pos;
	dt = now - self->lastTime;
	self->lastTime = now;
	time = self->typingTime + dt;
	self->typingTime = time;
	if (0.0f > time) {
		self->pos = 0;
	} else {
		self->pos = (int)(time / self->typingSpeed);
	}
	len = (int)strlen(buf);
	if (self->pos > 0 && Fonts_Unifont()) {
		/* the color codes (\001 and 3 bytes) do not count */
		int k = 0, j;
		do {
			if (buf[k] == 1) {
				k += 4;
				self->pos += 4;
			}
			j = k;
			k++;
		} while (j < self->pos && k < len);
		self->pos = k;
	}
	if (self->pos >= len) {
		self->pos = len;
	}
	pos = self->pos;
	if (self->typingSound && pos > prev && pos % 3 == 0 && buf[pos] != ' ' && g_TypingSoundName &&
		!strcmp(g_TypingSoundName, "keyboard%d")) {
		char snd[16];
		Bprintf(snd, sizeof(snd), g_TypingSoundName, (crt_rand() & 1) + 1);
		g_pSound->vftable->Play(g_pSound, snd, Snd_Volume(g_TypingSoundVol), 0);
	}
	pos = self->pos;
	if (((int)(self->typingTime * 4.0f) & 1) &&
		((pos > 0 && pos < len) || (self->cursorAtInit && pos <= 0) || (self->cursorAtEnd && pos >= len))) {
		buf[pos] = '_';
		n = (uint32_t)pos + 1;
	} else {
		n = (uint32_t)self->pos;
	}
	if (n >= sizeof(buf)) {
		crt_report_rangecheckfailure();
	}
	buf[n] = 0;
	t->font->align = (uint32_t)t->align;
	t->font->color = t->color;
	t->font->efectos = (uint32_t)t->effect;
	CFont_PrintfFlags(t->font, t->base.x, t->base.y, 0xb, "%s", buf);
}
