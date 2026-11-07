import marshal, string
def _r6cmp(path):
	src = string.replace(open(path + '.py', 'rb').read(), '\r\n', '\n')
	a = compile(src + '\n', path + '.py', 'exec')
	b = marshal.loads(open(path + '.pyc', 'rb').read()[8:])
	def funcs(co):
		r = {}
		for c in co.co_consts:
			if type(c) == type(co):
				r[c.co_name] = c
		return r
	fa = funcs(a)
	fb = funcs(b)
	bad = []
	for n in fb.keys():
		if not fa.has_key(n):
			bad.append((n, 'missing in py'))
		elif fa[n].co_code != fb[n].co_code or fa[n].co_names != fb[n].co_names or fa[n].co_consts != fb[n].co_consts:
			bad.append((n, 'differs'))
	for n in fa.keys():
		if not fb.has_key(n):
			bad.append((n, 'missing in pyc'))
	return (string.split(path, '/')[-1], len(fb), bad, a.co_code == b.co_code)
