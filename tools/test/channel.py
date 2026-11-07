#!/usr/bin/env python3
"""The tool side of the test channel of the game (src/game/Global/TestChannel.c; the game runs with -TestChannel or
SCRAP_TEST_CHANNEL=1): writes test_cmd.txt in the game folder ("<number>\\n<commands>", through a temporary file and a
rename) and waits for test_reply.txt with the same number ("<number>\\n<answers>\\n.end\\n"). The game looks once a
frame, also while the world clock stands, but not during a level load or the start video.

  channel.py <game folder> <line> ...         runs the lines (one command file), prints the answers
  channel.py <game folder> -f <file>          the lines of a file
  channel.py <game folder> --wait [seconds]   waits until the game answers (the menu or the level runs)

Used by tools/mcp/scrap_mcp.py and tools/test/golden.sh."""
import os
import sys
import time

CMD, REPLY, TMP = 'test_cmd.txt', 'test_reply.txt', 'test_cmd.tmp'


class NoAnswer(RuntimeError):
    pass


def next_seq(game):
    """a number the game has not answered yet: one more than the number of the last answer"""
    try:
        with open(os.path.join(game, REPLY), 'r', errors='replace') as f:
            return int(f.readline().strip() or 0) + 1
    except (OSError, ValueError):
        return int(time.time()) % 100000


def write(game, body, seq=None, pace=None):
    """writes the command file (through a temporary file and a rename); its number. pace: the game runs it only after
    that many seconds of game time since its last answer (as the old harness answered every 0.1 s of game time)"""
    seq = next_seq(game) if seq is None else seq
    tmp = os.path.join(game, TMP)
    with open(tmp, 'w', newline='\n') as f:
        f.write('%d%s\n%s\n' % (seq, ' pace=%g' % pace if pace else '', body))
    os.replace(tmp, os.path.join(game, CMD))
    return seq


def answer(game, seq, timeout=10.0, alive=None, poll=0.01):
    """waits for the answers to the command file of number seq (without the end mark). alive: a function that tells
    whether the game still runs (stops the wait early)"""
    end = time.time() + timeout
    want = str(seq)
    while time.time() < end:
        try:
            with open(os.path.join(game, REPLY), 'r', errors='replace') as f:
                data = f.read().replace('\r\n', '\n')
        except OSError:
            data = ''
        head, _, rest = data.partition('\n')
        if head.strip() == want and rest.endswith('.end\n'):
            return rest[:-len('.end\n')].rstrip('\n')
        if alive is not None and not alive():
            break
        time.sleep(poll)
    raise NoAnswer('no answer from the game in %s in %.0f s' % (game, timeout))


def send(game, body, timeout=10.0, seq=None, alive=None, poll=0.01, pace=None):
    """runs body (one or more command lines, or "py\\n<code>") in the game; the answers"""
    return answer(game, write(game, body, seq, pace), timeout, alive, poll)


def wait_ready(game, timeout=120.0, alive=None):
    """waits until the game answers; the level path"""
    end = time.time() + timeout
    while time.time() < end:
        if alive is not None and not alive():
            break
        try:
            return send(game, 'Scrap.GetLevelPath()', timeout=2.0, alive=alive)
        except NoAnswer:
            pass
    raise NoAnswer('the game in %s did not answer in %.0f s' % (game, timeout))


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    game = sys.argv[1]
    if sys.argv[2] == '--wait':
        print(wait_ready(game, float(sys.argv[3]) if len(sys.argv) > 3 else 120.0))
        return
    if sys.argv[2] == '-f':
        body = open(sys.argv[3]).read().rstrip('\n')
    else:
        body = '\n'.join(sys.argv[2:])
    print(send(game, body, timeout=float(os.environ.get('CHANNEL_TIMEOUT', 30))))


if __name__ == '__main__':
    main()
