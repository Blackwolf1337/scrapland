#!/usr/bin/env python3
"""The definitions a -mp2 client's level start must make as the server's (stage 4.2 of the second multiplayer design):
the engine variables of tools/script/netclass/gvar.tsv that a client keeps itself (WORLD_SETTING, STATE, CLIENT_ONLY)
and the module variables of the modules the client's DEF steps read (Init), read on a joined client and on its
instance through the test channel (`get`, `dump`). A value the level's own start sets on the instance and the client
lacks is a missing definition of the level's client part (src/game/Script/levels/Levels.h).

  defcmp.py --client <game folder> --instance <game folder> --level <L> [--json FILE]

The first line is the summary ("defs <L>: N differences"); exit 1 with a difference. Variables that differ by
design are in SKIP: the camera, the screen and the client's own view (the instance has no window), the time."""
import argparse
import json
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
CHANNEL = os.path.join(REPO, 'tools', 'test', 'channel.py')
GVARS = os.path.join(REPO, 'tools', 'script', 'netclass', 'gvar.tsv')
TAGS = ('WORLD_SETTING', 'STATE', 'CLIENT_ONLY')
MODULES = ('Init',)
# differ by design: the window, the camera and the client's own view, the screen, the input, the clocks; the HUD of
# the own player (the instance's world HUD has no player: EnergyBarActive) and its talk times; the loading screen's
# progress bar (ProgressBar); the autoplay volume
# the original resets to -48 on every channel it sets (Fmod_SetChannel: the instance plays none) and the dynamic music
# the sound system's start sets; the talk line width of the screen size; the module variables of the start that name
# the side, and the map libraries a client imports without running them (their functions are WORLD steps)
SKIP = re.compile(r'^(EnergyBarActive$|ProgressBar$|FullTalkTime$|RemainTalkTime$|SoundAutoplayVolume$|useDynamicMusic$|'
                  r'MaxTalkLineSizePix$|Init\.MapLibs$|R_|Cam|Camera(?!Cloud)|FOV|Fov|Screen|Video|Input|Mouse|Joy|Pad|Time|Frame|Fps|Show|Console|Debug|'
                  r'Init\.(MultiPlayer|OffLineGame|Path|inMainMenu|isDemo|NetGame|Server|Client)$)')


def gvar_names():
    out = []
    with open(GVARS) as f:
        for line in f:
            if line.startswith('#') or not line.strip():
                continue
            p = line.rstrip('\n').split('\t')
            if len(p) >= 2 and p[1] in TAGS and not re.search(r'[#*]', p[0]):
                out.append(p[0])
    return out


def channel(game, level, lines):
    cmd = [sys.executable, CHANNEL, game] + (['-i', level] if level else []) + lines
    r = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
    return r.stdout


def read_side(game, level, names):
    vals = {}
    for i in range(0, len(names), 40):
        out = channel(game, level, ['get ' + ' '.join(names[i:i + 40])])
        for m in re.finditer(r"\('([^']+)', (.*?)\)(?=, \(|\]\s*$)", out.strip(), re.M):
            vals[m.group(1)] = m.group(2)
    for mod in MODULES:
        out = channel(game, level, ['dump ' + mod])
        for line in out.splitlines():
            if '\t' in line and not line.startswith('=='):
                k, v = line.split('\t', 1)
                vals['%s.%s' % (mod, k)] = v
    return vals


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--client', required=True)
    ap.add_argument('--instance', required=True)
    ap.add_argument('--level', required=True)
    ap.add_argument('--json')
    a = ap.parse_args()
    names = gvar_names()
    cl = read_side(a.client, None, names)
    ins = read_side(a.instance, a.level, names)
    diffs = []
    for k in sorted(set(cl) | set(ins)):
        if SKIP.search(k):
            continue
        if cl.get(k) != ins.get(k):
            diffs.append({'name': k, 'client': cl.get(k), 'instance': ins.get(k)})
    print('defs %s: %d differences (%d engine and module variables)' % (a.level, len(diffs), len(set(cl) | set(ins))))
    for d in diffs:
        print('  %s: client %s, instance %s' % (d['name'], d['client'], d['instance']))
    if a.json:
        with open(a.json, 'w') as f:
            json.dump({'level': a.level, 'differences': diffs, 'client': cl, 'instance': ins}, f, indent=1)
    return 1 if diffs else 0


if __name__ == '__main__':
    sys.exit(main())
