#version 450
#extension GL_EXT_multiview : enable

// World from mesh, pushed per draw; world to clip per view in set 0 binding 1
layout(push_constant) uniform Transform {
	mat4 u_model;
};

layout(set = 0, binding = 1) uniform ViewTransform {
	mat4 eyeProj[2];
};

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec2 in_tex_coord;

layout(location = 0) out vec2 frag_tex_coord;

out gl_PerVertex {
	vec4 gl_Position;
};

void main() {
	gl_Position = eyeProj[gl_ViewIndex] * (u_model * vec4(in_position, 1.0));
	frag_tex_coord = in_tex_coord;
}
