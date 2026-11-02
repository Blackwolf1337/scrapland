#version 450
#extension GL_GOOGLE_include_directive : require
// bmp/Glowmap.psh: t0 diffuse, t1 glow; glow luminance to alpha
#include "ps.glsl"

void main() {
	vec4 t0 = Tex(0), t1 = Tex(1);
	vec4 r1 = Dp3(t1, c(0));
	vec4 r0 = t0 * v0 * 4.0;
	Out(vec4(r0.rgb + t1.rgb, v0.a * t0.a + r1.a));
}
