/*
 * ChatScorer.cpp of the game (Menu/): cChatScorer and the chat lines of the scripts and the net. See
 * include/scrap/scoreritems.h.
 */
#include <string.h>

#include "scrap/scoreritems.h"
#include "scrap/staticinit.h"

float g_ChatTime;

cChatScorer *cChatScorer_ctor(cChatScorer *self) {
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

void cChatScorer_dtor(cChatScorer *self) {
	CPsaImage *img = self->img;
	self->base.vftable = CCHATSCORER_VFTABLE;
	if (img) {
		CPsaImage_dtor(img);
		operator_delete_sized(img, sizeof(CPsaImage));
	}
	cScorerItem_dtor(&self->base);
}

cScorerItem *cChatScorer_Delete(cScorerItem *base, byte flags) {
	cChatScorer *self = (cChatScorer *)base;
	cChatScorer_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(cChatScorer));
	}
	return base;
}

cChatMsg g_ChatMsgs[16];

/* the static initializer: the lines empty and white */
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

int cChatScorer_Var(cScorerItem *base, const char *name, int set, char **s, double *d, int *i) {
	cChatScorer *self = (cChatScorer *)base;
	if (i && !crt_stricmp(name, "NumMsg")) {
		return ScorerVar_Int(&self->numMsg, set, i);
	}
	return cScorerItem_Var(&self->base, name, set, s, d, i);
}

void cChatScorer_Draw(cScorerItem *base, int player) {
	cChatScorer *self = (cChatScorer *)base;
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
		CFont_PrintfColorShadow(self->font, self->base.x, (float)(self->numMsg - CHAT_MSGS + k) * self->lineHeight
			+ self->base.y, m->color, "%s", m->text);
	}
}

/* the vtable of cChatScorer */
const cScorerItemVtbl cChatScorer_vftable = {
	.Draw = cChatScorer_Draw,
	.SetParam = cScorerItem_SetParam,
	.Var = cChatScorer_Var,
	.Delete = cChatScorer_Delete,
};

cGVar GVar_ChatMsgPersistence;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_FLOAT(Init_GVar_ChatMsgPersistence, &GVar_ChatMsgPersistence, "ChatMsgPersistence", 2, "Time that will be printed the chat message", 1.0f)
