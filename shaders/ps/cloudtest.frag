#version 450
#extension GL_GOOGLE_include_directive : require
// bmp/CloudTest.psh: two cloud layers and a mask
#include "ps.glsl"

void main() {
	vec4 t0 = Tex(0), t1 = Tex(1), t2 = Tex(2);
	vec4 r0;
	r0.rgb = t0.rgb * c(0).rgb + (t1.rgb - 0.5);
	r0.a = (t0.b * c(0).a + (t1.b - 0.5)) * t2.a;
	Out(r0);
}
