# console command table check (stage 5 of the interpreter removal): Init._co(line) runs a console line the way
# Console_Execute did before the table (the Python code it built, run in __main__), Init._cn(line) runs it through
# Scrap.ConsoleExec (the C table). Both return the lines the line wrote to log.html.
import Scrap, Init, string, sys
Scrap.Set('AlwaysFlushLog', 1)
Scrap.Set('debug', 2)
Scrap.ConsoleError('fcon ready\n')
def _fcpos():
	f = open('log.html', 'r')
	f.seek(0, 2)
	n = f.tell()
	f.close()
	return n
def _fcsince(n):
	f = open('log.html', 'r')
	f.seek(n)
	d = f.read()
	f.close()
	out = ''
	i = 0
	while 1:
		j = string.find(d, '<', i)
		if j < 0:
			out = out + d[i:]
			break
		out = out + d[i:j]
		k = string.find(d, '>', j)
		if k < 0:
			break
		i = k + 1
	out = string.replace(out, '&lt;', '<')
	out = string.replace(out, '&gt;', '>')
	out = string.replace(out, '&amp;', '&')
	return string.split(out, '\n')
def _fcrun(code):
	d = __import__('__main__').__dict__
	try:
		exec code + '\n' in d
	except:
		sys.stderr.write('Traceback: %s: %s\n' % (sys.exc_type, sys.exc_value))
def _fcold(cmd):
	numeric = '"\'+-.0123456789'
	if cmd[:1] == '/':
		name = cmd[1:]
		k = string.find(name, ' ')
		if k >= 0:
			val = string.strip(name[k + 1:])
			name = string.strip(name[:k])
			code = 'import QuickConsole;QuickConsole.%s(%s)' % (name, val)
		else:
			code = 'import QuickConsole;QuickConsole.%s()' % name
	elif cmd[:1] == ':':
		name = cmd[1:]
		k = string.find(name, ' ')
		if k >= 0:
			val = string.strip(name[k + 1:])
			name = string.strip(name[:k])
			if val[:1] and val[:1] in numeric:
				code = "if Scrap.Set('%s',%s):\n print '%s = %s'" % (name, val, name, val)
			else:
				code = "if Scrap.Set('%s','%s'):\n print '%s = %s'" % (name, val, name, val)
		else:
			name = string.strip(name)
			code = "Scrap.Print('%s = '+`Scrap.Get('%s')`+' (default = '+`Scrap.Def('%s')`+') ### '+Scrap.Des('%s')+'\\n')" % (name, name, name, name)
	elif cmd[:1] == '?':
		code = 'Scrap.ShowGVars("%s")' % cmd[1:]
	else:
		code = cmd
	_fcrun(code)
def _co(line):
	n = _fcpos()
	_fcold(line)
	return _fcsince(n)
def _cn(line):
	n = _fcpos()
	Scrap.ConsoleExec(line)
	return _fcsince(n)
import imp
sys.modules['QuickConsole'] = imp.load_source('QuickConsole', 'qc_lf.py')
Init._co = _co
Init._cn = _cn
