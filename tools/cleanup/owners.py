# the owner file of every object of the data files
import re, glob, os, sys, collections
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import datalib as d, users as u

TOK = re.compile(r'[A-Za-z_]\w*')


def refs_of(o, names):
    if '=' not in o['text']:
        return set()
    return set(t[3:] if t.startswith('sd_') else t for t in TOK.findall(o['text'].split('=', 1)[1])) & names


def owners():
    objs = d.objects()
    names = set(objs)
    us = u.users(objs)
    si = open(d.ROOT + 'src/game/Global/StaticInit.inc').read()
    cfiles = [f for f in glob.glob(d.ROOT + 'src/**/*.c', recursive=True) if f not in d.DATA and '/core/test' not in f]
    ctext = {f: d.strip_comments(open(f, errors='replace').read()) for f in cfiles}
    hdr_alias = collections.defaultdict(set)	# object -> macro / inline function names of headers that use it
    for h in glob.glob(d.ROOT + 'include/scrap/*.h') + glob.glob(d.ROOT + 'src/**/*.h', recursive=True):
        s = d.strip_comments(open(h, errors='replace').read())
        for m in re.finditer(r'^#define (\w+)[^\n]*(?:\\\n[^\n]*)*', s, re.M):
            for t in set(TOK.findall(m.group(0))) & names:
                hdr_alias[t].add(m.group(1))
        for a, b, t in d.top_decls(s):
            fm = re.match(r'\s*static\s+inline\s+[^(]*?\b(\w+)\s*\(', t)
            if fm:
                for x in set(TOK.findall(t)) & names:
                    hdr_alias[x].add(fm.group(1))
    own, why = {}, {}
    for n in objs:
        cs = {f: k for f, k in us.get(n, {}).items() if f.endswith('.c')}
        if not cs and hdr_alias.get(n):
            al = hdr_alias[n]
            for f, s in ctext.items():
                k = sum(len(re.findall(r'\b%s\b' % a, s)) for a in al)
                if k:
                    cs[f] = k
            if cs:
                why[n] = 'header'
        if cs:
            own[n] = max(sorted(cs), key=lambda f: cs[f])
            why.setdefault(n, 'c')
        elif re.search(r'\b%s\b' % n, si):
            own[n] = d.ROOT + 'src/game/Global/StaticInit.c'
            why[n] = 'staticinit'
    # the objects only data points at follow the first object that points at them
    refby = collections.defaultdict(list)
    for n, o in objs.items():
        for r in refs_of(o, names):
            refby[r].append(n)
    changed = True
    while changed:
        changed = False
        for n in objs:
            if n in own:
                continue
            for r in refby.get(n, ()):
                if r in own:
                    own[n] = own[r]
                    why[n] = 'data'
                    changed = True
                    break
    return objs, own, why, us


if __name__ == '__main__':
    objs, own, why, us = owners()
    print(collections.Counter(why.values()), len(objs) - len(own), 'without owner')
    print([n for n in objs if n not in own][:20])
    c = collections.Counter(d.rel(f) for f in own.values())
    print(c.most_common(15))
