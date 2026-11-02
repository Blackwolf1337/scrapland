#version 450
#extension GL_GOOGLE_include_directive : require
// bmp/Electric.psh and bmp/Fire.psh (the same): t0 bump, t1 diffuse, t2 mask; the sum to alpha (bloom)
#include "ps.glsl"

void main() {
	vec4 t0 = Tex(0), t1 = TexBem(1, t0), t2 = Tex(2);
	vec4 r0 = t1 * t2 * c(0);
	Out(vec4(r0.rgb, dot(r0.rgb, vec3(1.0))));
}
