# The float and double to unsigned int conversions of the MSVC runtime (__ftoui3 rva 0x314310, __dtoui3 rva 0x3144f1;
# src/standalone/crtvec.c in ScrapC.exe). SDTest.ConvU calls them with the argument bits in xmm0 and returns the
# result in eax and errno, so the C of the standalone matches the original here (execfile'd by sdcall.py, Python 1.5.2).
# The result is printed signed (a wrapped or negative conversion), errno beside it (0, 33 EDOM, 34 ERANGE).
cvFtoUI = 0x314310
cvDtoUI = 0x3144f1
# a buffer for the bits of a representable value (operator new, 0x133650); memcpy (0x3197d0) writes it, Peek reads
cvbuf = SDTest.Call(0x133650, 'ii', 16)[0]

def cvFbits(v):
	SDTest.Call(0x3197d0, 'iiFi', cvbuf, (v,), 4)
	return SDTest.Peek(cvbuf, '@i')

def cvDbits(v):
	SDTest.Call(0x3197d0, 'iiDi', cvbuf, v, 8)
	return SDTest.Peek(cvbuf, '@i'), SDTest.Peek(cvbuf + 4, '@i')

# float: value, then the three special bit patterns (+inf 0x7f800000, nan 0x7fc00000, -inf 0xff800000 signed)
for cvv in (0.0, 1.5, 3.5, 255.0, 4000000000.0, -1.5, -0.5):
	check('ftoui' + fmt((cvv,)), lambda b=cvFbits(cvv): SDTest.ConvU(cvFtoUI, b, 0, 0))
# 1e10 as a float is about 1e10, within range of a 64 bit truncation
check('ftoui 1e10', lambda b=cvFbits(1e10): SDTest.ConvU(cvFtoUI, b, 0, 0))
check('ftoui +inf', lambda: SDTest.ConvU(cvFtoUI, 0x7f800000, 0, 0))
check('ftoui nan', lambda: SDTest.ConvU(cvFtoUI, 0x7fc00000, 0, 0))
check('ftoui -inf', lambda: SDTest.ConvU(cvFtoUI, -8388608, 0, 0))

# double: representable values through the buffer, then the specials by bits
for cvv in (0.0, 1.5, 4000000000.0, 10000000000.0, -2.5, 1e20):
	cvlh = cvDbits(cvv)
	check('dtoui' + fmt((cvv,)), lambda lh=cvlh: SDTest.ConvU(cvDtoUI, lh[0], lh[1], 1))
check('dtoui +inf', lambda: SDTest.ConvU(cvDtoUI, 0, 0x7ff00000, 1))
check('dtoui nan', lambda: SDTest.ConvU(cvDtoUI, 0, 0x7ff80000, 1))
check('dtoui -inf', lambda: SDTest.ConvU(cvDtoUI, 0, -1048576, 1))
