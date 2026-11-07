#!/usr/bin/env python3
"""scrap-mcp: an MCP server that runs ScrapC.exe (and the frozen reference exe) of this checkout without a window,
so an AI assistant can test it: start named games, run Python in them, wait for the menu, a log line or time, take
screenshots (cropped, zoomed, side by side), read the logs, trace the render calls of one frame and list the Vulkan
validation errors that are ours.

Each game runs in a slot of its own (build-mcp/slot-<name>, made by tools/test/mkgame.sh --into: the game data
linked, Bin, the configs and the Wine prefix copied), through tools/test/wine.sh (gamescope headless, muted). The
game side is the test channel of the game (src/game/Global/TestChannel.c, tools/test/channel.py: no Python of the
tool runs in the game), or with harness=python the old tools/mcp/mcpgame.py, started with -PythonExecute: the only
one of the reference exe, and for one round the one of a C build made with the interpreter (cmake
-DSCRAP_NO_PYTHON=OFF; STAGE 7: goes with the interpreter). The default C builds have no interpreter. The server
speaks the Model Context Protocol over stdio (JSON-RPC 2.0, one message a line) and needs only the Python standard
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

VERSION = '0.3.1'
PROTOCOLS = ('2025-11-25', '2025-06-18', '2025-03-26', '2024-11-05')
ROOT = os.environ.get('SCRAP_MCP_ROOT') or os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
REF_EXE = os.path.join(ROOT, 'build-ref-noexact', 'ScrapC.exe')
SLOTS = os.path.join(ROOT, 'build-mcp')
RUNTIME = os.environ.get('XDG_RUNTIME_DIR') or '/run/user/%d' % os.getuid()
INSTRUCTIONS = (
    'Runs ScrapC of the Scrapland checkout without a window (gamescope headless, muted, on the AMD 610M). '
    'launch starts a named game (exe native64 = build-linux64/ScrapC by default, native64asan, c64 = the Windows '
    'build under Wine, ref = the frozen reference; map= loads a level), wait kind=ready waits until the menu or the '
    'level runs. cmd runs test channel commands in the game (values, calls with literal arguments, engine and entity '
    'variables, module dumps, console lines); python runs Python 1.5.2 code (only ref and a build made with the '
    'interpreter). menu_dump lists the items of the open menu, open_menu, click, key and mouse drive it. screenshot and compare show the window, log reads log.html '
    'or scrapdecomp.log, trace and vkcheck need launch with trace= or validation=true. quit ends a game. Build '
    'first; launch copies the build into the slot.')


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


def build_dir(exe):
    """the build folder of a C exe"""
    return os.path.join(ROOT, NATIVE_BUILDS.get(exe) or WIN_BUILDS[exe])


def no_python(exe):
    """whether the build of a C exe was made without the Python interpreter (SCRAP_NO_PYTHON in its CMakeCache.txt)"""
    try:
        with open(os.path.join(build_dir(exe), 'CMakeCache.txt'), errors='replace') as f:
            return 'SCRAP_NO_PYTHON:BOOL=ON' in f.read().split('\n')
    except OSError:
        return False


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
            r = subprocess.run([os.path.join(ROOT, 'tools', 'test', 'mkgame.sh'), '--native', self.game, ROOT],
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
        shutil.copy2(os.path.join(ROOT, 'tools', 'mcp', 'mcpgame.py'), self.game)
        for n in ('mcp_cmd.txt', 'mcp_reply.txt', 'test_cmd.txt', 'test_reply.txt', 'log.html'):
            p = os.path.join(self.game, n)
            if os.path.exists(p):
                os.remove(p)

    def prepare(self, exe):
        self.game = os.path.join(self.dir, 'game')
        if exe in NATIVE_BUILDS:
            return self.prepare_native(exe)
        if not os.path.isdir(os.path.join(self.game, 'Bin')) or not os.path.isdir(self.prefix):
            r = subprocess.run([os.path.join(ROOT, 'tools', 'test', 'mkgame.sh'), '--into', self.dir, ROOT],
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
        shutil.copy2(os.path.join(ROOT, 'tools', 'mcp', 'mcpgame.py'), self.game)
        for n in ('mcp_cmd.txt', 'mcp_reply.txt', 'test_cmd.txt', 'test_reply.txt', 'log.html'):
            p = os.path.join(self.game, n)
            if os.path.exists(p):
                os.remove(p)

    def start(self, a):
        if self.running():
            raise RuntimeError('game %s runs already; quit it first' % self.name)
        exe = a.get('exe', 'native64')
        if exe != 'ref' and exe not in WIN_BUILDS and exe not in NATIVE_BUILDS:
            raise RuntimeError("exe is one of: ref, %s" % ', '.join(list(WIN_BUILDS) + list(NATIVE_BUILDS)))
        harness = a.get('harness') or ('python' if exe == 'ref' else 'channel')
        if harness not in ('channel', 'python') or (exe == 'ref' and harness == 'channel'):
            raise RuntimeError('harness is channel or python (ref: only python)')
        nopy = exe != 'ref' and no_python(exe)
        if harness == 'python' and nopy:
            raise RuntimeError('harness=python runs Python in the game, but %s is built without the interpreter '
                               '(SCRAP_NO_PYTHON, the default): use the channel' % build_dir(exe))
        self.prepare(exe)
        if harness == 'channel' and not self.has_channel(exe):
            raise RuntimeError('the game binary of %s has no test channel (an old build): build it again' % exe)
        self.opts = dict(a, exe=exe, harness=harness, nopy=nopy)
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
        script = 'mcpgame.py'
        args = [] if exe in NATIVE_BUILDS else ['ScrapC.exe' if exe in WIN_BUILDS else 'ScrapRef.exe']
        args.append('-fullscreen:0')
        if a.get('map'):
            m = a['map']
            args.append('-imap:' + (m if '/' in m else 'Levels/' + m))
        args += list(a.get('args') or [])
        if harness == 'channel':
            env['SCRAP_TEST_CHANNEL'] = '1'
        else:
            args.append('-PythonExecute:execfile(%s)' % '+'.join('chr(%d)' % ord(c) for c in script))
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
        """whether the prepared game binary has the test channel (builds from before it get the python harness)"""
        name = 'ScrapC' if exe in NATIVE_BUILDS else 'ScrapC.exe'
        try:
            with open(os.path.join(self.game, 'Bin', name), 'rb') as f:
                return b'test_cmd.txt' in f.read()
        except OSError:
            return False

    def channel(self):
        return self.opts.get('harness', 'channel') == 'channel'

    def cmd(self, lines, timeout=10.0):
        """test channel commands (one or more lines); the answers"""
        if not self.running():
            raise RuntimeError('game %s does not run' % self.name)
        if not self.channel():
            raise RuntimeError('game %s runs with harness=python (no test channel)' % self.name)
        try:
            return channel.send(self.game, lines, timeout=timeout, alive=self.running)
        except channel.NoAnswer:
            raise RuntimeError('no answer from game %s in %.0f s (the menu or level may not run yet, or a level '
                               'loads)' % (self.name, timeout))

    def python(self, code, timeout=10.0):
        if self.opts.get('nopy'):
            raise RuntimeError('game %s has no Python interpreter (SCRAP_NO_PYTHON, the default build): use cmd'
                               % self.name)
        if self.channel():
            # STAGE 7: the channel's py command goes with the interpreter
            return self.cmd('py\n' + code, timeout)
        if not self.running():
            raise RuntimeError('game %s does not run' % self.name)
        self.seq += 1
        reply = os.path.join(self.game, 'mcp_reply.txt')
        tmp = os.path.join(self.game, 'mcp_cmd.tmp')
        with open(tmp, 'w', newline='\r\n') as f:
            f.write('%d\n%s\n' % (self.seq, code))
        os.replace(tmp, os.path.join(self.game, 'mcp_cmd.txt'))
        end = time.time() + timeout
        while time.time() < end and self.running():
            try:
                with open(reply, 'r', errors='replace') as f:
                    data = f.read().replace('\r\n', '\n')
            except OSError:
                data = ''
            head, _, rest = data.partition('\n')
            if head.strip() == str(self.seq) and rest.endswith('.end\n'):
                return rest[:-len('.end\n')].rstrip('\n')
            time.sleep(0.1)
        raise RuntimeError('no answer from game %s in %.0f s (the menu or level may not run yet, or the world timer '
                           'is stopped)' % (self.name, timeout))

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
                 'build-ref-noexact/ScrapC.exe (i686, Direct3D 8 through Wine)'},
         'map': {'type': 'string', 'description': 'a level for -imap: (FZ or Levels/FZ)'},
         'args': {'type': 'array', 'items': {'type': 'string'}, 'description': 'more arguments of the game'},
         'env': {'type': 'object', 'description': 'more environment variables (SCRAP_VK_DEBUG, SCRAP_RD_DUMP, ...)'},
         'validation': {'type': 'boolean', 'description': 'the Vulkan validation layer on (see vkcheck)'},
         'harness': {'type': 'string', 'enum': ['channel', 'python'], 'description': 'how the tools talk to the game: '
                     'channel (default of the C builds) = the test channel of the game (SCRAP_TEST_CHANNEL=1, answers '
                     'also while the world clock stands), python = the old mcpgame.py through -PythonExecute (answers '
                     'only while the world clock runs; the only one of ref, and for C builds only one made with the '
                     'interpreter, cmake -DSCRAP_NO_PYTHON=OFF; STAGE 7: goes with the interpreter)'},
         'trace': {'type': 'number', 'description': 'SCRAP_VK_TRACE: the render calls of the first frame this many '
                   'seconds after the device go into scrapdecomp.log (see trace)'},
         'size': {'type': 'string', 'description': 'the display of gamescope, default 1280x720'},
         'timeout': {'type': 'number', 'description': 'seconds until the game is killed, default 3600'}})},
    {'name': 'quit', 'description': 'End a game (all of them with name=all).', 'inputSchema': schema({'name': NAME})},
    {'name': 'status', 'description': 'The games and whether they run.', 'inputSchema': schema()},
    {'name': 'wait', 'description': 'Wait until the game answers (kind=ready: the menu or the level runs), a '
     'log line matches (kind=log), or for seconds (kind=time).', 'inputSchema': schema({
         'name': NAME, 'kind': {'type': 'string', 'enum': ['ready', 'log', 'time']},
         'pattern': {'type': 'string', 'description': 'regex for kind=log'},
         'which': {'type': 'string', 'enum': ['game', 'engine'], 'description': 'kind=log: game = log.html (default), '
                   'engine = scrapdecomp.log'},
         'seconds': {'type': 'number', 'description': 'kind=time'},
         'timeout': {'type': 'number', 'description': 'default 120'},
         'screenshot': {'type': 'boolean', 'description': 'a screenshot after the wait'}}, ['kind'])},
    {'name': 'python', 'description': 'Run Python 1.5.2 in the game (modules Scrap, SAI, SVec, ...). An expression gives '
     'its repr; statements give the repr of the global result, or ok. Only ref and a C build made with the '
     'interpreter (cmake -DSCRAP_NO_PYTHON=OFF); the default C builds have none: use cmd.', 'inputSchema': schema({
         'name': NAME, 'code': {'type': 'string'}, 'timeout': {'type': 'number', 'description': 'default 10'},
         'screenshot': {'type': 'boolean'}}, ['code'])},
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
     'server process only (e.g. a worktree), or back to the default; applies to the next launch.', 'inputSchema': schema({
         'exe': {'type': 'string', 'enum': ['native64', 'native64asan', 'c64']},
         'build': {'type': 'string', 'description': 'build folder, absolute or relative to the repo; empty: default'}},
         ['exe'])},
]


# ---- menus: the scorer items of a player (SScorer.Items of ScrapC), item actions, menus by name

MENU_VARS = ('Text', 'X', 'Y', 'W', 'H', 'Visible', 'Alpha', 'Red', 'Green', 'Blue', 'OnAccept', 'Up', 'Down', 'Left',
             'Right')


def py_str(s):
    """A Python 1.5.2 string literal of s (repr of ASCII text is the same in both)."""
    return repr(str(s))


def menu_items(g, player):
    if g.channel():
        out = g.cmd('menu %d' % player, timeout=15)
        if out.startswith('error'):
            raise RuntimeError(out)
        current, rows = ast.literal_eval(out)
        return current, [list(r) for r in rows]
    code = ('import SScorer\n'
            'r = []\n'
            'items = SScorer.Items(%d)\n'
            'if items is None:\n\titems = []\n'
            'for n in items:\n'
            '\trow = [n]\n'
            '\tfor v in %r:\n'
            '\t\trow.append(SScorer.Get(%d, n, v))\n'
            '\tr.append(row)\n'
            'result = (SScorer.GetActual(%d), r)\n') % (player, MENU_VARS, player, player)
    out = g.python(code, timeout=15)
    if out.startswith('error'):
        raise RuntimeError(out)
    return ast.literal_eval(out)


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
    if g.channel():
        if not re.fullmatch(r'\S+', item):
            raise RuntimeError('an item name has no blanks')
        out = g.cmd('click %s %d' % (item, player), timeout=15)
        # the repr, as the python path answers
        return out if out.startswith('error') else repr(out)
    code = ('import SScorer\n'
            'f = SScorer.Get(%d, %s, "OnAccept")\n'
            'if SScorer.Accept(%d, %s):\n\tresult = "called " + f\n'
            'else:\n\tresult = "the item has no OnAccept"\n') % (player, py_str(item), player, py_str(item))
    return g.python(code, timeout=15)


def menu_open(g, player, menu):
    if not re.fullmatch(r'[A-Za-z_][A-Za-z0-9_]*(\.[A-Za-z_][A-Za-z0-9_]*)+', menu):
        raise RuntimeError('menu is module.function, e.g. PCMenu.VideoMenu')
    module = menu.rsplit('.', 1)[0]
    if g.channel():
        out = g.cmd('do %s(%d, "")' % (menu, player), timeout=15)
        return out or repr('opened %s' % menu)
    return g.python('import %s\n%s(%d, "")\nresult = "opened %s"' % (module, menu, player, menu), timeout=15)


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


def after_shot(g, a, msg):
    content = [text(msg)]
    if a.get('screenshot'):
        content.append(image_content(g.screenshot()))
    return content


def call(name, a):
    if name == 'launch':
        g = game_of(a, create=True)
        return [text(g.start(a))]
    if name == 'quit':
        if a.get('name') == 'all':
            return [text('\n'.join(g.stop() for g in GAMES.values() if g.proc) or 'no game runs')]
        return [text(game_of(a).stop())]
    if name == 'status':
        rows = ['%s: %s, %s, %s%s' % (n, g.opts.get('exe', 'native64'), g.opts.get('harness', 'channel'),
                                      'runs' if g.running() else 'ended',
                                      ', map ' + g.opts['map'] if g.opts.get('map') else '') for n, g in GAMES.items()]
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
        while time.time() - start < timeout and g.running():
            try:
                level = g.cmd('Scrap.GetLevelPath()', timeout=2) if g.channel() else \
                    g.python('Scrap.GetLevelPath()', timeout=2)
                return after_shot(g, a, 'game %s runs after %.1f s, level %s' % (g.name, time.time() - start, level))
            except RuntimeError:
                pass
        raise RuntimeError('game %s did not answer in %.0f s%s' % (g.name, timeout, '' if g.running() else ' (it ended)'))
    if name == 'python':
        g = game_of(a)
        return after_shot(g, a, g.python(a['code'], float(a.get('timeout', 10))))
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
