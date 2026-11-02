#version 450
#extension GL_GOOGLE_include_directive : require
// bmp/specular.psh: the dot product of two normal maps
#include "ps.glsl"

void main() {
	vec4 t0 = Tex(0), t1 = Tex(1);
	Out(vec4(vec3(dot(Bx2(t0), Bx2(t1))), t0.a));
}
