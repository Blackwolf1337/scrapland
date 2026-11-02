#version 450
#extension GL_GOOGLE_include_directive : require
// bmp/Waves.psh: t0 diffuse, t1 bump, t2 env, t3 opacity
#include "ps.glsl"

void main() {
	vec4 t0 = Tex(0), t1 = Tex(1), t2 = TexBem(2, t1), t3 = Tex(3);
	vec4 r1 = t0 * v0 * 2.0;
	vec4 r0 = t2 * r1 * 16.0;
	Out(vec4((r0.rgb + r1.rgb) * t3.a * 2.0, t3.a * v0.a));
}
