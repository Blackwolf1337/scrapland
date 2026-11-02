#!/usr/bin/env python3
"""scrap-mcp: an MCP server that runs ScrapC.exe (and the frozen reference exe) of this checkout without a window,
so an AI assistant can test it: start named games, run Python in them, wait for the menu, a log line or time, take
screenshots (cropped, zoomed, side by side), read the logs, trace the render calls of one frame and list the Vulkan
validation errors that are ours.

Each game runs in a slot of its own (build/mcp/slot-<name>, made by tools/test/mkgame.sh --into: the game data
linked, Bin, the configs and the Wine prefix copied), through tools/test/wine.sh (gamescope headless, muted). The
game side is tools/mcp/mcpgame.py, started with -PythonExecute. The server speaks the Model Context Protocol over
stdio (JSON-RPC 2.0, one message a line) and needs only the Python standard library; Pillow does crops, zoom and
comparisons.
"""
import base64
import io
import json
import os
import re
import shutil
import signal
import subprocess
import sys
import time

VERSION = '0.1.0'
PROTOCOLS = ('2025-11-25', '2025-06-18', '2025-03-26', '2024-11-05')
ROOT = os.environ.get('SCRAP_MCP_ROOT') or os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
REF_EXE = os.path.join(ROOT, 'build-ref-noexact', 'ScrapC.exe')
SLOTS = os.path.join(ROOT, 'build', 'mcp')
RUNTIME = os.environ.get('XDG_RUNTIME_DIR') or '/run/user/%d' % os.getuid()
INSTRUCTIONS = (
    'Runs ScrapC.exe of the Scrapland checkout without a window (gamescope headless, Wine, muted, on the AMD 610M). '
    'launch starts a named game (exe c = the build, ref = the frozen reference; map= loads a level), wait kind=ready '
    'waits until the menu or the level runs, python runs code in the game (Scrap, SDTest), screenshot and compare show '
    'the window, log reads log.html or scrapdecomp.log, trace and vkcheck need launch with trace= or validation=true. '
    'quit ends a game. Build first (cmake --build build); launch copies build/ScrapC.exe into the slot.')


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

    def prepare(self, exe):
        if not os.path.isdir(os.path.join(self.game, 'Bin')) or not os.path.isdir(self.prefix):
            r = subprocess.run([os.path.join(ROOT, 'tools', 'test', 'mkgame.sh'), '--into', self.dir, ROOT],
                               capture_output=True, text=True)
            if r.returncode:
                raise RuntimeError('mkgame.sh failed: %s' % (r.stderr or r.stdout))
        binDir = os.path.join(self.game, 'Bin')
        src = os.path.join(ROOT, 'build', 'ScrapC.exe') if exe == 'c' else REF_EXE
        if not os.path.isfile(src):
            raise RuntimeError('%s is missing (build first)' % src)
        shutil.copy2(src, os.path.join(binDir, 'ScrapC.exe' if exe == 'c' else 'ScrapRef.exe'))
        ff = os.path.join(ROOT, 'build', 'ffmpeg', 'install', 'bin')
        if os.path.isdir(ff):
            for n in os.listdir(ff):
                if n.endswith('.dll'):
                    shutil.copy2(os.path.join(ff, n), binDir)
        with open(os.path.join(binDir, 'scrapdecomp.ini'), 'w') as f:
            f.write('[general]\ntestmod=1\n')
        shutil.copy2(os.path.join(ROOT, 'tools', 'mcp', 'mcpgame.py'), self.game)
        for n in ('mcp_cmd.txt', 'mcp_reply.txt', 'log.html'):
            p = os.path.join(self.game, n)
            if os.path.exists(p):
                os.remove(p)

    def start(self, a):
        if self.running():
            raise RuntimeError('game %s runs already; quit it first' % self.name)
        exe = a.get('exe', 'c')
        if exe not in ('c', 'ref'):
            raise RuntimeError("exe is 'c' or 'ref'")
        self.prepare(exe)
        self.opts = dict(a)
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
        args = ['ScrapC.exe' if exe == 'c' else 'ScrapRef.exe', '-fullscreen:0']
        if a.get('map'):
            m = a['map']
            args.append('-imap:' + (m if '/' in m else 'Levels/' + m))
        args += list(a.get('args') or [])
        args.append('-PythonExecute:execfile(%s)' % '+'.join('chr(%d)' % ord(c) for c in script))
        before = time.time() - 0.5
        self.proc = subprocess.Popen([os.path.join(ROOT, 'tools', 'test', 'wine.sh')] + args, env=env,
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

    def stop(self):
        if not self.proc:
            return 'game %s does not run' % self.name
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

    def python(self, code, timeout=10.0):
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
         'exe': {'type': 'string', 'enum': ['c', 'ref'], 'description': 'c: build/ScrapC.exe (default), ref: the frozen '
                 'reference build-ref-noexact/ScrapC.exe (Direct3D 8 through Wine)'},
         'map': {'type': 'string', 'description': 'a level for -imap: (FZ or Levels/FZ)'},
         'args': {'type': 'array', 'items': {'type': 'string'}, 'description': 'more arguments of the game'},
         'env': {'type': 'object', 'description': 'more environment variables (SCRAP_VK_DEBUG, SCRAP_RD_DUMP, ...)'},
         'validation': {'type': 'boolean', 'description': 'the Vulkan validation layer on (see vkcheck)'},
         'trace': {'type': 'number', 'description': 'SCRAP_VK_TRACE: the render calls of the first frame this many '
                   'seconds after the device go into scrapdecomp.log (see trace)'},
         'size': {'type': 'string', 'description': 'the display of gamescope, default 1280x720'},
         'timeout': {'type': 'number', 'description': 'seconds until the game is killed, default 3600'}})},
    {'name': 'quit', 'description': 'End a game (all of them with name=all).', 'inputSchema': schema({'name': NAME})},
    {'name': 'status', 'description': 'The games and whether they run.', 'inputSchema': schema()},
    {'name': 'wait', 'description': 'Wait until the game answers Python (kind=ready: the menu or the level runs), a '
     'log line matches (kind=log), or for seconds (kind=time).', 'inputSchema': schema({
         'name': NAME, 'kind': {'type': 'string', 'enum': ['ready', 'log', 'time']},
         'pattern': {'type': 'string', 'description': 'regex for kind=log'},
         'which': {'type': 'string', 'enum': ['game', 'engine'], 'description': 'kind=log: game = log.html (default), '
                   'engine = scrapdecomp.log'},
         'seconds': {'type': 'number', 'description': 'kind=time'},
         'timeout': {'type': 'number', 'description': 'default 120'},
         'screenshot': {'type': 'boolean', 'description': 'a screenshot after the wait'}}, ['kind'])},
    {'name': 'python', 'description': 'Run Python 1.5.2 in the game (modules Scrap, SDTest, ...). An expression gives '
     'its repr; statements give the repr of the global result, or ok.', 'inputSchema': schema({
         'name': NAME, 'code': {'type': 'string'}, 'timeout': {'type': 'number', 'description': 'default 10'},
         'screenshot': {'type': 'boolean'}}, ['code'])},
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
]


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
        rows = ['%s: %s, %s%s' % (n, g.opts.get('exe', 'c'), 'runs' if g.running() else 'ended',
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
                level = g.python('Scrap.GetLevelPath()', timeout=2)
                return after_shot(g, a, 'game %s runs after %.1f s, level %s' % (g.name, time.time() - start, level))
            except RuntimeError:
                pass
        raise RuntimeError('game %s did not answer in %.0f s%s' % (g.name, timeout, '' if g.running() else ' (it ended)'))
    if name == 'python':
        g = game_of(a)
        return after_shot(g, a, g.python(a['code'], float(a.get('timeout', 10))))
    if name == 'screenshot':
        g = game_of(a)
        return [image_content(g.screenshot(), crop=a.get('crop'), zoom=float(a.get('zoom', 1.0)))]
    if name == 'compare':
        games = [game_of({'name': n}) for n in a['names']]
        ims = [picture(g.screenshot(), a.get('crop')) for g in games]
        if any(i is None for i in ims):
            raise RuntimeError('compare needs Pillow')
        labels = ['%s (%s)' % (g.name, g.opts.get('exe', 'c')) for g in games]
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
