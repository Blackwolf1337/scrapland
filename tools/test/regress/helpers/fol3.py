# OBJLIB 7, stage 3 of the object library removal (docs/objlib-plan.md): the pass-through recorder of the port
# functions that PORT_CALL and PORT_OWN sites call. _pton(names) wraps each 'Module.Func' of names in a recorder that
# calls the function that was there with the same arguments and logs (name, arguments, result) with the names of
# _sdn ('error' for the result when the call raised; the exception goes on); modules the level has not loaded are
# skipped, so a case calls it after its level loads. _ptlog() gives the log and empties it, _ptoff() puts the
# functions back. A wrapped function is not the port's own, so the sites take their slow path: a base build and a new
# build log the same calls when the converted sites pass the same arguments (tools/script/portcalls.py wrap lists the
# functions of the sites of some files).
import sys
exec(open('fsd.py').read())
_ptr = []
_ptorig = []
class _PtRec:
	def __init__(self, n, f):
		self.n = n
		self.f = f
	def __call__(self, *a):
		box = [None, 0]
		try:
			box[0] = apply(self.f, a)
			box[1] = 1
		finally:
			if box[1]:
				_ptr.append((self.n, _sdn(a), _sdn(box[0])))
			else:
				_ptr.append((self.n, _sdn(a), 'error'))
		return box[0]
def _pton(names):
	import string
	for n in names:
		m, f = string.split(n, '.')
		if sys.modules.has_key(m):
			mod = sys.modules[m]
			_ptorig.append((mod, f, getattr(mod, f)))
			setattr(mod, f, _PtRec(n, getattr(mod, f)))
	return 1
def _ptlog():
	global _ptr
	r = _ptr
	_ptr = []
	return r
def _ptoff():
	while _ptorig:
		mod, f, v = _ptorig[-1]
		del _ptorig[-1]
		setattr(mod, f, v)
	return 1
