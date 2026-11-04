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
#include "scrap/app.h"
#include "scrap/global.h"
#include "scrap/scoreritems.h"
#include "scrap/scrap.h"
#include "scrap/staticinit.h"

#define CTEXTTYPINGSCORER_VFTABLE (&cTextTypingScorer_vftable.base)
#define CTEXTSCORER_VFTABLE (&cTextScorer_vftable.base)
#define g_TypingSoundName (GVar_TextTypingSound.value.s)	/* "keyboard%d" for the key sounds */
#define g_TypingSoundVol (GVar_TextTypingSoundVol.value.f)
static const cTextScorerVtbl cTextTypingScorer_vftable;
static const cTextScorerVtbl cTextScorer_vftable;
static cGVar GVar_TextTypingSoundVol;
static cGVar GVar_TextTypingSound;

/* the OnAccept of the item is the native edit start */
static void TextEdit_Start(cAbstractFile *params);
static byte Text_IsEdit(cTextScorer *self) {
	cPy_Function *f = &self->base.base.onAccept;
	void *func = f->flags & PYF_NATIVE ? f->func : NULL;
	return func == (void *)TextEdit_Start;
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

cTextScorer *cTextScorer_ctor(cTextScorer *self) {
	cAbstractTextScorer_ctor(&self->base);
	self->base.base.vftable = CTEXTSCORER_VFTABLE;
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

cTextTypingScorer *cTextTypingScorer_ctor(cTextTypingScorer *self) {
	cTextScorer_ctor(&self->base);
	self->base.base.base.vftable = CTEXTTYPINGSCORER_VFTABLE;
	self->typingTime = 0.0f;
	self->typingSpeed = 0.04f;
	self->lastTime = 0.0f;
	self->cursorAtInit = 1;
	self->cursorAtEnd = 1;
	self->pos = -1;
	self->typingSound = 1;
	return self;
}

/* also the destructor of cTextTypingScorer (the same code) */
static void cTextScorer_dtor(cTextScorer *self) {
	self->base.base.vftable = CTEXTSCORER_VFTABLE;
	ScorerImg_Free(self->img);
	cString_dtor(&self->editHint);
	cAbstractTextScorer_dtor(&self->base);
}

static void cTextScorer_Free(cScorerItem *base) {
	cTextScorer *self = (cTextScorer *)base;
	cTextScorer_dtor(self);
	operator_delete(self);
}

static void cTextTypingScorer_Free(cScorerItem *base) {
	cTextTypingScorer *self = (cTextTypingScorer *)base;
	cTextScorer_dtor(&self->base);
	operator_delete(self);
}

/* the text edit of each player */
static cTextScorer *g_TextEditItem[4];
static cScorerItem *g_TextEditCursor[4];

/* the edit of the text item of player ends: the cursor item back, the hint of the item */
static void TextEdit_End(int player) {
	cTextScorer *item;

	g_TextEditItem[player]->editing = 0;
	Scorer_Get(player)->cursor = g_TextEditCursor[player];
	item = g_TextEditItem[player];
	Scorer_Get(player)->skipFrames = 2;
	g_PlayerInput[player].onDone = NULL;
	cString_Assign(&Scorer_Get(player)->hint, &item->base.base.hint);
}

/* onDone of the player input: the typed text into the item, then TextEdit_End */
static void TextEdit_Done(int player, const char *text) {
	(void)text;
	cString_Set(&g_TextEditItem[player]->base.text, g_PlayerInput[player].text);
	TextEdit_End(player);
}

/* the native OnAccept of an edited text (Input/ActionSet.h): params player, item name; the virtual keyboard
 * starts on the text of the item */
static void TextEdit_Start(cAbstractFile *params) {
	cTextScorer *item;
	cScorerItem *cursor;
	char name[0x80];
	float dim[2];
	int player, max;

	VFile_Read(params, &player, 4);
	cAbstractFile_ReadString(params, name, 0x7f);
	item = (cTextScorer *)cScorer_FindItem(Scorer_Get(player), name);
	cVirtualKeyboard_Set(&g_VirtualKeyboard, item->isNumeric ? "Numbers" : "Console");
	g_TextEditItem[player] = item;
	cPlayerInput_SetString(&g_PlayerInput[player], item->base.text);
	g_TextEditItem[player]->editing = 1;
	g_TextEditCursor[player] = Scorer_Get(player)->cursor;
	Scorer_Get(player)->cursor = NULL;
	/* the exe's function, as the original stores it */
	g_PlayerInput[player].onDone = (void (*)(int, const char *))(void *)TextEdit_Done;
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

/* EditHint, CenterEdit, IsNumeric, MaxInput (the size of that many "W"; 0 above 255), then the text variables */
static int cTextScorer_Var(cScorerItem *base, const char *name, int set, char **s, double *d, int *i) {
	cTextScorer *self = (cTextScorer *)base;
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
	return cAbstractTextScorer_Var(&self->base.base, name, set, s, d, i);
}

/* TypingSpeed, TypingTime (return 3), CursorAtInit, CursorAtEnd, TypingSound, then cTextScorer_Var */
static int cTextTypingScorer_Var(cScorerItem *base, const char *name, int set, char **s, double *d, int *i) {
	cTextTypingScorer *self = (cTextTypingScorer *)base;
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
	return cTextScorer_Var(&self->base.base.base, name, set, s, d, i);
}

#define KEYS_COLOR 0xaee5fb
#define KEYS_FRAME_COLOR 0xc05b6e80
#define KEYS_EDIT_COLOR 0xa4ffffd6
#define KEYS_CURSOR_COLOR 0x32a4a4ff
#define KEYS_KEY_COLOR 0x401e242a
#define KEYS_BLINK 511			/* the cursor blinks every 511/1024 s */
#define KEYS_CENTER 4			/* the align of the keys and the centered text */
#define KEYS_HINT_SIZE 0x80

CPsaImage *g_ScorerTabs;

/* the text being typed with a blinking cursor; with the virtual keyboard active the grid of its keys (the one
 * under the cursor lit), the button hints and the cursor item on the key */
static void cTextScorer_DrawKeyboard(cTextScorer *self, int player) {
	cAbstractTextScorer *t = &self->base;
	cScorerItem *it = &t->base;
	cPlayerInput *in = &g_PlayerInput[player];
	CPsaImage *tabs = g_ScorerTabs;
	char buf[0x300], hint[0x100], key[2];

	int v = (int)(Scorer_Get(player)->time * 1024.0f) % KEYS_BLINK - 255;
	int blink = v < 0 ? -v : v;
	if (in->active) {
		float cell = self->cellSize;
		float half = (it->w - (float)g_VirtualKeyboard.a * cell) * 0.5f;
		float w = (float)g_VirtualKeyboard.a * self->cellSize + 6.0f;
		float h = (float)g_VirtualKeyboard.b * self->cellSize + 4.0f;
		tabs->color = KEYS_FRAME_COLOR;
		Menu_DrawFrame(g_ScorerTabs, 1, it->x + half - 3.0f, it->y + (cell + 3.0f) - 2.0f, w, h);
	}
	g_ScorerTabs->color = KEYS_EDIT_COLOR;
	Menu_DrawFrame(g_ScorerTabs, 1, it->x - 3.0f, it->y - 1.0f, it->w + 6.0f, it->h + 2.0f);
	Str_Copy(buf, in->text, sizeof(buf));
	CFont *font = t->font;
	font->effects = 0x20000;
	font->color = KEYS_COLOR;
	if (self->centerEdit) {
		font->align = KEYS_CENTER;
		CFont_PrintfFlags(font, it->w * 0.5f + it->x, it->h * 0.5f + it->y, 0xb, "%s", buf);
	} else {
		font->align = 0;
		CFont_PrintfFlags(font, it->x, it->y, 0xb, "%s", buf);
	}
	if (blink > 128) {
		font->align = 0;
		CFont_PrintfFlags(font, font->x, it->y, 0xb, "_");
	}
	if (in->active) {
		float cell = self->cellSize;
		float ox = (it->w - (float)g_VirtualKeyboard.a * cell) * 0.5f;
		float oy = cell + 3.0f;
		for (int col = 0; col < g_VirtualKeyboard.a; col++) {
			for (int row = 0; row < g_VirtualKeyboard.b; row++) {
				key[0] = g_VirtualKeyboard.text[g_VirtualKeyboard.a * row + col];
				key[1] = 0;
				cell = self->cellSize;
				if (cPlayerInput_GetCursorChar(in) == key[0]) {
					CPsaImage *img = self->img;
					img->scaleX = cell;
					img->scaleY = cell;
					img->color = KEYS_CURSOR_COLOR;
					CPsaImage_Draw(img, (float)col * cell + it->x + ox, (float)row * cell + (oy + it->y));
					g_ScorerTabs->color = (uint32_t)(blink / 2) << 24 | 0x52527f;
					cell = self->cellSize;
					Menu_DrawFrame(g_ScorerTabs, 1, cell * (float)col + it->x + ox - 2.0f,
						cell * (float)row + (oy + it->y) - 2.0f, cell + 4.0f, cell + 4.0f);
					font->align = KEYS_CENTER;
					font->color = (((uint32_t)blink | 0xffffff00) << 8 | (uint32_t)blink) << 8
						| (uint32_t)(int)((float)blink * 0.8f);
					font->effects = 0x40000;
					cell = self->cellSize;
					CFont_PrintfFlags(font, cell * (float)col + it->x + cell * 0.5f + ox,
						cell * (float)row + (oy + it->y) + cell * 0.5f, 0xb, "%s", key);
				} else {
					g_ScorerTabs->color = KEYS_KEY_COLOR;
					Menu_DrawFrame(g_ScorerTabs, 1, cell * (float)col + it->x + ox - 2.0f,
						cell * (float)row + (oy + it->y) - 2.0f, cell + 4.0f, cell + 4.0f);
					cell = self->cellSize;
					CFont_PrintfAlignColor(font, cell * (float)col + it->x + cell * 0.5f + ox,
						cell * (float)row + (oy + it->y) + cell * 0.5f, KEYS_CENTER, KEYS_COLOR, "%s", key);
				}
			}
		}
		g_ScorerTabs->color = KEYS_FRAME_COLOR;
		Menu_DrawFrame(g_ScorerTabs, 1, 60.0f, 415.0f, 520.0f, 34.0f);
		/* the button glyphs of the font */
		int n = Str_PrintfQuiet(hint, KEYS_HINT_SIZE, Lang_GetStr("XBox_Text_Input_Hint_0"), "\x14", "\x15");
		if (n > 0 && !hint[n]) {
			hint[n] = ' ';
		}
		Str_PrintfQuiet(hint + 1 + n, KEYS_HINT_SIZE, Lang_GetStr("XBox_Text_Input_Hint_1"), "\x16", "\x17");
		CFont_PrintfAlignColorShadow(font, 320.0f, 430.0f, KEYS_CENTER, KEYS_COLOR, "%s", hint);
		cScorerItem *c = g_TextEditCursor[player];
		c->x = in->cursorX * self->cellSize + it->x;
		c->y = it->y + (in->cursorY + 1.0f) * self->cellSize;
	}
	if (cInputAction_Pressed(InputActionTable_At(&ActionSetTable_At(&g_ActionSets, 0)->actions, 0), player)) {
		TextEdit_End(player);
	}
}

static void cTextScorer_Draw(cScorerItem *base, int player) {
	cTextScorer *self = (cTextScorer *)base;
	cAbstractTextScorer *t = &self->base;
	char buf[0x200];
	cScorer *sc;

	if (self->editing) {
		TextScorer_DrawKeyboard(self, player);
		return;
	}
	Text_Get(self, player, buf, sizeof(buf));
	sc = Scorer_Get(player);
	if (!cString_ICmp(&t->base.name, &sc->current)) {
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
			t->font->effects = TEXTSCORER_OUTLINE;
			CFont_PrintfFlags(t->font, t->base.x, t->base.y, 0xb, "%s", buf);
			return;
		}
		g_ScorerTabs->color = (((uint32_t)v | 0x4000) << 8 | (uint32_t)v) << 8 | (uint32_t)v;
		Menu_DrawFrame(g_ScorerTabs, 1, t->base.x - 3.0f, t->base.y - 1.0f, t->base.w + 6.0f, t->base.h + 2.0f);
	} else {
		t->font->align = (uint32_t)t->align;
		t->font->color = t->color;
		t->font->effects = (uint32_t)t->effect;
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

static void cTextTypingScorer_Draw(cScorerItem *base, int player) {
	cTextTypingScorer *self = (cTextTypingScorer *)base;
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
		SoundSys_Play(g_pSound, snd, Snd_Volume(g_TypingSoundVol), 0);
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
	t->font->effects = (uint32_t)t->effect;
	CFont_PrintfFlags(t->font, t->base.x, t->base.y, 0xb, "%s", buf);
}

/* the vtable of cTextScorer */
static const cTextScorerVtbl cTextScorer_vftable = {
	.base.Draw = cTextScorer_Draw,
	.base.SetParam = cScorerItem_SetParam,
	.base.Var = cTextScorer_Var,
	.base.Free = cTextScorer_Free,
	.DrawKeyboard = cTextScorer_DrawKeyboard,
};

/* the vtable of cTextTypingScorer */
static const cTextScorerVtbl cTextTypingScorer_vftable = {
	.base.Draw = cTextTypingScorer_Draw,
	.base.SetParam = cScorerItem_SetParam,
	.base.Var = cTextTypingScorer_Var,
	.base.Free = cTextTypingScorer_Free,
	.DrawKeyboard = cTextScorer_DrawKeyboard,
};

static cGVar GVar_TextTypingSound;
static cGVar GVar_TextTypingSoundVol;
static cPy_NativeFunc NativeFunc_EditText;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
FINI_GVAR_STRING(Fini_GVar_TextTypingSound, &GVar_TextTypingSound)
INIT_GVAR_STRING(Init_GVar_TextTypingSound, Fini_GVar_TextTypingSound, &GVar_TextTypingSound, "TextTypingSound", 0, "Set the Text Typing Sound", "keyboard%d")
INIT_GVAR_FLOAT(Init_GVar_TextTypingSoundVol, &GVar_TextTypingSoundVol, "TextTypingSoundVol", 0, "Set the Text Typing Sound Volume", 1.0f)
INIT_NATIVE_FUNC(Init_NativeFunc_EditText, &NativeFunc_EditText, "EditText", TextEdit_Start)
