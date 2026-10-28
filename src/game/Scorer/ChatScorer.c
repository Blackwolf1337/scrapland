/*
 * ChatScorer.cpp of the game (Menu/): cChatScorer and the chat lines of the scripts and the net. See
 * include/scrap/scoreritems.h.
 */
#include <string.h>

#include "scrap/scoreritems.h"

cChatScorer *__fastcall cChatScorer_ctor(cChatScorer *self) {
	cScorerItem_ctor(&self->base);
	self->base.vftable = CCHATSCORER_VFTABLE;
	CPsaImage *img = operator_new(sizeof(CPsaImage));
	self->img = img ? CPsaImage_ctor(img, "2D/white.tga", 0x24) : NULL;
	self->base.x = 200.0f;
	self->base.y = 0.0f;
	self->font = g_FontHoratioSmall;
	self->lineHeight = 14.0f;
	self->numMsg = CHAT_MSGS;
	g_ChatTime = Timer_GetTime(&g_pWorld->timer);
	return self;
}

void __fastcall cChatScorer_dtor(cChatScorer *self) {
	CPsaImage *img = self->img;
	self->base.vftable = CCHATSCORER_VFTABLE;
	if (img) {
		CPsaImage_dtor(img);
		operator_delete_sized(img, sizeof(CPsaImage));
	}
	cScorerItem_dtor(&self->base);
}

cChatScorer *__thiscall cChatScorer_Delete(cChatScorer *self, byte flags) {
	cChatScorer_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(cChatScorer));
	}
	return self;
}

/* the static initializer (rva 0x051be0): the lines empty and white */
void ChatScorer_InitMsgs(void) {
	for (int i = 0; i < CHAT_MSGS; i++) {
		g_ChatMsgs[i].text[0] = 0;
		g_ChatMsgs[i].color = 0xffffff;
	}
}

/* the lines one up, text the newest */
static void Chat_Scroll(const char *text) {
	g_ChatTime = Timer_GetTime(&g_pWorld->timer);
	memmove(&g_ChatMsgs[0], &g_ChatMsgs[1], (CHAT_MSGS - 1) * sizeof(cChatMsg));
	Str_Copy(g_ChatMsgs[CHAT_MSGS - 1].text, text, sizeof(g_ChatMsgs[0].text));
}

void Scorer_AddChatMsg(const char *text, uint32_t rgb) {
	Chat_Scroll(text);
	g_ChatMsgs[CHAT_MSGS - 1].color = rgb;
}

int __thiscall cChatScorer_Var(cChatScorer *self, const char *name, byte set, char **s, double *d, int *i) {
	if (i && !crt_stricmp(name, "NumMsg")) {
		return ScorerVar_Int(&self->numMsg, set, i);
	}
	return cScorerItem_Var(&self->base, name, set, s, d, i);
}

void __thiscall cChatScorer_Draw(cChatScorer *self, int player) {
	(void)player;
	float dt = Timer_GetTime(&g_pWorld->timer) - g_ChatTime;
	if (dt > g_ChatScrollTime) {
		Chat_Scroll("");
		g_ChatMsgs[CHAT_MSGS - 1].color = 0;
		g_ChatTime = g_ChatTime + dt;
	}
	if (self->numMsg > CHAT_MSGS) {
		self->numMsg = CHAT_MSGS;
	} else if (self->numMsg < 0) {
		self->numMsg = 0;
	}
	for (int k = CHAT_MSGS - self->numMsg; k < CHAT_MSGS; k++) {
		const cChatMsg *m = &g_ChatMsgs[k];
		CFont_PrintfColorSombra(self->font, self->base.x, (float)(self->numMsg - CHAT_MSGS + k) * self->lineHeight
			+ self->base.y, m->color, "%s", m->text);
	}
}
