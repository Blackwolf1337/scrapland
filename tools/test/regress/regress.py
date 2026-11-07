#!/usr/bin/env python3
"""Regression runner for the Scrapland script ports: runs the case files of cases.txt on the C build and compares
each with the stored pack baselines (README.md next to this file).

  regress.py check [--build DIR] [--lanes ca,ca2,ca3] [--out DIR] [case ...]   C run, compare, summary, exit code
  regress.py baseline [--runs 2] [--lanes pa,pa2,pa3] [--add] [case ...]       pack runs into baselines/
  regress.py compare RESULTDIR [case ...]                                      compare stored C results again
  regress.py show RESULTDIR case                                               every difference of one case
  regress.py accept RESULTDIR case ...                                         accept a C run's differences

A case is given by its file name (fc-x.txt) or by a substring of it; without cases all of cases.txt."""
import argparse
import fcntl
import glob
import hashlib
import json
import os
import queue
import re
import shutil
import signal
import sys
import threading
import time

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, '..', '..', '..'))
sys.path.insert(0, HERE)


def _data_root():
	"""The checkout with the game data (game/) and the MCP slots (build-mcp/): SCRAP_MCP_ROOT, else this checkout, else
	the main checkout of this worktree (agent worktrees have no game folder of their own)."""
	if os.path.isdir(os.path.join(REPO, 'game')):
		return REPO
	try:
		import subprocess
		common = subprocess.run(['git', '-C', REPO, 'rev-parse', '--git-common-dir'], capture_output=True,
								text=True).stdout.strip()
		main = os.path.dirname(os.path.abspath(os.path.join(REPO, common)))
		if os.path.isdir(os.path.join(main, 'game')):
			return main
	except OSError:
		pass
	return REPO


# REGRESS_MCP_DIR: the tools/mcp of another checkout; default the scrap_mcp.py of this checkout. The game data and the
# slots are those of SCRAP_MCP_ROOT (default: _data_root()). REGRESS_HARNESS=channel: the games run with the test
# channel of the game (scrap_mcp 0.3.0 or newer, no Python code in the game); python: mcpgame.py in the game.
MCP_DIR = os.environ.get('REGRESS_MCP_DIR', os.path.join(REPO, 'tools', 'mcp'))
HARNESS = os.environ.get('REGRESS_HARNESS', 'python')
os.environ.setdefault('SCRAP_MCP_ROOT', _data_root())
sys.path.insert(0, MCP_DIR)
import scrap_mcp  # noqa: E402
import slot  # noqa: E402
import tochannel  # noqa: E402
from compare import compare, show_diff, load_json  # noqa: E402

DEFAULT_BUILD = os.path.join(REPO, 'build-linux64')
BASELINES = os.path.join(HERE, 'baselines')
HELPERS = os.path.join(HERE, 'helpers')
RESULTS = os.path.join(REPO, 'build-regress')
VK_DEVICE = os.environ.get('REGRESS_VK_DEVICE', '1002:164e')
# REGRESS_PY_PROFILE=1: the games run with the Python profile (SCRAP_PY_PROFILE, PyProfile.c), each result keeps the
# Python code objects that ran in its game (pyprofile): what a case still runs as Python
PY_PROFILE = os.environ.get('REGRESS_PY_PROFILE', '') not in ('', '0')
LINE_TIMEOUT = 30.0
CASE_TIMEOUT = 1500.0
# game seconds a level load counts for in a !sleep (the real-time sleeps of the cases covered the load)
LOAD_CREDIT = 20.0
# real seconds a command may wait longer than its timeout while a level load it started runs
LOAD_WAIT = 240.0
# real seconds after the end of a load that still count as the load (the new level's scripts start)
LOAD_START = 15.0
_lock = threading.Lock()
PORT_CASES = set()


def log(msg):
	with _lock:
		sys.stdout.write(msg + '\n')
		sys.stdout.flush()


def read_cases():
	"""cases.txt: '<Level> <case file> [port]' per line, # comments; port: the case starts a real server (one such
	case at a time)."""
	out = []
	for line in open(os.path.join(HERE, 'cases.txt')):
		line = line.split('#', 1)[0].split()
		if len(line) >= 2:
			out.append((line[0], line[1]))
			if 'port' in line[2:]:
				PORT_CASES.add(line[1])
	return out


def pick(cases, names):
	if not names:
		return cases
	out = []
	for n in names:
		hit = [c for c in cases if c[1] == n] or [c for c in cases if n in c[1]]
		if not hit:
			raise SystemExit('no case %s in cases.txt' % n)
		out += [c for c in hit if c not in out]
	return out


def case_path(case):
	"""A case file of cases/."""
	return os.path.join(HERE, 'cases', case)


HELPER = re.compile(r"(?:execfile|open)\('([A-Za-z0-9_]+\.py)'")


def case_hash(case):
	"""Hash of the case file and the helper files it loads (a changed case makes its baseline stale)."""
	h = hashlib.sha1()
	text = open(case_path(case), 'rb').read()
	h.update(text)
	for p in sorted(set(HELPER.findall(text.decode('latin1')))):
		try:
			h.update(open(os.path.join(HELPERS, p), 'rb').read())
		except OSError:
			h.update(b'missing ' + p.encode())
	h.update(open(os.path.join(HELPERS, 'fst.py'), 'rb').read())
	return h.hexdigest()[:16]


def _python(self, code, timeout=10.0):
	"""scrap_mcp.Game.python with a 10 ms poll of the reply file instead of 100 ms (the game answers within 0.1 s of
	game time; at about 10000 case lines a run this saves a few minutes). Same files and protocol."""
	if not self.running():
		raise RuntimeError('game %s does not run' % self.name)
	# a level load that a command started (Scrap.LoadLevel loads at once, the answer comes after it) does not count
	# against the timeout: loads took 40 s on the shared 610M (Lane.loads)
	loading = getattr(self, 'loading', None)
	if self.opts.get('harness') == 'channel':
		# paced as the old harness answered: every line at least 0.1 s of game time after the one before
		seq = scrap_mcp.channel.write(self.game, code, None, 0.1)
		end = time.time() + timeout
		while True:
			try:
				return scrap_mcp.channel.answer(self.game, seq, max(end - time.time(), 0.5), self.running)
			except scrap_mcp.channel.NoAnswer:
				if not (self.running() and loading and loading() and time.time() < end + LOAD_WAIT):
					raise RuntimeError('no answer from game %s in %.0f s' % (self.name, timeout))
				end = time.time() + 1.0
	self.seq += 1
	reply = os.path.join(self.game, 'mcp_reply.txt')
	tmp = os.path.join(self.game, 'mcp_cmd.tmp')
	with open(tmp, 'w', newline='\r\n') as f:
		f.write('%d\n%s\n' % (self.seq, code))
	os.replace(tmp, os.path.join(self.game, 'mcp_cmd.txt'))
	start = time.time()
	end = start + timeout
	want = str(self.seq)
	while time.time() < end:
		try:
			with open(reply, 'r', errors='replace') as f:
				data = f.read().replace('\r\n', '\n')
		except OSError:
			data = ''
		head, _, rest = data.partition('\n')
		if head.strip() == want and rest.endswith('.end\n'):
			return rest[:-len('.end\n')].rstrip('\n')
		if not self.running():
			break
		if time.time() >= end and loading and loading() and time.time() < start + timeout + LOAD_WAIT:
			end = time.time() + 1.0
		time.sleep(0.01)
	raise RuntimeError('no answer from game %s in %.0f s' % (self.name, timeout))


scrap_mcp.Game.python = _python


def copy_helpers(game):
	"""The Python helpers of the cases (helpers/*.py) into the game folder, where the case lines load them; the source
	of NDeathMatchBots (pack only, fc-nm-bots) from REGRESS_PACK_SRC (the root of the extracted packs) when set."""
	for p in glob.glob(os.path.join(HELPERS, '*.py')):
		shutil.copy(p, os.path.join(game, os.path.basename(p)))
	src = os.environ.get('REGRESS_PACK_SRC')
	if src:
		p = os.path.join(src, 'Data.packed', 'scripts', 'net', 'ndeathmatchbots.py')
		if os.path.isfile(p):
			shutil.copy(p, os.path.join(game, 'ndeathmatchbots.py'))


class Lane:
	"""One slot: resets it, starts the game, runs the case lines."""

	def __init__(self, tag, pack, exe='native64'):
		self.tag, self.pack, self.exe = tag, pack, exe
		self.game = None

	def q(self, code, timeout=LINE_TIMEOUT):
		"""a command of the harness (Python, or channel commands with REGRESS_HARNESS=channel)"""
		return self.game.python(code, timeout=timeout)

	def line(self, line, timeout=LINE_TIMEOUT):
		"""a case line: as it is to the Python harness; on the channel its commands (tochannel.py) in one command file,
		the answer the first error of the helper commands, else that of the line"""
		if HARNESS == 'python':
			return self.q(line, timeout)
		try:
			cmds = tochannel.convert(line)
		except tochannel.Unconvertible as e:
			return '!! not on the channel: %s' % e
		return self.commands(cmds, timeout)

	def commands(self, cmds, timeout=LINE_TIMEOUT):
		"""channel commands in one command file; the first error answer of all but the last, else the last answer"""
		if len(cmds) == 1:
			return self.q(cmds[0], timeout)
		mark = "'--tc-mark--'"
		out = self.q('\n'.join(cmds[:-1] + [mark, cmds[-1]]), timeout)
		head, sep, tail = out.partition(mark + '\n')
		if not sep:
			head, sep, tail = out.partition(mark)
		for ln in head.split('\n'):
			if ln.startswith('error: '):
				return ln
		return tail

	def gstate(self):
		"""(game time, mark set) or None while the game does not answer (a level loads)."""
		try:
			r = self.q("(Scrap.GetTime(), globals().get('_rg_mark'))" if HARNESS == 'python' else
					   "(Scrap.GetTime(), __import__('__main__').__dict__.get('_rg_mark'))", timeout=2.0)
		except RuntimeError:
			return None
		m = re.match(r"\(([-\d.e+]+), (\w+)\)", r)
		if not m:
			return None
		return float(m.group(1)), m.group(2) != 'None'

	def loads(self):
		"""[level loads started, 1 while one runs] of this game: the 'Loading level at' and 'Level loaded in' lines
		of its output (App.c), read on from where the last call stopped."""
		try:
			with open(self.game.wine_log, 'rb') as f:
				f.seek(self.logpos)
				data = f.read()
		except OSError:
			return self.nload
		cut = data.rfind(b'\n') + 1
		self.logpos += cut
		for ln in data[:cut].split(b'\n'):
			if ln.startswith(b'Loading level at '):
				self.nload = [self.nload[0] + 1, 1]
			elif ln.startswith(b'Level loaded in '):
				self.nload[1] = 0
				self.load_end = time.time()
		return self.nload

	def gsleep_load(self, n, end):
		"""!sleep n after a level load that started during the case line before or during this wait (seen in the game
		output): the wait ends at n - LOAD_CREDIT seconds of the new level's own time (Scrap.GetTime() starts at the
		level start), however long the load took in real time. Real seconds count only while the new level answers
		with its clock standing (a menu)."""
		stand, prev, last_prog, last_real = 0.0, None, time.time(), time.time()
		while time.time() < end:
			time.sleep(0.2)
			st = None if self.loads()[1] else self.gstate()
			if not self.game.running():
				return
			now = time.time()
			if st is None:
				prev = None
			else:
				t = st[0]
				if prev is not None and t <= prev and now - last_prog > 3.0:
					stand += now - last_real
				if prev is None or t > prev:
					last_prog = now
				prev = t
				if LOAD_CREDIT + t + stand >= n:
					return
			last_real = now

	def gsleep(self, n):
		"""!sleep n: n seconds of game time. A level load during the wait (the case line before loaded a level) is
		seen by the mark (a level load starts Python over): the load counts as LOAD_CREDIT seconds and the time of the
		new level from its start. Real seconds count instead while the game clock stands (a menu stopped it) or the
		game does not answer; at most 2 n + 120 real seconds. A load the game output shows (loads()) goes to
		gsleep_load: real seconds of a slow load must not count, or the read after the wait comes earlier in the new
		level on a loaded machine (on the 610M shared with other games a load took 40 s instead of 5)."""
		start = time.time()
		end = start + 2 * n + 120
		if self.loads()[0] > self.line_loads:
			return self.gsleep_load(n, end)
		try:
			self.q('_rg_mark = 1', timeout=2.0)
		except RuntimeError:
			pass
		acc, prev, last_real, last_prog = 0.0, None, time.time(), time.time()
		while acc < n and time.time() < end:
			time.sleep(0.2)
			now = time.time()
			st = self.gstate()
			if not self.game.running():
				return
			if st is None:
				if self.loads()[0] > self.line_loads:
					return self.gsleep_load(n, end)
				if now - last_prog > 3.0:
					acc += now - last_real
				last_real = now
				continue
			if self.loads()[0] > self.line_loads:
				return self.gsleep_load(n, end)
			t, marked = st
			if not marked:
				try:
					self.q('_rg_mark = 1', timeout=2.0)
				except RuntimeError:
					pass
				# a new level cannot be older than this wait (else the first mark was lost while the game was busy)
				if t <= now - start + 1.0:
					prev = 0.0
					acc += LOAD_CREDIT
			if prev is None:
				prev = t
			if t > prev:
				acc += t - prev
				last_prog = now
			elif now - last_prog > 3.0:
				acc += now - last_real
			prev = t
			last_real = now

	def kill(self):
		"""Ends the game at once (SIGKILL to the game only: the shell inside gamescope then reports the exit code and
		headless.sh ends the session as usual). Nothing of a clean exit is compared, the slot is reset before the next
		case, and a clean quit costs about 4 s per case."""
		g = self.game
		for p in g.native_pids()[0]:
			try:
				os.kill(p, signal.SIGKILL)
			except OSError:
				pass
		try:
			g.proc.wait(timeout=20)
			g.proc = None
		except Exception:
			g.stop()

	def run(self, level, case, build):
		t0 = time.time()
		slot.reset(self.tag, self.exe)
		scrap_mcp.NATIVE_BUILDS['native64'] = build
		scrap_mcp.NATIVE_BUILDS['native64asan'] = build + '-asan'
		g = scrap_mcp.Game(self.tag)
		self.logpos, self.nload, self.line_loads, self.load_end = 0, [0, 0], 0, 0.0
		# a level load runs or ended a moment ago (the new level starts its scripts before it answers)
		g.loading = lambda: self.loads()[1] or time.time() < self.load_end + LOAD_START
		with _lock:
			scrap_mcp.GAMES[self.tag] = g
		self.game = g
		env = {'SCRAP_VK_DEVICE': VK_DEVICE}
		if PY_PROFILE:
			env['SCRAP_PY_PROFILE'] = 'pyprofile.txt'
		if self.pack:
			env['SCRAP_PACK_SCRIPTS'] = '1'
		res = {'case': case, 'level': level, 'tag': self.tag, 'pack': self.pack, 'build': build,
			   'hash': case_hash(case), 'date': time.strftime('%Y-%m-%d %H:%M:%S'), 'blocks': [], 'status': 'ok'}
		try:
			opts = {'name': self.tag, 'exe': self.exe, 'map': 'Levels/' + level, 'env': env, 'timeout': 2400}
			# scrap_mcp picks the channel for a build that has it: the harness is always named
			opts['harness'] = HARNESS
			g.start(opts)
			if HARNESS == 'python':
				copy_helpers(g.game)
			end = time.time() + 300
			while True:
				if not g.running() or time.time() > end:
					raise RuntimeError('no answer at the level start')
				try:
					self.q('Scrap.GetLevelPath()', timeout=2.0)
					break
				except RuntimeError:
					pass
			res['ready'] = round(time.time() - t0, 1)
			if HARNESS == 'python':
				self.q("execfile('fst.py')")
			else:
				r = self.commands(tochannel.helper_load('fst') + ['None'])
				if r != 'None':
					raise RuntimeError('channel/fst.tc: %s' % r)
			timeouts = 0
			for line in open(case_path(case), encoding='latin1').read().split('\n'):
				if not line.strip() or line.startswith('#'):
					continue
				if line.startswith('!sleep '):
					self.gsleep(float(line.split()[1]))
					continue
				if time.time() - t0 > CASE_TIMEOUT:
					res['status'] = 'timeout'
					break
				try:
					self.line_loads = self.loads()[0]
					reply = self.line(line)
					timeouts = 0
				except RuntimeError as e:
					if not g.running():
						res['blocks'].append([line, '!! the game ended'])
						res['status'] = 'crash'
						break
					reply = '!! no answer in %d s' % LINE_TIMEOUT
					timeouts += 1
				res['blocks'].append([line, reply])
				if timeouts >= 2:
					res['status'] = 'hang'
					break
		except Exception as e:
			res['status'] = 'error'
			res['error'] = '%s: %s' % (type(e).__name__, e)
		if PY_PROFILE and g.running():
			# the Python code objects that ran in this game (through all its level loads)
			try:
				self.q("Scrap.DumpPyProfile('pyprofile.txt')", timeout=5.0)
				res['pyprofile'] = open(os.path.join(g.game, 'pyprofile.txt'), errors='replace').read()
			except (RuntimeError, OSError) as e:
				res['pyprofile'] = 'no profile: %s' % e
		try:
			res['log'] = '\n'.join(g.log('game').splitlines()[-40:])
		except Exception:
			pass
		try:
			if g.proc:
				self.kill()
		except Exception as e:
			res['stop'] = str(e)
		try:
			w = open(g.wine_log, errors='replace').read()
			for k in ('ERROR: AddressSanitizer', 'Segmentation fault', 'CHECK', 'Assertion'):
				if k in w:
					res.setdefault('wine', []).append(k)
		except OSError:
			pass
		res['secs'] = round(time.time() - t0, 1)
		self.game = None
		return res


def save(path, res):
	os.makedirs(os.path.dirname(path), exist_ok=True)
	with open(path, 'w') as f:
		json.dump(res, f, indent=0)
	if os.path.dirname(path) == BASELINES:
		return
	# the same in the text form of func6.py, for a plain diff
	with open(path[:-5] + '.txt', 'w') as f:
		for cmd, reply in res['blocks']:
			f.write('>> %s\n%s\n' % (cmd, reply))


def lanes_run(tags, pack, jobs, build, done, exe='native64'):
	"""jobs: list of (level, case, outpath), longest first; runs them over the lanes; done(job, res) per result. The
	cases that start a real server have a queue of their own: a lane that gets the port lock (a file lock, so a pack
	and a C run side by side share it) takes the next of them, else the next other case, so the server cases run
	spread over the whole run, one at a time."""
	normal, ports = queue.Queue(), queue.Queue()
	for j in jobs:
		(ports if j[1] in PORT_CASES else normal).put(j)
	lockpath = os.path.join(HERE, '.port.lock')

	def worker(tag):
		lane = Lane(tag, pack, exe)
		while True:
			res = None
			if not ports.empty():
				with open(lockpath, 'w') as lk:
					try:
						fcntl.flock(lk, fcntl.LOCK_EX | (fcntl.LOCK_NB if not normal.empty() else 0))
						j = ports.get_nowait()
						res = lane.run(j[0], j[1], build)
					except (OSError, queue.Empty):
						pass
			if res is None:
				try:
					j = normal.get_nowait()
				except queue.Empty:
					if ports.empty():
						return
					continue
				res = lane.run(j[0], j[1], build)
			save(j[2], res)
			done(j, res)

	ts = [threading.Thread(target=worker, args=(t,)) for t in tags]
	for t in ts:
		t.start()
	for t in ts:
		t.join()


def expected_secs(case):
	"""The time of the case's first pack run (for the order: longest first)."""
	runs = pack_runs(case)
	return runs[0].get('secs', 60) if runs else 60


def pack_runs(case):
	"""The stored pack runs of a case: baselines/<case>.xz (a JSON list of runs, xz), with loose runs
	baselines/<case>.p<k>.json of a baseline command not folded in yet in place of run k."""
	p = os.path.join(BASELINES, case + '.xz')
	out = load_json(p) if os.path.isfile(p) else []
	k = 1
	while True:
		p = os.path.join(BASELINES, '%s.p%d.json' % (case, k))
		if os.path.isfile(p):
			r = json.load(open(p))
			if k <= len(out):
				out[k - 1] = r
			else:
				out.append(r)
		elif k > len(out):
			return out
		k += 1


def fold(case):
	"""Folds the loose runs baselines/<case>.p<k>.json into baselines/<case>.xz (under a file lock: parallel baseline
	commands fold the same case). The game log of a run is not kept (the results directory has it)."""
	import lzma
	with open(os.path.join(BASELINES, '.fold.lock'), 'w') as lk:
		fcntl.flock(lk, fcntl.LOCK_EX)
		runs = pack_runs(case)
		for r in runs:
			r.pop('log', None)
		data = json.dumps(runs, indent=0).encode()
		with lzma.open(os.path.join(BASELINES, case + '.xz.tmp'), 'wb', preset=9 | lzma.PRESET_EXTREME) as f:
			f.write(data)
		os.replace(os.path.join(BASELINES, case + '.xz.tmp'), os.path.join(BASELINES, case + '.xz'))
		for p in glob.glob(os.path.join(BASELINES, glob.escape(case) + '.p*.json')):
			os.remove(p)


def judge(case, cres):
	"""(verdict, detail lines) of one C result against its baselines."""
	packs = pack_runs(case)
	if len(packs) < 2:
		return 'nobase', ['fewer than 2 pack runs in baselines/']
	# a case that ends at the same fatal engine check in every pack run (an artificial state) must end there in C too
	statuses = set(p['status'] for p in packs)
	if len(statuses) > 1 or statuses - {'ok', 'crash'} or len(set(len(p['blocks']) for p in packs)) > 1:
		return 'badbase', ['pack run status %s' % ', '.join('%s (%d lines)' % (p['status'], len(p['blocks']))
															  for p in packs)]
	expected = statuses.pop()
	notes = []
	h = case_hash(case)
	if any(p['hash'] != h for p in packs):
		notes.append('stale baseline: the case file or a helper changed since the pack runs')
	if cres['status'] != expected:
		notes.append('C run status %s %s (pack %s)' % (cres['status'], cres.get('error', ''), expected))
	ok, diffs, masked = compare(case, [p['blocks'] for p in packs], cres['blocks'])
	sanitizer = [w for w in cres.get('wine', []) if w in ('ERROR: AddressSanitizer', 'Segmentation fault')]
	if sanitizer:
		ok = False
		notes.append('game output: %s' % ', '.join(sanitizer))
	for i, cmd, exp, got in diffs[:8]:
		notes.append('line %d: %s\n      %s' % (i + 1, cmd[:150], show_diff(exp, got)))
	if len(diffs) > 8:
		notes.append('... %d more differing lines' % (len(diffs) - 8))
	if ok and cres['status'] == expected:
		return 'pass', notes
	return 'FAIL', ['%d differing lines of %d (%d loose)' % (len(diffs), len(cres['blocks']), masked)] + notes


def cmd_baseline(a):
	cases = pick(read_cases(), a.cases)
	first = a.index or 1
	if a.add:
		first = min(len(pack_runs(c)) for l, c in cases) + 1
	jobs = []
	for k in range(first, first + a.runs):
		for level, case in sorted(cases, key=lambda c: -expected_secs(c[1])):
			jobs.append((level, case, os.path.join(BASELINES, '%s.p%d.json' % (case, k))))
	t0 = time.time()
	n = [0]

	def done(j, res):
		n[0] += 1
		log('[%d/%d %.0fs] %s %s: %s, %d lines, %.0f s' % (n[0], len(jobs), time.time() - t0, res['tag'], j[1],
														  res['status'], len(res['blocks']), res['secs']))
	lanes_run(a.lanes.split(','), True, jobs, a.build, done)
	for level, case in cases:
		fold(case)


def report(outdir, cases, results, t0=None):
	verdicts = {}
	lines = []
	for level, case in cases:
		if case not in results:
			continue
		v, notes = judge(case, results[case])
		verdicts[case] = v
		if v != 'pass' or notes:
			lines.append('%s %s (%s): %s' % (v, case, level, '\n    '.join(notes) if notes else ''))
	n = len(verdicts)
	p = sum(1 for v in verdicts.values() if v == 'pass')
	summary = 'regress: %d/%d pass, %d fail%s' % (p, n, n - p, ', %.0f min' % ((time.time() - t0) / 60) if t0 else '')
	if n - p:
		summary += ' (%s)' % ' '.join(c for c, v in verdicts.items() if v != 'pass')
	text = '\n'.join(lines + [summary]) + '\n'
	with open(os.path.join(outdir, 'report.txt'), 'w') as f:
		f.write(text)
	sys.stdout.write(text)
	return p == n


def cmd_check(a):
	cases = pick(read_cases(), a.cases)
	outdir = a.out or os.path.join(RESULTS, time.strftime('%Y%m%d-%H%M%S'))
	os.makedirs(outdir, exist_ok=True)
	jobs = [(l, c, os.path.join(outdir, c + '.json')) for l, c in sorted(cases, key=lambda c: -expected_secs(c[1]))]
	results = {}
	t0 = time.time()
	import subprocess
	rev = subprocess.run(['git', '-C', os.path.dirname(os.path.abspath(a.build)), 'log', '-1', '--format=%h %s'],
						 capture_output=True, text=True).stdout.strip()
	exe = os.path.join(a.build, 'ScrapC')
	info = 'build %s (%s), ScrapC of %s' % (a.build, rev, time.ctime(os.path.getmtime(exe)))
	open(os.path.join(outdir, 'build.txt'), 'w').write(info + '\n')
	log(info)

	def done(j, res):
		results[j[1]] = res
		v, notes = judge(j[1], res)
		log('[%d/%d %.0fs] %s %s: %s%s' % (len(results), len(jobs), time.time() - t0, res['tag'], j[1], v,
										   (' - ' + notes[0].split('\n')[0]) if v != 'pass' and notes else ''))
	lanes_run(a.lanes.split(','), False, jobs, a.build, done, 'native64asan' if a.asan else 'native64')
	return 0 if report(outdir, cases, results, t0) else 1


def cmd_compare(a):
	cases = pick(read_cases(), a.cases)
	results = {}
	for l, c in cases:
		p = os.path.join(a.resultdir, c + '.json')
		if os.path.isfile(p):
			results[c] = json.load(open(p))
	return 0 if report(a.resultdir, cases, results) else 1


def cmd_accept(a):
	"""Stores the C results of the cases as their accepted C runs (the differences were looked at)."""
	for c in a.cases:
		res = json.load(open(os.path.join(a.resultdir, c + '.json')))
		res['accepted'] = time.strftime('%Y-%m-%d %H:%M:%S')
		res.pop('log', None)
		import lzma
		with lzma.open(os.path.join(BASELINES, c + '.accepted.xz'), 'wb', preset=9 | lzma.PRESET_EXTREME) as f:
			f.write(json.dumps(res, indent=0).encode())
		print('accepted %s (%s, %d lines)' % (c, res['build'], len(res['blocks'])))
	return 0


def cmd_show(a):
	cres = json.load(open(os.path.join(a.resultdir, a.case + '.json')))
	packs = pack_runs(a.case)
	ok, diffs, masked = compare(a.case, [p['blocks'] for p in packs], cres['blocks'])
	print('%s: C status %s, %d differing lines, %d loose' % (a.case, cres['status'], len(diffs), masked))
	for i, cmd, exp, got in diffs:
		print('line %d: %s\n  P: %s\n  C: %s' % (i + 1, cmd, exp, got))
	return 0


def _stop(signum, frame):
	"""Ctrl-C or SIGTERM: end the games of the lanes too (they run in sessions of their own)."""
	for g in list(scrap_mcp.GAMES.values()):
		try:
			for pid in g.native_pids()[0]:
				os.kill(pid, signal.SIGKILL)
		except Exception:
			pass
	os._exit(130)


def main():
	signal.signal(signal.SIGINT, _stop)
	signal.signal(signal.SIGTERM, _stop)
	ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
	sub = ap.add_subparsers(dest='cmd', required=True)
	b = sub.add_parser('baseline')
	b.add_argument('--runs', type=int, default=2)
	b.add_argument('--add', action='store_true', help='add runs after the stored ones')
	b.add_argument('--index', type=int, help='number of the first run (p<index>.json), default 1')
	b.add_argument('--lanes', default='pa,pa2,pa3')
	b.add_argument('--build', default=os.environ.get('FUNC_BUILD', DEFAULT_BUILD))
	b.add_argument('cases', nargs='*')
	c = sub.add_parser('check')
	c.add_argument('--lanes', default='ca,ca2,ca3')
	c.add_argument('--build', default=os.environ.get('FUNC_BUILD', DEFAULT_BUILD))
	c.add_argument('--out')
	c.add_argument('--asan', action='store_true', help='the -asan twin of the build (lanes e.g. cax); ASan reports '
					'show as wine: ERROR: AddressSanitizer in the report')
	c.add_argument('cases', nargs='*')
	r = sub.add_parser('compare')
	r.add_argument('resultdir')
	r.add_argument('cases', nargs='*')
	ac = sub.add_parser('accept')
	ac.add_argument('resultdir')
	ac.add_argument('cases', nargs='+')
	s = sub.add_parser('show')
	s.add_argument('resultdir')
	s.add_argument('case')
	a = ap.parse_args()
	return {'baseline': cmd_baseline, 'check': cmd_check, 'compare': cmd_compare, 'show': cmd_show,
			'accept': cmd_accept}[a.cmd](a) or 0


if __name__ == '__main__':
	sys.exit(main())
