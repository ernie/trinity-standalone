#version 450

layout(set = 1, binding = 0) uniform sampler2D screenTexture;

layout(location = 0) in vec2 frag_tex_coord;

layout(location = 0) out vec4 out_color;

void main() {
	// Encoded in, encoded out: the texture and the attachment are both UNORM views
	out_color = vec4(texture(screenTexture, frag_tex_coord).rgb, 1.0);
}
