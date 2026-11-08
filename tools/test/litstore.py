#!/usr/bin/env python3
"""The stored form of tools/test/litfuzz.sh: the texts of one full run of litfuzz.py in the reference build with the
interpreter, each with what Python 1.5.2 made of it, checked against the native literal reader through the test
channel's native command (src/game/Global/TestNative.c) in a build without the interpreter.

  litstore.py save <game file> <store> <header>   the record of litfuzz.py's save() (in the game folder) as the store
  litstore.py check <game folder> <store>         every stored text through the channel: native, native str and
                                                  native check must answer the stored repr, str and check() texts, a
                                                  text 1.5.2 did not take must be refused; prints a summary line and
                                                  the first differences, exit code 1 when there are any

The store (tools/test/litfuzz.expected.xz) has a header line "# ..." and then a line per text: the repr of the text
in 1.5.2 form, a tab, and the repr of (repr, str, check() text) or None, as litfuzz.py writes them. The texts are
bytes of the game (latin-1 here)."""
import ast
import lzma
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import channel  # noqa: E402

REFUSED = 'error: native: not a literal of the native reader'
MARK = "'#m#'"
CHUNK = 1000


def entries(lines):
    """(text, (repr, str, check) or None) of the record lines"""
    out = []
    for n, line in enumerate(lines):
        if not line or line.startswith('#'):
            continue
        t, sep, v = line.partition('\t')
        if not sep:
            raise SystemExit('litstore: line %d has no tab' % (n + 1))
        out.append((ast.literal_eval(t), ast.literal_eval(v)))
    return out


def save(src, store, header):
    with open(src, encoding='latin-1') as f:
        lines = f.read().split('\n')
    n = len(entries(lines))
    with lzma.open(store, 'wt', encoding='latin-1', newline='\n') as f:
        f.write('# %s, %d texts\n' % (header, n))
        f.write('\n'.join(line for line in lines if line) + '\n')
    print('litfuzz: %d texts stored in %s' % (n, store))


def command(text):
    """the channel line prefix for a text: as it is, or escaped when it has a line end"""
    if '\r' in text:
        raise SystemExit('litstore: a stored text has a carriage return, the channel drops it: %r' % text)
    if '\n' in text:
        return 'native esc %s', text.replace('\\', '\\\\').replace('\n', '\\n')
    return 'native %s', text


def check(game, store):
    with lzma.open(store, 'rt', encoding='latin-1', newline='\n') as f:
        lines = f.read().split('\n')
    header = lines[0][2:] if lines and lines[0].startswith('# ') else '?'
    todo = entries(lines)
    bad, taken = [], 0
    for at in range(0, len(todo), CHUNK):
        part = todo[at:at + CHUNK]
        cmds = []
        for text, _ in part:
            form, arg = command(text)
            cmds += [form % arg, form % ('str ' + arg), form % ('check ' + arg), 'native ' + MARK]
        reply = channel.send(game, '\n'.join(cmds), timeout=120.0, enc='latin-1')
        got = ('\n' + reply + '\n').split('\n' + MARK + '\n')
        got[0] = got[0][1:]
        if got.pop() != '' or len(got) != len(part):
            bad.append(('answers', at, len(got), len(part)))
            break
        for (text, want), g in zip(part, got):
            if want is None:
                exp = '\n'.join([REFUSED] * 3)
            else:
                taken += 1
                exp = '\n'.join(want).replace('\r\n', '\n')
            if g != exp:
                bad.append((text, exp, g))
    print('litfuzz: stored texts %d (%s), taken %d, bad %d' % (len(todo), header, taken, len(bad)))
    for b in bad[:20]:
        print(repr(b))
    return 1 if bad else 0


if __name__ == '__main__':
    if len(sys.argv) == 5 and sys.argv[1] == 'save':
        save(sys.argv[2], sys.argv[3], sys.argv[4])
    elif len(sys.argv) == 4 and sys.argv[1] == 'check':
        sys.exit(check(sys.argv[2], sys.argv[3]))
    else:
        sys.exit(__doc__)
