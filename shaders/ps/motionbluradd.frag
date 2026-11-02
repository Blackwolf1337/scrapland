#version 450
#extension GL_GOOGLE_include_directive : require
// bmp/MotionBlurAdd.psh: four samples times c1
#include "ps.glsl"

void main() {
	vec4 r0 = c(1) * Tex(0);
	r0 = c(1) * Tex(1) + r0;
	r0 = c(1) * Tex(2) + r0;
	r0 = c(1) * Tex(3) + r0;
	Out(vec4(r0.rgb, c(0).a));
}
