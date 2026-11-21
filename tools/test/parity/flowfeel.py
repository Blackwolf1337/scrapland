#!/usr/bin/env python3
"""The feel of the flows of the scripts (stage 6b of the second multiplayer design: possession, the conversor, an
elevator or teleporter, the metro), measured on rendered games as feel.py measures walking and flying.

A side is sp (single player, -imap:Levels/<level>) or join (a -mp2 client that joins the world a second rendered game
hosts, -host:<level> -mode:sandbox -mp2). Per flow and repetition the player is put next to its target, facing it
(single player: the test verb goto; the client: the instance's line goto), and the flow's key is pressed through XTEST
in the game's own gamescope X server: the left mouse button (Walk.Use) for the conversor, the teleporter and the metro
door, the right one (Walk.Possession) for the possession (with the energy bar on and full, at a sentinel, which stands
still). The feel log's view column (the camera's link, a fade, the items of the HUD or a menu, the action set:
src/game/App/FeelLog.c) gives the latency: from the press to the first drawn frame whose view differs from the one
before the press. The corrections of the client from the press to the end of the flow are counted (the hand-over).
Afterwards the flow is ended (Escape in an interface) and the game waits until the walk controls are back.

On DownTown the flows board and land (ship_flows): the own ship boarded through the test lines, then landed on its
parking box with the vertical strafe key, the latency of each to the first changed view.

  flowfeel.py [--sides sp,join] [--level Townhall] [--flows conv,tele,metro,poss] [--reps 3] [--port 30300]
              [--build DIR] [--out DIR] [--scratch DIR] [--device ID] [--delay MS] [--jitter MS] [--loss PCT]
              [--cell NAME]
The results go to --out/flows-<side>[-<cell>].json and the table to --out/flows.md.
"""
import argparse
import json
import os
import statistics
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import feel  # noqa: E402  (pops SCRAP_TIMESCALE: the feel runs on the real clock)
import parity  # noqa: E402
import scrap_mcp  # noqa: E402

# target, dx, dz of the goto, the mouse button (XTEST: 1 left, 3 right), secs until the walk controls are back
FLOWS = {
    'TownHall': {
        'conv': (['DM_Conversion_Point_34'], 0, -180, 1, 14.0),
        'tele': (['DM_Door_Teleporter_01', 'DM_Door_Teleporter_02'], 0, -150, 1, 10.0),
        'metro': (['DM_Door_Metro_Press'], 150, 0, 1, 4.0),
        'poss': (['Sentinel_01', 'Sentinel_02', 'Sentinel_03'], 0, -200, 3, 30.0),
    },
    # the own ship (single player's PlayerShip, a client's PlayerShip<n> of the instance's log): boarded by the use of
    # its arrow through the test lines (Parking.UseShip, the instance's "use"; the test goto selects no ship with the
    # arrow), then the landing back on its parking box: the ship put still 3000 units above the place it stood, the
    # vertical strafe down held (Racer.StrafeDown, the keypad's plus)
    'DownTown': {
        'board': (['@own'], 0, -700, 1, 0.0),
    },
}
LAND_HIGH = 3000.0


def read_view(path):
    """(mono ns, view, corrections) of each feel log line"""
    out = []
    try:
        with open(path) as f:
            next(f, None)
            for line in f:
                p = line.rstrip('\n').split(',')
                if len(p) >= 14:
                    try:
                        out.append((int(p[0]), p[13], int(p[11])))
                    except ValueError:
                        pass
    except OSError:
        pass
    return out


def view_latency(rows, t0, within=2.0):
    before = [r for r in rows if r[0] <= t0]
    if not before:
        return None
    v0 = before[-1][1]
    for r in rows:
        if t0 < r[0] <= t0 + within * 1e9 and r[1] != v0:
            return (r[0] - t0) / 1e6, v0, r[1]
    return None


def corrections(rows, t0, t1):
    c = [r[2] for r in rows if t0 <= r[0] <= t1]
    return c[-1] - c[0] if len(c) > 1 else 0


def run_side(a, side):
    res = {'side': side, 'level': a.level, 'cell': a.cell, 'net': dict(a.net) if side != 'sp' else {}, 'flows': {}}
    log = os.path.join(a.scratch, 'flows-%s%s.csv' % (side, '-' + a.cell if a.cell else ''))
    if os.path.exists(log):
        os.remove(log)
    env = dict(a.net if side != 'sp' else {}, SCRAP_FEELLOG=log)
    hostg = None
    if side == 'sp':
        g = parity.Game(a, 'ws', ['-imap:Levels/' + a.level], env)
    else:
        hostenv = dict(a.net)
        if os.environ.get('FEEL_HOST_DEVICE'):
            hostenv['SCRAP_VK_DEVICE'] = os.environ['FEEL_HOST_DEVICE']
        hostg = parity.Game(a, 'wo', ['-host:' + a.level, '-port:%d' % a.port, '-mode:sandbox', '-name:Host', '-mp2'],
                            hostenv)
        g = parity.Game(a, 'wj', ['-join:127.0.0.1', '-port:%d' % a.port, '-name:Join', '-mp2'], env)
    try:
        if hostg:
            hostg.start()
            parity.wait_for(lambda: parity.own_entity(parity.parse_ents(hostg.try_ask('entinfo', timeout=5))), 240, 2)
        g.start()
        if side == 'sp':
            parity.wait_for(lambda: g.try_ask('Scrap.GetLevelPath()', timeout=5), 240, 2)
            player = 0
        else:
            own = parity.wait_for(lambda: parity.own_entity(parity.parse_ents(g.try_ask('entinfo', timeout=5))), 240, 2)
            if not own:
                res['error'] = 'no own entity'
                return res
            _, player = feel.player_char(a, hostg, own['pos'])
            if player is None:
                print('flowfeel: own %s, instance: %s' % (own, (hostg.try_ask('entinfo', inst=a.level, timeout=10) or '')[:600]))
                res['error'] = 'no player on the instance'
                return res
        res['player'] = player
        time.sleep(6)

        def world(line):
            return g.ask(line) if side == 'sp' else hostg.ask(line, inst=a.level)

        def goto(target, dx, dz):
            if side == 'sp':
                return g.ask('goto %s %s %s' % (target, dx, dz))
            return hostg.ask('iline goto %d %s %s %s' % (player, target, dx, dz), inst=a.level)

        for flow in a.flows:
            if flow == 'land':
                continue
            spec = FLOWS.get(a.level, FLOWS.get(a.level.capitalize(), {})).get(flow)
            if not spec:
                continue
            if flow == 'board':
                ship_flows(a, g, hostg, side, player, log, res, goto, world, spec)
                continue
            targets, dx, dz, button, settle = spec
            got = []
            for k in range(a.reps):
                target = targets[k % len(targets)]
                if flow == 'poss':
                    world("Scrap.Set('EnergyBarActive', 1)")
                    time.sleep(8.0)
                goto(target, dx, dz)
                # the arrow selects the target in the next frames; a press waits for the next tick: the pauses step
                # through a 30 Hz tick
                # (the characters walk on: the possession's press soon after)
                time.sleep((0.5 if flow == 'poss' else 2.0) + (k * 7 % 10) / 10.0 / 30.0)
                me = 'Player0' if side == 'sp' else (parity.own_entity(parity.parse_ents(g.try_ask('entinfo', timeout=5)))
                                                       or {}).get('name', '?')
                sel = g.try_ask('ent %s AutoSelectedEntity' % me, timeout=5)
                with scrap_mcp.XInput(feel.XGame(g)) as x:
                    t0 = time.monotonic_ns()
                    x.xtst.XTestFakeButtonEvent(x.dpy, button, 1, 0)
                    x.x11.XSync(x.dpy, 0)
                    time.sleep(0.12)
                    x.xtst.XTestFakeButtonEvent(x.dpy, button, 0, 0)
                    x.x11.XSync(x.dpy, 0)
                time.sleep(2.5)
                if flow in ('conv', 'metro'):
                    # out of the interface (Escape: CharExit, MetroExit)
                    feel.key(g, 'Escape')
                time.sleep(settle)
                t1 = time.monotonic_ns()
                rows = read_view(log)
                lat = view_latency(rows, t0)
                got.append({'selected': sel, 'target': target, 'latency': round(lat[0], 1) if lat else None,
                            'from': lat[1] if lat else None, 'to': lat[2] if lat else None,
                            'corrections': corrections(rows, t0, t1)})
                print('flowfeel %s %s %d: %s' % (side, flow, k, got[-1]), flush=True)
            lats = [x['latency'] for x in got if x['latency'] is not None]
            res['flows'][flow] = {'reps': got, 'samples': len(lats), 'misses': len(got) - len(lats),
                                  'latency_mean': round(statistics.mean(lats), 1) if lats else None,
                                  'latency_max': max(lats) if lats else None,
                                  'corrections': sum(x['corrections'] for x in got)}
    finally:
        g.stop()
        if hostg:
            hostg.stop()
    return res


def flow_result(got):
    lats = [x['latency'] for x in got if x['latency'] is not None]
    return {'reps': got, 'samples': len(lats), 'misses': len(got) - len(lats),
            'latency_mean': round(statistics.mean(lats), 1) if lats else None,
            'latency_max': max(lats) if lats else None, 'corrections': sum(x['corrections'] for x in got)}


def ship_flows(a, g, hostg, side, player, log, res, goto, world, spec):
    """board: the use of the own ship's arrow (test lines: the latency from the line to the first view change, the
    boarding's camera path, includes the channel's own delay); land (with --flows ...,land): the own ship back on its
    parking box, the latency from the strafe key to the first view change (the parking's camera and HUD); each
    repetition boards and lands"""
    _, dx, dz, _, _ = spec
    ship = 'PlayerShip' if side == 'sp' else feel.own_ship(hostg, player)
    if not ship:
        res['error'] = 'no own ship'
        return
    m = parity.parse_ents(world('entinfo') or '')
    spot = next((e['pos'] for e in m if e['name'] == ship), None)
    boards, lands = [], []
    for k in range(a.reps):
        rows = feel.read_log(log)
        if not rows or rows[-1]['kind'] != 'C':
            print('flowfeel %s board %d: not on foot (%s)' % (side, k, rows[-1]['kind'] if rows else '-'), flush=True)
            break
        goto(ship, dx, dz)
        time.sleep(2.0 + (k * 7 % 10) / 10.0 / 30.0)
        t0 = time.monotonic_ns()
        if side == 'sp':
            g.ask('Parking.UseShip("Player0", "PlayerShip", "", "")')
        else:
            hostg.ask('iline use %d %s' % (player, ship), inst=a.level)
        inship = parity.wait_for(lambda: feel.own_kind(feel.read_log(log), time.monotonic_ns()) == 'S', 40, 0.5)
        time.sleep(2.0)
        t1 = time.monotonic_ns()
        rows = read_view(log)
        lat = view_latency(rows, t0)
        boards.append({'target': ship, 'latency': round(lat[0], 1) if lat else None, 'in_ship': bool(inship),
                       'corrections': corrections(rows, t0, t1)})
        print('flowfeel %s board %d: %s' % (side, k, boards[-1]), flush=True)
        if not inship or 'land' not in a.flows or not spot:
            break
        target = 'Scrap.UsrEntity(0)' if side == 'sp' else 'Scrap.GetEntity("%s")' % ship
        world('do e = %s; e.Pos = (%.1f, %.1f, %.1f); e.Vel = (0, 0, 0)' % (target, spot[0], spot[1] + LAND_HIGH, spot[2]))
        time.sleep(2.5)
        with scrap_mcp.XInput(feel.XGame(g)) as x:
            code = x.keycode('KP_Add')
            t0 = time.monotonic_ns()
            x.xtst.XTestFakeKeyEvent(x.dpy, code, 1, 0)
            x.x11.XSync(x.dpy, 0)
            onfoot = parity.wait_for(lambda: feel.own_kind(feel.read_log(log), time.monotonic_ns()) == 'C', 15, 0.2)
            x.xtst.XTestFakeKeyEvent(x.dpy, code, 0, 0)
            x.x11.XSync(x.dpy, 0)
        time.sleep(4.0)
        t1 = time.monotonic_ns()
        rows = read_view(log)
        lat = view_latency(rows, t0, 6.0)
        lands.append({'target': ship, 'latency': round(lat[0], 1) if lat else None, 'on_foot': bool(onfoot),
                      'corrections': corrections(rows, t0, t1)})
        print('flowfeel %s land %d: %s' % (side, k, lands[-1]), flush=True)
        if not onfoot:
            break
    res['flows']['board'] = flow_result(boards)
    if 'land' in a.flows:
        res['flows']['land'] = flow_result(lands)


def table(results, out, limit=16.7):
    sp = next((r for r in results if r['side'] == 'sp'), None)
    lines = ['| side | cell | flow | latency ms mean/max | samples/misses | corrections | over single player by |',
             '|---|---|---|---|---|---|---|']
    for r in results:
        for flow, f in sorted(r.get('flows', {}).items()):
            ref = sp['flows'].get(flow, {}).get('latency_mean') if sp and r is not sp else None
            over = round(f['latency_mean'] - ref, 1) if ref is not None and f['latency_mean'] is not None else None
            lines.append('| %s | %s | %s | %s/%s | %d/%d | %d | %s |' % (
                r['side'], r.get('cell') or '-', flow, f['latency_mean'], f['latency_max'], f['samples'], f['misses'],
                f['corrections'], '-' if over is None else '%s%s' % (over, ' (over the limit)' if over > limit else '')))
    text = '\n'.join(lines)
    with open(out, 'w') as fh:
        fh.write(text + '\n')
    return text


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--sides', default='sp,join')
    ap.add_argument('--level', default='TownHall')
    ap.add_argument('--flows', default='conv,tele,metro,poss')
    ap.add_argument('--reps', type=int, default=3)
    ap.add_argument('--port', type=int, default=30300)
    ap.add_argument('--build', default=os.path.join(feel.REPO, 'build-linux64'))
    ap.add_argument('--out', default=os.path.join(feel.REPO, 'build-linux64', 'flowfeel'))
    ap.add_argument('--scratch', default=os.path.join(feel.REPO, 'build-linux64', 'parity-games'))
    ap.add_argument('--timeout', type=int, default=1200)
    ap.add_argument('--device', default='10de:27b0')
    ap.add_argument('--delay', type=int, default=0)
    ap.add_argument('--jitter', type=int, default=0)
    ap.add_argument('--loss', type=float, default=0.0)
    ap.add_argument('--cell', default='')
    ap.add_argument('--report-only', action='store_true')
    a = ap.parse_args()
    a.flows = [f for f in a.flows.split(',') if f]
    a.net = {}
    if a.delay:
        a.net['SCRAP_NET2_DELAY'] = str(a.delay)
    if a.jitter:
        a.net['SCRAP_NET2_JITTER'] = str(a.jitter)
    if a.loss:
        a.net['SCRAP_NET2_LOSS'] = '%g' % a.loss
    a.build = os.path.abspath(a.build)
    os.makedirs(a.out, exist_ok=True)
    os.makedirs(a.scratch, exist_ok=True)
    results = []
    for side in [s for s in a.sides.split(',') if s]:
        path = os.path.join(a.out, 'flows-%s%s.json' % (side, '-' + a.cell if a.cell and side != 'sp' else ''))
        if not a.report_only:
            r = run_side(a, side)
            with open(path, 'w') as f:
                json.dump(r, f, indent=1)
            print('%s: %s' % (side, r.get('error', 'done')), flush=True)
        if os.path.exists(path):
            results.append(json.load(open(path)))
    print(table(results, os.path.join(a.out, 'flows.md')))


if __name__ == '__main__':
    main()
