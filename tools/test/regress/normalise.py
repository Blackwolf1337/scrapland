# The normaliser of the regression runner: one function, norm(text), applied to every reply of a case line in the
# pack runs and in the C run before they are compared. Each rule removes a difference that is known not to be a port
# difference. Lines that still vary between the pack runs are handled by the comparison (compare.py), not here.
#
# Rules, in the order they are applied:
#  R1 function type   ('Name', 'py') and ('Name', 'C'): the case helpers list whether a module function is Python
#                     or C; the pack side is py, the C side C by design.             -> ('Name', T)
#  R2 addresses       "<function f at 557538fc39d0>", "<X instance at 0x55..>": heap addresses.      -> "at X>"
#  R3 arity texts     the TypeError texts of a wrong argument count or an unknown keyword differ between a Python
#                     function ("not enough arguments; expected 1, got 0", "too many arguments; ...", "unexpected
#                     keyword argument: x") and a C function ("function requires exactly 1 argument; 0 given",
#                     "no arguments expected", "x is an invalid keyword argument for this function", "takes no
#                     keyword arguments"); accepted by the brief. Also the text of a wrong argument type, which
#                     Python's string operations ("argument 1: expected read-only character buffer, int found") and
#                     PyArg_ParseTuple ("argument 1: expected string, int found") word differently.  -> "ARITY"
#  R4 level paths     the case of a level path depends on how the case or the slot named it (Levels/Downtown,
#                     Levels/DownTown, levels/police).                              -> lower case "levels/<name>"
#  R5 C type names    reprs of C function objects: "<built-in function F>" for a C port and "<function F at X>"
#                     for the script function it replaced (both are the same function of the module).
#                                                                                   -> "<function F>"
#  R6 bound methods   "<method C.f of C instance at X>" stays (R2 drops the address).
#  R7 blanks          trailing blanks of each line and CR characters.
#  R8 type names      <type 'builtin_function_or_method'> of a C port against <type 'function'> (as R1, R5), also
#                     the quoted type name of type(f).__name__.                    -> "<type 'function'>", "'function'"
#  R9 C-internal calls a recorder in a case replaces a module function (Speech.RemoteComunication) and lists the
#                     calls made through the module; C code calls the C function directly, so the recorder misses
#                     it and sees what it schedules instead. Removed from recorder lists: the engine's alarm taunt
#                     ('Speech.RemoteComunication', ('Generics_PoliceTaunt_1' or '_2', 'Police', 1), {}) (AlarmStatus
#                     -> Speech.LaunchTaunt, also skipped at random when it equals the last taunt) and the
#                     ('sched', t, 'fn:ScheduledVoiceCallback', (channel, rtime)) entries of a running
#                     RemoteComunication (and the PlayVoiceScheduled record of a Scene line), the
#                     MultiplayerUpdatePing that the C RacerScorer.ShowScorer schedules with Scheduler_Add directly
#                     (round 8b), the taunts of
#                     Speech.LaunchTaunt (('Generics_<X>Taunt_<n>', '<X>', 1)), and
#                     ('Scrap.Rand', (a, b)) records (the C ports call Scrap_RandRange, the body of Scrap.Rand,
#                     directly, as the brief says).
#  R13 idle actions   the idle animation of a character is picked at random: 'Relax', 'Relax0', 'Relax1' ...
#                                                                                   -> "'Relax'"
#  R10 dict order     the repr order of a Python 1.5.2 dict follows the hash slots and, on collisions, the insertion
#                     order; a C port that builds the same keyword dict in another order (RaceMaker.InitRace(...,
#                     numply=4, userOnShip=0, waypointlimit=15) recorded with its **kw) prints another order. The
#                     items of a flat dict repr (string keys, values without brackets) are sorted by key.
#  R11 entity numbers the engine numbers the entities it names itself with a global counter ('Parked190',
#                     'LonelyMerc167', 'Police16', 'net789', 'ent:Parked190' in recorder lists); the count depends on
#                     every entity made before (timing, traffic, temporary entities of the scripts). A quoted name of
#                     letters followed by two or more digits.                       -> "'Parked#'"
#  R12 drops          MisItems.OnVehicleDeat drops a coin when Scrap.Rand(0, 1) > 0.6 (the coin is deleted after 60 s,
#                     ActivateCoin after 1.1 s), else a box (deleted after 20 s): the two schedule records of a dying
#                     ship.                                                         -> "('sched', DROP)"
#  R14 callback names an engine callback (scorer item OnAccept, OnGainFocus, ...) set to a function is shown as
#                     'Module.Func' for a Python function and as 'Func' for a C function (a built-in has no module);
#                     the same applies to the labels the case recorders give ('Speech.AddRemoteCom'). A quoted
#                     Word.Word string.                                             -> "'Func'"
#  R15 NPC answers    the answer line of an NPC (Speech NPCAnswer) is picked at random from its list.
#                                                                                   -> "('NPCAnswer', ?)"
# Not normalised here on purpose: numbers (timers, positions, counters, random values). A number that differs
# between the two pack runs becomes a wildcard in compare.py; a number that is the same in both pack runs must be the
# same in the C run.
import re

_R1 = re.compile(r"'(py|C)'\)")
_R2 = re.compile(r" at (0x)?[0-9a-fA-F]+>")
_R3 = re.compile(
	r"function requires (exactly|at most|at least) \d+ arguments?; \d+ given"
	r"|no arguments expected"
	r"|(not enough|too many) arguments; expected \d+, got \d+"
	r"|unexpected keyword argument: \w+"
	r"|\w+ is an invalid keyword argument for this function"
	r"|(this function |\w+\(\) )?takes no keyword arguments"
	r"|argument \d+: expected (read-only character buffer|string), \w+ found")
_R4 = re.compile(r"[Ll]evels/[A-Za-z0-9_]+")
_R5 = re.compile(r"<built-in function (\w+)>|<function (\w+) at X>")
_R7 = re.compile(r"[ \t]+$", re.M)
_R9 = re.compile(r"(, )?\('Speech\.RemoteComunication', \('Generics_\w+Taunt_\d+', '\w+', 1\), \{\}\)"
				 r"|(, )?\('sched', [-\d.e+]+, 'fn:ScheduledVoiceCallback', \([^()]*\)\)"
				 r"|(, )?\('Scrap\.Rand', \([^()]*\)\)"
				 r"|(, )?\('(?:Speech\.)?PlayVoiceScheduled', \([^()]*\), \{\}\)"
				 r"|(, )?\('sched', [-\d.e+]+, 'fn:MultiplayerUpdatePing', \(\), None\)")

_R12 = re.compile(r"\('sched', 60\.0, 'fn:Del', \(\)\), \('sched', 1\.1, 'fn:ActivateCoin', \('ent:Usr_Coin_\d+_Euros',\)\)"
				  r"|\('sched', 20\.0, 'fn:Del', \(\)\)")
_R14 = re.compile(r"'[A-Z]\w*\.([A-Za-z_]\w*)'")
_R15 = re.compile(r"\('NPCAnswer', (\"(?:[^\"\\]|\\.)*\"|'(?:[^'\\]|\\.)*')\)")
_R11 = re.compile(r"'((?:ent:)?[A-Za-z]+)\d{2,}'")
_R10 = re.compile(r"\{'[^'{}]*': [^{}\[\]()]*\}")
_R10ITEM = re.compile(r"'[^']*': (?:'[^']*'|[^,']*)")


def _sortdict(m):
	text = m.group(0)
	items = _R10ITEM.findall(text[1:-1])
	if ', '.join(items) != text[1:-1]:
		return text
	return '{' + ', '.join(sorted(items)) + '}'


def norm(s):
	s = s.replace('\r', '')
	s = _R1.sub("T)", s)
	s = _R2.sub(" at X>", s)
	s = _R3.sub("ARITY", s)
	s = _R4.sub(lambda m: m.group(0).lower(), s)
	s = _R5.sub(lambda m: '<function %s>' % (m.group(1) or m.group(2)), s)
	s = _R7.sub('', s)
	s = s.replace("<type 'builtin_function_or_method'>", "<type 'function'>")
	s = s.replace("'builtin_function_or_method'", "'function'")
	s = re.sub(r"'Relax\d'", "'Relax'", s)
	s = _R9.sub('', s).replace('[, ', '[')
	s = _R10.sub(_sortdict, s)
	s = _R11.sub(r"'\1#'", s)
	s = _R12.sub("('sched', DROP)", s)
	s = _R14.sub(r"'\1'", s)
	s = _R15.sub("('NPCAnswer', ?)", s)
	return s


if __name__ == '__main__':
	import sys
	sys.stdout.write(norm(sys.stdin.read()))
