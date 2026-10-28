/*
 * Objects/floatobject.c with the two places where the exe computes on the x87 stack (pyx87.h), so with the 24 bit
 * precision that Direct3D sets:
 * - float_hash (rva 0x2df070): frexp returns in st(0) and "fractpart * 2147483648.0" is an fmul there (rva
 *   0x2df139). hash(0.1) is 1717888640 in the game, not -1718085018.
 * - float_pow (rva 0x2df4b0): powu returns in st(0) and "1./powu(iv, -intw)" of an integer exponent below 0 is an
 *   fdivrp (rva 0x2df5bf). 10.0 ** -3 is 0.0010000000474974513 in the game.
 * The type and the number methods of float get these at start, the rest stays the source's.
 */
#include "Objects/floatobject.c"
#include "pyx87.h"

static long Float_Hash(PyFloatObject *v) {
	double intpart, fractpart;
	int expo;
	long x;

	fractpart = modf(v->ob_fval, &intpart);
	if (fractpart == 0.0) {
		if (intpart > 0x7fffffffL || -intpart > 0x7fffffffL) {
			PyObject *w = PyLong_FromDouble(v->ob_fval);
			if (w == NULL) {
				return -1;
			}
			x = PyObject_Hash(w);
			Py_DECREF(w);
			return x;
		}
		x = (long)intpart;
	} else {
		long hipart;
		fractpart = X87_Mul(frexp(fractpart, &expo), 2147483648.0);
		hipart = (long)fractpart;
		fractpart = (fractpart - (double)hipart) * 2147483648.0;
		x = hipart + (long)fractpart + (long)intpart + (expo << 15);
	}
	if (x == -1) {
		x = -2;
	}
	return x;
}

static PyObject *Float_Pow(PyFloatObject *v, PyObject *w, PyFloatObject *z) {
	double iv = v->ob_fval;
	double iw = ((PyFloatObject *)w)->ob_fval;
	long intw = (long)iw;
	double ix;

	if (iw != intw || intw <= -10000 || intw >= 0) {
		return float_pow(v, w, z);
	}
	errno = 0;
	ix = X87_Div(1.0, powu(iv, -intw));
	CHECK(ix);
	if (errno != 0) {
		PyErr_SetFromErrno(PyExc_OverflowError);
		return NULL;
	}
	if ((PyObject *)z != Py_None) {
		ix = fmod(ix, z->ob_fval);
		if (ix != 0 && ((iv < 0 && z->ob_fval > 0) || (iv > 0 && z->ob_fval < 0))) {
			ix += z->ob_fval;
		}
	}
	return PyFloat_FromDouble(ix);
}

__attribute__((constructor)) static void Float_UseExeX87(void) {
	PyFloat_Type.tp_hash = (hashfunc)Float_Hash;
	float_as_number.nb_power = (ternaryfunc)Float_Pow;
}
