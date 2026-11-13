/*
 * The child processes of the coordinator (include/scrap/coord.h): started, watched and killed without waiting.
 * POSIX: posix_spawn into a process group of its own (a Ctrl+C of the coordinator's terminal does not reach the
 * instances: they end by their quit file or by themselves when the coordinator stays away), waitpid with WNOHANG.
 * Windows: CreateProcess with a new process group and no console window.
 */
#ifdef _WIN32
#include <windows.h>
#else
#define _GNU_SOURCE 1
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
extern char **environ;
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scrap/coord.h"

#define PROC_ENV_MAX 512			/* entries of the environment of a child */

#ifdef _WIN32

/* argv quoted for CommandLineToArgvW / the CRT */
static int Proc_Quote(char *out, int cap, const char *const *argv) {
	int o = 0;
	for (int i = 0; argv[i]; i++) {
		if (o + 2 >= cap) {
			return 0;
		}
		if (i) {
			out[o++] = ' ';
		}
		out[o++] = '"';
		int bs = 0;
		for (const char *p = argv[i]; *p; p++) {
			if (o + 2 * bs + 4 >= cap) {
				return 0;
			}
			if (*p == '\\') {
				bs++;
				continue;
			}
			for (int k = 0; k < (*p == '"' ? 2 * bs + 1 : bs); k++) {
				out[o++] = '\\';
			}
			bs = 0;
			out[o++] = *p;
		}
		for (int k = 0; k < 2 * bs && o < cap - 2; k++) {
			out[o++] = '\\';
		}
		out[o++] = '"';
	}
	out[o] = 0;
	return 1;
}

int CoordProc_Start(CoordProc *p, const char *exe, const char *const *argv, const char *cwd, const char *logPath,
	const char *envName, const char *envValue) {
	static char cmd[4096], env[32768];
	SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
	STARTUPINFOA si;
	PROCESS_INFORMATION pi;

	memset(p, 0, sizeof(*p));
	if (!Proc_Quote(cmd, sizeof(cmd), argv)) {
		return 0;
	}
	/* the environment of this process plus envName=envValue */
	int o = 0;
	char *block = GetEnvironmentStringsA();
	if (block) {
		for (const char *s = block; *s; s += strlen(s) + 1) {
			int n = (int)strlen(s) + 1;
			size_t en = strlen(envName);
			if (!_strnicmp(s, envName, en) && s[en] == '=') {
				continue;
			}
			if (o + n >= (int)sizeof(env) - 2) {
				break;
			}
			memcpy(env + o, s, (size_t)n);
			o += n;
		}
		FreeEnvironmentStringsA(block);
	}
	int n = snprintf(env + o, sizeof(env) - (size_t)o - 1, "%s=%s", envName, envValue);
	if (n < 0 || o + n + 2 >= (int)sizeof(env)) {
		return 0;
	}
	o += n + 1;
	env[o] = 0;
	HANDLE log = CreateFileA(logPath, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_ALWAYS,
		FILE_ATTRIBUTE_NORMAL, NULL);
	memset(&si, 0, sizeof(si));
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESTDHANDLES;
	si.hStdInput = INVALID_HANDLE_VALUE;
	si.hStdOutput = log;
	si.hStdError = log;
	BOOL ok = CreateProcessA(exe, cmd, NULL, NULL, TRUE, CREATE_NEW_PROCESS_GROUP | CREATE_NO_WINDOW, env, cwd, &si,
		&pi);
	if (log != INVALID_HANDLE_VALUE) {
		CloseHandle(log);
	}
	if (!ok) {
		return 0;
	}
	CloseHandle(pi.hThread);
	p->handle = (intptr_t)pi.hProcess;
	p->pid = pi.dwProcessId;
	return 1;
}

int CoordProc_Adopt(CoordProc *p, uint32_t pid) {
	memset(p, 0, sizeof(*p));
	HANDLE h = OpenProcess(SYNCHRONIZE | PROCESS_TERMINATE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
	if (!h) {
		return 0;
	}
	p->handle = (intptr_t)h;
	p->pid = pid;
	p->adopted = 1;
	if (!CoordProc_Alive(p)) {
		return 0;
	}
	return 1;
}

int CoordProc_Alive(CoordProc *p) {
	if (!p->handle) {
		return 0;
	}
	if (WaitForSingleObject((HANDLE)p->handle, 0) == WAIT_TIMEOUT) {
		return 1;
	}
	CloseHandle((HANDLE)p->handle);
	p->handle = 0;
	return 0;
}

void CoordProc_Kill(CoordProc *p) {
	if (p->handle) {
		TerminateProcess((HANDLE)p->handle, 1);
	}
}

void CoordProc_Self(char *buf, int size) {
	DWORD n = GetModuleFileNameA(NULL, buf, (DWORD)size);
	if (n >= (DWORD)size) {
		buf[0] = 0;
	}
}

#else

int CoordProc_Start(CoordProc *p, const char *exe, const char *const *argv, const char *cwd, const char *logPath,
	const char *envName, const char *envValue) {
	static char *envp[PROC_ENV_MAX + 2];
	static char extra[256];
	posix_spawn_file_actions_t fa;
	posix_spawnattr_t at;
	pid_t pid;

	memset(p, 0, sizeof(*p));
	int n = 0;
	size_t en = strlen(envName);
	for (char **e = environ; *e && n < PROC_ENV_MAX; e++) {
		if (!strncmp(*e, envName, en) && (*e)[en] == '=') {
			continue;
		}
		envp[n++] = *e;
	}
	if (snprintf(extra, sizeof(extra), "%s=%s", envName, envValue) >= (int)sizeof(extra)) {
		return 0;
	}
	envp[n++] = extra;
	envp[n] = NULL;
	if (posix_spawn_file_actions_init(&fa)) {
		return 0;
	}
	if (posix_spawnattr_init(&at)) {
		posix_spawn_file_actions_destroy(&fa);
		return 0;
	}
	posix_spawn_file_actions_addopen(&fa, 0, "/dev/null", O_RDONLY, 0);
	posix_spawn_file_actions_addopen(&fa, 1, logPath, O_WRONLY | O_CREAT | O_APPEND, 0600);
	posix_spawn_file_actions_adddup2(&fa, 1, 2);
	if (cwd && *cwd) {
		posix_spawn_file_actions_addchdir_np(&fa, cwd);
	}
	posix_spawnattr_setflags(&at, POSIX_SPAWN_SETPGROUP);
	posix_spawnattr_setpgroup(&at, 0);
	int err = posix_spawn(&pid, exe, &fa, &at, (char *const *)argv, envp);
	posix_spawn_file_actions_destroy(&fa);
	posix_spawnattr_destroy(&at);
	if (err) {
		return 0;
	}
	p->handle = pid;
	p->pid = (uint32_t)pid;
	return 1;
}

int CoordProc_Adopt(CoordProc *p, uint32_t pid) {
	memset(p, 0, sizeof(*p));
	if (!pid || kill((pid_t)pid, 0) != 0) {
		return 0;
	}
	p->handle = (intptr_t)pid;
	p->pid = pid;
	p->adopted = 1;
	return 1;
}

int CoordProc_Alive(CoordProc *p) {
	int st;
	if (!p->handle) {
		return 0;
	}
	if (p->adopted) {
		if (kill((pid_t)p->handle, 0) == 0 || errno == EPERM) {
			return 1;
		}
		p->handle = 0;
		return 0;
	}
	pid_t r = waitpid((pid_t)p->handle, &st, WNOHANG);
	if (r == 0) {
		return 1;
	}
	p->handle = 0;
	return 0;
}

void CoordProc_Kill(CoordProc *p) {
	if (p->handle) {
		kill((pid_t)p->handle, SIGKILL);
	}
}

void CoordProc_Self(char *buf, int size) {
	ssize_t n = readlink("/proc/self/exe", buf, (size_t)size - 1);
	buf[n > 0 ? n : 0] = 0;
}

#endif
