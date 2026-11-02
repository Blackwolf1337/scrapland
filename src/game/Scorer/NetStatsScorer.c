/*
 * NetStatsScorer.cpp of the game (Menu/): cNetStatsScorer, the ping and the traffic bars of a net client. See
 * include/scrap/scoreritems.h.
 */
#include <string.h>

#include "scrap/net.h"
#include "scrap/scoreritems.h"

cNetStatsScorer *cNetStatsScorer_ctor(cNetStatsScorer *self) {
	cScorerItem_ctor(&self->base);
	self->base.vftable = CNETSTATSSCORER_VFTABLE;
	CPsaImage *img = operator_new(sizeof(CPsaImage));
	self->img = img ? CPsaImage_ctor(img, "2D/white.tga", 0x24) : NULL;
	memset(self->sent, 0, 6 * NETSTATS_SLOTS * 4);
	return self;
}

void cNetStatsScorer_dtor(cNetStatsScorer *self) {
	CPsaImage *img = self->img;
	self->base.vftable = CNETSTATSSCORER_VFTABLE;
	if (img) {
		CPsaImage_dtor(img);
		operator_delete_sized(img, sizeof(CPsaImage));
	}
	cScorerItem_dtor(&self->base);
}

cScorerItem *cNetStatsScorer_Delete(cScorerItem *base, byte flags) {
	cNetStatsScorer *self = (cNetStatsScorer *)base;
	cNetStatsScorer_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(cNetStatsScorer));
	}
	return base;
}

int cNetStatsScorer_Var(cScorerItem *base, const char *name, int set, char **s, double *d, int *i) {
	cNetStatsScorer *self = (cNetStatsScorer *)base;
	return cScorerItem_Var(&self->base, name, set, s, d, i);
}

int g_NetStatSentSlot;

int g_NetStatSent;

void cNetStatsScorer_Draw(cScorerItem *base, int player) {
	cNetStatsScorer *self = (cNetStatsScorer *)base;
	cScorerItem *it = &self->base;

	if ((g_pApp->flags & APPF_CLIENT) && g_ShowClientPing && Net_GetClient()) {
		CFont_PrintfShadow(g_FontTerminal8, g_ClientPingX, g_ClientPingY, "Ping : %04d", Net_GetClient()->ping);
	}
	if (!g_ShowNetStatus) {
		return;
	}
	float dec = Scorer_Get(player)->frameTime * 5.0f;
	for (int k = 0; k < NETSTATS_SLOTS; k++) {
		CPsaImage *img;
		float a = self->recv[k];
		if (a > 0.0f) {
			img = self->img;
			img->scaleY = a;
			img->scaleX = 8.0f;
			self->img->color = 0xff0000ff;
			CPsaImage_Draw(self->img, (float)k * 8.0f + it->x, it->y);
		}
		float b = self->sent[k];
		if (b > 0.0f) {
			img = self->img;
			img->scaleY = b;
			img->scaleX = 8.0f;
			self->img->color = 0xff00ff00;
			CPsaImage_Draw(self->img, (float)k * 8.0f + it->x, it->y - self->sent[k]);
		}
		float v = (self->end[k] - self->start[k]) * 100.0f;
		if (v > 50.0f) {
			self->img->color = 0xffff0000;
			v = 50.0f;
		} else {
			self->img->color = 0xff800000;
		}
		img = self->img;
		img->scaleX = 4.0f;
		img->scaleY = v + v;
		float kx = (float)k * 8.0f;
		CPsaImage_Draw(self->img, (it->x + kx) + 2.0f, it->y - v);
		a = self->recv[k] - dec;
		b = self->sent[k] - dec;
		self->recv[k] = a;
		self->sent[k] = b;
		if (0.0f > a) {
			self->recv[k] = 0.0f;
		}
		if (0.0f > b) {
			self->sent[k] = 0.0f;
		}
		if (self->recv[k] > 0.0f && self->sent[k] > 0.0f) {
			float yo = (float)(k + 1) * 10.0f;
			CFont_PrintfShadow(g_FontTerminal8, (it->x + kx) + 4.0f, it->y + yo, "-%04d", self->recvBytes[k]);
			CFont_PrintfShadow(g_FontTerminal8, (it->x + kx) + 4.0f, it->y - yo, "-%04d", self->sentBytes[k]);
		}
	}
	if (g_NetStatSent > 0) {
		int slot = g_NetStatSentSlot;
		if (slot < NETSTATS_SLOTS) {
			self->sentBytes[slot] = g_NetStatSent;
			self->sent[g_NetStatSentSlot] = (float)g_NetStatSent * 0.25f;
			slot = g_NetStatSentSlot;
			self->start[slot] = self->end[slot];
			float t = (float)g_pApp->vftable->GetGameTime(g_pApp);
			self->end[g_NetStatSentSlot] = t;
		}
		g_NetStatSent = 0;
	}
	if (g_NetStatRecv > 0) {
		if (g_NetStatSentSlot < NETSTATS_SLOTS) {
			self->recv[g_NetStatRecvSlot] = (float)g_NetStatRecv * 0.25f;
			self->recvBytes[g_NetStatRecvSlot] = g_NetStatRecv;
		}
		g_NetStatRecv = 0;
	}
}

/* the vtable of cNetStatsScorer */
const cScorerItemVtbl cNetStatsScorer_vftable = {
	.Draw = cNetStatsScorer_Draw,
	.SetParam = cScorerItem_SetParam,
	.Var = cNetStatsScorer_Var,
	.Delete = cNetStatsScorer_Delete,
};

cGVar GVar_ShowNetStatus;

cGVar GVar_ShowClientPing;

cGVar GVar_ClientPingY;

cGVar GVar_ClientPingX;

