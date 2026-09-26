#version 450

layout(set = 0, binding = 0) uniform sampler2DArray src;

// The crop's top left in layer 0
layout(constant_id = 0) const int OFFSET_X = 0;
layout(constant_id = 1) const int OFFSET_Y = 0;

layout(location = 0) out vec4 out_color;

void main() {
	// Exact texel copy: both the source and the attachment are UNORM views of the same format
	out_color = vec4( texelFetch( src, ivec3( ivec2( gl_FragCoord.xy ) + ivec2( OFFSET_X, OFFSET_Y ), 0 ), 0 ).rgb, 1.0 );
}
