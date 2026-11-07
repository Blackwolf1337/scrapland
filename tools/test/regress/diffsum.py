# diffsum.py RESULTDIR [case ...]: per failing case each differing line, cut around its first difference (triage aid)
import sys, os, json, glob
import regress
from compare import compare
rd = sys.argv[1]
names = sys.argv[2:] or [os.path.basename(p)[:-5] for p in sorted(glob.glob(os.path.join(rd, '*.json')))]
for c in names:
	cres = json.load(open(os.path.join(rd, c + '.json')))
	packs = regress.pack_runs(c)
	if len(packs) < 2:
		continue
	ok, diffs, masked = compare(c, [p['blocks'] for p in packs], cres['blocks'])
	if ok:
		continue
	print('%s (%d pack runs, C %s)' % (c, len(packs), cres['status']))
	for i, cmd, exp, got in diffs:
		e = exp.split(' | ')[0]
		k = 0
		while k < min(len(e), len(got)) and e[k] == got[k]:
			k += 1
		print('  %d: %s' % (i + 1, cmd[:100]))
		print('     P: %s' % e[max(0, k - 70):k + 90].replace('\n', ' '))
		print('     C: %s' % got[max(0, k - 70):k + 90].replace('\n', ' '))
