#version 450
// The present pass: the back buffer scaled to the swapchain image through the gamma ramp of the device
// (SetGammaRamp, 256 entries per channel in a 256 x 1 texture).
layout(set = 0, binding = 0) uniform sampler2D backBuffer;
layout(set = 0, binding = 1) uniform sampler2D ramp;

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 outColor;

float Ramp(float c, int channel) {
	return texture(ramp, vec2((c * 255.0 + 0.5) / 256.0, 0.5))[channel];
}

void main() {
	vec3 c = texture(backBuffer, uv).rgb;
	outColor = vec4(Ramp(c.r, 0), Ramp(c.g, 1), Ramp(c.b, 2), 1.0);
}
