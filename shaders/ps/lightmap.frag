#version 450
#extension GL_GOOGLE_include_directive : require
// bmp/lightmap.psh: diffuse * lightmap
#include "ps.glsl"

void main() {
	vec4 t0 = Tex(0), t1 = Tex(1);
	Out(vec4(t0.rgb * t1.rgb * 4.0, t0.a * c(0).a));
}
