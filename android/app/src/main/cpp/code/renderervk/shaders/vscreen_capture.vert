#version 450

out gl_PerVertex {
	vec4 gl_Position;
};

void main() {
	// One triangle over the whole viewport
	vec2 p = vec2( ( gl_VertexIndex << 1 ) & 2, gl_VertexIndex & 2 );
	gl_Position = vec4( p * 2.0 - 1.0, 0.0, 1.0 );
}
