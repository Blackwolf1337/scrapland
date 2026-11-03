/*
 * Objects/intobject.c with an integer division that is right for INT_MIN: i_divmod of the source computes -(-x / y),
 * whose negation wraps for INT_MIN with gcc (-fwrapv), so pow(-2, 31, 2147483647) and friends came out wrong. One
 * division of x by y and the floor correction give the exact result (what MSVC made of the source). The number
 * methods of int take it at start; INT_MIN / -1 still traps in idiv. The repr of the source writes into 20 bytes,
 * one short for the smallest 64-bit long.
 */
#include "Objects/intobject.c"

static int Int_DivMod(PyIntObject *x, PyIntObject *y, long *pd, long *pm) {
	long xi = x->ob_ival;
	long yi = y->ob_ival;

	if (yi == 0) {
		PyErr_SetString(PyExc_ZeroDivisionError, "integer division or modulo");
		return -1;
	}
	long d = xi / yi;
	long m = xi - d * yi;
	if ((m < 0 && yi > 0) || (m > 0 && yi < 0)) {
		m += yi;
		d -= 1;
	}
	*pd = d;
	*pm = m;
	return 0;
}

static PyObject *Int_Div(PyIntObject *x, PyIntObject *y) {
	long d, m;
	return Int_DivMod(x, y, &d, &m) < 0 ? NULL : PyInt_FromLong(d);
}

static PyObject *Int_Mod(PyIntObject *x, PyIntObject *y) {
	long d, m;
	return Int_DivMod(x, y, &d, &m) < 0 ? NULL : PyInt_FromLong(m);
}

static PyObject *Int_DivModTuple(PyIntObject *x, PyIntObject *y) {
	long d, m;
	return Int_DivMod(x, y, &d, &m) < 0 ? NULL : Py_BuildValue("(ll)", d, m);
}

/*
 * int_pow calls the exe's i_divmod (rva 0x2de700, the folded one) for its final modulo, so with an
 * INT_MIN product there (pow(-2147483648, 1, 7)) the source's i_divmod gives another result. The loop of the
 * source, with Int_DivMod.
 */
static PyObject *Int_Pow(PyIntObject *v, PyIntObject *w, PyIntObject *z) {
	long iv = v->ob_ival;
	long iw = w->ob_ival;
	long iz = 0;

	if (iw < 0) {
		PyErr_SetString(PyExc_ValueError, "integer to the negative power");
		return NULL;
	}
	if ((PyObject *)z != Py_None) {
		iz = z->ob_ival;
		if (iz == 0) {
			PyErr_SetString(PyExc_ValueError, "pow(x, y, z) with z==0");
			return NULL;
		}
	}
	long temp = iv;
	long ix = 1;
	while (iw > 0) {
		long prev = ix;
		if (iw & 1) {
			ix = ix * temp;
			if (temp == 0) {
				break;
			}
			if (ix / temp != prev) {
				return err_ovf("integer pow()");
			}
		}
		iw >>= 1;
		if (iw == 0) {
			break;
		}
		prev = temp;
		temp *= temp;
		if (prev != 0 && temp / prev != prev) {
			return err_ovf("integer pow()");
		}
		if (iz) {
			ix = ix % iz;
			temp = temp % iz;
		}
	}
	if (iz) {
		PyObject *t1 = PyInt_FromLong(ix);
		PyObject *t2 = PyInt_FromLong(iz);
		long d, m;
		if (t1 == NULL || t2 == NULL || Int_DivMod((PyIntObject *)t1, (PyIntObject *)t2, &d, &m) < 0) {
			Py_XDECREF(t1);
			Py_XDECREF(t2);
			return NULL;
		}
		Py_DECREF(t1);
		Py_DECREF(t2);
		ix = m;
	}
	return PyInt_FromLong(ix);
}

static PyObject *Int_Repr(PyIntObject *v) {
	char buf[32];

	snprintf(buf, sizeof(buf), "%ld", v->ob_ival);
	return PyString_FromString(buf);
}

__attribute__((constructor)) static void Int_UseExeDivision(void) {
	PyInt_Type.tp_repr = (reprfunc)Int_Repr;
	int_as_number.nb_divide = (binaryfunc)Int_Div;
	int_as_number.nb_remainder = (binaryfunc)Int_Mod;
	int_as_number.nb_divmod = (binaryfunc)Int_DivModTuple;
	int_as_number.nb_power = (ternaryfunc)Int_Pow;
}
