#version 450

layout(set = 0, binding = 0) uniform sampler2DArray src;

// The crop's top left in layer 0, its texels per capture texel, and one over the source's size
layout(constant_id = 0) const int OFFSET_X = 0;
layout(constant_id = 1) const int OFFSET_Y = 0;
layout(constant_id = 2) const float SCALE_X = 1.0;
layout(constant_id = 3) const float SCALE_Y = 1.0;
layout(constant_id = 4) const float INV_WIDTH = 1.0;
layout(constant_id = 5) const float INV_HEIGHT = 1.0;

layout(location = 0) out vec4 out_color;

void main() {
	// A linear tap at the center of each capture texel's span of the crop; at scale 1 that is the texel itself
	vec2 uv = ( vec2( OFFSET_X, OFFSET_Y ) + gl_FragCoord.xy * vec2( SCALE_X, SCALE_Y ) ) * vec2( INV_WIDTH, INV_HEIGHT );
	out_color = vec4( texture( src, vec3( uv, 0.0 ) ).rgb, 1.0 );
}
