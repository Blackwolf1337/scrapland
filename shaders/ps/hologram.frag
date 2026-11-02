#version 450
#extension GL_GOOGLE_include_directive : require
// bmp/Hologram.psh: diffuse * c0 - noise - scans
#include "ps.glsl"

void main() {
	vec4 t0 = Tex(0), t1 = Tex(1), t2 = Tex(2);
	vec4 r1 = t0 * c(0) - t2;
	Out(vec4(r1.rgb - t1.rgb, v0.a));
}
