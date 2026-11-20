#!/usr/bin/env python3
"""Compares the talk traces (SCRAP_TALK_TRACE=1, include/scrap/netaudit.h) of a single player game and of a -mp2 client
and its instance, per character and use as talkbot.py marked them (stage 4.3 gate).

  talkcmp.py <sp log> <client log> <instance log> <player> [--json out.json]
  talkcmp.py cross <client log> <forbidden regex> [<instance log> <player> <regex of the player's own lines>]

The first form, per use "<npc> use <k>":
  - texts and voices: the conversation lines of single player (speech texts and times, the voices 0 and 1 of the talk:
    open, play, stop; the messages; the heads; the talk voices of the character talked to) against the client's, in
    order. The voices of the character's animations (Sounds/Stream/Anm_*, which its idle animations play at random)
    are only counted (anim_voices in the JSON). A voice name that differs only in its last number (the answer variant the game draws at random, InitAns2
    against InitAns3) counts as a variant, and the text spoken with that voice may differ then.
  - callbacks: single player's against the instance's lines of the player's context (p<player>).
  - money: the change of the use as talkbot.py marked it in single player and on the client.
  A use that differs where one game started no talk (still refused after talkbot.py's retries: a busy cop, a walking
  character), and every later use of that character, is "refused" (the arrow's options of the games part there), not
  a difference; the exit code counts only "DIFF".
  Lines of another player's context in the instance's segment are counted ("foreign").
The second form counts the client's conversation lines that match the forbidden regex (another player's talk) and,
with an instance log, the instance's lines matching the player's regex in another context than p<player>.
  talkcmp.py pair <client log> <instance log> <player>
the client's conversation lines (texts, the voices 0 and 1, messages) against the instance's lines of the player's
context: equal in order, so nothing of another player's talk reached it and nothing of its own is missing.
Exit 0 when everything is equal (variants allowed), 1 otherwise.
"""
import json
import re
import sys

LINE = re.compile(r'^talk (\S+) (\S+) (\S+) ?(.*)$')
MARK = re.compile(r'^(\S+) use (\d+)(: not selected.*)?$')
ANIM = re.compile(r'/Anm_')  # the voices of a character's animations (a desktop's laugh): played at random, counted apart
TALK_OPS = ('speech', 'voiceopen', 'voiceplay', 'voicestop', 'msg', 'head', 'talkvoice', 'cancel')


def parse(path):
    out = []
    with open(path, errors='replace') as f:
        for raw in f:
            m = LINE.match(raw.rstrip('\n'))
            if m:
                out.append((float(m.group(1)), m.group(2), m.group(3), m.group(4)))
    return out


MONEY = re.compile(r'^(\S+) money (\d+) (-?\d+)$')


def moneys(lines):
    """{(npc, use): the money a use changed} from talkbot.py's marks"""
    out = {}
    for ln in lines:
        m = MONEY.match(ln[3]) if ln[2] == 'mark' else None
        if m:
            out[(m.group(1), int(m.group(2)))] = int(m.group(3))
    return out


def segments(lines):
    """{(npc, use): [lines]} between a mark of a use and the next mark; skipped uses map to None"""
    segs, cur = {}, None
    for ln in lines:
        if ln[2] == 'mark':
            m = MARK.match(ln[3])
            cur = None
            if m:
                key = (m.group(1), int(m.group(2)))
                segs[key] = None if m.group(3) else []
                cur = key if not m.group(3) else None
            continue
        if cur is not None:
            segs[cur].append(ln)
    return segs


def talk_lines(seg, npc, ctx=None):
    """the conversation lines of a segment: (op, fields), the voices 0 and 1, the talk voices of npc, messages without
    repeats"""
    out = []
    for _, c, op, f in seg:
        if ctx and c != ctx:
            continue
        if op not in TALK_OPS:
            continue
        if op in ('voiceopen', 'voiceplay', 'voicestop') and f.split(' ')[0] not in ('0', '1'):
            continue
        if op == 'talkvoice' and (f.split(' ')[0] != npc or ANIM.search(f)):
            continue
        if op == 'msg' and out and out[-1] == (op, f):
            continue
        out.append((op, f))
    return out


def anims(seg, npc):
    return sum(1 for _, _, op, f in seg if op == 'talkvoice' and f.split(' ')[0] == npc and ANIM.search(f))


def callbacks(seg, ctx=None):
    return [f for _, c, op, f in seg if op == 'callback' and (ctx is None or c == ctx)]


def variant(a, b):
    return re.sub(r'\d+$', '#', a) == re.sub(r'\d+$', '#', b)


def compare(sp, cl):
    """(equal, variants, first difference) of two lists of conversation lines"""
    if len(sp) != len(cl):
        return False, 0, 'length %d against %d' % (len(sp), len(cl))
    keys = [{}, {}]  # the voice name per slot, single player and client
    slot = 0
    var = 0
    loose = set()
    for i, (a, b) in enumerate(zip(sp, cl)):
        if a[0] != b[0]:
            return False, var, 'line %d: %s against %s' % (i, a, b)
        op = a[0]
        if op == 'voiceopen':
            sa, na = a[1].split(' ', 1)
            sb, nb = b[1].split(' ', 1)
            keys[0][sa], keys[1][sb] = na, nb
            if sa != sb or (na != nb and not variant(na, nb)):
                return False, var, 'line %d: %s against %s' % (i, a, b)
            if na != nb:
                var += 1
                loose.add(sa)
            continue
        if op == 'voiceplay':
            slot = a[1]
        if op == 'speech' and a[1] != b[1]:
            if str(slot) in loose:
                continue
            return False, var, 'line %d: %s against %s' % (i, a, b)
        if op != 'speech' and a[1] != b[1]:
            return False, var, 'line %d: %s against %s' % (i, a, b)
    return True, var, ''


def main_cmp(argv):
    sp_log, cl_log, in_log, player = argv[0], argv[1], argv[2], int(argv[3])
    jout = argv[argv.index('--json') + 1] if '--json' in argv else None
    ctx = 'p%d' % player
    spl, cll = parse(sp_log), parse(cl_log)
    sps, cls, ins = segments(spl), segments(cll), segments(parse(in_log))
    spm, clm = moneys(spl), moneys(cll)
    rows, bad, refused = [], 0, {}
    for key in sorted(set(sps) | set(cls), key=lambda k: (list(sps).index(k) if k in sps else 999, k[1])):
        npc, use = key
        r = {'npc': npc, 'use': use}
        if sps.get(key) is None or cls.get(key) is None or ins.get(key) is None:
            r['state'] = 'skipped (%s)' % ', '.join(n for n, s in (('sp', sps), ('client', cls), ('instance', ins))
                                                    if s.get(key) is None)
            rows.append(r)
            continue
        a, b = talk_lines(sps[key], npc), talk_lines(cls[key], npc)
        eq, var, diff = compare(a, b)
        cba, cbi = callbacks(sps[key]), callbacks(ins[key], ctx)
        foreign = sum(1 for _, c, op, _ in ins[key] if c.startswith('p') and c != ctx and op != 'mark')
        r.update(lines=len(a), client_lines=len(b), equal=eq, variants=var, diff=diff,
                 anim_voices=(anims(sps[key], npc), anims(cls[key], npc)),
                 callbacks=len(cba), callbacks_equal=cba == cbi, foreign=foreign,
                 speech=[f for op, f in a if op == 'speech' and not f.startswith('0.00')])
        if not cba == cbi:
            r['callbacks_diff'] = '%s against %s' % (cba, cbi)
        r['money'] = (spm.get(key), clm.get(key))
        if r['money'][0] != r['money'][1]:
            r['callbacks_diff'] = r.get('callbacks_diff', '') + ' money %s against %s' % r['money']
        talked = [any(op == 'speech' and not f.startswith('0.00') for op, f in x) for x in (a, b)]
        r['state'] = 'ok' if eq and cba == cbi and not foreign and r['money'][0] == r['money'][1] else 'DIFF'
        if r['state'] != 'ok' and (refused.get(npc) or not all(talked)):
            # one of the games could not talk at this use or an earlier one of the character (a busy or walking
            # character after the retries of talkbot.py): the arrow's options of the two games part there
            r['state'] = 'refused'
        refused[npc] = refused.get(npc) or not all(talked) and any(talked)
        bad += r['state'] == 'DIFF'
        rows.append(r)
    for r in rows:
        if 'lines' not in r:
            print('%-22s %d  %s' % (r['npc'], r['use'], r['state']))
            continue
        print('%-22s %d  %-4s lines %2d/%2d variants %d callbacks %d %s foreign %d %s%s' % (
            r['npc'], r['use'], r['state'], r['lines'], r['client_lines'], r['variants'], r['callbacks'],
            'eq' if r['callbacks_equal'] else 'DIFF', r['foreign'], r['diff'], ' ' + r.get('callbacks_diff', '')))
    done = [r for r in rows if 'lines' in r]
    print('uses compared %d, equal %d, with variants %d, refused %d, differ %d, speech lines %d, skipped %d' % (
        len(done), sum(r['state'] == 'ok' for r in done), sum(r['variants'] > 0 for r in done),
        sum(r['state'] == 'refused' for r in done), sum(r['state'] == 'DIFF' for r in done),
        sum(len(r['speech']) for r in done), len(rows) - len(done)))
    if jout:
        with open(jout, 'w') as f:
            json.dump(rows, f, indent=1)
    return 1 if bad else 0


def main_cross(argv):
    cl_log, forbid = argv[0], re.compile(argv[1])
    lines = parse(cl_log)
    hits = [ln for ln in lines if ln[2] in TALK_OPS + ('callback',) and forbid.search(ln[3])]
    own = sum(1 for ln in lines if ln[2] in ('speech', 'voiceopen') and not ln[3].startswith('0.00'))
    print('client %s: %d conversation lines, %d of the other player' % (cl_log, own, len(hits)))
    for ln in hits[:10]:
        print('  ', ln)
    bad = len(hits)
    if len(argv) >= 5:
        in_log, ctx, mine = argv[2], 'p%d' % int(argv[3]), re.compile(argv[4])
        il = [ln for ln in parse(in_log) if ln[2] in TALK_OPS + ('callback',) and mine.search(ln[3])]
        wrong = [ln for ln in il if ln[1] != ctx]
        print('instance %s: %d lines of %s, %d in another context' % (in_log, len(il), ctx, len(wrong)))
        for ln in wrong[:10]:
            print('  ', ln)
        bad += len(wrong)
    return 1 if bad else 0


def own_lines(lines, ctx=None):
    """the conversation lines a player sees: speech texts, the voices 0 and 1 of the talk (open, play, stop), the
    messages (without repeats); with ctx only the instance's lines of that context"""
    out = []
    for _, c, op, f in lines:
        if ctx and c != ctx:
            continue
        if op not in ('speech', 'voiceopen', 'voiceplay', 'voicestop', 'msg'):
            continue
        if op.startswith('voice') and f.split(' ')[0] not in ('0', '1'):
            continue
        if op == 'msg' and (f == '""' or (out and out[-1] == (op, f))):
            continue
        out.append((op, f))
    return out


def after_mark(lines):
    """the lines from the first mark of the bot on (the client's own hints before it are no talk)"""
    k = next((i for i, ln in enumerate(lines) if ln[2] == 'mark'), len(lines))
    return lines[k:]


def main_pair(argv):
    """pair <client log> <instance log> <player>: the client shows exactly the conversation lines the instance made in
    the player's context, in the same order (no line of another player's talk, none of its own missing)"""
    cl, il, ctx = argv[0], argv[1], 'p%d' % int(argv[2])
    a = own_lines(after_mark(parse(cl)))
    b = own_lines(after_mark(parse(il)), ctx)
    n = 0
    while n < min(len(a), len(b)) and a[n] == b[n]:
        n += 1
    ok = len(a) == len(b) == n
    print('client %s against instance %s %s: %d and %d lines, %s' % (cl, il, ctx, len(a), len(b),
                                                                    'equal' if ok else 'first difference at %d' % n))
    if not ok:
        print('   client:  ', a[n:n + 3])
        print('   instance:', b[n:n + 3])
    return 0 if ok else 1


if __name__ == '__main__':
    if len(sys.argv) > 1 and sys.argv[1] == 'cross':
        sys.exit(main_cross(sys.argv[2:]))
    if len(sys.argv) > 1 and sys.argv[1] == 'pair':
        sys.exit(main_pair(sys.argv[2:]))
    if len(sys.argv) < 5:
        print(__doc__)
        sys.exit(2)
    sys.exit(main_cmp(sys.argv[1:]))
