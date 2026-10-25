#!/usr/bin/env python3
"""List the string literals of C sources that belong to exactly one function.

Used to name the functions of statically linked open source libraries (Python 1.5.2, zlib, libpng, libjpeg) in
Scrap.exe: tools/ghidra/NameFromStrings.java looks the strings up in the exe. Output, tab separated:
    string (escaped like the TSV files of ExportSymbols.java)  function  source file
Handles ANSI and K&R definitions and the usual library export macros. A crude parser, not a C compiler: a top
level "{" whose header has a "(" and no "=" opens a function, the name is the identifier before the first "(" that
is not a known macro.

usage: srcstrings.py <source dir or .c file> ... > strings.tsv
"""
import os
import re
import sys

MACROS = {
    'METHODDEF', 'GLOBAL', 'LOCAL', 'EXTERN', 'JMETHOD', 'PNGAPI', 'ZEXPORT', 'ZEXPORTVA', 'OF', 'FAR',
    'staticforward', 'statichere', 'DL_EXPORT', 'DL_IMPORT', 'PNG_EXPORT', 'PNG_CONST', 'JPP', 'P', 'ARGS',
    'Py_PROTO', 'BGN_SAVE', 'local', 'const', 'static', 'extern', 'if', 'while', 'for', 'switch', 'return',
    'sizeof', 'PyObject_HEAD', 'PyObject_VAR_HEAD', 'int', 'char', 'void', 'long', 'short', 'unsigned', 'signed',
    'double', 'float', 'struct', 'union', 'register',
}
ESCAPES = {'n': '\n', 't': '\t', 'r': '\r', '0': '\0', 'a': '\a', 'b': '\b', 'f': '\f', 'v': '\v'}
TOKEN = re.compile(r'''
    (?P<comment>/\*.*?\*/|//[^\n]*)
  | (?P<pp>^[ \t]*\#(?:[^\n\\]|\\.)*)
  | (?P<str>"(?:[^"\\\n]|\\.)*")
  | (?P<chr>'(?:[^'\\\n]|\\.)*')
  | (?P<punct>[{}();=])
  | (?P<ident>[A-Za-z_][A-Za-z0-9_]*)
  | (?P<other>.)
''', re.S | re.M | re.X)


def unquote(lit):
    out, i, s = [], 0, lit[1:-1]
    while i < len(s):
        c = s[i]
        if c != '\\':
            out.append(c)
            i += 1
            continue
        n = s[i + 1]
        if n in '01234567':
            m = re.match(r'[0-7]{1,3}', s[i + 1:]).group()
            out.append(chr(int(m, 8)))
            i += 1 + len(m)
        elif n == 'x':
            m = re.match(r'[0-9a-fA-F]+', s[i + 2:]).group()
            out.append(chr(int(m, 16) & 0xff))
            i += 2 + len(m)
        else:
            out.append(ESCAPES.get(n, n))
            i += 2
    return ''.join(out)


def esc(s):
    return s.replace('\\', '\\\\').replace('\t', '\\t').replace('\n', '\\n').replace('\r', '')


def scan(path):
    """yields (function, string) for every string literal inside a function body"""
    text = open(path, encoding='latin-1').read()
    # header: tokens since the last ";", segments: earlier headers since the last "}" (K&R parameter declarations
    # end with ";" before the "{")
    depth, func, header, segments, pending = 0, None, [], [], []
    for m in TOKEN.finditer(text):
        kind, tok = m.lastgroup, m.group()
        if kind in ('comment', 'pp') or tok.isspace():
            continue
        if kind != 'str' and pending:
            if func:
                yield func, ''.join(pending)
            pending = []
        if depth == 0:
            if tok == '{':
                func = None
                for seg in [header] + segments[::-1]:
                    if '=' in seg:
                        break
                    for i, t in enumerate(seg[:-1]):
                        if seg[i + 1] == '(' and re.match(r'[A-Za-z_]', t) and t not in MACROS:
                            func = t
                            break
                    if func:
                        break
                depth = 1
                header, segments = [], []
            elif tok == ';':
                segments.append(header)
                header = []
            elif tok == '}':
                header, segments = [], []
            else:
                header.append(tok)
            continue
        if kind == 'str':
            pending.append(unquote(tok))
        elif tok == '{':
            depth += 1
        elif tok == '}':
            depth -= 1
            if depth == 0:
                func = None


def main():
    seen = {}
    paths = []
    for arg in sys.argv[1:]:
        if os.path.isfile(arg):
            paths.append(arg)
        for dirpath, _, files in os.walk(arg):
            paths += [os.path.join(dirpath, f) for f in sorted(files) if f.endswith('.c')]
    for path in paths:
        for func, s in scan(path):
            if len(s) >= 6:
                seen.setdefault(s, set()).add((func, path))
    for s, owners in sorted(seen.items()):
        funcs = {fn for fn, _ in owners}
        if len(funcs) == 1:
            fn, src = sorted(owners)[0]
            print(f'{esc(s)}\t{fn}\t{src}')


if __name__ == '__main__':
    main()
