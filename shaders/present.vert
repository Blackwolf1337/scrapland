#version 450
// The present pass: one triangle over the whole swapchain image, uv 0..1 over its visible part.
layout(location = 0) out vec2 uv;

void main() {
	uv = vec2(gl_VertexIndex & 2, (gl_VertexIndex << 1) & 2);
	gl_Position = vec4(uv * 2.0 - 1.0, 0.0, 1.0);
}
