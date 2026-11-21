#!/usr/bin/env python3
"""The census by class of the multiplayer rewrite (plan mp2, stages 4.1 and 4.2) on every world level: per level a
headless -mp2 instance (sandbox, or the PvP mode of --mode, include/scrap/arena.h) and one joined headless -mp2
client (SCRAP_PLAYBOT, idle by default), then

  - mirror.py: the declared state of every entity on the server and the client (differences in all three pairs),
  - census.py --classes: the replicated entities by class and kind (missing, extra, state),
  - defcmp.py: the engine and module variables the client's level start must make as the server's,
  - the audits of both sides: the client's level start and its steady state (client.sim.call, client.script.create,
    client.purge, client.noclass, client.def.skip), pres.unforwarded, pres.double and the unresolved player reads
    (player.world.norule) of the instance, pres.double and pres.missing of the client,
  - a crash of either side (FATAL ERROR, AddressSanitizer, a signal; the instance's known exit check
    g_CountSprite2D == 0 is not counted).

  classes.py --out DIR [--levels A,B] [--bot idle] [--mode sandbox] [--jobs 4] [--port BASE] [--build DIR] [--scratch DIR]
             [--settle 40] [--report-only]

Writes <out>/<level>.json per level, <out>/classes.json and <out>/classes.md; exit 0 when every level has 0 in every
column, 1 otherwise, 2 when a level could not run. Worker n of --jobs uses --port + n. Settle and waits are game time
(SCRAP_TIMESCALE, tools/test/gameclock.py)."""
import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import threading
import time

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(REPO, 'tools', 'test'))
import parity  # noqa: E402
import gameclock  # noqa: E402

CHANNEL = os.path.join(REPO, 'tools', 'test', 'channel.py')
ENV = {'SCRAP_NO_MSGBOX': '1', 'PULSE_SERVER': 'unix:/nonexistent', 'PIPEWIRE_REMOTE': '/nonexistent',
       'SDL_AUDIODRIVER': 'dummy', 'SDL_VIDEODRIVER': 'dummy', 'SCRAP_TEST_CHANNEL': '1'}
CRASH = re.compile(r'FATAL ERROR|AddressSanitizer|LeakSanitizer|Segmentation fault|SIGSEGV|SIGABRT')
COLUMNS = ('mirror', 'classes', 'defs', 'start_sim', 'start_create', 'start_purge', 'start_noclass', 'sim', 'create',
           'purge', 'noclass', 'unforwarded', 'double', 'missing', 'norule', 'crash')


def ask(game, lines, level=None, timeout=60):
    cmd = [sys.executable, CHANNEL, game] + (['-i', level] if level else []) + lines
    try:
        return subprocess.run(cmd, capture_output=True, text=True, timeout=timeout).stdout
    except subprocess.TimeoutExpired:
        return ''


def audit_json(game, level=None):
    text = ask(game, ['audit json'], level)
    at = text.find('{')
    try:
        return json.loads(text[at:]) if at >= 0 else {}
    except ValueError:
        return {}


def keys(a, prefix):
    return sum(r.get('count', 0) for r in a.get('rows', []) if str(r.get('key', '')).startswith(prefix))


def game_dir(path, build):
    if not os.path.isdir(os.path.join(path, 'Bin')):
        subprocess.run([os.path.join(REPO, 'tools', 'test', 'mkgame.sh'), '--native', path], capture_output=True)
    shutil.copy2(os.path.join(build, 'ScrapC'), os.path.join(path, 'Bin', 'ScrapC'))
    lib = os.path.join(build, 'ffmpeg', 'install', 'lib')
    for f in os.listdir(lib) if os.path.isdir(lib) else []:
        if '.so' in f:
            subprocess.run(['cp', '-a', os.path.join(lib, f), os.path.join(path, 'Bin')])
    for f in os.listdir(path):
        if f.startswith('test_') or (f.startswith('instance_') and f.endswith('.quit')):
            os.remove(os.path.join(path, f))


def run_level(a, w, level):
    out = a.out
    gi, gc = os.path.join(a.scratch, 'i-%d' % w), os.path.join(a.scratch, 'c-%d' % w)
    port = a.port + w
    res = {'level': level, 'bot': a.bot}
    for g in (gi, gc):
        game_dir(g, a.build)
    env = dict(os.environ, **ENV)
    env.pop('DISPLAY', None)
    env.pop('WAYLAND_DISPLAY', None)
    ilog, clog = os.path.join(out, level + '.i.log'), os.path.join(out, level + '.c.log')
    inst = subprocess.Popen(['timeout', '-k', '3', '1200', './ScrapC', '-instance:' + level, '-port:%d' % port,
                             '-bind:127.0.0.1', '-id:' + level, '-mode:' + a.mode, '-mp2'], cwd=os.path.join(gi, 'Bin'),
                            env=dict(env, XDG_DATA_HOME=os.path.join(gi, 'xdg-data'), SCRAP_GAME_DIR=gi),
                            stdin=subprocess.PIPE, stdout=open(ilog, 'w'), stderr=subprocess.STDOUT)
    cli = None
    try:
        end = time.time() + 120
        while time.time() < end and not ask(gi, ['audit reset'], level, 5).strip():
            time.sleep(1)
        if 'Net2 server on' not in open(ilog, errors='replace').read():
            raise RuntimeError('the instance has no server on port %d (taken?)' % port)
        cli = subprocess.Popen(['timeout', '-k', '3', '1100', './ScrapC', '-play:127.0.0.1', '-port:%d' % port,
                                '-imap:Levels/' + level, '-headless', '-name:bot', '-mp2'], cwd=os.path.join(gc, 'Bin'),
                               env=dict(env, XDG_DATA_HOME=os.path.join(gc, 'xdg-data'), SCRAP_GAME_DIR=gc,
                                        SCRAP_PLAYBOT=a.bot),
                               stdin=subprocess.DEVNULL, stdout=open(clog, 'w'), stderr=subprocess.STDOUT)
        end = time.time() + 150
        while time.time() < end and not re.search(r'"made":[1-9]', ask(gc, ['mirror json'], None, 10)):
            time.sleep(2)
        start = audit_json(gc)
        ask(gi, ['audit reset'], level)
        ask(gc, ['audit reset'])
        gameclock.sleep(a.settle)
        tool = lambda name, *args: subprocess.run([sys.executable, os.path.join(HERE, name)] + list(args),
                                                 capture_output=True, text=True, timeout=300)
        m = tool('mirror.py', '--game', gc, '--instance', gi, '--level', level, '--json',
                 os.path.join(out, level + '.mirror.json'), '--show', '60')
        c = tool('census.py', '--classes', '--client', gc, '--instance', gi, '--level', level, '--json',
                 os.path.join(out, level + '.classes.json'))
        d = tool('defcmp.py', '--client', gc, '--instance', gi, '--level', level, '--json',
                 os.path.join(out, level + '.defs.json'))
        ia, ca = audit_json(gi, level), audit_json(gc)
        for name, p in (('mirror', m), ('classes', c), ('defs', d)):
            res[name + '_text'] = (p.stdout or p.stderr).strip().splitlines()[:1]
            num = re.search(r'(?:differences in all 3 pairs: |: )(\d+) (?:gaps|differences|\()', (p.stdout or ''))
            res[name] = int(num.group(1)) if num else None
        res.update(start_sim=keys(start, 'client.sim.call'), start_create=keys(start, 'client.script.create'),
                   start_purge=keys(start, 'client.purge'), start_noclass=keys(start, 'client.noclass'),
                   start_defskip=keys(start, 'client.def.skip'), sim=keys(ca, 'client.sim.call'),
                   create=keys(ca, 'client.script.create'), purge=keys(ca, 'client.purge'),
                   noclass=keys(ca, 'client.noclass'), unforwarded=keys(ia, 'pres.unforwarded'),
                   norule=keys(ia, 'player.world.norule'),
                   double=keys(ia, 'pres.double') + keys(ca, 'pres.double'), missing=keys(ca, 'pres.missing'))
        res['client_rows'] = [r for r in start.get('rows', []) + ca.get('rows', [])
                              if str(r.get('key', '')).startswith(('client.sim', 'client.script', 'client.purge',
                                                                   'client.noclass', 'pres.missing'))][:40]
        res['instance_rows'] = [r for r in ia.get('rows', [])
                                if str(r.get('key', '')).startswith(('pres.unforwarded', 'pres.double',
                                                                     'player.world.norule'))][:40]
    finally:
        if cli:
            cli.terminate()
        try:
            inst.stdin.write(b'quit\n')
            inst.stdin.flush()
        except OSError:
            pass
        try:
            inst.wait(15)
        except subprocess.TimeoutExpired:
            inst.kill()
        parity.stop_dir(gi)
        parity.stop_dir(gc)
    crashes = []
    for log in (ilog, clog):
        text = open(log, errors='replace').read()
        for m2 in CRASH.finditer(text):
            tail = text[m2.start():m2.start() + 400]
            if 'g_CountSprite2D == 0' not in tail:
                crashes.append('%s: %s' % (os.path.basename(log), tail.splitlines()[0]))
    res['crash'] = len(crashes)
    res['crashes'] = crashes[:4]
    with open(os.path.join(out, level + '.json'), 'w') as f:
        json.dump(res, f, indent=1)
    return res


def total(r):
    return sum(r.get(k) or 0 for k in COLUMNS) + sum(1 for k in ('mirror', 'classes', 'defs') if r.get(k) is None)


def table(results):
    head = ['level', 'mirror', 'classes', 'defs', 'start sim/create/purge/noclass', 'sim/create/purge/noclass',
            'pres unforwarded/double/missing', 'unresolved player reads', 'crash']
    lines = ['| ' + ' | '.join(head) + ' |', '|' + '---|' * len(head)]
    for r in results:
        n = lambda k: 'n/a' if r.get(k) is None else str(r[k])
        lines.append('| %s | %s | %s | %s | %s/%s/%s/%s | %s/%s/%s/%s | %s/%s/%s | %s | %s |' % (
            r['level'], n('mirror'), n('classes'), n('defs'), n('start_sim'), n('start_create'), n('start_purge'),
            n('start_noclass'), n('sim'), n('create'), n('purge'), n('noclass'), n('unforwarded'), n('double'),
            n('missing'), n('norule'), n('crash')))
    return lines


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--out', required=True)
    ap.add_argument('--levels')
    ap.add_argument('--bot', default='idle')
    ap.add_argument('--mode', default='sandbox')
    ap.add_argument('--jobs', type=int, default=4)
    ap.add_argument('--port', type=int, default=30120)
    ap.add_argument('--build', default=os.path.join(REPO, 'build-linux64'))
    ap.add_argument('--scratch')
    ap.add_argument('--settle', type=float, default=40.0)
    ap.add_argument('--report-only', action='store_true')
    a = ap.parse_args()
    a.out = os.path.abspath(a.out)
    a.build = os.path.abspath(a.build)
    a.scratch = os.path.abspath(a.scratch or os.path.join(a.out, 'games'))
    os.makedirs(a.out, exist_ok=True)
    os.makedirs(a.scratch, exist_ok=True)
    levels = [x for x in (a.levels.split(',') if a.levels else parity.world_levels()) if x]
    results = {}
    if a.report_only:
        for lv in levels:
            try:
                results[lv] = json.load(open(os.path.join(a.out, lv + '.json')))
            except (OSError, ValueError):
                pass
    else:
        todo = list(levels)
        lock = threading.Lock()

        def worker(w):
            while True:
                with lock:
                    if not todo:
                        return
                    lv = todo.pop(0)
                try:
                    r = run_level(a, w, lv)
                except Exception as e:  # noqa: BLE001 - a level that could not run is a result too
                    r = {'level': lv, 'error': str(e)}
                with lock:
                    results[lv] = r
                    print('classes %s: %s' % (lv, 'error ' + r['error'] if 'error' in r else
                                              '%d in the columns' % total(r)), flush=True)
        threads = [threading.Thread(target=worker, args=(w,)) for w in range(max(1, a.jobs))]
        for t in threads:
            t.start()
        for t in threads:
            t.join()
    rows = [results[lv] for lv in levels if lv in results and 'error' not in results[lv]]
    errors = [lv for lv in levels if lv not in results or 'error' in results[lv]]
    lines = ['# Census by class (%d levels, bot %s)' % (len(rows), a.bot), ''] + table(rows)
    if errors:
        lines += ['', 'could not run: ' + ', '.join(errors)]
    status = 'error' if errors else 'fail' if any(total(r) for r in rows) else 'pass'
    with open(os.path.join(a.out, 'classes.md'), 'w') as f:
        f.write('\n'.join(lines) + '\n')
    with open(os.path.join(a.out, 'classes.json'), 'w') as f:
        json.dump({'status': status, 'bot': a.bot, 'levels': rows, 'errors': errors}, f, indent=1)
    print('\n'.join(lines))
    return 2 if errors else 1 if status == 'fail' else 0


if __name__ == '__main__':
    sys.exit(main())
