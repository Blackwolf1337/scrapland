#version 450
#extension GL_GOOGLE_include_directive : require
// bmp/RadialBlurTarget.psh: t0 mask, t1..t3 the image at 1:1, 1:2, 1:3
#include "ps.glsl"

void main() {
	vec4 t0 = Tex(0), t1 = Tex(1), t2 = Tex(2), t3 = Tex(3);
	vec3 m = t0.rgb * 0.4;
	vec3 r0 = t1.rgb * (1.0 - m) + t2.rgb * m;
	m *= 0.4;
	r0 = r0 * (1.0 - m) + t3.rgb * m;
	Out(vec4(r0, 1.0));
}
