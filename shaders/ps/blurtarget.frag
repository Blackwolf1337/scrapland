#version 450
#extension GL_GOOGLE_include_directive : require
// bmp/BlurTarget.psh: the mean of four samples, darkened and mixed with half its luminance
#include "ps.glsl"

void main() {
	vec4 r0 = 0.25 * (Tex(0) + Tex(1) + Tex(2) + Tex(3));
	vec4 r1 = vec4(dot(r0.rgb, vec3(0.3, 0.59, 0.11)) * 0.5);
	Out(r0 * 0.25 + r1);
}
