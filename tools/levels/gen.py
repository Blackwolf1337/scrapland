#!/usr/bin/env python3
"""gen.py <packs> <repo>: data/levels/<level>/<module>.def from the level scripts of the extracted packs (<packs>/<pack>/levels/<level>/scripts/*.py, see docs/packed.md) by py2def.py, and src/game/Script/LevelFiles.inc"""
import glob, os, sys, re
sys.path.insert(0, os.path.dirname(__file__))
import py2def
root, repo = sys.argv[1], sys.argv[2]
MODULES = {'mapsnd': 'MapSnd', 'map': 'Map'}
entries = []
for path in sorted(glob.glob(root + '/*/levels/*/scripts/*.py')):
    level = path.split('/levels/')[1].split('/')[0]
    base = os.path.basename(path)[:-3]
    if level.startswith('_') or base not in MODULES:
        continue
    module = MODULES[base]
    sections, kept = py2def.convert(module, path)
    if not sections:
        continue
    pack = path.split('/packs/')[1].split('/')[0]
    head = ['# The functions of %s in level %s (the script levels/%s/scripts/%s.py of %s) that are data: one section' % (module, level, level, base, pack),
            '# per function, its parameters after the name. The other functions of the script stay Python%s.' % (
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
