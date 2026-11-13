/*
 * net2load: load tester of the Net2 transport (include/scrap/net2.h), stage M0b of the multiplayer plan. One
 * process opens hundreds of client hosts against a server: either the reference server of this tool (fake
 * snapshots on the sequenced channel, events and chat relays on the reliable one) or later the headless instance.
 *
 * Messages of the tool (byte 0 the type, numbers little endian through the Net2 writer and reader):
 *   JOIN     c>s reliable    index u32, identity public key[32], EdDSA signature[64] of the join text
 *   INPUT    c>s unreliable  count u8, count frames of LOAD_FRAME bytes (newest first), zeros to the input size
 *   CHAT     c>s reliable    seq u32, filler
 *   MARK     c>s reliable    start of the measured window: the server resets its tick times and baselines
 *   QUIT     c>s reliable    end; the server answers STATS, which arrives after every earlier reliable message
 *   WELCOME  s>c reliable    ok u8 (signature good), filler to the welcome size (stands for the game state)
 *   SNAP     s>c sequenced   tick u32, seq u32, last input tick u32, its client time u32, inputs u32, filler
 *   EVENT    s>c reliable    seq u32, filler
 *   RELAY    s>c reliable    origin index u32, chat seq u32, filler
 *   STATS    s>c reliable    LOAD_STAT_COUNT u32 values (LS_*)
 * An input frame: tick u32, client time u32 (ms), buttons u16, yaw u16, pitch u16, forward i8, side i8.
 */
#ifndef NET2LOAD_H
#define NET2LOAD_H

#include <stddef.h>
#include <stdint.h>

#include "net2local.h"		/* the byte writer and reader, the packet window */

#define LM_JOIN 1
#define LM_INPUT 2
#define LM_CHAT 3
#define LM_MARK 4
#define LM_QUIT 5
#define LM_WELCOME 10
#define LM_SNAP 11
#define LM_EVENT 12
#define LM_RELAY 13
#define LM_STATS 14

#define LOAD_FRAME 16
#define LOAD_REDUNDANT 3			/* input frames per message */
#define LOAD_INPUT_MIN (2 + LOAD_FRAME * LOAD_REDUNDANT)
#define LOAD_SNAP_MIN 21
#define LOAD_JOIN_SIZE (1 + 4 + 32 + 64)

#define LOAD_CH_RELIABLE 0			/* the default channels of Net2_DefaultConfig */
#define LOAD_CH_SEQUENCED 1
#define LOAD_CH_UNRELIABLE 2

/* STATS values: per client since its join (counts) or since MARK (traffic), then the server's since MARK */
enum {
	LS_INPUTS,						/* distinct input ticks received */
	LS_SNAPS,						/* snapshots queued */
	LS_EVENTS,						/* events queued (before this STATS) */
	LS_RELAYS,						/* chat relays queued (before this STATS) */
	LS_CHATS,						/* chat messages received */
	LS_CHATBAD,						/* chat messages out of order */
	LS_PKT_OUT, LS_PKT_IN,			/* the server connection since MARK */
	LS_KB_OUT, LS_KB_IN,			/* bytes / 1024 */
	LS_RESENT,
	LS_DROPPED,
	LS_RTT,
	LS_SENDFULL,					/* Net2_Send refused (E_FULL) for this client */
	LS_WINDOW_MS,					/* server: ms since MARK */
	LS_TICKS, LS_LATE,				/* ticks run, ticks that started more than one period late */
	LS_TICK_AVG, LS_TICK_P50, LS_TICK_P99, LS_TICK_MAX,	/* thread CPU per tick, us */
	LS_WALL_P99, LS_WALL_MAX,		/* wall time per tick, us */
	LS_CPU_PERMILLE,				/* process CPU / wall */
	LS_RSS_KB, LS_HWM_KB, LS_NET2_KB, LS_NET2_PEAK_KB,
	LS_CONNS, LS_JOINS, LS_JOINBAD,
	LS_SOCK_DROPS,					/* datagrams the server socket dropped since MARK (full receive buffer) */
	LOAD_STAT_COUNT
};

/* a histogram of uint32 values: exact below 64, then 32 steps per power of two (3 % at most off) */
#define LOAD_HIST_BUCKETS (32 + 27 * 32)
typedef struct LoadHist {
	uint32_t bucket[LOAD_HIST_BUCKETS];
	uint64_t count, sum;
	uint32_t min, max;
} LoadHist;

void Hist_Clear(LoadHist *h);
void Hist_Add(LoadHist *h, uint32_t v);
uint32_t Hist_Pct(const LoadHist *h, double pct);	/* 0..100 */
uint32_t Hist_Avg(const LoadHist *h);

/* Main.c: the host side helpers */
uint64_t Load_Now(void);			/* us of a monotonic clock */
uint64_t Load_ThreadCpu(void);		/* us of this thread */
uint64_t Load_ProcessCpu(void);		/* us of this process */
void Load_Wait(uint64_t us);		/* a short wait of the tool's own loop (never inside Net2) */
void Load_Memory(uint32_t *rssKb, uint32_t *hwmKb);
uint32_t Load_SockDrops(uint16_t port);	/* drops of the UDP sockets on this local port (Linux), else 0 */
uint32_t Load_Random(uint32_t *state);	/* xorshift32 */
void Load_Seed32(uint8_t out[32], const char *what, uint64_t seed, uint32_t index);
void Load_JoinText(uint8_t *out, uint32_t index, const uint8_t serverKey[NET2_KEY_SIZE]);	/* LOAD_JOIN_TEXT */
#define LOAD_JOIN_TEXT (16 + 4 + NET2_KEY_SIZE)

/* allocation counter given to Net2 (cfg.alloc / cfg.free) */
extern size_t load_memUsed, load_memPeak;
void *Load_Alloc(void *ctx, size_t size);
void Load_Free(void *ctx, void *p);

/* options of both modes */
typedef struct LoadOpts {
	uint64_t seed;
	uint32_t appVersion;
	uint32_t rate;
	long sockBuffer;				/* cfg.sockBuffer, -1 the Net2 default */
	double interval;				/* s between progress lines */
	const char *timeline;			/* CSV of the progress lines */
	const char *bind;				/* server: its address; client: the server's */
	int maxConns;
	int tickHz, snapHz, snapSize;
	int pollHz;						/* server: Net2_Poll between the ticks too (drains the socket), 0 only per tick */
	double eventHz;
	int eventSize, welcomeSize, relay;
	double seconds;					/* server: 0 runs until killed; client: the measured window */
	int exitEmpty;
	int clients;
	const char *clientBase;			/* first client address; each client its own (127.x.y.z on the loopback) */
	int clientPort;
	int inputHz, inputSize;
	double chatEvery;
	int chatSize;
	double loss;					/* percent, each direction, by the filter hook of every client host */
	double ramp;					/* ms between connects */
	double warmup, connectWait, drain;
	const char *csv, *label;
	int pin;
} LoadOpts;

int Server_Run(const LoadOpts *o);
int Client_Run(const LoadOpts *o);

#endif
