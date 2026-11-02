// The registers and instructions of ps.1.1 that the pixel shaders of the game (bmp/*.psh in Data.packed) use. Each
// ps/<name>.frag is the hand port of bmp/<name>.psh: tN = Tex(N) for "tex tN", TexBem for "texbem", c(i) the
// constants the game sets (SetPixelShaderConstant), v0 and v1 the diffuse and specular colors. Direct3D 8 applies
// the alpha test and fog after the shader (Finish in frag.glsl); r0 is clamped to 0..1 first.
#include "../frag.glsl"

#define v0 inDiffuse
#define v1 inSpecular

vec4 c(int i) {
	return ff.psConst[i];
}

// tex tN: (0, 0, 0, 1) for a stage without a texture
vec4 Tex(int s) {
	return ff.stageTex[s].z != 0 ? texture(tex[s], TexCoord(s)) : vec4(0.0, 0.0, 0.0, 1.0);
}

// texbem tN, src: the coordinates of stage s moved by the du, dv in src through the bump matrix of stage s
vec4 TexBem(int s, vec4 src) {
	vec4 m = ff.bumpMat[s];
	vec2 uv = TexCoord(s) + vec2(m.x * src.r + m.z * src.g, m.y * src.r + m.w * src.g);
	return ff.stageTex[s].z != 0 ? texture(tex[s], uv) : vec4(0.0, 0.0, 0.0, 1.0);
}

// the _bx2 source modifier
vec3 Bx2(vec4 v) {
	return v.rgb * 2.0 - 1.0;
}

// dp3: the dot product in all four channels
vec4 Dp3(vec4 a, vec4 b) {
	return vec4(dot(a.rgb, b.rgb));
}

void Out(vec4 r0) {
	Finish(clamp(r0, 0.0, 1.0));
}
