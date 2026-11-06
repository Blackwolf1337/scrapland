#!/usr/bin/env python3
"""py2def.py <module name> <script.py> [out.def]: the functions of a level script whose bodies are plain calls,
if/elif/else on simple conditions, local and global assignments of simple values, as .def sections
"section <Func> <params>". Prints the functions that stay Python (and why) to stderr."""
import re, sys

NUM = re.compile(r'^[+-]?(\d+\.?\d*|\.\d+)$')
STR = re.compile(r'^("([^"\\{}]*)"|\'([^\'\\{}]*)\')$')
NAME = re.compile(r'^[A-Za-z_]\w*$')


# the commands of the .def files (Script.c def_commands): an attribute line after `object` must not be one
COMMANDS = {'style', 'item', 'use', 'set', 'lr', 'ud', 'default', 'list', 'call', 'run', 'section', 'do', 'if', 'elif',
    'else', 'end', 'let', 'measure', 'action', 'getaction', 'class', 'event', 'setvar', 'entity', 'object'}


class Unsupported(Exception):
    pass


def quote(text):
    """a .def text token: in "" or, when the text holds a ", in '' (Def_Tokens has no escapes)"""
    if '"' in text and "'" in text:
        raise Unsupported('text ' + text)
    return "'%s'" % text if '"' in text else '"%s"' % text


def literal(m):
    """the text of a STR match; $( would be read as a value of the file"""
    text = m.group(2) if m.group(2) is not None else m.group(3)
    if '$(' in text:
        raise Unsupported('text ' + text)
    return text


def is_text(x):
    return x[:1] in '"\''


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
        self.pre, self.numeric, self.imported = [], set(), set()

    def quote(self, text):
        """a text token; a text with both " and ': its parts with a " in temporaries (let _qN '...'), joined by
        $(_qN) in a "..." text that Def_Text expands"""
        if not ('"' in text and "'" in text):
            return quote(text)
        if len(text) > 480:
            raise Unsupported('text longer than the .def text buffer')
        runs, cur = [], ''
        for ch in text:
            if ch in '"\'' and (('"' if ch == "'" else "'") in cur):
                runs.append(cur)
                cur = ''
            cur += ch
        runs.append(cur)
        out = ''
        for r in runs:
            if '"' in r:
                name = '_q%d' % len(self.pre)
                self.pre.append("let %s '%s'" % (name, r))
                out += '$(%s)' % name
            else:
                out += r
        return '"%s"' % out

    def module_name(self, n, scope):
        """n names a module: not a local, and not a global of the script unless the function imports it"""
        return n not in scope and (n not in self.globs or n in self.imported)

    def value(self, v, scope):
        v = v.strip()
        if NUM.match(v):
            if 'e' in v.lower():
                raise Unsupported('exponent ' + v)
            return v
        m = STR.match(v)
        if m:
            return self.quote(literal(m))
        if v == 'None':
            return 'None'
        m = re.match(r'^`([A-Za-z_]\w*)`$', v)
        if m and m.group(1) in scope:
            self.pre.append('run repr _r%s $%s' % (m.group(1), m.group(1)))
            return '$_r' + m.group(1)
        if NAME.match(v):
            if v in scope:
                return '$' + v
            if v in self.globs:
                return '%%%s.%s' % (self.module, v)
            raise Unsupported('name ' + v)
        m = re.match(r'^([A-Za-z_]\w*)\.([A-Za-z_]\w*)$', v)
        if m and self.module_name(m.group(1), scope):
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
                    text += literal(m)
                elif NAME.match(p) and p in scope:
                    text += '$(%s)' % p
                elif p.startswith('`') and p.endswith('`') and p[1:-1] in self.numeric:
                    self.pre.append('let _%s %%%s.%s' % (p[1:-1], self.module, p[1:-1]))
                    text += '$(_%s)' % p[1:-1]
                else:
                    raise Unsupported('value ' + v)
            return self.quote(text)
        if v.startswith('(') and v.endswith(')') and len(split_args(v[1:-1])) == 1 and not v[1:-1].strip().endswith(','):
            return self.value(v[1:-1], scope)
        if v.startswith('(') and v.endswith(')'):
            parts = split_args(v[1:-1])
            vals = [self.value(p, scope) for p in parts]
            if any(x.startswith('(') or is_text(x) and (' ' in x or ',' in x) for x in vals):
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
        if re.search(r'\b(and|or|not|in)\b', c):
            raise Unsupported('cond ' + c)
        m = re.match(r'^(.*?)\s*(<=|>=|<|>)\s*(.*)$', c)
        if m:
            # numbers (Def_Cond compares them as doubles): a local or global against a number
            a, b = self.value(m.group(1), scope), self.value(m.group(3), scope)
            if not (a[0] in '$%' and NUM.match(b) or b[0] in '$%' and NUM.match(a)) or re.search(r'[<>]', m.group(3)):
                raise Unsupported('cond ' + c)
            return '%s %s %s' % (a, m.group(2), b)
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
            r = re.match(r'^(?:([A-Za-z_]\w*)\s*=\s*)?SVec\.Add\((.*)\)$', a)
            if r:
                parts = split_args(r.group(2))
                rot = re.match(r'^SVec\.Rotate3D\((.*)\)$', parts[1]) if len(parts) == 2 else None
                inner = split_args(rot.group(1)) if rot else []
                if len(inner) != 2:
                    raise Unsupported('arg ' + a)
                tmp = '_v%d' % len(self.pre)
                self.pre.append('run vecrot %s %s %s %s' % (tmp, self.value(parts[0], scope), self.value(inner[0], scope),
                    self.value(inner[1], scope)))
                out.append(('%s=' % r.group(1) if r.group(1) else '') + '$' + tmp)
                continue
            k = re.match(r'^([A-Za-z_]\w*)\s*=(?!=)\s*(.*)$', a)
            if k:
                out.append('%s=%s' % (k.group(1), self.value(k.group(2), scope)))
            else:
                out.append(self.value(a, scope))
        if any(' ' in x and not is_text(x) for x in out[2:]):
            raise Unsupported('arg ' + t)
        return ' '.join(out)

    def body(self, name, params, lines):
        self.pre, self.numeric, self.imported = [], set(), set()
        scope = set(p.split('=')[0].strip() for p in params)
        lines = self.save_lists(lines)
        glob = set()
        out, stack = [], []   # stack of indentation levels of open if blocks
        obj = None            # the object of the last attribute line: (pad, name, len(out) after it)
        for n, (ind, t) in enumerate(lines):
            while stack and ind <= stack[-1] and not re.match(r'^(elif\b|else\s*:)', t) or stack and ind < stack[-1]:
                stack.pop()
                out.append('end')
            pad = '\t' * len(stack)
            if t == 'pass':
                continue
            m = re.match(r'^import\s+(.*)$', t)
            if m:
                self.imported |= set(x.strip() for x in m.group(1).split(','))
                continue
            if re.match(r'^from\s', t):
                continue
            m = re.match(r'^([A-Za-z_]\w*)\s*=\s*\[\s*\]$', t)
            if m and m.group(1) not in glob and m.group(1) not in self.globs:
                out.append(pad + 'run newlist ' + m.group(1))
                scope.add(m.group(1))
                continue
            m = re.match(r'^([A-Za-z_]\w*)\.append\((.*)\)$', t)
            if m and m.group(1) in scope and re.match(r'^[-+0-9.,()\s]+$', m.group(2)):
                # a literal of numbers (the race waypoints), evaluated by Python as the script would
                parts = split_args(m.group(2))
                text = '(%s)' % ', '.join(parts) if len(parts) > 1 else parts[0]
                out.append(pad + 'run evalappend $%s "%s"' % (m.group(1), re.sub(r'\s+', ' ', text)))
                continue
            m = re.match(r'^__savelistadd__ (.*)$', t)
            if m:
                out.append(pad + 'run savelistadd ' + m.group(1))
                continue
            m = re.match(r'^([A-Za-z_]\w*)\s*=\s*int\(\s*Scrap\.GetSaveVar\(\s*(.*?)\s*\)\s*\)$', t)
            if m and STR.match(m.group(2)) and m.group(1) not in glob and m.group(1) not in self.globs:
                out.append(pad + 'run saveint %s %s' % (m.group(1), self.value(m.group(2), scope)))
                scope.add(m.group(1))
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
            m = re.match(r'^([A-Za-z_]\w*)\.([A-Za-z_]\w*)\s*=(?!=)\s*(.*)$', t)
            if m and m.group(1) in scope and m.group(2) not in COMMANDS:
                # an attribute of a parameter or local (an entity handle, an object): `object $x` and attribute lines
                v = self.value(m.group(3), scope)
                out.extend(pad + x for x in self.pre)
                self.pre = []
                if obj != (pad, m.group(1), len(out)):
                    out.append(pad + 'object $' + m.group(1))
                out.append(pad + '%s %s' % (m.group(2), v))
                obj = (pad, m.group(1), len(out))
                continue
            if m and self.module_name(m.group(1), scope):
                v = self.value(m.group(3), scope)
                out.extend(pad + x for x in self.pre)
                self.pre = []
                out.append(pad + 'setvar %s.%s %s' % (m.group(1), m.group(2), v))
                continue
            m = re.match(r'^([A-Za-z_]\w*)\s*=(?!=)\s*(.*)$', t)
            if m:
                var, val = m.group(1), m.group(2)
                v = self.value(val, scope)
                out.extend(pad + x for x in self.pre)
                self.pre = []
                if var in glob and re.match(r'^%s\s*\+\s*\d+$' % var, val):
                    self.numeric.add(var)
                if var in glob:
                    out.append(pad + 'setvar %s.%s %s' % (self.module, var, v))
                else:
                    if var in self.globs:
                        raise Unsupported('local shadows global ' + var)
                    out.append(pad + 'let %s %s' % (var, v))
                    scope.add(var)
                continue
            line = self.call(t, scope)
            out.extend(pad + x for x in self.pre)
            self.pre = []
            out.append(pad + line)
        while stack:
            stack.pop()
            out.append('end')
        return out

    def save_lists(self, lines):
        """the four lines that add an item to a list save variable (Level_StepSaveListAdd) as one __savelistadd__
        line, when the list is not used after them"""
        out, n = [], 0
        while n < len(lines):
            ind, t = lines[n]
            m = re.match(r'^([A-Za-z_]\w*)\s*=\s*eval\(\s*Scrap\.GetSaveVar\(\s*(.*?)\s*\)\s*\)$', t)
            if m and STR.match(m.group(2)) and n + 3 < len(lines):
                lst, var = m.group(1), m.group(2)
                (i1, t1), (i2, t2), (i3, t3) = lines[n + 1:n + 4]
                c = re.match(r'^if\s+not\s+\(?\s*(.*?)\s+in\s+%s\s*\)?\s*:$' % lst, t1)
                a = re.match(r'^%s\.append\(\s*(.*?)\s*\)$' % lst, t2)
                sv = re.match(r'^Scrap\.SetSaveVar\(\s*(.*?)\s*,\s*`%s`\s*\)$' % lst, t3)
                rest = lines[n + 4:]
                if (c and a and sv and i1 == ind and i2 > ind and i3 == i2 and STR.match(c.group(1))
                        and c.group(1) == a.group(1) and sv.group(1) == var
                        and (not rest or rest[0][0] <= ind and not re.match(r'^(elif\b|else\s*:)', rest[0][1]))
                        and not any(re.search(r'\b%s\b' % lst, x) for _, x in rest)):
                    out.append((ind, '__savelistadd__ %s %s' % (self.value(var, set()), self.value(c.group(1), set()))))
                    n += 4
                    continue
            out.append(lines[n])
            n += 1
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
    triple = None
    for raw in src.split('\n'):
        t = raw.strip()
        if triple:
            if triple in t:
                triple = None
            continue
        if not buf and t[:3] in ('"""', "'''") and t.count(t[:3]) == 1:
            # a string statement over several lines: evaluated and dropped by Python
            triple = t[:3]
            continue
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
            if any(len(x.expandtabs(1)) > 480 for x in out):
                raise Unsupported('line longer than the .def line buffer')
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
