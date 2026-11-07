# helpers of the round 8r cases (coverage): menu dumps like the MCP menu_dump (name, text, position, size, visible,
# accept, links; numbers rounded), a time-speed-safe call that returns the error text, a call with its scheduled calls
# recorded. Load: exec(open('<rd>/cov/fcov.py').read()) in __import__('Init').__dict__
import Scrap, sys
def _md(player=0):
	import SScorer
	items = SScorer.Items(player) or []
	r = []
	for n in items:
		row = [n]
		for v in ('Text', 'X', 'Y', 'W', 'H', 'Visible', 'OnAccept', 'Up', 'Down', 'Left', 'Right'):
			x = SScorer.Get(player, n, v)
			if type(x) == type(1.0):
				x = round(x, 1)
			row.append(x)
		r.append(tuple(row))
	return (SScorer.GetActual(player), r)
def _ce(f, *a, **kw):
	try:
		try:
			return ('ok', apply(f, a, kw))
		except:
			return ('error', str(sys.exc_type), str(sys.exc_value))
	finally:
		Scrap.SetTimeSpeed(1)
def _cs(f, *a, **kw):
	rec = []
	old = Scrap.AddScheduledFunc
	def add(*b):
		rec.append(b[1:])
		return apply(old, b)
	Scrap.AddScheduledFunc = add
	try:
		r = apply(_ce, (f,) + a, kw)
	finally:
		Scrap.AddScheduledFunc = old
	return (r, map(lambda b: tuple(map(lambda x: getattr(x, '__name__', x), b)), rec))
