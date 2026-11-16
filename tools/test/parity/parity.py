#!/usr/bin/env python3
"""The parity tool of the multiplayer (stage M10 of issue #1, routes and the audit column since stage 0.4 of the
rewrite): what a multiplayer client shows against single player at the same places of a level.

For each level it starts (rendered, hidden, muted, in their own game folders under --scratch):
  sp    the single player game (-imap:Levels/<level>)
  host  a hosting game (-host:<level> -mode:freeroam, or -mode:<mode> -maps:<level> for "Level:mode"): the
        coordinator in the game, the level's instance a child, the host plays as a client over loopback
  join  a second game that joins that world (-join)
and walks a route: the host's spawn, then the points of the level in routes.txt (made by routes.py from the level's
dummies: the busiest anchor, an elevator or door, a parking). At each point the single player's player, the host's
player and the joined player (400 units beside) are put there (the clients' players through their entities on the
instance, src/game/Global/TestChannel.c -i), and after a wait the tool compares through the test channel: the
entities around the point by kind (entinfo; a replicated proxy is labelled by the instance's entity it stands for),
the icons of the radar (radar), the HUD items (menu), the sounds playing (sounds) and the particle systems drawn (fx),
plus screenshots side by side. The instance's audit counts (stage 0.1, test verb audit: reset after the join, audit
json at the end of the route) go into the audit column, n/a with a build without the verb. Output in --out:
<tag>.json per level (tag: the level, or <level>-<mode>), <tag>-<point>-shots.png, report.md (the summary table and
the gaps of each point) and parity.json (the summary per level). Exit code 0 without gaps, 1 with gaps, 2 when a level
did not run.

  parity.py [--levels DownTown,Outskirts,MCA:dm,...] [--port 29940] [--build build-linux64] [--out DIR]
            [--scratch DIR] [--points 4] [--radius 30000] [--settle 25] [--settle-point 12] [--no-join] [--no-shots]

Default levels: every level a free world hosts (the city and the listed places of data/places/places.def, the coop
story's levels among them) in free roam, and the PvP maps of the original in --pvp-mode (dm). Network only on
loopback (a world takes --port and its instances --port + 8 + the index of the level: up to --port + 36)."""
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

try:
    import resource
    # no core files of the games (the rendered games killed at the end of a run leave NVIDIA cores of ~100 MB each);
    # the games inherit the limit
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
except (ImportError, ValueError, OSError):
    pass

CITY = ['DownTown', 'Outskirts', 'Commercial', 'Scrapyard', 'GDB', 'Police', 'Bank', 'Press', 'Temple', 'TownHall',
        'GamblinDen']
RAIL = ('NormalTraffic', 'MediumTraffic', 'HeavyTraffic')
FREE = ('Traffic', 'FightingTraffic', 'ChasingTraffic', 'EnemiesTraffic')
# kinds whose count changes with time (traffic, effects of fights): compared loosely
# the frames of the blinking mission marks (RadarScorer.c Radar_DrawMission) as their first one
RADAR_BLINK = {'13': '12', '14': '12', '10': '9', '11': '9'}
LOOSE = ('rail traffic', 'traffic', 'police', 'coin', 'missile', 'npc', 'parked ship')
# kinds that come and go with the moment (coins, batteries and the items of shot traffic dropped in a fight, missiles, gears that police fuse into where a
# player sees them and that turn back): the clients against what the instance has, not against single player
TIMED = ('coin', 'item Battery', 'npc Gear', 'item dropped', 'missile')
# kinds single player has that the multiplayer worlds leave out on purpose: not counted as gaps, marked "single player
# only" (the players' Sputniks since M12, include/scrap/sputniks.h)
SP_ONLY = ()
# and in a PvP mode: single player's companion and its own parked ship, which the PvP modes leave out on purpose
SP_ONLY_PVP = ('npc Sputnik', 'parked ship')
SOUND_SAMPLES = 6                  # sounds playing taken this many times, 1 s apart
# the PvP maps of the original (data/modules/net.mod NetMaps) and the mode parity runs them in by default (every map
# has DeathMatch)
PVP_MAPS = ['QF', 'FZ', 'OM', 'CA', 'GDB', 'MCA', 'MCB', 'MDA', 'MDB', 'MOA', 'MOB', 'MSA', 'MSB']
PVP_MODE = 'dm'


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
    if n.startswith('ItemTraffic'):
        return 'item dropped'
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
        e['local'] = e['name']
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
    """inst: a function that gives the instance's entities of now (a proxy is labelled by the instance's entity at
    the same moment: the traffic moves)"""
    return collect_all([('x', g, inst)], at, radius)['x']


def collect_all(sides, at, radius):
    """collect for several games at once, sides: (key, game, inst function or None); the sounds of all games are
    sampled in the same rounds (SOUND_SAMPLES rounds 1 s apart), so a point costs the same time for one game or three"""
    outs = {}
    for key, g, inst in sides:
        out = outs[key] = {}
        fresh = inst() if inst else None
        out['ents'] = parse_ents(g.ask('entinfo %.0f %.0f %.0f %.0f' % (at[0], at[1], at[2], radius), timeout=30))
        if fresh is not None:
            label_proxies(out['ents'], fresh)
        out['radar'], out['radar_seen'], out['fx'], out['fx_seen'] = {}, {}, {}, {}
        out['sounds'] = {}
        out['loops'] = set()
    # the sounds playing near the player, the radar icons and the particle systems drawn, by name: the most of each
    # over a few samples and the samples that had it (one-shot sounds, a fight's effects and the icons of passing
    # traffic are there only for a moment; *_seen tells the steady ones)
    for i in range(SOUND_SAMPLES):
        if i:
            time.sleep(1.0)
        for key, g, _ in sides:
            out = outs[key]
            for what, h in (('radar', hist(g.ask('radar'), lambda f: 'icon %s%s' % (
                    RADAR_BLINK.get(f[0], f[0]), ' rim' if f[1] == '1' else ''))), ('fx', hist(g.ask('fx'),
                                                                                         lambda f: f[0]))):
                for k, n in h.items():
                    out[what][k] = max(out[what].get(k, 0), n)
                    out[what + '_seen'][k] = out[what + '_seen'].get(k, 0) + 1
            text = g.ask('sounds')
            h = hist(text, lambda f: '%s %s' % (f[0], f[2] if f[0] == 'ch' else f[1]))
            for k, n in h.items():
                out['sounds'][k] = max(out['sounds'].get(k, 0), n)
            for line in (text or '').splitlines():
                f = line.split('\t')
                if f[0] == 'ps' or (f[0] == 'ch' and len(f) > 3 and f[3] == '1'):
                    out['loops'].add('%s %s' % (f[0], f[2] if f[0] == 'ch' else f[1]))
    for key, g, _ in sides:
        out = outs[key]
        out['loops'] = sorted(out['loops'])
        out['parked'] = parked_motors(g, out['ents'], at, radius)
        menu = g.ask('menu') or ''
        out['hud'] = sorted(set(re.findall(r"\[\s*'([^']*)'", menu)))
        out['kinds'] = count(out['ents'], at, radius)
    return outs


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


def parked_motors(g, ents, at, radius):
    """the motor of each parked ship (and single player's own ship) within the radius whose motor plays: the idle one
    (sound item 0, the ship moved into its place) or the flying one (item 8), with the sound's name (the models of the
    parked ships are drawn at random by each game, so are their motors' names: compared by what they play, not by
    name)"""
    out = []
    for e in ents:
        # single player's own ship parked (its name is not a parked ship's; the players' own ships are)
        if (kind_of(e) != 'parked ship' and e['name'] != 'PlayerShip') or dist(e['pos'], at) > radius:
            continue
        snd = {}
        for line in (g.try_ask('entsnd %s' % e.get('local', e['name']), timeout=5) or '').splitlines():
            f = line.split('\t')
            if f[0] == 'sn' and len(f) > 4 and f[1] in ('0', '8'):
                snd[f[1]] = (f[2], float(f[3]), f[4] == '1')
        on = [k for k, v in snd.items() if v[2] and v[1] > 0.0]
        if on:
            k = on[0]
            out.append({'name': e['name'], 'role': 'idle' if k == '0' else 'flying', 'sound': snd[k][0]})
    return out


def places():
    """the listed places of data/places/places.def (the levels a free world has besides the city)"""
    out = []
    try:
        with open(os.path.join(REPO, 'data', 'places', 'places.def')) as f:
            for line in f:
                p = line.split()
                if len(p) >= 3 and p[0] == 'place' and 'listed' in p[2:]:
                    out.append(p[1])
    except OSError:
        pass
    return out


def world_levels():
    """the levels a free world hosts: the city and the places (the coop story's levels are among them)"""
    return CITY + [x for x in places() if x not in CITY]


def default_specs(pvp_mode=PVP_MODE):
    """the parity runs of a full gate: every world level in free roam, every PvP map in pvp_mode"""
    return world_levels() + ['%s:%s' % (m, pvp_mode) for m in PVP_MAPS]


def pick_device():
    """the RTX when it has >= 3000 MiB free, else the AMD"""
    try:
        free = int(subprocess.run(['nvidia-smi', '--query-gpu=memory.free', '--format=csv,noheader,nounits'],
                                  capture_output=True, text=True).stdout.split()[0])
    except (OSError, ValueError, IndexError):
        free = 0
    return '10de:27b0' if free >= 3000 else '1002:164e'


def load_routes(path):
    """routes.txt: {level: [(point, anchor, (x, y, z)), ...]}"""
    out = {}
    try:
        with open(path) as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith('#'):
                    continue
                lv, name, anchor, xyz = line.split('|')
                out.setdefault(lv, []).append((name, anchor, tuple(float(v) for v in xyz.split())))
    except OSError:
        pass
    return out


def audit(g, inst, verb):
    """the instance's audit verb (stage 0.1, src/game/Global/NetAudit.c): verb 'reset' or 'json'; None when the build
    has no such verb (the answer is an error) or the answer is no JSON object"""
    text = g.try_ask('audit ' + verb, inst, 10)
    if text is None or text.startswith('error'):
        return None
    if verb != 'json':
        return {}
    at = text.find('{')
    try:
        v = json.loads(text[at:]) if at >= 0 else None
    except ValueError:
        return None
    return v if isinstance(v, dict) else None


def audit_total(a):
    """one number for an audit answer: its "total" (or "unforwarded") when it has one, else the sum of its numbers"""
    if not isinstance(a, dict):
        return None
    for k in ('total', 'unforwarded'):
        if isinstance(a.get(k), (int, float)):
            return a[k]

    def walk(v):
        if isinstance(v, bool):
            return 0
        if isinstance(v, (int, float)):
            return v
        if isinstance(v, dict):
            return sum(walk(x) for x in v.values())
        if isinstance(v, list):
            return sum(walk(x) for x in v)
        return 0
    return walk(a)


def player_entities(ents, near):
    """the instance's entity of each client: the players' entity (a NetPlayer character or a player's ship, owner
    slot 4 on) nearest to the client's own position; near: [(key, pos)]. A player keeps the entity found first"""
    cands = [e for e in ents if e['name'].startswith('NetPlayer') or e.get('owner', -1) >= 4]
    out, taken = {}, set()
    for key, pos in near:
        c = sorted((e for e in cands if e['name'] not in taken), key=lambda e: dist(e['pos'], pos))
        if c:
            out[key] = c[0]['name']
            taken.add(c[0]['name'])
    return out


def put(g, ent, at, ang, inst=None):
    """an entity to a place, still (ent None: the single player's player)"""
    target = 'Scrap.UsrEntity(0)' if ent is None else 'Scrap.GetEntity("%s")' % ent
    g.ask('do e = %s; e.Pos = (%.1f, %.1f, %.1f); e.Ang = (%.3f, %.3f, %.3f)' % ((target,) + tuple(at) + tuple(ang)),
          inst=inst)


def run_level(a, spec, routes=None):
    """one level (spec "Level" for free roam or "Level:mode" for a PvP mode): the three games, the route, the
    collection at each point; the result dict"""
    level, _, mode = spec.partition(':')
    mode = mode or 'freeroam'
    port = a.port
    res = {'level': level, 'mode': mode, 'tag': tag_of(spec), 'radius': a.radius, 'points': []}
    hargs = ['-host:' + level, '-port:%d' % port, '-mode:' + mode, '-name:Host']
    if mode != 'freeroam':
        hargs.append('-maps:' + level)
    host = Game(a, 'ph', hargs, a.env)
    sp = Game(a, 'ps', ['-imap:Levels/' + level])
    join = Game(a, 'pj', ['-join:127.0.0.1', '-port:%d' % port, '-name:Join'], a.env) if a.join else None
    games = [g for g in (host, sp, join) if g]
    try:
        host.start()
        sp.start()

        def own(g):
            return own_entity(parse_ents(g.try_ask('entinfo', timeout=5)))
        me = wait_for(lambda: own(host), 240, 2)
        if not me:
            res['error'] = 'the host has no own entity'
            return res
        # where it stays (a fall from a bad spawn point respawns it elsewhere, Replicate.c)
        still, end = 0, time.time() + 40
        while still < 4 and time.time() < end:
            time.sleep(1.0)
            now = own(host) or me
            still = still + 1 if dist(now['pos'], me['pos']) < 50.0 else 0
            me = now
        at = me['pos']
        m = re.search(r"\('Ang', \(([-\d.e]+), ([-\d.e]+), ([-\d.e]+)\)\)", host.ask('ent %s Ang' % me['name']))
        ang = tuple(float(x) for x in m.groups()) if m else (0.0, 0.0, 0.0)
        res['ang'] = ang
        jme = None
        if join:
            join.start()
            jme = wait_for(lambda: own(join), 240, 2)
            # (a drop in the first seconds after a spawn counts as a fall from a bad spawn point, Replicate.c)
            time.sleep(6)
            jme = own(join) or jme
            res['join_own'] = bool(jme)
        wait_for(lambda: sp.try_ask('Scrap.GetLevelPath()', timeout=5), 180, 2)
        iel = parse_ents(host.try_ask('entinfo', inst=level, timeout=10))
        near = [('host', (own(host) or me)['pos'])] + ([('join', jme['pos'])] if jme else [])
        pents = player_entities(iel, near)
        res['players'] = pents
        # the audit counts cover the route (n/a without the verb)
        res['audit_verb'] = audit(host, level, 'reset') is not None
        points = [('spawn', 'host spawn', at)] + [(n, an, (x, y + 150.0, z)) for n, an, (x, y, z) in
                                                  (routes or {}).get(level, [])]
        for k, (name, anchor, pos) in enumerate(points[:a.points]):
            pt = {'name': name, 'anchor': anchor, 'at': pos}
            put(sp, None, pos, ang)
            if k and 'host' in pents:
                put(host, pents['host'], pos, ang, inst=level)
            if 'join' in pents:
                put(host, pents['join'], (pos[0] + 400.0, pos[1], pos[2]), ang, inst=level)
            time.sleep(a.settle if k == 0 else a.settle_point)

            def inst_now():
                return parse_ents(host.ask('entinfo %.0f %.0f %.0f %.0f' % (pos[0], pos[1], pos[2], a.radius * 1.2),
                                           inst=level, timeout=30))
            pt['instance'] = {'kinds': count(inst_now(), pos, a.radius)}
            sides = [('sp', sp, None), ('host', host, inst_now)] + ([('join', join, inst_now)] if join else [])
            pt.update(collect_all(sides, pos, a.radius))
            if a.shots:
                shots = [g.shot(os.path.join(a.out, '%s-%s-%s.png' % (res['tag'], name, g.name)))
                         for g in (sp, host, join) if g]
                pt['sheet'] = side_by_side(shots, ['single player', 'host (client of its own world)', 'joined client'],
                                           os.path.join(a.out, '%s-%s-shots.png' % (res['tag'], name)))
                for s in shots:
                    if s:
                        os.remove(s)
            for side in ('sp', 'host', 'join'):
                if side in pt:
                    del pt[side]['ents']
            res['points'].append(pt)
        res['audit'] = audit(host, level, 'json')
    finally:
        for g in games:
            g.stop()
    return res


def tag_of(spec):
    level, _, mode = spec.partition(':')
    return level if not mode or mode == 'freeroam' else '%s-%s' % (level, mode)


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
        if k in TIMED:
            # what the instance has there now; single player's count is of its own moment
            a_sp = res.get('instance', {}).get('kinds', {}).get(k, {}).get('active', 0)
        if loose:
            gap = a_sp > 0 and (worst == 0 or worst < a_sp * 0.5)
        else:
            gap = worst < a_sp
        if k in SP_ONLY or (res.get('mode', 'freeroam') not in ('freeroam', 'coop') and k in SP_ONLY_PVP):
            gap = False
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


def sound_gaps(res):
    """the steady sounds (loops of the mixer, positional sets) single player plays near the player that a client does
    not play at all: (name, [sp, host, join]). One-shot sounds (steps, mechanisms, shots) depend on what happens at
    that moment: only their totals are compared (sound_shots)"""
    sp = res.get('sp', {}).get('sounds', {})
    loops = set(res.get('sp', {}).get('loops', []))
    sides = [s for s in ('host', 'join') if s in res]
    out = []
    # the motors of the parked ships by what they play (parked_motors): a name single player's parked ships play as
    # their idle motor is no gap of its own, the idle motors are counted instead
    idle = {}
    for m in res.get('sp', {}).get('parked', []):
        if m['role'] == 'idle':
            idle['ch ' + m['sound']] = idle.get('ch ' + m['sound'], 0) + 1
    for k in sorted(sp):
        if 'loops' in res.get('sp', {}) and k not in loops:
            continue
        if 'parked' in res.get('sp', {}) and sp[k] <= idle.get(k, 0):
            continue
        v = [sp[k]] + [res[s].get('sounds', {}).get(k, 0) for s in sides]
        if k != 'ch ?' and min(v[1:] or [1]) == 0:
            out.append((k, v))
    if 'parked' in res.get('sp', {}):
        v = [sum(1 for m in res[s].get('parked', []) if m['role'] == 'idle') for s in ['sp'] + sides]
        fly = sum(1 for s in sides for m in res[s].get('parked', []) if m['role'] == 'flying')
        if fly or min(v[1:] or [v[0]]) < v[0]:
            out.append(('parked ships idling (flying on the clients: %d)' % fly, v))
    return out


def sound_shots(res):
    """the most one-shot sounds playing at once over the samples, summed over the names: [sp, host, join]"""
    out = []
    for s in ('sp', 'host', 'join'):
        if s in res:
            loops = set(res[s].get('loops', []))
            out.append(sum(n for k, n in res[s].get('sounds', {}).items() if k not in loops and k != 'ch ?'))
    return out


def lacks(res, key):
    """the names of a histogram (radar icons, particle systems) a client shows none of or less than half as many as
    single player (the most over the samples), only the steady ones of single player (seen in at least half of the
    samples: the icons of passing traffic and the effects of a fight come and go): [(name, [sp, host, join])]"""
    seen = res.get('sp', {}).get(key + '_seen')
    return [(k, v) for k, v in hist_gaps(res, key) if len(v) > 1 and v[0] > 0 and min(v[1:]) < v[0] * 0.5 and
            (seen is None or seen.get(k, 0) * 2 >= SOUND_SAMPLES)]


def hud_lacks(res):
    sp = set(res.get('sp', {}).get('hud', []))
    out = set()
    for s in ('host', 'join'):
        if s in res:
            out |= sp - set(res[s].get('hud', []))
    return sorted(out)


def points_of(res):
    """the points of a result (an older result of one place is one point)"""
    if 'points' in res:
        return res['points']
    return [dict(res, name='spawn', anchor='host spawn')] if 'sp' in res else []


def summary(res):
    """the gap counts of a level over its points: entity kinds, radar icons, HUD items, steady sounds, particle
    systems (a name counts once per point), the audit total (None: n/a); 'error' when the level did not run"""
    out = {'tag': res.get('tag', res.get('level')), 'level': res.get('level'), 'mode': res.get('mode', 'freeroam'),
           'points': 0, 'entity': 0, 'radar': 0, 'hud': 0, 'sound': 0, 'fx': 0, 'kinds': [],
           'audit': audit_total(res.get('audit')), 'secs': res.get('secs')}
    if 'error' in res:
        out['error'] = res['error']
    kinds = set()
    for pt in points_of(res):
        if 'sp' not in pt:
            continue
        out['points'] += 1
        rows = [r for r in gaps(dict(pt, mode=out['mode'])) if r[4]]
        out['entity'] += len(rows)
        kinds |= {r[0] for r in rows}
        out['radar'] += len(lacks(pt, 'radar'))
        out['fx'] += len(lacks(pt, 'fx'))
        out['hud'] += len(hud_lacks(pt))
        out['sound'] += len(sound_gaps(pt))
    out['kinds'] = sorted(kinds)
    if not out['points'] and 'error' not in out:
        out['error'] = 'no point collected'
    return out


SUMMARY_COLS = ('entity', 'radar', 'hud', 'sound', 'fx')


def summary_table(sums):
    """the markdown table of the level summaries"""
    lines = ['| level | mode | points | entity gaps | radar gaps | HUD gaps | sound gaps | particle gaps | audit | '
             'entity gap kinds |', '|---|---|---|---|---|---|---|---|---|---|']
    tot = {c: 0 for c in SUMMARY_COLS}
    for s in sums:
        if 'error' in s:
            lines.append('| %s | %s | error: %s | | | | | | | |' % (s['level'], s['mode'], s['error']))
            continue
        for c in SUMMARY_COLS:
            tot[c] += s[c]
        lines.append('| %s | %s | %d | %d | %d | %d | %d | %d | %s | %s |' % (
            s['level'], s['mode'], s['points'], s['entity'], s['radar'], s['hud'], s['sound'], s['fx'],
            'n/a' if s['audit'] is None else s['audit'], ', '.join(s['kinds'][:6]) +
            (' ...' if len(s['kinds']) > 6 else '')))
    au = [s['audit'] for s in sums if s.get('audit') is not None]
    lines.append('| all | | %d | %d | %d | %d | %d | %d | %s | |' % (
        sum(s['points'] for s in sums if 'error' not in s), tot['entity'], tot['radar'], tot['hud'], tot['sound'],
        tot['fx'], sum(au) if au else 'n/a'))
    return lines


def point_lines(res):
    """the detail of one point"""
    lines = []
    rows = gaps(res)
    ng = sum(1 for r in rows if r[4])
    sg = sound_gaps(res)
    lines += ['', 'Place %.0f %.0f %.0f, radius %.0f; %d gaps, %d sound gaps.%s' % (
        res['at'][0], res['at'][1], res['at'][2], res.get('radius', 0) or 0, ng, len(sg),
        ' Shots: %s' % os.path.basename(res['sheet']) if res.get('sheet') else ''), '',
        '| kind | SP (all) | SP | host | join | instance | | missing on the clients (examples) |',
        '|---|---|---|---|---|---|---|---|']
    ik = res.get('instance', {}).get('kinds', {})
    for k, n_sp, a_sp, cnt, gap, extra, miss in rows:
        c = cnt + ['-'] * (2 - len(cnt))
        lines.append('| %s | %d | %d | %s | %s | %s | %s | %s |' % (k, n_sp, a_sp, c[0], c[1],
                                                                   ik.get(k, {}).get('active', 0),
                                                                   'gap' if gap else 'extra' if extra else
                                                                   'single player only' if k in SP_ONLY or (res.get('mode', 'freeroam') not in ('freeroam', 'coop') and k in SP_ONLY_PVP) else '',
                                                                   ', '.join(miss)))
    lines += ['', 'One-shot sounds (the most of each name, summed; SP, host, join): %s' %
              ', '.join(str(x) for x in sound_shots(res))]
    if 'parked' in res.get('sp', {}):
        lines += ['', 'Parked ships whose motor plays, idle/flying (SP, host, join): %s' % ', '.join(
            '%d/%d' % (sum(1 for m in res[s].get('parked', []) if m['role'] == 'idle'),
                       sum(1 for m in res[s].get('parked', []) if m['role'] == 'flying'))
            for s in ('sp', 'host', 'join') if s in res)]
    if sg:
        lines += ['', 'Sound gaps (steady sounds single player plays near the player, a client not; SP, host, join):',
                  '']
        lines += ['- %s: %s' % (k, ', '.join(str(x) for x in v)) for k, v in sg]
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
    return lines


def report(results, out):
    """report.md (the summary table, then each level and point) and parity.json (the summaries) next to it; the
    total of the gap columns"""
    sums = [summary(r) for r in results]
    lines = ['# Parity report', '',
             'Single player (SP) against the multiplayer clients at the same places: the host (a -host game, client of '
             'its own world over loopback) and a joined client, at each point of the level\'s route (the host\'s '
             'spawn, then the points of tools/test/parity/routes.txt). Entity gaps: kinds the clients show less of '
             'within the radius of the point (loose kinds such as traffic: less than half; coins against what the '
             'instance has at that moment), summed over the points. Radar, HUD, particle gaps: icons, items and '
             'particle systems single player shows steadily (in at least half of the %d samples) that a client shows none of or less than half as many of. Sound gaps: steady sounds (a loop or a '
             'positional set, the most of each name over %d samples) single player plays near the player that a client '
             'does not play. Audit: the instance\'s count of presentation calls during the route (`audit json`, n/a '
             'without the verb). Generated by tools/test/parity/parity.py.' % (SOUND_SAMPLES, SOUND_SAMPLES), '']
    lines += summary_table(sums) + ['']
    for res in results:
        lines.append('## %s' % res.get('tag', res['level']))
        if 'error' in res:
            lines += ['', 'error: ' + res['error'], '']
        if res.get('audit') is not None:
            lines += ['', 'Audit: `%s`' % json.dumps(res['audit'], sort_keys=True)[:2000]]
        for pt in points_of(res):
            if 'sp' not in pt:
                continue
            lines += ['', '### %s (%s)' % (pt.get('name'), pt.get('anchor'))]
            lines += point_lines(dict(pt, radius=res.get('radius'), mode=res.get('mode', 'freeroam')))
        lines.append('')
    with open(out, 'w') as f:
        f.write('\n'.join(lines) + '\n')
    with open(os.path.join(os.path.dirname(out), 'parity.json'), 'w') as f:
        json.dump({'levels': sums}, f, indent=1)
    return sums


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--levels', default=None, help='Level or Level:mode, comma separated (default: every world level '
                    'in free roam and every PvP map in %s)' % PVP_MODE)
    ap.add_argument('--pvp-mode', default=PVP_MODE)
    ap.add_argument('--port', type=int, default=29940)
    ap.add_argument('--build', default=os.path.join(REPO, 'build-linux64'))
    ap.add_argument('--out', default=os.path.join(REPO, 'build-linux64', 'parity'))
    ap.add_argument('--scratch', default=os.path.join(REPO, 'build-linux64', 'parity-games'))
    ap.add_argument('--routes', default=os.path.join(HERE, 'routes.txt'))
    ap.add_argument('--points', type=int, default=4, help='points of a route at most (1: the spawn only)')
    ap.add_argument('--radius', type=float, default=30000.0)
    ap.add_argument('--settle', type=float, default=25.0, help='seconds at the first point')
    ap.add_argument('--settle-point', type=float, default=12.0, help='seconds at each further point')
    ap.add_argument('--timeout', type=int, default=1200)
    ap.add_argument('--device', default=None, help='Vulkan device (default the RTX with >= 3000 MiB free, else the AMD)')
    ap.add_argument('--no-join', dest='join', action='store_false')
    ap.add_argument('--no-shots', dest='shots', action='store_false')
    ap.add_argument('--report-only', action='store_true', help='the report of the JSON files in --out')
    a = ap.parse_args()
    a.build = os.path.abspath(a.build)
    a.env = {}
    os.makedirs(a.out, exist_ok=True)
    os.makedirs(a.scratch, exist_ok=True)
    a.device = a.device or pick_device()
    specs = [x for x in (a.levels.split(',') if a.levels else default_specs(a.pvp_mode)) if x]
    routes = load_routes(a.routes)
    results = []
    for spec in specs:
        path = os.path.join(a.out, tag_of(spec) + '.json')
        if not a.report_only:
            t0 = time.time()
            try:
                res = run_level(a, spec, routes)
            except Exception as ex:  # noqa: BLE001 (a level that breaks the tool is a row with an error)
                res = {'level': spec.partition(':')[0], 'mode': spec.partition(':')[2] or 'freeroam',
                       'tag': tag_of(spec), 'error': '%s: %s' % (type(ex).__name__, ex)}
            res['secs'] = round(time.time() - t0)
            with open(path, 'w') as f:
                json.dump(res, f, indent=1)
            s = summary(res)
            print('%s: %s' % (tag_of(spec), s.get('error') or 'done in %d s, %d points, gaps entity %d radar %d hud %d '
                                                              'sound %d fx %d, audit %s' % (
                res['secs'], s['points'], s['entity'], s['radar'], s['hud'], s['sound'], s['fx'],
                'n/a' if s['audit'] is None else s['audit'])), flush=True)
        if os.path.exists(path):
            results.append(json.load(open(path)))
    sums = report(results, os.path.join(a.out, 'report.md'))
    print('\n'.join(summary_table(sums)))
    errors = sum(1 for s in sums if 'error' in s) + len(specs) - len(sums)
    total = sum(s[c] for s in sums if 'error' not in s for c in SUMMARY_COLS)
    print('report %s: %d gaps, %d levels with errors' % (os.path.join(a.out, 'report.md'), total, errors))
    sys.exit(2 if errors else 1 if total else 0)


if __name__ == '__main__':
    main()
