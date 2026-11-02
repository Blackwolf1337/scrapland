// The state of the fixed function of Direct3D 8 for one draw (src/game/engine3d/Render/vk/VkDraw.c fills it in the
// same layout, std140). The matrices are the D3D ones as they are in memory, which makes them the transposed ones
// here, so M * v here is v * M there.

#define FL_RHW 1		// pretransformed vertices (XYZRHW)
#define FL_NORMAL 2		// the vertices have normals
#define FL_DIFFUSE 4		// and a diffuse color
#define FL_SPECULAR 8		// and a specular color
#define FL_LIGHTING 16
#define FL_SPECULARENABLE 32
#define FL_COLORVERTEX 64
#define FL_LOCALVIEWER 128
#define FL_NORMALIZE 256
#define FL_FOG 512		// FOGENABLE
#define FL_RANGEFOG 1024

layout(set = 0, binding = 0, std140) uniform FF {
	mat4 world;
	mat4 view;
	mat4 proj;
	mat4 texm[4];
	vec4 viewport;		// x, y, width, height in pixels
	vec4 matDiffuse;
	vec4 matAmbient;
	vec4 matSpecular;
	vec4 matEmissive;
	vec4 misc;		// material power, alpha reference (0..1), unused, unused
	vec4 ambient;
	vec4 fogColor;
	vec4 fogParams;		// start, end, density, 1 / (end - start)
	vec4 tfactor;
	ivec4 flags;		// FL_ bits, fog vertex mode, fog table mode, alpha test function (0 none)
	ivec4 matSource;	// diffuse, specular, ambient, emissive sources: 0 material, 1 color1, 2 color2
	ivec4 lightCount;	// enabled lights in order, unused...
	ivec4 texDim;		// the components of the texture coordinates 0 to 3 in the vertices
	vec4 lightDiffuse[8];
	vec4 lightSpecular[8];
	vec4 lightAmbient[8];
	vec4 lightPos[8];	// view space, w the type (1 point, 2 spot, 3 directional)
	vec4 lightDir[8];	// view space, towards the light
	vec4 lightAtt[8];	// range, attenuation 0, 1, 2
	vec4 lightSpot[8];	// falloff, cos of theta / 2, cos of phi / 2
	ivec4 stage[8];		// color op, color args 1 | 2 << 8 | 0 << 16, alpha op, alpha args
	ivec4 stageTex[8];	// texcoord index (mode << 16), transform flags, has a texture, result to temp
	vec4 bumpMat[8];	// 00, 01, 10, 11
	vec4 psConst[8];
} ff;
