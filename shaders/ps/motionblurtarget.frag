#version 450
#extension GL_GOOGLE_include_directive : require
// bmp/MotionBlurTarget.psh: target * c1 + blur * c0
#include "ps.glsl"

void main() {
	vec4 t0 = Tex(0), t1 = Tex(1);
	Out(t0 * c(1) + t1 * c(0));
}
