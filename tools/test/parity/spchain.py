#!/usr/bin/env python3
"""The single player reference of the coop story (stage 5 of plan mp2): one rendered single player game goes through
the chapters of tools/test/coop/story.txt in their order, from the new game to the credits. For every chapter with a
story point (a saved coop world of --points-dir) it loads the party's save variables into the game and the mission's
level again (as coopparity.py does for its single player side) and records what single player has then: the level,
Mission.Library, MissionState, the count of entities and the console's script errors of that load. A chapter whose
story the game does not reach (another mission after three loads) is marked. At the end the credits: the main menu
level with Map.Credits (the last chapter's own end).

  spchain.py --points-dir DIR [--story tools/test/coop/story.txt] [--parts 1,2,3] [--out DIR] [--scratch DIR]
             [--build DIR] [--device ID]

Output: <out>/spchain.json and spchain.md (one row per chapter). The reference for the coop chains: the mission and
the level single player has at each story point."""
import argparse
import json
import os
import re
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import parity  # noqa: E402
import gameclock  # noqa: E402
import coopparity  # noqa: E402


def read_story(path, parts):
    rows = []
    for line in open(path):
        if line.startswith('#') or '|' not in line:
            continue
        f = line.rstrip('\n').split('|')
        if len(f) >= 5 and int(f[3]) in parts:
            rows.append({'n': int(f[0]), 'mission': f[1], 'level': f[2], 'part': int(f[3]), 'point': f[4],
                         'chapter': f[5] if len(f) > 5 else ''})
    return rows


def errors_since(log, mark):
    try:
        lines = open(log, errors='replace').read().splitlines()[mark:]
    except OSError:
        return [], mark
    errs = [x for x in lines if re.search(r'(Error|error return|Traceback|FATAL)', x) and 'ALSA' not in x]
    return errs, mark + len(lines)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--points-dir', required=True)
    ap.add_argument('--story', default=os.path.join(parity.REPO, 'tools', 'test', 'coop', 'story.txt'))
    ap.add_argument('--parts', default='1,2,3')
    ap.add_argument('--build', default=os.path.join(parity.REPO, 'build-linux64'))
    ap.add_argument('--out', default=os.path.join(parity.REPO, 'build-linux64', 'spchain'))
    ap.add_argument('--scratch', default=os.path.join(parity.REPO, 'build-linux64', 'spchain-games'))
    ap.add_argument('--device', default='10de:27b0')
    ap.add_argument('--port', type=int, default=29860)
    ap.add_argument('--timeout', type=int, default=14400)
    a = ap.parse_args()
    a.build = os.path.abspath(a.build)
    os.makedirs(a.out, exist_ok=True)
    os.makedirs(a.scratch, exist_ok=True)
    rows = read_story(a.story, {int(x) for x in a.parts.split(',')})
    first = next((r for r in rows if r['point']), None)
    sp = parity.Game(a, 'sp', ['-imap:Levels/' + (first['level'] if first else 'DownTown')])
    res = []
    t0 = time.time()
    try:
        sp.start()
        parity.wait_for(lambda: sp.try_ask('Scrap.GetLevelPath()', timeout=5), 300, 2)
        mark = 0
        for r in rows:
            data = os.path.join(a.points_dir, r['point']) if r['point'] else ''
            if not data or not os.path.isdir(data):
                res.append(dict(r, result='no story point'))
                continue
            story = coopparity.load_store(os.path.join(data, 'party.story'))
            reached = False
            for _ in range(3):
                coopparity.sp_story(sp, story, {}, r['level'])
                gameclock.sleep(8)
                parity.wait_for(lambda: sp.try_ask('Scrap.GetLevelPath()', timeout=5), 240, 2)
                if coopparity.sp_mission(sp) == story.get('Mission.Library', ''):
                    reached = True
                    break
            gameclock.sleep(5)
            path = (sp.try_ask('Scrap.GetLevelPath()', timeout=10) or '').strip().strip("'")
            state = (sp.try_ask('Scrap.GetSaveVar("MissionState")', timeout=10) or '').strip().strip("'")
            ents = parity.parse_ents(sp.try_ask('entinfo', timeout=30) or '')
            errs, mark = errors_since(sp.log, mark)
            row = dict(r, result='reached' if reached else 'not reached', sp_level=path,
                       sp_mission=coopparity.sp_mission(sp), sp_state=state, story_state=story.get('MissionState', ''),
                       entities=len(ents), errors=errs[:8])
            res.append(row)
            print('%3d %-40s %-12s %s' % (r['n'], r['mission'], row['result'], path), flush=True)
        # the credits: the main menu level and Map.Credits, as the end of the last chapter runs them
        if 3 in {int(x) for x in a.parts.split(',')}:
            sp.try_ask('do SaveGame.QLoad("Levels/Menu")', timeout=30)
            gameclock.sleep(10)
            parity.wait_for(lambda: sp.try_ask('Scrap.GetLevelPath()', timeout=5), 240, 2)
            ok = sp.try_ask('do Map.Credits()', timeout=20)
            gameclock.sleep(10)
            errs, mark = errors_since(sp.log, mark)
            res.append({'n': len(rows) + 1, 'mission': 'the credits', 'level': 'Menu', 'part': 3, 'point': '',
                        'result': 'reached' if ok is not None and not errs else 'not reached',
                        'sp_level': (sp.try_ask('Scrap.GetLevelPath()', timeout=10) or '').strip(), 'errors': errs[:8]})
    finally:
        sp.stop()
    out = {'secs': round(time.time() - t0), 'rows': res}
    json.dump(out, open(os.path.join(a.out, 'spchain.json'), 'w'), indent=1)
    lines = ['# Single player through the coop story\'s chapters (tools/test/parity/spchain.py)', '',
             '| # | mission | part | story point | result | single player level | MissionState | entities | errors |',
             '|---|---|---|---|---|---|---|---|---|']
    for r in res:
        lines.append('| %s | %s | %s | %s | %s | %s | %s | %s | %s |' % (
            r['n'], r['mission'], r['part'], r.get('point') or '-', r['result'], r.get('sp_level', ''),
            r.get('sp_state', ''), r.get('entities', ''), len(r.get('errors') or [])))
    n = sum(1 for r in res if r['result'] == 'reached')
    m = sum(1 for r in res if r['result'] != 'no story point')
    lines += ['', '%d of %d chapters with a story point reached, %d s.' % (n, m, out['secs'])]
    open(os.path.join(a.out, 'spchain.md'), 'w').write('\n'.join(lines) + '\n')
    print('\n'.join(lines[-1:]))
    return 0 if n == m else 1


if __name__ == '__main__':
    sys.exit(main())
