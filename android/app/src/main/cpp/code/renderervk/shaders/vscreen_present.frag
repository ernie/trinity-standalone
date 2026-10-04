#version 450
#extension GL_EXT_multiview : require

layout(set = 0, binding = 0) uniform sampler2DArray scene;

layout(location = 0) out vec4 out_color;

void main() {
	// The scene buffer's sRGB view decodes and the swapchain's encodes, so the bytes arrive unchanged
	out_color = vec4(texelFetch(scene, ivec3(ivec2(gl_FragCoord.xy), gl_ViewIndex), 0).rgb, 1.0);
}
