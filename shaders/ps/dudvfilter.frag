#version 450
#extension GL_GOOGLE_include_directive : require
// bmp/DUDVFilter.psh: rgb - alpha, plus 1 in red
#include "ps.glsl"

void main() {
	vec4 t0 = Tex(0);
	Out(t0 - t0.a + vec4(1.0, 0.0, 0.0, 0.0));
}
