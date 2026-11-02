#version 450
#extension GL_GOOGLE_include_directive : require
// The vertex part of the fixed function of Direct3D 8: transform (or pretransformed vertices), the lights of D3D8
// per vertex, vertex fog and the texture coordinates of four stages with their generation and transforms.
#include "ff.glsl"

layout(location = 0) in vec4 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inDiffuse;
layout(location = 3) in vec4 inSpecular;
layout(location = 4) in vec4 inTex0;
layout(location = 5) in vec4 inTex1;
layout(location = 6) in vec4 inTex2;
layout(location = 7) in vec4 inTex3;

layout(location = 0) out vec4 outDiffuse;
layout(location = 1) out vec4 outSpecular;
layout(location = 2) out vec4 outTex[4];
layout(location = 6) out float outFog;
layout(location = 7) out float outEyeZ;

vec4 MatColor(int source, vec4 material) {
	if ((ff.flags.x & FL_COLORVERTEX) != 0) {
		if (source == 1 && (ff.flags.x & FL_DIFFUSE) != 0) {
			return inDiffuse;
		}
		if (source == 2 && (ff.flags.x & FL_SPECULAR) != 0) {
			return inSpecular;
		}
	}
	return material;
}

float FogFactor(int mode, float d) {
	if (mode == 3) {
		return clamp((ff.fogParams.y - d) * ff.fogParams.w, 0.0, 1.0);
	}
	if (mode == 1) {
		return clamp(exp(-d * ff.fogParams.z), 0.0, 1.0);
	}
	if (mode == 2) {
		float x = d * ff.fogParams.z;
		return clamp(exp(-x * x), 0.0, 1.0);
	}
	return 1.0;
}

vec4 TexInput(int idx) {
	if (idx == 0) {
		return inTex0;
	}
	if (idx == 1) {
		return inTex1;
	}
	if (idx == 2) {
		return inTex2;
	}
	return inTex3;
}

void main() {
	int fl = ff.flags.x;
	vec3 eyePos = vec3(0.0);
	vec3 eyeNormal = vec3(0.0, 0.0, 1.0);
	vec4 diffuse = (fl & FL_DIFFUSE) != 0 ? inDiffuse : vec4(1.0);
	vec4 specular = (fl & FL_SPECULAR) != 0 ? inSpecular : vec4(0.0);

	if ((fl & FL_RHW) != 0) {
		// screen coordinates: pixel centers on the integers as in D3D, so half a pixel more for Vulkan
		float w = inPos.w != 0.0 ? 1.0 / inPos.w : 1.0;
		vec2 ndc = vec2((inPos.x + 0.5 - ff.viewport.x) / ff.viewport.z * 2.0 - 1.0,
			1.0 - (inPos.y + 0.5 - ff.viewport.y) / ff.viewport.w * 2.0);
		gl_Position = vec4(ndc * w, inPos.z * w, w);
		outEyeZ = w;
	} else {
		vec4 wp = ff.world * vec4(inPos.xyz, 1.0);
		vec4 ep = ff.view * wp;
		eyePos = ep.xyz;
		gl_Position = ff.proj * ep;
		gl_Position.xy += vec2(1.0 / ff.viewport.z, -1.0 / ff.viewport.w) * gl_Position.w;
		outEyeZ = ep.z;
		if ((fl & FL_NORMAL) != 0) {
			eyeNormal = mat3(ff.view) * (mat3(ff.world) * inNormal);
			float l = length(eyeNormal);
			eyeNormal = l > 0.0 ? eyeNormal / l : eyeNormal;
		}
		if ((fl & FL_LIGHTING) != 0) {
			vec4 md = MatColor(ff.matSource.x, ff.matDiffuse);
			vec4 ms = MatColor(ff.matSource.y, ff.matSpecular);
			vec4 ma = MatColor(ff.matSource.z, ff.matAmbient);
			vec4 me = MatColor(ff.matSource.w, ff.matEmissive);
			vec3 accD = vec3(0.0), accS = vec3(0.0), accA = vec3(0.0);
			vec3 v = (fl & FL_LOCALVIEWER) != 0 ? normalize(-eyePos) : vec3(0.0, 0.0, -1.0);
			for (int i = 0; i < ff.lightCount.x; i++) {
				float type = ff.lightPos[i].w;
				vec3 l;
				float att = 1.0;
				if (type == 3.0) {
					l = ff.lightDir[i].xyz;
				} else {
					vec3 d = ff.lightPos[i].xyz - eyePos;
					float dist = length(d);
					if (dist > ff.lightAtt[i].x) {
						continue;
					}
					l = dist > 0.0 ? d / dist : vec3(0.0, 0.0, 1.0);
					float a = ff.lightAtt[i].y + ff.lightAtt[i].z * dist + ff.lightAtt[i].w * dist * dist;
					att = a > 0.0 ? 1.0 / a : 1.0;
					if (type == 2.0) {
						float rho = dot(-l, -ff.lightDir[i].xyz);
						float ct = ff.lightSpot[i].y, cp = ff.lightSpot[i].z;
						if (rho <= cp) {
							att = 0.0;
						} else if (rho < ct) {
							att *= pow(clamp((rho - cp) / max(ct - cp, 1e-6), 0.0, 1.0), ff.lightSpot[i].x);
						}
					}
				}
				float ndl = max(dot(eyeNormal, l), 0.0);
				accA += att * ff.lightAmbient[i].rgb;
				accD += att * ndl * ff.lightDiffuse[i].rgb;
				if ((fl & FL_SPECULARENABLE) != 0 && ndl > 0.0) {
					vec3 h = normalize(l + v);
					accS += att * pow(max(dot(eyeNormal, h), 0.0), ff.misc.x) * ff.lightSpecular[i].rgb;
				}
			}
			diffuse = vec4(clamp(me.rgb + ma.rgb * (ff.ambient.rgb + accA) + md.rgb * accD, 0.0, 1.0), md.a);
			specular = vec4(clamp(ms.rgb * accS, 0.0, 1.0), ms.a);
		}
	}
	outDiffuse = diffuse;
	outSpecular = specular;

	// fog: vertex fog from the eye distance, else the specular alpha of pretransformed vertices
	outFog = 1.0;
	if ((fl & FL_FOG) != 0 && ff.flags.z == 0) {
		if (ff.flags.y != 0 && (fl & FL_RHW) == 0) {
			float d = (fl & FL_RANGEFOG) != 0 ? length(eyePos) : abs(eyePos.z);
			outFog = FogFactor(ff.flags.y, d);
		} else {
			outFog = specular.a;
		}
	}

	for (int s = 0; s < 4; s++) {
		int tci = ff.stageTex[s].x;
		int mode = tci >> 16;
		vec4 tc;
		int dim = 4;
		if (mode == 1) {
			tc = vec4(eyeNormal, 1.0);
		} else if (mode == 2) {
			tc = vec4(eyePos, 1.0);
		} else if (mode == 3) {
			tc = vec4(reflect(normalize(eyePos), eyeNormal), 1.0);
		} else {
			tc = TexInput(tci & 3);
			dim = ff.texDim[tci & 3];
		}
		if ((ff.stageTex[s].y & 0xff) != 0) {
			// fewer than 4 components: a 1 after the last one, 0 after that, as D3D feeds the matrix
			if (dim == 1) {
				tc = vec4(tc.x, 1.0, 0.0, 0.0);
			} else if (dim == 2) {
				tc = vec4(tc.xy, 1.0, 0.0);
			} else if (dim == 3) {
				tc = vec4(tc.xyz, 1.0);
			}
			tc = ff.texm[s] * tc;
		}
		outTex[s] = tc;
	}
}
