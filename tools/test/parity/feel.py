#!/usr/bin/env python3
"""The feel tool of the free roam multiplayer (stage M10 of issue #1): input-to-screen latency, frame times,
prediction corrections and visible jitter of the own character, the own ship and the camera, walking and flying,
in single player and on multiplayer clients, measured on rendered games.

A side is one of
  sp        single player (-imap:Levels/<level>)
  host      a hosting game from the command line (-host:<level> -mode:freeroam)
  menuhost  a game that hosts from the menus (Multiplayer, Free roam, Host world, mode free roam, the Host button
            selected with the arrow keys and clicked with the real left mouse button through XTEST)
  join      a client that joins a host world (a second rendered game hosts it)
The game writes the feel log (SCRAP_FEELLOG, include/scrap/feellog.h: a line per drawn frame with the monotonic
clock); real key presses go through XTEST into the game's own gamescope X server (never the desktop), their time
taken on the same clock. Per side: does the player start on foot (and stay so after a click of the use button,
the left mouse button, at nothing); walking: W and S held 3 s in turn, --walks times (12); then the ship (single player:
Parking.UseShip on the player's ship; a client: F1, the mission info key, the sandbox's switch),
put 15000 units up, W held 6 s (5 s with --no-boost), three times. The results go to --out/<side>.json and the table to --out/feel.md:
  latency    from the key press to the first drawn frame where the own entity left its path (median, max)
  input      from the key press to the first frame that saw the input
  frames     frame time percentiles and a histogram over the whole run
  jitter     RMS distance of the drawn positions (entity, camera) from a local quadratic fit over 9 frames while the
             key is held (after the first second): what the eye sees as shaking
  corrections  prediction corrections while walking or flying, and per minute over the whole run (corr_per_min)
  boost      the action button: in each flight the boost (right mouse button) 3 s after W, its latency to the first
             frame off the path (the threshold four times the shake of the path before the press)
--delay, --jitter and --loss set SCRAP_NET2_DELAY (ms each way), SCRAP_NET2_JITTER (ms) and SCRAP_NET2_LOSS (percent)
on the client and the server of a multiplayer side (tools/test/mp2.sh feel runs the matrix of them); --cell names the
JSON files <side>-<cell>.json.

  feel.py [--sides sp,host,join,menuhost] [--level DownTown] [--port 29960] [--build DIR] [--out DIR] [--scratch DIR]
          [--delay MS] [--jitter MS] [--loss PCT] [--cell NAME] [--no-boost]
"""
import argparse
import json
import os
import re
import statistics
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(REPO, 'tools', 'mcp'))
import parity  # noqa: E402
import scrap_mcp  # noqa: E402

FRAME_BUCKETS = (8, 12, 17.5, 20, 25, 34, 50, 1e9)
BUMP_AHEAD = 1.5                   # s the rail traffic ship needs to the place the own ship is put


class XGame:
    """what scrap_mcp.game_display needs of a game"""

    def __init__(self, g):
        self.game, self.name = g.dir, g.name

    def running(self):
        return True


def press(g, key, hold):
    """the key held for hold seconds; the monotonic time of the press (ns)"""
    with scrap_mcp.XInput(XGame(g)) as x:
        code = x.keycode(key)
        t0 = time.monotonic_ns()
        x.xtst.XTestFakeKeyEvent(x.dpy, code, 1, 0)
        x.x11.XSync(x.dpy, 0)
        time.sleep(hold)
        x.xtst.XTestFakeKeyEvent(x.dpy, code, 0, 0)
        x.x11.XSync(x.dpy, 0)
    return t0


def press_with(g, key, hold, button, at, bhold):
    """the key held for hold seconds, the mouse button pressed at seconds after it for bhold seconds (an action while
    moving); the monotonic times of both presses (ns)"""
    with scrap_mcp.XInput(XGame(g)) as x:
        code = x.keycode(key)
        t0 = time.monotonic_ns()
        x.xtst.XTestFakeKeyEvent(x.dpy, code, 1, 0)
        x.x11.XSync(x.dpy, 0)
        time.sleep(at)
        t1 = time.monotonic_ns()
        x.xtst.XTestFakeButtonEvent(x.dpy, button, 1, 0)
        x.x11.XSync(x.dpy, 0)
        time.sleep(bhold)
        x.xtst.XTestFakeButtonEvent(x.dpy, button, 0, 0)
        x.x11.XSync(x.dpy, 0)
        time.sleep(max(0.0, hold - at - bhold))
        x.xtst.XTestFakeKeyEvent(x.dpy, code, 0, 0)
        x.x11.XSync(x.dpy, 0)
    return t0, t1


def key(g, name, hold=0.08):
    with scrap_mcp.XInput(XGame(g)) as x:
        x.key(name, hold)
    time.sleep(0.4)


def click(g, button=1, hold=0.12):
    """a mouse button pressed for hold seconds"""
    with scrap_mcp.XInput(XGame(g)) as x:
        x.button(button, hold)


def read_log(path):
    rows = []
    try:
        with open(path) as f:
            next(f, None)
            for line in f:
                p = line.rstrip('\n').split(',')
                if len(p) < 12:
                    continue
                try:
                    rows.append(dict(t=int(p[0]), dt=float(p[1]), kind=p[2], pos=tuple(map(float, p[3:6])),
                                     cam=tuple(map(float, p[6:9])), yaw=float(p[9]), inp=float(p[10]),
                                     corr=int(p[11])))
                except ValueError:
                    pass
    except OSError:
        pass
    return rows


def dist(a, b):
    return sum((x - y) ** 2 for x, y in zip(a, b)) ** 0.5


def latency(rows, t0, auto=False, within=None):
    """(input seen ms, entity moved ms) after the press at t0: the first frame off the path the frames before the
    press extrapolate (their last 0.3 s, linear) by more than 1 unit; auto: by more than 4 times the largest distance
    of those frames from their own line (an action pressed while the entity already moves: the drawn path shakes by a
    few units)"""
    before = [r for r in rows if t0 - 300_000_000 <= r['t'] <= t0]
    after = [r for r in rows if r['t'] > t0 and (within is None or r['t'] <= t0 + within * 1e9)]
    if len(before) < 2 or not after:
        return None, None
    a, b = before[0], before[-1]
    span = (b['t'] - a['t']) or 1
    vel = [(y - x) / span for x, y in zip(a['pos'], b['pos'])]
    limit = 1.0
    if auto:
        res = max(dist(r['pos'], tuple(p + v * (r['t'] - a['t']) for p, v in zip(a['pos'], vel))) for r in before)
        limit = max(1.0, 4.0 * res)
    seen = moved = None
    for r in after:
        if seen is None and abs(r['inp']) > 0.01:
            seen = (r['t'] - t0) / 1e6
        pred = tuple(p + v * (r['t'] - b['t']) for p, v in zip(b['pos'], vel))
        if moved is None and dist(r['pos'], pred) > limit:
            moved = (r['t'] - t0) / 1e6
        if seen is not None and moved is not None:
            break
    return seen, moved


def jitter(rows, key):
    """RMS of the distance of each drawn point from a quadratic fit (by time) through the 9 frames around it"""
    res = []
    for i in range(4, len(rows) - 4):
        w = rows[i - 4:i + 5]
        t = [(r['t'] - rows[i]['t']) / 1e9 for r in w]
        err2 = 0.0
        for k in range(3):
            y = [r[key][k] for r in w]
            # least squares y = c0 + c1 t + c2 t^2 at t = 0 (the middle frame)
            n = len(t)
            s = [sum(x ** e for x in t) for e in range(5)]
            sy = [sum(yy * x ** e for yy, x in zip(y, t)) for e in range(3)]
            m = [[s[0], s[1], s[2]], [s[1], s[2], s[3]], [s[2], s[3], s[4]]]
            c0 = solve3(m, sy)
            if c0 is None or n < 3:
                continue
            err2 += (y[4] - c0) ** 2
        res.append(err2 ** 0.5)
    if not res:
        return None
    res.sort()
    return {'rms': round((sum(x * x for x in res) / len(res)) ** 0.5, 3), 'p95': round(res[int(len(res) * 0.95)], 3),
            'max': round(res[-1], 3)}


def solve3(m, y):
    """c0 of the 3x3 system m c = y (Cramer)"""
    def det(a):
        return (a[0][0] * (a[1][1] * a[2][2] - a[1][2] * a[2][1]) - a[0][1] * (a[1][0] * a[2][2] - a[1][2] * a[2][0]) +
                a[0][2] * (a[1][0] * a[2][1] - a[1][1] * a[2][0]))
    d = det(m)
    if abs(d) < 1e-18:
        return None
    m0 = [[y[0], m[0][1], m[0][2]], [y[1], m[1][1], m[1][2]], [y[2], m[2][1], m[2][2]]]
    return det(m0) / d


def window(rows, t0, a, b):
    return [r for r in rows if t0 + a * 1e9 <= r['t'] <= t0 + b * 1e9]


def frames(rows):
    dts = sorted(r['dt'] for r in rows if r['dt'] > 0)
    if not dts:
        return None
    q = lambda p: round(dts[min(len(dts) - 1, int(len(dts) * p))], 2)  # noqa: E731
    hist, lo = [], 0
    for hi in FRAME_BUCKETS:
        hist.append(sum(1 for d in dts if lo <= d < hi))
        lo = hi
    return {'n': len(dts), 'p50': q(0.5), 'p95': q(0.95), 'p99': q(0.99), 'max': round(dts[-1], 2),
            'fps': round(1000.0 / statistics.mean(dts), 1), 'hist': hist}


def own_kind(rows, t):
    k = [r['kind'] for r in rows if r['t'] <= t]
    return k[-1] if k else '-'


def run_side(a, side):
    res = {'side': side, 'level': a.level, 'cell': a.cell}
    log = os.path.join(a.scratch, 'feel-%s%s.csv' % (side, '-' + a.cell if a.cell else ''))
    res['net'] = dict(a.net) if side != 'sp' else {}
    if os.path.exists(log):
        os.remove(log)
    env = dict(a.net if side != 'sp' else {}, SCRAP_FEELLOG=log)
    if a.playlog and side != 'sp':
        env['SCRAP_PLAYLOG'] = os.path.join(a.scratch, 'play-%s.csv' % side)
    port = a.port
    hostg = None
    v2 = ['-mp2'] if a.mp2 else []
    if side == 'sp':
        g = parity.Game(a, 'fs', ['-imap:Levels/' + a.level], env)
    elif side == 'host':
        g = parity.Game(a, 'fh', ['-host:' + a.level, '-port:%d' % port, '-mode:freeroam', '-name:Host'] + v2, env)
    elif side == 'menuhost':
        g = parity.Game(a, 'fm', [], env)
    else:
        hostg = parity.Game(a, 'fo', ['-host:' + a.level, '-port:%d' % port, '-mode:freeroam', '-name:Host'] + v2,
                            dict(a.net))
        g = parity.Game(a, 'fj', ['-join:127.0.0.1', '-port:%d' % port, '-name:Join'] + v2, env)
    inst_dir = hostg or g
    try:
        if hostg:
            hostg.start()
            parity.wait_for(lambda: parity.own_entity(parity.parse_ents(hostg.try_ask('entinfo', timeout=5))), 240, 2)
        g.start()
        if side == 'menuhost':
            parity.wait_for(lambda: g.try_ask('Scrap.GetLevelPath()', timeout=5), 180, 2)
            time.sleep(2)
            g.ask(["do PCMenu.MultiPlayerMenu(0, '')", 'click FreeRoam', 'click WorldHost'])
            time.sleep(1.5)
            # the mode to free roam: the page cycles its modes with the mode button
            for _ in range(10):
                if 'Free roam' in (g.ask('menu') or ''):
                    break
                g.ask('click HostMode')
                time.sleep(0.3)
            # the Host button selected with the keys (up twice from the page's tab) and clicked with the real left
            # mouse button, the use button of a character
            g.ask('click WorldHost')
            time.sleep(0.5)
            for k in ('Up', 'Up'):
                key(g, k)
            res['host_selected'] = (g.ask('menu') or '').startswith("('HostButton'")
            res['host_click'] = time.monotonic_ns()
            click(g, 1, 0.12)
        if side == 'sp':
            parity.wait_for(lambda: g.try_ask('Scrap.GetLevelPath()', timeout=5), 180, 2)
            me = 'Player0'
        else:
            own = parity.wait_for(lambda: parity.own_entity(parity.parse_ents(g.try_ask('entinfo', timeout=5))),
                                  240, 2)
            if not own:
                res['error'] = 'no own entity'
                return res
            me = own['name']
        res['own'] = me
        time.sleep(6)
        rows = read_log(log)
        res['start_kind'] = own_kind(rows, time.monotonic_ns())
        res['start_on_foot'] = res['start_kind'] == 'C'
        if side != 'sp':
            # a click of the use button where the arrow is on nothing: single player does nothing, the sandbox used to
            # board the nearest parked ship
            click(g, 1, 0.12)
            time.sleep(3)
            res['after_click_kind'] = own_kind(read_log(log), time.monotonic_ns())
        if res['start_kind'] == 'S' or res.get('after_click_kind') == 'S':
            press(g, 'F1', 0.12)
            time.sleep(3)
        walks = []
        # forwards and back in turn (six walks into one direction end at a wall); the pauses step through a 30 Hz
        # tick so the presses fall on every part of it (a press waits for the next tick or slot: the latency varies
        # by up to a tick with the moment of the press)
        for k in range(a.walks):
            time.sleep(1.0 + (k * 7 % a.walks) / a.walks / 30.0)
            walks.append(press(g, 'w' if k % 2 == 0 else 's', 3.0))
        time.sleep(1.0)
        # the ship
        if side == 'sp':
            g.ask('Parking.UseShip("Player0", "PlayerShip", "", "")')
        else:
            # the sandbox's switch (the mission info key, F1): the nearest parked ship or a called one
            press(g, 'F1', 0.12)
        ok = parity.wait_for(lambda: own_kind(read_log(log), time.monotonic_ns()) == 'S', 15, 0.5)
        res['boarded'] = bool(ok)
        flights = []
        boosts = []
        bumps = []
        if ok:
            time.sleep(3.0)
            for k in range(3):
                if k and a.boost:
                    # the boost recharges: 12 s more between the flights (the boosts come about 21 s apart)
                    time.sleep(12.0)
                lift(a, g, inst_dir, side)
                time.sleep(2.5)
                if a.boost:
                    # W held 6 s, the boost (the right mouse button: Racer.Boost) at 3 s for 0.4 s
                    t0, t1 = press_with(g, 'w', 6.0, 3, 3.0, 0.4)
                    flights.append(t0)
                    boosts.append(t1)
                else:
                    flights.append(press(g, 'w', 5.0))
                time.sleep(0.5)
            # the own ship into the path of a rail traffic ship of this game (a client's own, the server lacks it)
            for _ in range(a.bumps):
                b = bump(a, g, inst_dir, side)
                if b:
                    bumps.append(b)
        time.sleep(0.5)
        rows = read_log(log)
        res['frames'] = frames([r for r in rows if r['kind'] != '-'])
        res['walk'] = analyse(rows, walks, 3.0)
        res['flight_start'] = a.flight_start.get(side)
        res['fly'] = analyse(rows, flights, 3.0 if a.boost else 5.0)
        res['boost'] = analyse(rows, boosts, 0.4, auto=True, within=2.5)
        moving = [r for r in rows if r['kind'] != '-']
        if len(moving) > 1:
            mins = (moving[-1]['t'] - moving[0]['t']) / 60e9
            res['minutes'] = round(mins, 2)
            res['corrections'] = moving[-1]['corr'] - moving[0]['corr']
            res['corr_per_min'] = round(res['corrections'] / mins, 2) if mins > 0 else None
        res['bump'] = bump_result(rows, bumps)
    finally:
        g.stop()
        if hostg:
            hostg.stop()
    return res


def lift(a, g, inst_dir, side):
    """the own ship to the start of a flight, still: the first time 15000 units above where it is, each later time the
    same place with the same heading (a flight from where the last one ended runs into the walls; single player:
    through its channel; a client: the instance's ship of the player, nearest to the client's own entity)"""
    rows = read_log(os.path.join(a.scratch, 'feel-%s%s.csv' % (side, '-' + a.cell if a.cell else '')))
    if not rows:
        return
    p = rows[-1]['pos']
    if side == 'sp':
        target = 'Scrap.UsrEntity(0)'
        do = g.ask
    else:
        ents = parity.parse_ents(inst_dir.try_ask('entinfo %.0f %.0f %.0f 6000' % p, inst=a.level, timeout=10))
        # the ship a player drives (owner slot 4 on: the players of the instance)
        cars = sorted((e for e in ents if e['cls'] == 'Car' and e.get('owner', -1) >= 4),
                      key=lambda e: dist(e['pos'], p))
        if not cars:
            return
        target = 'Scrap.GetEntity("%s")' % cars[0]['name']

        def do(line):
            return inst_dir.ask(line, inst=a.level)
    start = a.flight_start.get(side) or a.fly_from
    if not start:
        m = re.search(r'\(([-\d.e]+), ([-\d.e]+), ([-\d.e]+)\)', do(target + '.Ang') or '')
        ang = tuple(float(x) for x in m.groups()) if m else None
        start = a.flight_start[side] = ((p[0], p[1] + 15000.0, p[2]), ang)
    a.flight_start[side] = start
    up, ang = start
    line = 'do e = %s; e.Pos = (%.1f, %.1f, %.1f); e.Vel = (0, 0, 0)' % ((target,) + up)
    if ang:
        line += '; e.Ang = (%.4f, %.4f, %.4f)' % ang
    do(line)


def bump(a, g, inst_dir, side):
    """the own ship put still where a rail traffic ship of this game will be in BUMP_AHEAD s; the window's start (ns),
    the ship's name and the own speed before, or None without one near"""
    rows = read_log(os.path.join(a.scratch, 'feel-%s%s.csv' % (side, '-' + a.cell if a.cell else '')))
    if not rows:
        return None
    p = rows[-1]['pos']
    ents = parity.parse_ents(g.try_ask('entinfo %.0f %.0f %.0f 60000' % p, timeout=10))
    rail = sorted((e for e in ents if e['list'] in parity.RAIL and e['active']), key=lambda e: dist(e['pos'], p))
    for r in rail[:8]:
        m = re.search(r"\('Vel', \(([-\d.e]+), ([-\d.e]+), ([-\d.e]+)\)\)", g.ask('ent %s Vel' % r['name']) or '')
        if not m:
            continue
        v = tuple(float(x) for x in m.groups())
        if dist(v, (0.0, 0.0, 0.0)) < 500.0:
            continue
        m = re.search(r"\('Pos', \(([-\d.e]+), ([-\d.e]+), ([-\d.e]+)\)\)", g.ask('ent %s Pos' % r['name']) or '')
        if not m:
            continue
        at = tuple(float(x) for x in m.groups())
        to = tuple(x + y * BUMP_AHEAD for x, y in zip(at, v))
        t0 = time.monotonic_ns()
        if side == 'sp':
            g.ask('do e = Scrap.UsrEntity(0); e.Pos = (%.1f, %.1f, %.1f); e.Vel = (0, 0, 0)' % to)
        else:
            iel = parity.parse_ents(inst_dir.try_ask('entinfo %.0f %.0f %.0f 6000' % p, inst=a.level, timeout=10))
            cars = sorted((e for e in iel if e['cls'] == 'Car' and e.get('owner', -1) >= 4),
                          key=lambda e: dist(e['pos'], p))
            if not cars:
                return None
            inst_dir.ask('do e = Scrap.GetEntity("%s"); e.Pos = (%.1f, %.1f, %.1f); e.Vel = (0, 0, 0)' %
                         ((cars[0]['name'],) + to), inst=a.level)
        time.sleep(BUMP_AHEAD + 3.0)
        return {'t0': t0, 'ship': r['name'], 'to': to}
    return None


def bump_result(rows, bumps):
    """per bump: the own ship hit (its drawn position left the place it was put more than 100 units within the
    window) and the corrections in the window"""
    out = {'tries': len(bumps), 'hits': 0, 'corrections': 0, 'ships': []}
    for b in bumps:
        w = [r for r in rows if b['t0'] + 300_000_000 <= r['t'] <= b['t0'] + int((BUMP_AHEAD + 3.0) * 1e9)]
        if not w:
            continue
        # the place it stood at first (the move through the instance arrives after a moment)
        still = [r for r in w if dist(r['pos'], b['to']) < 200.0]
        hit = bool(still) and any(dist(r['pos'], still[0]['pos']) > 100.0 for r in w if r['t'] > still[0]['t'])
        out['hits'] += hit
        out['corrections'] += w[-1]['corr'] - w[0]['corr']
        out['ships'].append(b['ship'])
        out.setdefault('each', []).append({'t0': b['t0'], 'hit': hit, 'corrections': w[-1]['corr'] - w[0]['corr']})
    return out


def analyse(rows, presses, hold, auto=False, within=None):
    """within: seconds after a press in which the entity must leave its path (else a miss: the boost did nothing
    while the key was held)"""
    if not presses:
        return None
    seen, moved, ent, cam, corr = [], [], [], [], 0
    for t0 in presses:
        s, m = latency(rows, t0, auto, within)
        if s is not None:
            seen.append(s)
        if m is not None:
            moved.append(m)
        w = window(rows, t0, 1.0, hold - 0.2)
        je, jc = jitter(w, 'pos'), jitter(w, 'cam')
        if je:
            ent.append(je)
        if jc:
            cam.append(jc)
        ww = window(rows, t0, 0.0, hold)
        if ww:
            corr += ww[-1]['corr'] - ww[0]['corr']
    med = lambda v: round(statistics.median(v), 1) if v else None  # noqa: E731
    mean = lambda v: round(statistics.mean(v), 1) if v else None  # noqa: E731
    return {'input_ms': med(seen), 'latency_ms': med(moved), 'latency_mean': mean(moved),
            'latency_max': round(max(moved), 1) if moved else None,
            'latency_p95': round(sorted(moved)[min(len(moved) - 1, int(len(moved) * 0.95))], 1) if moved else None,
            'latencies': [round(m, 1) for m in moved],
            'jitter_entity': max(ent, key=lambda j: j['rms']) if ent else None,
            'jitter_camera': max(cam, key=lambda j: j['rms']) if cam else None, 'corrections': corr,
            'samples': len(moved), 'misses': len(presses) - len(moved)}


def table(results, out):
    lines = ['| side | start on foot | walk input / move ms (median, mean) | walk jitter entity / camera (rms, p95) | walk corr | '
             'fly input / move ms | fly jitter entity / camera (rms, p95) | fly corr | bump hits/tries, corr | '
             'frame ms p50/p95/p99/max | fps | boost move ms (mean, p95) | corr/min |',
             '|---|---|---|---|---|---|---|---|---|---|---|---|---|']

    def j(x):
        return '%.2f/%.2f' % (x['rms'], x['p95']) if x else '-'
    for r in results:
        if 'error' in r:
            lines.append('| %s | error: %s |' % (r['side'], r['error']))
            continue
        w, f, fr = r.get('walk') or {}, r.get('fly') or {}, r.get('frames') or {}
        b = r.get('bump') or {}
        lines.append('| %s | %s (%s%s) | %s / %s, %s | %s ; %s | %s | %s / %s | %s ; %s | %s | %s | %s/%s/%s/%s | %s | %s | %s |' % (
            r['side'], 'yes' if r.get('start_on_foot') and r.get('after_click_kind', 'C') == 'C' else 'NO',
            r.get('start_kind'), ', after a click %s' % r['after_click_kind'] if 'after_click_kind' in r else '',
            w.get('input_ms'),
            w.get('latency_ms'), w.get('latency_mean'), j(w.get('jitter_entity')), j(w.get('jitter_camera')), w.get('corrections'),
            f.get('input_ms'), f.get('latency_ms'), j(f.get('jitter_entity')), j(f.get('jitter_camera')),
            f.get('corrections'), '%s/%s, %s' % (b.get('hits'), b.get('tries'), b.get('corrections')) if b else '-',
            fr.get('p50'), fr.get('p95'), fr.get('p99'), fr.get('max'), fr.get('fps'),
            '%s, %s' % ((r.get('boost') or {}).get('latency_mean'), (r.get('boost') or {}).get('latency_p95')),
            r.get('corr_per_min')))
    lines += ['', 'Frame time histogram (ms buckets <8, 8-12, 12-17.5, 17.5-20, 20-25, 25-34, 34-50, >50):', '']
    for r in results:
        if r.get('frames'):
            lines.append('- %s: %s' % (r['side'], ' '.join(str(x) for x in r['frames']['hist'])))
    with open(out, 'w') as fo:
        fo.write('\n'.join(lines) + '\n')
    return '\n'.join(lines)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--sides', default='sp,host,join,menuhost')
    ap.add_argument('--level', default='DownTown')
    ap.add_argument('--port', type=int, default=29960)
    ap.add_argument('--build', default=os.path.join(REPO, 'build-linux64'))
    ap.add_argument('--out', default=os.path.join(REPO, 'build-linux64', 'feel'))
    ap.add_argument('--scratch', default=os.path.join(REPO, 'build-linux64', 'parity-games'))
    ap.add_argument('--timeout', type=int, default=900)
    ap.add_argument('--walks', type=int, default=12)
    ap.add_argument('--bumps', type=int, default=4)
    ap.add_argument('--device', default=None)
    ap.add_argument('--playlog', action='store_true', help='the clients\' prediction logs (SCRAP_PLAYLOG) in --scratch')
    ap.add_argument('--delay', type=int, default=0, help='ms each way (SCRAP_NET2_DELAY on client and server)')
    ap.add_argument('--jitter', type=int, default=0, help='ms (SCRAP_NET2_JITTER)')
    ap.add_argument('--loss', type=float, default=0.0, help='percent (SCRAP_NET2_LOSS)')
    ap.add_argument('--cell', default='', help='a name added to the JSON files of the sides (<side>-<cell>.json)')
    ap.add_argument('--no-boost', dest='boost', action='store_false', help='flights without the boost button')
    ap.add_argument('--mp2', action='store_true', help='the multiplayer sides on the replication v2 (-mp2)')
    ap.add_argument('--fly-from', dest='fly_from_json', default=None,
                    help='the JSON of an earlier side (single player): its flight start for every flight of this run')
    ap.add_argument('--report-only', action='store_true')
    a = ap.parse_args()
    a.net = {}
    a.flight_start = {}
    a.fly_from = None
    if a.fly_from_json:
        # the flights of every side from single player's start place and heading (a client's own boarding place can
        # lead into a wall: the host side's ship stood still after 1.5 s of each flight)
        try:
            st = json.load(open(a.fly_from_json)).get('flight_start')
            a.fly_from = (tuple(st[0]), tuple(st[1]) if st[1] else None) if st else None
        except (OSError, ValueError, IndexError, TypeError):
            a.fly_from = None
    if a.delay:
        a.net['SCRAP_NET2_DELAY'] = str(a.delay)
    if a.jitter:
        a.net['SCRAP_NET2_JITTER'] = str(a.jitter)
    if a.loss:
        a.net['SCRAP_NET2_LOSS'] = '%g' % a.loss
    a.build = os.path.abspath(a.build)
    os.makedirs(a.out, exist_ok=True)
    os.makedirs(a.scratch, exist_ok=True)
    if not a.device:
        try:
            free = int(subprocess.run(['nvidia-smi', '--query-gpu=memory.free', '--format=csv,noheader,nounits'],
                                      capture_output=True, text=True).stdout.split()[0])
        except (OSError, ValueError, IndexError):
            free = 0
        a.device = '10de:27b0' if free >= 3000 else '1002:164e'
    results = []
    for side in [s for s in a.sides.split(',') if s]:
        path = os.path.join(a.out, side + ('-' + a.cell if a.cell else '') + '.json')
        if not a.report_only:
            r = run_side(a, side)
            with open(path, 'w') as f:
                json.dump(r, f, indent=1)
            print('%s: %s' % (side, r.get('error', 'done')), flush=True)
        if os.path.exists(path):
            results.append(json.load(open(path)))
    print(table(results, os.path.join(a.out, 'feel.md')))


if __name__ == '__main__':
    main()
