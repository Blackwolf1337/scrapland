# holders.py (Python 1.5.2, run in the game): the Python functions still reachable from the loaded modules while the
# module global of their name is a C function (a holder the loader did not reach). _holders() returns the sorted list
# of 'path -> module.func' lines; walks module dicts, lists, tuples, dicts, classes and instances up to depth 5.
# Python 1.5.2: no types module, no nested scopes (state goes in the argument st).
import sys
class _HC:
	pass
_HT = (type(_HC), type(_HC()), type([]), type(()), type({}), type(len))
def _hwalk(o, path, depth, st):
	if depth > 5 or st[0].has_key(id(o)):
		return
	t = type(o)
	if t is st[2]:
		n = o.__name__
		g = o.func_globals
		if g.has_key(n) and g[n] is not o and type(g[n]) is _HT[5]:
			st[1].append('%s -> %s.%s' % (path, g.get('__name__', '?'), n))
		return
	st[0][id(o)] = o
	if t is _HT[2] or t is _HT[3]:
		for i in range(len(o)):
			_hwalk(o[i], '%s[%d]' % (path, i), depth + 1, st)
	elif t is _HT[4]:
		for k, v in o.items():
			_hwalk(v, '%s[%s]' % (path, repr(k)[:30]), depth + 1, st)
	elif t is _HT[0] or t is _HT[1]:
		for k, v in o.__dict__.items():
			_hwalk(v, '%s.%s' % (path, k), depth + 1, st)
def _holders():
	st = ({}, [], type(_hwalk))
	for mn, m in sys.modules.items():
		if m is None:
			continue
		for k, v in m.__dict__.items():
			_hwalk(v, mn + '.' + k, 0, st)
	st[1].sort()
	return st[1]
