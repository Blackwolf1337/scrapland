/*
 * x87 arithmetic where the MSVC build of the Python in Scrap.exe did it on the x87 stack (a double that a function
 * returned in st(0) and that the next operation used there). The result is rounded by the precision control of the
 * x87 control word, which Direct3D sets to 24 bits when it creates the device, so these give single precision
 * results in the game like the exe. gcc with -mfpmath=sse does the same operation in SSE with double precision.
 */
#ifndef SCRAP_PYX87_H
#define SCRAP_PYX87_H

static __inline__ double X87_Mul(double a, double b) {
	double r;
	__asm__("fldl %1\n\tfmull %2\n\tfstpl %0" : "=m"(r) : "m"(a), "m"(b));
	return r;
}

static __inline__ double X87_Div(double a, double b) {
	double r;
	__asm__("fldl %1\n\tfdivl %2\n\tfstpl %0" : "=m"(r) : "m"(a), "m"(b));
	return r;
}

#endif
