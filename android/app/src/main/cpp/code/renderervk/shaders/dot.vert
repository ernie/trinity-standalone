#version 450
#extension GL_EXT_multiview : enable

// Flare visibility probe: a quad in the flare's surface plane. RB_TestFlare pushes per-eye
// clip-space center and unit-axis deltas; this pipeline's layout has no eyeProj UBO.
layout(push_constant) uniform Probe {
	vec4 center[2];   // per-eye clip-space center, viewer bias already applied
	vec4 du[2];       // per-eye clip-space delta for one unit along u
	vec4 dv[2];       // per-eye clip-space delta for one unit along v
	vec4 extent;      // half extents: xy for eye 0 (along u, v), zw for eye 1
	vec4 params;      // x: which of the eye's counters the fragment adds to (0 passed, 1 total)
};

layout(location = 0) in vec3 in_position; // unused; satisfies the pipeline's vertex input

layout(location = 0) flat out int counter;

out gl_PerVertex {
	vec4 gl_Position;
};

void main() {
	const vec2 corner[6] = vec2[6](
		vec2(-1.0, -1.0), vec2( 1.0, -1.0), vec2( 1.0,  1.0),
		vec2(-1.0, -1.0), vec2( 1.0,  1.0), vec2(-1.0,  1.0) );
	vec2 halfExtent = ( gl_ViewIndex == 0 ) ? extent.xy : extent.zw;
	vec2 k = corner[gl_VertexIndex] * halfExtent;
	gl_Position = center[gl_ViewIndex] + k.x * du[gl_ViewIndex] + k.y * dv[gl_ViewIndex];
	counter = int( params.x ) + 2 * gl_ViewIndex;
}
