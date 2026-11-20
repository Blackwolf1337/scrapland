#!/usr/bin/env python3
"""The positional sound census of the second multiplayer design (stage 3.3): the sound sources around a -mp2 client's
player against single player's at the same spot.

The games run already (a joined -mp2 client, its instance, a single player game of the same level, all with the test
channel). The tool takes the client's view point from its "mirror json", asks the games "sounds src <x> <y> <z>
<radius>" (src/game/Global/TestChannel.c: the positional sets "ps" and the sound items of the objects "es" within the
radius) and compares:
  ps   the positional sets by name, sound, loop and position (the level's loops and interval sounds, MapSnd), client
       against single player, around the player and in the whole level
  es   the sound items of the objects by entity name, item, sound and loop: client against the instance for the
       entities both have (the replication), client against single player for the entities whose items the instance
       and single player agree on (the others are of the moment: a parked ship of another type, NPCs, traffic, the
       players); entities only one side has are listed
A source is "playing" when its sound has a handle (ps) or the item plays (es); the playing states depend on where the
listener of each game is and are listed, not counted. Rail traffic (NormalTraffic, MediumTraffic, HeavyTraffic,
FakeTraffic) and the players' own entities are left out.

  census.py --client <game> --instance <game> --level <Level> --sp <game> [--radius 15000] [--json FILE]
            [--samples 3]

Gaps: the positional sets that differ (near and whole level), the items of shared entities the client lacks or has
more than the instance, and those it differs in from single player where the instance agrees with single player.
Exit 0 without gaps, 1 otherwise.

The census by class (stage 4.1, --classes): every entity of every class by class and kind (the action class of a
thing with life, else the model, else the name without digits) on the instance, the client and single player
(test verbs entinfo and mirror json), --samples times 1 s apart; a gap is one found in every sample:
  missing  an entity of the instance in the client's interest (the level scope: every object, item, thing with life
           and plain entity; ships, characters and missiles within --interest of the client's own entity) that the
           client has no entity of the same name and class of
  state    a replica that differs from the instance's entity in active, model, action class, team or radar
  extra    an entity of the client that is no replica (it made it itself), but for its own entity and cameras
The table per class and kind (instance whole level, instance in the interest, client, single player whole level)
goes into the report; kinds single player has and the instance not are listed (the moment: traffic, NPC types).

  census.py --classes --client <game> --instance <game> --level <Level> [--sp <game>] [--interest 54000]
            [--json FILE] [--samples 3]"""
import argparse
import json
import os
import re
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(os.path.dirname(HERE)))
import channel  # noqa: E402

LEAVE = re.compile(r'^(NormalTraffic|MediumTraffic|HeavyTraffic|FakeTraffic|Me\d|NetPlayer|Player|MainChar)')


def ask(game, line, level=None):
    if level:
        channel.use_instance(level)
    try:
        return channel.send(game, line, timeout=20.0)
    finally:
        channel.CMD, channel.REPLY, channel.TMP = 'test_cmd.txt', 'test_reply.txt', 'test_cmd.tmp'


WHOLE = 10000000.0


def census(game, x, y, z, r, level=None):
    ps, es = {}, {}
    for line in ask(game, 'sounds src %.0f %.0f %.0f %.0f' % (x, y, z, r), level).splitlines():
        f = line.split('\t')
        if f[0] == 'ps' and len(f) >= 8:
            ps[(f[1], f[2], f[3], f[4], f[5], f[6])] = int(f[7])
        elif f[0] == 'es' and len(f) >= 9 and not LEAVE.match(f[1]):
            es.setdefault(f[1], {})[(f[2], f[3], f[4])] = int(f[8])
    return ps, es


def merge(samples):
    """the union of the samples, playing when it played in any"""
    ps, es = {}, {}
    for p, e in samples:
        for k, v in p.items():
            ps[k] = max(ps.get(k, 0), v)
        for n, items in e.items():
            d = es.setdefault(n, {})
            for k, v in items.items():
                d[k] = max(d.get(k, 0), v)
    return ps, es


CAMERAS = ('FCam', 'Cam', 'CamPath')
MOVING = (1, 2, 3)      # ship, character, missile (the grid's); the other kinds are in the level scope


def entinfo(game, level=None):
    """entinfo lines by name: class, active, model, action class, position, team, radar, replica"""
    out = {}
    for line in ask(game, 'entinfo', level).splitlines():
        f = line.split('\t')
        if len(f) < 12:
            continue
        out[f[0]] = dict(name=f[0], cls=f[1], active=int(f[3]), model=f[4], act=f[5],
                         pos=(float(f[6]), float(f[7]), float(f[8])), tg=int(f[9]), radar=int(f[10]),
                         replica=int(f[11]))
    return out


def mirror_of(game, level=None):
    for line in ask(game, 'mirror json', level).splitlines():
        if line.startswith('{'):
            return json.loads(line)
    raise RuntimeError('no mirror')


def kind_of(e):
    if e['act']:
        return e['act']
    if e['model']:
        return os.path.splitext(os.path.basename(e['model'].replace('\\', '/')))[0]
    return re.sub(r'\d+', '', e['name'])


def class_sample(a):
    """one sample: the gaps (a dict key -> text) and the table rows"""
    srv = mirror_of(a.instance, a.level)
    cli = mirror_of(a.client)
    sinfo = entinfo(a.instance, a.level)
    cinfo = entinfo(a.client)
    view = cli['view']
    own = cli.get('own', -1)
    own_names = {e['name'] for e in srv['ents'] if e['slot'] == own}
    replicas = {e['name'] for e in cli['ents']}
    gaps = {}
    table = {}

    def row(e):
        return table.setdefault((e['cls'], kind_of(e)), [0, 0, 0, 0])

    for e in sinfo.values():
        if e['cls'] not in CAMERAS:
            row(e)[0] += 1
    for s in srv['ents']:
        if s['name'] in own_names:
            continue
        e = sinfo.get(s['name'])
        if not e:
            continue
        d = sum((p - q) ** 2 for p, q in zip(e['pos'], view)) ** 0.5
        if s['kind'] in MOVING and d > a.interest:
            continue
        row(e)[1] += 1
        c = cinfo.get(s['name'])
        if not c or c['cls'] != e['cls']:
            gaps[('missing', e['cls'], e['name'])] = '%s %s (%s) at %.0f%s' % (
                e['cls'], e['name'], kind_of(e), d, ', client has class %s' % c['cls'] if c else '')
            continue
        for f in ('active', 'model', 'act', 'tg', 'radar'):
            # a model by the name it was first loaded with (the packs' names are case blind)
            if (e[f].lower() != c[f].lower()) if f == 'model' else e[f] != c[f]:
                gaps[('state', e['cls'], e['name'] + '.' + f)] = '%s %s %s: %r / %r' % (e['cls'], e['name'], f, e[f],
                                                                                       c[f])
    for c in cinfo.values():
        if c['cls'] in CAMERAS:
            continue
        if c['name'] in replicas or c['replica']:
            row(c)[2] += 1
            continue
        if re.match(r'^Me\d', c['name']) or c['name'] in own_names:
            continue
        gaps[('extra', c['cls'], c['name'])] = '%s %s (%s)' % (c['cls'], c['name'], kind_of(c))
    if a.sp:
        for e in entinfo(a.sp).values():
            if e['cls'] not in CAMERAS and not re.match(r'^(Player0|PlayerShip|MainChar)', e['name']):
                row(e)[3] += 1
    return gaps, table, len(srv['ents']), len(cli['ents'])


def classes(a):
    found, tables = [], []
    for i in range(a.samples):
        if i:
            time.sleep(1.0)
        g, t, ns, nc = class_sample(a)
        found.append(g)
        tables.append(t)
    keep = set(found[0])
    for f in found[1:]:
        keep &= set(f)
    table = tables[-1]
    counts = {}
    for k in keep:
        counts[k[0]] = counts.get(k[0], 0) + 1
    print('classes %s: %d gaps (%s); instance %d replicated entities, client %d replicas; %d classes and kinds' % (
        a.level, len(keep), ', '.join('%s %d' % kv for kv in sorted(counts.items())) or 'none', ns, nc, len(table)))
    for k in sorted(keep):
        print('  %-8s %s' % (k[0], found[-1][k]))
    sp_only = sorted('%s %s (%d)' % (k[0], k[1], v[3]) for k, v in table.items() if v[3] and not v[0])
    if a.sp:
        print('  kinds single player has and the instance not: %d' % len(sp_only))
        for v in sp_only[:40]:
            print('    %s' % v)
    print('  %-10s %-32s %8s %8s %8s %8s' % ('class', 'kind', 'instance', 'interest', 'client', 'sp'))
    for k in sorted(table):
        v = table[k]
        print('  %-10s %-32s %8d %8d %8d %8d' % (k[0], k[1][:32], v[0], v[1], v[2], v[3]))
    if a.json:
        with open(a.json, 'w') as f:
            json.dump({'level': a.level, 'gaps': len(keep), 'counts': counts,
                       'diffs': [[list(k), found[-1][k]] for k in sorted(keep)],
                       'table': [[k[0], k[1]] + v for k, v in sorted(table.items())], 'sp_only': sp_only}, f,
                      indent=1)
    return 1 if keep else 0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--client', required=True)
    ap.add_argument('--instance', required=True)
    ap.add_argument('--level', required=True)
    ap.add_argument('--sp')
    ap.add_argument('--radius', type=float, default=15000.0)
    ap.add_argument('--samples', type=int, default=3)
    ap.add_argument('--json')
    ap.add_argument('--classes', action='store_true')
    ap.add_argument('--interest', type=float, default=54000.0)
    a = ap.parse_args()
    if a.classes:
        return classes(a)
    if not a.sp:
        ap.error('--sp is needed for the sound census')
    m = json.loads(ask(a.client, 'mirror json').splitlines()[0])
    x, y, z = m['view']
    cs, ins, ss = [], [], []
    for i in range(a.samples):
        cs.append(census(a.client, x, y, z, a.radius))
        ins.append(census(a.instance, x, y, z, a.radius, a.level))
        ss.append(census(a.sp, x, y, z, a.radius))
        time.sleep(1.0)
    cps, ces = merge(cs)
    ips, ies = merge(ins)
    sps, ses = merge(ss)
    cwhole, _ = census(a.client, x, y, z, WHOLE)
    swhole, _ = census(a.sp, x, y, z, WHOLE)
    out = {'view': [x, y, z], 'radius': a.radius}
    out['ps_sp_only'] = sorted('|'.join(k) for k in sps if k not in cps)
    out['ps_client_only'] = sorted('|'.join(k) for k in cps if k not in sps)
    out['ps_playing_differs'] = sorted('|'.join(k) + ' sp %d client %d' % (sps[k], cps[k])
                                       for k in sps if k in cps and sps[k] != cps[k])
    out['ps_count'] = [len(sps), len(cps)]
    out['ps_level_sp_only'] = sorted('|'.join(k) for k in swhole if k not in cwhole)
    out['ps_level_client_only'] = sorted('|'.join(k) for k in cwhole if k not in swhole)
    out['ps_level_count'] = [len(swhole), len(cwhole)]
    out['es_instance_differ'] = []
    out['es_sp_differ'] = []
    out['es_moment'] = []
    out['es_playing_differs'] = []
    for n in sorted(set(ies) & set(ces)):
        for k in sorted(set(ies[n]) | set(ces[n])):
            if k not in ies[n] or k not in ces[n]:
                out['es_instance_differ'].append('%s %s instance %s client %s' % (n, '|'.join(k), k in ies[n],
                                                                                  k in ces[n]))
    for n in sorted(set(ses) & set(ces)):
        if n in ies and set(ies[n]) != set(ses[n]):
            out['es_moment'].append(n)
            continue
        for k in sorted(set(ses[n]) | set(ces[n])):
            if k not in ses[n] or k not in ces[n]:
                out['es_sp_differ'].append('%s %s sp %s client %s' % (n, '|'.join(k), k in ses[n], k in ces[n]))
            elif ses[n][k] != ces[n][k]:
                out['es_playing_differs'].append('%s %s sp %d client %d' % (n, '|'.join(k), ses[n][k], ces[n][k]))
    out['es_sp_only'] = sorted('%s %s' % (n, ','.join(k[1] for k in ses[n])) for n in ses if n not in ces)
    out['es_client_only'] = sorted('%s %s' % (n, ','.join(k[1] for k in ces[n])) for n in ces if n not in ses)
    out['es_instance_only'] = sorted('%s %s' % (n, ','.join(k[1] for k in ies[n])) for n in ies if n not in ces)
    out['es_count'] = [len(ses), len(ies), len(ces)]
    gaps = (len(out['ps_sp_only']) + len(out['ps_client_only']) + len(out['ps_level_sp_only']) +
            len(out['ps_level_client_only']) + len(out['es_instance_differ']) + len(out['es_sp_differ']))
    out['gaps'] = gaps
    print('census at %.0f %.0f %.0f radius %.0f: %d gaps; positional sets sp %d client %d (sp only %d, client only %d, '
          'playing differs %d), whole level sp %d client %d (sp only %d, client only %d); entities with sounds sp %d '
          'instance %d client %d (items differ from the instance %d, from single player %d, of the moment %d, playing '
          'differs %d, sp only %d, client only %d, instance only %d)' % (
              x, y, z, a.radius, gaps, len(sps), len(cps), len(out['ps_sp_only']), len(out['ps_client_only']),
              len(out['ps_playing_differs']), len(swhole), len(cwhole), len(out['ps_level_sp_only']),
              len(out['ps_level_client_only']), len(ses), len(ies), len(ces), len(out['es_instance_differ']),
              len(out['es_sp_differ']), len(out['es_moment']), len(out['es_playing_differs']), len(out['es_sp_only']),
              len(out['es_client_only']), len(out['es_instance_only'])))
    for key in ('ps_sp_only', 'ps_client_only', 'ps_level_sp_only', 'ps_level_client_only', 'ps_playing_differs',
                'es_instance_differ', 'es_sp_differ', 'es_moment', 'es_playing_differs', 'es_sp_only',
                'es_client_only', 'es_instance_only'):
        for v in out[key][:60]:
            print('  %-20s %s' % (key, v))
    if a.json:
        with open(a.json, 'w') as f:
            json.dump(out, f, indent=1)
    return 1 if gaps else 0


if __name__ == '__main__':
    sys.exit(main())
