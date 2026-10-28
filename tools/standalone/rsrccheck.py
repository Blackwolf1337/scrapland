#!/usr/bin/env python3
# rsrccheck.py <Scrap.exe> <ScrapC.exe>: compares the resources of ScrapC.exe (src/standalone/scrap.rc, built by
# windres) with the ones of Scrap.exe 1.6.1, resource by resource by type, id and language (docs/standalone.md, stage
# 3b3). The bytes must be the same, but for a dialog template, where windres encodes the same template otherwise:
# then the parsed templates must match, with the class names compared without case (windres writes them in capitals,
# window classes are found without case) and the zero padding after the last control left out. Run by the build after
# the link of ScrapC.exe.
import os, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from rsrc import RT_DIALOG, RsrcError, parse_dialog, resources
from sdexe import Exe, ExeError


def dialog_key(d):
	d = dict(d, tail=d['tail'].rstrip(b'\0'), controls=[dict(c, cls=c['cls'].lower() if isinstance(c['cls'], str)
		else c['cls']) for c in d['controls']])
	return d


def main():
	if len(sys.argv) != 3:
		sys.exit('usage: rsrccheck.py <Scrap.exe> <ScrapC.exe>')
	try:
		orig, own = resources(Exe(sys.argv[1])), resources(Exe(sys.argv[2]))
	except (ExeError, RsrcError) as e:
		sys.exit('rsrccheck.py: %s' % e)
	bad = 0
	same = parsed = 0
	for key in sorted(set(orig) | set(own), key=lambda k: tuple(str(v) for v in k)):
		what = 'type %s id %s lang %04x' % key
		a, b = orig.get(key), own.get(key)
		if a is None or b is None:
			print('rsrccheck.py: %s only in %s' % (what, 'ScrapC.exe' if a is None else 'Scrap.exe'))
			bad += 1
		elif a == b:
			same += 1
		elif key[0] == RT_DIALOG:
			try:
				ok = dialog_key(parse_dialog(a, what)) == dialog_key(parse_dialog(b, what))
			except RsrcError as e:
				print('rsrccheck.py: %s' % e)
				ok = False
			if ok:
				parsed += 1
			else:
				print('rsrccheck.py: dialog %s differs' % what)
				bad += 1
		else:
			n = min(len(a), len(b))
			at = next((i for i in range(n) if a[i] != b[i]), n)
			print('rsrccheck.py: %s differs (%d bytes, ScrapC.exe %d, first at +%x)' % (what, len(a), len(b), at))
			bad += 1
	print('rsrccheck.py: %d resources match, %d of them as parsed dialog templates, %d differ'
		% (same + parsed, parsed, bad))
	if bad:
		sys.exit(1)


main()
