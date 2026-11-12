/*
 * Net2 platform layer: non-blocking UDP sockets of both families on Winsock 2 or BSD sockets, numeric addresses
 * (a name lookup would block, so there is none) and the random numbers of the system.
 */
#ifdef _WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601			/* inet_pton, inet_ntop */
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>
#include <bcrypt.h>
typedef int socklen_t;
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
#include <stdlib.h>
#define NET2_ARC4RANDOM
#else
#include <sys/random.h>
#endif
#endif

#include <stdio.h>

#include "net2local.h"

/* the IPv6 group of LAN discovery (link local scope): ff02::5c72:6170 */
static const uint8_t net2_lanGroup[16] = { 0xff, 0x02, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x5c, 0x72, 0x61, 0x70 };

int Net2_Random(void *buf, size_t size) {
	uint8_t *p = buf;
#ifdef _WIN32
	while (size > 0) {
		ULONG n = size > 0x10000 ? 0x10000 : (ULONG)size;
		if (BCryptGenRandom(NULL, p, n, BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) {
			return 0;
		}
		p += n;
		size -= n;
	}
#elif defined(NET2_ARC4RANDOM)
	arc4random_buf(p, size);
#else
	while (size > 0) {
		ssize_t n = getrandom(p, size, 0);
		if (n < 0) {
			if (errno == EINTR) {
				continue;
			}
			return 0;
		}
		p += n;
		size -= (size_t)n;
	}
#endif
	return 1;
}

static socklen_t Net2Sock_ToSys(const Net2Addr *a, struct sockaddr_storage *ss) {
	memset(ss, 0, sizeof(*ss));
	if (a->family == NET2_AF_IPV6) {
		struct sockaddr_in6 *s6 = (struct sockaddr_in6 *)ss;
		s6->sin6_family = AF_INET6;
		s6->sin6_port = htons(a->port);
		s6->sin6_scope_id = a->scope;
		memcpy(&s6->sin6_addr, a->ip, 16);
		return sizeof(*s6);
	}
	struct sockaddr_in *s4 = (struct sockaddr_in *)ss;
	s4->sin_family = AF_INET;
	s4->sin_port = htons(a->port);
	memcpy(&s4->sin_addr, a->ip, 4);
	return sizeof(*s4);
}

static int Net2Sock_FromSys(const struct sockaddr_storage *ss, Net2Addr *a) {
	memset(a, 0, sizeof(*a));
	if (ss->ss_family == AF_INET6) {
		const struct sockaddr_in6 *s6 = (const struct sockaddr_in6 *)ss;
		a->family = NET2_AF_IPV6;
		a->port = ntohs(s6->sin6_port);
		a->scope = s6->sin6_scope_id;
		memcpy(a->ip, &s6->sin6_addr, 16);
		return 1;
	}
	if (ss->ss_family == AF_INET) {
		const struct sockaddr_in *s4 = (const struct sockaddr_in *)ss;
		a->family = NET2_AF_IPV4;
		a->port = ntohs(s4->sin_port);
		memcpy(a->ip, &s4->sin_addr, 4);
		return 1;
	}
	return 0;
}

int Net2Sock_Startup(void) {
#ifdef _WIN32
	WSADATA wsa;
	return WSAStartup(MAKEWORD(2, 2), &wsa) == 0;
#else
	return 1;
#endif
}

void Net2Sock_Cleanup(void) {
#ifdef _WIN32
	WSACleanup();
#endif
}

void Net2Sock_Close(uintptr_t s) {
	if (s == N2_NOSOCK) {
		return;
	}
#ifdef _WIN32
	closesocket((SOCKET)s);
#else
	close((int)s);
#endif
}

static int Net2Sock_Opt(uintptr_t s, int level, int name, int value) {
#ifdef _WIN32
	return setsockopt((SOCKET)s, level, name, (const char *)&value, sizeof(value)) == 0;
#else
	return setsockopt((int)s, level, name, &value, sizeof(value)) == 0;
#endif
}

/* joins the LAN group on the default interface; only for sockets bound to any address (a server on a LAN) */
static void Net2Sock_JoinLan6(uintptr_t s) {
	struct ipv6_mreq mr;

	memset(&mr, 0, sizeof(mr));
	memcpy(&mr.ipv6mr_multiaddr, net2_lanGroup, 16);
	mr.ipv6mr_interface = 0;
#ifdef _WIN32
	setsockopt((SOCKET)s, IPPROTO_IPV6, IPV6_JOIN_GROUP, (const char *)&mr, sizeof(mr));
#else
	setsockopt((int)s, IPPROTO_IPV6, IPV6_JOIN_GROUP, &mr, sizeof(mr));
#endif
}

uintptr_t Net2Sock_Open(int family, const Net2Addr *at, uint16_t port) {
	int af = family == NET2_AF_IPV6 ? AF_INET6 : AF_INET;
	struct sockaddr_storage ss;
	Net2Addr local;
	uintptr_t s;

#ifdef _WIN32
	SOCKET ws = socket(af, SOCK_DGRAM, IPPROTO_UDP);
	if (ws == INVALID_SOCKET) {
		return N2_NOSOCK;
	}
	s = (uintptr_t)ws;
	u_long nonblock = 1;
	DWORD off = 0, got = 0;
	if (ioctlsocket(ws, (long)FIONBIO, &nonblock) != 0) {
		closesocket(ws);
		return N2_NOSOCK;
	}
	/* no WSAECONNRESET on recvfrom after an ICMP port unreachable */
	WSAIoctl(ws, SIO_UDP_CONNRESET, &off, sizeof(off), NULL, 0, &got, NULL, NULL);
#else
	int fd = socket(af, SOCK_DGRAM, IPPROTO_UDP);
	if (fd < 0) {
		return N2_NOSOCK;
	}
	s = (uintptr_t)fd;
	int fl = fcntl(fd, F_GETFL, 0);
	if (fl < 0 || fcntl(fd, F_SETFL, fl | O_NONBLOCK) < 0) {
		close(fd);
		return N2_NOSOCK;
	}
#endif
	if (af == AF_INET6) {
		Net2Sock_Opt(s, IPPROTO_IPV6, IPV6_V6ONLY, 1);
	} else {
		Net2Sock_Opt(s, SOL_SOCKET, SO_BROADCAST, 1);
	}
	if (at && at->family == family) {
		local = *at;
	} else {
		memset(&local, 0, sizeof(local));
		local.family = (uint8_t)family;
	}
	local.port = port;
	socklen_t len = Net2Sock_ToSys(&local, &ss);
#ifdef _WIN32
	int bad = bind((SOCKET)s, (struct sockaddr *)&ss, len) != 0;
#else
	int bad = bind((int)s, (struct sockaddr *)&ss, len) != 0;
#endif
	if (bad) {
		Net2Sock_Close(s);
		return N2_NOSOCK;
	}
	if (af == AF_INET6 && !(at && at->family == family)) {
		Net2Sock_JoinLan6(s);
	}
	return s;
}

int Net2Sock_Send(uintptr_t s, const Net2Addr *to, const uint8_t *data, int size) {
	struct sockaddr_storage ss;
	socklen_t len = Net2Sock_ToSys(to, &ss);
#ifdef _WIN32
	return sendto((SOCKET)s, (const char *)data, size, 0, (struct sockaddr *)&ss, len) == size;
#else
	ssize_t n;
	do {
		n = sendto((int)s, data, (size_t)size, 0, (struct sockaddr *)&ss, len);
	} while (n < 0 && errno == EINTR);
	return n == size;
#endif
}

int Net2Sock_Recv(uintptr_t s, Net2Addr *from, uint8_t *buf, int cap) {
	struct sockaddr_storage ss;

	for (;;) {
		socklen_t len = sizeof(ss);
#ifdef _WIN32
		int n = recvfrom((SOCKET)s, (char *)buf, cap, 0, (struct sockaddr *)&ss, &len);
		if (n == SOCKET_ERROR) {
			int e = WSAGetLastError();
			if (e == WSAEMSGSIZE || e == WSAECONNRESET || e == WSAENETRESET) {
				continue;			/* too large for NET2_MTU, or an ICMP error: the next one */
			}
			return 0;
		}
#else
		ssize_t n = recvfrom((int)s, buf, (size_t)cap, 0, (struct sockaddr *)&ss, &len);
		if (n < 0) {
			if (errno == EINTR || errno == ECONNREFUSED) {
				continue;
			}
			return 0;
		}
#endif
		if (n == 0 || !Net2Sock_FromSys(&ss, from)) {
			continue;
		}
		return (int)n;
	}
}

uint16_t Net2Sock_Port(uintptr_t s) {
	struct sockaddr_storage ss;
	socklen_t len = sizeof(ss);
	Net2Addr a;

#ifdef _WIN32
	if (getsockname((SOCKET)s, (struct sockaddr *)&ss, &len) != 0) {
#else
	if (getsockname((int)s, (struct sockaddr *)&ss, &len) != 0) {
#endif
		return 0;
	}
	return Net2Sock_FromSys(&ss, &a) ? a.port : 0;
}

void Net2_AddrKey(const Net2Addr *a, uint8_t out[20]) {
	out[0] = a->family;
	out[1] = (uint8_t)(a->port >> 8);
	out[2] = (uint8_t)a->port;
	out[3] = 0;
	memcpy(out + 4, a->ip, 16);
}

int Net2_AddrEqual(const Net2Addr *a, const Net2Addr *b) {
	if (a->family != b->family || a->port != b->port) {
		return 0;
	}
	return memcmp(a->ip, b->ip, a->family == NET2_AF_IPV6 ? 16 : 4) == 0;
}

static int Net2_ParsePort(const char *s, uint16_t *port) {
	uint32_t v = 0;

	if (!*s) {
		return 0;
	}
	for (; *s; s++) {
		if (*s < '0' || *s > '9') {
			return 0;
		}
		v = v * 10 + (uint32_t)(*s - '0');
		if (v > 0xffff) {
			return 0;
		}
	}
	if (v == 0) {
		return 0;
	}
	*port = (uint16_t)v;
	return 1;
}

/* "1.2.3.4", "1.2.3.4:5", "::1", "[::1]", "[fe80::1%eth0]:5" */
int Net2_ParseAddr(const char *s, uint16_t defPort, Net2Addr *out) {
	char host[96];
	const char *end, *port = NULL;
	struct addrinfo hints, *res = NULL;
	size_t n;

	memset(out, 0, sizeof(*out));
	if (!s || !*s) {
		return 0;
	}
	if (s[0] == '[') {
		s++;
		end = strchr(s, ']');
		if (!end) {
			return 0;
		}
		if (end[1] == ':') {
			port = end + 2;
		} else if (end[1]) {
			return 0;
		}
	} else {
		const char *c = strchr(s, ':');
		if (c && !strchr(c + 1, ':')) {
			end = c;
			port = c + 1;
		} else {
			end = s + strlen(s);
		}
	}
	n = (size_t)(end - s);
	if (n == 0 || n >= sizeof(host)) {
		return 0;
	}
	memcpy(host, s, n);
	host[n] = 0;
	uint16_t p = defPort;
	if (port && !Net2_ParsePort(port, &p)) {
		return 0;
	}
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_NUMERICHOST;
	if (getaddrinfo(host, NULL, &hints, &res) != 0 || !res) {
		return 0;
	}
	struct sockaddr_storage ss;
	memset(&ss, 0, sizeof(ss));
	int ok = res->ai_addrlen <= sizeof(ss);
	if (ok) {
		memcpy(&ss, res->ai_addr, res->ai_addrlen);
		ok = Net2Sock_FromSys(&ss, out);
	}
	freeaddrinfo(res);
	out->port = p;
	return ok;
}

void Net2_AddrString(const Net2Addr *a, char *buf, int size) {
	char ip[64];

	if (size <= 0) {
		return;
	}
	if (a->family == NET2_AF_IPV6) {
		if (!inet_ntop(AF_INET6, (void *)a->ip, ip, sizeof(ip))) {
			ip[0] = 0;
		}
		snprintf(buf, (size_t)size, "[%s]:%u", ip, (unsigned)a->port);
	} else if (a->family == NET2_AF_IPV4) {
		snprintf(buf, (size_t)size, "%u.%u.%u.%u:%u", a->ip[0], a->ip[1], a->ip[2], a->ip[3], (unsigned)a->port);
	} else {
		snprintf(buf, (size_t)size, "none");
	}
}

int Net2_LanAddr(int family, uint16_t port, Net2Addr *out) {
	memset(out, 0, sizeof(*out));
	out->port = port;
	if (family == NET2_AF_IPV6) {
		out->family = NET2_AF_IPV6;
		memcpy(out->ip, net2_lanGroup, 16);
		return 1;
	}
	if (family == NET2_AF_IPV4) {
		out->family = NET2_AF_IPV4;
		memset(out->ip, 0xff, 4);
		return 1;
	}
	return 0;
}
