# The game side of the scrap MCP server (tools/mcp/scrap_mcp.py), Python 1.5.2 inside ScrapC.exe, started with
# -PythonExecute. Every 0.1 s of game time (Scrap.AddScheduledFunc, which runs while the world timer runs) it reads
# mcp_cmd.txt in the game directory: "<number>\n<code>". A new number runs the code, an expression is evaluated and
# its repr is the answer, statements are run and the answer is the repr of the global "result" (or 'ok'). The
# answer goes to mcp_reply.txt: "<number>\n<answer>\n.end\n". Python's own files are used, not the game's.
import Scrap
import string
import sys
try:
	import SDTest
except ImportError:
	pass

mcpSeen = -1


def mcpRun(code):
	g = globals()
	try:
		try:
			c = compile(code, 'mcp', 'eval')
		except SyntaxError:
			c = None
		if c is not None:
			return repr(eval(c, g))
		if g.has_key('result'):
			del g['result']
		exec compile(code, 'mcp', 'exec') in g
		if g.has_key('result'):
			return repr(g['result'])
		return 'ok'
	except:
		return 'error: %s: %s' % (sys.exc_type, sys.exc_value)


def mcpPoll():
	global mcpSeen
	try:
		f = open('mcp_cmd.txt', 'r')
		data = f.read()
		f.close()
	except IOError:
		data = ''
	nl = string.find(data, '\n')
	if nl > 0:
		try:
			seq = string.atoi(data[:nl])
		except ValueError:
			seq = mcpSeen
		if seq != mcpSeen:
			mcpSeen = seq
			answer = mcpRun(data[nl + 1:])
			f = open('mcp_reply.txt', 'w')
			f.write('%d\n%s\n.end\n' % (seq, answer))
			f.close()
	Scrap.AddScheduledFunc(Scrap.GetTime() + 0.1, mcpPoll, ())


mcpPoll()
