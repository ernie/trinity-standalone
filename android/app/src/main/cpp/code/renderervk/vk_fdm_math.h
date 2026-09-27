/* Density map authoring: R8G8 texels, one layer per eye, rows running down the image. */
#ifndef VK_FDM_MATH_H
#define VK_FDM_MATH_H
#include <math.h>
#include <stdint.h>
#include <string.h>
typedef struct {
	uint32_t width, height;           // framebuffer pixels
	uint32_t texelWidth, texelHeight; // framebuffer pixels a map texel covers
	uint32_t mapWidth, mapHeight, layers;
	uint32_t tileWidth, tileHeight;   // the tiler's bin, 0 while unknown
} vkFdmGeometry_t;
/* The tiler reads one density a bin, so a texel stands for its bin's point nearest the target. */
static inline float VK_FdmBinNearest( float p, float target, uint32_t tile ) {
	float lo;
	if ( !tile )
		return p;
	lo = floorf( p / tile ) * tile;
	return target < lo ? lo : target > lo + tile ? lo + tile : target;
}
/*
 * A map per eye, drawn in that eye's own frustum. A texel and the gaze are both directions,
 * (tan x, tan y, 1), and the eccentricity is the angle between them, compared as a cosine squared.
 */
static inline void VK_FdmWriteGaze( uint8_t *dst, const vkFdmGeometry_t *g, float sharpDeg, float coarseDeg,
	const float center[2][2], const float fovTan[2][4] ) {
	const float toRad = 3.14159265358979f / 180.0f;
	const float midDeg = 0.5f * ( sharpDeg + coarseDeg );
	const float cosSharp = cosf( sharpDeg * toRad ), cosMid = cosf( midDeg * toRad ), cosCoarse = cosf( coarseDeg * toRad );
	uint32_t layer, y, x;
	if ( !g->width || !g->height )
		return;
	for ( layer = 0; layer < g->layers; layer++ ) {
		const int eye = layer < 2 ? (int)layer : 0;
		const float tanL = fovTan[eye][0], tanU = fovTan[eye][2];
		const float spanX = fovTan[eye][1] - tanL, spanY = tanU - fovTan[eye][3];
		const float gazeX = ( center[eye][0] + 1.0f ) * 0.5f * g->width;
		const float gazeY = ( center[eye][1] + 1.0f ) * 0.5f * g->height;
		const float gx = tanL + gazeX / g->width * spanX;
		const float gy = tanU - gazeY / g->height * spanY;
		const float gazeLen2 = gx * gx + gy * gy + 1.0f;
		const float sharpK = cosSharp * cosSharp * gazeLen2;
		const float midK = cosMid * cosMid * gazeLen2;
		const float coarseK = cosCoarse * cosCoarse * gazeLen2;
		for ( y = 0; y < g->mapHeight; y++ ) {
			const float py = VK_FdmBinNearest( ( y + 0.5f ) * g->texelHeight, gazeY, g->tileHeight );
			const float ty = tanU - py / g->height * spanY;
			for ( x = 0; x < g->mapWidth; x++ ) {
				const float px = VK_FdmBinNearest( ( x + 0.5f ) * g->texelWidth, gazeX, g->tileWidth );
				const float tx = tanL + px / g->width * spanX;
				const float dot = tx * gx + ty * gy + 1.0f;
				const float cosNum = dot * dot, texelLen2 = tx * tx + ty * ty + 1.0f;
				uint8_t value;
				// The device scales by at most four an axis, rounding one axis up inside whatever area the
				// two channels leave spare. 255, 127 and 63 land square; 64 leaves room for the round-up
				if ( cosNum > sharpK * texelLen2 )
					value = 255; // 1x1
				else if ( cosNum > midK * texelLen2 )
					value = 127; // 2x2
				else if ( cosNum > coarseK * texelLen2 )
					value = 64;  // 2x4
				else
					value = 63;  // 4x4
				dst[0] = dst[1] = value;
				dst += 2;
			}
		}
	}
}
/* The part of a cell nearest the target along one axis: the tiler's bin, or the texel while the bin is unknown. */
static inline float VK_FdmCellNearest( uint32_t texel, uint32_t texelSize, float target, uint32_t tile ) {
	const float lo = (float)texel * texelSize;
	if ( tile )
		return VK_FdmBinNearest( lo + 0.5f * texelSize, target, tile );
	return target < lo ? lo : target > lo + texelSize ? lo + texelSize : target;
}
/*
 * The scope's view: full density wherever a cell reaches the centered circle, coarsest over the mask
 * outside it. The circle's diameter is a fraction of the buffer's width.
 */
static inline void VK_FdmWriteScope( uint8_t *dst, const vkFdmGeometry_t *g, float circleWidth ) {
	const float cx = 0.5f * g->width, cy = 0.5f * g->height, r = 0.5f * circleWidth * g->width;
	uint32_t layer, y, x;
	for ( layer = 0; layer < g->layers; layer++ )
		for ( y = 0; y < g->mapHeight; y++ ) {
			const float dy = VK_FdmCellNearest( y, g->texelHeight, cy, g->tileHeight ) - cy;
			for ( x = 0; x < g->mapWidth; x++ ) {
				const float dx = VK_FdmCellNearest( x, g->texelWidth, cx, g->tileWidth ) - cx;
				dst[0] = dst[1] = dx * dx + dy * dy <= r * r ? 255 : 63;
				dst += 2;
			}
		}
}
/* Texels (x0, y0, x1, y1, ends exclusive) a Vulkan NDC rectangle touches; 0 when none. */
static inline int VK_FdmCarveTexels( const vkFdmGeometry_t *g, const float rect[4], int32_t texels[4] ) {
	float x0, y0, x1, y1;
	int i;
	for ( i = 0; i < 4; i++ )
		if ( !isfinite( rect[i] ) )
			return 0;
	if ( !g->texelWidth || !g->texelHeight )
		return 0;
	x0 = ( rect[0] * 0.5f + 0.5f ) * g->width;
	y0 = ( rect[1] * 0.5f + 0.5f ) * g->height;
	x1 = ( rect[2] * 0.5f + 0.5f ) * g->width;
	y1 = ( rect[3] * 0.5f + 0.5f ) * g->height;
	/* Only what the framebuffer shows: the map's last texels run past its edge */
	if ( x0 < 0.0f ) x0 = 0.0f;
	if ( y0 < 0.0f ) y0 = 0.0f;
	if ( x1 > (float)g->width ) x1 = (float)g->width;
	if ( y1 > (float)g->height ) y1 = (float)g->height;
	if ( x1 <= x0 || y1 <= y0 )
		return 0;
	x0 = floorf( x0 / g->texelWidth );
	y0 = floorf( y0 / g->texelHeight );
	x1 = ceilf( x1 / g->texelWidth );
	y1 = ceilf( y1 / g->texelHeight );
	if ( x0 < 0.0f ) x0 = 0.0f;
	if ( y0 < 0.0f ) y0 = 0.0f;
	if ( x1 > (float)g->mapWidth ) x1 = (float)g->mapWidth;
	if ( y1 > (float)g->mapHeight ) y1 = (float)g->mapHeight;
	if ( x1 <= x0 || y1 <= y0 )
		return 0;
	texels[0] = (int32_t)x0;
	texels[1] = (int32_t)y0;
	texels[2] = (int32_t)x1;
	texels[3] = (int32_t)y1;
	return 1;
}
static inline void VK_FdmFillFull( uint8_t *map, const vkFdmGeometry_t *g, uint32_t layer, const int32_t texels[4] ) {
	int32_t y;
	if ( layer >= g->layers )
		return;
	for ( y = texels[1]; y < texels[3]; y++ )
		memset( map + ( ( (size_t)layer * g->mapHeight + y ) * g->mapWidth + texels[0] ) * 2, 0xFF,
			(size_t)( texels[2] - texels[0] ) * 2 );
}
/* A mask holds one byte a map texel, in framebuffer position, set where the map must stay full. */
static inline void VK_FdmMarkNdc( uint8_t *mask, const vkFdmGeometry_t *g, const float rect[4] ) {
	int32_t t[4], y;
	if ( !VK_FdmCarveTexels( g, rect, t ) )
		return;
	for ( y = t[1]; y < t[3]; y++ )
		memset( mask + (size_t)y * g->mapWidth + t[0], 1, (size_t)( t[2] - t[0] ) );
}
/* Whether a mask marks any texel in the framebuffer pixels [x0, x1) by [y0, y1). */
static inline int VK_FdmMaskAny( const uint8_t *mask, const vkFdmGeometry_t *g, int32_t x0, int32_t y0, int32_t x1,
	int32_t y1 ) {
	const int32_t tw = (int32_t)g->texelWidth, th = (int32_t)g->texelHeight;
	const int32_t w = (int32_t)g->mapWidth, h = (int32_t)g->mapHeight;
	int32_t tx0 = (int32_t)floorf( (float)x0 / tw ), ty0 = (int32_t)floorf( (float)y0 / th );
	int32_t tx1 = (int32_t)ceilf( (float)x1 / tw ), ty1 = (int32_t)ceilf( (float)y1 / th ), x, y;
	if ( tx0 < 0 ) tx0 = 0;
	if ( ty0 < 0 ) ty0 = 0;
	if ( tx1 > w ) tx1 = w;
	if ( ty1 > h ) ty1 = h;
	for ( y = ty0; y < ty1; y++ )
		for ( x = tx0; x < tx1; x++ )
			if ( mask[y * w + x] )
				return 1;
	return 0;
}
/* Map texels [lo, hi) a cell reads along one axis; a cell past the map reads the clamped edge texel. */
static inline void VK_FdmCellTexels( int32_t lo, int32_t size, int32_t texel, int32_t count, int32_t *t0, int32_t *t1 ) {
	*t0 = (int32_t)floorf( (float)lo / texel );
	*t1 = (int32_t)ceilf( (float)( lo + size ) / texel );
	if ( *t1 <= 0 ) {
		*t0 = 0;
		*t1 = 1;
	} else if ( *t0 >= count ) {
		*t0 = count - 1;
		*t1 = count;
	} else {
		if ( *t0 < 0 ) *t0 = 0;
		if ( *t1 > count ) *t1 = count;
	}
}
/*
 * The tiler's bins hold still in the map and it reads the map at a pixel less the offset the pass
 * ends with, so a bin goes full when the framebuffer pixels it covers hold a mark. Bins the offset
 * puts past the map's edge read the edge, so they fill it.
 */
static inline void VK_FdmCarveMask( uint8_t *map, const vkFdmGeometry_t *g, uint32_t layer, const uint8_t *mask,
	const int32_t offset[2] ) {
	const int32_t tw = (int32_t)g->texelWidth, th = (int32_t)g->texelHeight;
	const int32_t w = (int32_t)g->mapWidth, h = (int32_t)g->mapHeight;
	const int32_t cw = ( g->tileWidth && g->tileHeight ) ? (int32_t)g->tileWidth : tw;
	const int32_t ch = ( g->tileWidth && g->tileHeight ) ? (int32_t)g->tileHeight : th;
	int32_t x, y, x0, y0, t[4];
	if ( !tw || !th || !w || !h )
		return;
	/* the first cells whose framebuffer pixels start on screen */
	x0 = (int32_t)floorf( (float)-offset[0] / cw ) * cw;
	y0 = (int32_t)floorf( (float)-offset[1] / ch ) * ch;
	for ( y = y0; y + offset[1] < (int32_t)g->height; y += ch )
		for ( x = x0; x + offset[0] < (int32_t)g->width; x += cw ) {
			if ( !VK_FdmMaskAny( mask, g, x + offset[0], y + offset[1], x + cw + offset[0], y + ch + offset[1] ) )
				continue;
			VK_FdmCellTexels( x, cw, tw, w, &t[0], &t[2] );
			VK_FdmCellTexels( y, ch, th, h, &t[1], &t[3] );
			VK_FdmFillFull( map, g, layer, t );
		}
}
#define VK_FDM_ROUND_UP( value, alignment ) ( ( ( value ) + ( alignment ) - 1 ) / ( alignment ) * ( alignment ) )
/* Whether the A7xx LRZ fast-clear flag RAM (1024 bytes) covers a two-layer depth image this size (fdl6 LRZ layout). */
static inline int VK_FdmLrzCovered( uint32_t width, uint32_t height, uint32_t samples ) {
	uint32_t pitch, rows;
	// LRZ covers the supersampled surface (fdl6_lrz_get_super_sampled_size)
	if ( samples >= 2 ) height *= 2;
	if ( samples >= 4 ) width *= 2;
	if ( samples >= 8 ) height *= 2;
	pitch = VK_FDM_ROUND_UP( ( width + 7 ) / 8, 32 );
	rows = VK_FDM_ROUND_UP( ( height + 7 ) / 8, 32 );
	return VK_FDM_ROUND_UP( ( pitch * rows * 2 ) >> 7, 512 ) / 8 * 2 <= 1024;
}
/*
 * A depth image made for density map offsets gets its LRZ padded by the largest tile that keeps LRZ fast
 * clears, and the tiles are then held to that (fdl6_lrz_get_max_fdm_extra_size).
 */
static inline void VK_FdmTurnipOffsetLimit( uint32_t width, uint32_t height, uint32_t samples, uint32_t *limitW,
	uint32_t *limitH ) {
	uint32_t extra;
	*limitW = 2016;
	*limitH = 2032;
	if ( !VK_FdmLrzCovered( width, height, samples ) )
		return;
	for ( extra = 2016; extra > 192; extra -= 4 )
		if ( VK_FdmLrzCovered( width + extra, height + extra, samples ) ) {
			*limitW = extra / 16 * 16;
			*limitH = extra / 4 * 4;
			return;
		}
}
/*
 * Turnip's bin for a two-view pass on an Adreno 750 (tu_util.cc), from each GMEM attachment's bytes a pixel:
 * 3 MB GMEM less the VPC attribute buffer and a quarter of the color cache, split by the bytes; then the fewest
 * bins with no side over twice the other, most square on a tie. 0 for a format outside the model; stale if
 * Turnip changes its tiling.
 */
static inline int VK_FdmTurnipTile( uint32_t width, uint32_t height, const uint32_t *cpp, uint32_t count,
	uint32_t maxWidth, uint32_t maxHeight, uint32_t *tileWidth, uint32_t *tileHeight ) {
	const uint32_t gmem = 3 * 1024 * 1024 - 6 * 0xc000 - ( 6 * 64 * 1024 ) / 4;
	const uint32_t alignW = 96, alignH = 32, layers = 2, gmemAlign = 8 * alignW * alignH;
	uint32_t blocks = gmem / gmemAlign, total = 0, pixels = ~0u, best = ~0u, bestW = 0, bestH = 0, i, w;
	if ( !count || !width || !height )
		return 0;
	for ( i = 0; i < count; i++ ) {
		if ( !cpp[i] )
			return 0;
		total += cpp[i];
	}
	for ( i = 0; i < count; i++ ) {
		// wide pixels take whole pairs (or more) of blocks
		const uint32_t align = ( cpp[i] >> 3 ) ? ( cpp[i] >> 3 ) : 1;
		uint32_t n = ( blocks * cpp[i] / total ) & ~( align - 1 );
		if ( n < align )
			n = align;
		blocks -= n;
		total -= cpp[i];
		if ( n * gmemAlign / cpp[i] < pixels )
			pixels = n * gmemAlign / cpp[i];
	}
	for ( w = alignW; w <= maxWidth && w <= VK_FDM_ROUND_UP( width, alignW ); w += alignW ) {
		uint32_t h = pixels / ( w * layers ), countW, countH, cost;
		if ( h > maxHeight )
			h = maxHeight;
		if ( h > VK_FDM_ROUND_UP( height, alignH ) )
			h = VK_FDM_ROUND_UP( height, alignH );
		h = h / alignH * alignH;
		if ( !h )
			continue;
		cost = ( w > h * 2 || h > w * 2 ) ? 1000 : 0;
		countW = ( width + w - 1 ) / w;
		countH = ( height + h - 1 ) / h;
		h = VK_FDM_ROUND_UP( ( height + countH - 1 ) / countH, alignH );
		cost += countW * countH;
		if ( cost < best ||
			( cost == best && ( w > h ? w - h : h - w ) < ( bestW > bestH ? bestW - bestH : bestH - bestW ) ) ) {
			best = cost;
			bestW = w;
			bestH = h;
		}
	}
	if ( !bestW )
		return 0;
	*tileWidth = bestW;
	*tileHeight = bestH;
	return 1;
}
/*
 * Each eye's map is drawn around its optical axis, in pixels. With the bin known, the point moves to the middle
 * of the bin holding the axis: the tiler samples the map at bin centers, and the offsets keep that lattice fixed
 * in the map, so the gaze always lands mid-bin.
 */
static inline void VK_FdmReference( const vkFdmGeometry_t *g, const float fovTan[2][4], int32_t ref[2][2] ) {
	const float width = (float)g->width, height = (float)g->height;
	int eye;
	for ( eye = 0; eye < 2; eye++ ) {
		const float spanX = fovTan[eye][1] - fovTan[eye][0], spanY = fovTan[eye][2] - fovTan[eye][3];
		float axisX = ( spanX > 1e-6f ) ? -fovTan[eye][0] / spanX * width : 0.5f * width;
		float axisY = ( spanY > 1e-6f ) ? fovTan[eye][2] / spanY * height : 0.5f * height;
		if ( axisX < 0.0f ) axisX = 0.0f; else if ( axisX > width - 1.0f ) axisX = width - 1.0f;
		if ( axisY < 0.0f ) axisY = 0.0f; else if ( axisY > height - 1.0f ) axisY = height - 1.0f;
		if ( g->tileWidth && g->tileHeight ) {
			ref[eye][0] = (int32_t)( ( (uint32_t)axisX / g->tileWidth ) * g->tileWidth + g->tileWidth / 2 );
			ref[eye][1] = (int32_t)( ( (uint32_t)axisY / g->tileHeight ) * g->tileHeight + g->tileHeight / 2 );
		} else {
			ref[eye][0] = (int32_t)axisX;
			ref[eye][1] = (int32_t)axisY;
		}
	}
}
/*
 * The offset map, drawn with the gaze on each eye's reference point. Eccentricity is taken as if that point
 * were the optical axis, which only overstates the sharp region once the eye turns. With the bin known, a
 * texel takes the level its bin's nearest point to the gaze asks for, so a bin the sharp region reaches is
 * sharp all through.
 */
static inline void VK_FdmWriteFixed( uint8_t *dst, const vkFdmGeometry_t *g, float sharpDeg, float coarseDeg,
	const int32_t ref[2][2], const float fovTan[2][4] ) {
	const float toRad = 3.14159265358979f / 180.0f;
	const float midDeg = 0.5f * ( sharpDeg + coarseDeg );
	const float tileW = (float)g->tileWidth, tileH = (float)g->tileHeight;
	const int binned = g->tileWidth && g->tileHeight;
	float sharp2, mid2, coarse2, t;
	uint32_t layer, y, x;
	t = tanf( sharpDeg * toRad ); sharp2 = t * t;
	t = tanf( midDeg * toRad ); mid2 = t * t;
	t = tanf( coarseDeg * toRad ); coarse2 = t * t;
	for ( layer = 0; layer < g->layers; layer++ ) {
		const int eye = layer < 2 ? (int)layer : 0;
		const float spanX = fovTan[eye][1] - fovTan[eye][0], spanY = fovTan[eye][2] - fovTan[eye][3];
		const float tanPerPxX = g->width ? spanX / (float)g->width : 0.0f;
		const float tanPerPxY = g->height ? spanY / (float)g->height : 0.0f;
		const float refX = (float)ref[eye][0], refY = (float)ref[eye][1];
		for ( y = 0; y < g->mapHeight; y++ ) {
			float py = ( (float)y + 0.5f ) * g->texelHeight, dy;
			if ( binned ) {
				const float y0 = floorf( py / tileH ) * tileH;
				py = ( refY < y0 ) ? y0 : ( refY > y0 + tileH ) ? y0 + tileH : refY;
			}
			dy = ( py - refY ) * tanPerPxY;
			for ( x = 0; x < g->mapWidth; x++ ) {
				float px = ( (float)x + 0.5f ) * g->texelWidth, dx, r2;
				uint8_t value;
				if ( binned ) {
					const float x0 = floorf( px / tileW ) * tileW;
					px = ( refX < x0 ) ? x0 : ( refX > x0 + tileW ) ? x0 + tileW : refX;
				}
				dx = ( px - refX ) * tanPerPxX;
				r2 = dx * dx + dy * dy;
				// the same four values as the gaze-drawn map, for the same reasons
				if ( r2 < sharp2 )
					value = 255; // 1x1
				else if ( r2 < mid2 )
					value = 127; // 2x2
				else if ( r2 < coarse2 )
					value = 64;  // 2x4
				else
					value = 63;  // 4x4
				dst[0] = dst[1] = value;
				dst += 2;
			}
		}
	}
}
/* Where the gaze sits relative to each eye's reference point, in whole steps of the offset granularity. */
static inline void VK_FdmOffsets( const vkFdmGeometry_t *g, const float center[2][2], const int32_t ref[2][2],
	uint32_t granularityWidth, uint32_t granularityHeight, int32_t offset[2][2] ) {
	const int32_t granX = granularityWidth ? (int32_t)granularityWidth : 1;
	const int32_t granY = granularityHeight ? (int32_t)granularityHeight : 1;
	int eye;
	for ( eye = 0; eye < 2; eye++ ) {
		float gx = ( center[eye][0] + 1.0f ) * 0.5f * (float)g->width;
		float gy = ( center[eye][1] + 1.0f ) * 0.5f * (float)g->height;
		if ( gx < 0.0f ) gx = 0.0f; else if ( gx > (float)g->width ) gx = (float)g->width;
		if ( gy < 0.0f ) gy = 0.0f; else if ( gy > (float)g->height ) gy = (float)g->height;
		offset[eye][0] = (int32_t)floorf( ( gx - (float)ref[eye][0] ) / (float)granX + 0.5f ) * granX;
		offset[eye][1] = (int32_t)floorf( ( gy - (float)ref[eye][1] ) / (float)granY + 0.5f ) * granY;
	}
}
#endif
