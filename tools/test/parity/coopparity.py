#!/usr/bin/env python3
"""The coop variant of the parity tool (stage M7d of issue #1): what a member's client of the coop story shows against
single player at the same story point.

A story point is the data folder of a coop world (ScrapCoord -mode:coop -data:<dir>: party.story and the players)
and a level. For each point it starts (rendered, hidden, muted, in their own game folders under --scratch):
  sp      the single player game: the level, then the party's save variables (party.story and the member's own: its
          class) through the test channel and SaveGame.QLoad of the level, as a saved game loads
  world   ScrapCoord -mode:coop with a copy of the data folder, the member placed in the level (its instance answers
          the test channel in the world's game folder)
  member  a rendered client that joins the world with the key of the data's member (SCRAP_PLAYER_KEY)
puts the single player's player where the member stands, waits and compares the same things as parity.py: the
entities around the place by kind (a proxy labelled by the instance's entity it stands for), the radar icons, the HUD
items, the sounds and the particle systems, plus screenshots side by side. Output in --out: <point>.json,
<point>-shots.png and report.md.

  coopparity.py --point NAME=DATA:LEVEL[:KEY] ... [--port 29860] [--build DIR] [--coord DIR/src/coord/ScrapCoord]
                [--out DIR] [--scratch DIR] [--radius 30000] [--settle 25]

KEY is the member's player.key (default: the owner of the party, found as <scratch>/key-<owner>.key or given). Network
only on loopback (--port and the 28 ports after it)."""
import argparse
import glob
import json
import os
import re
import shutil
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import parity  # noqa: E402

STORE_HEAD = 'scrapplayer 1\n'
# a player's own variables of the story (Coop.c cp_local) and the coop ones: not set in the single player
SKIP = ('Map', 'Spawn', 'ComeFrom', 'PrevMap', 'AcBatteries', 'Money', 'Alarm', 'GameplayTime')


def load_store(path):
    with open(path, encoding='latin-1') as f:
        t = f.read()
    body = t[len(STORE_HEAD):]
    body = body[:body.rfind('sum ')]
    out = {}
    for line in body.split('\n'):
        if '=' in line:
            k, v = line.split('=', 1)
            out[k] = v
    return out


def lit(v):
    """a text as a literal of the test channel"""
    return '"%s"' % v.replace('\\', '\\\\').replace('"', '\\"')


class World:
    """ScrapCoord -mode:coop with its instances in the game folder g-world (test channel on)"""

    def __init__(self, a, data):
        self.a, self.data = a, data
        self.dir = os.path.join(a.scratch, 'g-world')
        self.proc = None

    def start(self):
        if not os.path.isdir(os.path.join(self.dir, 'Bin')):
            subprocess.run([os.path.join(parity.REPO, 'tools/test/mkgame.sh'), '--native', self.dir],
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=True)
        b = self.a.build
        shutil.copy2(os.path.join(b, 'ScrapC'), os.path.join(self.dir, 'Bin', 'ScrapC.new'))
        os.replace(os.path.join(self.dir, 'Bin', 'ScrapC.new'), os.path.join(self.dir, 'Bin', 'ScrapC'))
        for lib in glob.glob(os.path.join(b, 'ffmpeg', 'install', 'lib', 'lib*.so*')):
            dst = os.path.join(self.dir, 'Bin', os.path.basename(lib))
            if not os.path.exists(dst):
                shutil.copy2(lib, dst, follow_symlinks=False)
        for f in os.listdir(self.dir):
            if f.startswith('test_cmd') or f.startswith('test_reply'):
                os.remove(os.path.join(self.dir, f))
        env = dict(os.environ, XDG_DATA_HOME=os.path.join(self.dir, 'xdg-data'), SCRAP_NO_MSGBOX='1',
                   PULSE_SERVER='unix:/nonexistent', PIPEWIRE_REMOTE='/nonexistent', SDL_AUDIODRIVER='dummy',
                   SDL_VIDEODRIVER='dummy', SCRAP_TEST_CHANNEL='1')
        env.pop('DISPLAY', None)
        env.pop('WAYLAND_DISPLAY', None)
        self.log = open(os.path.join(self.a.scratch, 'world.log'), 'w')
        self.proc = subprocess.Popen([self.a.coord, '-port:%d' % self.a.port, '-bind:127.0.0.1', '-name:coopparity',
                                      '-data:' + self.data, '-gamedir:' + self.dir,
                                      '-exe:' + os.path.join(self.dir, 'Bin', 'ScrapC'), '-mode:coop',
                                      '-stopinstances', '-grace:5'],
                                     env=env, stdin=subprocess.PIPE, stdout=self.log, stderr=subprocess.STDOUT,
                                     start_new_session=True)

    def ask(self, lines, level, timeout=20.0):
        save = parity.channel.CMD, parity.channel.REPLY, parity.channel.TMP
        parity.channel.use_instance(level)
        try:
            return parity.channel.send(self.dir, '\n'.join(lines) if isinstance(lines, list) else lines,
                                       timeout=timeout)
        finally:
            parity.channel.CMD, parity.channel.REPLY, parity.channel.TMP = save

    def try_ask(self, lines, level, timeout=3.0):
        try:
            return self.ask(lines, level, timeout)
        except parity.channel.NoAnswer:
            return None

    def stop(self):
        if self.proc and self.proc.poll() is None:
            try:
                self.proc.stdin.write(b'quit\n')
                self.proc.stdin.flush()
            except OSError:
                pass
            try:
                self.proc.wait(20)
            except subprocess.TimeoutExpired:
                self.proc.kill()
        parity.stop_dir(self.dir)


def prepare_data(a, name, data, level):
    """a copy of the data folder with every player in level (no entrance: the level's start or next to a member)"""
    dst = os.path.join(a.scratch, name + '-data')
    shutil.rmtree(dst, ignore_errors=True)
    shutil.copytree(data, dst, ignore=shutil.ignore_patterns('*.log', 'instances.txt'))
    subprocess.run([sys.executable, a.storyedit, dst, '--level', level, '--entrance', '', '--door', ''], check=True)
    return dst


def sp_story(sp, story, own, level):
    """the party's save variables into the single player game, then the level loaded again with them"""
    lines = []
    for k, v in story.items():
        if k in SKIP or k.startswith('Coop.'):
            continue
        lines.append('do Scrap.SetSaveVar(%s, %s)' % (lit(k), lit(v)))
    if own.get('char'):
        lines.append('do Scrap.SetSaveVar("Char", %s)' % lit(own['char']))
    for k in range(0, len(lines), 40):
        sp.ask(lines[k:k + 40], timeout=30)
    sp.ask('do SaveGame.QLoad(%s)' % lit('Levels/' + level), timeout=30)


def run_point(a, name, data, level, key):
    res = {'point': name, 'level': level, 'radius': a.radius}
    story = load_store(os.path.join(data, 'party.story'))
    owner = open(os.path.join(data, 'party.owner')).read().split()[0] if os.path.exists(
        os.path.join(data, 'party.owner')) else ''
    own = {}
    for p in glob.glob(os.path.join(data, 'players', '*.dat')):
        if os.path.basename(p).startswith(owner):
            own = load_store(p)
    res['mission'] = '%s (%s) in %s' % (story.get('Mission.Library', '-'), story.get('MissionState', '-'),
                                        story.get('Mission.Map', '-'))
    wdata = prepare_data(a, name, data, level)
    world = World(a, wdata)
    sp = parity.Game(a, 'cs', ['-imap:Levels/' + level])
    member = parity.Game(a, 'cm', ['-join:127.0.0.1', '-port:%d' % a.port, '-name:' + (own.get('name') or 'pa')],
                         env={'SCRAP_PLAYER_KEY': key})
    try:
        world.start()
        sp.start()
        member.start()
        parity.wait_for(lambda: sp.try_ask('Scrap.GetLevelPath()', timeout=5), 240, 2)
        sp_story(sp, story, own, level)
        time.sleep(8)
        parity.wait_for(lambda: sp.try_ask('Scrap.GetLevelPath()', timeout=5), 240, 2)

        def member_own():
            return parity.own_entity(parity.parse_ents(member.try_ask('entinfo', timeout=5)))
        me = parity.wait_for(member_own, 300, 2)
        if not me:
            res['error'] = 'the member has no own entity'
            return res
        still, end = 0, time.time() + 40
        while still < 4 and time.time() < end:
            time.sleep(1.0)
            now = member_own() or me
            still = still + 1 if parity.dist(now['pos'], me['pos']) < 50.0 else 0
            me = now
        at = me['pos']
        res['at'] = at
        m = re.search(r"\('Ang', \(([-\d.e]+), ([-\d.e]+), ([-\d.e]+)\)\)", member.ask('ent %s Ang' % me['name']))
        ang = tuple(float(x) for x in m.groups()) if m else (0.0, 0.0, 0.0)
        sp.ask('do e = Scrap.UsrEntity(0); e.Pos = (%.1f, %.1f, %.1f); e.Ang = (%.3f, %.3f, %.3f)' % (at + ang))
        time.sleep(a.settle)
        inst = parity.parse_ents(world.ask('entinfo %.0f %.0f %.0f %.0f' % (at[0], at[1], at[2], a.radius * 1.2),
                                           level, timeout=30))
        res['instance'] = {'kinds': parity.count(inst, at, a.radius)}
        res['sp'] = parity.collect(sp, at, a.radius)
        res['join'] = parity.collect(member, at, a.radius, inst)
        res['sp_mission'] = (sp.try_ask('Scrap.GetSaveVar("Mission.Library")') or '').strip()
        shots = []
        for g in (sp, member):
            p = os.path.join(a.out, '%s-%s.png' % (name, g.name))
            shots.append(g.shot(p))
        res['sheet'] = parity.side_by_side(shots, ['single player', 'coop member'],
                                           os.path.join(a.out, '%s-shots.png' % name))
        for s in shots:
            if s:
                os.remove(s)
        for side in ('sp', 'join'):
            del res[side]['ents']
    finally:
        member.stop()
        sp.stop()
        world.stop()
    return res


def report(results, out):
    lines = ['# Coop parity report (M7d)', '',
             'Single player (SP) at a story point (the party\'s save variables, the level loaded again) against a '
             'coop member\'s client at the same story point and place. Counts are active entities within the radius, '
             'by kind; "gap" marks a kind the member shows less of (loose kinds such as traffic: less than half), '
             '"extra" more. Generated by tools/test/parity/coopparity.py.', '']
    total = 0
    for res in results:
        lines.append('## %s (%s)' % (res['point'], res['level']))
        if 'error' in res:
            lines += ['', 'error: ' + res['error'], '']
            continue
        rows = parity.gaps(res)
        ng = sum(1 for r in rows if r[4])
        total += ng
        lines += ['', 'Story: %s. Place %.0f %.0f %.0f, radius %.0f; %d gaps.%s' % (
            res['mission'], res['at'][0], res['at'][1], res['at'][2], res['radius'], ng,
            ' Shots: %s' % os.path.basename(res['sheet']) if res.get('sheet') else ''), '',
            '| kind | SP (all) | SP | member | instance | | missing on the member (examples) |',
            '|---|---|---|---|---|---|---|']
        ik = res.get('instance', {}).get('kinds', {})
        for k, n_sp, a_sp, cnt, gap, extra, miss in rows:
            lines.append('| %s | %d | %d | %s | %s | %s | %s |' % (k, n_sp, a_sp, cnt[0] if cnt else '-',
                                                                 ik.get(k, {}).get('active', 0),
                                                                 'gap' if gap else 'extra' if extra else '',
                                                                 ', '.join(miss)))
        for key, title in (('radar', 'Radar icons'), ('hud', 'HUD items'), ('sounds', 'Sounds playing'),
                           ('fx', 'Particle systems drawn')):
            if key == 'hud':
                sp = set(res['sp']['hud'])
                m = set(res['join']['hud'])
                lines += ['', '%s: %s' % (title, 'member lacks %s, has more %s' % (sorted(sp - m) or '-', sorted(m - sp)
                                                                                   or '-') if sp != m else 'same')]
                continue
            hg = parity.hist_gaps(res, key)
            if not hg:
                lines += ['', '%s: same' % title]
                continue
            lines += ['', '%s (SP, member):' % title, '']
            lines += ['- %s: %s' % (k, ', '.join(str(x) for x in v)) for k, v in hg[:40]]
        lines.append('')
    lines.insert(3, 'Total gaps (entity kinds): %d' % total)
    lines.insert(4, '')
    with open(out, 'w') as f:
        f.write('\n'.join(lines) + '\n')
    return total


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--point', action='append', default=[], help='NAME=DATA:LEVEL[:KEY]')
    ap.add_argument('--port', type=int, default=29860)
    ap.add_argument('--build', default=os.path.join(parity.REPO, 'build-linux64'))
    ap.add_argument('--coord', default=None)
    ap.add_argument('--storyedit', default=os.path.join(HERE, 'storyedit.py'))
    ap.add_argument('--out', default=os.path.join(parity.REPO, 'build-linux64', 'coopparity'))
    ap.add_argument('--scratch', default=os.path.join(parity.REPO, 'build-linux64', 'coopparity-games'))
    ap.add_argument('--radius', type=float, default=30000.0)
    ap.add_argument('--settle', type=float, default=25.0)
    ap.add_argument('--timeout', type=int, default=900)
    ap.add_argument('--device', default=None)
    ap.add_argument('--report-only', action='store_true')
    a = ap.parse_args()
    a.build = os.path.abspath(a.build)
    a.coord = a.coord or os.path.join(a.build, 'src', 'coord', 'ScrapCoord')
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
    for p in a.point:
        name, rest = p.split('=', 1)
        f = rest.split(':')
        data, level, key = f[0], f[1], f[2] if len(f) > 2 else ''
        path = os.path.join(a.out, name + '.json')
        if not a.report_only:
            t0 = time.time()
            res = run_point(a, name, os.path.abspath(data), level, os.path.abspath(key) if key else '')
            res['secs'] = round(time.time() - t0)
            with open(path, 'w') as fo:
                json.dump(res, fo, indent=1)
            print('%s: %s' % (name, res.get('error', 'done in %d s' % res['secs'])), flush=True)
        if os.path.exists(path):
            results.append(json.load(open(path)))
    total = report(results, os.path.join(a.out, 'report.md'))
    print('report %s: %d gaps' % (os.path.join(a.out, 'report.md'), total))


if __name__ == '__main__':
    main()
