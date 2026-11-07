# probe.py <tag> <level> <case> <pack 0/1> [build]: one run of a case on one slot, prints the replies (debug aid)
import sys
sys.argv += [None] * 2
import regress
tag, level, case, pack, build = sys.argv[1:6]
res = regress.Lane(tag, pack == '1').run(level, case, build or regress.DEFAULT_BUILD)
print(res['status'], res.get('error', ''))
for c, r in res['blocks']:
	print('>> ' + c[:160]); print(r)
