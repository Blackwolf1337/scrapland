"""The games' clock for the test tools. SCRAP_TIMESCALE=<n> (include/scrap/platform.h) makes every clock of the games
and of the coordinator run n times as fast as the real one (1 to 16, 1 without the variable). A tool that lets the
games play for a while (a settle time, the time between two samples) waits game time with sleep() here; a timeout
that only bounds the wait for an answer stays real time. The deterministic test mode ignores the variable."""
import os
import time

MAX = 16


def scale():
    try:
        n = int(os.environ.get('SCRAP_TIMESCALE', '1') or 1)
    except ValueError:
        n = 1
    return max(1, min(MAX, n))


SCALE = scale()


def secs(s):
    """real seconds of s seconds of game time"""
    return s / SCALE


def sleep(s):
    """waits s seconds of game time"""
    time.sleep(s / SCALE)


def real_env(env):
    """env without the scale, for the gates that measure against the real clock (feel) or compare with baselines of
    single player (regress, golden, levels)"""
    env = dict(env)
    env.pop('SCRAP_TIMESCALE', None)
    return env
