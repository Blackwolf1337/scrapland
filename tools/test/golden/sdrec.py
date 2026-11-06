# The recorder that made sdtest.cmd from sdtest.py (Python 1.5.2, run in the game; mkcmd.py runs it). It wraps the
# functions of the engine modules and replaces check(): each check writes its name, the kind and the engine calls its
# lambda made. kind "call": the lambda made exactly one call and gave its result (or its exception) unchanged, so the
# check is that call with the literal arguments; "special": anything else (mkcmd.py has a line for each of them).
# Goes with the interpreter (stage 7 of the removal of Python): sdtest.cmd is the source of the API checks then.
recCalls = []
recDepth = [0]

class RecWrap:
	def __init__(self, mod, name, f):
		self.mod = mod
		self.name = name
		self.f = f
	def __call__(self, *a, **kw):
		rec = [self.mod, self.name, a, kw, None, None]
		if recDepth[0] == 0:
			recCalls.append(rec)
		recDepth[0] = recDepth[0] + 1
		try:
			try:
				r = apply(self.f, a, kw)
			except:
				rec[5] = (sys.exc_type, sys.exc_value)
				raise sys.exc_type, sys.exc_value, sys.exc_traceback
		finally:
			recDepth[0] = recDepth[0] - 1
		rec[4] = r
		return r

for recMod in (Scrap, SWeap, SVec, SAct, SFX, SLogic, SSound, SInput, SAI, SNet, SScorer):
	for recName in dir(recMod):
		recF = getattr(recMod, recName)
		if type(recF) == type(len):
			setattr(recMod, recName, RecWrap(recMod.__name__, recName, recF))

def recText(rec):
	r = ''
	for x in rec[2]:
		if r:
			r = r + ', '
		r = r + repr(x)
	for k, v in rec[3].items():
		if r:
			r = r + ', '
		r = r + k + '=' + repr(v)
	return '%s.%s(%s)' % (rec[0], rec[1], r)

def check(name, fn):
	global recCalls
	recCalls = []
	exc = None
	r = None
	try:
		r = fn()
	except:
		exc = sys.exc_value
	calls = recCalls
	recCalls = []
	kind = 'special'
	if len(calls) == 1:
		c = calls[0]
		if exc is None and c[5] is None and c[4] is r:
			kind = 'call'
		elif exc is not None and c[5] is not None and c[5][1] is exc:
			kind = 'call'
	texts = map(recText, calls)
	out.write('%s\t%s\t%s\n' % (name, kind, strop.join(texts, '\t')))
	out.flush()
