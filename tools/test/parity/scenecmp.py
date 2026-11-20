#!/usr/bin/env python3
"""scenecmp.py <ref dir> <dir> [<dir> ...] [--mad M] [--bad B] [--sheet out.png]: the scene screenshots
<scene>-<k>.pcx of each dir against those of the reference dir (single player) at the same scene time. Per shot: the
mean absolute difference of the RGB values at 320x180 (0..255) and the share of pixels differing by more than 48 in
one channel. A shot passes when mad <= M and bad <= B; a shot of the reference missing in a dir fails. Exit 1 on a
failure. --sheet writes the shots side by side (reference first)."""
import os, sys, glob, argparse
from PIL import Image, ImageChops, ImageStat

def load(p):
    return Image.open(p).convert('RGB').resize((320, 180), Image.BILINEAR)

def compare(a, b):
    d = ImageChops.difference(a, b)
    mad = sum(ImageStat.Stat(d).mean) / 3.0
    px = d.get_flattened_data() if hasattr(d, 'get_flattened_data') else d.getdata()
    bad = sum(1 for p in px if max(p) > 48) / float(len(px))
    return mad, bad

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('ref')
    ap.add_argument('dirs', nargs='+')
    ap.add_argument('--mad', type=float, default=12.0)
    ap.add_argument('--bad', type=float, default=0.12)
    ap.add_argument('--sheet')
    a = ap.parse_args()
    refs = sorted(glob.glob(os.path.join(a.ref, '*.pcx')))
    fails = 0
    rows = []
    for r in refs:
        name = os.path.basename(r)
        ra = load(r)
        row = [ra]
        cells = []
        for d in a.dirs:
            p = os.path.join(d, name)
            if not os.path.exists(p):
                cells.append('missing')
                fails += 1
                row.append(Image.new('RGB', (320, 180), (255, 0, 255)))
                continue
            b = load(p)
            row.append(b)
            mad, bad = compare(ra, b)
            ok = mad <= a.mad and bad <= a.bad
            fails += not ok
            cells.append('%s mad %.1f bad %.3f' % ('ok' if ok else 'FAIL', mad, bad))
        rows.append(row)
        print('%-28s %s' % (name[:-4], '  |  '.join(cells)))
    if not refs:
        print('no reference shots in %s' % a.ref)
        fails += 1
    if a.sheet and rows:
        w = 320 * len(rows[0])
        sheet = Image.new('RGB', (w, 180 * len(rows)))
        for y, row in enumerate(rows):
            for x, im in enumerate(row):
                sheet.paste(im, (320 * x, 180 * y))
        sheet.save(a.sheet)
    print('%d shots, %d failed (mad <= %.1f, bad <= %.3f)' % (len(refs), fails, a.mad, a.bad))
    sys.exit(1 if fails else 0)

main()
