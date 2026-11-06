#!/usr/bin/env python3
"""pyc2mod.py <module name> <script.pyc>: the module code of a Python 1.5.2 pack module (its imports, variables,
function and class definitions and load time calls, in the order of the script) as a .mod file of ScrapC
(src/game/Script/ScriptModule.c), read from the bytecode of the .pyc, so the constants are the ones the game loads.
Prints the file; exits with 2 and names the statements it cannot say (loops, try, unpacking, ...) on stderr, those
need a C hook ("c <name>").

The values are Python expressions the loader evaluates in C: numbers as parsenumber reads them (a float as the %.12g
text that marshal of 1.5.2 wrote, read back with atof), strings in the repr of 1.5.2, None, tuple, list and dict
displays, names, attributes, calls (keywords too), subscripts, slices, + - * / %, comparisons, not, and, or."""
import struct, sys

OPS = {0: 'STOP_CODE', 1: 'POP_TOP', 2: 'ROT_TWO', 3: 'ROT_THREE', 4: 'DUP_TOP', 10: 'UNARY_POSITIVE',
    11: 'UNARY_NEGATIVE', 12: 'UNARY_NOT', 13: 'UNARY_CONVERT', 15: 'UNARY_INVERT', 19: 'BINARY_POWER',
    20: 'BINARY_MULTIPLY', 21: 'BINARY_DIVIDE', 22: 'BINARY_MODULO', 23: 'BINARY_ADD', 24: 'BINARY_SUBTRACT',
    25: 'BINARY_SUBSCR', 30: 'SLICE', 31: 'SLICE+1', 32: 'SLICE+2', 33: 'SLICE+3', 40: 'STORE_SLICE', 50: 'DELETE_SLICE',
    60: 'STORE_SUBSCR', 61: 'DELETE_SUBSCR', 62: 'BINARY_LSHIFT', 63: 'BINARY_RSHIFT', 64: 'BINARY_AND',
    65: 'BINARY_XOR', 66: 'BINARY_OR', 70: 'PRINT_EXPR', 71: 'PRINT_ITEM', 72: 'PRINT_NEWLINE', 80: 'BREAK_LOOP',
    82: 'LOAD_LOCALS', 83: 'RETURN_VALUE', 85: 'EXEC_STMT', 87: 'POP_BLOCK', 88: 'END_FINALLY', 89: 'BUILD_CLASS',
    90: 'STORE_NAME', 91: 'DELETE_NAME', 92: 'UNPACK_TUPLE', 93: 'UNPACK_LIST', 95: 'STORE_ATTR', 96: 'DELETE_ATTR',
    97: 'STORE_GLOBAL', 98: 'DELETE_GLOBAL', 100: 'LOAD_CONST', 101: 'LOAD_NAME', 102: 'BUILD_TUPLE',
    103: 'BUILD_LIST', 104: 'BUILD_MAP', 105: 'LOAD_ATTR', 106: 'COMPARE_OP', 107: 'IMPORT_NAME', 108: 'IMPORT_FROM',
    110: 'JUMP_FORWARD', 111: 'JUMP_IF_FALSE', 112: 'JUMP_IF_TRUE', 113: 'JUMP_ABSOLUTE', 114: 'FOR_LOOP',
    116: 'LOAD_GLOBAL', 120: 'SETUP_LOOP', 121: 'SETUP_EXCEPT', 122: 'SETUP_FINALLY', 124: 'LOAD_FAST',
    125: 'STORE_FAST', 126: 'DELETE_FAST', 127: 'SET_LINENO', 130: 'RAISE_VARARGS', 131: 'CALL_FUNCTION',
    132: 'MAKE_FUNCTION', 133: 'BUILD_SLICE'}
CMP = ['<', '<=', '==', '!=', '>', '>=', 'in', 'not in', 'is', 'is not']
BINOP = {'BINARY_ADD': '+', 'BINARY_SUBTRACT': '-', 'BINARY_MULTIPLY': '*', 'BINARY_DIVIDE': '/', 'BINARY_MODULO': '%'}


class Code:
    def __init__(self, **kw):
        self.__dict__.update(kw)


class Long(int):
    pass


class Marshal:
    """the marshal format of Python 1.5.2 (Python/marshal.c)"""
    def __init__(self, data):
        self.d, self.p = data, 0

    def byte(self):
        self.p += 1
        return self.d[self.p - 1]

    def int32(self):
        self.p += 4
        return struct.unpack('<i', self.d[self.p - 4:self.p])[0]

    def short(self):
        self.p += 2
        return struct.unpack('<h', self.d[self.p - 2:self.p])[0]

    def obj(self):
        t = chr(self.byte())
        if t == '0':
            return None
        if t == 'N':
            return None
        if t == 'i':
            return self.int32()
        if t == 'I':
            self.p += 8
            return struct.unpack('<q', self.d[self.p - 8:self.p])[0]
        if t == 'l':
            n = self.int32()
            v = 0
            for i in range(abs(n)):
                v |= (self.short() & 0x7fff) << (15 * i)
            return Long(-v if n < 0 else v)
        if t == 'f':
            n = self.byte()
            self.p += n
            return ('float', self.d[self.p - n:self.p].decode('ascii'))
        if t == 's':
            n = self.int32()
            self.p += n
            return self.d[self.p - n:self.p]
        if t in '([':
            n = self.int32()
            v = [self.obj() for _ in range(n)]
            return tuple(v) if t == '(' else v
        if t == '{':
            d = {}
            while self.d[self.p] != ord('0'):
                k = self.obj()
                d[k] = self.obj()
            self.p += 1
            return d
        if t == 'c':
            c = Code(argcount=self.short(), nlocals=self.short(), stacksize=self.short(), flags=self.short())
            c.code, c.consts, c.names, c.varnames = self.obj(), self.obj(), self.obj(), self.obj()
            c.filename, c.name = self.obj(), self.obj()
            c.firstlineno = self.short()
            c.lnotab = self.obj()
            return c
        raise ValueError('marshal type %r' % t)


def strrepr(b):
    """repr of a string in Python 1.5.2 (Objects/stringobject.c string_repr)"""
    q = "'"
    if b"'" in b and b'"' not in b:
        q = '"'
    out = [q]
    for c in b:
        ch = chr(c)
        if ch == q or ch == '\\':
            out.append('\\' + ch)
        elif ch == '\t':
            out.append('\\t')
        elif ch == '\n':
            out.append('\\n')
        elif ch == '\r':
            out.append('\\r')
        elif c < 0x20 or c >= 0x7f:
            out.append('\\%03o' % c)
        else:
            out.append(ch)
    out.append(q)
    return ''.join(out)


def const(v):
    if v is None:
        return 'None'
    if isinstance(v, tuple) and len(v) == 2 and v[0] == 'float':
        return v[1] if any(ch in v[1] for ch in '.eEn') else v[1] + '.0'
    if isinstance(v, Long):
        return '%dL' % v
    if isinstance(v, int):
        return str(v)
    if isinstance(v, bytes):
        return strrepr(v)
    raise Unsupported('constant %r' % (v,))


class Unsupported(Exception):
    pass


class E:
    """a value on the symbolic stack: the text of an expression (changed to the name it was stored in, for the copy
    DUP_TOP left), a dict being built, an imported module, a code object or a function"""
    def __init__(self, text, kind='expr', **kw):
        self.text, self.kind = text, kind
        self.__dict__.update(kw)


def instrs(code):
    b, i, out = code.code, 0, []
    while i < len(b):
        op = b[i]
        name = OPS.get(op, 'OP%d' % op)
        if op >= 90:
            out.append((i, name, b[i + 1] | (b[i + 2] << 8)))
            i += 3
        else:
            out.append((i, name, None))
            i += 1
    return out


class Decoder:
    def __init__(self, code, module):
        self.code, self.module = code, module
        self.ins = instrs(code)
        self.at = dict((off, k) for k, (off, _, _) in enumerate(self.ins))
        self.line = code.firstlineno

    def fail(self, why):
        raise Unsupported('line %d: %s' % (self.line, why))

    def block(self, start, end, out, indent):
        """the statements of the instructions [start, end) into out; returns the index where it stopped"""
        stack = []
        k = self.at[start]
        while k < len(self.ins) and self.ins[k][0] < end:
            k = self.step(k, stack, out, indent)
        if stack:
            self.fail('values left on the stack: %s' % ', '.join(s.text for s in stack))
        return k

    def expr_until(self, k, end, stack):
        """runs the instructions from index k up to offset end on stack (an expression), returns the index"""
        while self.ins[k][0] < end:
            k = self.step(k, stack, None, '')
        return k

    def emit(self, out, indent, text):
        if out is None:
            self.fail('a statement inside an expression: ' + text)
        out.append(indent + text)

    def step(self, k, st, out, ind):
        off, op, arg = self.ins[k]
        nxt = self.ins[k + 1][0] if k + 1 < len(self.ins) else len(self.code.code)
        c, names = self.code.consts, self.code.names
        if op == 'SET_LINENO':
            self.line = arg
        elif op == 'LOAD_CONST':
            v = c[arg]
            st.append(E('<code %s>' % v.name.decode(), 'code', code=v) if isinstance(v, Code) else E(const(v)))
        elif op in ('LOAD_NAME', 'LOAD_GLOBAL'):
            st.append(E(names[arg].decode()))
        elif op == 'LOAD_ATTR':
            st.append(E('%s.%s' % (self.val(st.pop()), names[arg].decode())))
        elif op in ('BUILD_TUPLE', 'BUILD_LIST'):
            items = [self.val(s) for s in st[len(st) - arg:]] if arg else []
            del st[len(st) - arg:]
            if op == 'BUILD_LIST':
                st.append(E('[' + ', '.join(items) + ']'))
            else:
                st.append(E('(' + ', '.join(items) + (',)' if len(items) == 1 else ')')))
        elif op == 'BUILD_MAP':
            st.append(E(None, 'dict', items=[]))
        elif op == 'DUP_TOP':
            st.append(st[-1])
        elif op == 'ROT_TWO':
            st[-1], st[-2] = st[-2], st[-1]
        elif op == 'STORE_SUBSCR':
            key, obj, val = st.pop(), st.pop(), st.pop()
            if obj.kind == 'dict':
                obj.items.append((self.val(key), self.val(val)))
            else:
                self.emit(out, ind, 'set %s[%s] = %s' % (self.val(obj), self.val(key), self.val(val)))
        elif op == 'BINARY_SUBSCR':
            i, o = st.pop(), st.pop()
            st.append(E('%s[%s]' % (self.val(o), self.val(i))))
        elif op.startswith('SLICE'):
            n = int(op[6:] or 0)
            hi = self.val(st.pop()) if n & 2 else ''
            lo = self.val(st.pop()) if n & 1 else ''
            st.append(E('%s[%s:%s]' % (self.val(st.pop()), lo, hi)))
        elif op in BINOP:
            b, a = st.pop(), st.pop()
            st.append(E('(%s %s %s)' % (self.val(a), BINOP[op], self.val(b))))
        elif op == 'UNARY_NEGATIVE':
            st.append(E('(-%s)' % self.val(st.pop())))
        elif op == 'UNARY_NOT':
            st.append(E('(not %s)' % self.val(st.pop())))
        elif op == 'UNARY_CONVERT':
            st.append(E('repr(%s)' % self.val(st.pop())))
        elif op == 'COMPARE_OP':
            b, a = st.pop(), st.pop()
            st.append(E('(%s %s %s)' % (self.val(a), CMP[arg], self.val(b))))
        elif op == 'CALL_FUNCTION':
            na, nk = arg & 0xff, arg >> 8
            kw = []
            for _ in range(nk):
                v, key = st.pop(), st.pop()
                kw.insert(0, '%s=%s' % (key.text.strip("'\""), self.val(v)))
            args = [self.val(st.pop()) for _ in range(na)][::-1]
            f = st.pop()
            if f.kind == 'func':
                if na or nk:
                    self.fail('call of a function definition')
                st.append(E(None, 'classbody', code=f.code))
            else:
                st.append(E('%s(%s)' % (self.val(f), ', '.join(args + kw))))
        elif op == 'MAKE_FUNCTION':
            code = st.pop()
            defaults = [st.pop() for _ in range(arg)]
            for d in defaults:
                if not self.plain(d.text):
                    self.fail('a default value with a call: ' + d.text)
            st.append(E(None, 'func', code=code.code))
        elif op == 'BUILD_CLASS':
            body, bases, name = st.pop(), st.pop(), st.pop()
            st.append(E(None, 'class', name=name.text.strip("'"), bases=bases.text, body=body.code))
        elif op == 'IMPORT_NAME':
            st.append(E(names[arg].decode(), 'module', froms=[]))
        elif op == 'IMPORT_FROM':
            st[-1].froms.append(names[arg].decode())
        elif op == 'STORE_NAME' or op == 'STORE_GLOBAL':
            name = names[arg].decode()
            v = st.pop()
            if v.kind == 'module':
                if name != v.text:
                    self.fail('import %s as %s' % (v.text, name))
                self.emit(out, ind, 'import ' + name)
            elif v.kind == 'func':
                if v.code.name.decode() != name:
                    self.fail('def %s stored as %s' % (v.code.name.decode(), name))
                self.emit(out, ind, 'def ' + name)
            elif v.kind == 'class':
                if v.name != name:
                    self.fail('class %s stored as %s' % (v.name, name))
                bases = v.bases
                if bases == '()':
                    bases = ''
                elif bases.endswith(',)'):
                    bases = bases[:-2] + ')'
                self.emit(out, ind, 'class %s%s' % (name, bases))
                Decoder(v.body, self.module).body(out, ind + '\t', True)
                self.emit(out, ind, 'end')
            else:
                self.emit(out, ind, 'let %s = %s' % (name, self.val(v)))
                v.text = name
        elif op == 'STORE_ATTR':
            o, v = st.pop(), st.pop()
            self.emit(out, ind, 'set %s.%s = %s' % (self.val(o), names[arg].decode(), self.val(v)))
        elif op == 'DELETE_NAME':
            self.emit(out, ind, 'del ' + names[arg].decode())
        elif op == 'POP_TOP':
            v = st.pop()
            if v.kind == 'module':
                if not v.froms:
                    self.fail('import without a name')
                self.emit(out, ind, 'from %s import %s' % (v.text, ' '.join(v.froms)))
            else:
                self.emit(out, ind, 'call ' + self.val(v))
        elif op in ('JUMP_IF_FALSE', 'JUMP_IF_TRUE'):
            target = nxt + arg
            if self.ins[k + 1][1] != 'POP_TOP':
                self.fail('a jump without POP_TOP')
            tk = self.at[target]
            if out is not None and len(st) == 1 and self.ins[tk][1] == 'POP_TOP' and op == 'JUMP_IF_FALSE' and \
                    self.ins[tk - 1][1] == 'JUMP_FORWARD':
                # if <cond>: <body> [else: <body>]: JUMP_FORWARD over the else part, POP_TOP of the condition there
                cond = st.pop()
                jf = self.ins[tk - 1]
                anchor = jf[0] + 3 + jf[2]
                self.emit(out, ind, 'if ' + self.val(cond))
                self.block(self.ins[k + 2][0], jf[0], out, ind + '\t')
                if tk + 1 < len(self.ins) and self.ins[tk + 1][0] < anchor:
                    els = []
                    self.block(self.ins[tk + 1][0], anchor, els, ind + '\t')
                    if els:
                        self.emit(out, ind, 'else')
                        out.extend(els)
                self.emit(out, ind, 'end')
                return self.at[anchor]
            # a and b, a or b: the value stays when the jump is taken
            a = st.pop()
            sub = []
            self.expr_until(k + 2, target, sub)
            if len(sub) != 1:
                self.fail('and/or with %d values' % len(sub))
            st.append(E('(%s %s %s)' % (self.val(a), 'and' if op == 'JUMP_IF_FALSE' else 'or', self.val(sub[0]))))
            return tk
        elif op == 'RETURN_VALUE':
            st.pop()
        elif op == 'LOAD_LOCALS':
            st.append(E('<locals>'))
        else:
            self.fail(op)
        return k + 1

    def val(self, e):
        if e.kind == 'dict':
            return '{' + ', '.join('%s: %s' % kv for kv in e.items) + '}'
        if e.kind != 'expr':
            self.fail('%s used as a value' % e.kind)
        return e.text

    @staticmethod
    def plain(text):
        """a default value without a call: a constant, a name or an attribute (the C function has its own defaults;
        the name was bound before the def, as the script needs it)"""
        return '(' not in text.lstrip('(') or text[:1] in '\'"0123456789-['

    def body(self, out, indent, cls=False):
        """the statements of a module or class body; the module ends with return None, a class with LOAD_LOCALS"""
        n = len(self.ins)
        end = self.ins[n - 2][0]
        if cls:
            if self.ins[n - 2][1] != 'LOAD_LOCALS' or self.ins[n - 1][1] != 'RETURN_VALUE':
                self.fail('class body without LOAD_LOCALS')
        elif self.ins[n - 2][1] != 'LOAD_CONST' or self.ins[n - 1][1] != 'RETURN_VALUE':
            self.fail('module code without return None at its end')
        self.block(0, end, out, indent)


def convert(module, path):
    data = open(path, 'rb').read()
    code = Marshal(data[8:]).obj()
    out = []
    Decoder(code, module).body(out, '')
    return out


def wrap(line, width=118):
    """a long let line with one item of its outer list, tuple or dict per line (the loader reads lines as one while
    brackets are open)"""
    if len(line) <= width:
        return line
    head, eq, value = line.partition(' = ')
    if not eq or value[:1] not in '[({':
        return line
    parts, depth, cur, q, esc = [], 0, '', None, False
    for ch in value[1:-1]:
        cur += ch
        if q:
            if esc:
                esc = False
            elif ch == '\\':
                esc = True
            elif ch == q:
                q = None
            continue
        if ch in '\'"':
            q = ch
        elif ch in '([{':
            depth += 1
        elif ch in ')]}':
            depth -= 1
        elif ch == ',' and depth == 0:
            parts.append(cur.strip())
            cur = ''
    if cur.strip():
        parts.append(cur.strip())
    indent = '\t' * (len(head) - len(head.lstrip('\t')) + 1)
    return '%s = %s\n%s\n%s' % (head, value[0], '\n'.join(indent + p for p in parts), indent[:-1] + value[-1])


if __name__ == '__main__':
    try:
        lines = convert(sys.argv[1], sys.argv[2])
    except Unsupported as e:
        sys.stderr.write('%s: %s\n' % (sys.argv[1], e))
        sys.exit(2)
    print('\n'.join(wrap(l) for l in lines))
