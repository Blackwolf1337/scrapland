#version 450
#extension GL_GOOGLE_include_directive : require
// bmp/BloomTarget.psh: blur * (1 - target alpha / 2) + target
#include "ps.glsl"

void main() {
	vec4 t0 = Tex(0), t1 = Tex(1);
	Out(t1 * (1.0 - t0.a * 0.5) + t0);
}
