#version 450

layout(set = 1, binding = 0) uniform sampler2D screenTexture;

layout(location = 0) in vec2 frag_tex_coord;

layout(location = 0) out vec4 out_color;

// SPAN must match VSCREEN_REFLECT_SPAN in vk.c; the mesh covers only that band
const float SPAN = 0.25;
const float STRENGTH = 0.25;
const float BLUR_LOD = 3.0;

void main() {
	float depth = (1.0 - frag_tex_coord.y) / SPAN;
	vec3 color = textureLod(screenTexture, frag_tex_coord, BLUR_LOD).rgb;
	out_color = vec4(color, STRENGTH * (1.0 - smoothstep(0.0, 0.9, depth)));
}
