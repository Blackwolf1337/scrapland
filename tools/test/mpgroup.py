#!/usr/bin/env python3
"""mpgroup.py: a scripted network test of a server and N clients, all headless, through the scrap MCP server
(tools/mcp/scrap_mcp.py, spoken to over stdio as an assistant does). Python standard library only.

Steps (--steps, default join,move,fire,score; bots when --bots is given):
  join    the server (dedicated by default) and N clients start in their own slots (reset to the regression
          template), the clients connect, wait for their ships and spawn (Ffa.RestartOnPress, the Fire key of the
          help screen); every side must see every client ship, active, with its player name
  move    all clients hold the forward key (w) at once for --move-secs; on the server every client ship must have
          moved, and every client must see each ship where the server has it (within --tolerance)
  fire    client 1 shoots client 2: the server puts the ship of client 2 in front of the ship of client 1 (life 1),
          client 1 holds Fire (ctrl); the server must count the kill (Ffa.Scorer)
  score   the scores of every client must equal those of the server
  bots    (--bots N: mode DeathMatchBots) every side sees the bot ships net256.. and they move on the server

Each step prints PASS or FAIL with what it saw; --out writes all states as JSON. Exit code 0 when all steps pass.
Timeouts on every tool call and on the whole run (--timeout); the games end with the run (quit all, then the
processes of the slots are killed if any is left). Sleeps are host side only.

  tools/test/mpgroup.py --clients 3 --mode DeathMatch --map FZ
  tools/test/mpgroup.py --clients 2 --slots qm,cm,cm2 --build build-linux64 --device 10de:27b0 --out /tmp/x.json
  tools/test/mpgroup.py --clients 1 --bots 6 --steps join,bots
Slots: --slots <server>,<client 1>,... (default qm,cm,cm2,cm3); SCRAP_MCP_SLOTS puts them in another folder.
"""
import argparse
import json
import math
import os
import queue
import re
import signal
import subprocess
import sys
import threading
import time

HERE = os.path.dirname(os.path.abspath(__file__))
MCP = os.path.join(HERE, '..', 'mcp', 'scrap_mcp.py')


class Fail(RuntimeError):
    pass


class Mcp:
    """the scrap MCP server as a child process; call() waits at most timeout seconds for an answer"""

    def __init__(self, env):
        self.p = subprocess.Popen([sys.executable, MCP], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True,
                                  env=env)
        self.q = queue.Queue()
        self.n = 0
        threading.Thread(target=self._read, daemon=True).start()
        self.rpc('initialize', {'protocolVersion': '2025-06-18', 'capabilities': {},
                                'clientInfo': {'name': 'mpgroup', 'version': '1'}}, 30)
        self.p.stdin.write(json.dumps({'jsonrpc': '2.0', 'method': 'notifications/initialized'}) + '\n')
        self.p.stdin.flush()

    def _read(self):
        for line in self.p.stdout:
            try:
                self.q.put(json.loads(line))
            except ValueError:
                pass
        self.q.put(None)

    def rpc(self, method, params, timeout):
        self.n += 1
        self.p.stdin.write(json.dumps({'jsonrpc': '2.0', 'id': self.n, 'method': method, 'params': params}) + '\n')
        self.p.stdin.flush()
        end = time.time() + timeout
        while True:
            try:
                r = self.q.get(timeout=max(0.1, end - time.time()))
            except queue.Empty:
                raise Fail('%s: no answer of the MCP server in %.0f s' % (params.get('name', method), timeout))
            if r is None:
                raise Fail('the MCP server ended')
            if r.get('id') == self.n:
                return r

    def tool(self, tool_name, limit=60, **args):
        """a tool call; limit: seconds to wait for its answer"""
        r = self.rpc('tools/call', {'name': tool_name, 'arguments': args}, limit)
        res = r.get('result') or {}
        txt = ''.join(c.get('text', '') for c in res.get('content', []) if c.get('type') == 'text')
        if 'error' in r or res.get('isError'):
            raise Fail('%s: %s' % (tool_name, txt or r.get('error')))
        return txt

    def close(self, timeout=90):
        try:
            self.p.stdin.close()
        except OSError:
            pass
        try:
            self.p.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            self.p.kill()


def slot_dir(tag):
    if os.environ.get('SCRAP_MCP_SLOTS'):
        return os.path.join(os.environ['SCRAP_MCP_SLOTS'], 'slot-' + tag)
    sys.path.insert(0, os.path.join(HERE, '..', 'mcp'))
    import scrap_mcp
    return os.path.join(scrap_mcp.SLOTS, 'slot-' + tag)


def kill_leftovers(tags):
    """SIGKILL to the games of the slots and their gamescope sessions (only what the MCP server did not end)"""
    exes = set(os.path.realpath(os.path.join(slot_dir(t), 'native64-game', 'Bin', 'ScrapC')) for t in tags)
    sids = set()
    for p in os.listdir('/proc'):
        if p.isdigit():
            try:
                if os.path.realpath('/proc/%s/exe' % p) in exes:
                    sids.add(os.getsid(int(p)))
            except OSError:
                pass
    for p in os.listdir('/proc'):
        if p.isdigit():
            try:
                if os.getsid(int(p)) in sids:
                    os.kill(int(p), signal.SIGKILL)
            except OSError:
                pass
    return len(sids)


def dist(a, b):
    return math.sqrt(sum((x - y) ** 2 for x, y in zip(a, b)))


class Run:
    def __init__(self, a, mcp):
        self.a, self.m = a, mcp
        self.tags = a.slots.split(',')
        self.server, self.clients = self.tags[0], self.tags[1:1 + a.clients]
        self.results, self.log = [], []
        self.ships = {}        # client tag -> ship name
        self.players = {}    # client tag -> player name

    def say(self, msg):
        print(msg, flush=True)

    def step(self, name, ok, detail, data=None):
        self.results.append({'step': name, 'pass': bool(ok), 'detail': detail, 'data': data})
        self.say('%s: %s  %s' % (name, 'PASS' if ok else 'FAIL', detail))
        return ok

    def states(self):
        return json.loads(self.m.tool('state', names=[self.server] + self.clients, limit=60))

    def env(self):
        return {'SCRAP_VK_DEVICE': self.a.device}

    def start(self):
        a = self.a
        srv = dict(name=self.server, mode=a.mode, map=a.map, reset=True, env=self.env(), timeout=a.timeout + 120,
                   dedicated=not a.host, deterministic=a.deterministic, fraglimit=a.fraglimit)
        if a.port:
            srv['port'] = a.port
        if a.bots:
            srv['bots'] = a.bots
        t = self.m.tool('server_launch', limit=200, **srv)
        self.say(t)
        self.port = int(re.search(r'on 127\.0\.0\.1:(\d+)', t).group(1))
        for i, c in enumerate(self.clients):
            self.players[c] = 'P%d' % (i + 1)
            self.say(self.m.tool('launch', limit=200, name=c, connect='127.0.0.1:%d' % self.port,
                                 player=self.players[c], reset=True, env=self.env(), timeout=a.timeout + 120,
                                 deterministic=a.deterministic))
        for c in self.clients:
            t = self.m.tool('wait', limit=200, name=c, kind='joined', spawn=True, timeout=150)
            self.say(t)
            self.ships[c] = re.search(r'ship (net\d+)', t).group(1)

    # ---- steps

    def s_join(self):
        st = self.states()
        self.log.append(('join', st))
        want = set(self.ships.values())
        bad = []
        for s in st:
            if 'error' in s:
                bad.append('%s: %s' % (s['name'], s['error']))
                continue
            seen = set(s['players'])
            if not want <= seen:
                bad.append('%s misses %s' % (s['name'], sorted(want - seen)))
            for c, sh in self.ships.items():
                p = s['players'].get(sh) or {}
                if p and (p.get('descriptor') != self.players[c] or p.get('active') != 1):
                    bad.append('%s sees %s as %r active %r' % (s['name'], sh, p.get('descriptor'), p.get('active')))
        names = ', '.join('%s=%s(%s)' % (c, self.ships[c], self.players[c]) for c in self.clients)
        return self.step('join', not bad, '; '.join(bad) or '%d clients joined on all %d sides: %s' % (
            len(self.clients), len(st), names), st)

    def s_move(self):
        a = self.a
        before = self.states()
        srv0 = before[0]['players']
        self.m.tool('hold', limit=a.move_secs + 30, names=self.clients, keys=['w'], seconds=a.move_secs)
        time.sleep(a.settle)
        after = self.states()
        self.log.append(('move', [before, after]))
        srv1 = after[0]['players']
        bad, info = [], []
        self.moved = {}
        for c, sh in self.ships.items():
            if sh not in srv0 or sh not in srv1:
                bad.append('server lost %s' % sh)
                continue
            d = dist(srv0[sh]['pos'], srv1[sh]['pos'])
            self.moved[sh] = [y - x for x, y in zip(srv0[sh]['pos'], srv1[sh]['pos'])]
            info.append('%s %.0f' % (sh, d))
            if d < a.min_move:
                bad.append('%s moved %.0f on the server' % (sh, d))
        worst = 0.0
        for s in after[1:]:
            for sh in self.ships.values():
                if sh in s['players'] and sh in srv1:
                    # the sides are read one after the other: a ship still gliding moves meanwhile
                    e = dist(s['players'][sh]['pos'], srv1[sh]['pos'])
                    allow = a.tolerance + a.skew * math.sqrt(sum(v * v for v in srv1[sh]['vel']))
                    worst = max(worst, e)
                    if e > allow:
                        bad.append('%s sees %s %.0f away from the server (allowed %.0f)' % (s['name'], sh, e, allow))
                else:
                    bad.append('%s does not see %s' % (s['name'], sh))
        return self.step('move', not bad, '; '.join(bad) or 'moved on the server: %s; largest client/server '
                         'difference %.0f (tolerance %.0f + %.1f s of speed)' % (', '.join(info), worst, a.tolerance, a.skew),
                         {'before': before, 'after': after})

    def s_fire(self):
        if len(self.clients) < 2:
            return self.step('fire', True, 'skipped (needs 2 clients)')
        a = self.a
        c1, c2 = self.clients[0], self.clients[1]
        s1, s2 = self.ships[c1], self.ships[c2]
        st = json.loads(self.m.tool('state', name=c1, limit=30))
        p1 = st['players'][s1]
        srv = json.loads(self.m.tool('state', name=self.server, limit=30))
        score0 = srv['scores'].get(s1, 0)
        # the old net code takes the position of a ship from its own client (the server's Pos is overwritten by the
        # next packet), so each client places its own ship. The shooter turns back the way it came in the move step
        # (open space), the target goes there, fire_dist ahead of it
        back = [-x for x in getattr(self, 'moved', {}).get(s1, [])]
        if back and math.hypot(back[0], back[2]) > 1.0:
            n = math.hypot(back[0], back[2])
            fwd = (back[0] / n, 0.0, back[2] / n)
        else:
            yaw = math.radians(p1['ang'][1])
            fwd = (math.sin(yaw), 0.0, math.cos(yaw))
        # yaw 90 faces -x (seen in the move step): forward is (-sin(yaw), 0, -cos(yaw))
        yaw = math.degrees(math.atan2(-fwd[0], -fwd[2]))
        at = tuple(p + a.fire_dist * f for p, f in zip(p1['pos'], fwd))
        self.m.tool('cmd', limit=30, name=c1, lines="Scrap.GetEntity('%s').Ang = (0.0, %r, 0.0)\n"
                    "Scrap.GetEntity('%s').Vel = (0.0, 0.0, 0.0)" % (s1, yaw, s1))
        self.m.tool('cmd', limit=30, name=c2, lines="Scrap.GetEntity('%s').Pos = %r\nScrap.GetEntity('%s').Ang = "
                    "(0.0, %r, 0.0)\nScrap.GetEntity('%s').Vel = (0.0, 0.0, 0.0)" % (s2, at, s2, yaw, s2))
        self.m.tool('cmd', limit=30, name=self.server, lines="Scrap.GetEntity('%s').Life = 1" % s2)
        # the server takes a placed position from the client a moment later (seen after a key or within about 2 s):
        # a key that does not move the ship (shift, BockStrafe) until the server has both where they were put
        end = time.time() + 10
        while True:
            self.m.tool('hold', limit=30, names=[c1, c2], keys=['shift'], seconds=0.2)
            srv = json.loads(self.m.tool('state', name=self.server, limit=30))
            gap = dist(srv['players'][s1]['pos'], srv['players'][s2]['pos'])
            if abs(gap - a.fire_dist) < 500 or time.time() > end:
                break
            time.sleep(0.5)
        self.m.tool('hold', limit=a.fire_secs + 30, name=c1, keys=['ctrl'], seconds=a.fire_secs)
        end = time.time() + 10
        score = score0
        while time.time() < end:
            srv = json.loads(self.m.tool('state', name=self.server, limit=30))
            score = srv['scores'].get(s1, 0)
            if score > score0:
                break
            time.sleep(0.5)
        self.log.append(('fire', srv))
        return self.step('fire', score > score0, '%s (client %s) fired at %s, %.0f apart on the server: server score '
                         'of %s %s -> %s, life of %s %s' % (s1, c1, s2, gap, s1, score0, score, s2,
                                                           srv['players'].get(s2, {}).get('life')), srv)

    def s_score(self):
        time.sleep(self.a.settle)
        st = self.states()
        self.log.append(('score', st))
        srv = st[0].get('scores')
        bad = ['%s has %r' % (s['name'], s.get('scores')) for s in st[1:] if s.get('scores') != srv]
        return self.step('score', not bad and srv is not None, '; '.join(bad) or 'all %d sides: %r' % (len(st), srv),
                         st)

    def s_bots(self):
        st0 = self.states()
        time.sleep(2.0)
        st1 = self.states()
        self.log.append(('bots', [st0, st1]))
        bots = sorted(n for n in st1[0]['players'] if int(n[3:]) >= 256)
        bad = []
        if len(bots) < self.a.bots:
            bad.append('the server has %d bots of %d' % (len(bots), self.a.bots))
        moving = [b for b in bots if b in st0[0]['players'] and
                  dist(st0[0]['players'][b]['pos'], st1[0]['players'][b]['pos']) > 50]
        if not moving:
            bad.append('no bot moved on the server in 2 s')
        for s in st1[1:]:
            miss = [b for b in bots if b not in s['players']]
            if miss:
                bad.append('%s misses %s' % (s['name'], miss))
        return self.step('bots', not bad, '; '.join(bad) or '%d bots on all sides, %d moving on the server' % (
            len(bots), len(moving)), [st0, st1])


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--clients', type=int, default=3)
    ap.add_argument('--mode', default='DeathMatch')
    ap.add_argument('--map', default='FZ')
    ap.add_argument('--port', type=int, help='default: the first free UDP port of 29100-29199')
    ap.add_argument('--slots', default='qm,cm,cm2,cm3', help='server slot, then the client slots')
    ap.add_argument('--build', help='the build folder of native64 (absolute or relative to the checkout)')
    ap.add_argument('--device', default=os.environ.get('SCRAP_VK_DEVICE', '1002:164e'),
                    help='Vulkan device of the games (default SCRAP_VK_DEVICE, else the AMD 610M)')
    ap.add_argument('--host', action='store_true', help='a server that plays (-server) instead of a dedicated one')
    ap.add_argument('--bots', type=int, default=0, help='mode DeathMatchBots with this many server bots (3 or more)')
    ap.add_argument('--deterministic', action='store_true', help='all games with SCRAP_TEST_DETERMINISTIC=1')
    ap.add_argument('--steps', help='default join,move,fire,score (plus bots with --bots)')
    ap.add_argument('--fraglimit', type=int, default=20)
    ap.add_argument('--move-secs', type=float, default=2.0)
    ap.add_argument('--fire-secs', type=float, default=2.0)
    ap.add_argument('--fire-dist', type=float, default=1500.0)
    ap.add_argument('--settle', type=float, default=1.5, help='seconds after an input before the states are read')
    ap.add_argument('--min-move', type=float, default=300.0)
    ap.add_argument('--tolerance', type=float, default=600.0, help='largest client/server position difference of '
                    'a ship at rest')
    ap.add_argument('--skew', type=float, default=0.5, help='seconds of the speed of a ship added to the tolerance '
                    '(the sides are read one after the other)')
    ap.add_argument('--timeout', type=float, default=900.0, help='seconds for the whole run')
    ap.add_argument('--out', help='JSON file with the results and all states')
    a = ap.parse_args()
    if a.bots:
        a.mode = 'DeathMatchBots'
    steps = (a.steps or 'join,move,fire,score' + (',bots' if a.bots else '')).split(',')
    tags = a.slots.split(',')
    if len(tags) < a.clients + 1:
        ap.error('--slots names %d slots, %d needed' % (len(tags), a.clients + 1))
    mcp = Mcp(dict(os.environ))
    run = Run(a, mcp)
    done = threading.Event()

    def watchdog():
        if not done.wait(a.timeout):
            print('mpgroup: the run took longer than %.0f s, ending the games' % a.timeout, flush=True)
            mcp.p.kill()
            kill_leftovers(run.tags)
            os._exit(3)
    threading.Thread(target=watchdog, daemon=True).start()
    signal.signal(signal.SIGTERM, lambda *x: sys.exit(4))
    t0 = time.time()
    ok = False
    try:
        if a.build:
            print(mcp.tool('configure', exe='native64', build=a.build), flush=True)
        run.start()
        ok = True
        for s in steps:
            ok = getattr(run, 's_' + s)() and ok
    except Fail as e:
        run.step('error', False, str(e))
        ok = False
    finally:
        try:
            print(mcp.tool('quit', limit=120, name='all'), flush=True)
        except Fail as e:
            print('quit: %s' % e, flush=True)
        mcp.close()
        n = kill_leftovers(run.tags)
        if n:
            print('killed %d game sessions left over' % n, flush=True)
        done.set()
    summary = 'mpgroup: %s, %d/%d steps pass, %.0f s (server %s, clients %s, %s on %s)' % (
        'PASS' if ok else 'FAIL', sum(r['pass'] for r in run.results), len(run.results), time.time() - t0,
        run.server, ','.join(run.clients), a.mode, a.map)
    print(summary, flush=True)
    if a.out:
        with open(a.out, 'w') as f:
            json.dump({'summary': summary, 'args': vars(a), 'results': run.results}, f, indent=1)
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
