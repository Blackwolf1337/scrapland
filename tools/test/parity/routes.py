#!/usr/bin/env python3
"""Makes the routes of the parity tool (tools/test/parity/routes.txt) from the levels' own dummies, so no point is
placed by hand. For each level it starts a single player game (-imap:Levels/<level>, rendered, hidden, muted), waits
--settle seconds and reads the dummies that are left (Scrap.StartDummySearch("DM_*", 1)) and the entities (entinfo,
where the dummies the scripts turned into entities keep their DM_ names: doors, elevators, links, phones, parking
areas, and the parking boxes ParkParking*). The points of a level, after the spawn (the host's own spawn, taken at
run time by parity.py):

  busy     the anchor (a dummy or one of the entities above) with the most living things within BUSY_RADIUS:
           characters, traffic, police and rail traffic of the single player census at that moment; at least
           MIN_APART from the single player's start
  door     the elevator door nearest to the single player's start (DM_Door_Elevator*), else a metro or level door,
           else any DM_Door_*
  parking  the parking box nearest to the start (ParkParking*), else a parking area (DM_AreaToPark*), else a parked
           ship's dummy (DM_Parked*, DM_Parking*)

No anchor is farther than MAX_AWAY from the start, none is a dummy in the air or outside the playing space (SKIP).
A level without such an anchor has fewer points. Output one table, a line per point: level|point|anchor|x y z.

  routes.py [--levels A,B] [--build DIR] [--scratch DIR] [--out tools/test/parity/routes.txt] [--jobs 4]
"""
import argparse
import os
import re
import sys
import threading
import time
import types

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import parity  # noqa: E402

BUSY_RADIUS = 15000.0
MIN_APART = 6000.0
MAX_AWAY = 60000.0                 # no point farther from the start (dummies left far outside an arena's space)
# anchors in the air or outside the playing space: the rail tracks, the 3D graph of the AI, the map markers, the ship
# editor's keypad (a dummy of the Junkyard that the arenas carry too)
SKIP = ('DM_Track', 'DM_PointGraph3DAI', 'DM_2dMap', 'DM_MapTop', 'DM_Door_EditShip')


def living(e):
    k = parity.kind_of(e)
    return bool(k) and e['active'] and (k.startswith('npc') or k in ('traffic', 'police', 'rail traffic') or
                                        k.startswith('ship '))


def dummies(g):
    """(name, (x, y, z)) of the dummies left in the level"""
    out = []
    lines = ['do Scrap.StartDummySearch("DM_*", 1)'] + ['Scrap.NextDummySearch()'] * 200
    while True:
        text = g.ask(lines, timeout=60)
        got = 0
        for line in text.splitlines():
            m = re.match(r"\('([^']+)', \(([-\d.e]+), ([-\d.e]+), ([-\d.e]+)\)", line)
            if m:
                out.append((m.group(1), tuple(float(x) for x in m.groups()[1:])))
                got += 1
        if got < 200:
            return out
        lines = ['Scrap.NextDummySearch()'] * 200


def pick(level, ents, dms):
    me = None
    for e in ents:
        if e['name'] in ('Player0', 'Player') or e['list'] == 'Players' and e['cls'] == 'WalkChar':
            me = e
            break
    start = me['pos'] if me else (dms[0][1] if dms else (0.0, 0.0, 0.0))
    anchors = list(dms)
    for e in ents:
        if e['name'].startswith('DM_') or e['name'].startswith('ParkParking'):
            anchors.append((e['name'], e['pos']))
    anchors = [x for x in anchors if not x[0].startswith(SKIP) and parity.dist(x[1], start) <= MAX_AWAY]
    life = [e['pos'] for e in ents if living(e)]
    points = []

    def near(prefixes):
        for p in prefixes:
            c = [a for a in anchors if a[0].startswith(p)]
            if c:
                return min(c, key=lambda a: parity.dist(a[1], start))
        return None
    best, bn = None, -1
    for a in anchors:
        if parity.dist(a[1], start) < MIN_APART:
            continue
        n = sum(1 for p in life if parity.dist(p, a[1]) <= BUSY_RADIUS)
        if n > bn or (n == bn and best and parity.dist(a[1], start) < parity.dist(best[1], start)):
            best, bn = a, n
    if best and bn > 0:
        points.append(('busy', best))
    d = near(('DM_Door_Elevator', 'DM_Door_Metro', 'DM_Door_Outmap', 'DM_Door_'))
    if d:
        points.append(('door', d))
    p = near(('ParkParking', 'DM_AreaToPark', 'DM_Parked', 'DM_Parking'))
    if p:
        points.append(('parking', p))
    return ['%s|%s|%s|%.0f %.0f %.0f' % ((level, n, a[0]) + tuple(a[1])) for n, a in points]


def one(a, level, slot, out):
    g = parity.Game(a, 'rt%d' % slot, ['-imap:Levels/' + level])
    try:
        g.start()
        if not parity.wait_for(lambda: g.try_ask('Scrap.GetLevelPath()', timeout=5), 240, 2):
            out[level] = ['# %s: the level did not start' % level]
            return
        time.sleep(a.settle)
        ents = parity.parse_ents(g.ask('entinfo', timeout=60))
        out[level] = pick(level, ents, dummies(g)) or ['# %s: no anchors' % level]
    except Exception as ex:  # noqa: BLE001
        out[level] = ['# %s: %s' % (level, ex)]
    finally:
        g.stop()
    print('%s: %d points' % (level, len([x for x in out[level] if not x.startswith('#')])), flush=True)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--levels', default=','.join(parity.world_levels()))
    ap.add_argument('--build', default=os.path.join(parity.REPO, 'build-linux64'))
    ap.add_argument('--scratch', default=os.path.join(parity.REPO, 'build-linux64', 'routes-games'))
    ap.add_argument('--out', default=os.path.join(HERE, 'routes.txt'))
    ap.add_argument('--settle', type=float, default=30.0)
    ap.add_argument('--jobs', type=int, default=4)
    ap.add_argument('--device', default=None)
    a = ap.parse_args()
    a = types.SimpleNamespace(**vars(a), port=0, timeout=900)
    a.build = os.path.abspath(a.build)
    a.device = a.device or parity.pick_device()
    os.makedirs(a.scratch, exist_ok=True)
    levels = [x for x in a.levels.split(',') if x]
    out, todo, lock = {}, list(levels), threading.Lock()

    def worker(slot):
        while True:
            with lock:
                if not todo:
                    return
                level = todo.pop(0)
            one(a, level, slot, out)
    ts = [threading.Thread(target=worker, args=(k,)) for k in range(a.jobs)]
    for t in ts:
        t.start()
    for t in ts:
        t.join()
    with open(a.out, 'w') as f:
        f.write('# The routes of tools/test/parity/parity.py after the spawn point: level|point|anchor|x y z.\n'
                '# Made by tools/test/parity/routes.py from the levels\' dummies (see its header); not edited by hand.\n')
        for level in levels:
            f.write('\n'.join(out.get(level, ['# %s: not run' % level])) + '\n')
    print('routes: %s' % a.out)


if __name__ == '__main__':
    main()
