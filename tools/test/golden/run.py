#!/usr/bin/env python3
"""The golden test through the test channel (tools/test/golden.sh, GOLDEN_MODE=channel): the game runs with
SCRAP_TEST_CHANNEL=1 and no Python of the test.
  run.py prepare <game folder>             before the start: sdtest.cmd (the API checks) as the command file, so the
                                           game runs it in its first frame (the world clock at 0, as -PythonExecute ran
                                           sdtest.py while the menu loaded)
  run.py collect <game folder> <timeout>   the answers into <game folder>/sdtest.out (the transforms of a third field
                                           done here), then the interpreter checks (sdpy.py) through the channel's py
                                           command (STAGE 7: they go with the interpreter), their lines, then done.
"""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
import channel  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))


def transform(kind, value):
    """the comparisons of sdlib.py's helpers on an answer (an exception stays as it is)"""
    if value.startswith('exception '):
        return value
    if kind == 'ge0':
        return '1' if int(value) >= 0 else '0'
    if kind == 'nonempty':
        return value if value[:1] not in '\'"' else ('0' if value in ("''", '""') else '1')
    if kind == 'modes':
        n = int(value)
        return '(%d, %d)' % (n > 0, n % 2)
    raise SystemExit('run.py: unknown transform %s' % kind)


# the engine variable the old way set on its command line (-PythonExecute:execfile('sdtest.py')); Scrap.Get of it is
# one of the checks
PREAMBLE = ["do Scrap.Set('PythonExecute', 'execfile(%s)')" % '+'.join('chr(%d)' % ord(c) for c in 'sdtest.py')]
SEQ = 1


def commands():
    cmds, kinds = list(PREAMBLE), [None] * len(PREAMBLE)
    for line in open(os.path.join(HERE, 'sdtest.cmd'), encoding='latin1'):
        line = line.rstrip('\n')
        f = line.split('\t')
        if line.startswith('check ') and len(f) == 3:
            line = '\t'.join(f[:2])
            kinds.append(f[2])
        else:
            kinds.append(None)
        cmds.append(line)
    return cmds, kinds


def prepare(game):
    for n in ('test_cmd.txt', 'test_reply.txt', 'sdpy.out'):
        try:
            os.remove(os.path.join(game, n))
        except OSError:
            pass
    for n in ('sdlib.py', 'sdpy.py'):
        with open(os.path.join(HERE, n), 'rb') as src, open(os.path.join(game, n), 'wb') as dst:
            dst.write(src.read())
    channel.write(game, '\n'.join(commands()[0]), SEQ)


def collect(game, timeout):
    cmds, kinds = commands()
    answers = channel.answer(game, SEQ, timeout).split('\n')
    out, i = [], 0
    for cmd, kind in zip(cmds, kinds):
        if not (cmd.startswith('check ') or cmd.startswith('print ')):
            # do and recorder answer only an error
            if i < len(answers) and answers[i].startswith('error: '):
                out.append(answers[i])
                i += 1
            continue
        a = answers[i] if i < len(answers) else '(no answer)'
        i += 1
        if kind:
            name, _, value = a.partition(' = ')
            a = '%s = %s' % (name, transform(kind, value))
        out.append(a)
    out += answers[i:]
    with open(os.path.join(game, 'sdtest.out'), 'w') as f:
        f.write('\n'.join(out) + '\n')
    r = channel.send(game, "py\nOUT = 'sdpy.out'\nexecfile('sdlib.py')\nexecfile('sdpy.py')\nfinish()\nresult = 'ok'",
                     timeout=timeout)
    if r != "'ok'":
        print('golden: the interpreter checks: %s' % r, file=sys.stderr)
    try:
        with open(os.path.join(game, 'sdpy.out')) as f:
            tail = f.read()
    except OSError:
        tail = ''
    with open(os.path.join(game, 'sdtest.out'), 'a') as f:
        f.write(tail)


if sys.argv[1] == 'prepare':
    prepare(sys.argv[2])
else:
    collect(sys.argv[2], float(sys.argv[3]))
