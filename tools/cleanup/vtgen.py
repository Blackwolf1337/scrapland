#!/usr/bin/env python3
# The typed-vtable conversion (tools/cleanup, removed at the end of the phase).
#
#   vtgen.py emit <Struct> <vtable> ...   each vtable of Vtables.c becomes "const <Struct> <vtable> = { .Field = fn };"
#                                          at the end of its owner file (where its CX_VFTABLE macro or its symbol is
#                                          used, the one with the class constructor first), the macro points at it
#                                          (at the base part when <Struct> embeds the family struct), the array
#                                          leaves Vtables.c
#   vtgen.py fields <Struct>              the slot fields of a struct in slot order (with the embedded bases)
import os, re, sys
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
    t = re.sub(r'(?:struct )?__va_list_tag ?\*', 'va_list', t)
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

CALL_RE = re.compile(r"^(.+?):(\d+):(\d+): error: passing argument (\d+) of ‘(.+?)’ from incompatible pointer type")
NOTE_RE = re.compile(r"note: expected ‘(?:const )?(?:struct )?(\w+) \*’ but argument is of type ‘(?:const )?(?:struct )?(\w+) \*’")


def display_to_index(line, col):
    """the display column of gcc (tabs to 8) to an index"""
    i, d = 0, 1
    while i < len(line) and d < col:
        d = (d - 1) // 8 * 8 + 9 if line[i] == '\t' else d + 1
        i += 1
    return i


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
            if v.upcast_path(st, base, derived) is not None:
                # the code holds the base pointer of a class that derives: a downcast
                path = ('CAST', base)
            else:
                print('report: %s:%s %s -> %s no path' % (m.group(1), m.group(2), derived, base))
                continue
        loc = (m.group(1), int(m.group(2)), int(m.group(3)))
        if not os.path.exists(v.ROOT + m.group(1)):
            continue
        if open(v.ROOT + m.group(1)).read().split('\n')[loc[1] - 1].lstrip().startswith('#define'):
            ex = next((re.match(r'^(.+?):(\d+):(\d+): note: in expansion of macro', x) for x in lines[k + 1:k + 20]
                       if re.match(r'^(.+?):(\d+):(\d+): note: in expansion of macro', x)), None)
            if not ex:
                print('report: %s:%s inside a macro' % loc[:2])
                continue
            loc = (ex.group(1), int(ex.group(2)), int(ex.group(3)))
        # the range of the argument from the marker line of gcc (the caret of 'a->b' is on the '->')
        span = None
        if loc[0] == m.group(1) and k + 2 < len(lines):
            mk = re.match(r'^(\s*\|\s)(.*)$', lines[k + 2])
            if mk and re.match(r'^\s*\d+ \|', lines[k + 1]):
                r = [j for j, ch in enumerate(mk.group(2)) if ch in '~^']
                if r:
                    span = (r[0] + 1, r[-1] + 1)
        edits.setdefault(os.path.realpath(v.ROOT + loc[0]), []).append((loc[1], loc[2], path, m.group(5), span))
    for f, es in edits.items():
        src = open(f).read().split('\n')
        uniq = {}
        for e in es:	# a header error comes once per source file, the callee text differs (<U...> of gcc)
            uniq.setdefault((e[0], e[1]), e)
        for ln, col, path, fn, span in sorted(uniq.values(), key=lambda e: (e[0], -e[1])):  # noqa
            line = src[ln - 1]
            if span:
                i = display_to_index(line, span[0])
                e = display_to_index(line, span[1] + 1)
            else:
                i = display_to_index(line, col)
                e = arg_end(line, i)
            arg = line[i:e].strip()
            if not arg:
                print('report: %s:%d empty argument' % (v.rel(f), ln))
                continue
            if isinstance(path, tuple):
                new = '(%s *)%s' % (path[1], arg if re.match(r'[\w>.\[\]-]+$', arg) else '(%s)' % arg)
            elif arg.startswith('&') and re.match(r'&[\w>.\[\]-]+$', arg):
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


# ---- stubs: a trivial function (empty, or returning a constant or its first argument) in a slot whose signature
# differs gets a typed default of that slot, <Class>_<Field><Kind>, defined next to the other functions of the class
# that has the field (the old stub stays for the tests)

def field_sigs(st, name):
    """designator -> (field, return text, params text) of a struct, through the embedded bases"""
    out = {}
    body = st[name]
    first = True
    for decl in re.finditer(r'[^;{}]+;', body):
        d = decl.group(0).strip()
        mb = re.match(r'(?:const\s+)?(\w+Vtbl\w*)\s+(\w+)\s*;$', d)
        if first and mb:
            for k, val in field_sigs(st, mb.group(1)).items():
                out[mb.group(2) + '.' + k] = val
            first = False
            continue
        first = False
        m = re.match(r'(.*?)\(\s*\*\s*(\w+)\s*\)\s*\((.*)\)\s*;$', d, re.S)
        if m:
            out[m.group(2)] = (m.group(2), re.sub(r'\s+', ' ', m.group(1)).strip(), re.sub(r'\s+', ' ', m.group(3)).strip())
    return out


def stub_kind(fn, defs):
    if fn not in defs:
        return None
    f, off, ret, params = defs[fn]
    s = v.strip_comments(open(f).read())
    m = re.compile(r'\b%s\([^;{]*\)\s*\{' % fn).search(s, off)
    if not m:
        return None
    i, depth = m.end(), 1
    while depth:
        depth += {'{': 1, '}': -1}.get(s[i], 0)
        i += 1
    body = re.sub(r'\(void\)\s*\w+\s*;', '', s[m.end():i - 1]).strip()
    names = [ptype(p)[1] for p in params]
    if body == '':
        return ('Nop', None)
    mm = re.match(r'return\s+(.+?)\s*;$', body)
    if not mm:
        return None
    e = mm.group(1)
    if e in ('1', 'true'):
        return ('True', '1')
    if e in ('0', 'false'):
        return ('False', '0')
    if e == 'NULL':
        return ('Null', 'NULL')
    if names and e == names[0]:
        return ('Self', 'SELF')
    if re.match(r'-?\d+$|0x[0-9a-fA-F]+$', e):
        return ('Ret%s' % e.replace('-', 'M'), e)
    return None


def stubs(files):
    st = v.structs()
    defs = v.definitions()
    made = {}
    for f in files:
        f = v.ROOT + f if not f.startswith('/') else f
        src = open(f).read()
        out = src
        for vm in re.finditer(r'const (\w+) (\w+_vftable) = \{\n(.*?)\n\};', src, re.S):
            struct = vm.group(1)
            sigs = field_sigs(st, struct)
            body = vm.group(3)
            newbody = body
            for lm in re.finditer(r'^\t\.([\w.]+) = (\w+),$', body, re.M):
                des, fn = lm.group(1), lm.group(2)
                if des not in sigs or fn == 'crt_purecall':
                    continue
                field, wret, wparams = sigs[des]
                if fn not in defs:
                    continue
                f2, off, ret, params = defs[fn]
                wp = v.split_params(wparams)
                same = len(params) == len(wp) and all(norm(ptype(a)[0]) == norm(ptype(b)[0]) for a, b in zip(params, wp)) \
                    and norm(ret.replace('static', '').replace('inline', '').strip()) == norm(wret)
                if same:
                    continue
                kind = stub_kind(fn, defs)
                if not kind:
                    continue
                cls = ptype(wp[0])[0].replace('const ', '').replace('*', '').strip() if wp else 'Vtbl'
                name = '%s_%s%s' % (cls, field, kind[0])
                if name not in made:
                    made[name] = (cls, field, wret, wp, kind, f)
                newbody = re.sub(r'^\t\.%s = %s,$' % (re.escape(des), fn), '\t.%s = %s,' % (des, name), newbody, flags=re.M)
            out = out.replace(body, newbody)
        if out != src:
            open(f, 'w').write(out)
    # the defaults next to the functions of their class, the prototypes next to the class's prototypes
    for name, (cls, field, wret, wp, kind, vtfile) in sorted(made.items()):
        if name in defs:
            continue
        names = [ptype(p)[1] or 'arg%d' % i for i, p in enumerate(wp)]
        params = ', '.join(p if ptype(p)[1] else '%s arg%d' % (p, i) for i, p in enumerate(wp))
        lines = ['/* the slot %s of a class that does %s */' % (field, {'Nop': 'nothing there', 'True': 'only return 1',
                 'False': 'only return 0', 'Null': 'only return NULL', 'Self': 'only return itself'}.get(kind[0], 'only return ' + str(kind[1])))]
        lines.append('%s%s%s(%s) {' % (wret, '' if wret.endswith('*') else ' ', name, params))
        unused = names[1:] if kind[0] == 'Self' else names
        for n in unused:
            lines.append('\t(void)%s;' % n)
        if kind[1] is not None:
            lines.append('\treturn %s;' % (names[0] if kind[1] == 'SELF' else kind[1]))
        lines.append('}')
        fn_text = '\n'.join(lines) + '\n'
        home = next((d[0] for n, d in defs.items() if n.startswith(cls + '_')), None) or vtfile
        if not home:
            print('report: no module for', name)
            continue
        t = open(home).read()
        i = t.find('\n/* the vtable of ')
        t = (t[:i + 1] + fn_text + t[i:]) if i >= 0 else (t.rstrip('\n') + '\n\n' + fn_text)
        open(home, 'w').write(t)
        proto = '%s%s%s(%s);' % (wret, '' if wret.endswith('*') else ' ', name, params)
        hdr = None
        for g in v.sources():
            if g.endswith('.h') and re.search(r'^[\w \*]*\b%s_\w+\(' % cls, open(g, errors='replace').read(), re.M):
                hdr = g
                break
        if not hdr:
            hdr = next((g for g in v.sources() if g.endswith('.h') and
                        re.search(r'^struct %s \{' % cls, open(g, errors='replace').read(), re.M)), None)
            if hdr:
                t = open(hdr).read()
                m = re.search(r'^struct %s \{.*?\n\};\n' % cls, t, re.M | re.S)
                t = t[:m.end()] + '\n' + proto + '\n' + t[m.end():]
                open(hdr, 'w').write(t)
                print('%-48s %s %s' % (name, v.rel(home), v.rel(hdr)))
                continue
        if hdr:
            t = open(hdr).read()
            ms = list(re.finditer(r'^[\w \*]*\b%s_\w+\([^;{]*\);\n' % cls, t, re.M))
            t = t[:ms[-1].end()] + proto + '\n' + t[ms[-1].end():]
            open(hdr, 'w').write(t)
        print('%-48s %s %s' % (name, v.rel(home), v.rel(hdr) if hdr else '(no header)'))


if __name__ == '__main__' and sys.argv[1] == 'stubs':
    stubs(sys.argv[2:])


# ---- protos: "'F' undeclared here (not in a function)" in a vtable: the prototype of F (its definition as it is)
# next to the prototypes of its class in a header

UNDECL_RE = re.compile(r"^(.+?):(\d+):\d+: error: ‘(\w+)’ undeclared here \(not in a function\)")


def add_proto(fn, defs):
    f, off, ret, params = defs[fn]
    cls = fn.rsplit('_', 1)[0]
    proto = '%s%s%s(%s);' % (ret.replace('static ', '').replace('inline ', '').strip(),
                             '' if ret.strip().endswith('*') else ' ', fn, ', '.join(params))
    for g in v.sources():
        if not g.endswith('.h'):
            continue
        t = open(g, errors='replace').read()
        ms = list(re.finditer(r'^[\w \*]*\b%s_\w+\([^;{]*\);\n' % re.escape(cls), t, re.M))
        if ms:
            t = t[:ms[-1].end()] + proto + '\n' + t[ms[-1].end():]
            open(g, 'w').write(t)
            return v.rel(g)
    return None


def protos(logfile):
    defs = v.definitions()
    done = set()
    for line in open(logfile):
        m = UNDECL_RE.match(line)
        if not m or m.group(3) in done:
            continue
        fn = m.group(3)
        done.add(fn)
        if fn not in defs:
            print('report: %s has no definition' % fn)
            continue
        h = add_proto(fn, defs)
        print('%-44s %s' % (fn, h or 'report: no header'))


# ---- fixvt: "assignment to 'const T *' from incompatible pointer type 'const U *'" where the right side is a
# CX_VFTABLE macro: the macro points at the part of type T of its vtable

ASG_RE = re.compile(r"^(.+?):(\d+):\d+: error: assignment to ‘const (\w+) \*’ from incompatible pointer type ‘const (\w+) \*’")


def vt_path(st, struct, target, prefix=''):
    if struct == target:
        return prefix.rstrip('.')
    body = st.get(struct, '').strip()
    mb = re.match(r'(?:const\s+)?(\w+Vtbl\w*)\s+(\w+)\s*;', body)
    if not mb:
        return None
    return vt_path(st, mb.group(1), target, prefix + mb.group(2) + '.')


def fixvt(logfile):
    st = v.structs()
    for line in open(logfile):
        m = ASG_RE.match(line)
        if not m:
            continue
        src = open(v.ROOT + m.group(1)).read().split('\n')[int(m.group(2)) - 1]
        mm = re.search(r'=\s*(C\w+_VFTABLE)\s*;', src)
        if not mm:
            print('report: %s:%s %s' % (m.group(1), m.group(2), src.strip()))
            continue
        macro, target = mm.group(1), m.group(3)
        for g in v.sources():
            t = open(g, errors='replace').read()
            dm = re.search(r'extern const (\w+) (\w+);\n#define %s \(&[\w.]+\)' % macro, t)
            if not dm:
                continue
            path = vt_path(st, dm.group(1), target)
            if path is None:
                print('report: %s: %s has no part %s' % (macro, dm.group(1), target))
                break
            ref = '&%s%s' % (dm.group(2), '.' + path if path else '')
            t2 = re.sub(r'(#define %s )\(&[\w.]+\)' % macro, lambda x: x.group(1) + '(%s)' % ref, t)
            if t2 != t:
                open(g, 'w').write(t2)
                print('%-36s -> %s' % (macro, ref))
            break


if __name__ == '__main__' and sys.argv[1] in ('protos', 'fixvt'):
    {'protos': protos, 'fixvt': fixvt}[sys.argv[1]](sys.argv[2])


# ---- fixmember: "'const cXVtbl' has no member named 'F'" where F sits in an embedded base: insert the path
def fixmember(log):
    st = v.structs()
    lines = open(log, errors='replace').read().split('\n')
    edits = {}
    for x in lines:
        x = re.sub(r"‘[^’]*’ \{aka ‘([^’]*)’\}", r"‘\1’", x)
        m = re.match(r"^(.+?):(\d+):(\d+): error: ‘(?:const )?(?:struct )?(\w+Vtbl\w*)’ has no member named ‘(\w+)’", x)
        if not m or m.group(4) not in st:
            continue
        path = next((d for d, f in fields(st, m.group(4)) if f == m.group(5) and '.' in d), None)
        if not path:
            print('report: %s:%s %s.%s' % (m.group(1), m.group(2), m.group(4), m.group(5)))
            continue
        edits.setdefault(os.path.realpath(v.ROOT + m.group(1)), set()).add((int(m.group(2)), int(m.group(3)), path, m.group(5)))
    for f, es in edits.items():
        t = open(f).read().split('\n')
        for ln, col, path, name in sorted(es, reverse=True):
            s = t[ln - 1]
            c = display_to_index(s, col)
            mm = re.match(r'\s*(->|\.)\s*', s[c:])
            if mm:
                c += mm.end()
            if s[c:c + len(name)] != name:
                print('report: %s:%d column' % (v.rel(f), ln))
                continue
            t[ln - 1] = s[:c] + path + s[c + len(name):]
            print('%s:%d %s' % (v.rel(f), ln, path))
        open(f, 'w').write('\n'.join(t))


if __name__ == '__main__' and sys.argv[1] == 'fixmember':
    fixmember(sys.argv[2])
