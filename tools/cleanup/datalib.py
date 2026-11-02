# Helpers of the data move (tools/cleanup, removed at the end): the objects of the generated data files
# src/game/Global/Globals*.c and Strings*.c, their declarations, and an "undeclared" fixer.
import os, re, glob, sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')) + '/'
DATA = sorted(glob.glob(ROOT + 'src/game/Global/Globals*.c') + glob.glob(ROOT + 'src/game/Global/Strings*.c'))


def rel(p):
    return p.replace(ROOT, '')


def strip_comments(s):
    return re.sub(r'/\*.*?\*/', lambda m: ' ' * len(m.group(0)), s, flags=re.S)


def top_decls(s):
    """(start, end, text) of each top-level declaration of a C file (ends at ';' at brace depth 0)"""
    c = strip_comments(s)
    out, i, n = [], 0, len(c)
    start = None
    depth = 0
    k = 0
    while k < n:
        ch = c[k]
        if start is None:
            if ch.isspace():
                k += 1
                continue
            if ch == '#':
                e = c.find('\n', k)
                while e > 0 and c[e - 1] == '\\':
                    e = c.find('\n', e + 1)
                k = n if e < 0 else e + 1
                continue
            start = k
        if ch == '"' or ch == "'":
            q = ch
            k += 1
            while k < n and c[k] != q:
                k += 2 if c[k] == '\\' else 1
        elif ch == '{':
            depth += 1
        elif ch == '}':
            depth -= 1
        elif ch == ';' and depth == 0:
            out.append((start, k + 1, s[start:k + 1]))
            start = None
        k += 1
    return out


NAME_RE = re.compile(r'([A-Za-z_]\w*)\s*(\[[^\]]*\]\s*)*(=|;|$)')


def decl_name(text):
    """the declared name of 'T name[..] = ...;'"""
    head = text.split('=', 1)[0] if '=' in text else text.rstrip(';')
    head = head.strip()
    m = re.search(r'([A-Za-z_]\w*)\s*((\[[^\]]*\]\s*)*)$', head)
    return (m.group(1), head) if m else (None, head)


def objects():
    """name -> dict(file, start, end, text, head) of every object defined in the data files"""
    out = {}
    for f in DATA:
        s = open(f).read()
        for a, b, t in top_decls(s):
            if t.lstrip().startswith(('typedef', 'extern', 'struct ', 'enum ', 'union ')) and '=' not in t and not re.search(r'\}\s*\w', t):
                continue
            name, head = decl_name(t)
            if not name:
                continue
            out[name] = dict(file=f, start=a, end=b, text=t, head=head)
    return out


def extern_of(o):
    """the extern declaration of an object: its head without static, with 'extern', array sizes kept"""
    h = re.sub(r'^\s*(static\s+)', '', o['head'])
    h = re.sub(r'\[\s*\]', '[]', h)
    return 'extern ' + h.strip() + ';'
