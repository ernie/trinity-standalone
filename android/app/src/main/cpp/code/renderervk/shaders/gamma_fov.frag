#version 450
#extension GL_EXT_multiview : enable

// gamma_subpass.frag for the foveated split: Adreno displaces input attachment reads under
// a density map, so sample the stored scene instead.
layout(set = 0, binding = 0) uniform sampler2DArray sceneColor;

layout(location = 0) in vec2 frag_tex_coord;
layout(location = 1) in flat uint view_index;

layout(location = 0) out vec4 out_color;

// Specialization constants (same IDs as gamma.frag)
layout(constant_id = 0) const float gamma = 1.0;
layout(constant_id = 1) const float obScale = 2.0;
layout(constant_id = 2) const float greyscale = 0.0;
layout(constant_id = 7) const int ditherMode = 0; // 0 - disabled, 1 - ordered
layout(constant_id = 8) const int depth_r = 255;
layout(constant_id = 9) const int depth_g = 255;
layout(constant_id = 10) const int depth_b = 255;
// 1: the target view encodes sRGB itself, so the display-encoded result goes out decoded to linear
layout(constant_id = 11) const int srgbTarget = 0;

vec3 srgbToLinear(vec3 c) {
	c = clamp(c, 0.0, 1.0);
	return mix(c / 12.92, pow((c + 0.055) / 1.055, vec3(2.4)), step(vec3(0.04045), c));
}

const vec3 lumCoeff = { 0.2126, 0.7152, 0.0722 };


// Dithering functions (from gamma.frag)
const int bayerSize = 8;
const float bayerMatrix[bayerSize * bayerSize] = {
	0,  32, 8,  40, 2,  34, 10, 42,
	48, 16, 56, 24, 50, 18, 58, 26,
	12, 44, 4,  36, 14, 46, 6,  38,
	60, 28, 52, 20, 62, 30, 54, 22,
	3,  35, 11, 43, 1,  33, 9,  41,
	51, 19, 59, 27, 49, 17, 57, 25,
	15, 47, 7,  39, 13, 45, 5,  37,
	63, 31, 55, 23, 61, 29, 53, 21
};

float threshold() {
	ivec2 coordDenormalized = ivec2(gl_FragCoord.xy);
	ivec2 bayerCoord = coordDenormalized % bayerSize;
	float bayerSample = bayerMatrix[bayerCoord.x + bayerCoord.y * bayerSize];
	float threshold = (bayerSample + 0.5) / float(bayerSize * bayerSize);
	return threshold;
}

vec3 dither(vec3 color) {
	ivec3 depth = ivec3(depth_r, depth_g, depth_b);
	vec3 cDenormalized = color * depth;
	vec3 cLow = floor(cDenormalized);
	vec3 cFractional = cDenormalized - cLow;
	vec3 cDithered = cLow + step(threshold(), cFractional);
	return cDithered / depth;
}

void main() {
	vec3 base = texture(sceneColor, vec3(frag_tex_coord, float(view_index))).rgb;

	// Greyscale conversion (from gamma.frag)
	if ( greyscale == 1 )
	{
		base = vec3(dot(base, lumCoeff));
	}
	else if ( greyscale != 0 )
	{
		vec3 luma = vec3(dot(base, lumCoeff));
		base = mix(base, luma, greyscale);
	}

	// Gamma correction and overbright scaling (from gamma.frag)
	if ( gamma != 1.0 )
	{
		out_color = vec4(pow(base, vec3(gamma)) * obScale, 1);
	}
	else
	{
		out_color = vec4(base * obScale, 1);
	}

	// Optional dithering (from gamma.frag)
	if ( ditherMode == 1 ) {
		out_color.rgb = dither(out_color.rgb);
	}

	if ( srgbTarget == 1 ) {
		out_color.rgb = srgbToLinear(out_color.rgb);
	}

}
