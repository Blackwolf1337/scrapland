#version 450
#extension GL_GOOGLE_include_directive : require
// bmp/DUDVTarget.psh: texm3x2pad / texm3x2tex, the color texture at the distorted screen position
#include "ps.glsl"

void main() {
	vec3 n = Bx2(Tex(0));
	vec2 uv = vec2(dot(inTex[1].xyz, n), dot(inTex[2].xyz, n));
	Out(ff.stageTex[2].z != 0 ? texture(tex[2], uv) : vec4(0.0, 0.0, 0.0, 1.0));
}
