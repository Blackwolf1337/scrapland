#!/usr/bin/env python3
"""Records and plays the input tape of the game (include/scrap/inputtape.h) in the deterministic test mode
(SCRAP_TEST_DETERMINISTIC=1, docs/testing.md), each run with the deterministic trace (SCRAP_TEST_DET_TRACE).

  tape.py rec <level> <tape> [--secs 60] [--trace FILE]   a recording driven by the test channel (below)
  tape.py play <level> <tape> [--trace FILE]              the replay in single player
  tape.py solo <level> <tape> [--trace FILE]              the solo instance on the tape (-instance:<level> -solo:)
  tape.py cmp <trace A> <trace B>                         compares the "pl" lines of two traces (below)
  options: --build DIR (default build-linux64 of this checkout), --name NAME (the slot of the game folder under
           build-mcp, see tools/mcp/scrap_mcp.py; default tape-<command>), --vk DEVICE (default 10de:27b0), --timeout S

The single player runs are rendered games, hidden and muted in gamescope (scrap_mcp.Game); the solo instance is a
headless process without gamescope. A replay and the solo instance end by themselves at the end of the tape.

The recording runs with SCRAP_TEST_CHANNEL=step, so the clock of the game stands while the driver thinks, and the
driver is the same on every level: about --secs seconds of game time in four parts, each line of it a command file
with a pace of 0.1 s that holds input actions with "tape key" and reads the player's entity back:
  walk     forward, a turn, a jump (Walk.StrafeUp; Dtritus does not jump, the key is on the tape all the same), the
           action button
  board    to the player's parked ship (PlayerShip, else the nearest ship whose name starts with Parked) and the use
           button until the player's entity is a ship (the boarding of the scripts with its camera path)
  fly      up, forward, a turn, the fire button, back
  park     down onto the nearest parking box (ParkParking_*) slowly, until the player is on foot again (the scripts'
           parking), then a walk until the time is over
A part that does not reach its goal in its time goes on to the next; the tape holds what happened.

cmp takes the "pl" lines (the state of the player's entity after each of its slot ticks) of two traces without their
frame numbers (single player and the instance step in other frames) and prints the first line that differs, or
"equal: <n> slots", and then per field (the kind and type, pos, ang, vel, view, life, act, money, alarm) the first slot
where it differs. Exit code 0 when equal."""
import argparse
import math
import os
import re
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(REPO, 'tools', 'mcp'))
os.environ.setdefault('SCRAP_MCP_ROOT', REPO)
import channel  # noqa: E402
import scrap_mcp  # noqa: E402

PACE = 0.1


class Rec:
    """the driver of a recording: one command file per step of PACE s of game time"""

    def __init__(self, game, secs):
        self.game, self.secs = game, secs
        self.t = 0.0
        self.held = {}
        self.log = []

    def step(self, keys=None, lines=(), n=1):
        """holds keys ({action: value}, the others let go) for n steps; the answers of lines of the last step"""
        keys = keys or {}
        cmds = ['tape key %s %g' % (k, v) for k, v in keys.items() if self.held.get(k) != v]
        cmds += ['tape key %s 0' % k for k in self.held if k not in keys]
        self.held = dict(keys)
        out = None
        for i in range(n):
            body = cmds + ['tape'] + list(lines) if i == 0 else ['tape'] + list(lines)
            out = channel.send(self.game, '\n'.join(body), timeout=60, pace=PACE).split('\n')
            self.t += PACE
        self.state = out[len(out) - len(lines) - 1] if out else ''
        return out[len(out) - len(lines):] if lines else []

    def player(self):
        m = re.search(r' player (\S+)$', self.state)
        return m.group(1) if m else '-'

    def set(self):
        m = re.search(r' set (\d+) ', self.state)
        return int(m.group(1)) if m else -1

    def ent(self, name, *vars):
        """the variables of an entity (None for one it does not have)"""
        r = self.step(self.held, ['ent %s %s' % (name, ' '.join(vars))])[0]
        out = {}
        for v in vars:
            m = re.search(r"\('%s', (\([^)]*\)|[^)]*)\)" % v, r)
            out[v] = None if not m or m.group(1) == "'?'" else eval(m.group(1))
        return out

    def ents(self, lst):
        r = self.step(self.held, ['ents %s' % lst])[0]
        try:
            return eval(r)
        except Exception:
            return []

    def done(self):
        return self.t >= self.secs


def yaw_to(src, dst):
    """the yaw in degrees from src to dst (the game's: 0 looks to -z, forward is (-sin yaw, 0, -cos yaw), the angle
    grows with Walk.TurnRight)"""
    return math.degrees(math.atan2(-(dst[0] - src[0]), -(dst[2] - src[2]))) % 360.0


def turn_err(want, have):
    return (want - have + 180.0) % 360.0 - 180.0


def walk_to(r, me, target, secs, use=False):
    """turns and walks the character me to target (x, y, z) for at most secs; with use the use button every 0.6 s
    from 1200 away; 1 when it got there (or, with use, the action set is no longer Walk)"""
    end = r.t + secs
    while r.t < end and not r.done() and r.set() == 3:
        e = r.ent(me, 'Pos', 'ViewAng')
        if not e['Pos'] or not e['ViewAng']:
            return 0
        err = turn_err(yaw_to(e['Pos'], target), e['ViewAng'][1])
        d = math.hypot(target[0] - e['Pos'][0], target[2] - e['Pos'][2])
        if d < 450 and not use:
            return 1
        keys = {}
        if abs(err) > 4:
            keys['Walk.TurnRight' if err > 0 else 'Walk.TurnLeft'] = min(1.0, abs(err) / 45.0)
        if d > 450 and abs(err) < 60:
            keys['Walk.Forward'] = 1 if d > 900 else 0.6
        if use and d < 1200 and int(round(r.t / PACE)) % 6 < 2:
            keys['Walk.Use'] = 1
        r.step(keys)
    return r.set() != 3


def fly_to(r, ship, target, secs, land):
    """flies the ship over target (x, y, z) with the thrust, strafe and vertical strafe keys in the ship's own frame
    (forward (-sin yaw, -cos yaw), left (cos yaw, -sin yaw) on the ground), the height of target + 1500 until it is
    above it; with land it goes down onto it then; 1 when the action set is no longer Racer (the ship parked) or, without
    land, it is there"""
    end = r.t + secs
    while r.t < end and not r.done() and r.set() == 1:
        e = r.ent(ship, 'Pos', 'Ang', 'Vel')
        if not e['Pos'] or not e['Ang']:
            return 0
        v = e['Vel'] or (0.0, 0.0, 0.0)
        dx, dy, dz = (target[i] - e['Pos'][i] for i in range(3))
        yaw = math.radians(e['Ang'][1])
        fwd = -math.sin(yaw) * dx - math.cos(yaw) * dz
        left = math.cos(yaw) * dx - math.sin(yaw) * dz
        vf = -math.sin(yaw) * v[0] - math.cos(yaw) * v[2]
        vl = math.cos(yaw) * v[0] - math.sin(yaw) * v[2]
        flat = math.hypot(fwd, left)
        if flat < 200 and not land and math.hypot(vf, vl) < 300:
            return 1
        keys = {}
        cf = max(-0.6, min(0.6, fwd / 6000.0 - vf / 3000.0))
        cl = max(-1.0, min(1.0, left / 2000.0 - vl / 1000.0))
        if cf > 0.03:
            keys['Racer.TrustUp'] = cf
        elif cf < -0.03:
            keys['Racer.Backward'] = -cf
        if cl > 0.05:
            keys['Racer.StrafeLeft'] = cl
        elif cl < -0.05:
            keys['Racer.StrafeRight'] = -cl
        cy = (dy + (0.0 if land and flat < 250 else 1500.0)) / 1000.0 - v[1] / 1500.0
        if cy > 0.05:
            keys['Racer.StrafeUp'] = min(1.0, cy)
        elif cy < -0.05:
            keys['Racer.StrafeDown'] = min(1.0, -cy)
        r.step(keys)
    return r.set() != 1


def drive(r, level):
    """the four parts on the level; a list of what happened"""
    # walk
    r.step({'Walk.Forward': 1}, n=20)
    r.step({'Walk.Forward': 1, 'Walk.StrafeUp': 1}, n=5)
    r.step({'Walk.Forward': 1, 'Walk.TurnRight': 0.6}, n=15)
    r.step({'Walk.Forward': 0.6, 'Walk.TurnLeft': 0.6}, n=15)
    r.step({'Walk.Action': 1}, n=3)
    r.step({}, n=5)
    me = r.player()
    r.log.append('walk done at %.1f s, player %s' % (r.t, me))
    # board: the player's own ship or the nearest parked one
    pos = r.ent(me, 'Pos')['Pos']
    best = None
    # around the player only: "ents" over the whole world loops in some levels (Police)
    near = r.ents('Vehicles %g %g %g 30000' % tuple(pos)) if pos else []
    for n in [n for n in near if n == 'PlayerShip' or n.startswith('Parked')]:
        p = r.ent(n, 'Pos')['Pos']
        if p and pos:
            d = math.dist(pos, p) - (1e6 if n == 'PlayerShip' else 0)
            if best is None or d < best[1]:
                best = (n, d, p)
    if best:
        walk_to(r, me, best[2], 25.0, use=True)
    # the boarding scene runs with the action set Inactive: wait for the ship's
    end = r.t + (12.0 if best else 0.0)
    while r.t < end and r.set() != 1 and not r.done():
        r.step({}, n=2)
    r.log.append('board %s at %.1f s, player %s, set %d' % (best[0] if best else '-', r.t, r.player(), r.set()))
    if r.set() == 1:
        # fly
        ship = r.player()
        home = r.ent(ship, 'Pos')['Pos']
        r.step({'Racer.StrafeUp': 1}, n=15)
        r.step({'Racer.TrustUp': 0.3}, n=20)
        r.step({'Racer.Right': 0.5}, n=10)
        r.step({'Racer.Fire': 1}, n=10)
        r.step({'Racer.Up': 0.4}, n=3)
        r.step({'Racer.Down': 0.4}, n=3)
        r.step({'Racer.TrustUp': 0.2, 'Racer.Fire': 1}, n=10)
        r.step({'Racer.Backward': 0.6}, n=10)
        r.log.append('fly done at %.1f s, player %s' % (r.t, r.player()))
        # park: back over the place the ship stood and down onto it (the scripts' parking lets the player out)
        if home:
            fly_to(r, ship, home, 20.0, land=False)
            fly_to(r, ship, home, 15.0, land=True)
        end = r.t + 8.0
        while r.t < end and r.set() != 3 and not r.done():
            r.step({}, n=2)
        r.log.append('park at %.1f s, player %s, set %d' % (r.t, r.player(), r.set()))
    # the rest of the time
    while not r.done():
        if r.set() == 3:
            r.step({'Walk.Forward': 1, 'Walk.TurnLeft': 0.3}, n=5)
        else:
            r.step({}, n=5)
    r.step({}, n=1)
    r.log.append('end at %.1f s, player %s, set %d' % (r.t, r.player(), r.set()))


PL_FIELDS = ('kind', 'pos', 'ang', 'vel', 'view', 'life', 'act', 'money', 'alarm')


def pl_lines(path):
    out = []
    with open(path, errors='replace') as f:
        for line in f:
            p = line.split(' ', 2)
            if len(p) == 3 and p[1] == 'pl':
                out.append(p[2].rstrip('\n'))
    return out


def pl_fields(line):
    """the fields of a pl line body (after "pl ") by name"""
    w = line.split()
    out = {'slot': w[0], 'kind': ' '.join(w[1:3])}
    i = 3
    while i < len(w):
        j = i + 1
        while j < len(w) and w[j] not in PL_FIELDS:
            j += 1
        out[w[i]] = ' '.join(w[i + 1:j])
        i = j
    return out


def compare(a_path, b_path):
    a, b = pl_lines(a_path), pl_lines(b_path)
    i = 0
    while i < min(len(a), len(b)) and a[i] == b[i]:
        i += 1
    if i == len(a) == len(b):
        print('equal: %d slots' % i)
        return 0
    print('first difference at slot %d (of %d and %d)' % (i, len(a), len(b)))
    print('  A: pl %s' % (a[i] if i < len(a) else '-'))
    print('  B: pl %s' % (b[i] if i < len(b) else '-'))
    for f in PL_FIELDS:
        for k in range(min(len(a), len(b))):
            fa, fb = pl_fields(a[k]).get(f), pl_fields(b[k]).get(f)
            if fa != fb:
                print('  %-6s first differs at slot %d: %s | %s' % (f, k, fa, fb))
                break
        else:
            print('  %-6s equal' % f)
    return 1


def env_for(a, trace):
    env = {'SCRAP_TEST_DETERMINISTIC': '1', 'SCRAP_VK_DEVICE': a.vk}
    if trace:
        env['SCRAP_TEST_DET_TRACE'] = os.path.abspath(trace)
    return env


def native_done(g):
    """the game's process is gone (gamescope may hang on its way out)"""
    return not g.native_pids()[0]


def run_game(a, args, env, body=None):
    scrap_mcp.NATIVE_BUILDS['native64'] = os.path.abspath(a.build)
    g = scrap_mcp.Game(a.name)
    scrap_mcp.GAMES[a.name] = g
    g.start({'name': a.name, 'exe': 'native64', 'map': 'Levels/' + a.level, 'env': env, 'args': args,
             'timeout': a.timeout})
    try:
        if body:
            body(g)
        end = time.time() + a.timeout
        while time.time() < end and g.running() and not (time.time() > g.t0 + 5 and native_done(g)):
            time.sleep(0.5)
    finally:
        g.stop()
    return g


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('cmd', choices=('rec', 'play', 'solo', 'cmp'))
    ap.add_argument('level', help='the level (cmp: trace A)')
    ap.add_argument('tape', help='the tape (cmp: trace B)')
    ap.add_argument('--secs', type=float, default=60.0)
    ap.add_argument('--trace')
    ap.add_argument('--build', default=os.path.join(REPO, 'build-linux64'))
    ap.add_argument('--name')
    ap.add_argument('--vk', default='10de:27b0')
    ap.add_argument('--timeout', type=float, default=1800.0)
    a = ap.parse_args()
    if a.cmd == 'cmp':
        return compare(a.level, a.tape)
    a.name = a.name or 'tape-' + a.cmd
    tape = os.path.abspath(a.tape)
    env = env_for(a, a.trace)
    if a.cmd == 'rec':
        env['SCRAP_TEST_CHANNEL'] = 'step'
        result = {}

        def body(g):
            g.t0 = time.time()
            channel.wait_ready(g.game, 300, alive=g.running)
            r = Rec(g.game, a.secs)
            try:
                drive(r, a.level)
            finally:
                result['log'] = r.log
                channel.send(g.game, 'tape end', timeout=60)
        run_game(a, ['-inputtape:rec:' + tape], env, body)
        print('\n'.join(result.get('log', [])))
        return 0
    if a.cmd == 'play':
        env['SCRAP_TEST_CHANNEL'] = '0'

        def body(g):
            g.t0 = time.time()
        run_game(a, ['-inputtape:play:' + tape], env, body)
        return 0
    # solo: the headless instance in the game folder of the slot, without gamescope
    scrap_mcp.NATIVE_BUILDS['native64'] = os.path.abspath(a.build)
    g = scrap_mcp.Game(a.name)
    g.prepare('native64')
    full = dict(os.environ, **env, SCRAP_TEST_CHANNEL='0', SCRAP_SND_NULL='1', SCRAP_NO_MSGBOX='1',
                XDG_DATA_HOME=os.path.join(g.game, 'xdg-data'))
    log = open(os.path.join(g.dir, 'solo.log'), 'w')
    p = subprocess.run(['timeout', '-k', '3', str(int(a.timeout)), './ScrapC', '-instance:' + a.level, '-solo:' + tape],
                       cwd=os.path.join(g.game, 'Bin'), env=full, stdin=subprocess.DEVNULL, stdout=log,
                       stderr=subprocess.STDOUT)
    return p.returncode


if __name__ == '__main__':
    sys.exit(main())
