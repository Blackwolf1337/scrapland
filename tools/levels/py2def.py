#!/usr/bin/env python3
"""py2def.py <module name> <script.py> [out.def]: the functions of a level script whose bodies are plain calls,
if/elif/else on simple conditions, local and global assignments of simple values, as .def sections
"section <Func> <params>". Prints the functions that stay Python (and why) to stderr."""
import re, sys

NUM = re.compile(r'^[+-]?(\d+\.?\d*|\.\d+)$')
STR = re.compile(r'^("([^"\\{}]*)"|\'([^\'\\{}"]*)\')$')
NAME = re.compile(r'^[A-Za-z_]\w*$')


class Unsupported(Exception):
    pass


def split_args(s):
    """the comma separated parts of s at depth 0"""
    out, depth, cur, q = [], 0, '', None
    for ch in s:
        if q:
            cur += ch
            if ch == q:
                q = None
            continue
        if ch in '"\'':
            q = ch
        elif ch in '([':
            depth += 1
        elif ch in ')]':
            depth -= 1
        if ch == ',' and depth == 0:
            out.append(cur.strip())
            cur = ''
        else:
            cur += ch
    if cur.strip():
        out.append(cur.strip())
    return out


class Conv:
    def __init__(self, module, globs, funcs, setdef=False):
        self.module, self.globs, self.funcs, self.setdef = module, globs, funcs, setdef

    def value(self, v, scope):
        v = v.strip()
        if NUM.match(v):
            if 'e' in v.lower():
                raise Unsupported('exponent ' + v)
            return v
        m = STR.match(v)
        if m:
            s = m.group(2) if m.group(2) is not None else m.group(3)
            return '"%s"' % s
        if v == 'None':
            return 'None'
        if NAME.match(v):
            if v in scope:
                return '$' + v
            if v in self.globs:
                return '%%%s.%s' % (self.module, v)
            raise Unsupported('name ' + v)
        m = re.match(r'^([A-Za-z_]\w*)\.([A-Za-z_]\w*)$', v)
        if m and m.group(1) not in scope and m.group(1) not in self.globs:
            return '%%%s.%s' % (m.group(1), m.group(2))
        m = re.match(r'^([A-Za-z_]\w*)\s*([+-])\s*(\d+)$', v)
        if m:
            base = self.value(m.group(1), scope)
            if base[0] in '$%':
                return '%s%s%s' % (base, m.group(2), m.group(3))
        parts = split_plus(v)
        if len(parts) > 1:
            text = ''
            for p in parts:
                m = STR.match(p)
                if m:
                    text += m.group(2) if m.group(2) is not None else m.group(3)
                elif NAME.match(p) and p in scope:
                    text += '$(%s)' % p
                else:
                    raise Unsupported('value ' + v)
            return '"%s"' % text
        if v.startswith('(') and v.endswith(')'):
            parts = split_args(v[1:-1])
            vals = [self.value(p, scope) for p in parts]
            if any(x.startswith('(') or x.startswith('"') and (' ' in x or ',' in x) for x in vals):
                raise Unsupported('tuple ' + v)
            return '(' + ','.join(vals) + (',' if len(vals) == 1 else '') + ')'
        raise Unsupported('value ' + v)

    def cond(self, c, scope):
        c = c.strip()
        if c.startswith('(') and c.endswith(')') and not split_args(c[1:-1])[1:]:
            c = c[1:-1].strip()
        for op in ('==', '!='):
            if op in c:
                a, b = c.split(op, 1)
                a, b = self.value(a, scope), self.value(b, scope)
                if a.startswith('(') or b.startswith('('):
                    raise Unsupported('cond ' + c)
                return '%s %s %s' % (a, op, b)
        if re.search(r'\b(and|or|not|in)\b|[<>]', c):
            raise Unsupported('cond ' + c)
        return self.value(c, scope)

    def call(self, t, scope):
        # an engine variable back to its default (the end of the scene loaders): a step of Levels.c (only for the
        # levels whose logic is C so far)
        m = re.match(r'^Scrap\.Set\(\s*(["\']\w+["\'])\s*,\s*Scrap\.Def\(\s*(["\']\w+["\'])\s*\)\s*\)$', t)
        if m and self.setdef and m.group(1)[1:-1] == m.group(2)[1:-1]:
            return 'run setdef "%s"' % m.group(1)[1:-1]
        m = re.match(r'^([A-Za-z_]\w*(?:\.[A-Za-z_]\w*)?)\((.*)\)$', t)
        if not m:
            raise Unsupported('stmt ' + t)
        fn = m.group(1)
        if '.' not in fn:
            if fn not in self.funcs:
                raise Unsupported('call ' + fn)
            fn = '%s.%s' % (self.module, fn)
        elif fn.split('.')[0] in scope:
            raise Unsupported('method ' + fn)
        out = ['call', fn]
        for a in split_args(m.group(2)):
            k = re.match(r'^([A-Za-z_]\w*)\s*=(?!=)\s*(.*)$', a)
            if k:
                out.append('%s=%s' % (k.group(1), self.value(k.group(2), scope)))
            else:
                out.append(self.value(a, scope))
        if any(' ' in x and not x.startswith('"') for x in out[2:]):
            raise Unsupported('arg ' + t)
        return ' '.join(out)

    def body(self, name, params, lines):
        scope = set(p.split('=')[0] for p in params)
        glob = set()
        out, stack = [], []   # stack of indentation levels of open if blocks
        for n, (ind, t) in enumerate(lines):
            while stack and ind <= stack[-1] and not re.match(r'^(elif\b|else\s*:)', t) or stack and ind < stack[-1]:
                stack.pop()
                out.append('end')
            pad = '\t' * len(stack)
            if t == 'pass':
                continue
            if re.match(r'^(import|from)\s', t):
                continue
            m = re.match(r'^global\s+(.*)$', t)
            if m:
                glob |= set(x.strip() for x in m.group(1).split(','))
                continue
            m = re.match(r'^(if|elif)\s+(.*):$', t)
            if m:
                if m.group(1) == 'if':
                    out.append(pad + 'if ' + self.cond(m.group(2), scope))
                    stack.append(ind)
                else:
                    if not stack:
                        raise Unsupported('elif')
                    out.append('\t' * (len(stack) - 1) + 'elif ' + self.cond(m.group(2), scope))
                continue
            if re.match(r'^else\s*:$', t):
                if not stack:
                    raise Unsupported('else')
                out.append('\t' * (len(stack) - 1) + 'else')
                continue
            if t == 'return' and n == len(lines) - 1:
                continue
            m = re.match(r'^([A-Za-z_]\w*)\s*=(?!=)\s*(.*)$', t)
            if m:
                var, val = m.group(1), m.group(2)
                v = self.value(val, scope)
                if var in glob:
                    out.append(pad + 'setvar %s.%s %s' % (self.module, var, v))
                else:
                    if var in self.globs:
                        raise Unsupported('local shadows global ' + var)
                    out.append(pad + 'let %s %s' % (var, v))
                    scope.add(var)
                continue
            out.append(pad + self.call(t, scope))
        while stack:
            stack.pop()
            out.append('end')
        return out


def split_plus(s):
    """the + separated parts of s at depth 0 outside strings"""
    out, depth, cur, q = [], 0, '', None
    for ch in s:
        if q:
            cur += ch
            if ch == q:
                q = None
            continue
        if ch in '"\'':
            q = ch
        elif ch in '([':
            depth += 1
        elif ch in ')]':
            depth -= 1
        if ch == '+' and depth == 0 and cur.strip():
            out.append(cur.strip())
            cur = ''
        else:
            cur += ch
    out.append(cur.strip())
    return out


def logical_lines(src):
    """(indent, text) of the lines without comments, continuation lines joined"""
    res, buf, bind, depth = [], '', 0, 0
    for raw in src.split('\n'):
        line = raw.replace('\t', '    ')
        s = re.sub(r'#.*$', '', line) if '"' not in line and "'" not in line else strip_comment(line)
        if not s.strip() and not buf:
            continue
        if not buf:
            bind = len(s) - len(s.lstrip())
        buf += (' ' if buf else '') + s.strip()
        depth = sum(1 if c in '([' else -1 if c in ')]' else 0 for c in buf)
        if depth > 0 or buf.endswith('\\'):
            buf = buf.rstrip('\\')
            continue
        res.append((bind, buf))
        buf = ''
    return res


def strip_comment(line):
    q = None
    for i, ch in enumerate(line):
        if q:
            if ch == q:
                q = None
        elif ch in '"\'':
            q = ch
        elif ch == '#':
            return line[:i]
    return line


def convert(module, path, setdef=False):
    src = open(path, encoding='latin1').read().replace('\r', '')
    lines = logical_lines(src)
    globs, funcs = set(), set()
    for ind, t in lines:
        if ind:
            continue
        m = re.match(r'^def\s+(\w+)', t)
        if m:
            funcs.add(m.group(1))
            globs.add(m.group(1))
            continue
        m = re.match(r'^([A-Za-z_]\w*)\s*=', t)
        if m:
            globs.add(m.group(1))
        m = re.match(r'^import\s+(.*)$', t)
        if m:
            pass
    conv = Conv(module, globs, funcs, setdef)
    defs = []
    i = 0
    while i < len(lines):
        ind, t = lines[i]
        m = re.match(r'^def\s+(\w+)\s*\((.*)\)\s*:$', t)
        if not m or ind:
            i += 1
            continue
        name, params = m.group(1), [p.strip() for p in split_args(m.group(2))]
        body = []
        i += 1
        while i < len(lines) and lines[i][0] > 0:
            body.append(lines[i])
            i += 1
        base = min((b[0] for b in body), default=0)
        body = [(b[0] - base, b[1]) for b in body]
        try:
            ps = []
            for p in params:
                if p.startswith('*'):
                    raise Unsupported('varargs')
                if '=' in p:
                    k, v = p.split('=', 1)
                    dv = conv.value(v, set())
                    if dv[0] in '$%':
                        raise Unsupported('default ' + v)
                    ps.append('%s=%s' % (k.strip(), dv))
                else:
                    ps.append(p)
            out = conv.body(name, params, body)
            defs.append((name, ['section ' + ' '.join([name] + ps)] + out, None))
        except Unsupported as e:
            defs.append((name, None, str(e)))
    # a name defined twice: the last definition is the function, as Python binds it
    last = dict((name, k) for k, (name, _, _) in enumerate(defs))
    sections = [sec for k, (name, sec, _) in enumerate(defs) if last[name] == k and sec]
    kept = [(name, why) for k, (name, sec, why) in enumerate(defs) if last[name] == k and not sec]
    return sections, kept


if __name__ == '__main__':
    module, path = sys.argv[1], sys.argv[2]
    sections, kept = convert(module, path)
    text = '\n\n'.join('\n'.join(s) for s in sections) + '\n'
    if len(sys.argv) > 3:
        open(sys.argv[3], 'w').write(text)
    else:
        sys.stdout.write(text)
    for k, why in kept:
        sys.stderr.write('stays Python: %s (%s)\n' % (k, why))
