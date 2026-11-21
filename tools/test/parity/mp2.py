#!/usr/bin/env python3
"""The gate runner of the multiplayer rewrite (plan mp2, stage 0.4), called by tools/test/mp2.sh. Each gate prints one
table, writes it as <out>/<gate>/<gate>.md with the numbers as <out>/<gate>/<gate>.json, and ends with an exit code:
0 pass, 1 fail (gaps, cells over a limit, failed steps or checks), 2 the gate could not run (a level, side or chapter
without a result).

  mp2.py parity [--levels A,B:dm] [--points N] [...]   tools/test/parity/parity.py over the world levels and their
                                                        routes, the PvP maps in their mode
  mp2.py feel [--cells 0,50,150] [--sides join,host]   tools/test/parity/feel.py: single player once, then each side
                                                        per cell of delay (each way), jitter and loss
  mp2.py coop <chapter> [--points DIR]                 a chapter of tools/test/coop/chapters.txt from its story point
                                                        (tools/test/coop/autochain.sh), then coopparity.py at the
                                                        chapter's start and end
  mp2.py sp [--lanes a,b,c] [--levels-time 30]         single player: regress.py check, golden.sh, levels.sh
  mp2.py classes [--levels A,B] [--bot idle]           tools/test/parity/classes.py (stages 4.1, 4.2): per world level a
                                                        -mp2 instance and a joined client: mirror, census by class, the
                                                        definitions of the level's start, the audits, crashes
  mp2.py weapons [--weapons Vulcan,Laser]              tools/test/parity/weapons.sh (stage 4.1): fight, single player
                                                        and the sound and effect census of each weapon
  mp2.py all <chapter> [...]                           the four above and one summary table (<out>/summary.md, .json)
  mp2.py diff <old out> <new out>                      the gates two runs have in common, number by number against
                                                        the run-to-run noise (NOISE); exit 1 on a real difference

Common options: --out DIR (default build-linux64/mp2), --scratch DIR, --build DIR (absolute), --port BASE (a block of
40 ports from BASE: a free world's coordinator at BASE and its instances from BASE + 8; one world at a time), --device.
The audit column of parity is the instance's `audit json` (stage 0.1); a build without that verb shows n/a."""
import argparse
import json
import os
import re
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
sys.path.insert(0, HERE)
import parity  # noqa: E402
import gameclock  # noqa: E402

COOP = os.path.join(REPO, 'tools', 'test', 'coop')
FRAME_MS = 1000.0 / 60.0
# the feel cells: name -> (delay ms each way, jitter ms, loss percent); the limits of stage 3.5: latency within one
# frame of single player, corrections per minute at most 1 at 0 ms and 3 at 150 ms (50 ms: 2, between the two)
CELLS = {'0': (0, 0, 0.0), '50': (50, 0, 0.0), '150': (150, 10, 2.0)}
CORR_LIMIT = {'0': 1.0, '50': 2.0, '150': 3.0}
# a side whose frame time p95 is above this ran on a busy machine: run once more (FEEL_RETRIES), then marked "slow"
FRAME_SLOW = 20.0
FEEL_RETRIES = 1
# the run-to-run noise of the numbers mp2.py diff compares (measured on two runs of one build, see the stage 0.4
# hand-back): an absolute allowance and a share of the old value, whichever is larger
NOISE = {
    'parity.entity': (3, 0.5), 'parity.radar': (2, 0.5), 'parity.hud': (1, 0.0), 'parity.sound': (5, 0.5),
    'parity.fx': (5, 0.5), 'parity.total': (8, 0.2), 'parity.audit': (0, 0.2), 'parity.mirror': (2, 0.0), 'parity.census': (2, 0.0),
    'feel.latency': (12.0, 0.15), 'feel.p95': (25.0, 0.25), 'feel.boost': (20.0, 0.3), 'feel.jitter': (0.5, 0.5),
    'feel.corr_per_min': (3.0, 0.0),
}


def default_device():
    return parity.pick_device()


def write_gate(a, gate, data, table):
    d = os.path.join(a.out, gate)
    os.makedirs(d, exist_ok=True)
    data = dict(data, gate=gate, build=a.build, finished=time.strftime('%Y-%m-%d %H:%M:%S'))
    with open(os.path.join(d, gate + '.json'), 'w') as f:
        json.dump(data, f, indent=1)
    with open(os.path.join(d, gate + '.md'), 'w') as f:
        f.write('\n'.join(table) + '\n')
    print('\n'.join(table))
    print('%s: %s (%s)' % (gate, data['status'], os.path.join(d, gate + '.json')), flush=True)
    return data


def run(cmd, env=None, log=None, timeout=None, real=False):
    """a tool; its exit code (its output into log, or through). real: without SCRAP_TIMESCALE (tools/test/gameclock.py)"""
    e = dict(os.environ)
    e.update(env or {})
    if real:
        e = gameclock.real_env(e)
    out = open(log, 'w') if log else None
    try:
        return subprocess.run(cmd, env=e, stdout=out, stderr=subprocess.STDOUT if out else None,
                              timeout=timeout).returncode
    except subprocess.TimeoutExpired:
        return 124
    finally:
        if out:
            out.close()


def run_real(cmd, **kw):
    """the gates that measure against the real clock (feel) or the baselines of single player (regress, golden,
    levels) run on the real clock"""
    return run(cmd, real=True, **kw)


# ---- parity ----

def gate_parity(a):
    out = os.path.join(a.out, 'parity')
    os.makedirs(out, exist_ok=True)
    cmd = [sys.executable, os.path.join(HERE, 'parity.py'), '--out', out, '--scratch', os.path.join(a.scratch, 'parity'),
           '--port', str(a.port), '--build', a.build, '--device', a.device, '--points', str(a.route_points),
           '--pvp-mode', a.pvp_mode]
    if a.levels:
        cmd += ['--levels', a.levels]
    if a.mp2:
        cmd.append('--mp2')
    if a.report_only:
        cmd.append('--report-only')
    run(cmd)
    try:
        sums = json.load(open(os.path.join(out, 'parity.json')))['levels']
    except (OSError, ValueError, KeyError):
        sums = []
    want = [x for x in (a.levels.split(',') if a.levels else parity.default_specs(a.pvp_mode)) if x]
    got = {s['tag'] for s in sums}
    missing = [parity.tag_of(s) for s in want if parity.tag_of(s) not in got]
    errors = [s for s in sums if 'error' in s]
    gaps = sum(parity.gap_total(s) for s in sums if 'error' not in s)
    status = 'error' if errors or missing else 'fail' if gaps else 'pass'
    table = ['# Parity (%d levels, %d points)' % (len(sums), sum(s['points'] for s in sums)), ''] + \
        parity.summary_table(sums) + ([''] + ['missing: ' + ', '.join(missing)] if missing else [])
    return write_gate(a, 'parity', {'status': status, 'levels': sums, 'missing': missing, 'gaps': gaps}, table)


# ---- census by class and weapons (stages 4.1 and 4.2) ----

def gate_classes(a):
    out = os.path.join(a.out, 'classes')
    cmd = [sys.executable, os.path.join(HERE, 'classes.py'), '--out', out, '--scratch', os.path.join(a.scratch, 'classes'),
           '--port', str(a.port), '--build', a.build, '--bot', a.bot, '--jobs', str(a.classes_jobs)]
    if a.levels:
        cmd += ['--levels', ','.join(x.split(':')[0] for x in a.levels.split(',') if ':' not in x)]
    if a.report_only:
        cmd.append('--report-only')
    run(cmd)
    try:
        data = json.load(open(os.path.join(out, 'classes.json')))
        table = open(os.path.join(out, 'classes.md')).read().splitlines()
    except (OSError, ValueError):
        data, table = {'status': 'error', 'levels': [], 'errors': ['no result']}, ['# Census by class: no result']
    return write_gate(a, 'classes', data, table)


WEAPONS = ('Vulcan', 'Laser', 'Devastator', 'Tesla', 'ATPC', 'Swarm', 'Inferno')


def gate_weapons(a):
    out = os.path.join(a.out, 'weapons')
    fight, sp = os.path.join(out, 'fight'), os.path.join(out, 'sp')
    weaps = [w for w in (a.weapons.split(',') if a.weapons else WEAPONS) if w]
    sh = os.path.join(HERE, 'weapons.sh')
    env = {'SCRAP_BUILD': a.build, 'WEAP_SCRATCH': os.path.join(a.scratch, 'weapons'), 'WEAP_PORT': str(a.port),
           'SCRAP_VK_DEVICE': a.device}
    if not a.report_only:
        os.makedirs(out, exist_ok=True)
        run_real(['bash', sh, 'fight', fight] + weaps, env=env, log=os.path.join(out, 'fight.log'))
        run_real(['bash', sh, 'sp', sp, 'none'] + weaps, env=env, log=os.path.join(out, 'sp.log'))
        run_real(['bash', sh, 'census', fight, sp] + weaps, env=env, log=os.path.join(out, 'census.log'))
    rows = []
    census = open(os.path.join(out, 'census.log')).read() if os.path.exists(os.path.join(out, 'census.log')) else ''
    for w in weaps:
        r = {'weapon': w, 'hits': None, 'deaths': None, 'mirror_a': None, 'mirror_b': None, 'census': None}
        try:
            text = open(os.path.join(fight, w + '.summary.txt')).read()
            m = re.search(r'(\d+) hits by players in the fight, (\d+) deaths', text)
            if m:
                r['hits'], r['deaths'] = int(m.group(1)), int(m.group(2))
            mm = re.findall(r'differences in all 3 pairs: (\d+)', text)
            if len(mm) == 2:
                r['mirror_a'], r['mirror_b'] = int(mm[0]), int(mm[1])
        except OSError:
            pass
        m = re.search(r'^%s: .*gaps \(on neither client\) (\d+)' % re.escape(w), census, re.M)
        if m:
            r['census'] = int(m.group(1))
        rows.append(r)
    missing = [r['weapon'] for r in rows if None in (r['hits'], r['mirror_a'], r['mirror_b'], r['census'])]
    bad = [r['weapon'] for r in rows if r['weapon'] not in missing and
           (not r['hits'] or not r['deaths'] or r['mirror_a'] or r['mirror_b'] or r['census'])]
    status = 'error' if missing else 'fail' if bad else 'pass'
    table = ['# Weapons (DownTown, a -mp2 shooter and target)', '',
             '| weapon | hits | deaths | mirror shooter / target | census gaps |', '|---|---|---|---|---|']
    table += ['| %s | %s | %s | %s / %s | %s |' % (r['weapon'], r['hits'], r['deaths'], r['mirror_a'], r['mirror_b'],
                                                  r['census']) for r in rows]
    if missing:
        table += ['', 'no result: ' + ', '.join(missing)]
    return write_gate(a, 'weapons', {'status': status, 'weapons': rows, 'missing': missing, 'failed': bad}, table)


# ---- feel ----

def feel_row(r, cell):
    """the numbers of one side of one cell"""
    def lat(k):
        x = r.get(k) or {}
        return x.get('latency_mean'), x.get('latency_p95'), x.get('samples', 0), x.get('misses', 0)
    w, f, b, x = lat('walk'), lat('fly'), lat('boost'), lat('fire')
    jw = ((r.get('walk') or {}).get('jitter_entity') or {}).get('rms')
    jf = ((r.get('fly') or {}).get('jitter_entity') or {}).get('rms')
    return {'cell': cell, 'side': r.get('side'), 'error': r.get('error'),
            'walk_mean': w[0], 'walk_p95': w[1], 'walk_n': w[2], 'fly_mean': f[0], 'fly_p95': f[1], 'fly_n': f[2],
            'boost_mean': b[0], 'boost_p95': b[1], 'boost_n': b[2], 'boost_misses': b[3], 'fire_mean': x[0],
            'fire_p95': x[1], 'fire_n': x[2], 'fire_misses': x[3], 'jitter_walk': jw,
            'jitter_fly': jf,
            'corr_per_min': r.get('corr_per_min'), 'corrections': r.get('corrections'), 'minutes': r.get('minutes'),
            'start_on_foot': r.get('start_on_foot'), 'boarded': r.get('boarded'),
            'frame_p95': (r.get('frames') or {}).get('p95'), 'bump': r.get('bump')}


def feel_marks(rows):
    """the over-limit marks of each row against the single player row"""
    sp = next((r for r in rows if r['side'] == 'sp'), None)
    for r in rows:
        r['over'] = []
        if r.get('frame_p95') and r['frame_p95'] > FRAME_SLOW:
            r['over'].append('slow frames (p95 %.1f ms)' % r['frame_p95'])
        if r['side'] == 'sp' or r.get('error'):
            continue
        for k in ('walk', 'fly', 'boost', 'fire'):
            v, ref = r.get(k + '_mean'), sp and sp.get(k + '_mean')
            if v is not None and ref is not None and v > ref + FRAME_MS:
                r['over'].append(k)
        for k in ('boost', 'fire'):
            if r.get(k + '_misses'):
                r['over'].append('%s missed %d' % (k, r[k + '_misses']))
        lim = CORR_LIMIT.get(r['cell'])
        if lim is not None and r.get('corr_per_min') is not None and r['corr_per_min'] > lim:
            r['over'].append('corrections')
    return rows


def feel_table(rows):
    def n(v, fmt='%.1f'):
        return '-' if v is None else fmt % v
    lines = ['| cell (delay/jitter/loss) | side | walk ms mean/p95 | fly ms mean/p95 | boost ms mean/p95 | '
             'fire ms mean/p95 | jitter walk/fly | corr/min | bump hits/tries, corr | frame p95 ms | over the limit |',
             '|---|---|---|---|---|---|---|---|---|---|---|']
    for r in rows:
        c = CELLS.get(r['cell'])
        cell = '-' if r['side'] == 'sp' else '%s ms (%d/%d/%g)' % (r['cell'], c[0], c[1], c[2]) if c else r['cell']
        if r.get('error'):
            lines.append('| %s | %s | error: %s | | | | | | | | |' % (cell, r['side'], r['error']))
            continue
        b = r.get('bump') or {}
        lines.append('| %s | %s | %s/%s | %s/%s | %s/%s | %s/%s | %s/%s | %s | %s | %s | %s |' % (
            cell, r['side'], n(r['walk_mean']), n(r['walk_p95']), n(r['fly_mean']), n(r['fly_p95']),
            n(r['boost_mean']) + (' (%d missed)' % r['boost_misses'] if r.get('boost_misses') else ''), n(r['boost_p95']),
            n(r.get('fire_mean')) + (' (%d missed)' % r['fire_misses'] if r.get('fire_misses') else ''),
            n(r.get('fire_p95')), n(r['jitter_walk'], '%.2f'), n(r['jitter_fly'], '%.2f'),
            n(r['corr_per_min'], '%.2f'),
            '%s/%s, %s' % (b.get('hits'), b.get('tries'), b.get('corrections')) if b.get('tries') else '-',
            n(r.get('frame_p95')), ', '.join(r.get('over', [])) or '-'))
    return lines


def gate_feel(a):
    out = os.path.join(a.out, 'feel')
    os.makedirs(out, exist_ok=True)
    base = [sys.executable, os.path.join(HERE, 'feel.py'), '--out', out, '--scratch', os.path.join(a.scratch, 'feel'),
            '--port', str(a.port), '--build', a.build, '--device', a.device, '--level', a.level,
            '--walks', str(a.walks), '--bumps', str(a.bumps), '--playlog'] + (['--mp2'] if a.mp2 else [])
    jobs = [('sp', 'sp', [])]
    for cell in [c for c in a.cells.split(',') if c]:
        d, j, l = CELLS[cell]
        for side in [s for s in a.sides.split(',') if s]:
            jobs.append((cell, side, ['--cell', cell, '--delay', str(d), '--jitter', str(j), '--loss', '%g' % l,
                                      '--fly-from', os.path.join(out, 'sp.json')]))
    rows = []
    for cell, side, extra in jobs:
        path = os.path.join(out, side + ('' if side == 'sp' else '-' + cell) + '.json')
        for k in range(1 if a.report_only else 1 + FEEL_RETRIES):
            if not a.report_only:
                run_real(base + ['--sides', side] + extra, log=os.path.join(out, 'feel-%s-%s.log' % (cell, side)),
                    timeout=1800)
            try:
                r = json.load(open(path))
            except (OSError, ValueError):
                r = {'side': side, 'error': 'no result'}
            # a run whose frames were slow (the machine busy with other games) measured the machine: once more
            if (r.get('frames') or {}).get('p95', 0) <= FRAME_SLOW:
                break
            print('feel %s %s: frame time p95 %.1f ms, again' % (cell, side, r['frames']['p95']), flush=True)
        rows.append(feel_row(r, cell))
        print('feel %s %s: %s' % (cell, side, r.get('error') or 'done'), flush=True)
    feel_marks(rows)
    errors = [r for r in rows if r.get('error')]
    over = [r for r in rows if r.get('over')]
    status = 'error' if errors else 'fail' if over else 'pass'
    table = ['# Feel (%s, %d walks, flights with the boost%s)' % (
             a.level, a.walks, ', %d flights into rail traffic' % a.bumps if a.bumps else ''), '',
             'Key to frame latency (ms from the real key press to the first drawn frame off the path), the boost as '
             'the action button (right mouse button in the ship), the fire button (left mouse button in the ship: '
             'to the first frame after the own ship\'s weapon fired), drawn jitter (RMS of the drawn own entity from a '
             'local fit, units), prediction corrections per minute. Limits: latency within one frame (%.1f ms) of '
             'single player, corrections per minute at most %s.' % (
                 FRAME_MS, ', '.join('%s at %s ms' % (v, k) for k, v in CORR_LIMIT.items())), ''] + feel_table(rows)
    return write_gate(a, 'feel', {'status': status, 'rows': rows, 'cells': {k: CELLS[k] for k in CELLS},
                                  'level': a.level}, table)


# ---- coop ----

def read_chapters():
    """tools/test/coop/chapters.txt: {name: (story point, level of the mission, what)}"""
    out = {}
    with open(os.path.join(COOP, 'chapters.txt')) as f:
        for line in f:
            line = line.strip()
            if line and not line.startswith('#'):
                p = line.split('|')
                out[p[0]] = (p[1], p[2], p[3] if len(p) > 3 else '')
    return out


def story_val(data, key):
    try:
        for line in open(os.path.join(data, 'party.story'), encoding='latin-1'):
            if line.startswith(key + '='):
                return line.rstrip('\n').split('=', 1)[1]
    except OSError:
        pass
    return ''


def level_name(name):
    for lv in parity.world_levels():
        if lv.lower() == name.lower():
            return lv
    return name


def gate_coop(a):
    out = os.path.join(a.out, 'coop')
    os.makedirs(out, exist_ok=True)
    chapters = read_chapters()
    if a.chapter not in chapters:
        print('mp2 coop: no chapter %s in %s (%s)' % (a.chapter, os.path.join(COOP, 'chapters.txt'),
                                                       ', '.join(sorted(chapters))))
        return write_gate(a, 'coop', {'status': 'error', 'chapter': a.chapter, 'error': 'unknown chapter'},
                          ['coop: unknown chapter %s' % a.chapter])
    point, level, what = chapters[a.chapter]
    data = os.path.join(a.points_dir, point)
    scratch = os.path.join(a.scratch, 'coop')
    tag = 'g' + a.chapter
    env = {'COOP_SCRATCH': scratch, 'COOP_KEYS': os.path.join(a.points_dir, 'keys'), 'BUILD': a.build}
    log = os.path.join(out, 'chain.log')
    t0 = time.time()
    if not a.report_only:
        os.makedirs(scratch, exist_ok=True)
        run(['bash', '-c', '. "$1/chainenv.sh"; exec "$1/autochain.sh" "$2" "$3" "$4" 1', 'chain', COOP, tag,
             str(a.port), data], env=env, log=log, timeout=a.coop_secs + 600)
    secs = round(time.time() - t0)
    steps = []
    # the chapter's step lines (autochain.sh's chap.sh output), kept next to the gate's JSON
    chap_out = os.path.join(out, 'chapter.out')
    if not a.report_only and os.path.exists(os.path.join(scratch, tag + '1.out')):
        with open(os.path.join(scratch, tag + '1.out')) as f, open(chap_out, 'w') as g:
            g.write(f.read())
    if a.report_only:
        try:
            secs = json.load(open(os.path.join(out, 'coop.json'))).get('secs', 0)
        except (OSError, ValueError):
            pass
    try:
        for line in open(chap_out):
            m = re.match(r'(ok|FAIL): (.*)', line.rstrip('\n'))
            if m:
                steps.append({'ok': m.group(1) == 'ok', 'step': m.group(2)})
    except OSError:
        pass
    end = os.path.join(scratch, tag + '-1-data')
    res = {'chapter': a.chapter, 'point': point, 'what': what, 'mission': story_val(data, 'Mission.Library'),
           'level': level, 'steps': steps, 'secs': secs, 'next': None, 'parity': []}
    reached = any(s['ok'] and 'the next mission' in s['step'] for s in steps) and (os.path.isdir(end) or
                                                                                   a.report_only)
    if reached and os.path.isdir(end):
        res['next'] = '%s in %s' % (story_val(end, 'Mission.Library'), story_val(end, 'Mission.Map'))
    elif reached:
        try:
            res['next'] = json.load(open(os.path.join(out, 'coop.json'))).get('next')
        except (OSError, ValueError):
            pass
    # coopparity at the chapter's story points: its start and (when reached) the start of the next mission
    cp_out = os.path.join(out, 'coopparity')
    pts = [('start', data, level)]
    if reached:
        nxt = (res['next'] or '').split(' in ')
        pts.append(('end', end, level_name(story_val(end, 'Mission.Map') or nxt[-1]) or level))
    if not a.no_coopparity:
        cmd = [sys.executable, os.path.join(HERE, 'coopparity.py'), '--out', cp_out, '--scratch',
               os.path.join(a.scratch, 'coopparity'), '--port', str(a.port), '--build', a.build, '--device', a.device]
        for name, d, lv in pts:
            cmd += ['--point', '%s=%s:%s:%s' % (name, d, lv, os.path.join(a.points_dir, 'keys', 'pa.key'))]
        if a.report_only:
            cmd.append('--report-only')
        run(cmd, log=os.path.join(out, 'coopparity.log'), timeout=3600)
        for name, d, lv in pts:
            try:
                r = json.load(open(os.path.join(cp_out, name + '.json')))
            except (OSError, ValueError):
                res['parity'].append({'point': name, 'level': lv, 'error': 'no result'})
                continue
            s = parity.summary(dict(r, points=[dict(r, name=name, anchor='member')] if 'join' in r else []))
            # single player at the story point: its Mission.Library is the story's (a failed load of the story
            # leaves it at Mission_JailBreak with that mission's page open: HUD gaps that are no client's)
            sp_ok = (r.get('sp_mission') or '').strip("'") == (r.get('mission') or '').split(' ')[0]
            res['parity'].append(dict(s, point=name, level=lv, mission=r.get('mission'), sp_ok=sp_ok))
    nfail = sum(1 for s in steps if not s['ok'])
    gaps = sum(p.get(c, 0) for p in res['parity'] if 'error' not in p for c in parity.SUMMARY_COLS)
    status = 'error' if not steps else 'fail' if nfail or not reached or gaps else 'pass'
    table = ['# Coop chapter %s: %s (%s, from %s)' % (a.chapter, what, res['mission'], point), '',
             '| step | result |', '|---|---|']
    table += ['| %s | %s |' % (s['step'], 'pass' if s['ok'] else 'FAIL') for s in steps] or ['| (no steps ran) | |']
    table += ['', 'Next mission: %s; %d s.' % (res['next'] or 'not reached', secs), '',
              '| story point | level | entity gaps | radar gaps | HUD gaps | sound gaps | particle gaps |',
              '|---|---|---|---|---|---|---|']
    for p in res['parity']:
        if 'error' in p:
            table.append('| %s | %s | error: %s | | | | |' % (p['point'], p['level'], p['error']))
        else:
            table.append('| %s (%s)%s | %s | %d | %d | %d | %d | %d |' % (p['point'], p.get('mission') or '-',
                                                                         '' if p.get('sp_ok', True) else
                                                                         ' (single player not at the story point)',
                                                                       p['level'], p['entity'], p['radar'],
                                                                       p['hud'], p['sound'], p['fx']))
    return write_gate(a, 'coop', dict(res, status=status, failed=nfail, gaps=gaps), table)


# ---- single player ----

def gate_sp(a):
    out = os.path.join(a.out, 'sp')
    os.makedirs(out, exist_ok=True)
    rows = []
    env = {'SCRAP_BUILD': a.build, 'SCRAP_VK_DEVICE': a.device, 'REGRESS_VK_DEVICE': a.device}
    if a.report_only:
        try:
            rows = json.load(open(os.path.join(out, 'sp.json')))['rows']
        except (OSError, ValueError, KeyError):
            rows = [{'check': 'sp', 'result': 'no result in %s' % out, 'pass': False, 'error': True}]
        a.checks = []
    if 'regress' in a.checks:
        log = os.path.join(out, 'regress.log')
        rc = run_real([sys.executable, os.path.join(REPO, 'tools', 'test', 'regress', 'regress.py'), 'check', '--build',
                  a.build, '--lanes', a.lanes, '--out', os.path.join(a.scratch, 'regress')] +
                 [c for c in (a.regress_cases or '').split(',') if c], env=env, log=log)
        text = open(log, errors='replace').read()
        m = re.findall(r'regress: (\d+)/(\d+) pass, (\d+) fail[^\n(]*(?:\(([^)]*)\))?', text)
        if m:
            p, n, f, failed = m[-1]
            rows.append({'check': 'regress', 'result': '%s/%s pass' % (p, n), 'pass': rc == 0, 'passed': int(p),
                         'total': int(n), 'failed': failed.split()})
        else:
            rows.append({'check': 'regress', 'result': 'no summary (exit %d)' % rc, 'pass': False, 'error': True})
    if 'golden' in a.checks:
        log = os.path.join(out, 'golden.log')
        rc = run_real([os.path.join(REPO, 'tools', 'test', 'golden.sh')], env=env, log=log)
        tail = [x for x in open(log, errors='replace').read().splitlines() if x.startswith('golden')]
        rows.append({'check': 'golden', 'result': (tail[-1] if tail else 'exit %d' % rc)[:160], 'pass': rc == 0})
    if 'levels' in a.checks:
        asan = a.asan_build or a.build + '-asan'
        if not os.path.exists(os.path.join(asan, 'ScrapC')):
            rows.append({'check': 'levels', 'result': 'n/a (no ASan build %s)' % asan, 'pass': True, 'skipped': True})
        else:
            log = os.path.join(out, 'levels.log')
            rc = run_real([os.path.join(REPO, 'tools', 'test', 'levels.sh')],
                     env=dict(env, SCRAP_BUILD=asan, SCRAP_VK_DEVICE=a.asan_device, LEVELS_TIME=str(a.levels_time),
                              LEVELS_JOBS=str(a.levels_jobs),
                              SCRAP_GAME_DIR=os.path.join(a.scratch, 'levels-game')), log=log)
            lines = open(log, errors='replace').read().splitlines()
            fails = [x.split()[0] for x in lines if ' FAIL' in x]
            ok = sum(1 for x in lines if x.endswith(' ok'))
            rows.append({'check': 'levels', 'result': '%d ok, %d fail%s' % (ok, len(fails), (' (' + ' '.join(fails) +
                                                                                       ')') if fails else ''),
                         'pass': rc == 0, 'failed': fails})
    status = 'error' if any(r.get('error') for r in rows) else 'pass' if all(r['pass'] for r in rows) else 'fail'
    table = ['# Single player', '', '| check | result | |', '|---|---|---|']
    table += ['| %s | %s | %s |' % (r['check'], r['result'], 'pass' if r['pass'] else 'FAIL') for r in rows]
    return write_gate(a, 'sp', {'status': status, 'rows': rows}, table)


# ---- all and diff ----

def gate_all(a):
    res = []
    for gate, fn in (('sp', gate_sp), ('parity', gate_parity), ('classes', gate_classes), ('weapons', gate_weapons),
                     ('feel', gate_feel), ('coop', gate_coop)):
        if gate == 'coop' and not a.chapter:
            continue
        try:
            r = fn(a)
        except Exception as ex:  # noqa: BLE001 (one gate that breaks is a row of the summary)
            r = {'gate': gate, 'status': 'error', 'error': '%s: %s' % (type(ex).__name__, ex)}
        res.append(r)
    lines = ['# mp2 gates', '', '| gate | status | numbers |', '|---|---|---|']
    for r in res:
        lines.append('| %s | %s | %s |' % (r['gate'], r['status'], gate_numbers(r)))
    status = 'error' if any(r['status'] == 'error' for r in res) else 'fail' if any(
        r['status'] == 'fail' for r in res) else 'pass'
    os.makedirs(a.out, exist_ok=True)
    with open(os.path.join(a.out, 'summary.json'), 'w') as f:
        json.dump({'status': status, 'gates': [{'gate': r['gate'], 'status': r['status'],
                                                'numbers': gate_numbers(r)} for r in res]}, f, indent=1)
    with open(os.path.join(a.out, 'summary.md'), 'w') as f:
        f.write('\n'.join(lines) + '\n')
    print('\n'.join(lines))
    return {'status': status}


def gate_numbers(r):
    g = r.get('gate')
    if r.get('error') and g != 'coop':
        return 'error: ' + str(r['error'])
    if g == 'parity':
        sums = [s for s in r.get('levels', []) if 'error' not in s]
        au = [s['audit'] for s in sums if s.get('audit') is not None]
        return '%d levels, gaps entity %d radar %d HUD %d sound %d particle %d, audit %s%s' % (
            len(sums), *[sum(s[c] for s in sums) for c in parity.SUMMARY_COLS], sum(au) if au else 'n/a',
            ', %d levels with errors' % (len(r['levels']) - len(sums)) if len(r.get('levels', [])) > len(sums) else '') + (
            ', mirror %d, census %d' % tuple(sum(s.get(k) or 0 for s in sums) for k in ('mirror', 'census'))
            if any(s.get('mp2') for s in sums) else '')
    if g == 'feel':
        return '; '.join('%s %s over: %s' % (x['cell'], x['side'], ','.join(x['over']))
                         for x in r.get('rows', []) if x.get('over')) or 'all cells within the limits'
    if g == 'coop':
        return '%s: %d steps, %d failed, next %s, coopparity gaps %s' % (
            r.get('chapter'), len(r.get('steps', [])), r.get('failed', 0), r.get('next') or 'not reached',
            r.get('gaps'))
    if g == 'sp':
        return '; '.join('%s %s' % (x['check'], x['result']) for x in r.get('rows', []))
    if g == 'classes':
        lv = r.get('levels', [])
        return '%d levels: mirror %d, class gaps %d, definitions %d, client sim/create/purge/noclass %d, pres %d, ' \
            'unresolved player reads %d, crashes %d%s' % (
                len(lv), *[sum(x.get(k) or 0 for x in lv) for k in ('mirror', 'classes', 'defs')],
                              sum(sum(x.get(k) or 0 for k in ('start_sim', 'start_create', 'start_purge', 'start_noclass',
                                                              'sim', 'create', 'purge', 'noclass')) for x in lv),
                              sum(sum(x.get(k) or 0 for k in ('unforwarded', 'double', 'missing')) for x in lv),
                              sum(x.get('norule') or 0 for x in lv), sum(x.get('crash') or 0 for x in lv),
                              ', could not run: ' + ','.join(r['errors']) if r.get('errors') else '')
    if g == 'weapons':
        return '; '.join('%s %s hits %s deaths, mirror %s/%s, census %s' % (
            x['weapon'], x['hits'], x['deaths'], x['mirror_a'], x['mirror_b'], x['census']) for x in r.get('weapons', []))
    return ''


def noisy(key, old, new):
    """a difference within the noise of NOISE[key]"""
    if old is None or new is None or not all(isinstance(x, (int, float)) and not isinstance(x, bool)
                                             for x in (old, new)):
        return old == new
    ab, share = NOISE.get(key, (0, 0.0))
    return abs(new - old) <= max(ab, share * abs(old))


def gate_diff(a):
    rows, real = [], 0

    def add(what, key, old, new):
        nonlocal real
        same = noisy(key, old, new)
        if not same:
            real += 1
        if old != new:
            rows.append((what, old, new, 'noise' if same else 'DIFFERENT'))

    def load(d, gate):
        try:
            return json.load(open(os.path.join(d, gate, gate + '.json')))
        except (OSError, ValueError):
            return None
    for gate in ('parity', 'feel', 'coop', 'sp'):
        o, n = load(a.old, gate), load(a.new, gate)
        if not o or not n:
            continue
        if o.get('status') != n.get('status'):
            rows.append(('%s status' % gate, o.get('status'), n.get('status'), 'see below'))
        if gate == 'parity':
            om = {s['tag']: s for s in o['levels']}
            for s in n['levels']:
                p = om.get(s['tag'])
                if not p:
                    continue
                if ('error' in p) != ('error' in s):
                    add('parity %s ran' % s['tag'], 'x', 'error' if 'error' in p else 'ok',
                        'error' if 'error' in s else 'ok')
                    continue
                for c in parity.SUMMARY_COLS + ('audit', 'mirror', 'census'):
                    add('parity %s %s' % (s['tag'], c), 'parity.' + c, p.get(c), s.get(c))
            # the columns over the levels both runs have: a change that is noise in each level can add up
            both = [t for t in n['levels'] if t['tag'] in om and 'error' not in t and 'error' not in om[t['tag']]]
            for c in parity.SUMMARY_COLS:
                add('parity all %s' % c, 'parity.total', sum(om[t['tag']][c] for t in both), sum(t[c] for t in both))
        elif gate == 'feel':
            om = {(r['cell'], r['side']): r for r in o['rows']}
            for r in n['rows']:
                p = om.get((r['cell'], r['side']))
                if not p:
                    continue
                for c, key in (('walk_mean', 'feel.latency'), ('fly_mean', 'feel.latency'), ('walk_p95', 'feel.p95'),
                               ('boost_mean', 'feel.boost'), ('boost_p95', 'feel.boost'),
                               ('fire_mean', 'feel.boost'), ('fire_p95', 'feel.boost')):
                    add('feel %s %s %s' % (r['cell'], r['side'], c), key, p.get(c), r.get(c))
                for c in ('jitter_walk', 'jitter_fly'):
                    add('feel %s %s %s' % (r['cell'], r['side'], c), 'feel.jitter', p.get(c), r.get(c))
                add('feel %s %s corr/min' % (r['cell'], r['side']), 'feel.corr_per_min', p.get('corr_per_min'),
                    r.get('corr_per_min'))
                add('feel %s %s over' % (r['cell'], r['side']), 'x', ','.join(p.get('over', [])),
                    ','.join(r.get('over', [])))
        elif gate == 'coop':
            os_ = {s['step']: s['ok'] for s in o.get('steps', [])}
            for s in n.get('steps', []):
                if s['step'] in os_:
                    add('coop step %s' % s['step'], 'x', os_[s['step']], s['ok'])
            add('coop next mission', 'x', o.get('next'), n.get('next'))
            op = {p['point']: p for p in o.get('parity', [])}
            for p in n.get('parity', []):
                q = op.get(p['point'])
                if q and not (q.get('sp_ok', True) and p.get('sp_ok', True)):
                    rows.append(('coopparity %s' % p['point'], 'sp ok' if q.get('sp_ok', True) else 'sp off',
                                 'sp ok' if p.get('sp_ok', True) else 'sp off',
                                 'not compared (single player not at the story point)'))
                elif q:
                    for c in parity.SUMMARY_COLS:
                        add('coopparity %s %s' % (p['point'], c), 'parity.' + c, q.get(c), p.get(c))
        elif gate == 'sp':
            om = {r['check']: r for r in o['rows']}
            for r in n['rows']:
                p = om.get(r['check'])
                if p:
                    add('sp %s' % r['check'], 'x', p.get('pass'), r.get('pass'))
                    if 'failed' in r and 'failed' in p:
                        add('sp %s failed' % r['check'], 'x', ' '.join(sorted(p['failed'])),
                            ' '.join(sorted(r['failed'])))
    lines = ['# mp2 diff', '', 'old %s, new %s: %d real differences, %d within the noise' % (
        a.old, a.new, real, sum(1 for r in rows if r[3] == 'noise')), '', '| number | old | new | |', '|---|---|---|---|']
    lines += ['| %s | %s | %s | %s |' % r for r in rows]
    print('\n'.join(lines))
    with open(os.path.join(a.new, 'diff.md'), 'w') as f:
        f.write('\n'.join(lines) + '\n')
    return {'status': 'fail' if real else 'pass'}


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('gate', choices=('parity', 'feel', 'coop', 'sp', 'classes', 'weapons', 'all', 'diff'))
    ap.add_argument('args', nargs='*', help='coop and all: the chapter; diff: <old out> <new out>')
    ap.add_argument('--out', default=os.path.join(REPO, 'build-linux64', 'mp2'))
    ap.add_argument('--scratch', default=None, help='game folders and logs (default <out>/games)')
    ap.add_argument('--build', default=os.path.join(REPO, 'build-linux64'))
    ap.add_argument('--port', type=int, default=29940)
    ap.add_argument('--device', default=None)
    ap.add_argument('--report-only', action='store_true', help='the tables of the JSON files already in --out')
    ap.add_argument('--mp2', action='store_true', help='parity and feel on the replication v2 (-mp2 for the host, its '
                    'instance and the joined client); parity adds the mirror diff and the sound census')
    # parity
    ap.add_argument('--levels', default=None, help='parity: Level or Level:mode, comma separated')
    ap.add_argument('--points', dest='route_points', type=int, default=4, help='parity: the points of a route at most')
    ap.add_argument('--pvp-mode', default=parity.PVP_MODE)
    # classes, weapons
    ap.add_argument('--bot', default='idle', help='classes: SCRAP_PLAYBOT of the joined client (idle, npc, walk)')
    ap.add_argument('--classes-jobs', type=int, default=4, help='classes: levels at the same time (ports from --port)')
    ap.add_argument('--weapons', default=None, help='weapons: comma separated (default all seven)')
    # feel
    ap.add_argument('--cells', default='0,50,150')
    ap.add_argument('--sides', default='join,host', help='feel: the multiplayer sides of each cell (join, host)')
    ap.add_argument('--level', default='DownTown')
    ap.add_argument('--walks', type=int, default=12)
    ap.add_argument('--bumps', type=int, default=0, help='feel: flights into the path of rail traffic per side')
    # coop
    ap.add_argument('--points-dir', dest='points_dir', default=os.environ.get('MP2_COOP_POINTS', ''),
                    help='coop: the story points (<name>-data folders and keys/pa.key, keys/pb.key; MP2_COOP_POINTS)')
    ap.add_argument('--coop-secs', type=int, default=1800)
    ap.add_argument('--no-coopparity', action='store_true')
    # single player
    ap.add_argument('--checks', default='regress,golden,levels')
    ap.add_argument('--lanes', default='s04a,s04b,s04c', help='sp: the lanes (MCP slots) of regress.py')
    ap.add_argument('--regress-cases', default=None, help='sp: only these regress cases (comma separated)')
    ap.add_argument('--asan-build', default=None, help='the ASan build of levels.sh (default <build>-asan)')
    ap.add_argument('--asan-device', default='1002:164e', help='levels.sh: the Vulkan device of the ASan games (the '
                    'AMD: the NVIDIA driver raises a signal in the swapchain under ASan when a game is stopped)')
    ap.add_argument('--levels-time', type=int, default=30)
    ap.add_argument('--levels-jobs', type=int, default=6)
    a = ap.parse_args()
    a.build = os.path.abspath(a.build)
    a.out = os.path.abspath(a.out)
    a.scratch = os.path.abspath(a.scratch or os.path.join(a.out, 'games'))
    a.device = a.device or default_device()
    a.chapter = a.args[0] if a.gate in ('coop', 'all') and a.args else None
    if a.gate in ('coop', 'all'):
        if a.gate == 'coop' and not a.chapter:
            ap.error('coop needs a chapter (tools/test/coop/chapters.txt)')
        if a.chapter and not a.points_dir:
            ap.error('coop needs the story points: --points-dir or MP2_COOP_POINTS')
    a.checks = a.checks.split(',')
    if a.gate == 'diff':
        if len(a.args) != 2:
            ap.error('diff needs <old out> <new out>')
        a.old, a.new = (os.path.abspath(x) for x in a.args)
        r = gate_diff(a)
    else:
        r = {'parity': gate_parity, 'feel': gate_feel, 'coop': gate_coop, 'sp': gate_sp, 'classes': gate_classes,
             'weapons': gate_weapons, 'all': gate_all}[a.gate](a)
    sys.exit({'pass': 0, 'fail': 1}.get(r['status'], 2))


if __name__ == '__main__':
    main()
