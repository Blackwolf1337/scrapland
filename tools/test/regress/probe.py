# probe.py <tag> <level> <case> [build]: one run of a case on one slot, prints the replies (debug aid)
import sys
sys.argv += [None]
import regress
tag, level, case, build = sys.argv[1:5]
build = build or regress.DEFAULT_BUILD
res = regress.Lane(tag).run(level, case, build)
print(res['status'], res.get('error', ''))
for c, r in res['blocks']:
	print('>> ' + c[:160]); print(r)
