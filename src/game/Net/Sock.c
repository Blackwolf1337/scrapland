/*
 * The UDP socket of the network game on Winsock (Windows) or BSD sockets, see include/scrap/net.h. The original
 * called Winsock 1.1 directly; the layouts that differ between the two (hostent, fd_set, the ioctl of the
 * non-blocking mode) stay in here.
 */
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
typedef int socklen_t;
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include "scrap/net.h"

int Sock_Startup(void) {
#ifdef _WIN32
	WSADATA wsa;
	return WSAStartup(0x101, &wsa) == 0;
#else
	return 1;
#endif
}

void Sock_Cleanup(void) {
#ifdef _WIN32
	WSACleanup();
#endif
}

uint32_t Sock_OpenUdp(void) {
	int bcast = 1;

#ifdef _WIN32
	SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	u_long nonblock = 1;
	if (s == INVALID_SOCKET) {
		return 0;
	}
	if (ioctlsocket(s, (long)FIONBIO, &nonblock) == SOCKET_ERROR
		|| setsockopt(s, SOL_SOCKET, SO_BROADCAST, (const char *)&bcast, sizeof(bcast)) == SOCKET_ERROR) {
		closesocket(s);
		return 0;
	}
	return (uint32_t)s;
#else
	int s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (s < 0) {
		return 0;
	}
	if (fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK) < 0
		|| setsockopt(s, SOL_SOCKET, SO_BROADCAST, &bcast, sizeof(bcast)) < 0) {
		close(s);
		return 0;
	}
	return (uint32_t)s;
#endif
}

int Sock_Close(uint32_t s) {
#ifdef _WIN32
	return closesocket((SOCKET)s) != 0;
#else
	return close((int)s) != 0;
#endif
}

int Sock_Bind(uint32_t s, const NetAddr *a) {
	return bind(s, (const struct sockaddr *)a, sizeof(*a)) != 0;
}

int Sock_RecvFrom(uint32_t s, void *buf, int len, NetAddr *from) {
	socklen_t n = sizeof(*from);
	return (int)recvfrom(s, buf, (size_t)len, 0, (struct sockaddr *)from, &n);
}

int Sock_SendTo(uint32_t s, const void *buf, int len, const NetAddr *to) {
	return (int)sendto(s, buf, (size_t)len, 0, (const struct sockaddr *)to, sizeof(*to));
}

int Sock_Readable(uint32_t s) {
	fd_set fds;
	struct timeval tv = { 0, 0 };

	FD_ZERO(&fds);
	FD_SET(s, &fds);
	return select((int)s + 1, &fds, NULL, NULL, &tv) > 0;
}

int Sock_Resolve(const char *name, uint32_t *addr, char *canon, int size) {
	struct hostent *h = gethostbyname(name);

	if (!h) {
		int a = -1, b = -1, c = -1, d = -1;
		if (sscanf(name, "%d.%d.%d.%d", &a, &b, &c, &d) != 4) {
			return 0;
		}
		byte v[4] = { (byte)a, (byte)b, (byte)c, (byte)d };
		h = gethostbyaddr((const char *)v, 4, AF_INET);
		if (!h) {
			return 0;
		}
	}
	if (h->h_addrtype != AF_INET || !h->h_addr_list[0]) {
		return 0;
	}
	memcpy(addr, h->h_addr_list[0], 4);
	if (canon) {
		snprintf(canon, (size_t)size, "%s", h->h_name);
	}
	return 1;
}
