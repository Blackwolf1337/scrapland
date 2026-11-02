# the users of every object of the data files: name -> {file: count} (StaticInit.inc and the test maps not counted)
import re, glob, os, sys, collections
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import datalib as d


def users(objs):
    files = [f for f in glob.glob(d.ROOT + 'src/**/*.[ch]', recursive=True) + glob.glob(d.ROOT + 'include/scrap/*.h')
             if f not in d.DATA and '/core/test' not in f and '/third_party/' not in f]
    names = set(objs)
    out = collections.defaultdict(dict)
    tok = re.compile(r'[A-Za-z_]\w*')
    for f in files:
        s = d.strip_comments(open(f, errors='replace').read())
        s = re.sub(r'"(\\.|[^"\\\n])*"', '""', s)
        c = collections.Counter(t for t in tok.findall(s) if t in names)
        for n, k in c.items():
            out[n][f] = k
    return out
