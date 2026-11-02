#version 450
#extension GL_GOOGLE_include_directive : require
// bmp/Envmap.psh: t0 diffuse, t1 env
#include "ps.glsl"

void main() {
	vec4 t0 = Tex(0), t1 = Tex(1);
	vec4 r1 = t0 * v0 * 2.0;
	vec4 r0 = t1 * r1 * 4.0;
	r0 = v1 * t0 + r0;
	Out(vec4((r0.rgb + r1.rgb) * 2.0, v0.a));
}
