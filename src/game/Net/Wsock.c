/*
 * The jump thunks of the linker to the Winsock 1.1 functions of wsock32 (rva 0x20d9cc..0x20da20, "jmp [iat]"): the
 * game code and the network code linked into the exe below the game call them. Each one calls through the import
 * slot of the exe.
 */
#include "scrap/net.h"
#include "scrap/win32.h"

int __stdcall Wsock_bind(uint32_t s, const void *addr, int len) {
	return exe_bind(s, addr, len);
}

int __stdcall Wsock_closesocket(uint32_t s) {
	return exe_closesocket(s);
}

int __stdcall Wsock_ioctlsocket(uint32_t s, int32_t cmd, uint32_t *arg) {
	return exe_ioctlsocket(s, cmd, arg);
}

uint32_t __stdcall Wsock_htonl(uint32_t v) {
	return exe_htonl(v);
}

uint16_t __stdcall Wsock_htons(uint16_t v) {
	return exe_htons(v);
}

uint16_t __stdcall Wsock_ntohs(uint16_t v) {
	return exe_ntohs(v);
}

int __stdcall Wsock_recvfrom(uint32_t s, void *buf, int len, int flags, void *from, int *fromlen) {
	return exe_recvfrom(s, buf, len, flags, from, fromlen);
}

int __stdcall Wsock_select(int n, void *readfds, void *writefds, void *exceptfds, const void *timeout) {
	return exe_select(n, readfds, writefds, exceptfds, timeout);
}

int __stdcall Wsock_sendto(uint32_t s, const void *buf, int len, int flags, const void *to, int tolen) {
	return exe_sendto(s, buf, len, flags, to, tolen);
}

int __stdcall Wsock_setsockopt(uint32_t s, int level, int name, const void *val, int len) {
	return exe_setsockopt(s, level, name, val, len);
}

uint32_t __stdcall Wsock_socket(int af, int type, int protocol) {
	return exe_socket(af, type, protocol);
}

void *__stdcall Wsock_gethostbyaddr(const void *addr, int len, int type) {
	return exe_gethostbyaddr(addr, len, type);
}

void *__stdcall Wsock_gethostbyname(const char *name) {
	return exe_gethostbyname(name);
}

int __stdcall Wsock_WSAStartup(uint16_t version, void *data) {
	return exe_WSAStartup(version, data);
}

int __stdcall Wsock_WSACleanup(void) {
	return exe_WSACleanup();
}
