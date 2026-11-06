# The module code of string (Data.packed/scripts/python/string.pyc, the string.py of Python 1.5.2), written by hand
# from the output of tools/modules/pyc2mod.py. The for loop and the try statement, which the lines cannot say, are
# lines with the same result, and the Python functions that "from strop import *" replaces at the end are taken
# from strop where the script defines them, so the keys come in the order of the script. The functions string.py
# keeps are C (src/game/Script/String.c).
# A pack in mods/ with scrapc/modules/string.mod replaces this file.

let __doc__ = 'Common string manipulations.\n\nPublic module variables:\n\nwhitespace -- a string containing all characters considered whitespace\nlowercase -- a string containing all characters considered lowercase letters\nuppercase -- a string containing all characters considered uppercase letters\nletters -- a string containing all characters considered letters\ndigits -- a string containing all characters considered decimal digits\nhexdigits -- a string containing all characters considered hexadecimal digits\noctdigits -- a string containing all characters considered octal digits\n\n'
let whitespace = ' \t\n\r\013\014'
let lowercase = 'abcdefghijklmnopqrstuvwxyz'
let uppercase = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ'
let letters = (lowercase + uppercase)
let digits = '0123456789'
let hexdigits = ((digits + 'abcdef') + 'ABCDEF')
let octdigits = '01234567'
let _idmap = ''
# for i in range(256): _idmap = _idmap + chr(i): the first round decides the places of the keys,
# the later ones only change the value
let i = 255
let _idmap = '\000\001\002\003\004\005\006\007\010\011\012\013\014\015\016\017\020\021\022\023\024\025\026\027\030\031\032\033\034\035\036\037 !"#$%&\047()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\134]^_`abcdefghijklmnopqrstuvwxyz{|}~\177\200\201\202\203\204\205\206\207\210\211\212\213\214\215\216\217\220\221\222\223\224\225\226\227\230\231\232\233\234\235\236\237\240\241\242\243\244\245\246\247\250\251\252\253\254\255\256\257\260\261\262\263\264\265\266\267\270\271\272\273\274\275\276\277\300\301\302\303\304\305\306\307\310\311\312\313\314\315\316\317\320\321\322\323\324\325\326\327\330\331\332\333\334\335\336\337\340\341\342\343\344\345\346\347\350\351\352\353\354\355\356\357\360\361\362\363\364\365\366\367\370\371\372\373\374\375\376\377'
let _lower = ((_idmap[:ord('A')] + lowercase) + _idmap[(ord('Z') + 1):])
let _upper = ((_idmap[:ord('a')] + uppercase) + _idmap[(ord('z') + 1):])
let _swapcase = ((_upper[:ord('A')] + lowercase) + _upper[(ord('Z') + 1):])
del i
let index_error = ValueError
let atoi_error = ValueError
let atof_error = ValueError
let atol_error = ValueError
from strop import lower upper swapcase strip lstrip rstrip split splitfields join joinfields
def index
def rindex
from strop import count find rfind
let _safe_env = {'__builtins__': {}}
let _re = None
from strop import atof atoi atol
def ljust
def rjust
def center
def zfill
from strop import expandtabs translate capitalize
def capwords
let _idmapL = None
from strop import maketrans replace
# try: from strop import * ... except ImportError: pass (strop is built in, the import does not fail)
from strop import *
let letters = (lowercase + uppercase)
