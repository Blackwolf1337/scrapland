#!/usr/bin/env python3
"""The mirror diff of the replication v2 (stage 3.2 of the second multiplayer design, include/scrap/repent.h): the
declared state and the journal of every replicated entity on an instance against what a joined -mp2 client shows.

Both sides answer the test channel verb "mirror json" (src/game/App/RepEnt.c, ReplicaEnt.c). The tool takes --samples
pairs of mirrors (server, then client) --gap seconds apart and reports the differences found in every pair (a
difference of one moment, an action that just changed, a door on its way, is not one in the next pair):
  missing  an entity of the server in the client's interest (the level scope: all objects, items and things with
           life; the moving ones within --radius of the client's own entity) that the client has no replica of
  extra    a replica whose slot and generation the server does not have
  field    a field of the declared state that differs (positions, velocities, angles and the times of the actions
           with a tolerance: the client draws the moving ones ~100 ms behind; the life of a burning thing as much
           as or a few points more than the server's)
  journal  the journal of the slot differs (names, order, values)
  readback a journal variable reads back another value from the replica than from the server's entity
The own entity of the client (its prediction) is left out (the rail traffic is the server's since stage 2.2 and compared).
The action times of a ship are not compared: its classes show the ship's parts with times each side derives from its
smoothed motion (cActCtrl_UpdateShip).

  mirror.py --game <client game folder> --instance <instance game folder> --level <level> [--samples 3] [--gap 1]
            [--radius 90000] [--json out.json] [--show 30]
Exit code 0 when no difference is left, 1 otherwise, 2 when a side does not answer."""
import argparse
import json
import os
import struct
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
import channel  # noqa: E402

POS_SCALE = 4.0


def fbits(v):
    """the float whose bits the int v holds (a class block value)"""
    return struct.unpack('<f', struct.pack('<i', v))[0]

# class block (include/scrap/repent.h REPX_)
X_NAMES = ['gen', 'act2', 'act2t', 'act3', 'act3t', 'walk', 'head', 'maxlife', 'prevlife', 'burn', 'radar', 'fx',
           'fxp', 'boost', 'cloud', 'emi', 'firew', 'move', 'movet', 'item', 'blink', 'target', 'level', 'arrow']
# times in ms that run on: compared with a tolerance
X_TIMES = {'act2t', 'act3t', 'burn', 'boost', 'emi', 'item'}
MOVING = (1, 2, 3)          # ship, character, missile: the grid's
FL_PLAYER = 0x08
FL_DERIVED = 0x08 | 0x10 | 0x20 | 0x40 | 0x80   # player, fire key, siren, slow, asleep: the server's own view


BURN_LAG = 5               # life points a burning thing loses while the client draws it ~100 ms behind


def lags(server, client):
    """the life of a burning entity on the client: the server's of a moment before (as much or a little more)"""
    return 0 <= client - server <= BURN_LAG


def ask(game, line, inst=None, timeout=60.0):
    save = channel.CMD, channel.REPLY, channel.TMP
    if inst:
        channel.use_instance(inst)
    try:
        return channel.send(game, line, timeout=timeout)
    finally:
        channel.CMD, channel.REPLY, channel.TMP = save


def mirror(game, inst=None):
    text = ask(game, 'mirror json', inst)
    for line in text.splitlines():
        line = line.strip()
        if line.startswith('{'):
            return json.loads(line)
    raise RuntimeError('no mirror: ' + text[:200])


def dist(a, b):
    return sum(((x - y) / POS_SCALE) ** 2 for x, y in zip(a, b)) ** 0.5


def speed(e):
    return sum(v * v for v in e['vel']) ** 0.5


def ang_diff(a, b):
    d = (a - b) & 0xffff
    return min(d, 0x10000 - d)


def jval_eq(a, b):
    if isinstance(a, float) or isinstance(b, float):
        try:
            return abs(float(a) - float(b)) <= 1e-6 * max(1.0, abs(float(a)))
        except (TypeError, ValueError):
            return False
    if isinstance(a, list) and isinstance(b, list):
        return len(a) == len(b) and all(jval_eq(x, y) for x, y in zip(a, b))
    return a == b


def turn_rates(pairs, times):
    """per slot and generation of the server: the fastest turn between two of its samples (angle steps a second, of
    the angles and of the view), the tolerance of a turning entity (the client draws it ~100 ms behind, and is asked
    after the server)"""
    rates = {}
    for i in range(1, len(pairs)):
        dt = max(0.1, times[i] - times[i - 1])
        before = {(e['slot'], e['gen']): e for e in pairs[i - 1][0]['ents']}
        for e in pairs[i][0]['ents']:
            b = before.get((e['slot'], e['gen']))
            if not b:
                continue
            r = max([ang_diff(e['ang'][k], b['ang'][k]) for k in range(3)] +
                    [ang_diff(e['view'][k], b['view'][k]) for k in range(2)]) / dt
            key = (e['slot'], e['gen'])
            rates[key] = max(rates.get(key, 0.0), r)
    return rates


def compare(srv, cli, radius, rates=None):
    """the differences of one pair: a set of keys (kind, slot, what) and a dict of the details"""
    rates = rates or {}
    view = [round(v * POS_SCALE) for v in cli['view']]
    own = cli.get('own', -1)
    sents = {(e['slot'], e['gen']): e for e in srv['ents']}
    cents = {(e['slot'], e['gen']): e for e in cli['ents']}
    diffs = {}
    for key, s in sents.items():
        if s['slot'] == own:
            continue
        if s['kind'] in MOVING and dist(s['pos'], view) > radius:
            continue
        c = cents.get(key)
        if not c:
            diffs[('missing', s['slot'], s['name'])] = '%s %s at %.0f' % (s['cls'], s['name'], dist(s['pos'], view))
            continue
        for f in ('kind', 'cls', 'name'):
            if s[f] != c[f]:
                diffs[('field', s['slot'], f)] = '%s: %r / %r' % (s['name'], s[f], c[f])
        tol = 50.0 + speed(s) * 0.6
        d = dist(s['pos'], c['pos'])
        if d > tol:
            diffs[('field', s['slot'], 'pos')] = '%s: %.0f units apart (tolerance %.0f)' % (s['name'], d, tol)
        atol = (3000 if s['kind'] in MOVING else 40) + rates.get(key, 0.0) * 0.6
        for k in range(3):
            if ang_diff(s['ang'][k], c['ang'][k]) > atol:
                diffs[('field', s['slot'], 'ang')] = '%s: %s / %s' % (s['name'], s['ang'], c['ang'])
        for k in range(2):
            if s['kind'] == 2 and ang_diff(s['view'][k], c['view'][k]) > atol:
                diffs[('field', s['slot'], 'view')] = '%s: %s / %s' % (s['name'], s['view'], c['view'])
        burning = s['x'][X_NAMES.index('burn')] > 0
        for f in ('life', 'weapon'):
            if f == 'life' and burning and lags(s[f], c[f]):
                continue
            if s[f] != c[f] and not (f == 'life' and s['kind'] == 3):
                diffs[('field', s['slot'], f)] = '%s: %r / %r' % (s['name'], s[f], c[f])
        if (s['flags'] & ~FL_DERIVED) != (c['flags'] & ~FL_DERIVED):
            diffs[('field', s['slot'], 'flags')] = '%s: %#x / %#x' % (s['name'], s['flags'], c['flags'])
        for k in range(2):
            if s['act'][k] != c['act'][k]:
                diffs[('field', s['slot'], 'act%d' % k)] = '%s: %d / %d' % (s['name'], s['act'][k], c['act'][k])
            elif abs(s['actTime'][k] - c['actTime'][k]) > 600 and s['act'][k] and s['kind'] != 1:
                diffs[('field', s['slot'], 'actTime%d' % k)] = '%s: %d / %d' % (s['name'], s['actTime'][k],
                                                                                 c['actTime'][k])
        for i, n in enumerate(X_NAMES):
            a, b = s['x'][i], c['x'][i]
            if n == 'move' or n == 'cloud':
                a, b = a >> 30, b >> 30      # the switch; the time runs on
            if n in X_TIMES and n in ('act2t', 'act3t') and s['kind'] == 1:
                continue    # a ship's part actions: their times follow the smoothed ship on each side
            if n in X_TIMES:
                if abs(a - b) > 600:
                    diffs[('field', s['slot'], n)] = '%s: %d / %d' % (s['name'], a, b)
            elif n == 'prevlife' and burning and lags(a, b):
                continue
            elif n == 'fxp' and s['x'][i - 1] == c['x'][i - 1] and abs(fbits(a) - fbits(b)) <= 0.25:
                continue    # the parameter of a running effect (the fade in of a placed traffic ship), ~100 ms behind
            elif a != b:
                diffs[('field', s['slot'], n)] = '%s: %d / %d' % (s['name'], a, b)
        sj, cj = s['j'], c['j']
        if [(x[0], ) for x in sj] != [(x[0], ) for x in cj] or \
                any(not jval_eq(x[1], y[1]) for x, y in zip(sj, cj)):
            first = next((i for i, (x, y) in enumerate(zip(sj, cj)) if x[0] != y[0] or not jval_eq(x[1], y[1])),
                         min(len(sj), len(cj)))
            diffs[('journal', s['slot'], s['name'])] = '%s: %d / %d entries, first difference at %d: %s / %s' % (
                s['name'], len(sj), len(cj), first, sj[first] if first < len(sj) else '-',
                cj[first] if first < len(cj) else '-')
        else:
            for x, y in zip(sj, cj):
                if not jval_eq(x[2], y[2]):
                    diffs[('readback', s['slot'], x[0])] = '%s.%s: %r / %r' % (s['name'], x[0], x[2], y[2])
    for key, c in cents.items():
        if key not in sents:
            diffs[('extra', c['slot'], c['name'])] = '%s %s' % (c['cls'], c['name'])
    return diffs


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--game', required=True, help='the client game folder')
    ap.add_argument('--instance', required=True, help='the instance game folder')
    ap.add_argument('--level', required=True)
    ap.add_argument('--samples', type=int, default=3)
    ap.add_argument('--gap', type=float, default=1.0)
    ap.add_argument('--radius', type=float, default=90000.0)
    ap.add_argument('--json')
    ap.add_argument('--show', type=int, default=30)
    a = ap.parse_args()
    pairs, times = [], []
    try:
        for i in range(a.samples):
            if i:
                time.sleep(a.gap)
            times.append(time.time())
            srv = mirror(a.instance, a.level)
            cli = mirror(a.game)
            pairs.append((srv, cli))
    except (channel.NoAnswer, RuntimeError, ValueError) as e:
        print('mirror: %s' % e)
        return 2
    rates = turn_rates(pairs, times)
    found = [compare(s, c, a.radius, rates) for s, c in pairs]
    keep = set(found[0])
    for f in found[1:]:
        keep &= set(f)
    srv, cli = pairs[-1]
    counts = {}
    for k in keep:
        counts[k[0]] = counts.get(k[0], 0) + 1
    print('mirror %s: server %d entities, client %d replicas (made %s, waits %s, fails %s, purged %s); differences '
          'in all %d pairs: %d (%s)' % (a.level, len(srv['ents']), len(cli['ents']), cli.get('made'), cli.get('waits'),
                                       cli.get('fails'), cli.get('purged'), len(pairs), len(keep),
                                       ', '.join('%s %d' % kv for kv in sorted(counts.items())) or 'none'))
    for k in sorted(keep, key=lambda k: (k[0], str(k[2]), k[1]))[:a.show]:
        print('  %-8s slot %5d %-14s %s' % (k[0], k[1], k[2] if k[0] in ('field', 'readback') else '',
                                           found[-1][k]))
    if len(keep) > a.show:
        print('  ... %d more' % (len(keep) - a.show))
    if a.json:
        with open(a.json, 'w') as f:
            json.dump({'level': a.level, 'counts': counts, 'diffs': [[list(k), found[-1][k]] for k in sorted(
                keep, key=str)], 'server': len(srv['ents']), 'client': len(cli['ents'])}, f, indent=1)
    return 1 if keep else 0


if __name__ == '__main__':
    sys.exit(main())
