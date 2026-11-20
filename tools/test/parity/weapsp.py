#!/usr/bin/env python3
"""The single player side of the weapon census (weapons.sh sp): in a running single player game with the test channel
the player walks to its ship and boards it (tape.py), climbs, takes the weapon and fires it in bursts with the
countermeasure now and then while it turns slowly (the auto aim picks the traffic), secs seconds; audit reset before
the firing, audit json after it into out (none: the control run, no firing).
  weapsp.py <game> <weapon|none> <out.json> [secs]"""
import json
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import tape  # noqa: E402
import channel  # noqa: E402

game, weap, out = sys.argv[1], sys.argv[2], sys.argv[3]
secs = float(sys.argv[4]) if len(sys.argv) > 4 else 30.0
r = tape.Rec(game, 1e9)
r.step({}, n=5)
me = r.player()
pos = r.ent(me, 'Pos')['Pos']
near = r.ents('Vehicles %g %g %g 30000' % tuple(pos)) if pos else []
best = None
for n in [n for n in near if n == 'PlayerShip' or n.startswith('Parked')]:
    p = r.ent(n, 'Pos')['Pos']
    if p and pos:
        d = math.dist(pos, p) - (1e6 if n == 'PlayerShip' else 0)
        if best is None or d < best[1]:
            best = (n, d, p)
if best:
    tape.walk_to(r, me, best[2], 25.0, use=True)
end = r.t + 14.0
while r.t < end and r.set() != 1:
    r.step({}, n=2)
log = {'board': best[0] if best else None, 'set': r.set(), 'player': r.player()}
if r.set() != 1:
    log['error'] = 'not in a ship'
    json.dump(log, open(out, 'w'))
    sys.exit(1)
r.step({'Racer.StrafeUp': 1}, n=15)
if weap != 'none':
    r.step({'Racer.' + weap: 1}, n=2)
    r.step({}, n=3)
channel.send(game, 'audit reset', timeout=30)
t0 = r.t
while r.t - t0 < secs:
    k = (r.t - t0) % 5.0
    keys = {'Racer.Right': 0.15}
    if weap != 'none':
        if k < 3.0 and (r.t - t0) % 2.0 < 1.5:
            keys['Racer.Fire'] = 1
        elif k >= 3.5:
            keys['Racer.CM'] = 1
    r.step(keys, n=5)
r.step({}, n=1)
log['secs'] = r.t - t0
text = channel.send(game, 'audit json', timeout=30)
log['audit'] = json.loads(text.splitlines()[0])
json.dump(log, open(out, 'w'), indent=1)
print('%s: %s, %.0f s' % (weap, log['board'], log['secs']))
