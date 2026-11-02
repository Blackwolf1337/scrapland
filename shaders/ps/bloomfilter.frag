#version 450
#extension GL_GOOGLE_include_directive : require
// bmp/BloomFilter.psh: the mean of four samples, its color times its alpha
#include "ps.glsl"

void main() {
	vec4 r1 = 0.25 * (Tex(0) + Tex(1) + Tex(2) + Tex(3));
	Out(vec4(r1.rgb * r1.a * 2.0, r1.a));
}
