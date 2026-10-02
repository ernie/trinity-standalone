#version 450

layout(set = 1, binding = 0) uniform sampler2D baseTexture;

layout(location = 0) in vec4 frag_color;
layout(location = 1) in vec2 frag_tex_coord;

layout(location = 0) out vec4 out_color;

void main() {
	// Encoded in, encoded out, as the screen beside it; the texture's own alpha is not used
	out_color = vec4(texture(baseTexture, frag_tex_coord).rgb * frag_color.rgb, frag_color.a);
}
