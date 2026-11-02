#version 450
#extension GL_GOOGLE_include_directive : require
// The pixel part of the fixed function of Direct3D 8: the texture stages with their operations and arguments,
// bump env mapping, the specular add, the alpha test and fog.
#include "ff.glsl"

layout(set = 0, binding = 1) uniform sampler2D tex[4];

layout(location = 0) in vec4 inDiffuse;
layout(location = 1) in vec4 inSpecular;
layout(location = 2) in vec4 inTex[4];
layout(location = 6) in float inFog;
layout(location = 7) in float inEyeZ;

layout(location = 0) out vec4 outColor;

vec4 Arg(int a, vec4 cur, vec4 texel, vec4 temp) {
	vec4 v;
	int sel = a & 15;
	if (sel == 0) {
		v = inDiffuse;
	} else if (sel == 1) {
		v = cur;
	} else if (sel == 2) {
		v = texel;
	} else if (sel == 3) {
		v = ff.tfactor;
	} else if (sel == 4) {
		v = inSpecular;
	} else {
		v = temp;
	}
	if ((a & 0x10) != 0) {
		v = 1.0 - v;
	}
	if ((a & 0x20) != 0) {
		v = vec4(v.a);
	}
	return v;
}

vec4 Op(int op, vec4 a1, vec4 a2, vec4 a0, vec4 cur, vec4 texel) {
	switch (op) {
	case 2: return a1;
	case 3: return a2;
	case 4: return a1 * a2;
	case 5: return a1 * a2 * 2.0;
	case 6: return a1 * a2 * 4.0;
	case 7: return a1 + a2;
	case 8: return a1 + a2 - 0.5;
	case 9: return (a1 + a2 - 0.5) * 2.0;
	case 10: return a1 - a2;
	case 11: return a1 + a2 - a1 * a2;
	case 12: return mix(a2, a1, inDiffuse.a);
	case 13: return mix(a2, a1, texel.a);
	case 14: return mix(a2, a1, ff.tfactor.a);
	case 15: return a1 + a2 * (1.0 - texel.a);
	case 16: return mix(a2, a1, cur.a);
	case 17: return a1;
	case 18: return vec4(a1.rgb + a1.a * a2.rgb, a1.a);
	case 19: return vec4(a1.rgb * a2.rgb + a1.a, a1.a);
	case 20: return vec4(a1.rgb + (1.0 - a1.a) * a2.rgb, a1.a);
	case 21: return vec4((1.0 - a1.rgb) * a2.rgb + a1.a, a1.a);
	case 24: return vec4(clamp(4.0 * dot(a1.rgb - 0.5, a2.rgb - 0.5), 0.0, 1.0));
	case 25: return a0 + a1 * a2;
	case 26: return mix(a2, a1, a0);
	default: return cur;
	}
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

void main() {
	vec4 cur = inDiffuse;
	vec4 temp = vec4(0.0);
	vec2 bump = vec2(0.0);

	for (int s = 0; s < 8; s++) {
		int cop = ff.stage[s].x;
		if (cop == 1) {
			break;
		}
		vec4 texel = vec4(1.0);
		if (s < 4 && ff.stageTex[s].z != 0) {
			vec4 tc = inTex[s];
			vec2 uv = (ff.stageTex[s].y & 0x100) != 0 && tc.w != 0.0 ? tc.xy / tc.w : tc.xy;
			texel = texture(tex[s], uv + bump);
		}
		bump = vec2(0.0);
		if (cop == 22 || cop == 23) {
			// bump env map: the du, dv of this texture move the coordinates of the next stage
			vec4 m = ff.bumpMat[s];
			vec2 duv = texel.rg;
			bump = vec2(m.x * duv.x + m.z * duv.y, m.y * duv.x + m.w * duv.y);
			continue;
		}
		int ca = ff.stage[s].y, aa = ff.stage[s].w;
		vec4 c = Op(cop, Arg(ca & 0xff, cur, texel, temp), Arg(ca >> 8 & 0xff, cur, texel, temp),
			Arg(ca >> 16 & 0xff, cur, texel, temp), cur, texel);
		int aop = ff.stage[s].z;
		float a = aop == 1 ? cur.a : Op(aop, Arg(aa & 0xff, cur, texel, temp), Arg(aa >> 8 & 0xff, cur, texel, temp),
			Arg(aa >> 16 & 0xff, cur, texel, temp), cur, texel).a;
		vec4 res = clamp(vec4(c.rgb, a), 0.0, 1.0);
		if (ff.stageTex[s].w != 0) {
			temp = res;
		} else {
			cur = res;
		}
	}
	if ((ff.flags.x & FL_SPECULARENABLE) != 0) {
		cur.rgb = clamp(cur.rgb + inSpecular.rgb, 0.0, 1.0);
	}
	if (!AlphaPass(ff.flags.w, cur.a, ff.misc.y)) {
		discard;
	}
	vec4 nofog = cur;
	float f = 1.0;
	if ((ff.flags.x & FL_FOG) != 0) {
		f = inFog;
		if (ff.flags.z != 0) {
			float d = abs(inEyeZ);
			int mode = ff.flags.z;
			if (mode == 3) {
				f = clamp((ff.fogParams.y - d) * ff.fogParams.w, 0.0, 1.0);
			} else if (mode == 1) {
				f = clamp(exp(-d * ff.fogParams.z), 0.0, 1.0);
			} else {
				float x = d * ff.fogParams.z;
				f = clamp(exp(-x * x), 0.0, 1.0);
			}
		}
		cur.rgb = mix(ff.fogColor.rgb, cur.rgb, f);
	}
	int dbg = int(ff.misc.z);
	if (dbg == 1) {
		cur = ff.stageTex[0].z != 0 ? texture(tex[0], inTex[0].xy) : vec4(1.0, 0.0, 1.0, 1.0);
	} else if (dbg == 2) {
		cur = inDiffuse;
	} else if (dbg == 3) {
		cur = vec4(vec3(f), 1.0);
	} else if (dbg == 4) {
		cur = vec4(nofog.rgb, 1.0);
	} else if (dbg == 5) {
		cur = vec4(vec3(nofog.a), 1.0);
	}
	outColor = cur;
}
