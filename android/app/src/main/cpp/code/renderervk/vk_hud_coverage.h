/* HUD target alpha is the composite coverage mask, so only opaque materials force it. */
static ID_INLINE int VK_HudCoverage( int is2D, unsigned first, unsigned current ) {
	if ( is2D || (first & GLS_BLEND_BITS) ) {
		return 0;
	}
	return (current & GLS_BLEND_BITS) ? 2 : 1;
}

/* Coverage needs factors of its own: the color factors would square the source
 * alpha on source-over and let additive passes inflate coverage. */
static ID_INLINE void VK_HudAlphaBlend( unsigned stateBits, int coverage,
	VkPipelineColorBlendAttachmentState *blend ) {
	if ( coverage == 2 || (stateBits & GLS_DSTBLEND_BITS) == GLS_DSTBLEND_ONE ) {
		blend->srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		blend->dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	} else if ( (stateBits & GLS_BLEND_BITS) ==
		(GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA) ) {
		blend->srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		blend->dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	}
}

/* HUD target RGB is premultiplied, so compositing must not scale it by alpha again. */
static ID_INLINE unsigned VK_HudCompositeBlend( unsigned stateBits ) {
	if ( (stateBits & GLS_BLEND_BITS) ==
		(GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA) ) {
		stateBits = (stateBits & ~GLS_SRCBLEND_BITS) | GLS_SRCBLEND_ONE;
	}
	return stateBits;
}
