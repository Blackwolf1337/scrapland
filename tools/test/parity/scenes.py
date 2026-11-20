#!/usr/bin/env python3
"""Plays scenes through the test verb "scene" and waits for each end (include/scrap/netscene.h, stage 4.5 of the
second multiplayer design; the games run with the test channel, SCRAP_SCENE_SHOTS takes their screenshots).

  scenes.py sp <game> <scene>...                        single player
  scenes.py net <instance game> <level> <player|all> --clients <game>,<game> <scene>... [--out DIR]
      on a -mp2 instance (player: the context the scene starts in, sandbox); after each scene the instance's audit
      (pres.unforwarded, pres.double, scene keys) since the scene's start, each client's (pres.double, pres.missing,
      client.fail), and the mirror diff of each client (tools/test/parity/mirror.py) into <out>/<scene>.json
  scenes.py auto <game> [<level>]                       the scenes the level plays itself at its start (Fake, ABIndoor):
      waits until one plays and until none has played for 15 s (an instance's with its level)
Waits are game time (gameclock.py, SCRAP_TIMESCALE)."""
import json, os, subprocess, sys, time
R = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
sys.path.insert(0, os.path.join(R, 'tools', 'test'))
import channel, gameclock  # noqa: E402

def ask(game, lines, inst=None, timeout=20.0):
    if inst:
        channel.use_instance(inst)
    else:
        channel.CMD, channel.REPLY, channel.TMP = 'test_cmd.txt', 'test_reply.txt', 'test_cmd.tmp'
    seq = channel.write(game, '\n'.join(lines))
    return channel.answer(game, seq, timeout=timeout)

def audit(game, inst=None):
    out = ask(game, ['audit json'], inst)
    try:
        return json.loads(out.strip().splitlines()[-1])
    except Exception:
        return {}

def keys(a, prefixes):
    res = {}
    for k, v in (a.get('keys') or a).items() if isinstance(a, dict) else []:
        name = k if isinstance(k, str) else str(k)
        if any(name.startswith(p) or ('\t' + p) in name for p in prefixes):
            res[name] = v
    return res

def playing(game, inst=None):
    st = ask(game, ['scene state'], inst)
    if inst:
        return any(l.startswith('server\t') and not l.startswith('server\t-') for l in st.splitlines())
    return any(l.startswith('time\t') for l in st.splitlines())

def wait_end(game, inst=None, limit=240.0):
    t0 = time.time()
    while time.time() - t0 < gameclock.secs(limit) + 30:
        if not playing(game, inst):
            return True
        time.sleep(0.25)
    return False

def auto(game, inst):
    t0 = time.time()
    seen, idle = [], None
    while time.time() - t0 < gameclock.secs(600) + 60:
        st = ask(game, ['scene state'], inst)
        cur = [l.split('\t')[1] for l in st.splitlines() if l.startswith('time\t')]
        if inst:
            cur = [l.split('\t')[2] for l in st.splitlines() if l.startswith('server\t') and not l.startswith('server\t-')]
        if cur:
            if not seen or seen[-1] != cur[0]:
                seen.append(cur[0])
                print('playing', cur[0], flush=True)
            idle = None
        elif seen:
            idle = idle or time.time()
            if time.time() - idle > gameclock.secs(15):
                break
        time.sleep(0.25)
    print('auto: %d scenes: %s' % (len(seen), ' '.join(seen)), flush=True)

def main():
    a = sys.argv[1:]
    if a[0] == 'auto':
        auto(a[1], a[2] if len(a) > 2 else None)
        return
    if a[0] == 'sp':
        game = a[1]
        for sc in a[2:]:
            r = ask(game, ['scene play %s' % sc])
            print(sc, r.strip(), flush=True)
            if 'playing' in r:
                print(sc, 'ended' if wait_end(game) else 'TIMEOUT', flush=True)
            gameclock.sleep(2)
        return
    game, level, player = a[1], a[2], a[3]
    rest = a[4:]
    clients, out = [], None
    scenes = []
    i = 0
    while i < len(rest):
        if rest[i] == '--clients':
            clients = rest[i + 1].split(',')
            i += 2
        elif rest[i] == '--out':
            out = rest[i + 1]
            i += 2
        else:
            scenes.append(rest[i])
            i += 1
    if out:
        os.makedirs(out, exist_ok=True)
    for sc in scenes:
        ask(game, ['audit reset'], level)
        for c in clients:
            ask(c, ['audit reset'])
        r = ask(game, ['scene play %s%s' % (sc, '' if player == 'all' else ' ' + player)], level)
        print(sc, r.strip(), flush=True)
        res = {'scene': sc, 'started': 'playing' in r}
        if res['started']:
            res['ended'] = wait_end(game, level)
            gameclock.sleep(3)
        ia = ask(game, ['audit'], level)
        res['instance'] = [l for l in ia.splitlines() if 'pres.unforwarded' in l or 'pres.double' in l or 'scene.' in l]
        res['clients'] = []
        for c in clients:
            ca = ask(c, ['audit'])
            cl = {'game': c, 'audit': [l for l in ca.splitlines() if 'pres.double' in l or 'pres.missing' in l or
                                       'client.fail' in l or 'scene.' in l or 'client.sim.call' in l]}
            m = subprocess.run([sys.executable, os.path.join(R, 'tools/test/parity/mirror.py'), '--game', c,
                                '--instance', game, '--level', level, '--samples', '2', '--gap', '1', '--show', '10'],
                               capture_output=True, text=True, timeout=300)
            cl['mirror_rc'] = m.returncode
            cl['mirror'] = m.stdout.strip().splitlines()[-12:]
            res['clients'].append(cl)
        unf = sum(int(l.split('\t')[0]) for l in res['instance'] if 'pres.unforwarded' in l)
        dbl = sum(int(l.split('\t')[0]) for l in res['instance'] if 'pres.double' in l) + sum(
            int(l.split('\t')[0]) for c in res['clients'] for l in c['audit'] if 'pres.double' in l)
        res['unforwarded'], res['double'] = unf, dbl
        print('%s: started %s ended %s unforwarded %d double %d mirror %s' % (sc, res['started'], res.get('ended'), unf,
              dbl, [c['mirror_rc'] for c in res['clients']]), flush=True)
        if out:
            json.dump(res, open(os.path.join(out, sc + '.json'), 'w'), indent=1)

main()
