#!/usr/bin/env python3
# The typed-vtable conversion (tools/cleanup, removed at the end of the phase).
#
#   vtgen.py emit <Struct> <vtable> ...   each vtable of Vtables.c becomes "const <Struct> <vtable> = { .Field = fn };"
#                                          at the end of its owner file (where its CX_VFTABLE macro or its symbol is
#                                          used, the one with the class constructor first), the macro points at it
#                                          (at the base part when <Struct> embeds the family struct), the array
#                                          leaves Vtables.c
#   vtgen.py fields <Struct>              the slot fields of a struct in slot order (with the embedded bases)
import re, sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vtlib as v

FP_RE = re.compile(r'\(\s*\*\s*(\w+)\s*\)\s*\(')


def fields(st, name, prefix=''):
    """[(designator, field)] in slot order; a first member '<X>Vtbl base;' is flattened"""
    body = st[name]
    out = []
    first = True
    for decl in re.finditer(r'[^;{}]+;', body):
        d = decl.group(0).strip()
        mb = re.match(r'(?:const\s+)?(\w+Vtbl\w*)\s+(\w+)\s*;$', d)
        if first and mb:
            out += fields(st, mb.group(1), prefix + mb.group(2) + '.')
            first = False
            continue
        first = False
        m = FP_RE.search(d)
        if m:
            out.append((prefix + m.group(1), m.group(1)))
        elif re.match(r'(?:const\s+)?void\s*\*\s*(\w+)\s*;$', d):
            out.append((prefix + re.match(r'(?:const\s+)?void\s*\*\s*(\w+)', d).group(1), None))
    return out


def base_path(st, name):
    """the designator path down to the innermost embedded Vtbl ('base.base'), '' without one"""
    body = st[name].strip()
    mb = re.match(r'(?:const\s+)?(\w+Vtbl\w*)\s+(\w+)\s*;', body)
    if not mb:
        return ''
    inner = base_path(st, mb.group(1))
    return mb.group(2) + ('.' + inner if inner else '')


def owner(vt, macro):
    """the file to hold the vtable: users of the macro or the symbol, the one with the constructor first"""
    cls = vt[:-len('_vftable')]
    users = []
    for f in v.sources(headers=False):
        if f == v.VTABLES or '/core/test' in f:
            continue
        s = open(f, errors='replace').read()
        if (macro and re.search(r'\b%s\b' % macro, s)) or re.search(r'\b%s\b' % vt, s):
            users.append(f)
    for f in users:
        if re.search(r'^\S[^;\n]*\b%s_ctor\(' % cls, open(f).read(), re.M):
            return f
    return users[0] if users else None


def emit(struct, names):
    st = v.structs()
    vts = v.vtables()
    flds = fields(st, struct)
    bp = base_path(st, struct)
    src = open(v.VTABLES).read()
    for vt in names:
        count, slots, _, _ = vts[vt]
        if len(slots) != len(flds):
            sys.exit('%s: %d slots, %s has %d fields' % (vt, len(slots), struct, len(flds)))
        macro = None
        for f in v.sources():
            m = re.search(r'#define (\w+)\s+GAME_VFTABLE\(%s\)' % vt, open(f, errors='replace').read())
            if m:
                macro, mfile = m.group(1), f
                break
        own = owner(vt, macro)
        if not own:
            sys.exit('%s: no owner file' % vt)
        lines = ['', '/* the vtable of %s */' % vt[:-len('_vftable')], 'const %s %s = {' % (struct, vt)]
        for (off, fn), (des, fld) in zip(slots, flds):
            lines.append('\t.%s = %s,' % (des, '(void *)crt_purecall' if fn == 'crt_purecall' else fn))
        lines.append('};')
        text = open(own).read().rstrip('\n') + '\n' + '\n'.join(lines) + '\n'
        open(own, 'w').write(text)
        ref = '&%s%s' % (vt, '.' + bp if bp else '')
        if macro:
            t = open(mfile).read()
            block = 'extern const %s %s;\n#define %s (%s)\n' % (struct, vt, macro, ref)
            t = re.sub(r'#define %s\s+GAME_VFTABLE\(%s\)[^\n]*\n' % (macro, vt), lambda m: block, t)
            # after the definition of the struct when that comes later in the same file
            d = t.find('} %s;' % struct)
            if d > t.find(block):
                t = t.replace(block, '', 1)
                d = t.find('} %s;' % struct)
                e = t.index('\n', d) + 1
                while t[e:].startswith('SD_'):
                    e = t.index('\n', e) + 1
                t = t[:e] + block + t[e:]
            open(mfile, 'w').write(t)
        # the array out of Vtables.c
        src = re.sub(r'void \(\*const %s\[\d+\]\)\(void\) = \{.*?\n\};\n' % vt, '', src, flags=re.S)
        print('%-40s -> %s%s' % (vt, v.rel(own), '' if macro else '  (no macro)'))
    open(v.VTABLES, 'w').write(src)


if __name__ == '__main__':
    if sys.argv[1] == 'emit':
        emit(sys.argv[2], sys.argv[3:])
    elif sys.argv[1] == 'fields':
        for d, f in fields(v.structs(), sys.argv[2]):
            print(d, f)


# ---- fixsig: the slot functions of the gcc errors "initialization of 'A' from incompatible pointer type 'B'" get
# the signature of their slot: the first parameter becomes the slot's base pointer ("base", the old name a local
# cast from it), the return type and the other parameter types the slot's

ERR_RE = re.compile(r"^(.+?):(\d+):\d+: error: initialization of ‘(.+?)’(?: \{aka ‘.+?’\})? from incompatible pointer type ‘(.+?)’")


def parse_fptype(t):
    """'int (*)(cScorerItem *, int)' -> ('int', ['cScorerItem *', 'int'])"""
    i = t.index('(*)')
    ret = t[:i].strip()
    params = v.split_params(t[i + 3:].strip()[1:-1])
    return ret, [re.sub(r'\s+', ' ', p) for p in params]


def ptype(p):
    """the type of a parameter text without its name"""
    p = re.sub(r'\s+', ' ', p.strip())
    m = re.match(r'(.*?[\s\*])(\w+)$', p)
    if m and m.group(2) not in ('int', 'float', 'double', 'char', 'byte', 'void', 'long', 'short'):
        return m.group(1).strip(), m.group(2)
    return p, None


def norm(t):
    return re.sub(r'\s*\*', ' *', re.sub(r'\s+', ' ', t)).replace('* *', '**').strip()


def fixsig(logfile):
    defs = v.definitions()
    todo = {}
    for line in open(logfile):
        m = ERR_RE.match(line)
        if not m:
            continue
        f, ln = v.ROOT + m.group(1), int(m.group(2))
        src = open(f).read().split('\n')[ln - 1]
        fm = re.search(r'=\s*(\w+)\s*,', src)
        if not fm:
            continue
        todo[fm.group(1)] = (parse_fptype(m.group(3)), parse_fptype(m.group(4)))
    report = []
    protos = {}
    # per file from the last definition up, so the offsets of the others stay valid
    order = sorted((fn for fn in todo if fn in defs), key=lambda fn: (defs[fn][0], -defs[fn][1]))
    report += ['%s: no definition' % fn for fn in todo if fn not in defs]
    for fn in order:
        (wret, wpar), (hret, hpar) = todo[fn]
        f, off, ret, params = defs[fn]
        if len(params) != len(wpar):
            report.append('%s: %d parameters, the slot %d' % (fn, len(params), len(wpar)))
            continue
        s = open(f).read()
        ss = v.strip_comments(s)
        # the definition found again in the current text (no stored offsets)
        dm = None
        for cand in re.finditer(r'^[A-Za-z_][\w \t\*]*?\b%s\(' % fn, ss, re.M):
            depth, k = 1, cand.end()
            while depth and k < len(ss):
                depth += {'(': 1, ')': -1}.get(ss[k], 0)
                k += 1
            if re.match(r'\s*\{', ss[k:]):
                dm = cand
                break
        if not dm:
            report.append('%s: definition not found again' % fn)
            continue
        off = dm.start()
        hm = re.compile(r'\b%s\(' % fn).search(ss, off)
        # the header of the definition: from the return type to the brace
        head_start = off
        brace = ss.index('{', hm.end())
        body_end = brace + 1
        depth = 1
        while depth:
            if ss[body_end] == '{':
                depth += 1
            elif ss[body_end] == '}':
                depth -= 1
            body_end += 1
        body = s[brace + 1:body_end - 1]
        newp = []
        local = None
        for i, (p, w) in enumerate(zip(params, wpar)):
            t, name = ptype(p)
            if norm(t) == norm(w):
                newp.append(p)
                continue
            if i == 0 and '*' in t and '*' in w:
                if name and re.search(r'\b%s\b' % name, body):
                    bname = 'base' if name != 'base' and not re.search(r'\bbase\b', body) else 'elem'
                    newp.append('%s%s%s' % (w, '' if w.endswith('*') else ' ', bname))
                    local = (t, name, bname)
                else:
                    newp.append('%s%s%s' % (w, '' if w.endswith('*') else ' ', name or 'self'))
            else:
                newp.append('%s%s%s' % (w, '' if w.endswith('*') else ' ', name or 'arg%d' % i))
        newret = ret
        if norm(ret.replace('static', '').replace('inline', '').strip()) != norm(wret):
            keep = ' '.join(x for x in ret.split() if x in ('static', 'inline'))
            newret = (keep + ' ' if keep else '') + wret
        new_head = '%s%s%s(%s) ' % (newret, '' if newret.endswith('*') else ' ', fn, ', '.join(newp))
        new_body = body
        if local:
            t, name, bname = local
            new_body = '\n\t%s%s%s = (%s)%s;%s' % (t, '' if t.endswith('*') else ' ', name, t, bname, body if body.startswith('\n') else '\n' + body)
            if newret != ret:
                new_body = re.sub(r'\breturn %s;' % name, 'return %s;' % bname, new_body)
        s = s[:head_start] + new_head + '{' + new_body + '}' + s[body_end:]
        open(f, 'w').write(s)
        protos[fn] = '%s%s%s(%s);' % (newret.replace('static ', '').replace('inline ', ''), '' if newret.endswith('*') else ' ', fn, ', '.join(newp))
        print('%-44s %s' % (fn, v.rel(f)))
    # the prototypes, after all definitions
    for g in v.sources():
        if g == v.VTABLES:
            continue
        t = open(g, errors='replace').read()
        t2 = t
        for fn, proto in protos.items():
            # a prototype starts in column 0 with its type, not a statement (return f(...);)
            t2 = re.sub(r'^(?!static\b|return\b)[A-Za-z_][\w \t\*]*\b%s\([^;{]*\);' % fn, proto, t2, flags=re.M)
        if t2 != t:
            open(g, 'w').write(t2)
    for r in report:
        print('report:', r)


if __name__ == '__main__' and sys.argv[1] == 'fixsig':
    fixsig(sys.argv[2])


# ---- fixcall: "passing argument N of 'F' from incompatible pointer type" with "expected 'B *' but argument is of
# type 'D *'": the argument becomes the address of the base part (&x->a.b), found from the struct layout

CALL_RE = re.compile(r"^(.+?):(\d+):(\d+): error: passing argument (\d+) of ‘(\w+)’ from incompatible pointer type")
NOTE_RE = re.compile(r"note: expected ‘(?:const )?(?:struct )?(\w+) \*’ but argument is of type ‘(?:const )?(?:struct )?(\w+) \*’")


def arg_end(line, i):
    depth = 0
    while i < len(line):
        c = line[i]
        if c in '([{':
            depth += 1
        elif c in ')]}':
            if depth == 0:
                return i
            depth -= 1
        elif c == ',' and depth == 0:
            return i
        i += 1
    return i


def fixcall(logfile):
    st = v.structs()
    lines = open(logfile).read().split('\n')
    edits = {}
    for k, line in enumerate(lines):
        m = CALL_RE.match(line)
        if not m:
            continue
        note = next((NOTE_RE.search(x) for x in lines[k + 1:k + 14] if NOTE_RE.search(x)), None)
        if not note:
            continue
        base, derived = note.group(1), note.group(2)
        path = v.upcast_path(st, derived, base)
        if path is None:
            print('report: %s:%s %s -> %s no path' % (m.group(1), m.group(2), derived, base))
            continue
        edits.setdefault(v.ROOT + m.group(1), []).append((int(m.group(2)), int(m.group(3)), path, m.group(5)))
    for f, es in edits.items():
        src = open(f).read().split('\n')
        for ln, col, path, fn in sorted(set(es), key=lambda e: (e[0], -e[1])):
            line = src[ln - 1]
            # the display column of gcc (tabs to 8) to an index
            i, d = 0, 1
            while i < len(line) and d < col:
                d = (d - 1) // 8 * 8 + 9 if line[i] == '\t' else d + 1
                i += 1
            e = arg_end(line, i)
            arg = line[i:e].strip()
            if not arg:
                print('report: %s:%d empty argument' % (v.rel(f), ln))
                continue
            if arg.startswith('&') and re.match(r'&[\w>.\[\]-]+$', arg):
                new = '%s.%s' % (arg, path)
            elif re.match(r'[\w>.\[\]-]+$', arg):
                new = '&%s->%s' % (arg, path)
            else:
                new = '&(%s)->%s' % (arg, path)
            src[ln - 1] = line[:i] + new + line[e:]
            print('%s:%d  %s -> %s' % (v.rel(f), ln, arg, new))
        open(f, 'w').write('\n'.join(src))


if __name__ == '__main__' and sys.argv[1] == 'fixcall':
    fixcall(sys.argv[2])
