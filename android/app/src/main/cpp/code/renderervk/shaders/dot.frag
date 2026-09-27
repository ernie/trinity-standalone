#version 450

// Flare visibility probe: each fragment adds one to the counter the vertex stage picked;
// RB_TestFlare reads them back a frame later and resets them on the CPU.
layout(set = 0, binding = 0) buffer SSBO {
	uint counts[4];   // passed, total for eye 0, then eye 1
};

layout(location = 0) flat in int counter;

layout(location = 0) out vec4 out_color;
layout(early_fragment_tests) in; // the depth test must decide before we count

void main() {
	atomicAdd( counts[counter], 1u );
	discard;
}
