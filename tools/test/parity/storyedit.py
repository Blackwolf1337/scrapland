#!/usr/bin/env python3
"""Edits the saved story of a coop world's data folder (a story point to start chap.sh from):
  storyedit.py <data dir> [--level L] [--entrance E] [--door D] [--del name ...] [name=value ...]
party.story and each player's <key>.story copy get the variables (name= with an empty value sets it empty; --del
removes it), each players/<key>.dat its level, entrance and door. The files keep the store format of the coordinator
(src/coord/Store.c: "scrapplayer 1", the lines, "sum " BLAKE2b-16 of everything before it)."""
import argparse
import glob
import hashlib
import os
import sys

HEAD = "scrapplayer 1\n"


def load(path):
    with open(path, encoding="latin-1") as f:
        t = f.read()
    if not t.startswith(HEAD):
        sys.exit(f"storyedit: {path} is no store file")
    body = t[len(HEAD):]
    at = body.rfind("sum ")
    return [l for l in body[:at].split("\n") if l]


def save(path, lines):
    t = HEAD + "".join(l + "\n" for l in lines)
    s = hashlib.blake2b(t.encode("latin-1"), digest_size=16).hexdigest()
    tmp = path + ".tmp"
    with open(tmp, "w", encoding="latin-1") as f:
        f.write(t + "sum " + s + "\n")
    os.replace(tmp, path)


def setvars(lines, sets, dels):
    out, seen = [], set()
    for l in lines:
        k = l.split("=", 1)[0]
        if k in dels:
            continue
        if k in sets:
            out.append(k + "=" + sets[k])
            seen.add(k)
        else:
            out.append(l)
    out += [k + "=" + v for k, v in sets.items() if k not in seen]
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("data")
    ap.add_argument("--level")
    ap.add_argument("--entrance")
    ap.add_argument("--door")
    ap.add_argument("--del", dest="dels", nargs="*", default=[])
    ap.add_argument("vars", nargs="*")
    a = ap.parse_args()
    sets = dict(v.split("=", 1) for v in a.vars)
    dels = set(a.dels)
    stories = [os.path.join(a.data, "party.story")] + glob.glob(os.path.join(a.data, "players", "*.story"))
    for p in stories:
        save(p, setvars(load(p), sets, dels))
        bak = p + "bak"
        if os.path.exists(bak):
            os.remove(bak)
    pl = {}
    if a.level is not None:
        pl["level"] = a.level
    if a.entrance is not None:
        pl["entrance"] = a.entrance
    if a.door is not None:
        pl["door"] = a.door
    if pl:
        for p in glob.glob(os.path.join(a.data, "players", "*.dat")):
            save(p, setvars(load(p), pl, set()))
            bak = p[:-4] + ".bak"
            if os.path.exists(bak):
                os.remove(bak)
    # the instances of the old run are gone
    inst = os.path.join(a.data, "instances.txt")
    if os.path.exists(inst):
        os.remove(inst)


if __name__ == "__main__":
    main()
