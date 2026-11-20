#!/usr/bin/env python3
"""The weapon census (weapons.sh census): per weapon the samples played and the debris systems dropped (audit category
census) of single player firing it (weapsp.py) without those of single player not firing (the control, none.json),
against the instance and the clients of the weapon scenario (weapons.sh fight).
  weapcensus.py <fight dir> <sp dir> <weapon> ...
A gap: a name of the weapon (single player has it firing, not in the control) that the instance played near a
player in the scenario (its census keys snd.near / debris.near) and neither client played (B: the target, which sees
the shooter's replica; A: the shooter itself, whose own steps are its own). A name only one client lacks is listed
(the other player's own events: its pickups, its own ship's sounds)."""
import json
import os
import sys


def names(rows, near=False):
    """census keys by name (near: the instance's near keys under the name keys)"""
    out = {}
    for r in rows:
        if r['cat'] != 'census' or ':' not in r['key']:
            continue
        kind, name = r['key'].split(':', 1)
        if near:
            if not kind.endswith('.near'):
                continue
            kind = kind[:-5] + '.name'
        elif not kind.endswith('.name'):
            continue
        out[kind + ':' + name] = r['count']
    return out


def load(path, near=False):
    try:
        with open(path) as f:
            text = f.read()
        try:
            d = json.loads(text)
        except ValueError:
            d = json.loads(text.splitlines()[0])
        return names(d['audit']['rows'] if 'audit' in d else d['rows'], near)
    except (OSError, ValueError, KeyError) as e:
        return None


scen, spd = sys.argv[1], sys.argv[2]
control = load(os.path.join(spd, 'none.json')) or {}
total = 0
out = {}
for w in sys.argv[3:]:
    sp = load(os.path.join(spd, w + '.json'))
    inst = load(os.path.join(scen, w + '.iaudit.json'), near=True)
    a = load(os.path.join(scen, w + '.aaudit.json'))
    b = load(os.path.join(scen, w + '.baudit.json'))
    if sp is None or inst is None or a is None or b is None:
        print('%s: missing data (sp %s, instance %s, A %s, B %s)' % (w, sp is not None, inst is not None,
                                                                    a is not None, b is not None))
        total += 1
        continue
    wn = {k for k in sp if k not in control}
    seen = wn & set(inst)
    gb = sorted(k for k in seen if k not in b)
    ga = sorted(k for k in seen if k not in a)
    both = sorted(k for k in seen if k not in a and k not in b)
    notseen = sorted(wn - set(inst))
    extra_b = sorted(k for k in b if k not in inst and k not in sp and k not in control)
    print('%s: weapon names %d (single player firing, not in the control), %d of them on the instance near a player; '
          'gaps (on neither client) %d; missing on the target client %d, on the shooter client %d; not near in this '
          'scenario %d; target only %d' % (w, len(wn), len(seen), len(both), len(gb), len(ga), len(notseen),
                                            len(extra_b)))
    for k in seen:
        print('    %-50s sp %5d instance %5d shooter %5d target %5d' % (k, sp[k], inst.get(k, 0), a.get(k, 0),
                                                                        b.get(k, 0)))
    for k in both:
        print('  gap         %s' % k)
    for k in gb:
        print('  no target   %s' % k)
    for k in ga:
        print('  no shooter  %s' % k)
    for k in notseen:
        print('  not seen    %s' % k)
    for k in extra_b:
        print('  target only %s (%d)' % (k, b[k]))
    total += len(both)
    out[w] = {'weapon_names': sorted(wn), 'seen': sorted(seen), 'gaps': both, 'no_target': gb, 'no_shooter': ga,
              'not_seen': notseen, 'target_only': extra_b}
json.dump(out, open(os.path.join(scen, 'weapcensus.json'), 'w'), indent=1)
print('weapon census gaps: %d' % total)
sys.exit(1 if total else 0)
