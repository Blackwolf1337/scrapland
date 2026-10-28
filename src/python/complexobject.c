/*
 * Objects/complexobject.c with the hash of the exe (complex_hash, rva 0x30d790): like float_hash, the products of
 * the frexp results with 2**31 are fmuls on the x87 stack (rva 0x30d877 and 0x30d8f4), with the 24 bit precision
 * that Direct3D sets (pyx87.h). PyComplex_Type gets it at start.
 */
#include "Objects/complexobject.c"
#include "pyx87.h"

/* the source's hash of one part, with the x87 product */
static long Complex_HashPart(double fractpart, double intpart) {
	int expo;
	long hipart;

	fractpart = X87_Mul(frexp(fractpart, &expo), 2147483648.0);
	hipart = (long)fractpart;
	fractpart = (fractpart - (double)hipart) * 2147483648.0;
	return hipart + (long)fractpart + (long)intpart + (expo << 15);
}

static long Complex_Hash(PyComplexObject *v) {
	double intpart, fractpart;
	long x;

	fractpart = modf(v->cval.real, &intpart);
	if (fractpart == 0.0 && v->cval.imag == 0.0) {
		if (intpart > 0x7fffffffL || -intpart > 0x7fffffffL) {
			PyObject *w = PyLong_FromDouble(v->cval.real);
			if (w == NULL) {
				return -1;
			}
			x = PyObject_Hash(w);
			Py_DECREF(w);
			return x;
		}
		x = (long)intpart;
	} else {
		x = Complex_HashPart(fractpart, intpart);
		if (v->cval.imag != 0.0) {
			fractpart = modf(v->cval.imag, &intpart);
			x ^= Complex_HashPart(fractpart, intpart);
		}
	}
	if (x == -1) {
		x = -2;
	}
	return x;
}

__attribute__((constructor)) static void Complex_UseExeHash(void) {
	PyComplex_Type.tp_hash = (hashfunc)Complex_Hash;
}
