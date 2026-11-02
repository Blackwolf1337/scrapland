#version 450
#extension GL_GOOGLE_include_directive : require
// bmp/GlowmapLightmap.psh: t0 diffuse, t1 lightmap, t2 glow; glow luminance to alpha
#include "ps.glsl"

void main() {
	vec4 t0 = Tex(0), t1 = Tex(1), t2 = Tex(2);
	vec4 r1 = Dp3(t2, c(0));
	vec4 r0 = t0 * t1 * 4.0;
	Out(vec4(r0.rgb + t2.rgb, v0.a * t0.a + r1.a));
}
