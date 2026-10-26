# Inputs for the math checks of sdcall.py (Python 1.5.2): a small linear congruential generator, so both runs see
# the same numbers, and special values. execfile'd by sdcall.py.
_seed = [12345]
def rnd(lo=-10.0, hi=10.0):
	_seed[0] = (_seed[0] * 3877 + 29573) % 139968
	return lo + (hi - lo) * _seed[0] / 139968.0
inf = 1e300 * 1e300
nan = inf - inf
specials = (0.0, -0.0, 1.0, -1.0, 1e-30, 1e30, inf, -inf, nan)
def vec():
	return (rnd(), rnd(), rnd())
def vecs(n):
	r = []
	for i in range(n):
		r.append(vec())
	return r
