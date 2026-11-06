#!/usr/bin/env python3
"""gen.py <packs> <repo>: the .mod files of the modules listed in tools/modules/modules.tsv (data/modules/<module>.mod,
data/levels/<level>/<module>.mod) from the .pyc of the extracted packs (pyc2mod.py), and
src/game/Script/ModuleFiles.inc that builds them in"""
import os, sys
sys.path.insert(0, os.path.dirname(__file__))
import pyc2mod
root, repo = sys.argv[1], sys.argv[2]
entries = []
for line in open(os.path.join(repo, 'tools/modules/modules.tsv')):
    if not line.strip() or line.startswith('#'):
        continue
    module, level, pyc = line.rstrip('\n').split('\t')
    base = module.lower()
    lines = pyc2mod.convert(module, os.path.join(root, pyc))
    rel = ('modules/%s.mod' % base) if level == '-' else ('levels/%s/%s.mod' % (level, base))
    head = ['# The module code of %s%s (%s): its imports, variables, functions and load time calls in the order of the' %
        (module, '' if level == '-' else ' in level ' + level, pyc),
        '# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.',
        '# A pack in mods/ with scrapc/%s replaces this file.' % rel, '']
    out = os.path.join(repo, 'data', rel)
    os.makedirs(os.path.dirname(out), exist_ok=True)
    open(out, 'w').write('\n'.join(head + [pyc2mod.wrap(l) for l in lines]) + '\n')
    entries.append((None if level == '-' else level, module, rel))
inc = ['/* the module files (tools/modules/gen.py): the built-in copies and their table */']
for level, module, rel in entries:
    ident = 'mod_' + rel[:-4].replace('/', '_')
    inc += ['static const char %s[] = {' % ident, '#embed "../../../data/%s"' % rel, '\t, 0 };']
inc += ['', 'static const moduleFile_t module_files[] = {']
for level, module, rel in entries:
    ident = 'mod_' + rel[:-4].replace('/', '_')
    inc.append('\t{ %s, "%s", "%s", %s },' % ('"%s"' % level if level else 'NULL', module, rel, ident))
inc.append('};')
open(os.path.join(repo, 'src/game/Script/ModuleFiles.inc'), 'w').write('\n'.join(inc) + '\n')
print(len(entries), 'files')
