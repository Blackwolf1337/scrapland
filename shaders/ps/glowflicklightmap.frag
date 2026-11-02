#version 450
#extension GL_GOOGLE_include_directive : require
// bmp/GlowFlickLightmap.psh: t0 diffuse, t1 bump, t2 lightmap, t3 glow
#include "ps.glsl"

void main() {
	vec4 t0 = Tex(0), t1 = Tex(1), t2 = Tex(2), t3 = TexBem(3, t1);
	vec4 r0 = t0 * t2 * 4.0;
	Out(vec4(t3.rgb * c(0).rgb + r0.rgb, v0.a * t0.a));
}
