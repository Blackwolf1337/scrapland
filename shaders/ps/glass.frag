#version 450
#extension GL_GOOGLE_include_directive : require
// bmp/Glass.psh: t0 diffuse + opacity, t1 env
#include "ps.glsl"

void main() {
	vec4 t0 = Tex(0), t1 = Tex(1);
	vec4 r1 = vec4(t0.rgb * v0.rgb * 2.0, t0.a * v0.a);
	vec4 r0 = t1 * r1 * 4.0;
	Out(vec4((r0.rgb + r1.rgb) * r1.a * 2.0, r1.a));
}
