#!/usr/bin/env python3
"""scrap-mcp: an MCP server that runs ScrapC.exe (and the frozen reference exe) of this checkout without a window,
so an AI assistant can test it: start named games, run test channel commands in them, wait for the menu, a log line
or time, take screenshots (cropped, zoomed, side by side), read the logs, trace the render calls of one frame and list
the Vulkan validation errors that are ours.

Each game runs in a slot of its own (build-mcp/slot-<name>, made by tools/test/mkgame.sh --into: the game data
linked, Bin, the configs and the Wine prefix copied), through tools/test/wine.sh (gamescope headless, muted). The
game side is the test channel of the game (src/game/Global/TestChannel.c, tools/test/channel.py: no Python of the
tool runs in the game). The frozen reference exe (exe=ref) has no test channel (its harness, mcpgame.py through
-PythonExecute, went with the interpreter at tag interp-final): it runs for the tools that need no game side
(screenshots, compare, logs, key and mouse, wait kind=log or time). The server speaks the Model Context Protocol over stdio (JSON-RPC 2.0, one message a line) and needs only the Python standard
library; Pillow does crops, zoom and comparisons.
"""
import ast
import base64
import ctypes
import ctypes.util
import io
import json
import os
import re
import shutil
import signal
import subprocess
import sys
import time

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'test'))
import channel  # noqa: E402

VERSION = '0.5.0'
PROTOCOLS = ('2025-11-25', '2025-06-18', '2025-03-26', '2024-11-05')
ROOT = os.environ.get('SCRAP_MCP_ROOT') or os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))


def data_root(root):
    """The checkout with the game data (game/) and the slots: root itself, else the main checkout of the worktree root
    (agent worktrees have no game folder; the builds stay those of root)."""
    if os.path.isdir(os.path.join(root, 'game')):
        return root
    try:
        common = subprocess.run(['git', '-C', root, 'rev-parse', '--git-common-dir'], capture_output=True,
                                text=True).stdout.strip()
        main = os.path.dirname(os.path.abspath(os.path.join(root, common))) if common else ''
        if main and os.path.isdir(os.path.join(main, 'game')):
            return main
    except OSError:
        pass
    return root


DATA = data_root(ROOT)
REF_EXE = os.path.join(DATA, 'build-ref-noexact', 'ScrapC.exe')
# the slot folders: SCRAP_MCP_SLOTS (private slots, e.g. in a temporary folder), else build-mcp of the data checkout
SLOTS = os.path.abspath(os.environ.get('SCRAP_MCP_SLOTS') or os.path.join(DATA, 'build-mcp'))
RUNTIME = os.environ.get('XDG_RUNTIME_DIR') or '/run/user/%d' % os.getuid()
INSTRUCTIONS = (
    'Runs ScrapC of the Scrapland checkout without a window (gamescope headless, muted, on the AMD 610M). '
    'launch starts a named game (exe native64 = build-linux64/ScrapC by default, native64asan, c64 = the Windows '
    'build under Wine, ref = the frozen reference; map= loads a level), wait kind=ready waits until the menu or the '
    'level runs. cmd runs test channel commands in the game (values, calls with literal arguments, engine and entity '
    'variables, module dumps, console lines; not in ref, which has no test channel). menu_dump lists the items of the open menu, open_menu, click, key and mouse drive it. screenshot and compare show the window, log reads log.html '
    'or scrapdecomp.log, trace and vkcheck need launch with trace= or validation=true. quit ends a game. Network '
    'games: server_launch starts a server (dedicated by default, port 29100-29199), launch connect=host:port with '
    'player= starts a client that joins it, wait kind=joined spawn=true until its ship flies, state gives what each '
    'side sees (players net<id>, positions, scores) as JSON, hold keeps keys down on several games at once, '
    'server_quit ends the server. Build first; launch copies the build into the slot.')


def text(s):
    return {'type': 'text', 'text': s}


def sockets(since=0.0):
    """The gamescope sockets made after since (a name can come back, so by time, not by name)."""
    out = set()
    try:
        for n in os.listdir(RUNTIME):
            if re.fullmatch(r'gamescope-\d+', n) and os.stat(os.path.join(RUNTIME, n)).st_mtime >= since:
                out.add(n)
    except OSError:
        pass
    return out


# the native Linux builds by their exe name
# the Windows builds by their exe name (the DLLs of FFmpeg and SDL3 from the same folder)
WIN_BUILDS = {'c64': 'build-win64'}
NATIVE_BUILDS = {'native64': 'build-linux64', 'native64asan': 'build-linux64-asan'}
DEFAULT_NATIVE_BUILDS = dict(NATIVE_BUILDS)
DEFAULT_WIN_BUILDS = dict(WIN_BUILDS)


class Game:
    def __init__(self, name):
        self.name = name
        self.dir = os.path.join(SLOTS, 'slot-' + name)
        self.game = os.path.join(self.dir, 'game')
        self.prefix = os.path.join(self.dir, 'prefix')
        self.proc = None
        self.socket = None
        self.seq = int(time.time()) % 100000
        self.opts = {}
        self.wine_log = os.path.join(self.dir, 'wine.log')

    def running(self):
        return self.proc is not None and self.proc.poll() is None

    def prepare_native(self, exe):
        """A native Linux build in a game folder of its own (mkgame.sh --native, no prefix)."""
        build = NATIVE_BUILDS[exe]
        self.game = os.path.join(self.dir, exe + '-game')
        binDir = os.path.join(self.game, 'Bin')
        if not os.path.isdir(binDir):
            r = subprocess.run([os.path.join(ROOT, 'tools', 'test', 'mkgame.sh'), '--native', self.game, DATA],
                               capture_output=True, text=True)
            if r.returncode:
                raise RuntimeError('mkgame.sh failed: %s' % (r.stderr or r.stdout))
        src = os.path.join(ROOT, build, 'ScrapC')
        if not os.path.isfile(src):
            raise RuntimeError('%s is missing (cmake --build %s first)' % (src, build))
        shutil.copy2(src, binDir)
        lib = os.path.join(ROOT, build, 'ffmpeg', 'install', 'lib')
        for n in os.listdir(lib):
            if '.so' in n:
                p = os.path.join(lib, n)
                dst = os.path.join(binDir, n)
                if os.path.islink(p):
                    if os.path.lexists(dst):
                        os.remove(dst)
                    os.symlink(os.readlink(p), dst)
                else:
                    shutil.copy2(p, dst)
        with open(os.path.join(binDir, 'scrapdecomp.ini'), 'w') as f:
            f.write('[general]\n')
        for n in ('test_cmd.txt', 'test_reply.txt', 'log.html'):
            p = os.path.join(self.game, n)
            if os.path.exists(p):
                os.remove(p)

    def prepare(self, exe):
        self.game = os.path.join(self.dir, 'game')
        if exe in NATIVE_BUILDS:
            return self.prepare_native(exe)
        if not os.path.isdir(os.path.join(self.game, 'Bin')) or not os.path.isdir(self.prefix):
            r = subprocess.run([os.path.join(ROOT, 'tools', 'test', 'mkgame.sh'), '--into', self.dir, DATA],
                               capture_output=True, text=True)
            if r.returncode:
                raise RuntimeError('mkgame.sh failed: %s' % (r.stderr or r.stdout))
        binDir = os.path.join(self.game, 'Bin')
        build = WIN_BUILDS.get(exe, 'build-win64')
        src = os.path.join(ROOT, build, 'ScrapC.exe') if exe in WIN_BUILDS else REF_EXE
        if not os.path.isfile(src):
            raise RuntimeError('%s is missing (build first)' % src)
        shutil.copy2(src, os.path.join(binDir, 'ScrapC.exe' if exe in WIN_BUILDS else 'ScrapRef.exe'))
        for d in (os.path.join(ROOT, build, 'ffmpeg', 'install', 'bin'), os.path.join(ROOT, build, 'sdl3', 'install', 'bin')):
            if os.path.isdir(d):
                for n in os.listdir(d):
                    if n.endswith('.dll'):
                        shutil.copy2(os.path.join(d, n), binDir)
        with open(os.path.join(binDir, 'scrapdecomp.ini'), 'w') as f:
            f.write('[general]\n')
        for n in ('test_cmd.txt', 'test_reply.txt', 'log.html'):
            p = os.path.join(self.game, n)
            if os.path.exists(p):
                os.remove(p)

    def start(self, a):
        if self.running():
            raise RuntimeError('game %s runs already; quit it first' % self.name)
        exe = a.get('exe', 'native64')
        if exe != 'ref' and exe not in WIN_BUILDS and exe not in NATIVE_BUILDS:
            raise RuntimeError("exe is one of: ref, %s" % ', '.join(list(WIN_BUILDS) + list(NATIVE_BUILDS)))
        if a.get('reset'):
            if exe not in NATIVE_BUILDS:
                raise RuntimeError('reset is for the native builds (native64, native64asan)')
            reset_slot(self.name, exe)
        self.prepare(exe)
        if exe != 'ref' and not self.has_channel(exe):
            raise RuntimeError('the game binary of %s has no test channel (an old build): build it again' % exe)
        self.opts = dict(a, exe=exe)
        env = dict(os.environ)
        env.update({'SCRAP_GAME_DIR': self.game, 'SCRAP_PREFIX': self.prefix,
                    'HEADLESS_TIMEOUT': str(int(a.get('timeout', 3600))), 'WINE_LOG': self.wine_log,
                    'WINEDLLOVERRIDES': 'dinput8=b'})
        w, h = (a.get('size') or '1280x720').lower().split('x')
        env['HEADLESS_SIZE'] = '%s %s' % (w, h)
        if a.get('validation'):
            env['SCRAP_VK_VALIDATION'] = '1'
        if a.get('trace'):
            env['SCRAP_VK_TRACE'] = str(a['trace'])
        env.update({k: str(v) for k, v in (a.get('env') or {}).items()})
        args = [] if exe in NATIVE_BUILDS else ['ScrapC.exe' if exe in WIN_BUILDS else 'ScrapRef.exe']
        args.append('-fullscreen:0')
        if a.get('map'):
            m = a['map']
            args.append('-imap:' + (m if '/' in m else 'Levels/' + m))
        # engine variables of the config file as -name:value (before args: -dedicated and -server come last)
        args += [param(k, v) for k, v in (a.get('settings') or {}).items()]
        args += list(a.get('args') or [])
        if a.get('deterministic'):
            env['SCRAP_TEST_DETERMINISTIC'] = '1'
        if exe != 'ref':
            # the regress runner asks for 'step' (the deterministic test mode with the clock held between commands)
            env['SCRAP_TEST_CHANNEL'] = str((a.get('env') or {}).get('SCRAP_TEST_CHANNEL', '1'))
        before = time.time() - 0.5
        runner = 'native.sh' if exe in NATIVE_BUILDS else 'wine.sh'
        if exe in NATIVE_BUILDS:
            env['NATIVE_LOG'] = self.wine_log
            env['SCRAP_BUILD'] = os.path.join(ROOT, NATIVE_BUILDS[exe])
            if exe == 'native64asan':
                env.setdefault('ASAN_OPTIONS', 'detect_leaks=0')
        self.proc = subprocess.Popen([os.path.join(ROOT, 'tools', 'test', runner)] + args, env=env,
                                     stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                                     start_new_session=True)
        end = time.time() + 30
        self.socket = None
        while time.time() < end and self.running():
            taken = set(g.socket for g in GAMES.values() if g is not self and g.running())
            new = sorted(sockets(before) - taken, key=lambda n: os.stat(os.path.join(RUNTIME, n)).st_mtime)
            if new:
                self.socket = new[-1]
                break
            time.sleep(0.2)
        return 'game %s started (%s, pid %d, gamescope %s); wait kind=ready for the menu or the level' % (
            self.name, exe, self.proc.pid, self.socket or 'socket not found')

    def native_pids(self):
        """The processes of the native game of this slot and of its gamescope session."""
        exe = os.path.realpath(os.path.join(self.game, 'Bin', 'ScrapC'))
        game, sids = [], set()
        for p in os.listdir('/proc'):
            if p.isdigit():
                try:
                    if os.path.realpath(os.readlink('/proc/%s/exe' % p)) == exe:
                        game.append(int(p))
                        sids.add(os.getsid(int(p)))
                except OSError:
                    pass
        session = []
        for p in os.listdir('/proc'):
            if p.isdigit():
                try:
                    if os.getsid(int(p)) in sids:
                        session.append(int(p))
                except OSError:
                    pass
        return game, session

    def stop_native(self):
        """SIGTERM to the game (SDL makes it a quit event), after 10 s SIGKILL to its gamescope session."""
        game, session = self.native_pids()
        for p in game:
            try:
                os.kill(p, signal.SIGTERM)
            except OSError:
                pass
        end = time.time() + 10
        while time.time() < end and self.native_pids()[0]:
            time.sleep(0.2)
        for p in self.native_pids()[1] or session:
            try:
                os.kill(p, signal.SIGKILL)
            except OSError:
                pass
        try:
            self.proc.wait(timeout=20)
        except subprocess.TimeoutExpired:
            os.killpg(self.proc.pid, signal.SIGKILL)
            self.proc.wait(timeout=10)
        code = self.proc.returncode
        self.proc = None
        self.socket = None
        return 'game %s stopped (exit code %s)' % (self.name, code)

    def stop(self):
        if not self.proc:
            return 'game %s does not run' % self.name
        if self.opts.get('exe') in NATIVE_BUILDS:
            return self.stop_native()
        env = dict(os.environ, WINEPREFIX=self.prefix)
        subprocess.run(['wineserver', '-k'], env=env, capture_output=True)
        try:
            self.proc.wait(timeout=20)
        except subprocess.TimeoutExpired:
            try:
                os.killpg(self.proc.pid, signal.SIGKILL)
            except OSError:
                pass
            subprocess.run(['wineserver', '-k9'], env=env, capture_output=True)
            self.proc.wait(timeout=10)
        code = self.proc.returncode
        self.proc = None
        self.socket = None
        return 'game %s stopped (exit code %s)' % (self.name, code)

    def has_channel(self, exe):
        """whether the prepared game binary has the test channel (builds from before it have none)"""
        name = 'ScrapC' if exe in NATIVE_BUILDS else 'ScrapC.exe'
        try:
            with open(os.path.join(self.game, 'Bin', name), 'rb') as f:
                return b'test_cmd.txt' in f.read()
        except OSError:
            return False

    def channel(self):
        """whether the game has the test channel (all C builds; not the reference exe)"""
        return self.opts.get('exe', 'native64') != 'ref'

    def cmd(self, lines, timeout=10.0):
        """test channel commands (one or more lines); the answers"""
        if not self.running():
            raise RuntimeError('game %s does not run' % self.name)
        if not self.channel():
            raise RuntimeError('game %s is the reference exe, which has no test channel' % self.name)
        try:
            return channel.send(self.game, lines, timeout=timeout, alive=self.running)
        except channel.NoAnswer:
            raise RuntimeError('no answer from game %s in %.0f s (the menu or level may not run yet, or a level '
                               'loads)' % (self.name, timeout))

    def logfile(self, which):
        return os.path.join(self.game, 'log.html') if which == 'game' else os.path.join(self.game, 'Bin', 'scrapdecomp.log')

    def log(self, which='game'):
        try:
            with open(self.logfile(which), 'r', errors='replace') as f:
                data = f.read()
        except OSError:
            return ''
        if which == 'game':
            data = re.sub(r'<[^>]*>', '', data).replace('&gt;', '>').replace('&lt;', '<').replace('&amp;', '&')
        return data

    def screenshot(self, timeout=15.0):
        if not self.running() or not self.socket:
            raise RuntimeError('game %s does not run or has no gamescope socket' % self.name)
        path = os.path.join(self.dir, 'shot.png')
        if os.path.exists(path):
            os.remove(path)
        env = dict(os.environ, GAMESCOPE_WAYLAND_DISPLAY=self.socket)
        subprocess.run(['gamescopectl', 'screenshot', path], env=env, capture_output=True, timeout=timeout)
        end = time.time() + timeout
        size = -1
        while time.time() < end:
            if os.path.exists(path):
                s = os.path.getsize(path)
                if s > 0 and s == size:
                    break
                size = s
            time.sleep(0.2)
        if not os.path.exists(path):
            raise RuntimeError('gamescope wrote no screenshot')
        with open(path, 'rb') as f:
            return f.read()


GAMES = {}


def reset_slot(name, exe):
    """The slot back to the fixed state of the regression runner (tools/test/regress/slot.py): the engine
    configuration from its template (PlayerName Tester, ServerType DeathMatch, ...), no saves, nothing a run left in
    the game folder, and no intro video (a game is ready in seconds)"""
    sys.path.insert(0, os.path.join(ROOT, 'tools', 'test', 'regress'))
    try:
        import slot
    finally:
        sys.path.pop(0)
    slot.reset(name, exe, slots=SLOTS, main=DATA)


def param(name, value):
    """-name:value for the command line of the game. The reader of the engine (CApp_ParseCmdLine) starts a new
    parameter at every '-', starts the value again at every ':' and drops blanks, so values are kept to letters,
    digits and _ . [ ] , ' (a map list such as ['FZ','OM'] is quoted in double quotes)."""
    if not re.fullmatch(r'[A-Za-z][A-Za-z0-9_]*', name):
        raise RuntimeError('bad engine variable name %r' % name)
    v = str(value)
    if not re.fullmatch(r"[A-Za-z0-9_.\[\],']+", v) or (v.startswith("'") and "[" not in v):
        raise RuntimeError("the value %r of %s cannot pass the command line of the game (letters, digits and "
                           "_ . [ ] , ' only)" % (v, name))
    return '-%s:%s' % (name, '"%s"' % v if "'" in v else v)


def game_of(a, create=False):
    name = a.get('name') or 'main'
    if not re.fullmatch(r'[A-Za-z0-9_-]+', name):
        raise RuntimeError('a name is letters, digits, _ and -')
    if name not in GAMES:
        if not create:
            raise RuntimeError('no game %s' % name)
        GAMES[name] = Game(name)
    return GAMES[name]


def _pil():
    try:
        from PIL import Image, ImageDraw
        return Image, ImageDraw
    except ImportError:
        return None, None


def picture(data, crop=None, zoom=1.0):
    Image, _ = _pil()
    if Image is None:
        if crop or zoom != 1.0:
            raise RuntimeError('crop and zoom need Pillow')
        return None
    im = Image.open(io.BytesIO(data)).convert('RGB')
    if crop:
        x, y, w, h = [int(round(v)) for v in crop]
        im = im.crop((max(0, x), max(0, y), min(im.width, x + max(1, w)), min(im.height, y + max(1, h))))
    if zoom and zoom != 1.0:
        im = im.resize((max(1, round(im.width * zoom)), max(1, round(im.height * zoom))), Image.LANCZOS)
    return im


def image_content(data=None, im=None, crop=None, zoom=1.0, max_width=1600):
    if im is None:
        im = picture(data, crop, zoom)
    if im is None:
        return {'type': 'image', 'data': base64.b64encode(data).decode(), 'mimeType': 'image/png'}
    Image, _ = _pil()
    if im.width > max_width:
        im = im.resize((max_width, round(im.height * max_width / im.width)), Image.LANCZOS)
    buf = io.BytesIO()
    im.save(buf, 'PNG')
    return {'type': 'image', 'data': base64.b64encode(buf.getvalue()).decode(), 'mimeType': 'image/png'}


def strip(images, labels):
    Image, ImageDraw = _pil()
    if Image is None:
        raise RuntimeError('compare needs Pillow')
    h = max(i.height for i in images)
    images = [i if i.height == h else i.resize((round(i.width * h / i.height), h), Image.LANCZOS) for i in images]
    out = Image.new('RGB', (sum(i.width for i in images) + 8 * (len(images) - 1), h + 22), (255, 0, 255))
    draw = ImageDraw.Draw(out)
    x = 0
    for im, label in zip(images, labels):
        out.paste(im, (x, 22))
        draw.rectangle((x, 0, x + im.width, 21), fill=(0, 0, 0))
        draw.text((x + 4, 4), label, fill=(255, 255, 255))
        x += im.width + 8
    return out


def foreign_vuids():
    out = set()
    with open(os.path.join(ROOT, 'tools', 'test', 'vk-foreign-vuids.txt')) as f:
        for line in f:
            if line.strip() and not line.startswith('#'):
                out.add(line.split('\t')[0].strip())
    return out


def schema(props=None, required=None):
    s = {'type': 'object', 'properties': props or {}}
    if required:
        s['required'] = required
    return s


NAME = {'type': 'string', 'description': 'the game, default main'}
TOOLS = [
    {'name': 'launch', 'description': 'Start a game in its own slot (no window, muted, AMD 610M). Returns at once; '
     'then wait kind=ready.', 'inputSchema': schema({
         'name': NAME,
         'exe': {'type': 'string', 'enum': ['native64', 'native64asan', 'c64', 'ref'], 'description': 'native64: build-linux64/ScrapC '
                 '(default), native64asan: build-linux64-asan/ScrapC (AddressSanitizer, its report in the log, '
                 'which=wine), c64: build-win64/ScrapC.exe under Wine, ref: the frozen reference '
                 'build-ref-noexact/ScrapC.exe (i686, Direct3D 8 through Wine; no test channel: screenshots, logs, '
                 'key and mouse only)'},
         'map': {'type': 'string', 'description': 'a level for -imap: (FZ or Levels/FZ)'},
         'args': {'type': 'array', 'items': {'type': 'string'}, 'description': 'more arguments of the game'},
         'env': {'type': 'object', 'description': 'more environment variables (SCRAP_VK_DEBUG, SCRAP_RD_DUMP, ...)'},
         'validation': {'type': 'boolean', 'description': 'the Vulkan validation layer on (see vkcheck)'},
         'deterministic': {'type': 'boolean', 'description': 'SCRAP_TEST_DETERMINISTIC=1: fixed random seed, a clock '
                           'of 60 fixed steps a second and the sound mixed by it, so a run repeats (not ref)'},
         'trace': {'type': 'number', 'description': 'SCRAP_VK_TRACE: the render calls of the first frame this many '
                   'seconds after the device go into scrapdecomp.log (see trace)'},
         'size': {'type': 'string', 'description': 'the display of gamescope, default 1280x720'},
         'timeout': {'type': 'number', 'description': 'seconds until the game is killed, default 3600'},
         'connect': {'type': 'string', 'description': 'host:port of a server (server_launch): the client starts in '
                     'the menu level, waits until it answers and calls SNet.InitClient; then wait kind=joined. '
                     'Not with map'},
         'player': {'type': 'string', 'description': 'PlayerName of the game (letters, digits, _ . [ ]); default the '
                    'game name with connect'},
         'team': {'type': 'integer', 'description': 'PlayerTeamID: 0 red, 2 green'},
         'settings': {'type': 'object', 'description': 'engine variables of the config file passed as -name:value '
                      '(e.g. {"ClientPPS": 20})'},
         'reset': {'type': 'boolean', 'description': 'the slot back to the template of the regression runner first '
                   '(config, no saves, no intro video: ready in seconds); native builds only'}})},
    {'name': 'server_launch', 'description': 'Start a network server game in its own slot (no window, muted) and '
     'wait until it serves: dedicated (-dedicated, no own ship) or a host that plays (dedicated=false, -server). '
     'Clients join with launch connect=127.0.0.1:<port>.', 'inputSchema': schema({
         'name': {'type': 'string', 'description': 'the game, default server'},
         'mode': {'type': 'string', 'description': 'ServerType: DeathMatch (default), TeamDM, FlagHunt, TeamFlag, '
                  'Postman, TeamPost, DeathMatchBots'},
         'map': {'type': 'string', 'description': 'the level, default FZ'},
         'maps': {'type': 'array', 'items': {'type': 'string'}, 'description': 'ServerMapList, default [map]'},
         'port': {'type': 'integer', 'description': 'UDP port, default the first free one of 29100-29199'},
         'dedicated': {'type': 'boolean', 'description': 'default true'},
         'player': {'type': 'string', 'description': 'PlayerName of a host that plays, default Host'},
         'max_players': {'type': 'integer', 'description': 'DefaultMaxPlayersOnServer, default 8 (1..16)'},
         'fraglimit': {'type': 'integer', 'description': 'default 20'},
         'password': {'type': 'string', 'description': 'a server password (default none)'},
         'server_name': {'type': 'string', 'description': 'ServerName, default Test_Server'},
         'bots': {'type': 'integer', 'description': 'server bots: mode DeathMatchBots with this many (3 or more; '
                  'the mode makes 3, the tool adds the rest through NDeathMatchBots.BotsList and InitServerMod)'},
         'deterministic': {'type': 'boolean', 'description': 'SCRAP_TEST_DETERMINISTIC=1 (fixed seed, fixed step '
                           'clock; the network timing between games stays real)'},
         'exe': {'type': 'string', 'enum': ['native64', 'native64asan', 'c64']},
         'env': {'type': 'object'}, 'args': {'type': 'array', 'items': {'type': 'string'}},
         'settings': {'type': 'object', 'description': 'more engine variables as -name:value'},
         'reset': {'type': 'boolean', 'description': 'reset the slot first (as launch)'},
         'size': {'type': 'string'}, 'timeout': {'type': 'number', 'description': 'seconds until the game is killed, '
                                                                                 'default 3600'}})},
    {'name': 'server_quit', 'description': 'End a server game (default name server).',
     'inputSchema': schema({'name': {'type': 'string', 'description': 'default server'}})},
    {'name': 'state', 'description': 'What games see of the network game, as JSON (one object per game): level, '
     'net flags, own ship, scores (Ffa.Scorer), the players and bots (ships net<id>, id <= 512) with Pos, Vel, Ang, '
     'Life, Active, Descriptor (player name), TgType, and the count of the other net<id> objects.',
     'inputSchema': schema({'name': NAME, 'names': {'type': 'array', 'items': {'type': 'string'},
                                                     'description': 'several games, asked one after the other'}})},
    {'name': 'hold', 'description': 'Hold keys and mouse buttons down together for some seconds on one or several '
     'games at once (XTEST on the display of each game). Ship keys: w forward, s back, a d strafe, left right up down '
     'turn, ctrl (LCONTROL) or button 1 fire, button 2 boost, space countermeasure.', 'inputSchema': schema({
         'name': NAME, 'names': {'type': 'array', 'items': {'type': 'string'}},
         'keys': {'type': 'array', 'items': {'type': 'string'}},
         'buttons': {'type': 'array', 'items': {'type': 'integer'}, 'description': '1 left, 2 middle, 3 right'},
         'seconds': {'type': 'number', 'description': 'default 1'}, 'screenshot': {'type': 'boolean'}})},
    {'name': 'quit', 'description': 'End a game (all of them with name=all).', 'inputSchema': schema({'name': NAME})},
    {'name': 'status', 'description': 'The games and whether they run.', 'inputSchema': schema()},
    {'name': 'wait', 'description': 'Wait until the game answers (kind=ready: the menu or the level runs), a '
     'log line matches (kind=log), for seconds (kind=time), or a client has its ship on the server (kind=joined).', 'inputSchema': schema({
         'name': NAME, 'kind': {'type': 'string', 'enum': ['ready', 'log', 'time', 'joined']},
         'spawn': {'type': 'boolean', 'description': 'kind=joined: then Ffa.RestartOnPress(0) (the Fire key of the '
                   'help screen) and wait until the ship is active'},
         'pattern': {'type': 'string', 'description': 'regex for kind=log'},
         'which': {'type': 'string', 'enum': ['game', 'engine'], 'description': 'kind=log: game = log.html (default), '
                   'engine = scrapdecomp.log'},
         'seconds': {'type': 'number', 'description': 'kind=time'},
         'timeout': {'type': 'number', 'description': 'default 120'},
         'screenshot': {'type': 'boolean', 'description': 'a screenshot after the wait'}}, ['kind'])},
    {'name': 'cmd', 'description': 'Test channel commands in the game (no Python), one a line: an expression gives its '
     'repr (literals, names, Module.attr, calls with literal or nested arguments and keywords, x[i], x[a:b], '
     '"is None"); "target = expr" sets a module, entity or item attribute; do <..> (no answer); get <var> ...; '
     'ent <entity> <var> ...; ents <list> [x y z r]; dump [module ...]; menu [player]; click <item> [player]; '
     'con <console line>; print <expr>; recorder <Module.attr> <list> [<literal> | raise <Exc> <text>]; '
     'check <name>\\t<expr>. Errors answer "error: <type>: <value>".', 'inputSchema': schema({
         'name': NAME, 'lines': {'type': 'string', 'description': 'the commands, one a line'},
         'timeout': {'type': 'number', 'description': 'default 10'}, 'screenshot': {'type': 'boolean'}}, ['lines'])},
    {'name': 'screenshot', 'description': 'The window of the game (gamescope).', 'inputSchema': schema({
        'name': NAME, 'crop': {'type': 'array', 'items': {'type': 'number'}, 'description': 'x, y, w, h'},
        'zoom': {'type': 'number'}})},
    {'name': 'compare', 'description': 'Screenshots of several games side by side (e.g. c and ref on the same level).',
     'inputSchema': schema({'names': {'type': 'array', 'items': {'type': 'string'}},
                            'crop': {'type': 'array', 'items': {'type': 'number'}}}, ['names'])},
    {'name': 'log', 'description': 'The end of log.html (which=game, default) or scrapdecomp.log (which=engine).',
     'inputSchema': schema({'name': NAME, 'which': {'type': 'string', 'enum': ['game', 'engine']},
                            'lines': {'type': 'integer', 'description': 'default 40'},
                            'pattern': {'type': 'string', 'description': 'only the lines that match (regex)'}})},
    {'name': 'trace', 'description': 'The traced frame of a game launched with trace= (draws, targets, clears, pixel '
     'shaders), with a count of the draws by target and shader first.', 'inputSchema': schema({
         'name': NAME, 'full': {'type': 'boolean', 'description': 'all lines, not only the summary and the first 80'}})},
    {'name': 'vkcheck', 'description': 'The Vulkan validation errors of a game launched with validation=true, without '
     'those of gamescope and Wine (tools/test/vk-foreign-vuids.txt).', 'inputSchema': schema({'name': NAME})},
    {'name': 'menu_dump', 'description': 'The scorer items of a player (menus and HUD) in draw order: name, text, '
     'position, size, color and alpha, hidden, OnAccept and the cursor links; > marks the cursor item.',
     'inputSchema': schema({'name': NAME, 'player': {'type': 'integer', 'description': 'default 0'},
                            'filter': {'type': 'string', 'description': 'only items whose name or text matches (regex)'},
                            'screenshot': {'type': 'boolean'}})},
    {'name': 'open_menu', 'description': 'Open a menu by its script function (module.function, called with the player '
     'and an empty control), e.g. PCMenu.VideoMenu, PCMenu.OptionsMenu, Menu.MainMenu.', 'inputSchema': schema({
         'name': NAME, 'menu': {'type': 'string'}, 'player': {'type': 'integer', 'description': 'default 0'},
         'screenshot': {'type': 'boolean'}}, ['menu'])},
    {'name': 'click', 'description': 'Activate a scorer item by name: runs its OnAccept with the player and the item '
     'name, as the accept key does.', 'inputSchema': schema({
         'name': NAME, 'item': {'type': 'string'}, 'player': {'type': 'integer', 'description': 'default 0'},
         'screenshot': {'type': 'boolean'}}, ['item'])},
    {'name': 'key', 'description': 'Real key presses through the X display of the game (XTEST): a list of keys or '
     'combos like "down", "enter", "esc", "ctrl+a", "f1", one character.', 'inputSchema': schema({
         'name': NAME, 'keys': {'type': 'array', 'items': {'type': 'string'}},
         'hold': {'type': 'number', 'description': 'seconds each key is held, default 0.08'},
         'gap': {'type': 'number', 'description': 'seconds between keys, default 0.15'},
         'screenshot': {'type': 'boolean'}}, ['keys'])},
    {'name': 'mouse', 'description': 'Real mouse input through the X display of the game (XTEST): relative motion in '
     'pixels, then an optional button press.', 'inputSchema': schema({
         'name': NAME, 'dx': {'type': 'integer'}, 'dy': {'type': 'integer'},
         'button': {'type': 'integer', 'description': '1 left, 2 middle, 3 right'},
         'hold': {'type': 'number', 'description': 'seconds the button is held, default 0.08'},
         'screenshot': {'type': 'boolean'}})},
    {'name': 'configure', 'description': 'Point native64, native64asan or c64 at another build folder for this '
     'server process only (e.g. a worktree), or back to the default; applies to the next launch. slots= moves the '
     'slot folder.', 'inputSchema': schema({
         'exe': {'type': 'string', 'enum': ['native64', 'native64asan', 'c64']},
         'build': {'type': 'string', 'description': 'build folder, absolute or relative to the repo; empty: default'},
         'slots': {'type': 'string', 'description': 'another folder for the slots (private slots; empty: back to '
                   'SCRAP_MCP_SLOTS or build-mcp); all games must be ended'}})},
]


# ---- menus: the scorer items of a player (SScorer.Items of ScrapC), item actions, menus by name

MENU_VARS = ('Text', 'X', 'Y', 'W', 'H', 'Visible', 'Alpha', 'Red', 'Green', 'Blue', 'OnAccept', 'Up', 'Down', 'Left',
             'Right')


def menu_items(g, player):
    """(cursor item, rows of the name and the MENU_VARS values) of the scorer items of a player"""
    out = g.cmd('menu %d' % player, timeout=15)
    if out.startswith('error'):
        raise RuntimeError(out)
    current, rows = ast.literal_eval(out)
    return current, [list(r) for r in rows]


def fmt_num(v):
    return '-' if v is None else ('%g' % v if isinstance(v, float) else str(v))


def menu_dump(g, player, rx):
    current, rows = menu_items(g, player)
    lines = ['%d items of player %d in draw order (first drawn first), cursor on %s' % (len(rows), player, current)]
    for r in rows:
        v = dict(zip(('Name',) + MENU_VARS, r))
        if rx and not (rx.search(v['Name'] or '') or rx.search(str(v['Text'] or ''))):
            continue
        parts = ['%s%s' % ('> ' if v['Name'] == current else '', v['Name'])]
        if v['Text'] is not None:
            parts.append('text %r' % v['Text'])
        parts.append('at %s,%s size %sx%s' % tuple(fmt_num(v[k]) for k in ('X', 'Y', 'W', 'H')))
        if v['Red'] is not None:
            parts.append('rgba %s,%s,%s,%s' % tuple(fmt_num(v[k]) for k in ('Red', 'Green', 'Blue', 'Alpha')))
        if v['Visible'] == 0:
            parts.append('hidden')
        if v['OnAccept']:
            parts.append('accept %s' % v['OnAccept'])
        links = ['%s %s' % (k.lower(), v[k]) for k in ('Up', 'Down', 'Left', 'Right') if v[k]]
        if links:
            parts.append('links ' + ', '.join(links))
        lines.append('  ' + '; '.join(parts))
    return '\n'.join(lines)


def menu_click(g, player, item):
    if not re.fullmatch(r'\S+', item):
        raise RuntimeError('an item name has no blanks')
    out = g.cmd('click %s %d' % (item, player), timeout=15)
    # the repr, as the old python harness answered
    return out if out.startswith('error') else repr(out)


def menu_open(g, player, menu):
    if not re.fullmatch(r'[A-Za-z_][A-Za-z0-9_]*(\.[A-Za-z_][A-Za-z0-9_]*)+', menu):
        raise RuntimeError('menu is module.function, e.g. PCMenu.VideoMenu')
    out = g.cmd('do %s(%d, "")' % (menu, player), timeout=15)
    return out or repr('opened %s' % menu)


# ---- real input (XTEST) into the X display of the game's gamescope

XKEYS = {'enter': 'Return', 'return': 'Return', 'esc': 'Escape', 'escape': 'Escape', 'tab': 'Tab',
         'backspace': 'BackSpace', 'delete': 'Delete', 'space': 'space', 'up': 'Up', 'down': 'Down', 'left': 'Left',
         'right': 'Right', 'home': 'Home', 'end': 'End', 'pageup': 'Prior', 'pagedown': 'Next', 'ctrl': 'Control_L',
         'shift': 'Shift_L', 'alt': 'Alt_L'}
XLIB = None


def xlib():
    """libX11 and libXtst through ctypes (gamescope needs both, so they are there where it is)."""
    global XLIB
    if XLIB is None:
        try:
            x11 = ctypes.CDLL(ctypes.util.find_library('X11') or 'libX11.so.6')
            xtst = ctypes.CDLL(ctypes.util.find_library('Xtst') or 'libXtst.so.6')
        except OSError as e:
            raise RuntimeError('input needs libX11 and libXtst: %s' % e)
        vp, ul, ci, cu = ctypes.c_void_p, ctypes.c_ulong, ctypes.c_int, ctypes.c_uint
        for fn, res, args in (('XOpenDisplay', vp, [ctypes.c_char_p]), ('XCloseDisplay', ci, [vp]),
                              ('XSync', ci, [vp, ci]), ('XStringToKeysym', ul, [ctypes.c_char_p]),
                              ('XKeysymToKeycode', ctypes.c_ubyte, [vp, ul])):
            getattr(x11, fn).restype, getattr(x11, fn).argtypes = res, args
        for fn, args in (('XTestFakeRelativeMotionEvent', [vp, ci, ci, ul]), ('XTestFakeButtonEvent', [vp, cu, ci, ul]),
                         ('XTestFakeKeyEvent', [vp, cu, ci, ul])):
            getattr(xtst, fn).restype, getattr(xtst, fn).argtypes = ci, args
        # (Xlib's own error handler ends the whole process: this one only notes it)
        handler = ctypes.CFUNCTYPE(ci, vp, vp)(lambda d, e: 0)
        x11.XSetErrorHandler.restype, x11.XSetErrorHandler.argtypes = vp, [vp]
        x11.XSetErrorHandler(ctypes.cast(handler, vp))
        XLIB = (x11, xtst, handler)
    return XLIB


def game_display(g):
    """DISPLAY of the game process itself (gamescope's own X server), never the desktop's: the process must be the
    game binary (native) or a Wine process running ScrapC/ScrapRef.exe, with the SCRAP_GAME_DIR of this game."""
    if not g.running():
        raise RuntimeError('game %s does not run' % g.name)
    desktop = {os.environ.get('DISPLAY'), os.environ.get('SCRAP_MCP_DESKTOP_DISPLAY')} - {None, ''}
    native = os.path.realpath(os.path.join(g.game, 'Bin', 'ScrapC'))
    for pid in os.listdir('/proc'):
        if not pid.isdigit():
            continue
        try:
            exe = os.path.realpath('/proc/%s/exe' % pid)
            with open('/proc/%s/cmdline' % pid, 'rb') as f:
                cmd = f.read().decode('utf-8', 'replace').split('\0')
            with open('/proc/%s/environ' % pid, 'rb') as f:
                env = dict(x.decode('utf-8', 'replace').partition('=')[::2] for x in f.read().split(b'\0') if b'=' in x)
        except OSError:
            continue
        is_game = exe == native or (os.path.basename(exe).startswith('wine') and
                                    any(os.path.basename(c.replace('\\', '/')) in ('ScrapC.exe', 'ScrapRef.exe') for c in cmd))
        if not is_game or env.get('SCRAP_GAME_DIR') != g.game or not env.get('DISPLAY'):
            continue
        if env['DISPLAY'] in desktop:
            raise RuntimeError('the game process has the desktop display %s; refusing to send input' % env['DISPLAY'])
        if env.get('XAUTHORITY'):
            os.environ['XAUTHORITY'] = env['XAUTHORITY']
        return env['DISPLAY']
    raise RuntimeError('found no game process of %s with its own DISPLAY' % g.name)


class XInput:
    def __init__(self, g):
        self.x11, self.xtst, _ = xlib()
        self.display = game_display(g)
        self.dpy = self.x11.XOpenDisplay(self.display.encode())
        if not self.dpy:
            raise RuntimeError('could not open the X display %s of the game' % self.display)

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.x11.XCloseDisplay(self.dpy)

    def sync(self, pause):
        self.x11.XSync(self.dpy, 0)
        time.sleep(pause)

    def keycode(self, name):
        if len(name) == 1:
            sym = ord(name)
        else:
            n = XKEYS.get(name.lower()) or (name.upper() if re.fullmatch(r'(?i)f\d{1,2}', name) else name)
            sym = self.x11.XStringToKeysym(n.encode())
        code = self.x11.XKeysymToKeycode(self.dpy, sym) if sym else 0
        if not code:
            raise RuntimeError('unknown key %r; use one character, f1-f12, an X keysym name or one of %s'
                               % (name, ', '.join(sorted(XKEYS))))
        return code

    def key(self, combo, hold):
        parts = combo.split('+') if combo != '+' else ['+']
        codes = [self.keycode(p) for p in parts]
        for c in codes:
            self.xtst.XTestFakeKeyEvent(self.dpy, c, 1, 0)
            self.sync(0.02)
        self.sync(hold)
        for c in reversed(codes):
            self.xtst.XTestFakeKeyEvent(self.dpy, c, 0, 0)
            self.sync(0.02)

    def move(self, dx, dy, step=40):
        """Relative motion in steps (SDL's relative mode recenters the pointer after every event)."""
        while dx or dy:
            sx, sy = max(-step, min(step, dx)), max(-step, min(step, dy))
            self.xtst.XTestFakeRelativeMotionEvent(self.dpy, sx, sy, 0)
            self.sync(0.012)
            dx, dy = dx - sx, dy - sy

    def button(self, number, hold):
        self.xtst.XTestFakeButtonEvent(self.dpy, number, 1, 0)
        self.sync(hold)
        self.xtst.XTestFakeButtonEvent(self.dpy, number, 0, 0)
        self.sync(0.03)


# ---- multiplayer: a server game, clients that connect to it, their state through the test channel, held input

PORTS = (29100, 29199)
PLAYER_RX = r'[A-Za-z0-9_.\[\]]{1,15}'
BOT_SHIPS = (('SArchbishop1', 'MegaAce'), ('SHump1', 'MegaAce'), ('SBankMaster1', 'MegaAce'))


def udp_free(port):
    import socket
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.bind(('0.0.0.0', port))
        return True
    except OSError:
        return False
    finally:
        s.close()


def free_port():
    """the first UDP port of PORTS that no game of this server uses and that can be bound"""
    taken = set(g.opts.get('port') for g in GAMES.values() if g.running())
    for p in range(PORTS[0], PORTS[1] + 1):
        if p not in taken and udp_free(p):
            return p
    raise RuntimeError('no free UDP port in %d-%d' % PORTS)


def wait_answer(g, timeout):
    """waits until the game answers the test channel; its level"""
    end = time.time() + timeout
    while time.time() < end and g.running():
        try:
            return g.cmd('Scrap.GetLevelPath()', timeout=2)
        except RuntimeError:
            pass
    raise RuntimeError('game %s did not answer in %.0f s%s' % (g.name, timeout, '' if g.running() else ' (it ended)'))


def answers(g, lines, timeout=15):
    """the answers of several channel commands (one command file), each read as a Python literal where it is one"""
    out = g.cmd('\n'.join(lines), timeout=timeout).split('\n')
    if len(out) != len(lines):
        raise RuntimeError('game %s answered %d lines to %d commands: %s' % (g.name, len(out), len(lines), out[:3]))
    res = []
    for line in out:
        try:
            res.append(ast.literal_eval(line))
        except (ValueError, SyntaxError):
            res.append(line)
    return res


def server_start(a):
    """server_launch: a server game on a level, dedicated (no own ship, -dedicated) or a host that plays (-server)"""
    g = game_of(dict(a, name=a.get('name') or 'server'), create=True)
    mode = a.get('mode') or 'DeathMatch'
    bots = int(a.get('bots') or 0)
    if bots:
        if bots < 3:
            raise RuntimeError('DeathMatchBots makes the 3 bots of its BotsList; bots is 3 or more')
        mode = 'DeathMatchBots'
    if not re.fullmatch(r'[A-Za-z]+', mode):
        raise RuntimeError('mode is a ServerType: DeathMatch, TeamDM, FlagHunt, TeamFlag, Postman, TeamPost, '
                           'DeathMatchBots')
    level = a.get('map') or 'FZ'
    level = level.split('/')[-1]
    maps = a.get('maps') or [level]
    if not all(re.fullmatch(r'[A-Za-z0-9_]+', m) for m in maps):
        raise RuntimeError('maps are level names like FZ')
    port = int(a.get('port') or free_port())
    if not udp_free(port):
        raise RuntimeError('UDP port %d is taken' % port)
    dedicated = a.get('dedicated', True)
    settings = {'ServerType': mode, 'ServerMapList': '[%s]' % ','.join("'%s'" % m for m in maps),
                'DefaultServerPort': port, 'DefaultMaxPlayersOnServer': int(a.get('max_players', 8)),
                'Fraglimit': int(a.get('fraglimit', 20)), 'ForceServerVehicle': 0,
                'ServerName': a.get('server_name') or 'Test_Server'}
    # the dedicated server reads Scrap.cfg of the game folder (App_GetConfigDir is "" there), whose settings asked for
    # a password (UseServerPassword 1, seen 2026-11-13): off unless a password is given
    if a.get('password'):
        settings.update({'UseServerPassword': 1, 'ServerPassword': a['password']})
    else:
        settings['UseServerPassword'] = 0
    if not dedicated:
        settings['PlayerName'] = a.get('player') or 'Host'
    settings.update(a.get('settings') or {})
    opts = {k: a[k] for k in ('exe', 'env', 'deterministic', 'size', 'timeout', 'validation', 'reset') if k in a}
    opts.update({'name': g.name, 'map': level, 'settings': settings,
                 'args': list(a.get('args') or []) + ['-dedicated' if dedicated else '-server']})
    msg = g.start(opts)
    g.opts.update({'role': 'server', 'port': port, 'mode': mode, 'dedicated': bool(dedicated)})
    wait_answer(g, float(a.get('ready_timeout', 120)))
    end = time.time() + 30
    data = None
    while time.time() < end:
        data = answers(g, ['SNet.GetServerData()'])[0]
        if isinstance(data, tuple) and data[2]:
            break
        time.sleep(0.5)
    if not (isinstance(data, tuple) and data[2]):
        raise RuntimeError('game %s answers but runs no server: %r' % (g.name, data))
    g.opts['port'] = data[2]
    added = ''
    if bots > 3:
        extra = [('[Bot%d]' % (i + 1),) + BOT_SHIPS[i % 3] for i in range(3, bots)]
        r = g.cmd('NDeathMatchBots.BotsList = %r\nNDeathMatchBots.InitServerMod()' % (extra,), timeout=30)
        added = ', %d bots added (%s)' % (len(extra), r.replace('\n', ' '))
    return '%s\nserver %s on 127.0.0.1:%d: %s on %s, %s%s' % (
        msg.split(';')[0], g.name, data[2], mode, level, 'dedicated' if dedicated else 'the host plays', added)


def connect_target(target):
    host, _, port = str(target).rpartition(':')
    if not host or not port.isdigit():
        raise RuntimeError('connect is host:port, e.g. 127.0.0.1:29100')
    return ('127.0.0.1' if host == 'localhost' else host), int(port)


def client_connect(g, a):
    """after the launch of a client: the menu level answers, then SNet.InitClient (what the join menu calls)"""
    host, port = connect_target(a['connect'])
    if not re.fullmatch(r'[A-Za-z0-9.\-]+', host):
        raise RuntimeError('bad host %r' % host)
    wait_answer(g, float(a.get('ready_timeout', 120)))
    r = g.cmd("SNet.InitClient('%s', %d)" % (host, port), timeout=15)
    g.opts.update({'role': 'client', 'connect': '%s:%d' % (host, port)})
    if r.strip() != '1':
        raise RuntimeError('SNet.InitClient answered %s' % r)
    return 'client %s connects to %s:%d as %s; wait kind=joined' % (g.name, host, port, g.opts.get('player', '?'))


def wait_joined(g, timeout, spawn):
    """waits until a client has its own ship (the level of the server loaded); spawn: then Ffa.RestartOnPress(0), as
    the Fire key on the help screen, and waits until the ship is active"""
    end = time.time() + timeout
    ship = None
    while time.time() < end and g.running():
        try:
            lvl, ship = answers(g, ['Scrap.GetLevelPath()', 'SNet.GetMyClientShip()'], timeout=2)
            if isinstance(ship, str) and ship.startswith('net'):
                break
        except RuntimeError:
            pass
        time.sleep(0.5)
    else:
        raise RuntimeError('client %s has no ship after %.0f s%s' % (g.name, timeout, '' if g.running() else ' (ended)'))
    g.opts['ship'] = ship
    if not spawn:
        return 'client %s joined %s with ship %s' % (g.name, lvl, ship)
    g.cmd('Ffa.RestartOnPress(0)', timeout=10)
    while time.time() < end and g.running():
        act = answers(g, ['ent %s Active' % ship], timeout=5)[0]
        if isinstance(act, list) and act and act[0][1] == 1:
            return 'client %s joined %s with ship %s, active' % (g.name, lvl, ship)
        time.sleep(0.5)
    raise RuntimeError('ship %s of client %s did not become active in %.0f s' % (ship, g.name, timeout))


ENT_VARS = ('Pos', 'Vel', 'Ang', 'Life', 'Active', 'Descriptor', 'TgType')


def net_id(name):
    m = re.fullmatch(r'net(\d+)', name or '')
    return int(m.group(1)) if m else None


def game_state(g):
    """what a game sees of the network game: level, flags, own ship, scores, the players (ships net<id> with id
    <= 512: players and bots) with their variables, and the count of the other net<id> objects"""
    lvl, flags, t, ship, ents = answers(g, ['Scrap.GetLevelPath()', 'Scrap.GetNetFlags()', 'Scrap.GetTime()',
                                            'SNet.GetMyClientShip()', 'ents vwilr'])
    st = {'name': g.name, 'role': g.opts.get('role', 'game'), 'level': lvl, 'time': t,
          'client': bool(flags[0]), 'server': bool(flags[1]), 'dedicated': bool(flags[2]),
          'ship': ship if isinstance(ship, str) and ship else None}
    names = [n for n in (ents if isinstance(ents, list) else []) if net_id(n) is not None]
    st['objects'] = len([n for n in names if net_id(n) > 512])
    if not (st['client'] or st['server']):
        st['players'], st['scores'] = {}, {}
        return st
    scores = answers(g, ['tc_try(lambda: Ffa.Scorer, (), None)'])[0]
    st['scores'] = scores if isinstance(scores, dict) else {}
    players = sorted(set([n for n in names if net_id(n) <= 512] + list(st['scores'])), key=net_id)
    st['players'] = {}
    if players:
        rows = answers(g, ['ent %s %s' % (n, ' '.join(ENT_VARS)) for n in players])
        for n, row in zip(players, rows):
            st['players'][n] = {k.lower(): v for k, v in row} if isinstance(row, list) else {'error': row}
    return st


def hold_input(games, keys, buttons, seconds):
    """keys and mouse buttons held down together for seconds on every game at once (XTEST, the display of each game)"""
    xis = []
    try:
        for g in games:
            xis.append(XInput(g))
        codes = [[xi.keycode(k) for k in keys] for xi in xis]
        for xi, cs in zip(xis, codes):
            for c in cs:
                xi.xtst.XTestFakeKeyEvent(xi.dpy, c, 1, 0)
            for b in buttons:
                xi.xtst.XTestFakeButtonEvent(xi.dpy, int(b), 1, 0)
            xi.x11.XSync(xi.dpy, 0)
        time.sleep(seconds)
        for xi, cs in zip(xis, codes):
            for b in buttons:
                xi.xtst.XTestFakeButtonEvent(xi.dpy, int(b), 0, 0)
            for c in reversed(cs):
                xi.xtst.XTestFakeKeyEvent(xi.dpy, c, 0, 0)
            xi.x11.XSync(xi.dpy, 0)
    finally:
        for xi in xis:
            xi.__exit__()
    return 'held %s for %.2f s on %s' % (', '.join(list(keys) + ['button %s' % b for b in buttons]) or 'nothing',
                                         seconds, ', '.join('%s (X display %s)' % (g.name, xi.display)
                                                            for g, xi in zip(games, xis)))


def after_shot(g, a, msg):
    content = [text(msg)]
    if a.get('screenshot'):
        content.append(image_content(g.screenshot()))
    return content


def call(name, a):
    if name == 'launch':
        g = game_of(a, create=True)
        b = dict(a)
        settings = dict(a.get('settings') or {})
        player = a.get('player') or (g.name if a.get('connect') else None)
        if player:
            if not re.fullmatch(PLAYER_RX, player):
                raise RuntimeError('player is 1 to 15 letters, digits, _ . [ ]')
            settings['PlayerName'] = player
        if a.get('team') is not None:
            settings['PlayerTeamID'] = int(a['team'])
        b['settings'] = settings
        if a.get('connect'):
            if a.get('map'):
                raise RuntimeError('a client with connect starts in the menu level: no map')
            connect_target(a['connect'])
        msg = g.start(b)
        if player:
            g.opts['player'] = player
        if a.get('connect'):
            try:
                msg = msg.split(';')[0] + '\n' + client_connect(g, a)
            except RuntimeError as e:
                raise RuntimeError('%s; the game runs (quit it)' % e)
        return [text(msg)]
    if name == 'server_launch':
        return [text(server_start(a))]
    if name == 'server_quit':
        return [text(game_of({'name': a.get('name') or 'server'}).stop())]
    if name == 'state':
        names = a.get('names') or [a.get('name') or 'main']
        out = []
        for n in names:
            g = game_of({'name': n})
            try:
                out.append(game_state(g))
            except RuntimeError as e:
                out.append({'name': n, 'error': str(e)})
        return [text(json.dumps(out if a.get('names') else out[0]))]
    if name == 'hold':
        games = [game_of({'name': n}) for n in (a.get('names') or [a.get('name') or 'main'])]
        msg = hold_input(games, a.get('keys') or [], a.get('buttons') or [], float(a.get('seconds', 1.0)))
        return after_shot(games[0], a, msg)
    if name == 'quit':
        if a.get('name') == 'all':
            return [text('\n'.join(g.stop() for g in GAMES.values() if g.proc) or 'no game runs')]
        return [text(game_of(a).stop())]
    if name == 'status':
        rows = ['%s: %s, %s%s%s' % (n, g.opts.get('exe', 'native64'), 'runs' if g.running() else 'ended',
                                        ', map ' + g.opts['map'] if g.opts.get('map') else '',
                                        ', server %s port %s' % (g.opts.get('mode'), g.opts.get('port'))
                                        if g.opts.get('role') == 'server' else
                                        ', client of %s' % g.opts['connect'] if g.opts.get('connect') else '')
                for n, g in GAMES.items()]
        return [text('\n'.join(rows) or 'no game')]
    if name == 'wait':
        g = game_of(a)
        kind, timeout = a['kind'], float(a.get('timeout', 120))
        start = time.time()
        if kind == 'time':
            time.sleep(float(a.get('seconds', 1)))
            return after_shot(g, a, 'waited %.1f s' % float(a.get('seconds', 1)))
        if kind == 'log':
            rx = re.compile(a.get('pattern') or '.')
            while time.time() - start < timeout and g.running():
                lines = [l for l in g.log(a.get('which', 'game')).splitlines() if rx.search(l)]
                if lines:
                    return after_shot(g, a, 'after %.1f s: %s' % (time.time() - start, lines[-1].strip()))
                time.sleep(0.5)
            raise RuntimeError('no matching log line in %.0f s%s' % (timeout, '' if g.running() else ' (the game ended)'))
        if not g.channel():
            raise RuntimeError('game %s is the reference exe, which has no test channel: use kind=log or kind=time'
                               % g.name)
        if kind == 'joined':
            return after_shot(g, a, wait_joined(g, timeout, a.get('spawn')))
        while time.time() - start < timeout and g.running():
            try:
                level = g.cmd('Scrap.GetLevelPath()', timeout=2)
                return after_shot(g, a, 'game %s runs after %.1f s, level %s' % (g.name, time.time() - start, level))
            except RuntimeError:
                pass
        raise RuntimeError('game %s did not answer in %.0f s%s' % (g.name, timeout, '' if g.running() else ' (it ended)'))
    if name == 'cmd':
        g = game_of(a)
        return after_shot(g, a, g.cmd(a['lines'], float(a.get('timeout', 10))))
    if name == 'screenshot':
        g = game_of(a)
        return [image_content(g.screenshot(), crop=a.get('crop'), zoom=float(a.get('zoom', 1.0)))]
    if name == 'compare':
        games = [game_of({'name': n}) for n in a['names']]
        ims = [picture(g.screenshot(), a.get('crop')) for g in games]
        if any(i is None for i in ims):
            raise RuntimeError('compare needs Pillow')
        labels = ['%s (%s)' % (g.name, g.opts.get('exe', 'native64')) for g in games]
        return [image_content(im=strip(ims, labels))]
    if name == 'log':
        g = game_of(a)
        lines = g.log(a.get('which', 'game')).splitlines()
        if a.get('pattern'):
            rx = re.compile(a['pattern'])
            lines = [l for l in lines if rx.search(l)]
        return [text('\n'.join(lines[-int(a.get('lines', 40)):]) or '(empty)')]
    if name == 'trace':
        g = game_of(a)
        lines = g.log('engine').splitlines()
        tr = [l for l in lines if re.match(r'(draw |  st\d|target |clear |ps |copyrects )', l)]
        if not tr:
            raise RuntimeError('no traced frame yet (launch with trace=<seconds>, wait that long)')
        count = {}
        target = '?'
        for l in tr:
            if l.startswith('target '):
                target = l.split()[1]
            elif l.startswith('draw '):
                ps = re.search(r' ps (\d+)', l)
                k = '%s ps %s' % (target, ps.group(1) if ps else '?')
                count[k] = count.get(k, 0) + 1
        summary = ['%d draws: %s' % (n, k) for k, n in sorted(count.items(), key=lambda kv: -kv[1])]
        body = tr if a.get('full') else tr[:80]
        return [text('\n'.join(summary + [''] + body))]
    if name == 'vkcheck':
        g = game_of(a)
        if not g.opts.get('validation'):
            raise RuntimeError('launch the game with validation=true first')
        try:
            with open(g.wine_log, 'r', errors='replace') as f:
                data = f.read()
        except OSError:
            data = ''
        foreign = foreign_vuids()
        ours = {}
        for m in re.finditer(r'\[ (VUID-[A-Za-z0-9_-]+) \][^\n]*\n([^\n]*)', data):
            if m.group(1) not in foreign:
                n, first = ours.get(m.group(1), (0, m.group(2)))
                ours[m.group(1)] = (n + 1, first)
        total = data.count('Validation Error')
        if not ours:
            return [text('no validation errors of ours (%d of gamescope and Wine left out)' % total)]
        return [text('\n'.join('%d x %s\n    %s' % (n, v, first[:300]) for v, (n, first) in
                               sorted(ours.items(), key=lambda kv: -kv[1][0])))]
    if name == 'menu_dump':
        g = game_of(a)
        rx = re.compile(a['filter']) if a.get('filter') else None
        return after_shot(g, a, menu_dump(g, int(a.get('player', 0)), rx))
    if name == 'open_menu':
        g = game_of(a)
        return after_shot(g, a, menu_open(g, int(a.get('player', 0)), a['menu']))
    if name == 'click':
        g = game_of(a)
        return after_shot(g, a, menu_click(g, int(a.get('player', 0)), a['item']))
    if name == 'key':
        g = game_of(a)
        with XInput(g) as xi:
            for k in a['keys']:
                xi.key(k, float(a.get('hold', 0.08)))
                time.sleep(float(a.get('gap', 0.15)))
            msg = 'pressed %s (X display %s)' % (', '.join(a['keys']), xi.display)
        return after_shot(g, a, msg)
    if name == 'mouse':
        g = game_of(a)
        with XInput(g) as xi:
            xi.move(int(a.get('dx', 0)), int(a.get('dy', 0)))
            if a.get('button'):
                xi.button(int(a['button']), float(a.get('hold', 0.08)))
            msg = 'moved %d,%d%s (X display %s)' % (int(a.get('dx', 0)), int(a.get('dy', 0)),
                                                    ', button %d' % a['button'] if a.get('button') else '', xi.display)
        return after_shot(g, a, msg)
    if name == 'configure':
        global SLOTS
        if 'slots' in a:
            if any(g.running() for g in GAMES.values()):
                raise RuntimeError('quit all games before the slot folder changes')
            SLOTS = os.path.abspath(a['slots'] or os.environ.get('SCRAP_MCP_SLOTS') or os.path.join(DATA, 'build-mcp'))
            GAMES.clear()
            if not a.get('exe'):
                return [text('slots in %s' % SLOTS)]
        if not a.get('exe'):
            raise RuntimeError('configure takes exe (with build) or slots')
        exe = a['exe']
        builds, defaults, binary = (WIN_BUILDS, DEFAULT_WIN_BUILDS, 'ScrapC.exe') if exe in WIN_BUILDS else \
            (NATIVE_BUILDS, DEFAULT_NATIVE_BUILDS, 'ScrapC')
        build = a.get('build') or defaults[exe]
        path = os.path.join(ROOT, build)
        if not os.path.isfile(os.path.join(path, binary)):
            raise RuntimeError('%s has no %s (build it first)' % (path, binary))
        builds[exe] = build
        return [text('%s now uses %s (next launch)' % (exe, path))]
    raise RuntimeError('unknown tool %s' % name)


def main():
    out = sys.stdout

    def reply(msg_id, result=None, error=None):
        msg = {'jsonrpc': '2.0', 'id': msg_id}
        if error is not None:
            msg['error'] = error
        else:
            msg['result'] = result
        out.write(json.dumps(msg) + '\n')
        out.flush()

    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            msg = json.loads(line)
        except ValueError:
            continue
        method, msg_id, params = msg.get('method'), msg.get('id'), msg.get('params') or {}
        if msg_id is None:
            continue
        if method == 'initialize':
            asked = params.get('protocolVersion')
            reply(msg_id, {'protocolVersion': asked if asked in PROTOCOLS else PROTOCOLS[0], 'capabilities': {'tools': {}},
                           'serverInfo': {'name': 'scrap-mcp', 'title': 'Scrapland', 'version': VERSION},
                           'instructions': INSTRUCTIONS})
        elif method == 'ping':
            reply(msg_id, {})
        elif method == 'tools/list':
            reply(msg_id, {'tools': TOOLS})
        elif method == 'tools/call':
            name = params.get('name')
            if not any(t['name'] == name for t in TOOLS):
                reply(msg_id, error={'code': -32602, 'message': 'unknown tool %s' % name})
                continue
            try:
                reply(msg_id, {'content': call(name, params.get('arguments') or {}), 'isError': False})
            except Exception as e:
                reply(msg_id, {'content': [text('%s: %s' % (type(e).__name__, e))], 'isError': True})
        else:
            reply(msg_id, error={'code': -32601, 'message': 'unknown method %s' % method})
    for g in list(GAMES.values()):
        if g.proc:
            g.stop()


if __name__ == '__main__':
    main()
