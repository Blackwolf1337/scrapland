#!/usr/bin/env python3
"""Records and plays the input tape of the game (include/scrap/inputtape.h) in the deterministic test mode
(SCRAP_TEST_DETERMINISTIC=1, docs/testing.md), each run with the deterministic trace (SCRAP_TEST_DET_TRACE).

  tape.py rec <level> <tape> [--secs 60] [--trace FILE]   a recording driven by the test channel (below)
  tape.py play <level> <tape> [--trace FILE]              the replay in single player
  tape.py solo <level> <tape> [--trace FILE]              the solo instance on the tape (-instance:<level> -solo:)
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
A part that does not reach its goal in its time goes on to the next; the tape holds what happened."""
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
    """the yaw in degrees (the game's: 0 looks to -z, the angle grows to the left) from src to dst"""
    return math.degrees(math.atan2(-(dst[0] - src[0]), -(dst[2] - src[2]))) % 360.0


def turn_err(want, have):
    return (want - have + 180.0) % 360.0 - 180.0


def drive(r, level):
    """the four parts on the level; a list of what happened"""
    # walk
    r.step({'Walk.Forward': 1}, n=15)
    r.step({'Walk.Forward': 1, 'Walk.StrafeUp': 1}, n=3)
    r.step({'Walk.Forward': 1, 'Walk.TurnRight': 0.6}, n=8)
    r.step({'Walk.TurnLeft': 0.6}, n=8)
    r.step({'Walk.Action': 1}, n=2)
    r.step({}, n=3)
    r.log.append('walk done at %.1f s, player %s' % (r.t, r.player()))
    # board
    me = r.player()
    ships = [n for n in r.ents('Vehicles') if n == 'PlayerShip' or n.startswith('Parked')]
    best = None
    pos = r.ent(me, 'Pos')['Pos']
    for n in ships:
        p = r.ent(n, 'Pos')['Pos']
        if p:
            d = math.dist(pos, p)
            if best is None or (n == 'PlayerShip' and d < 20000) or (best[0] != 'PlayerShip' and d < best[1]):
                best = (n, d, p)
    end = r.t + 25.0
    while best and r.t < end and not r.done() and r.set() == 3:
        e = r.ent(me, 'Pos', 'ViewAng')
        if not e['Pos'] or not e['ViewAng']:
            break
        err = turn_err(yaw_to(e['Pos'], best[2]), e['ViewAng'][1])
        d = math.hypot(best[2][0] - e['Pos'][0], best[2][2] - e['Pos'][2])
        keys = {}
        if abs(err) > 4:
            keys['Walk.TurnLeft' if err > 0 else 'Walk.TurnRight'] = min(1.0, abs(err) / 45.0)
        if d > 450 and abs(err) < 60:
            keys['Walk.Forward'] = 1 if d > 900 else 0.6
        if d < 1200 and int(r.t / PACE) % 6 < 2:
            keys['Walk.Use'] = 1
        r.step(keys, n=1)
    # the boarding scene runs with the action set Inactive or none: wait for the ship's set
    end = r.t + 12.0
    while r.t < end and r.set() != 1 and not r.done():
        r.step({}, n=2)
    r.log.append('board %s at %.1f s, player %s, set %d' % (best[0] if best else '-', r.t, r.player(), r.set()))
    if r.set() == 1:
        # fly
        ship = r.player()
        r.step({'Racer.StrafeUp': 1}, n=15)
        r.step({'Racer.TrustUp': 1}, n=20)
        r.step({'Racer.TrustUp': 1, 'Racer.Right': 0.7}, n=12)
        r.step({'Racer.Fire': 1}, n=6)
        r.step({'Racer.TrustUp': 1, 'Racer.Fire': 1}, n=6)
        r.step({'Racer.Backward': 1}, n=15)
        r.log.append('fly done at %.1f s, player %s' % (r.t, r.player()))
        # park: the nearest parking box, with the strafe and thrust keys in the ship's own frame
        boxes = [n for n in r.ents('Vehicles') if n.startswith('ParkParking_')]
        p = r.ent(ship, 'Pos')['Pos']
        target = None
        for n in boxes:
            q = r.ent(n, 'Pos')['Pos']
            if q and (target is None or math.dist(p, q) < math.dist(p, target[1])):
                target = (n, q)
        end = r.t + 30.0
        while target and r.t < end and r.set() == 1 and not r.done():
            e = r.ent(ship, 'Pos', 'Ang', 'Vel')
            if not e['Pos'] or not e['Ang']:
                break
            dx, dy, dz = (target[1][i] - e['Pos'][i] for i in range(3))
            yaw = math.radians(e['Ang'][1])
            # forward (-sin yaw, -cos yaw) and left (-cos yaw, sin yaw) on the ground
            fwd = -math.sin(yaw) * dx - math.cos(yaw) * dz
            left = -math.cos(yaw) * dx + math.sin(yaw) * dz
            v = e['Vel'] or (0, 0, 0)
            vf = -math.sin(yaw) * v[0] - math.cos(yaw) * v[2]
            vl = -math.cos(yaw) * v[0] + math.sin(yaw) * v[2]
            keys = {}
            cf = fwd / 3000.0 - vf / 1500.0
            cl = left / 3000.0 - vl / 1500.0
            if cf > 0.05:
                keys['Racer.TrustUp'] = min(1.0, cf)
            elif cf < -0.05:
                keys['Racer.Backward'] = min(1.0, -cf)
            if cl > 0.05:
                keys['Racer.StrafeLeft'] = min(1.0, cl)
            elif cl < -0.05:
                keys['Racer.StrafeRight'] = min(1.0, -cl)
            flat = math.hypot(fwd, left)
            cy = (dy + (0 if flat < 1500 else 1500)) / 2000.0 - v[1] / 1500.0
            if cy > 0.05:
                keys['Racer.StrafeDown'] = min(1.0, cy)
            elif cy < -0.05:
                keys['Racer.StrafeUp'] = min(1.0, -cy)
            r.step(keys, n=1)
        r.log.append('park %s at %.1f s, player %s, set %d' % (target[0] if target else '-', r.t, r.player(),
                                                               r.set()))
    # the rest of the time on foot (or wherever the player is)
    while not r.done():
        if r.set() == 3:
            r.step({'Walk.Forward': 1, 'Walk.TurnLeft': 0.3}, n=5)
        else:
            r.step({}, n=5)
    r.step({}, n=1)
    r.log.append('end at %.1f s, player %s, set %d' % (r.t, r.player(), r.set()))


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
    ap.add_argument('cmd', choices=('rec', 'play', 'solo'))
    ap.add_argument('level')
    ap.add_argument('tape')
    ap.add_argument('--secs', type=float, default=60.0)
    ap.add_argument('--trace')
    ap.add_argument('--build', default=os.path.join(REPO, 'build-linux64'))
    ap.add_argument('--name')
    ap.add_argument('--vk', default='10de:27b0')
    ap.add_argument('--timeout', type=float, default=1800.0)
    a = ap.parse_args()
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
