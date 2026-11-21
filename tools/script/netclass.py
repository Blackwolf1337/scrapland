#!/usr/bin/env python3
# The network classification tables (stage 0.2 of the second multiplayer design, include/scrap/netclass.h): what each
# engine API function, entity variable, engine variable, script module, module variable, save variable prefix and
# level init step is for a networked game. The data are the files tools/script/netclass/<table>.tsv (name, tag, why;
# '#' lines are comments), the tool writes src/game/Script/NetClassTable.c (one array per table sorted as strcmp
# orders the names, for the binary search of src/game/Script/NetClass.c) and include/scrap/netclass.h (the tags and
# tables as enums).
#
#   tools/script/netclass.py              write the table and the header (run it after a .tsv changes, and to resolve
#                                         a merge of the generated files)
#   tools/script/netclass.py --check      exit 1 when the generated files are not what the tool writes, a row has a tag
#                                         its table does not have, a name is twice in a table, or an engine API
#                                         function (a row of the function table of an engine module) has no tag;
#                                         prints the untagged rows of every table (the names the scan finds in the
#                                         code and data that a table does not have count as untagged)
#   tools/script/netclass.py --scan       appends the names the scan finds that a table does not have to its .tsv
#                                         (without a tag; a module gets the tag the engine calls of its file propose,
#                                         marked "proposed")
#   tools/script/netclass.py --list [table ...]   the untagged and missing names per table
#   --quiet                               --check prints only the errors (the build step of CMakeLists.txt)
#
# The build runs --check --quiet (CMakeLists.txt, target netclass_check, before scrapcode): an engine function
# without a tag fails the build. The other tables only report their untagged rows until stage 3.2 of the plan.
#
# A name of a table may end in '*' (every name with that start: "AI_*") or hold '#' for a run of digits
# ("snd#_vol#"): NetClass_Find tries the name, then the name with its digit runs as '#', each with the longest
# start that has a '*' row.
import argparse, collections, glob, os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DATA = os.path.join(ROOT, 'tools/script/netclass')
TABLEFILE = os.path.join(ROOT, 'src/game/Script/NetClassTable.c')
HEADER = os.path.join(ROOT, 'include/scrap/netclass.h')
GAME = os.path.join(ROOT, 'src/game')

# every tag, in the order of the enum (NC_UNTAGGED is 0)
TAGS = ['SIM', 'DEF', 'PRES_STATE', 'PRES_EVENT', 'UI', 'QUERY', 'CLIENT', 'NET_OLD', 'STATE', 'EVENT',
	'SERVER_ONLY', 'CLIENT_ONLY', 'WORLD_SETTING', 'SCENE', 'WORLD', 'PLAYER', 'PARTY', 'STORY', 'PLAYER_ENTER',
	'NEAREST', 'ANY', 'PLAYER_SHARED', 'PARTY_SHARED', 'GROUP', 'MENU', 'DEAD', 'ALL']
# the tables: name, the tags its rows may have, what a row is
TABLES = collections.OrderedDict([
	('api', (['SIM', 'DEF', 'PRES_STATE', 'PRES_EVENT', 'UI', 'QUERY', 'CLIENT', 'NET_OLD'],
		'an engine API function: Module.Function')),
	('entvar', (['SIM', 'STATE', 'EVENT', 'SERVER_ONLY', 'CLIENT_ONLY'], 'an entity variable')),
	('gvar', (['SIM', 'STATE', 'EVENT', 'SERVER_ONLY', 'CLIENT_ONLY', 'WORLD_SETTING'],
		'an engine variable scripts or data set')),
	('module', (['DEF', 'SIM', 'UI', 'SCENE'],
		'a script module: Module, or level/Module for a module of a level; Module.func for a function of a mixed module')),
	('modvar', (['WORLD', 'PLAYER', 'PARTY', 'PLAYER_SHARED', 'PARTY_SHARED', 'CLIENT'],
		'a script module variable: Module.name')),
	('savevar', (['STORY', 'PLAYER'], 'a save variable prefix (the name up to its first dot)')),
	('initstep', (['WORLD', 'PLAYER_ENTER', 'DEF', 'GROUP'], 'a step of the level start: File:call')),
	('playerread', (['NEAREST', 'ANY', 'PARTY', 'MENU', 'DEAD', 'ALL', 'WORLD'],
		'a read of "the player" in the world context: C function or Module.variable')),
])

Row = collections.namedtuple('Row', 'name tag why')


def src(path):
	return open(os.path.join(ROOT, path), encoding='utf-8', errors='replace').read()


def nocomments(s):
	return re.sub(r'/\*.*?\*/', ' ', re.sub(r'//[^\n]*', '', s), flags=re.S)


def body_at(s, i):
	"""the text of the block whose '{' is at or after i, without the braces"""
	i = s.index('{', i)
	d, j = 1, i + 1
	while d and j < len(s):
		if s[j] == '{':
			d += 1
		elif s[j] == '}':
			d -= 1
		j += 1
	return s[i + 1:j - 1]


def norm_digits(name):
	return re.sub(r'[0-9]+', '#', name)


def covered(name, names):
	"""name is in names as itself, with its digit runs as '#', or under a '*' row (NetClass_Find in C)"""
	for n in (name, norm_digits(name)):
		if n in names:
			return n
		for k in range(len(n), 0, -1):
			if n[:k] + '*' in names:
				return n[:k] + '*'
	return None


# ---- the scans: the names the code and data have, name -> (proposed tag, why) ----

def scan_api():
	out = collections.OrderedDict()
	for f in sorted(glob.glob(os.path.join(GAME, 'PyInterface/*.c'))):
		s = nocomments(open(f, errors='replace').read())
		for m in re.finditer(r'^SD_PY_MODULE\((\w+),', s, re.M):
			mod = m.group(1)
			t = re.search(r'\bportFn_t\s+%s_methods\s*\[\s*\]\s*=\s*\{(.*?)PORT_FNEND' % mod, s, re.S)
			for name in re.findall(r'PORT_FN\w*\(\s*(\w+)\s*,', t.group(1) if t else ''):
				out.setdefault('%s.%s' % (mod, name), ('', os.path.relpath(f, ROOT)))
	# the original's PvP modes of the split screen game (their own C modules, counted with the engine API)
	s = nocomments(src('src/game/Script/Split.c'))
	for mod in ('SDeathMatch', 'SFlagHunt', 'SPostman', 'SRace'):
		t = re.search(r'\bportFn_t\s+g_%sMethods\s*\[\s*\]\s*=\s*\{(.*?)PORT_FNEND' % mod, s, re.S)
		for name in re.findall(r'PORT_FN\w*\(\s*(\w+)\s*,', t.group(1) if t else ''):
			out.setdefault('%s.%s' % (mod, name), ('', 'src/game/Script/Split.c'))
	return out


# the entity variables: the names the Var functions of the classes compare, with the start the entity's dispatch puts
# before the names of a part (cEntity_VarF: "AI_" and "LLogic", cWithLifeEntity_VarI: "Hit_", cVehicleEntity_VarF:
# "Traf_" and "InCtrl_"), and the names the scripts set on entities
ENT_DIRS = ['Entities', 'AI', 'Logic', 'Actions', 'Sound']
ENT_SKIP = {'ActClass.c', 'Actions.c', 'PosSound.c', 'SndInterface.c'}	# class, action and sound set variables
ENT_PREFIX = [(r'^AI/', 'AI_'), (r'^Logic/', 'LLogic'), (r'VehicleAttack\.c$', 'Hit_'),
	(r'VehicleTraffic\.c$', 'Traf_'), (r'(Vehicle/Controls|WalkCharControl)\.c$', 'InCtrl_')]
# the sound items of an entity (cSndInterface: "snd<nn>_<field>") and the node places of its model
ENT_FAMILIES = ['snd#_n', 'snd#_loop', 'snd#_int#', 'snd#_ini', 'snd#_end', 'snd#_dopp', 'snd#_vol', 'snd#_time',
	'snd#_pitch', 'snd#_vol#', 'snd#_pitch#', 'snd#_time#', 'snd#_name', 'NodePos_*', 'NT_NodePos_*']
ENT_SETTERS = r'(?:Mode_VSet\w*|LvN_Set[ISF]|Sp_HSet[ISF]|Char_Set(?:Text|Int|Num)?|Mode_EntSet\w*|Native_EntSet)'
IDENT = re.compile(r'^[A-Za-z_][A-Za-z0-9_]*$')


def scan_entvar():
	out = collections.OrderedDict()
	for n in ENT_FAMILIES:
		out[n] = ('', 'cSndInterface' if n.startswith('snd') else 'cObjEntity_VarV')
	fnre = re.compile(r'^(?:static\s+)?(?:byte|int)\s+(\w*Var[IFSV]?)\s*\(([^)]*)\)\s*\{', re.M)
	for d in ENT_DIRS:
		for f in sorted(glob.glob(os.path.join(GAME, d, '**/*.c'), recursive=True)):
			if os.path.basename(f) in ENT_SKIP:
				continue
			rel = os.path.relpath(f, GAME)
			pre = next((p for r, p in ENT_PREFIX if re.search(r, rel)), '')
			s = nocomments(open(f, errors='replace').read())
			for m in fnre.finditer(s):
				if 'name' not in m.group(2):
					continue
				body = body_at(s, m.end() - 1)
				lits = re.findall(r'(?:str(?:i)?cmp|crt_str(?:i)?cmp)\s*\(\s*name\s*,\s*"([^"]+)"', body)
				lits += re.findall(r'(?:str(?:i)?cmp|crt_str(?:i)?cmp)\s*\(\s*"([^"]+)"\s*,\s*name\s*\)', body)
				lits += re.findall(r'\{\s*"([A-Za-z_][A-Za-z0-9_]*)"\s*,', body)
				for lit in lits:
					if IDENT.match(lit):
						out.setdefault(pre + lit, ('', '%s %s' % (rel, m.group(1))))
	for f in sorted(glob.glob(os.path.join(GAME, 'Script/**/*.c'), recursive=True)):
		s = nocomments(open(f, errors='replace').read())
		for lit in re.findall(ENT_SETTERS + r'\([^,;()]*(?:\([^()]*\))?[^,;()]*,\s*"([A-Za-z_][A-Za-z0-9_]*)"', s):
			n = lit if not re.search(r'[0-9]', lit) else norm_digits(lit)
			if not covered(n, out):
				out.setdefault(n, ('', 'set by %s' % os.path.relpath(f, GAME)))
	return out


def scan_gvar():
	out = collections.OrderedDict()
	for f in sorted(glob.glob(os.path.join(ROOT, 'data/**/*.def'), recursive=True) +
			glob.glob(os.path.join(ROOT, 'data/**/*.mod'), recursive=True)):
		for name in re.findall(r'\bcall\s+Scrap\.Set\s+([A-Za-z_]\w*)', open(f, errors='replace').read()):
			out.setdefault(name, ('', os.path.relpath(f, ROOT)))
	for f in sorted(glob.glob(os.path.join(GAME, 'Script/**/*.c'), recursive=True)):
		s = nocomments(open(f, errors='replace').read())
		names = re.findall(r'\b(?:Mode_GVarSet\w*|GVar_Set\w*|Native_GVarSet|Native_GVarStore|LvN_SetG)\(\s*"(\w+)"', s)
		names += re.findall(r'PORT_CALL\(\s*Scrap\s*,\s*Set\s*,\s*\w+\s*,\s*"[^"]*"\s*,\s*"(\w+)"', s)
		for name in names:
			out.setdefault(name, ('', os.path.relpath(f, GAME)))
	return out


def module_files():
	"""(module, level) of tools/modules/modules.tsv"""
	mods = []
	for line in src('tools/modules/modules.tsv').split('\n'):
		if not line.strip() or line.startswith('#'):
			continue
		f = line.split('\t')
		mods.append((f[0], None if f[1] == '-' else f[1]))
	return mods


def module_name(mod, level):
	return '%s/%s' % (level, mod) if level else mod


def scan_module(api):
	"""the modules, each with a tag proposed by the engine calls of the C files that define its functions"""
	files = collections.defaultdict(set)
	hdr = src('include/scrap/scriptports.h')
	for m in re.finditer(r'/\* (\S+\.c) \*/(.*?)(?=/\* \S+\.c \*/|\Z)', hdr, re.S):
		for mod in re.findall(r'PORT_DECL\((\w+),', m.group(2)):
			files[(mod, None)].add(m.group(1))
		for lvl, mod in re.findall(r'PORT_DECLL\((\w+),\s*(\w+),', m.group(2)):
			files[(mod, lvl)].add(m.group(1))
	paths = {os.path.relpath(f, os.path.join(GAME, 'Script')): f
		for f in glob.glob(os.path.join(GAME, 'Script/**/*.c'), recursive=True)}
	tags = {n: r.tag for n, r in api.items()}
	out = collections.OrderedDict()
	for mod, level in module_files():
		calls = collections.Counter()
		for f in files.get((mod, level), set()) | files.get((mod, None), set()):
			if f not in paths:
				continue
			s = nocomments(open(paths[f], errors='replace').read())
			for em, fn in re.findall(r'PORT_CALL\(\s*(\w+)\s*,\s*(\w+)', s):
				t = tags.get('%s.%s' % (em, fn))
				if t:
					calls[t] += 1
			calls['UI'] += len(re.findall(r'\b(?:Mode_Scorer\w*|Scorer_\w+|ScorerItem_\w+)\(', s))
			calls['SIM'] += len(re.findall(r'\b(?:Mode_Create|Native_EntSet|LvN_Set[ISF]|Mode_VSet)\(', s))
		if mod == 'Scene' or mod.startswith('Scene'):
			tag = 'SCENE'
		elif not calls:
			tag = 'DEF'
		elif calls['SIM'] or calls['NET_OLD']:
			tag = 'UI' if calls['UI'] > 2 * calls['SIM'] else 'SIM'
		elif calls['UI']:
			tag = 'UI'
		else:
			tag = 'DEF'
		why = 'proposed: ' + ', '.join('%s %d' % kv for kv in sorted(calls.items())) if calls else 'proposed: no engine calls'
		out[module_name(mod, level)] = (tag, why)
	return out


def scan_modvar():
	s = src('src/game/Script/ModVarTable.c')
	tabs = {m.group(1): re.findall(r'\{ "([^"]*)", (MVK_\w+)', m.group(2))
		for m in re.finditer(r'static const modVar_t (\w+)\[\] = \{(.*?)\n\};', s, re.S)}
	out = collections.OrderedDict()
	for mod, level, tab in re.findall(r'\{ "(\w+)", (NULL|"\w+"), (\w+), sizeof', s):
		for name, kind in tabs[tab]:
			if kind == 'MVK_VAR':
				out.setdefault('%s.%s' % (mod, name), ('', 'ModVarTable.c'))
	return out


def scan_savevar():
	out = collections.OrderedDict()
	for f in sorted(glob.glob(os.path.join(GAME, '**/*.c'), recursive=True)):
		s = nocomments(open(f, errors='replace').read())
		for name in re.findall(r'\b\w*Save(?:Var|Vars|Int|S|Is|Repr|List|Append|Copy)\w*\(\s*"([A-Za-z_][\w.]*)"', s):
			out.setdefault(name.split('.')[0], ('', os.path.relpath(f, GAME)))
	for f in sorted(glob.glob(os.path.join(ROOT, 'data/**/*.def'), recursive=True)):
		for name in re.findall(r'\bcall\s+Scrap\.(?:Set|Create|Inc|Get|Del)SaveVars?\s+([A-Za-z_][\w.]*)',
				open(f, errors='replace').read()):
			out.setdefault(name.split('.')[0], ('', os.path.relpath(f, ROOT)))
	return out


# the functions of the level start and the calls in them that are steps
INIT_FUNCS = [('InitLevel', 'src/game/Script/InitLevel.c', ['In_InitN', 'In_Game']),
	('OutMap', 'src/game/Script/OutMap.c', ['Om_Init', 'Om_InitOutMap']),
	('InMap', 'src/game/Script/InMap.c', ['InMap_Init', 'InMap_Start'])]
INIT_CALL = re.compile(r'\b(?:IN_CALLV?|OM_START|PORT_CALL|PORT_OWN)\(\s*(\w+)\s*,\s*(\w+)'
	r'|\bMODE_CBATTR\(\s*"(\w+)"\s*,\s*"(\w+)"'
	r'|\b(?:Mode_GVarSet\w*|GVar_Set\w*|Native_GVarSet)\(\s*"(\w+)"'
	r'|\b(?:SaveVar_Set|Mode_SaveS)\(\s*"([\w.]+)"'
	r'|\b(In_(?!Flag\b|Method\b)\w+|Places_\w+|Coop_\w+|Mode_SetGrid|Grid_Set)\('
	r'|\bIn_Method\(\s*\w+\s*,\s*"(\w+)"')


def scan_initstep():
	out = collections.OrderedDict()
	for tag, path, funcs in INIT_FUNCS:
		s = nocomments(src(path))
		for fn in funcs:
			m = re.search(r'^(?:static\s+)?byte\s+%s\s*\(' % fn, s, re.M)
			if not m:
				continue
			for c in INIT_CALL.finditer(body_at(s, m.end())):
				g = c.groups()
				if g[0]:
					name = '%s.%s' % (g[0], g[1])
				elif g[2]:
					name = '%s.%s' % (g[2], g[3])
				elif g[4]:
					name = 'GVar.' + g[4]
				elif g[5]:
					name = 'SaveVar.' + g[5]
				elif g[6]:
					name = g[6]
				else:
					name = 'SuperDealModule.' + g[7]
				out.setdefault('%s:%s' % (tag, name), ('', fn))
	return out


# ---- the tables ----

def read_tsv(table):
	path = os.path.join(DATA, table + '.tsv')
	head, rows, errors = [], collections.OrderedDict(), []
	if not os.path.exists(path):
		return head, rows, errors
	for i, line in enumerate(open(path, encoding='utf-8').read().split('\n')):
		if line.startswith('#'):
			if not rows:
				head.append(line)
			continue
		if not line.strip():
			continue
		f = line.split('\t')
		f += [''] * (3 - len(f))
		name, tag, why = f[0].strip(), f[1].strip(), f[2].strip()
		if name in rows:
			errors.append('%s.tsv:%d: %s is twice in the table' % (table, i + 1, name))
		if tag and tag not in TABLES[table][0]:
			errors.append('%s.tsv:%d: %s has the tag %s, the table has %s' % (table, i + 1, name, tag,
				' '.join(TABLES[table][0])))
		rows[name] = Row(name, tag, why)
	return head, rows, errors


def write_tsv(table, head, rows):
	with open(os.path.join(DATA, table + '.tsv'), 'w', encoding='utf-8') as f:
		for h in head:
			f.write(h + '\n')
		for r in rows.values():
			f.write('%s\t%s\t%s\n' % (r.name, r.tag, r.why))


def cstr(s):
	return '"%s"' % s.replace('\\', '\\\\').replace('"', '\\"')


def header():
	o = ['/*',
		' * The network classification tables (stage 0.2 of the second multiplayer design): the tag of each engine API',
		' * function, entity variable, engine variable, script module, module variable, save variable prefix and level',
		' * init step. Made by tools/script/netclass.py from tools/script/netclass/<table>.tsv (the tags and their meaning are',
		' * in the head of each file), with src/game/Script/NetClassTable.c. Do not edit: run the tool after a .tsv',
		' * changes. The lookup is src/game/Script/NetClass.c.',
		' */',
		'#ifndef SCRAP_NETCLASS_H',
		'#define SCRAP_NETCLASS_H',
		'',
		'#include <stdint.h>',
		'',
		'/* the tags; NC_UNTAGGED: a row without one, NC_ABSENT: no row */',
		'typedef enum {',
		'\tNC_ABSENT = -1,',
		'\tNC_UNTAGGED = 0,']
	for i, t in enumerate(TAGS):
		o.append('\tNC_%s = %d,' % (t, i + 1))
	o += ['\tNC_TAGS', '} netTag_t;', '', '/* the tables */', 'typedef enum {']
	for t, (_, what) in TABLES.items():
		o.append('\tNCT_%s,\t\t\t/* %s */' % (t.upper(), what))
	o += ['\tNCT_TABLES', '} netTable_t;', '',
		'typedef struct {',
		'\tconst char *name;',
		'\tuint8_t tag;			/* netTag_t */',
		'} netClassRow_t;',
		'',
		'typedef struct {',
		'\tconst char *name;',
		'\tconst netClassRow_t *rows;	/* sorted as strcmp orders the names */',
		'\tint count;',
		'} netClassTable_t;',
		'',
		'extern const netClassTable_t g_NetClassTables[NCT_TABLES];',
		'',
		'/* the tag of name in table t: the row of the name, else of the name with its digit runs as \'#\', each also as',
		' * the longest start with a \'*\' row ("AI_*"); NC_ABSENT without one */',
		'netTag_t NetClass_Find(netTable_t t, const char *name);',
		'/* the row of name in table t as NetClass_Find finds it, NULL */',
		'const netClassRow_t *NetClass_Row(netTable_t t, const char *name);',
		'/* the tag of a function of a script module in the module table: the row "Module.func", else "Module", else the',
		' * same forms of a level\'s module "level/Module.func", "level/Module" (level: the lower case name, NULL) */',
		'netTag_t NetClass_ModuleTag(const char *module, const char *func, const char *level);',
		'/* the name of a tag ("SIM"; "untagged", "absent") and of a table ("api") */',
		'const char *NetClass_TagName(netTag_t tag);',
		'const char *NetClass_TableName(netTable_t t);',
		'/* the rows of table t without a tag */',
		'int NetClass_Untagged(netTable_t t);',
		'',
		'#endif']
	return '\n'.join(o) + '\n'


def table_c(data):
	o = ['/*',
		' * The network classification tables (include/scrap/netclass.h): the rows of tools/script/netclass/<table>.tsv, each',
		' * table sorted as strcmp orders the names. Made by tools/script/netclass.py. Do not edit: run the tool after a',
		' * .tsv changes, and to resolve a merge.',
		' */',
		'#include "scrap/netclass.h"',
		'']
	for t, rows in data.items():
		o.append('static const netClassRow_t nc_%s[] = {' % t)
		for name in sorted(rows, key=lambda x: x.encode()):
			r = rows[name]
			o.append('\t{ %s, %s },' % (cstr(name), 'NC_' + r.tag if r.tag else 'NC_UNTAGGED'))
		o.append('};')
	o.append('')
	o.append('const netClassTable_t g_NetClassTables[NCT_TABLES] = {')
	for t in data:
		o.append('\t{ "%s", nc_%s, sizeof(nc_%s) / sizeof(nc_%s[0]) },' % (t, t, t, t))
	o.append('};')
	return '\n'.join(o) + '\n'


def scans(api_rows):
	return collections.OrderedDict([('api', scan_api()), ('entvar', scan_entvar()), ('gvar', scan_gvar()),
		('module', scan_module(api_rows)), ('modvar', scan_modvar()), ('savevar', scan_savevar()),
		('initstep', scan_initstep())])


def main():
	ap = argparse.ArgumentParser(description='the network classification tables (include/scrap/netclass.h)')
	ap.add_argument('--check', action='store_true')
	ap.add_argument('--scan', action='store_true')
	ap.add_argument('--list', nargs='*')
	ap.add_argument('--quiet', action='store_true')
	a = ap.parse_args()
	heads, data, errors = {}, collections.OrderedDict(), []
	for t in TABLES:
		heads[t], data[t], e = read_tsv(t)
		errors += e
	# the build step needs only the API scan; the others are for the report
	found = scans(data['api']) if not (a.check and a.quiet) else {'api': scan_api()}
	for name in found['api']:
		r = data['api'].get(name)
		if not r:
			errors.append('api: %s has no row in tools/script/netclass/api.tsv (give it a tag)' % name)
		elif not r.tag:
			errors.append('api: %s has no tag in tools/script/netclass/api.tsv' % name)
	if a.scan:
		for t, names in found.items():
			added = 0
			for name, (tag, why) in names.items():
				if not covered(name, data[t]):
					data[t][name] = Row(name, tag, why)
					added += 1
			if added:
				write_tsv(t, heads[t], data[t])
			print('netclass: %s: %d names added' % (t, added))
		return 0
	report = []
	for t in TABLES:
		untagged = [n for n, r in data[t].items() if not r.tag]
		missing = [n for n in found.get(t, ()) if not covered(n, data[t])]
		report.append((t, len(data[t]), untagged, missing))
	if a.list is not None:
		for t, n, untagged, missing in report:
			if a.list and t not in a.list:
				continue
			print('== %s: %d rows, %d untagged, %d missing' % (t, n, len(untagged), len(missing)))
			for name in untagged:
				print('untagged\t' + name)
			for name in missing:
				print('missing\t' + name)
		return 0
	tab, hdr = table_c(data), header()
	if a.check:
		for path, text in ((TABLEFILE, tab), (HEADER, hdr)):
			old = open(path).read() if os.path.exists(path) else ''
			if old != text:
				errors.append('%s is not up to date (run tools/script/netclass.py)' % os.path.relpath(path, ROOT))
		for e in errors:
			print('netclass: error: ' + e)
		if not a.quiet:
			for t, n, untagged, missing in report:
				print('netclass: %-8s %5d rows, %4d untagged, %4d found in the code and data without a row' % (t, n,
					len(untagged), len(missing)))
		return 1 if errors else 0
	if errors:
		for e in errors:
			print('netclass: error: ' + e)
	open(TABLEFILE, 'w').write(tab)
	open(HEADER, 'w').write(hdr)
	print('netclass: %s written to %s and %s' % (', '.join('%s %d' % (t, len(r)) for t, r in data.items()),
		os.path.relpath(TABLEFILE, ROOT), os.path.relpath(HEADER, ROOT)))
	return 1 if errors else 0


if __name__ == '__main__':
	sys.exit(main())
