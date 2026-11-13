/*
 * net2load: command line (Usage below), clock, memory and the histogram. run.sh runs the standard matrix.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/resource.h>
#include <time.h>
#endif

#include "load.h"
#include "monocypher.h"

size_t load_memUsed, load_memPeak;

/* a size header in front of every block; 16 bytes keep malloc's alignment */
void *Load_Alloc(void *ctx, size_t size) {
	(void)ctx;
	uint8_t *p = malloc(size + 16);
	if (!p) {
		return NULL;
	}
	memcpy(p, &size, sizeof(size));
	load_memUsed += size;
	if (load_memUsed > load_memPeak) {
		load_memPeak = load_memUsed;
	}
	return p + 16;
}

void Load_Free(void *ctx, void *p) {
	size_t size;

	(void)ctx;
	if (!p) {
		return;
	}
	uint8_t *b = (uint8_t *)p - 16;
	memcpy(&size, b, sizeof(size));
	load_memUsed -= size;
	free(b);
}

uint64_t Load_Now(void) {
#ifdef _WIN32
	LARGE_INTEGER f, c;
	QueryPerformanceFrequency(&f);
	QueryPerformanceCounter(&c);
	return (uint64_t)((double)c.QuadPart * 1e6 / (double)f.QuadPart);
#else
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000000 + (uint64_t)ts.tv_nsec / 1000;
#endif
}

#ifdef _WIN32
static uint64_t Load_FileTimeUs(const FILETIME *a, const FILETIME *b) {
	uint64_t x = ((uint64_t)a->dwHighDateTime << 32 | a->dwLowDateTime);
	uint64_t y = ((uint64_t)b->dwHighDateTime << 32 | b->dwLowDateTime);
	return (x + y) / 10;
}
#endif

uint64_t Load_ThreadCpu(void) {
#ifdef _WIN32
	FILETIME c, e, k, u;
	GetThreadTimes(GetCurrentThread(), &c, &e, &k, &u);
	return Load_FileTimeUs(&k, &u);
#else
	struct timespec ts;
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
	return (uint64_t)ts.tv_sec * 1000000 + (uint64_t)ts.tv_nsec / 1000;
#endif
}

uint64_t Load_ProcessCpu(void) {
#ifdef _WIN32
	FILETIME c, e, k, u;
	GetProcessTimes(GetCurrentProcess(), &c, &e, &k, &u);
	return Load_FileTimeUs(&k, &u);
#else
	struct rusage ru;
	getrusage(RUSAGE_SELF, &ru);
	return (uint64_t)(ru.ru_utime.tv_sec + ru.ru_stime.tv_sec) * 1000000
		+ (uint64_t)(ru.ru_utime.tv_usec + ru.ru_stime.tv_usec);
#endif
}

void Load_Wait(uint64_t us) {
#ifdef _WIN32
	Sleep((DWORD)((us + 999) / 1000));
#else
	struct timespec ts = { (time_t)(us / 1000000), (long)(us % 1000000) * 1000 };
	nanosleep(&ts, NULL);
#endif
}

/* resident and peak resident memory; zeros where /proc is missing */
void Load_Memory(uint32_t *rssKb, uint32_t *hwmKb) {
	char line[256];

	*rssKb = 0;
	*hwmKb = 0;
	FILE *f = fopen("/proc/self/status", "r");
	if (!f) {
		return;
	}
	while (fgets(line, sizeof(line), f)) {
		if (!strncmp(line, "VmRSS:", 6)) {
			*rssKb = (uint32_t)strtoul(line + 6, NULL, 10);
		} else if (!strncmp(line, "VmHWM:", 6)) {
			*hwmKb = (uint32_t)strtoul(line + 6, NULL, 10);
		}
	}
	fclose(f);
}

uint32_t Load_SockDrops(uint16_t port) {
	static const char *const files[2] = { "/proc/net/udp", "/proc/net/udp6" };
	char line[512];
	uint32_t sum = 0;

	for (int i = 0; i < 2; i++) {
		FILE *f = fopen(files[i], "r");
		if (!f) {
			continue;
		}
		while (fgets(line, sizeof(line), f)) {
			unsigned lp, drops;
			/* sl local:port rem st tx:rx tr:when retrnsmt uid timeout inode ref pointer drops */
			if (sscanf(line, " %*d: %*[0-9A-Fa-f]:%x %*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %u", &lp, &drops) == 2
				&& lp == port) {
				sum += drops;
			}
		}
		fclose(f);
	}
	return sum;
}

uint32_t Load_Random(uint32_t *state) {
	uint32_t x = *state ? *state : 0x9e3779b9u;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	*state = x;
	return x;
}

/* 32 bytes for a key pair: BLAKE2b of what, the seed and the index */
void Load_Seed32(uint8_t out[32], const char *what, uint64_t seed, uint32_t index) {
	uint8_t in[64];
	Net2Writer w;

	Net2W_Init(&w, in, sizeof(in));
	Net2W_Bytes(&w, what, (int)strlen(what) > 48 ? 48 : (int)strlen(what));
	Net2W_U32(&w, (uint32_t)seed);
	Net2W_U32(&w, (uint32_t)(seed >> 32));
	Net2W_U32(&w, index);
	crypto_blake2b(out, 32, in, (size_t)w.pos);
}

void Load_JoinText(uint8_t *out, uint32_t index, const uint8_t serverKey[NET2_KEY_SIZE]) {
	Net2Writer w;

	Net2W_Init(&w, out, LOAD_JOIN_TEXT);
	Net2W_Bytes(&w, "net2load join v1", 16);
	Net2W_U32(&w, index);
	Net2W_Bytes(&w, serverKey, NET2_KEY_SIZE);
}

void Hist_Clear(LoadHist *h) {
	memset(h, 0, sizeof(*h));
}

static int Hist_Bucket(uint32_t v) {
	if (v < 32) {
		return (int)v;
	}
	int msb = 31;
	while (!(v >> msb)) {
		msb--;
	}
	return 32 + (msb - 5) * 32 + (int)((v >> (msb - 5)) & 31);
}

static uint32_t Hist_Value(int b) {
	if (b < 32) {
		return (uint32_t)b;
	}
	int shift = (b - 32) / 32;
	uint64_t lo = (uint64_t)(32 + (b - 32) % 32) << shift;
	return (uint32_t)(lo + ((uint64_t)1 << shift) / 2);
}

void Hist_Add(LoadHist *h, uint32_t v) {
	if (!h->count || v < h->min) {
		h->min = v;
	}
	if (v > h->max) {
		h->max = v;
	}
	h->bucket[Hist_Bucket(v)]++;
	h->count++;
	h->sum += v;
}

uint32_t Hist_Pct(const LoadHist *h, double pct) {
	if (!h->count) {
		return 0;
	}
	uint64_t want = (uint64_t)((double)h->count * pct / 100.0 + 0.5);
	uint64_t seen = 0;
	if (want < 1) {
		want = 1;
	}
	for (int b = 0; b < LOAD_HIST_BUCKETS; b++) {
		seen += h->bucket[b];
		if (seen >= want) {
			uint32_t v = Hist_Value(b);
			return v < h->min ? h->min : v > h->max ? h->max : v;
		}
	}
	return h->max;
}

uint32_t Hist_Avg(const LoadHist *h) {
	return h->count ? (uint32_t)(h->sum / h->count) : 0;
}

static void Usage(void) {
	fputs(
		"usage: net2load server [options]   the reference server\n"
		"       net2load client [options]   N fake clients in this process\n"
		"both:   --seed N (1) --app-version N (0) --rate BYTES (131072, per connection)\n"
		"        --interval S (10, progress lines) --timeline FILE (progress lines as CSV)\n"
		"server: --bind ADDR (127.0.0.1:29300) --max-conns N (512) --tick-hz N (30) --snap-hz N (20)\n"
		"        --snap-size N (600) --event-hz F (1) --event-size N (48) --welcome-size N (16384)\n"
		"        --relay 0|1 (1, chat to every client) --seconds S (0: until killed) --exit-empty\n"
		"client: --server ADDR (127.0.0.1:29300) --clients N (64) --client-base ADDR (127.10.0.1, one\n"
		"        address per client) --client-port N (29301) --input-hz N (30) --input-size N (50)\n"
		"        --chat-every S (10, 0 none) --chat-size N (96) --loss PCT (0, each direction)\n"
		"        --ramp MS (0, between connects) --warmup S (2) --connect-wait S (15) --seconds S (30)\n"
		"        --drain S (10) --csv FILE (appends one line) --label TEXT --pin (pin the seed's server key)\n",
		stderr);
}

int main(int argc, char **argv) {
	LoadOpts o;

	memset(&o, 0, sizeof(o));
	o.seed = 1;
	o.rate = 128 * 1024;
	o.interval = 10;
	o.maxConns = 512;
	o.tickHz = 30;
	o.snapHz = 20;
	o.snapSize = 600;
	o.eventHz = 1;
	o.eventSize = 48;
	o.welcomeSize = 16384;
	o.relay = 1;
	o.clients = 64;
	o.clientBase = "127.10.0.1";
	o.clientPort = 29301;
	o.inputHz = 30;
	o.inputSize = 50;
	o.chatEvery = 10;
	o.chatSize = 96;
	o.warmup = 2;
	o.connectWait = 15;
	o.drain = 10;
	o.label = "";
	if (argc < 2) {
		Usage();
		return 2;
	}
	int server = !strcmp(argv[1], "server");
	if (!server && strcmp(argv[1], "client")) {
		Usage();
		return 2;
	}
	o.bind = "127.0.0.1:29300";
	o.seconds = server ? 0 : 30;
	for (int i = 2; i < argc; i++) {
		const char *k = argv[i];
		const char *v = i + 1 < argc ? argv[i + 1] : NULL;
		int used = 1;
#define OPT_D(name, field) else if (!strcmp(k, name) && v) { o.field = strtod(v, NULL); }
#define OPT_I(name, field) else if (!strcmp(k, name) && v) { o.field = (int)strtol(v, NULL, 0); }
#define OPT_S(name, field) else if (!strcmp(k, name) && v) { o.field = v; }
		if (!strcmp(k, "--exit-empty")) {
			o.exitEmpty = 1;
			used = 0;
		} else if (!strcmp(k, "--pin")) {
			o.pin = 1;
			used = 0;
		} else if (!strcmp(k, "--seed") && v) {
			o.seed = strtoull(v, NULL, 0);
		} else if (!strcmp(k, "--app-version") && v) {
			o.appVersion = (uint32_t)strtoul(v, NULL, 0);
		} else if (!strcmp(k, "--rate") && v) {
			o.rate = (uint32_t)strtoul(v, NULL, 0);
		}
		OPT_D("--interval", interval)
		OPT_S("--timeline", timeline)
		OPT_S("--bind", bind)
		OPT_S("--server", bind)
		OPT_I("--max-conns", maxConns)
		OPT_I("--tick-hz", tickHz)
		OPT_I("--snap-hz", snapHz)
		OPT_I("--snap-size", snapSize)
		OPT_D("--event-hz", eventHz)
		OPT_I("--event-size", eventSize)
		OPT_I("--welcome-size", welcomeSize)
		OPT_I("--relay", relay)
		OPT_D("--seconds", seconds)
		OPT_I("--clients", clients)
		OPT_S("--client-base", clientBase)
		OPT_I("--client-port", clientPort)
		OPT_I("--input-hz", inputHz)
		OPT_I("--input-size", inputSize)
		OPT_D("--chat-every", chatEvery)
		OPT_I("--chat-size", chatSize)
		OPT_D("--loss", loss)
		OPT_D("--ramp", ramp)
		OPT_D("--warmup", warmup)
		OPT_D("--connect-wait", connectWait)
		OPT_D("--drain", drain)
		OPT_S("--csv", csv)
		OPT_S("--label", label)
		else {
			fprintf(stderr, "net2load: unknown option %s\n", k);
			Usage();
			return 2;
		}
#undef OPT_D
#undef OPT_I
#undef OPT_S
		i += used;
	}
	if (o.tickHz < 1 || o.snapHz < 1 || o.snapHz > o.tickHz || o.inputHz < 1 || o.clients < 1
		|| o.maxConns < 1 || o.maxConns > NET2_MAX_CONNS) {
		fputs("net2load: rates and counts must be positive (snap-hz <= tick-hz, max-conns <= 1024)\n", stderr);
		return 2;
	}
	if (o.snapSize < LOAD_SNAP_MIN || o.snapSize > NET2_MAX_UNRELIABLE || o.inputSize < LOAD_INPUT_MIN
		|| o.inputSize > NET2_MAX_UNRELIABLE || o.eventSize < 5 || o.eventSize > NET2_MAX_MESSAGE
		|| o.welcomeSize < 2 || o.welcomeSize > NET2_MAX_MESSAGE || o.chatSize < 5 || o.chatSize > NET2_MAX_MESSAGE) {
		fprintf(stderr, "net2load: sizes: snap %d..%d, input %d..%d, event/chat 5.., welcome 2.., at most %d\n",
			LOAD_SNAP_MIN, NET2_MAX_UNRELIABLE, LOAD_INPUT_MIN, NET2_MAX_UNRELIABLE, NET2_MAX_MESSAGE);
		return 2;
	}
	return server ? Server_Run(&o) : Client_Run(&o);
}
