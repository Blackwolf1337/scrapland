#version 450
#extension GL_GOOGLE_include_directive : require
// bmp/Noise.psh: noise * c0
#include "ps.glsl"

void main() {
	vec4 t0 = Tex(0);
	Out(vec4(t0.rgb * c(0).rgb * 4.0, c(0).a));
}
