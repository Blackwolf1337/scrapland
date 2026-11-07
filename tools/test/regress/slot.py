# Slot reset for the regression runner: puts a game slot of the scrap MCP server (build-mcp/slot-<tag>/native64-game,
# made by tools/test/mkgame.sh --native) back to a fixed state before each case, the same for pack and C slots.
#
# What a slot can carry from one run to the next, and what reset() does with it:
#   xdg-data/MercurySteam/Scrapland/   the engine configuration Scrap.cfg (ServerType, ServerMapList, HangarShip1-9,
#                                      iHangarShip, PlayerName, PlayerTeamID, PilotModel, Fraglimit, quick chat lines,
#                                      split screen and net settings) and the save files (Save<n>.sav, SDTest.sav):
#                                      emptied, then template/Scrap.cfg copied in (no save files at all)
#   Scrap.cfg in the game folder       the shipped defaults (mkgame copies it): copied again from the main game folder
#   commonappdata/                     copied again from the main game folder
#   other files and folders            whatever a case or tool wrote into the game folder (the helpers, nmbots.pyc,
#                                      lt.py/lt.pyc, misev.txt, sweep.txt, logs, core dumps, mcp_cmd/mcp_reply,
#                                      log.html, logAI.html): removed (a .py or .pyc there can be imported by a case)
#   Bin/                               the build's ScrapC and FFmpeg libraries are copied in by the launch; anything
#                                      else (scrapdecomp.log, ScrapC.real, ...) removed, scrapdecomp.ini rewritten
#   symlinks to the main game data     kept (read only data: the .packed files, tmp, ...)
#   Video/                             a folder of links to the main Video files without the intro MSE.vds and
#                                      MSE-Short.vds: the engine skips a missing intro video (it skips the other three
#                                      intro videos, missing in the GOG data, the same way), which saves 32 s per start;
#                                      no script reads them
#   slot-<tag>/wine.log, shot.png      removed (the game output of the last run)
# Only paths inside build-mcp/slot-<tag> are touched.
import os
import shutil
import subprocess

HERE = os.path.dirname(os.path.abspath(__file__))
# the checkout with the game data and the slots (regress.py sets SCRAP_MCP_ROOT, as the scrap MCP server reads it)
MAIN = os.environ.get('SCRAP_MCP_ROOT') or os.path.abspath(os.path.join(HERE, '..', '..', '..'))
SLOTS = os.path.join(MAIN, 'build-mcp')
TEMPLATE = os.path.join(HERE, 'template', 'Scrap.cfg')
BIN_KEEP = ('ScrapC', 'Scrap.exe', 'scrapdecomp.ini')
NO_VIDEOS = ('MSE.vds', 'MSE-Short.vds')


def _rm(path, slot):
	real = os.path.realpath(path) if not os.path.islink(path) else os.path.realpath(os.path.dirname(path))
	if not real.startswith(slot + os.sep) and real != slot:
		raise RuntimeError('refusing to remove %s outside %s' % (path, slot))
	if os.path.islink(path) or not os.path.isdir(path):
		os.remove(path)
	else:
		shutil.rmtree(path)


def reset(tag, exe='native64'):
	slot = os.path.join(SLOTS, 'slot-' + tag)
	game = os.path.join(slot, exe + '-game')
	if not os.path.isdir(os.path.join(game, 'Bin')):
		r = subprocess.run([os.path.join(HERE, '..', 'mkgame.sh'), '--native', game, MAIN],
						   capture_output=True, text=True)
		if r.returncode:
			raise RuntimeError('mkgame.sh failed: %s' % (r.stderr or r.stdout))
	slot = os.path.realpath(slot)
	for n in ('wine.log', 'shot.png'):
		p = os.path.join(slot, n)
		if os.path.lexists(p):
			_rm(p, slot)
	maingame = os.path.join(MAIN, 'game')
	for n in os.listdir(game):
		p = os.path.join(game, n)
		if n == 'Video':
			_rm(p, slot)
			continue
		if os.path.islink(p):
			if os.path.realpath(p) != os.path.realpath(os.path.join(maingame, n)):
				_rm(p, slot)
			continue
		if n in ('Bin', 'xdg-data', 'commonappdata'):
			continue
		_rm(p, slot)
	shutil.copy2(os.path.join(maingame, 'Scrap.cfg'), os.path.join(game, 'Scrap.cfg'))
	video = os.path.join(game, 'Video')
	os.mkdir(video)
	for n in os.listdir(os.path.join(maingame, 'Video')):
		if n not in NO_VIDEOS:
			os.symlink(os.path.join(maingame, 'Video', n), os.path.join(video, n))
	for n in os.listdir(maingame):
		p = os.path.join(game, n)
		if n not in ('Bin', 'Scrap.cfg', 'commonappdata', 'log.html', 'logAI.html') and not n.startswith('sd') \
				and not os.path.lexists(p):
			os.symlink(os.path.join(maingame, n), p)
	cad = os.path.join(game, 'commonappdata')
	if os.path.lexists(cad):
		_rm(cad, slot)
	shutil.copytree(os.path.join(maingame, 'commonappdata'), cad, symlinks=True)
	binDir = os.path.join(game, 'Bin')
	for n in os.listdir(binDir):
		if n in BIN_KEEP or n.startswith('lib'):
			continue
		_rm(os.path.join(binDir, n), slot)
	if not os.path.lexists(os.path.join(binDir, 'Scrap.exe')):
		os.symlink(os.path.join(maingame, 'Bin', 'Scrap.exe'), os.path.join(binDir, 'Scrap.exe'))
	xdg = os.path.join(game, 'xdg-data')
	if os.path.lexists(xdg):
		_rm(xdg, slot)
	cfgDir = os.path.join(xdg, 'MercurySteam', 'Scrapland')
	os.makedirs(cfgDir)
	shutil.copy2(TEMPLATE, os.path.join(cfgDir, 'Scrap.cfg'))
	return game


if __name__ == '__main__':
	import sys
	for t in sys.argv[1:]:
		print(reset(t))
