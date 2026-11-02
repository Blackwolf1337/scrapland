#version 450
#extension GL_GOOGLE_include_directive : require
// bmp/GlowmapMaskEnvmap.psh: t0 diffuse / mask, t1 env, t2 glow; glow luminance to alpha
#include "ps.glsl"

void main() {
	vec4 t0 = Tex(0), t1 = Tex(1), t2 = Tex(2);
	vec4 r0 = t1 * t0.a;
	t1 = Dp3(t2, c(0));
	vec4 r1 = t0 * v0 * 2.0;
	r0 = r0 * r1 * 4.0;
	r0 = v1 * t0 + r0;
	r0 = (r0 + r1) * 2.0;
	Out(vec4(r0.rgb + t2.rgb, t1.a + v0.a));
}
