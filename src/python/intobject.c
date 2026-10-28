/*
 * Objects/intobject.c with the integer division of the exe (rva 0x2ddfe0 int_div, 0x2de050 int_mod, 0x2de0b0
 * int_divmod). MSVC compiled i_divmod as one idiv of x by y and the floor correction: it folded the -(-x / y) of
 * the source into x / y, so INT_MIN / y has the exact result there. gcc keeps the negations, which wrap for INT_MIN
 * (-fwrapv) and give another quotient. The number methods of int get the exe's form at start; INT_MIN / -1 traps in
 * idiv in both. The modulo of int_pow still goes through the source's i_divmod, which differs only when a product
 * there is INT_MIN.
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

__attribute__((constructor)) static void Int_UseExeDivision(void) {
	int_as_number.nb_divide = (binaryfunc)Int_Div;
	int_as_number.nb_remainder = (binaryfunc)Int_Mod;
	int_as_number.nb_divmod = (binaryfunc)Int_DivModTuple;
}
