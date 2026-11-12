/*
 * Messages of the Net2 tests: a reliable test message is index u32, size u32, then bytes (index * 31 + i), so the
 * receiver checks order, size and content.
 */
#ifndef NET2_TESTMSG_H
#define NET2_TESTMSG_H

#include <stdint.h>

static inline uint32_t Msg_U32(const uint8_t *p) {
	return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static inline void Msg_PutU32(uint8_t *p, uint32_t v) {
	for (int i = 0; i < 4; i++) {
		p[i] = (uint8_t)(v >> (8 * i));
	}
}

/* fills buf (size >= 8) and returns the size */
static inline int Msg_Reliable(uint8_t *buf, uint32_t index, int size) {
	if (size < 8) {
		size = 8;
	}
	Msg_PutU32(buf, index);
	Msg_PutU32(buf + 4, (uint32_t)size);
	for (int i = 8; i < size; i++) {
		buf[i] = (uint8_t)(index * 31 + (uint32_t)i);
	}
	return size;
}

static inline int Msg_Check(const uint8_t *data, int size, uint32_t index) {
	if (size < 8 || Msg_U32(data) != index || Msg_U32(data + 4) != (uint32_t)size) {
		return 0;
	}
	for (int i = 8; i < size; i++) {
		if (data[i] != (uint8_t)(index * 31 + (uint32_t)i)) {
			return 0;
		}
	}
	return 1;
}

#endif
