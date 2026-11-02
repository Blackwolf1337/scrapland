#version 450
#extension GL_GOOGLE_include_directive : require
// bmp/MaskEnvBumpLightmap.psh: t0 diffuse / mask, t1 bump, t2 env, t3 lightmap
#include "ps.glsl"

void main() {
	vec4 t0 = Tex(0), t1 = Tex(1), t2 = TexBem(2, t1), t3 = Tex(3);
	vec4 r0 = t2 * t0.a;
	vec4 r1 = t0 * t3 * 2.0;
	r0 = r0 * r1 * 4.0;
	Out(vec4((r0.rgb + r1.rgb) * 2.0, v0.a));
}
