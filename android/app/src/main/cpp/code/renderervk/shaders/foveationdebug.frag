#version 450
#extension GL_EXT_fragment_invocation_density : require
#extension GL_EXT_multiview : require

layout(location = 0) out vec4 out_color;

// Framebuffer pixels: per eye, xy then zw
layout(push_constant) uniform FoveationDebug {
	vec4 gaze;        // where the renderer put this frame's gaze
	vec4 gridOrigin;  // a corner of the bins the renderer assumes
	vec4 tile;        // xy: the bin size it assumes, 0 while unknown; z: 1 while the map foveates
} debug;

// gl_FragSizeEXT is the fragment the device actually rasterized, not the density we
// asked it for, so this shows what the map became rather than what it said.
void main() {
	int area = gl_FragSizeEXT.x * gl_FragSizeEXT.y;
	// A coarse fragment's center sits half its size from a bin edge, so the marks reach past that
	vec2 reach = max(vec2(1.5), vec2(gl_FragSizeEXT) * 0.5 + 0.5);
	vec2 p = gl_FragCoord.xy;
	vec2 gaze = (gl_ViewIndex == 0) ? debug.gaze.xy : debug.gaze.zw;
	vec2 origin = (gl_ViewIndex == 0) ? debug.gridOrigin.xy : debug.gridOrigin.zw;
	float d = length(p - gaze);

	// Gaze: a dot inside a ring, so the sharp block can be judged against it
	if (debug.tile.z > 0.0 && (d < 3.0 + reach.x || abs(d - 14.0) < reach.x)) {
		out_color = vec4(0.0, 1.0, 1.0, 0.9);
		return;
	}

	// Assumed bin edges: the tint's edges should land on these
	if (debug.tile.z > 0.0 && debug.tile.x > 0.0 && debug.tile.y > 0.0) {
		vec2 r = mod(p - origin, debug.tile.xy);
		vec2 edge = min(r, debug.tile.xy - r);
		if (edge.x < reach.x || edge.y < reach.y) {
			out_color = vec4(1.0, 0.0, 1.0, 0.6);
			return;
		}
	}

	vec3 tint;
	if (area <= 1)      tint = vec3(0.0, 0.0, 0.0);   // 1x1, untinted
	else if (area <= 2) tint = vec3(0.0, 1.0, 0.0);   // 2x1 or 1x2
	else if (area <= 4) tint = vec3(1.0, 1.0, 0.0);   // 2x2
	else if (area <= 8) tint = vec3(1.0, 0.5, 0.0);   // 4x2 or 2x4
	else                tint = vec3(1.0, 0.0, 0.0);   // 4x4

	out_color = vec4(tint, area <= 1 ? 0.0 : 0.35);
}
