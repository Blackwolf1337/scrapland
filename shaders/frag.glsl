// The fragment side shared by the fixed function (ff.frag) and the pixel shaders (ps/): the inputs of ff.vert, the
// textures, and what Direct3D 8 does after the texture stages or the pixel shader: the alpha test and fog.
#include "ff.glsl"

layout(set = 0, binding = 1) uniform sampler2D tex[4];

layout(location = 0) in vec4 inDiffuse;
layout(location = 1) in vec4 inSpecular;
layout(location = 2) in vec4 inTex[4];
layout(location = 6) in float inFog;
layout(location = 7) in float inEyeZ;

layout(location = 0) out vec4 outColor;

// the coordinates of stage s, divided by the last one with D3DTTFF_PROJECTED
vec2 TexCoord(int s) {
	vec4 tc = inTex[s];
	return (ff.stageTex[s].y & 0x100) != 0 && tc.w != 0.0 ? tc.xy / tc.w : tc.xy;
}

bool AlphaPass(int func, float a, float ref) {
	switch (func) {
	case 1: return false;
	case 2: return a < ref;
	case 3: return a == ref;
	case 4: return a <= ref;
	case 5: return a > ref;
	case 6: return a != ref;
	case 7: return a >= ref;
	default: return true;
	}
}

// the alpha test and fog on the color of the pixel, then out
void Finish(vec4 color) {
	if (!AlphaPass(ff.flags.w, color.a, ff.misc.y)) {
		discard;
	}
	if ((ff.flags.x & FL_FOG) != 0) {
		float f = inFog;
		int mode = ff.flags.z;
		if (mode != 0) {
			float d = abs(inEyeZ);
			if (mode == 3) {
				f = clamp((ff.fogParams.y - d) * ff.fogParams.w, 0.0, 1.0);
			} else if (mode == 1) {
				f = clamp(exp(-d * ff.fogParams.z), 0.0, 1.0);
			} else {
				float x = d * ff.fogParams.z;
				f = clamp(exp(-x * x), 0.0, 1.0);
			}
		}
		color.rgb = mix(ff.fogColor.rgb, color.rgb, f);
	}
	outColor = color;
}
