#version 450
#extension GL_GOOGLE_include_directive : require
// bmp/BloomBlur.psh: four samples weighted 0.24, 0.26, 0.26, 0.24
#include "ps.glsl"

void main() {
	Out(0.24 * Tex(0) + 0.26 * Tex(1) + 0.26 * Tex(2) + 0.24 * Tex(3));
}
