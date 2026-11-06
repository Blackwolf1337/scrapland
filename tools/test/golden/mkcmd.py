#!/usr/bin/env python3
"""Makes sdtest.cmd (the API checks of sdtest.py as test channel commands) from the output of the recorder sdrec.py:
  mkcmd.py <sdrec.out> > sdtest.cmd
(GOLDEN_MODE=record tools/test/golden.sh runs the recorder and this.) A check whose lambda made one engine call and gave
its result unchanged becomes "check <name>\\t<call>"; the others are in SPECIAL below. A third field "\\t<transform>"
is done by run.py on the answer (the comparisons of sdlib.py's helpers, which channel expressions do not have).
Goes with the interpreter (stage 7): then sdtest.cmd is the source of the API checks."""
import re
import sys

ITEM = "SScorer.GetItem(0, 'SDItem')"


def special(name, calls):
    """the command of a check the recorder could not turn into one call; None: left out (an interpreter check)"""
    if name in ('SScorer.GetItem(0, actual)', 'SScorer.New(0, SDItem, Text)'):
        return calls[0] + ' is not None'
    if name == 'Scrap.GetVideoCurrentMode()':
        return calls[0] + '\tge0'
    if name in ('Scrap.Get(R_LastDevice)', 'Scrap.Def(R_LastDevice)'):
        return calls[0] + '\tnonempty'
    if name == 'Scrap.SetLanguage(current)':
        return 'Scrap.SetLanguage(Scrap.GetLanguage())'
    if name == 'Scrap.Rand(0, 10) type':
        return 'type(%s).__name__' % calls[0]
    if name == 'Scrap.GetVideoModes(-1)':
        return 'Scrap.GetVideoModes(-1)\tmodes'
    if name in ('Scrap.GetVideoModes(0)', 'Scrap.GetVideoModes(1)'):
        return calls[0] + '[-9:]'
    if name in ('sdcall args', 'sdcall args again'):
        return 'sdcalls[-1]'
    if name in ('sdcall count', 'sdcall count after'):
        return 'len(sdcalls)'
    m = re.fullmatch(r'SDItem\.(\w+)( set)?', name)
    if m:
        return '%s.%s' % (ITEM, m.group(1))
    m = re.fullmatch(r'SDItem\.(\w+) = (.*)', name)
    if m:
        return '%s.%s = %s' % (ITEM, m.group(1), m.group(2))
    if name.startswith('Python int '):
        return None
    raise SystemExit('mkcmd.py: no command for the check %r (calls %r)' % (name, calls))


def main():
    out = []
    for line in open(sys.argv[1], encoding='latin1'):
        line = line.rstrip('\n')
        if line == 'done':
            break
        if line.startswith('Menu item: '):
            out.append('print ' + ITEM)
            continue
        f = line.split('\t')
        name, kind, calls = f[0], f[1], f[2:]
        calls = [c for c in calls if c]
        if name == 'Scrap.SetCallFunc(__main__.sdcall)' and not any(l.startswith('recorder ') for l in out):
            # the Python functions sdtest.py defines for the call builder, as channel recorders
            out += ['recorder __main__.sdcall sdcalls 7', 'recorder __main__.sdraise - raise ValueError sdraise',
                    'do __main__.sdnotcallable = 1']
        if kind == 'call':
            cmd = re.sub(r'<function check at [0-9a-f]+>', 'Scrap.GetTime', calls[0])
        else:
            cmd = special(name, calls)
            if cmd is None:
                continue
        out.append('check %s\t%s' % (name, cmd))
    sys.stdout.write('\n'.join(out) + '\n')


main()
