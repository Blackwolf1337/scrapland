#version 450
#extension GL_GOOGLE_include_directive : require
// bmp/GlowmapMaskEnvmapLightmap.psh: t0 diffuse / mask, t1 env, t2 lightmap, t3 glow
#include "ps.glsl"

void main() {
	vec4 t0 = Tex(0), t1 = Tex(1), t2 = Tex(2), t3 = Tex(3);
	vec4 r0 = t1 * t0.a;
	vec4 r1 = t0 * t2 * 2.0;
	r0 = r0 * r1 * 4.0;
	r0 = (r0 + r1) * 2.0;
	r1 = Dp3(t3, c(0));
	Out(vec4(r0.rgb + t3.rgb, r1.a + v0.a));
}
