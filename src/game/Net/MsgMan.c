/*
 * Net/msgman.cpp of the game: cNetClient, the reliable message queue and the state of one connection, and the base
 * class of the user managers (cUserNetManager). See include/scrap/net.h.
 */
#include <string.h>

#include "scrap/console.h"
#include "scrap/net.h"

uint32_t g_NetWaitTime = 5000;

uint32_t g_NetCloseTime = 2000;

/* the time out of the state, the same code is inlined in Send */
static int NetClient_CheckTime(cNetClient *self, uint32_t time) {
	uint32_t d = time - self->lastRecv;

	if (self->state == NETCLIENT_DISCONNECTED) {
		if (d > g_NetCloseTime) {
			self->state = NETCLIENT_TIMEOUT;
		}
	} else if (g_NetTimeOut >= 0 && (uint32_t)g_NetTimeOut < d) {
		self->state = NETCLIENT_TIMEOUT;
	} else if (d > g_NetWaitTime) {
		self->state = NETCLIENT_WAITING;
	}
	return self->state;
}

static void NetClient_PopHead(cNetClient *self) {
	cNetMsgNode *n = self->head;

	self->head = n->next;
	if (!self->head) {
		self->tail = NULL;
	}
	Mem_Free(n->data);
	Mem_Free(n);
}

netuserfactory_t g_NetUserFactory;

cNetClient *cNetClient_ctor(cNetClient *self, int slot) {
	netuserfactory_t f = g_NetUserFactory;

	self->head = NULL;
	self->tail = NULL;
	self->seqOut = 0;
	self->seqIn = 0x7f;
	self->lastRecv = 0;
	self->state = NETCLIENT_CONNECTING;
	self->pingStart = 0;
	self->ping = 0;
	self->toggle = 0;
	self->lastSend = 0;
	self->received = 0;
	self->user = f ? f(self, slot) : NULL;
	return self;
}

cUserNetManager *cUserNetManager_ctor(cUserNetManager *self, cNetClient *client, int slot) {
	self->client = client;
	self->slot = slot;
	self->vftable = g_UserNetManagerVftable;
	return self;
}

void cNetClient_dtor(cNetClient *self) {
	if (self->user) {
		self->user->vftable->Delete(self->user, 1);
	}
	while (self->head) {
		NetClient_PopHead(self);
	}
}

void cUserNetManager_dtor(cUserNetManager *self) {
	self->vftable = g_UserNetManagerVftable;
	self->client->user = NULL;
}

cUserNetManager *cUserNetManager_Delete(cUserNetManager *self, byte flags) {
	cUserNetManager_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

cNetMsgNode *cNetClient_NewMsg(cNetClient *self, const void *data, uint16_t size) {
	cNetMsgNode *n = Mem_Alloc(sizeof(*n));

	n->size = size + 1;
	n->data = Mem_Alloc(size + 1);
	n->next = NULL;
	memcpy(n->data + 1, data, size);
	n->data[0] = self->seqOut & 0x7f;
	self->seqOut++;
	return n;
}

void cNetClient_Push(cNetClient *self, const void *data, uint16_t size) {
	cNetMsgNode *n = cNetClient_NewMsg(self, data, size);

	if (!self->head) {
		self->head = n;
		self->tail = n;
		return;
	}
	self->tail->next = n;
	self->tail = n;
}

int cNetClient_UpdateState(cNetClient *self, uint32_t time) {
	return NetClient_CheckTime(self, time);
}

uint32_t g_NetSendDelay = 100;

int cNetClient_Flush(cNetClient *self, uint32_t time, int slot) {
	if (time > self->lastSend + g_NetSendDelay) {
		cNetClient_Send(self, time, slot, 0, NULL);
	}
	return self->state != NETCLIENT_TIMEOUT;
}

void cNetClient_RequestFull(cNetClient *self) {
	CHECK(Transport_GetMode() == NETMODE_CLIENT);
	/* data[0] is the sequence number, not the message kind: the test of the original */
	for (cNetMsgNode *n = self->head; n; n = n->next) {
		if (n->data[0] == NETMSG_USR_DATA) {
			return;
		}
	}
	byte msg = NETMSG_USR_DATA;
	cNetClient_Push(self, &msg, 1);
	Con_Print(0x606060, "I need full data brother!\n", 0);
}

void cNetClient_GetSeq(cNetClient *self, int *head, int *in) {
	*head = self->head ? self->head->data[0] : -1;
	*in = self->seqIn & 0x7f;
}

uint32_t cNetClient_TimeToSend(cNetClient *self, uint32_t time) {
	uint32_t d = time - self->lastSend;

	return d < g_NetSendDelay ? g_NetSendDelay - d : 0;
}

void cNetClient_Goodbye(cNetClient *self) {
	Transport_GetMode();
	byte msg = NETMSG_GOODBYE;
	self->state = NETCLIENT_DISCONNECTED;
	cNetClient_Push(self, &msg, 1);
}

int cNetClient_ClientRecv(cNetClient *self, uint32_t time, int *slot, void *data, int *dataSize, void *ctl,
	int *ctlSize) {
	int len = *ctlSize + (*dataSize + 8);
	byte *buf = cStaticBuffer_Get(g_PyStaticBuffer, len);

	*slot = Transport_Recv(buf, &len);
	if (*slot != -1 && len >= 3) {
		cNetClient_Receive(self, time, buf, len, data, dataSize, ctl, ctlSize);
		cStaticBuffer_Unlock(g_PyStaticBuffer);
		return 1;
	}
	cStaticBuffer_Unlock(g_PyStaticBuffer);
	return 0;
}

uint32_t cNetClient_Send(cNetClient *self, uint32_t time, int slot, int size, const void *data) {
	uint16_t len = (uint16_t)(size + 3);
	int sent;

	self->lastSend = time;
	self->received = 0;
	if (!self->pingStart) {
		self->pingStart = time;
	}
	if (!self->lastRecv) {
		self->lastRecv = time;
	}
	NetClient_CheckTime(self, time);
	if (self->head) {
		len += self->head->size;
	}
	byte *p = cStaticBuffer_Get(g_PyStaticBuffer, len);
	p[0] = self->toggle ? (self->seqIn | 0x80) : (self->seqIn & 0x7f);
	if (self->head) {
		memcpy(p + 1, &self->head->size, 2);
		memcpy(p + 3, self->head->data, self->head->size);
		memcpy(p + self->head->size + 3, data, size);
	} else {
		p[1] = 0;
		p[2] = 0;
		memcpy(p + 3, data, size);
	}
	switch (Transport_GetMode()) {
	case NETMODE_SERVER:
		sent = slot >= 0 ? Transport_SendToClient(p, len, slot) : 0;
		break;
	case NETMODE_CLIENT:
		Transport_SendToServer(p, len);
		sent = 1;
		break;
	default:
		sent = 0;
		break;
	}
	cStaticBuffer_Unlock(g_PyStaticBuffer);
	return sent ? len : 0;
}

int cNetClient_Receive(cNetClient *self, uint32_t time, const byte *pkt, int len, void *data, int *dataSize,
	void *ctl, int *ctlSize) {
	/* an answer flips the toggle: the time since it flipped is the ping */
	if ((Transport_GetMode() == NETMODE_SERVER && (pkt[0] >> 7) == self->toggle) ||
		(Transport_GetMode() == NETMODE_CLIENT && (pkt[0] >> 7) != self->toggle)) {
		self->ping = self->pingStart ? (int)(time - self->pingStart) : 0;
		self->pingStart = 0;
		self->toggle = self->toggle == 0;
	}
	self->lastRecv = time;
	self->received = 1;
	if (self->state == NETCLIENT_WAITING || self->state == NETCLIENT_CONNECTING) {
		self->state = NETCLIENT_CONNECTED;
	}
	/* the other side has the first queued message */
	if (self->head && !((self->head->data[0] ^ pkt[0]) & 0x7f)) {
		NetClient_PopHead(self);
	}
	uint32_t n = (uint32_t)pkt[1] | (uint32_t)pkt[2] << 8;
	if (n > 1 && ((self->seqIn + 1) & 0x7f) == (pkt[3] & 0x7f)) {
		self->seqIn = pkt[3] & 0x7f;
		if (*ctlSize > (int)(n - 1)) {
			*ctlSize = n - 1;
		}
		memcpy(ctl, pkt + 4, *ctlSize);
		Transport_GetMode();
		if (pkt[4] == NETMSG_GOODBYE) {
			self->state = NETCLIENT_TIMEOUT;
		}
	} else {
		*ctlSize = 0;
	}
	if (*dataSize > (int)(len - n - 3)) {
		*dataSize = len - n - 3;
	}
	memcpy(data, pkt + 3 + n, *dataSize);
	if (self->user) {
		self->user->vftable->OnReceive(self->user, data, *dataSize, ctl, *ctlSize);
	}
	return 1;
}

void MsgMan_SetCloseTime(uint32_t ms) {
	g_NetCloseTime = ms;
}

void MsgMan_SetUserFactory(netuserfactory_t f) {
	g_NetUserFactory = f;
}

void MsgMan_SetSendDelay(uint32_t ms) {
	g_NetSendDelay = ms;
}

void MsgMan_SetWaitTime(uint32_t ms) {
	g_NetWaitTime = ms;
}

/* the vtable of cUserNetManager */
const cUserNetManagerVtbl cUserNetManager_vftable = {
	.Delete = cUserNetManager_Delete,
	.OnReceive = (void *)crt_purecall,
	.Send = (void *)crt_purecall,
	.SetLocalData = cUserNetManager_Nop,
};

cGVar GVar_NetTimeOut;

