#!/usr/bin/env python3
"""The case lines on the test channel of the game (src/game/Global/TestChannel.c, TestExpr.c): a case line written for
the Python harness becomes the channel commands of one command file (run in one frame, as the Python line ran in one
call), so the case runs with no Python code in the game and its replies compare with the stored pack baselines.

Most case lines are channel statements as they are (the channel reads Python's expression syntax, lambdas and simple
statements in C). What changes:
  - execfile('<helper>.py'), exec(open('<helper>.py').read()) [in <module>.__dict__]: the helper's channel twin
    channel/<helper>.tc (the same functions as channel statements) is loaded into __main__ or the module first;
  - exec "<code>" [in <module>.__dict__]: the code's channel twin from EXEC below;
  - what still needs Python (a def, a class, a loop, compile, ...) raises Unconvertible with the reason.

  tochannel.py [case ...]          the converted lines of the cases (all of cases.txt without names)
  tochannel.py --stats [case ...]  per case the lines that do not convert and why, then the totals

A converted line is a list of commands: "do <statement>" or "in <module> do <statement>" for the helpers, then the
line itself. The reply of the line is the first error answer of the helper commands, else the answer of the last
command (regress.py)."""
import ast
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
CHANNEL = os.path.join(HERE, 'channel')


class Unconvertible(Exception):
	pass


# the exec strings of the case files and their channel statements (run in __main__ unless the exec names a module)
EXEC = {
	# fc-cc-pos: the frames of a traceback
	"def _tb(t):\n\tr = []\n\twhile t:\n\t\tr.append((t.tb_frame.f_code.co_filename, t.tb_frame.f_code.co_name, t.tb_lineno))\n"
	"\t\tt = t.tb_next\n\treturn r\n": [
		"_tb = lambda t: map(lambda tb: (tb.tb_frame.f_code.co_filename, tb.tb_frame.f_code.co_name, tb.tb_lineno), "
		"tc_walk(t, lambda tb: tb.tb_next))"],
	# fc-chat
	"import Init, SNet, SScorer\nInit._rec = []\ndef _snd(*a):\n\tInit._rec.append(('Send',) + a)\ndef _msg(*a):\n"
	"\tInit._rec.append(('Msg',) + a)\n\tapply(Init._oldmsg, a)\nInit._oldmsg = SScorer.AddChatMsg\n"
	"Init._oldflags = Scrap.GetNetFlags\nSNet.SendChatString = _snd\nSScorer.AddChatMsg = _msg\n"
	"SNet.GetMyClientShip = lambda: 'net7'": [
		"import Init, SNet, SScorer",
		"Init._rec = []",
		"_snd = lambda *a: Init._rec.append(('Send',) + a)",
		"_msg = lambda *a: (Init._rec.append(('Msg',) + a), apply(Init._oldmsg, a), None)[2]",
		"Init._oldmsg = SScorer.AddChatMsg",
		"Init._oldflags = Scrap.GetNetFlags",
		"SNet.SendChatString = _snd",
		"SScorer.AddChatMsg = _msg",
		"SNet.GetMyClientShip = lambda: 'net7'"],
	# fc-credits
	"import Init, CreditsText\nInit._rec = []\nCreditsText.EndOfCredits = lambda: Init._rec.append('end')": [
		"import Init, CreditsText",
		"Init._rec = []",
		"CreditsText.EndOfCredits = lambda: Init._rec.append('end')"],
	"import Init\ndef _cr():\n\timport CreditsText\n\treturn (Init._mod('CreditsText', ('TextInfo','TextPos','TextLines','ctr')), "
	"map(lambda v: SScorer.Get(0, 'Credits', v), ('Visible','Time','Text0','Text1','Text2','Text3','Text19')), "
	"Scrap.Get('R_CreditsX'), Scrap.Get('R_CreditsY'), Init._rec)\nInit._cr = _cr": [
		"import Init",
		"_cr = lambda: (__import__('CreditsText'), (Init._mod('CreditsText', ('TextInfo','TextPos','TextLines','ctr')), "
		"map(lambda v: SScorer.Get(0, 'Credits', v), ('Visible','Time','Text0','Text1','Text2','Text3','Text19')), "
		"Scrap.Get('R_CreditsX'), Scrap.Get('R_CreditsY'), Init._rec))[1]",
		"Init._cr = _cr"],
	# fc-credits2
	"import Init, SScorer\nInit._rec = []\nInit._oldset = SScorer.Set\ndef _set(id, item, var, val):\n"
	"\tif var[:4] == 'Text': Init._rec.append((id, item, var, val))\n\treturn Init._oldset(id, item, var, val)\n"
	"SScorer.Set = _set": [
		"import Init, SScorer",
		"Init._rec = []",
		"Init._oldset = SScorer.Set",
		"_set = lambda id, item, var, val: (var[:4] == 'Text' and Init._rec.append((id, item, var, val)), "
		"Init._oldset(id, item, var, val))[1]",
		"SScorer.Set = _set"],
	# fc-inmap
	"def _dm():\n\tScrap.StartDummySearch('DM_DesktopFunctionary_*',1)\n\tr = []\n\twhile 1:\n\t\td = Scrap.NextDummySearch()\n"
	"\t\tif d == None: return r\n\t\tr.append(d[0])\nimport Init\nInit._dm = _dm": [
		"_dm = lambda: (Scrap.StartDummySearch('DM_DesktopFunctionary_*',1), map(lambda d: d[0], "
		"tc_walk(Scrap.NextDummySearch(), lambda d: Scrap.NextDummySearch())))[1]",
		"import Init",
		"Init._dm = _dm"],
	# fc-lvl-menu: a module of its own for CreditsText
	"import imp, sys\nct = imp.new_module('CreditsText')\nct.ShowCredit = __import__('Init')._recn('ShowCredit')\n"
	"sys.modules['CreditsText'] = ct\n__import__('Init')._ct = ct": [
		"import imp, sys",
		"ct = imp.new_module('CreditsText')",
		"ct.ShowCredit = __import__('Init')._recn('ShowCredit')",
		"sys.modules['CreditsText'] = ct",
		"__import__('Init')._ct = ct"],
	# the mission cases (fc-mis*)
	"def _dd(pat):\n\tScrap.StartDummySearch(pat,1)\n\tr = []\n\twhile 1:\n\t\td = Scrap.NextDummySearch()\n"
	"\t\tif d == None: return r\n\t\tr.append(d[0])\nimport Init\nInit._dd = _dd": [
		"_dd = lambda pat: (Scrap.StartDummySearch(pat,1), map(lambda d: d[0], "
		"tc_walk(Scrap.NextDummySearch(), lambda d: Scrap.NextDummySearch())))[1]",
		"import Init",
		"Init._dd = _dd"],
	"def _fn(v):\n\tif type(v) in (type(()), type([])):\n\t\treturn map(_fn, v)\n\tif hasattr(v, '__name__'): return v.__name__\n"
	"\treturn v\ndef _di(d):\n\tk = d.keys()\n\tk.sort()\n\treturn map(lambda k, d=d: (k, _fn(d[k])), k)\nimport Init\n"
	"Init._fn = _fn\nInit._di = _di\nInit._r = []\ndef _rec(*a, **k):\n\tInit._r.append(a)\n\treturn 1\nInit._rec = _rec\n"
	"class _Rec:\n\tdef __init__(self, n):\n\t\tself.n = n\n\tdef __call__(self, *a, **k):\n"
	"\t\tInit._r.append((self.n,) + a + tuple(k.items()))\n\t\treturn 1\nInit._recn = _Rec": [
		"_fn = lambda v: map(_fn, v) if type(v) in (type(()), type([])) else (v.__name__ if hasattr(v, '__name__') else v)",
		"_di = lambda d: map(lambda k: (k, _fn(d[k])), tc_sorted(d.keys()))",
		"import Init",
		"Init._fn = _fn",
		"Init._di = _di",
		"Init._r = []",
		"_rec = lambda *a, **k: (Init._r.append(a), 1)[1]",
		"Init._rec = _rec",
		"_Rec = lambda n: tc_obj('__main__._Rec', {'n': n}, lambda *a, **k: (Init._r.append((n,) + a + tuple(k.items())), 1)[1])",
		"Init._recn = _Rec"],
	# the deleted-handle probes (fc-nc-*)
	"def _pb(m, f, n):\n\timport Scrap, sys, Init\n\ttry:\n\t\tr = apply(getattr(__import__(m), f), (Init._dh,) * n)\n"
	"\t\tr = ('ok', type(r).__name__)\n\texcept:\n\t\tr = ('error', str(sys.exc_type), str(sys.exc_value))\n"
	"\tScrap.SetTimeSpeed(1)\n\treturn r\nimport Init\nInit._pb = _pb": [
		"_pb = lambda m, f, n: (lambda r: (Scrap.SetTimeSpeed(1), r)[1])(tc_catch(lambda: ('ok', "
		"type(apply(getattr(__import__(m), f), (Init._dh,) * n)).__name__), (), None, lambda t, v: ('error', str(t), str(v))))",
		"import Init",
		"Init._pb = _pb"],
	# fc-npc
	"def _res():\n\timport CharsNPC\n\tk = CharsNPC.CharRes.keys()\n\tk.sort()\n\tr = []\n\tfor n in k:\n"
	"\t\tc = CharsNPC.CharRes[n]\n\t\tr.append((n, c.Name, c.ActCtrl, c.Pos, c.Ang, c.repos, len(c.CallbackFunc), "
	"c.__class__.__name__))\n\treturn r\nimport Init\nInit._res = _res": [
		"_res = lambda: (lambda R: map(lambda n: (lambda c: (n, c.Name, c.ActCtrl, c.Pos, c.Ang, c.repos, len(c.CallbackFunc), "
		"c.__class__.__name__))(R[n]), tc_sorted(R.keys())))(CharsNPC.CharRes)",
		"import Init",
		"Init._res = _res"],
	"def _npcs():\n\timport CharsNPC, Init\n\tk = CharsNPC.CharRes.keys()\n\tk.sort()\n\tr = []\n\tfor n in k:\n"
	"\t\tif Scrap.GetEntity(n): r.append((n, Init._ent(n, ('ActCtrl','Money','MoneyReward','Life','PrevLife','ArrOpt0',"
	"'ArrnOpt','UseFunc','ViewFunc','AI_Repos','ArrowCanUse','ArrowCanView','EntityList','Descriptor','LLogicIdle',"
	"'Invulnerable','MoveTime','MoveViewAng'))))\n\treturn r\nimport Init\nInit._npcs = _npcs": [
		"_npcs = lambda: map(lambda n: (n, Init._ent(n, ('ActCtrl','Money','MoneyReward','Life','PrevLife','ArrOpt0','ArrnOpt',"
		"'UseFunc','ViewFunc','AI_Repos','ArrowCanUse','ArrowCanView','EntityList','Descriptor','LLogicIdle','Invulnerable',"
		"'MoveTime','MoveViewAng'))), filter(lambda n: Scrap.GetEntity(n), tc_sorted(CharsNPC.CharRes.keys())))",
		"import Init",
		"Init._npcs = _npcs"],
	"def _sp(n):\n\timport Init\n\treturn (Init._mod('Speech', ('PlayerName','NPCName','AnsAnima','NPCAnswer','OnEndOfSpeech')), "
	"Init._mod('CharsNPC', ('optionsList','subOptionsList','ActualEntityName')), Init._ent(n, ('Action','ArrOpt0','ArrOpt1',"
	"'ArrOpt2','ArrOpt3','ArrOpt4','ArrnOpt','ArriOpt','LLogicIdle','SelectedEntity')), Init._mod('Challenge', "
	"('challengeType','foeType')))\nimport Init\nInit._sp = _sp": [
		"_sp = lambda n: (Init._mod('Speech', ('PlayerName','NPCName','AnsAnima','NPCAnswer','OnEndOfSpeech')), "
		"Init._mod('CharsNPC', ('optionsList','subOptionsList','ActualEntityName')), Init._ent(n, ('Action','ArrOpt0','ArrOpt1',"
		"'ArrOpt2','ArrOpt3','ArrOpt4','ArrnOpt','ArriOpt','LLogicIdle','SelectedEntity')), Init._mod('Challenge', "
		"('challengeType','foeType')))",
		"import Init",
		"Init._sp = _sp"],
	# fc-r8e-sched: scheduled calls that schedule more
	"import Init\nInit._r8 = []\ndef _r8rec(*a):\n\tInit._r8.append(a)\nInit._r8rec = _r8rec\ndef _r8sd(tag):\n"
	"\tInit._r8.append(('sd', tag, Scrap.DeleteScheduledFuncs('SD')))\nInit._r8sd = _r8sd\ndef _r8nest(tag):\n"
	"\tInit._r8.append(('nest', tag))\n\tScrap.AddScheduledFunc(0, Init._r8rec, ('fromnest', tag))\n"
	"\tScrap.AddScheduledFunc(0, Init._r8rec, ('fromnest2', tag))\nInit._r8nest = _r8nest": [
		"import Init",
		"Init._r8 = []",
		"_r8rec = lambda *a: Init._r8.append(a)",
		"Init._r8rec = _r8rec",
		"_r8sd = lambda tag: Init._r8.append(('sd', tag, Scrap.DeleteScheduledFuncs('SD')))",
		"Init._r8sd = _r8sd",
		"_r8nest = lambda tag: (Init._r8.append(('nest', tag)), Scrap.AddScheduledFunc(0, Init._r8rec, ('fromnest', tag)), "
		"Scrap.AddScheduledFunc(0, Init._r8rec, ('fromnest2', tag)), None)[3]",
		"Init._r8nest = _r8nest"],
	# fc-r8r-sg (in I.__dict__)
	"def _set(n, v):\n\timport Init\n\tif n in ('PlayVideo', 'ForceReload'):\n\t\tInit._rec.append(('Set', n, v))\n\telse:\n"
	"\t\tInit._os(n, v)\n": [
		"_set = lambda n, v: (Init._rec.append(('Set', n, v)) if n in ('PlayVideo', 'ForceReload') else Init._os(n, v), None)[1]"],
	"class _R:\n\ta = 1\n\tb = 'x'\n\tc = [1, 2.5, None]\n": [
		"_R = tc_class(__name__ + '._R', {'a': 1, 'b': 'x', 'c': [1, 2.5, None]})"],
}

# whole case lines that define a function, and their channel statements
LINES = {
	"def _fdh(): return _sdn((Parking.SwitchVehicleMissionCallback, Parking.SwitchCharMissionCallback, CharAct.TakenPhoto))":
		"_fdh = lambda: _sdn((Parking.SwitchVehicleMissionCallback, Parking.SwitchCharMissionCallback, CharAct.TakenPhoto))",
	"def _gk(): d = map(lambda x: (x[0], x[1].__class__.__name__, x[1].__dict__.items()), GK.GateKeepers.items()); d.sort(); "
	"return (d, GK.GKDoors, GK.LocalCallFunc, GK.LocalParams, GK.AfterGateKeeperSpeech)":
		"_gk = lambda: (lambda d: (d.sort(), (d, GK.GKDoors, GK.LocalCallFunc, GK.LocalParams, GK.AfterGateKeeperSpeech))[1])("
		"map(lambda x: (x[0], x[1].__class__.__name__, x[1].__dict__.items()), GK.GateKeepers.items()))",
}

# the module of "in <x>.__dict__"
_TARGET = re.compile(r"^(?:__import__\('(\w+)'\)|(\w+))\.__dict__$")
_ALIASES = {'I': 'Init'}


def split_statements(line):
	"""The top level statements of a line (split at ';' outside strings, brackets and backquotes)."""
	out, cur, depth, i, q, bq = [], '', 0, 0, None, False
	while i < len(line):
		c = line[i]
		if q:
			cur += c
			if c == '\\' and i + 1 < len(line):
				cur += line[i + 1]
				i += 1
			elif c == q:
				q = None
		elif c in '\'"':
			q = c
			cur += c
		elif c == '`':
			bq = not bq
			cur += c
		elif c in '([{':
			depth += 1
			cur += c
		elif c in ')]}':
			depth -= 1
			cur += c
		elif c == ';' and depth == 0 and not bq:
			out.append(cur.strip())
			cur = ''
		else:
			cur += c
		i += 1
	if cur.strip():
		out.append(cur.strip())
	return out


def read_tc(name, seen=None):
	"""The statements of channel/<name>.tc (comments dropped, indented lines joined to the one before, "include x"
	expanded)."""
	seen = seen if seen is not None else set()
	path = os.path.join(CHANNEL, name + '.tc')
	if not os.path.isfile(path):
		raise Unconvertible('no channel twin channel/%s.tc of the helper %s.py' % (name, name))
	out = []
	for raw in open(path, encoding='latin1').read().split('\n'):
		if not raw.strip() or raw.lstrip().startswith('#'):
			continue
		if raw[0] in ' \t' and out:
			out[-1] = out[-1] + ' ' + raw.strip()
			continue
		if raw.startswith('include '):
			out += read_tc(raw.split()[1], seen)
			continue
		out.append(raw.rstrip())
	return out


def _module(target):
	if target is None:
		return None
	m = _TARGET.match(target.strip())
	if not m:
		raise Unconvertible('exec in %s' % target)
	name = m.group(1) or m.group(2)
	return _ALIASES.get(name, name)


_EXECFILE = re.compile(r"^execfile\('(\w+)\.py'\)$")
_EXECOPEN = re.compile(r"^exec\s*\(?\s*open\('(\w+)\.py'\)\.read\(\)\s*\)?(?:\s+in\s+(.+))?$")
_EXECSTR = re.compile(r'^exec\s+"((?:[^"\\]|\\.)*)"(?:\s+in\s+(.+))?$')


def _prelude(stmts, module):
	if module and module != '__main__':
		return ['in %s do %s' % (module, s) for s in stmts]
	return ['do ' + s for s in stmts]


# nodes of Python's grammar the channel does not read (TestExpr.c)
_UNSUPPORTED = {
	'FunctionDef': 'a def', 'ClassDef': 'a class', 'For': 'a for loop', 'While': 'a while loop', 'If': 'an if statement',
	'Try': 'a try statement', 'With': 'a with statement', 'Global': 'a global statement', 'Return': 'a return',
	'Raise': 'a raise statement', 'Assert': 'an assert', 'AugAssign': 'an augmented assignment',
	'ListComp': 'a list comprehension', 'GeneratorExp': 'a generator', 'DictComp': 'a comprehension',
	'SetComp': 'a comprehension', 'Starred': 'a starred argument', 'ImportFrom': 'from ... import', 'Set': 'a set',
}
_PYTHON_ONLY = {'execfile': 'execfile (runs Python code)', 'eval': 'eval (compiles Python code)',
				'compile': 'compile (Python code)', 'exec': 'exec (runs Python code)',
				'load_source': 'imp.load_source (compiles a .py)', 'func_code': 'a code object'}


def check_syntax(stmt):
	"""Unconvertible when the statement needs more than the channel reads."""
	if stmt.startswith('print ') or stmt == 'print':
		raise Unconvertible('a print statement')
	text = re.sub(r'`([^`]*)`', r'repr(\1)', stmt).replace('<>', '!=')
	try:
		tree = ast.parse(text)
	except SyntaxError as e:
		raise Unconvertible('Python syntax the channel does not read: %s' % e.msg)
	for n in ast.walk(tree):
		k = type(n).__name__
		if k in _UNSUPPORTED:
			raise Unconvertible(_UNSUPPORTED[k])
		if isinstance(n, ast.Call):
			if any(isinstance(a, ast.Starred) for a in n.args) or any(kw.arg is None for kw in n.keywords):
				raise Unconvertible('a call with *args or **kw')
		if isinstance(n, ast.Name) and n.id in _PYTHON_ONLY:
			raise Unconvertible(_PYTHON_ONLY[n.id])
		if isinstance(n, ast.Attribute) and n.attr in _PYTHON_ONLY:
			raise Unconvertible(_PYTHON_ONLY[n.attr])


def convert(line):
	"""The channel commands of a case line (the last one answers); Unconvertible with the reason."""
	if line in LINES:
		return [LINES[line]]
	stmts = split_statements(line)
	prelude, main = [], []
	exec_mode = len(stmts) > 1
	only_execfile = False
	for s in stmts:
		m = _EXECFILE.match(s)
		if m:
			prelude += _prelude(read_tc(m.group(1)), None)
			only_execfile = len(stmts) == 1
			continue
		m = _EXECOPEN.match(s)
		if m:
			prelude += _prelude(read_tc(m.group(1)), _module(m.group(2)))
			exec_mode = True
			continue
		m = _EXECSTR.match(s)
		if m:
			code = ast.literal_eval('"%s"' % m.group(1))
			if code not in EXEC:
				raise Unconvertible('exec of Python code with no channel twin: %r' % code[:60])
			prelude += _prelude(EXEC[code], _module(m.group(2)))
			exec_mode = True
			continue
		check_syntax(s)
		main.append(s)
	if not main:
		final = 'None' if only_execfile else 'pass'
	elif exec_mode and len(main) == 1:
		# a line of several statements answers with result or ok, also when one statement is left
		final = main[0] + '; pass'
	else:
		final = '; '.join(main)
	return prelude + [final]


def helper_load(name):
	"""The commands that load channel/<name>.tc into __main__ (the runner's execfile('fst.py'))."""
	return _prelude(read_tc(name), None)


def main():
	sys.path.insert(0, HERE)
	import regress
	args = sys.argv[1:]
	stats = '--stats' in args
	args = [a for a in args if a != '--stats']
	cases = regress.pick(regress.read_cases(), args)
	total = bad = 0
	reasons = {}
	for level, case in cases:
		for line in open(regress.case_path(case), encoding='latin1').read().split('\n'):
			if not line.strip() or line.startswith('#') or line.startswith('!sleep '):
				continue
			total += 1
			try:
				cmds = convert(line)
				if not stats:
					print('>> ' + line[:200])
					for c in cmds:
						print('   ' + c[:200])
			except Unconvertible as e:
				bad += 1
				reasons.setdefault(str(e), []).append(case)
				if stats:
					print('%s: %s\n    %s' % (case, e, line[:160]))
	if stats:
		print('%d of %d lines do not convert' % (bad, total))
		for r, cs in sorted(reasons.items(), key=lambda x: -len(x[1])):
			print('%5d %s (%s)' % (len(cs), r, ' '.join(sorted(set(cs)))[:300]))


if __name__ == '__main__':
	main()
