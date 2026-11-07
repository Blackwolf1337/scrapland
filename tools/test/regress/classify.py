# classify.py RESULTDIR [RESULTDIR ...]: the lines that fail in the first result dir, with how the C runs of all the
# dirs and the pack runs behave: "C varies" (the C reply differs between C runs: random or timing in C),
# "pack varies" (C stable, the pack runs vary), "STABLE DIFF" (pack and C each stable, and different: a candidate
# port difference to look at). Triage aid for masks/.
import sys, os, json, glob
import regress
from compare import compare
from normalise import norm
dirs = sys.argv[1:]
for p in sorted(glob.glob(os.path.join(dirs[0], '*.json'))):
	c = os.path.basename(p)[:-5]
	packs = regress.pack_runs(c)
	if len(packs) < 2:
		continue
	cruns = [json.load(open(os.path.join(d, c + '.json'))) for d in dirs if os.path.isfile(os.path.join(d, c + '.json'))]
	ok, diffs, masked = compare(c, [x['blocks'] for x in packs], cruns[0]['blocks'])
	for i, cmd, exp, got in diffs:
		cv = set(norm(r['blocks'][i][1]) if i < len(r['blocks']) else '<none>' for r in cruns)
		pv = set(norm(x['blocks'][i][1]) for x in packs if i < len(x['blocks']))
		kind = 'C varies' if len(cv) > 1 else 'pack varies' if len(pv) > 1 else 'STABLE DIFF'
		print('%-12s %s line %d (%d C runs, %d pack runs): %s' % (kind, c, i + 1, len(cruns), len(packs), cmd[:90]))
