#!/usr/bin/env python3
"""gen.py <packs> <repo>: data/levels/<level>/<module>.def from the level scripts of the extracted packs (<packs>/<pack>/levels/<level>/scripts/*.py, see docs/packed.md) by py2def.py, and src/game/Script/LevelFiles.inc"""
import glob, os, sys, re
sys.path.insert(0, os.path.dirname(__file__))
import py2def
root, repo = sys.argv[1], sys.argv[2]
MODULES = {'mapsnd': 'MapSnd', 'map': 'Map'}
# the other modules of a level (missions, map libraries): the packs keep lower case file names, the module name is the
# spelling the scripts use (Mission_MeetArchbishop)
alltext = '\n'.join(open(f, encoding='latin1').read() for f in glob.glob(root + '/**/*.py', recursive=True))


def module_name(base):
    if base in MODULES:
        return MODULES[base]
    found = {}
    for m in re.findall(r'\b(%s)\b' % re.escape(base), alltext, re.I):
        if m != base:
            found[m] = found.get(m, 0) + 1
    return max(found, key=found.get) if found else None


entries = []
for path in sorted(glob.glob(root + '/*/levels/*/scripts/*.py')):
    level = path.split('/levels/')[1].split('/')[0]
    base = os.path.basename(path)[:-3]
    module = module_name(base)
    if level.startswith('_') or not module:
        continue
    sections, kept = py2def.convert(module, path, True)
    if not sections:
        continue
    pack = path.split('/packs/')[1].split('/')[0]
    cfiles = dict((f.lower(), f) for f in os.listdir(os.path.join(repo, 'src/game/Script/levels')))
    rest = 'are C (src/game/Script/levels/%s)' % cfiles[level + '.c'] if level + '.c' in cfiles else 'stay Python'
    head = ['# The functions of %s in level %s (the script levels/%s/scripts/%s.py of %s) that are data: one section' % (module, level, level, base, pack),
            '# per function, its parameters after the name. The other functions of the script %s%s.' % (rest,
                (': ' + ', '.join(k for k, _ in kept)) if kept else ''),
            '# Made by a converter from the script. A pack in mods/ with scrapc/levels/%s/%s.def replaces this file.' % (level, base), '']
    text = '\n'.join(head) + '\n\n'.join('\n'.join(s) for s in sections) + '\n'
    out = os.path.join(repo, 'data', 'levels', level)
    os.makedirs(out, exist_ok=True)
    open(os.path.join(out, base + '.def'), 'w').write(text)
    entries.append((level, module, base))
inc = ['/* the data files of the level modules (made by the level converter): the built-in copies and their table */']
for level, module, base in entries:
    inc += ['static const char lvl_%s_%s[] = {' % (level, base), '#embed "../../../data/levels/%s/%s.def"' % (level, base), '\t, 0 };']
inc.append('')
inc.append('static const levelFile_t level_files[] = {')
for level, module, base in entries:
    inc.append('\t{ "%s", "%s", "levels/%s/%s.def", lvl_%s_%s },' % (level, module, level, base, level, base))
inc.append('};')
open(os.path.join(repo, 'src/game/Script/LevelFiles.inc'), 'w').write('\n'.join(inc) + '\n')
print(len(entries), 'files')
