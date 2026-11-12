/*
 * Net2 server queries: a QUERY (LAN broadcast, the IPv6 group or one address) gets an INFO with the server list
 * entry. Neither is encrypted: they carry nothing secret, and the query is padded to more bytes than the answer.
 * The token is random per host, so stray INFO packets of other browsers are not taken; the time is echoed for the
 * ping.
 */
#include "net2local.h"

static void Net2Lan_WriteString(Net2Writer *w, const char *s, int max) {
	int n = 0;

	while (n < max && s[n]) {
		n++;
	}
	Net2W_U8(w, (uint32_t)n);
	Net2W_Bytes(w, s, n);
}

static int Net2Lan_ReadString(Net2Reader *r, char *out, int max) {
	int n = (int)Net2R_U8(r);
	const uint8_t *p;

	if (n > max || !(p = Net2R_Bytes(r, n))) {
		out[0] = 0;
		return 0;
	}
	for (int i = 0; i < n; i++) {
		out[i] = p[i] < 0x20 || p[i] == 0x7f ? '?' : (char)p[i];
	}
	out[n] = 0;
	return 1;
}

int Net2Lan_WriteInfo(uint8_t *buf, int cap, uint32_t token, uint32_t time, const Net2ServerInfo *info) {
	Net2Writer w;

	Net2W_Init(&w, buf, cap);
	Net2W_U8(&w, N2P_INFO);
	Net2W_U32(&w, N2_PROTOCOL);
	Net2W_U32(&w, token);
	Net2W_U32(&w, time);
	Net2W_U16(&w, info->version);
	Net2W_U32(&w, info->appVersion);
	Net2W_U8(&w, info->players);
	Net2W_U8(&w, info->maxPlayers);
	Net2W_U8(&w, info->flags);
	Net2Lan_WriteString(&w, info->name, NET2_INFO_NAME);
	Net2Lan_WriteString(&w, info->map, NET2_INFO_MAP);
	Net2Lan_WriteString(&w, info->mode, NET2_INFO_MODE);
	return w.overflow ? 0 : w.pos;
}

int Net2Lan_ReadInfo(const uint8_t *pkt, int size, uint32_t *token, uint32_t *time, Net2ServerInfo *info) {
	Net2Reader r;

	memset(info, 0, sizeof(*info));
	Net2R_Init(&r, pkt, size);
	if (Net2R_U8(&r) != N2P_INFO || Net2R_U32(&r) != N2_PROTOCOL) {
		return 0;
	}
	*token = Net2R_U32(&r);
	*time = Net2R_U32(&r);
	info->version = (uint16_t)Net2R_U16(&r);
	info->appVersion = Net2R_U32(&r);
	info->players = (uint8_t)Net2R_U8(&r);
	info->maxPlayers = (uint8_t)Net2R_U8(&r);
	info->flags = (uint8_t)Net2R_U8(&r);
	if (!Net2Lan_ReadString(&r, info->name, NET2_INFO_NAME) || !Net2Lan_ReadString(&r, info->map, NET2_INFO_MAP)
		|| !Net2Lan_ReadString(&r, info->mode, NET2_INFO_MODE)) {
		return 0;
	}
	return !r.bad && Net2R_Left(&r) == 0;
}

int Net2_Query(Net2Host *h, const Net2Addr *to, uint64_t now) {
	uint8_t pkt[N2_QUERY_SIZE];
	Net2Writer w;

	h->now = now;
	Net2W_Init(&w, pkt, sizeof(pkt));
	Net2W_U8(&w, N2P_QUERY);
	Net2W_U32(&w, N2_PROTOCOL);
	Net2W_U32(&w, h->queryToken);
	Net2W_U32(&w, (uint32_t)now);
	Net2W_Zeros(&w, (int)sizeof(pkt) - w.pos);
	return Net2_RawSend(h, to, pkt, w.pos) ? NET2_OK : NET2_E_SOCKET;
}

int Net2_LanQuery(Net2Host *h, uint16_t basePort, uint64_t now) {
	static const int families[2] = { NET2_AF_IPV4, NET2_AF_IPV6 };
	Net2Addr to;
	int sent = 0;

	for (int f = 0; f < 2; f++) {
		if (!h->sockStarted || (families[f] == NET2_AF_IPV6 ? h->sock6 : h->sock4) != N2_NOSOCK) {
			for (int i = 0; i < NET2_LAN_PORTS && basePort + i <= 0xffff; i++) {
				Net2_LanAddr(families[f], (uint16_t)(basePort + i), &to);
				sent += Net2_Query(h, &to, now) == NET2_OK;
			}
		}
	}
	return sent;
}

void Net2Lan_Packet(Net2Host *h, const Net2Addr *from, const uint8_t *pkt, int size) {
	uint8_t out[NET2_MTU];
	Net2ServerInfo info;
	uint32_t token, time;

	if (pkt[0] == N2P_QUERY) {
		Net2Reader r;
		Net2R_Init(&r, pkt, size);
		Net2R_U8(&r);
		if (!h->cfg.server || size != N2_QUERY_SIZE || Net2R_U32(&r) != N2_PROTOCOL) {
			return;
		}
		token = Net2R_U32(&r);
		time = Net2R_U32(&r);
		info = h->info;
		info.version = NET2_VERSION;
		info.appVersion = h->cfg.appVersion;
		int n = Net2Lan_WriteInfo(out, sizeof(out), token, time, &info);
		if (n > 0) {
			Net2_RawSend(h, from, out, n);
		}
		return;
	}
	if (!Net2Lan_ReadInfo(pkt, size, &token, &time, &info) || token != h->queryToken || h->evCount == N2_EVENTS) {
		return;
	}
	Net2Event *ev = &h->events[(h->evHead + h->evCount++) % N2_EVENTS];
	memset(ev, 0, sizeof(*ev));
	ev->type = NET2_EV_SERVERINFO;
	ev->addr = *from;
	ev->ping = (uint32_t)h->now - time;
	ev->info = info;
}
