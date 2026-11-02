# Helpers of the typed-vtable conversion (tools/cleanup, removed at the end of the phase): the vtables of
# src/game/Global/Vtables.c, the struct definitions of the headers and sources, and the C function definitions.
import os, re, glob

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')) + '/'
VTABLES = ROOT + 'src/game/Global/Vtables.c'


def rel(p):
    return p.replace(ROOT, '')


def strip_comments(s):
    return re.sub(r'/\*.*?\*/', lambda m: ' ' * len(m.group(0)), s, flags=re.S)


def sources(headers=True):
    fs = glob.glob(ROOT + 'src/**/*.c', recursive=True)
    if headers:
        fs += glob.glob(ROOT + 'include/scrap/*.h') + glob.glob(ROOT + 'src/**/*.h', recursive=True)
    return [f for f in fs if '/third_party/' not in f]


def vtables():
    """name -> (count, [(offset, function)], start, end) of each vtable array in Vtables.c"""
    s = open(VTABLES).read()
    out = {}
    for m in re.finditer(r'void \(\*const (\w+)\[(\d+)\]\)\(void\) = \{(.*?)\n\};\n', s, re.S):
        slots = [(int(x.group(2), 16), x.group(1)) for x in re.finditer(r'^\s*(\w+),\s*/\*\s*(0x[0-9a-f]+)', m.group(3), re.M)]
        out[m.group(1)] = (int(m.group(2)), slots, m.start(), m.end())
    return out


def split_params(p):
    p = p.strip()
    if p in ('', 'void'):
        return []
    out, depth, cur = [], 0, ''
    for ch in p:
        if ch == '(':
            depth += 1
        elif ch == ')':
            depth -= 1
        if ch == ',' and depth == 0:
            out.append(cur.strip())
            cur = ''
        else:
            cur += ch
    out.append(cur.strip())
    return out


DEF_RE = re.compile(r'^([A-Za-z_][\w \t\*]*?)\b(\w+)\(([^;{)]*(?:\([^)]*\)[^;{)]*)*)\)\s*\{', re.M)


def definitions():
    """function name -> (file, offset of the definition, return text, [param texts])"""
    out = {}
    for f in sources(headers=False):
        raw = open(f, errors='replace').read()
        s = strip_comments(raw)
        for m in DEF_RE.finditer(s):
            name = m.group(2)
            if name in ('if', 'while', 'for', 'switch', 'return', 'sizeof'):
                continue
            if 'static' in m.group(1).split() and name in out:
                continue
            out.setdefault(name, (f, m.start(), m.group(1).strip(), split_params(m.group(3))))
    return out


def structs():
    """struct name -> body text (typedef struct X {...} X; and struct X {...};)"""
    out = {}
    for f in sources():
        s = strip_comments(open(f, errors='replace').read())
        for m in re.finditer(r'(?:typedef\s+)?struct\s+(\w+)\s*\{', s):
            i = m.end()
            depth = 1
            while depth and i < len(s):
                if s[i] == '{':
                    depth += 1
                elif s[i] == '}':
                    depth -= 1
                i += 1
            out.setdefault(m.group(1), s[m.end():i - 1])
    # typedef aliases of struct types (typedef cGenCharLocalLogic cCrazyDealerLocalLogic;): the body of the struct
    for f in sources():
        s = strip_comments(open(f, errors='replace').read())
        for m in re.finditer(r'^typedef\s+(?:struct\s+)?(\w+)\s+(\w+)\s*;', s, re.M):
            if m.group(1) in out and m.group(2) not in out:
                out[m.group(2)] = out[m.group(1)]
    return out


def first_members(body):
    """the members a pointer to the struct also points to: the first member, or all members of a first anonymous
    union (and the first members of anonymous structs in it): [(type, name)]"""
    b = body.strip()
    mu = re.match(r'union\s*\{', b)
    if mu:
        i, depth = mu.end(), 1
        while depth:
            if b[i] == '{':
                depth += 1
            elif b[i] == '}':
                depth -= 1
            i += 1
        inner = b[mu.end():i - 1]
        res = []
        for d in re.finditer(r'\s*(struct\s*\{[^}]*\}|[^;]+);', inner):
            t = d.group(1).strip()
            if t.startswith('struct') and '{' in t:
                res += first_members(t[t.index('{') + 1:t.rindex('}')])
            else:
                mm = re.match(r'(?:const\s+)?(?:struct\s+)?(\w+)\s+(\w+)$', t)
                if mm:
                    res.append((mm.group(1), mm.group(2)))
        return res
    mm = re.match(r'(?:const\s+)?(?:struct\s+)?(\w+)\s+(\w+)\s*;', b)
    return [(mm.group(1), mm.group(2))] if mm else []


def upcast_path(st, derived, base, depth=0):
    """the member path from struct derived to its base struct at offset 0 ('base', 'wl.obj.base'), None if none"""
    if derived == base:
        return ''
    if depth > 8 or derived not in st:
        return None
    for t, n in first_members(st[derived]):
        if t == base:
            return n
        p = upcast_path(st, t, base, depth + 1)
        if p is not None:
            return n + ('.' + p if p else '')
    return None
