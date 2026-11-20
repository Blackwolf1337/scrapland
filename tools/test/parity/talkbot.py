#!/usr/bin/env python3
"""A bot that talks to characters, in single player or as a -mp2 client (stage 4.3 gate).

  talkbot.py sp <sp game> <npc>[:<nexts>,<nexts>,...] ...
  talkbot.py mp <instance game> <level> <client game> <player> <npc>[:<nexts>,...] ...

Per character: the player is kept next to it (test verb "talk follow" of single player or of the instance) until its
arrow selects it, then for each entry of the list (default "0,0,0": three uses) it presses the next option that many times
and then use, each through the input (test verb "tape key Walk.<action> 1": single player's input, or a client's input
that goes to the server), WAIT seconds apart (env, default 12). Each use is marked in the games' talk traces ("talk
... mark <npc> use <k>", SCRAP_TALK_TRACE=1), so talkcmp.py splits the traces per character and use. A player that is
not selected by the character's arrow after about ten seconds gets "mark <npc> use <k>: not selected (<selected>)".
"""
import os
import sys
import time

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
import channel  # noqa: E402

WAIT = float(os.environ.get('WAIT', '12'))


def ask(game, lines, inst=None, timeout=20.0):
    save = channel.CMD, channel.REPLY, channel.TMP
    if inst:
        channel.use_instance(inst)
    try:
        return channel.send(game, '\n'.join(lines) if isinstance(lines, list) else lines, timeout=timeout)
    finally:
        channel.CMD, channel.REPLY, channel.TMP = save


class Side:
    def __init__(self, argv):
        self.mode = argv[0]
        if self.mode == 'sp':
            self.game, self.inst, self.client, self.player = argv[1], None, argv[1], 0
            self.npcs = argv[2:]
            self.me = 'Player0'
        else:
            self.game, self.inst, self.client, self.player = argv[1], argv[2], argv[3], int(argv[4])
            self.npcs = argv[5:]
            self.me = None

    def own(self):
        if self.me:
            return self.me
        # the instance's entity of the player: NetPlayer<i>_<n>, the newest one that exists
        for n in range(8, 0, -1):
            r = ask(self.game, 'ent NetPlayer%d_%d Name' % (self.player, n), self.inst)
            if 'NetPlayer' in r:
                self.me = 'NetPlayer%d_%d' % (self.player, n)
                return self.me
        return 'NetPlayer%d_1' % self.player

    def follow(self, npc, dx=120):
        ask(self.game, 'talk follow %d %s %d 0' % (self.player, npc, dx), self.inst)

    def selected(self):
        r = ask(self.game, 'ent %s AutoSelectedEntity' % self.own(), self.inst)
        try:
            return eval(r)[0][1]
        except Exception:
            return ''

    def key(self, action, hold=0.25):
        ask(self.client, 'tape key Walk.%s 1' % action)
        time.sleep(hold)
        ask(self.client, 'tape key Walk.%s 0' % action)

    def mark(self, text):
        print('talkbot: %.3f %s' % (time.time(), text), flush=True)
        ask(self.client, 'talk mark ' + text)
        if self.inst:
            ask(self.game, 'talk mark ' + text, self.inst)


def main():
    s = Side(sys.argv[1:])
    for spec in s.npcs:
        npc, _, plan = spec.partition(':')
        nexts = [int(x) for x in (plan or '0,0,0').split(',')]
        s.follow(npc)
        for u, nx in enumerate(nexts):
            sel = ''
            for attempt in range(12):
                sel = s.selected()
                if sel == npc:
                    break
                if attempt in (4, 8):
                    s.follow(npc, (90, 150)[attempt // 4 - 1])
                time.sleep(0.8)
            if sel != npc:
                s.mark('%s use %d: not selected (%s)' % (npc, u, sel))
                continue
            for _ in range(nx):
                s.key('NextAction', 0.15)
                time.sleep(0.3)
            s.mark('%s use %d' % (npc, u))
            s.key('Use')
            time.sleep(WAIT)
        s.follow('-')
        s.mark('%s done' % npc)


if __name__ == '__main__':
    main()
