#!/usr/bin/env python3
"""The parity tool of the free roam multiplayer (stage M10 of issue #1): what a multiplayer client shows against
single player at the same place of a level.

For each level it starts (rendered, hidden, muted, in their own game folders under --scratch):
  sp    the single player game (-imap:Levels/<level>)
  host  a hosting game (-host:<level> -mode:freeroam): the coordinator in the game, the level's instance a child,
        the host plays as a client over loopback
  join  a second game that joins that world (-join)
puts the players at the same place (the host's spawn point: the single player's player and the joined player are
moved there through the test channel, the joined one on the instance, src/game/Global/TestChannel.c -i), waits and
compares through the test channel: the entities around the place by kind (entinfo; a replicated proxy is labelled by
the instance's entity it stands for), the icons of the radar (radar), the HUD items (menu), the sounds playing
(sounds) and the particle systems drawn (fx), plus screenshots side by side. Output in --out: <level>.json,
<level>-shots.png and report.md (the gap report: kind, count single player against host and joined client,
examples).

  parity.py [--levels DownTown,Outskirts,...] [--port 29940] [--build build-linux64] [--out DIR] [--scratch DIR]
            [--radius 100000] [--settle 25] [--no-join]

Needs a build with the M10 test channel verbs (entinfo, radar, fx, sounds, the instance files). Network only on
loopback (the ports --port .. --port + 40)."""
import argparse
import json
import os
import re
import shutil
import signal
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
sys.path.insert(0, os.path.join(REPO, 'tools', 'test'))
import channel  # noqa: E402

CITY = ['DownTown', 'Outskirts', 'Commercial', 'Scrapyard', 'GDB', 'Police', 'Bank', 'Press', 'Temple', 'TownHall',
        'GamblinDen']
RAIL = ('NormalTraffic', 'MediumTraffic', 'HeavyTraffic')
FREE = ('Traffic', 'FightingTraffic', 'ChasingTraffic', 'EnemiesTraffic')
# kinds whose count changes with time (traffic, effects of fights): compared loosely
# the frames of the blinking mission marks (RadarScorer.c Radar_DrawMission) as their first one
RADAR_BLINK = {'13': '12', '14': '12', '10': '9', '11': '9'}
LOOSE = ('rail traffic', 'traffic', 'police', 'coin', 'missile', 'npc', 'parked ship')


class Game:
    """a rendered game in its own game folder (tools/test/native.sh through setsid; ended by its environment)"""

    def __init__(self, a, name, args, env=None):
        self.a, self.name, self.args = a, name, args
        self.dir = os.path.join(a.scratch, 'g-' + name)
        self.log = os.path.join(a.scratch, name + '.log')
        self.env = env or {}
        self.proc = None

    def start(self):
        # a fresh world each time: a hosting game keeps its players (their level) in its data folder
        shutil.rmtree(os.path.join(self.dir, 'xdg-data', 'MercurySteam', 'Scrapland', 'host'), ignore_errors=True)
        if not os.path.isdir(os.path.join(self.dir, 'Bin')):
            subprocess.run([os.path.join(REPO, 'tools/test/mkgame.sh'), '--native', self.dir],
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=True)
        for f in os.listdir(self.dir):
            if f.startswith('test_cmd') or f.startswith('test_reply'):
                os.remove(os.path.join(self.dir, f))
        with open(os.path.join(self.dir, 'Bin', 'scrapdecomp.ini'), 'w') as f:
            f.write('[general]\ntestmod=1\nworldport=%d\nlanaddr=127.0.0.1\nhostbind=127.0.0.1\n' % self.a.port)
        env = dict(os.environ)
        env.update(TMPDIR=os.path.join(self.a.scratch, 'tmp'), SCRAP_GAME_DIR=self.dir, SCRAP_VK_DEVICE=self.a.device,
                   HEADLESS_TIMEOUT=str(self.a.timeout), NATIVE_LOG=self.log, SCRAP_BUILD=self.a.build,
                   SCRAP_TEST_CHANNEL='1', SCRAP_LAN_ADDR='127.0.0.1',
                   XDG_DATA_HOME=os.path.join(self.dir, 'xdg-data'))
        env.update(self.env)
        os.makedirs(env['TMPDIR'], exist_ok=True)
        self.proc = subprocess.Popen([os.path.join(REPO, 'tools/test/native.sh'), '-fullscreen:0'] + self.args,
                                     env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                                     start_new_session=True)

    def ask(self, lines, inst=None, timeout=20.0):
        """the answers of the test channel lines (of the instance of level inst in this folder)"""
        save = channel.CMD, channel.REPLY, channel.TMP
        if inst:
            channel.use_instance(inst)
        try:
            return channel.send(self.dir, '\n'.join(lines) if isinstance(lines, list) else lines, timeout=timeout)
        finally:
            channel.CMD, channel.REPLY, channel.TMP = save

    def try_ask(self, lines, inst=None, timeout=3.0):
        try:
            return self.ask(lines, inst, timeout)
        except channel.NoAnswer:
            return None

    def shot(self, out):
        """a screenshot of its gamescope (found by the game folder in the environment of its processes)"""
        for pid in os.listdir('/proc'):
            if not pid.isdigit():
                continue
            try:
                env = open('/proc/%s/environ' % pid, 'rb').read().split(b'\0')
            except OSError:
                continue
            if ('SCRAP_GAME_DIR=' + self.dir).encode() not in env:
                continue
            disp = [e[len(b'GAMESCOPE_WAYLAND_DISPLAY='):].decode() for e in env
                    if e.startswith(b'GAMESCOPE_WAYLAND_DISPLAY=')]
            if not disp:
                continue
            if os.path.exists(out):
                os.remove(out)
            subprocess.run(['gamescopectl', 'screenshot', out], env=dict(os.environ, GAMESCOPE_WAYLAND_DISPLAY=disp[0]),
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=10)
            for _ in range(30):
                if os.path.exists(out) and os.path.getsize(out) > 0:
                    return out
                time.sleep(0.2)
            return None
        return None

    def stop(self):
        stop_dir(self.dir)


def stop_dir(d):
    """ends every process with SCRAP_GAME_DIR=d (the game, its gamescope, a host's instance children); SIGKILL after
    3 s (gamescope on the RTX ignores SIGTERM)"""
    pids = []
    for pid in os.listdir('/proc'):
        if not pid.isdigit() or int(pid) == os.getpid():
            continue
        try:
            if ('SCRAP_GAME_DIR=' + d).encode() in open('/proc/%s/environ' % pid, 'rb').read().split(b'\0'):
                pids.append(int(pid))
        except OSError:
            pass
    for sig in (signal.SIGTERM, signal.SIGKILL):
        for p in pids:
            try:
                os.kill(p, sig)
            except OSError:
                pass
        end = time.time() + 3
        while time.time() < end and any(os.path.exists('/proc/%d' % p) for p in pids):
            time.sleep(0.1)


def parse_ents(text):
    """entinfo lines to dicts"""
    out = []
    for line in (text or '').splitlines():
        f = line.split('\t')
        if len(f) < 12:
            continue
        out.append(dict(name=f[0], cls=f[1], list=f[2], active=int(f[3]), model=f[4], act=f[5],
                        pos=(float(f[6]), float(f[7]), float(f[8])), tg=int(f[9]), radar=int(f[10]),
                        proxy=int(f[11]), owner=int(f[12]) if len(f) > 12 else -1))
    return out


def base(model):
    return os.path.splitext(os.path.basename(model))[0] if model else ''


def kind_of(e):
    """the kind of an entity of the world by its class, entity list, name and model; None for what is not compared
    (cameras, the players and their own ships)"""
    n, cls, lst = e['name'], e['cls'], e['list']
    if cls in ('FCam', 'Cam', 'CamPath') or re.match(r'^(Me|NetPlayer)\d', n):
        return None
    if n in ('Player0', 'PlayerShip') or lst == 'Players':
        return None
    if lst in RAIL:
        return 'rail traffic'
    if lst in FREE:
        return 'traffic'
    if lst == 'OutCop' or n.startswith('Police') or lst.startswith('Police'):
        return 'police'
    if n.startswith('Parked'):
        return 'parked ship'
    if lst == 'ParkingBox' or n.startswith('ParkParking'):
        return 'parking box'
    if n.startswith('DM_AreaToPark'):
        return 'parking area'
    if n.startswith('Accelerator'):
        return 'accelerator'
    if n.startswith('DM_OutLink'):
        return 'level link'
    if lst == 'PhoneCabs' or n.startswith('DM_Phone'):
        return 'phone cab' if n.endswith('PhoneCab') else 'phone'
    if n.startswith('Usr_Coin'):
        return 'coin'
    if cls == 'item':
        return 'item ' + (base(e['model']) or re.sub(r'[\d_]+$', '', n))
    if cls == 'Missile':
        return 'missile'
    if cls == 'WithLife' and e['act'].startswith('Door'):
        return 'door ' + e['act']
    if cls == 'WalkChar':
        return 'npc ' + (e['act'] or base(e['model']))
    if cls == 'Car':
        return 'ship ' + (e['act'] or base(e['model']))
    return '%s %s' % (cls, re.sub(r'[\d_]+$', '', n))


def dist(a, b):
    return sum((x - y) ** 2 for x, y in zip(a, b)) ** 0.5


def label_proxies(ents, inst):
    """a proxy takes the entity list and name of the instance's entity it stands for: same model, nearest (the proxy
    is drawn ~100 ms behind)"""
    for e in ents:
        if not e['proxy']:
            continue
        best, bd = None, 6000.0
        for s in inst:
            if s['model'].lower() != e['model'].lower():
                continue
            d = dist(s['pos'], e['pos'])
            if d < bd:
                best, bd = s, d
        if best:
            e['name'], e['list'], e['srv'] = best['name'], best['list'], best['name']
        else:
            e['name'] = 'proxy?' + base(e['model'])


def count(ents, at, radius):
    kinds = {}
    for e in ents:
        if dist(e['pos'], at) > radius:
            continue
        k = kind_of(e)
        if k is None:
            continue
        d = kinds.setdefault(k, {'n': 0, 'active': 0, 'names': []})
        d['n'] += 1
        d['active'] += e['active']
        d['names'].append(e['name'])
    return kinds


def own_entity(ents):
    for e in ents:
        if e['cls'] == 'WalkChar' and e['name'].startswith('Me') and not e['proxy']:
            return e
    for e in ents:
        if e['cls'] == 'Car' and not e['proxy'] and e['name'].startswith('Me'):
            return e
    return None


def hist(lines, key):
    h = {}
    for line in (lines or '').splitlines():
        f = line.split('\t')
        if len(f) < 2:
            continue
        k = key(f)
        h[k] = h.get(k, 0) + 1
    return h


def collect(g, at, radius, inst=None):
    out = {}
    out['ents'] = parse_ents(g.ask('entinfo %.0f %.0f %.0f %.0f' % (at[0], at[1], at[2], radius), timeout=30))
    if inst is not None:
        label_proxies(out['ents'], inst)
    out['radar'] = hist(g.ask('radar'), lambda f: 'icon %s%s' % (RADAR_BLINK.get(f[0], f[0]), ' rim' if f[1] == '1' else ''))
    out['fx'] = hist(g.ask('fx'), lambda f: f[0])
    out['sounds'] = hist(g.ask('sounds'), lambda f: '%s %s' % (f[0], f[2] if f[0] == 'ch' else f[1]))
    menu = g.ask('menu') or ''
    out['hud'] = sorted(set(re.findall(r"\[\s*'([^']*)'", menu)))
    out['kinds'] = count(out['ents'], at, radius)
    return out


def wait_for(fn, secs, step=1.0):
    end = time.time() + secs
    while time.time() < end:
        r = fn()
        if r:
            return r
        time.sleep(step)
    return None


def side_by_side(paths, labels, out):
    try:
        from PIL import Image, ImageDraw
    except ImportError:
        return None
    ims = [Image.open(p).convert('RGB').resize((960, 540)) if p and os.path.exists(p) else None for p in paths]
    w = max((i.width for i in ims if i), default=0)
    h = max((i.height for i in ims if i), default=0)
    if not w:
        return None
    sheet = Image.new('RGB', (w * len(ims), h + 28), (0, 0, 0))
    d = ImageDraw.Draw(sheet)
    for k, (im, lab) in enumerate(zip(ims, labels)):
        if im:
            sheet.paste(im, (k * w, 28))
        d.text((k * w + 8, 6), lab, fill=(255, 255, 0))
    sheet.save(out)
    return out


def run_level(a, level):
    """one level: the three games, the place, the collection; the result dict"""
    port = a.port
    res = {'level': level, 'radius': a.radius}
    host = Game(a, 'ph', ['-host:' + level, '-port:%d' % port, '-mode:freeroam', '-name:Host'])
    sp = Game(a, 'ps', ['-imap:Levels/' + level])
    join = Game(a, 'pj', ['-join:127.0.0.1', '-port:%d' % port, '-name:Join']) if a.join else None
    games = [g for g in (host, sp, join) if g]
    try:
        host.start()
        sp.start()

        def host_own():
            ents = parse_ents(host.try_ask('entinfo', timeout=5))
            return own_entity(ents)
        me = wait_for(host_own, 240, 2)
        if not me:
            res['error'] = 'the host has no own entity'
            return res
        # where it stays (a fall from a bad spawn point respawns it elsewhere, Replicate.c)
        still, end = 0, time.time() + 40
        while still < 4 and time.time() < end:
            time.sleep(1.0)
            now = host_own() or me
            still = still + 1 if dist(now['pos'], me['pos']) < 50.0 else 0
            me = now
        at = me['pos']
        res['at'] = at
        m = re.search(r"\('Ang', \(([-\d.e]+), ([-\d.e]+), ([-\d.e]+)\)\)", host.ask('ent %s Ang' % me['name']))
        ang = tuple(float(x) for x in m.groups()) if m else (0.0, 0.0, 0.0)
        res['ang'] = ang
        if join:
            join.start()
        # the single player's player to the place
        wait_for(lambda: sp.try_ask('Scrap.GetLevelPath()', timeout=5), 180, 2)
        sp.ask('do e = Scrap.UsrEntity(0); e.Pos = (%.1f, %.1f, %.1f); e.Ang = (%.3f, %.3f, %.3f)' % (at + ang))
        inst_me = None
        if join:
            def join_own():
                return own_entity(parse_ents(join.try_ask('entinfo', timeout=5)))
            jme = wait_for(join_own, 240, 2)
            # (a drop in the first seconds after a spawn counts as a fall from a bad spawn point, Replicate.c)
            time.sleep(6)
            jme = join_own() or jme
            # the joined player beside the host's (its instance entity: the NetPlayer nearest to the joined client's
            # own entity)
            iel = parse_ents(host.try_ask('entinfo', inst=level, timeout=10))
            if jme:
                cands = [e for e in iel if e['name'].startswith('NetPlayer')]
                cands.sort(key=lambda e: dist(e['pos'], jme['pos']))
                if cands:
                    inst_me = cands[0]['name']
                    host.ask('do e = Scrap.GetEntity("%s"); e.Pos = (%.1f, %.1f, %.1f); e.Ang = (%.3f, %.3f, %.3f)' %
                             ((inst_me, at[0] + 400.0, at[1], at[2]) + ang), inst=level)
            res['join_own'] = bool(jme)
        time.sleep(a.settle)
        inst = parse_ents(host.ask('entinfo %.0f %.0f %.0f %.0f' % (at[0], at[1], at[2], a.radius * 1.2),
                                   inst=level, timeout=30))
        res['instance'] = {'kinds': count(inst, at, a.radius)}
        res['sp'] = collect(sp, at, a.radius)
        res['host'] = collect(host, at, a.radius, inst)
        if join:
            res['join'] = collect(join, at, a.radius, inst)
        shots = []
        for g in [g for g in (sp, host, join) if g]:
            p = os.path.join(a.out, '%s-%s.png' % (level, g.name))
            shots.append(g.shot(p))
        res['sheet'] = side_by_side(shots, ['single player', 'host (client of its own world)', 'joined client'],
                                    os.path.join(a.out, '%s-shots.png' % level))
        for s in shots:
            if s:
                os.remove(s)
        for side in ('sp', 'host', 'join'):
            if side in res:
                del res[side]['ents']
    finally:
        for g in games:
            g.stop()
    return res


def gaps(res):
    """the rows of the gap report: kind, sp, host, join, gap flag, missing examples"""
    rows = []
    sp = res.get('sp', {}).get('kinds', {})
    sides = [s for s in ('host', 'join') if s in res]
    kinds = set(sp)
    for s in sides:
        kinds |= set(res[s]['kinds'])
    for k in sorted(kinds):
        n_sp = sp.get(k, {}).get('n', 0)
        a_sp = sp.get(k, {}).get('active', 0)
        cnt = [res[s]['kinds'].get(k, {}).get('active', 0) for s in sides]
        loose = any(k.startswith(x) for x in LOOSE)
        worst = min(cnt) if cnt else 0
        if loose:
            gap = a_sp > 0 and (worst == 0 or worst < a_sp * 0.5)
        else:
            gap = worst < a_sp
        extra = cnt and max(cnt) > a_sp and not loose
        names = set(sp.get(k, {}).get('names', []))
        for s in sides:
            names -= set(res[s]['kinds'].get(k, {}).get('names', []))
        rows.append((k, n_sp, a_sp, cnt, gap, extra, sorted(names)[:4]))
    return rows


def hist_gaps(res, key):
    sp = res.get('sp', {}).get(key, {})
    sides = [s for s in ('host', 'join') if s in res]
    out = []
    keys = set(sp)
    for s in sides:
        keys |= set(res[s].get(key, {}))
    for k in sorted(keys):
        v = [sp.get(k, 0)] + [res[s].get(key, {}).get(k, 0) for s in sides]
        if v[0] != min(v) or v[0] != max(v):
            out.append((k, v))
    return out


def report(results, out):
    lines = ['# Parity report (M10)', '',
             'Single player (SP) against the multiplayer clients at the same place: the host (a -host game, client of '
             'its own world over loopback) and a joined client. Counts are active entities within the radius of '
             'the place, by kind; "gap" marks a kind the clients show less of (loose kinds such as traffic: less '
             'than half), "extra" more. Generated by tools/test/parity/parity.py.', '']
    total = 0
    for res in results:
        lines.append('## %s' % res['level'])
        if 'error' in res:
            lines += ['', 'error: ' + res['error'], '']
            continue
        rows = gaps(res)
        ng = sum(1 for r in rows if r[4])
        total += ng
        lines += ['', 'Place %.0f %.0f %.0f, radius %.0f; %d gaps.%s' % (res['at'][0], res['at'][1], res['at'][2],
                                                                     res['radius'], ng,
                                                                     ' Shots: %s' % os.path.basename(res['sheet'])
                                                                     if res.get('sheet') else ''), '',
                  '| kind | SP (all) | SP | host | join | instance | | missing on the clients (examples) |',
                  '|---|---|---|---|---|---|---|---|']
        ik = res.get('instance', {}).get('kinds', {})
        for k, n_sp, a_sp, cnt, gap, extra, miss in rows:
            c = cnt + ['-'] * (2 - len(cnt))
            lines.append('| %s | %d | %d | %s | %s | %s | %s | %s |' % (k, n_sp, a_sp, c[0], c[1],
                                                                       ik.get(k, {}).get('active', 0),
                                                                       'gap' if gap else 'extra' if extra else '',
                                                                       ', '.join(miss)))
        for key, title in (('radar', 'Radar icons'), ('hud', 'HUD items'), ('sounds', 'Sounds playing'),
                           ('fx', 'Particle systems drawn')):
            if key == 'hud':
                sp = set(res['sp']['hud'])
                diffs = [(s, sorted(sp - set(res[s]['hud'])), sorted(set(res[s]['hud']) - sp))
                         for s in ('host', 'join') if s in res]
                diffs = [d for d in diffs if d[1] or d[2]]
                lines += ['', '%s: %s' % (title, '; '.join('%s lacks %s, has more %s' % (s, m or '-', x or '-')
                                                            for s, m, x in diffs) or 'same')]
                continue
            hg = hist_gaps(res, key)
            if not hg:
                lines += ['', '%s: same' % title]
                continue
            lines += ['', '%s (SP, host, join):' % title, '']
            lines += ['- %s: %s' % (k, ', '.join(str(x) for x in v)) for k, v in hg[:40]]
            if len(hg) > 40:
                lines.append('- ... %d more' % (len(hg) - 40))
        lines.append('')
    lines.insert(3, 'Total gaps (entity kinds): %d' % total)
    lines.insert(4, '')
    with open(out, 'w') as f:
        f.write('\n'.join(lines) + '\n')
    return total


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--levels', default=','.join(CITY))
    ap.add_argument('--port', type=int, default=29940)
    ap.add_argument('--build', default=os.path.join(REPO, 'build-linux64'))
    ap.add_argument('--out', default=os.path.join(REPO, 'build-linux64', 'parity'))
    ap.add_argument('--scratch', default=os.path.join(REPO, 'build-linux64', 'parity-games'))
    ap.add_argument('--radius', type=float, default=100000.0)
    ap.add_argument('--settle', type=float, default=25.0)
    ap.add_argument('--timeout', type=int, default=900)
    ap.add_argument('--device', default=None, help='Vulkan device (default the RTX with >= 3000 MiB free, else the AMD)')
    ap.add_argument('--no-join', dest='join', action='store_false')
    ap.add_argument('--report-only', action='store_true', help='the report of the JSON files in --out')
    a = ap.parse_args()
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
    levels = [x for x in a.levels.split(',') if x]
    results = []
    for level in levels:
        path = os.path.join(a.out, level + '.json')
        if not a.report_only:
            t0 = time.time()
            res = run_level(a, level)
            res['secs'] = round(time.time() - t0)
            with open(path, 'w') as f:
                json.dump(res, f, indent=1)
            print('%s: %s' % (level, res.get('error', 'done in %d s' % res['secs'])), flush=True)
        if os.path.exists(path):
            results.append(json.load(open(path)))
    total = report(results, os.path.join(a.out, 'report.md'))
    print('report %s: %d gaps' % (os.path.join(a.out, 'report.md'), total))


if __name__ == '__main__':
    main()
