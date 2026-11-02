#version 450
#extension GL_GOOGLE_include_directive : require
// bmp/EnvmapLightmap.psh: t0 diffuse, t1 env, t2 lightmap
#include "ps.glsl"

void main() {
	vec4 t0 = Tex(0), t1 = Tex(1), t2 = Tex(2);
	vec4 r1 = t0 * t2 * 2.0;
	vec4 r0 = t1 * r1 * 4.0;
	Out(vec4((r0.rgb + r1.rgb) * 2.0, v0.a));
}
