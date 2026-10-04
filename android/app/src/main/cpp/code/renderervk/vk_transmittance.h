/* The scene image's alpha is transmittance: what post-scene draws let bloom through in the composite. */
#ifndef VK_TRANSMITTANCE_H
#define VK_TRANSMITTANCE_H

/* Scene stages: 0 masks alpha writes; 1 writes alpha for a later stage that blends on it; 2 (the shader's
 * last stage, when any stage read it) writes 1 back. */
static inline int VK_SceneAlphaMode( const int *readsDestinationAlpha, int numStages, int stage ) {
	int last = -1, i;
	for ( i = 0; i < numStages; i++ )
		if ( readsDestinationAlpha[i] )
			last = i;
	if ( stage < last )
		return 1;
	if ( last >= 0 && stage == numStages - 1 )
		return 2;
	return 0;
}

static inline int VK_TransmittanceReadsDestination( VkBlendFactor factor ) {
	return factor == VK_BLEND_FACTOR_DST_COLOR || factor == VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR ||
		factor == VK_BLEND_FACTOR_DST_ALPHA || factor == VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA ||
		factor == VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
}

/* Post-scene draws keep their color blend; alpha instead tracks whatever multiplies the destination.
 * Exact for scalar multipliers, through the source's luminance for per-channel ones (returns 1 so the
 * shader writes luminance to alpha), unchanged where the blend reads the scene. */
static inline int VK_TransmittanceBlend( VkPipelineColorBlendAttachmentState *t ) {
	const VkBlendFactor src = t->srcColorBlendFactor, dst = t->dstColorBlendFactor;
	const int srcAlphaColor = src == VK_BLEND_FACTOR_SRC_ALPHA || src == VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA ||
		dst == VK_BLEND_FACTOR_SRC_ALPHA || dst == VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	int mode = 0;
	t->srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	t->alphaBlendOp = VK_BLEND_OP_ADD;
	if ( t->colorWriteMask )
		t->colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	if ( !t->blendEnable ) {
		t->blendEnable = VK_TRUE;
		t->srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
		t->dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
		t->colorBlendOp = VK_BLEND_OP_ADD;
		t->dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		return 0;
	}
	if ( src == VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR && dst == VK_BLEND_FACTOR_ONE ) {
		// screen: the scene keeps one minus the source
		t->dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		return 1;
	}
	if ( src == VK_BLEND_FACTOR_DST_COLOR && dst == VK_BLEND_FACTOR_ZERO ) {
		// filter: the scene is multiplied by the source
		t->dstAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		return 1;
	}
	switch ( dst ) {
		case VK_BLEND_FACTOR_ZERO:
		case VK_BLEND_FACTOR_ONE:
		case VK_BLEND_FACTOR_SRC_ALPHA:
		case VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA:
			t->dstAlphaBlendFactor = dst;
			break;
		case VK_BLEND_FACTOR_SRC_COLOR:
		case VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR:
			if ( srcAlphaColor ) {
				// the color blend needs the real alpha, so the scene shows through unscaled
				t->dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
			} else {
				t->dstAlphaBlendFactor = dst == VK_BLEND_FACTOR_SRC_COLOR ? VK_BLEND_FACTOR_SRC_ALPHA
					: VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
				mode = 1;
			}
			break;
		default:
			t->dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
			break;
	}
	if ( src == VK_BLEND_FACTOR_DST_COLOR ) {
		// the destination is scaled by the source plus that factor (the skin shine), over an opaque stage below
		t->dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		mode = 0;
	} else if ( VK_TransmittanceReadsDestination( src ) || VK_TransmittanceReadsDestination( dst ) ) {
		t->dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		mode = 0;
	}
	return mode;
}

#endif
