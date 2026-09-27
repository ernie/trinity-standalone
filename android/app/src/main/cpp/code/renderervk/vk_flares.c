/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// tr_flares.c

#include "tr_local.h"

/*
=============================================================================

LIGHT FLARES

A light flare is an effect that takes place inside the eye when bright light
sources are visible.  The size of the flare relative to the screen is nearly
constant, irrespective of distance, but the intensity should be proportional to the
projected area of the light source.

A surface that has been flagged as having a light flare will calculate the depth
buffer value that its midpoint should have when the surface is added.

After all opaque surfaces have been rendered, the depth buffer is read back for
each flare in view.  If the point has not been obscured by a closer surface, the
flare should be drawn.

Surfaces that have a repeated texture should never be flagged as flaring, because
there will only be a single flare added at the midpoint of the polygon.

To prevent abrupt popping, the intensity of the flare is interpolated up and
down as it changes visibility.  This involves scene to scene state, unlike almost
all other aspects of the renderer, and is complicated by the fact that a single
frame may have multiple scenes.

RB_RenderFlares() will be called once per view (twice in a mirrored scene, potentially
up to five or more times in a frame with 3D status bar icons).

=============================================================================
*/


// flare states maintain visibility over multiple frames for fading
// layers: view, mirror, menu
typedef struct flare_s {
	struct		flare_s	*next;		// for active chain

	int			addedFrame;
	uint32_t	testCount;

	portalView_t portalView;
	int			frameSceneNum;
	void		*surface;
	int			fogNum;

	int			testTime;			// refdef time of the last test, for the intensity slew
	qboolean	probeBinary;		// the last probe was the one-pixel test, answered yes or no
	float		target[2];			// per eye, what the last test asked for: 0, 1, or the covered fraction
	float		intensity[2];		// per eye, slews toward target; the corona's color in that eye

	float		drawIntensity;		// this view's draw gate, the brighter eye's intensity; non 0 while fading
	float		deferredIntensity;	// drawIntensity captured at own-view test; a later world view zeroes drawIntensity before the deferred draw

	int			windowX, windowY;
	float		eyeZ;
	float		drawZ;

	// Owning view's basis/metrics captured in RB_AddFlare so the deferred draw
	// sizes/orients the world-space billboard from the flare's own view instead
	// of whatever (HUD-icon/2D) view backEnd.viewParms holds at the 2D boundary.
	int			viewportWidth;		// owning view's viewport width, for corona sizing
	float		projScaleX;			// owning view's projectionMatrix[0] (1/tanHalfFovX)
	vec3_t		viewOrigin;			// owning view's eye position
	vec3_t		viewLeft;			// owning view's or.axis[1]
	vec3_t		viewUp;				// owning view's or.axis[2]

	vec3_t		origin;
	vec3_t		normal;				// surface normal the probe patch lies in; zero when unknown
	vec3_t		color;
} flare_t;

static flare_t	r_flareStructs[ MAX_FLARES ];
static flare_t	*r_activeFlares, *r_inactiveFlares;

// Main (PV_NONE) view stereo projection + mono modelview, captured in
// RB_RenderFlares so RB_RenderDeferredFlares can re-establish the exact main
// view camera after later views have overwritten the eyeProj UBO / viewParms.
static float	deferredEyeProj[2][16];
static float	deferredModelMatrix[16];

// In-world VR HUD sprite deferral: the HUD RT_SPRITE is intercepted in
// RB_SurfaceSprite during the main view and replayed in the post-scene pass AFTER the corona
// (RB_DrawDeferredHud) so opaque HUD pixels composite over the additive corona. All
// state is captured in the back end at sprite-draw time so the replay uses the exact
// main-view camera the sprite would have drawn with, independent of r_flares.
static float		deferredHudEyeProj[2][16];	// main view's per-eye projections
static float		deferredHudModelMatrix[16];	// main view's mono world modelview
static vec3_t		deferredHudOrigin;			// world-space sprite center
static vec3_t		deferredHudLeft, deferredHudUp;	// resolved (aspect/rotation-applied) billboard axes
static color4ub_t	deferredHudColor;			// e.shaderRGBA
static int			deferredHudFogNum;


/*
==================
R_ClearFlares
==================
*/
void R_ClearFlares( void ) {
	int		i;

	if ( !vk.fragmentStores )
		return;

	Com_Memset( r_flareStructs, 0, sizeof( r_flareStructs ) );
	r_activeFlares = NULL;
	r_inactiveFlares = NULL;

	for ( i = 0 ; i < MAX_FLARES ; i++ ) {
		r_flareStructs[i].next = r_inactiveFlares;
		r_inactiveFlares = &r_flareStructs[i];
	}
}


static flare_t *R_SearchFlare( void *surface )
{
	flare_t *f;

	// see if a flare with a matching surface, scene, and view exists
	for ( f = r_activeFlares ; f ; f = f->next ) {
		if ( f->surface == surface && f->frameSceneNum == backEnd.viewParms.frameSceneNum && f->portalView == backEnd.viewParms.portalView ) {
			return f;
		}
	}

	return NULL;
}


/*
==================
RB_AddFlare

This is called at surface tesselation time
==================
*/
void RB_AddFlare( void *surface, int fogNum, vec3_t point, vec3_t color, vec3_t normal ) {
	int				i;
	flare_t			*f;
	vec3_t			local;
	float			d = 1;
	vec4_t			eye, clip, normalized, window;

	backEnd.pc.c_flareAdds++;

	if ( normal && (normal[0] || normal[1] || normal[2] ) )	{
		VectorSubtract( backEnd.viewParms.or.origin, point, local );
		VectorNormalizeFast( local );
		d = DotProduct( local, normal );
		// If the viewer is behind the flare don't add it.
		if ( d < 0 ) {
			return;
		}
	}

	// if the point is off the screen, don't bother adding it
	// calculate screen coordinates and depth
	R_TransformModelToClip( point, backEnd.or.modelMatrix, backEnd.viewParms.projectionMatrix, eye, clip );

	// check to see if the point is completely off screen
	for ( i = 0 ; i < 3 ; i++ ) {
		if ( clip[i] >= clip[3] || clip[i] <= -clip[3] ) {
			return;
		}
	}

	R_TransformClipToWindow( clip, &backEnd.viewParms, normalized, window );

	if ( window[0] < 0 || window[0] >= backEnd.viewParms.viewportWidth || window[1] < 0 || window[1] >= backEnd.viewParms.viewportHeight ) {
		return;	// shouldn't happen, since we check the clip[] above, except for FP rounding
	}

	f = R_SearchFlare( surface );

	// allocate a new one
	if ( !f ) {
		if ( !r_inactiveFlares ) {
			// the list is completely full
			return;
		}
		f = r_inactiveFlares;
		r_inactiveFlares = r_inactiveFlares->next;
		f->next = r_activeFlares;
		r_activeFlares = f;

		f->surface = surface;
		f->frameSceneNum = backEnd.viewParms.frameSceneNum;
		f->portalView = backEnd.viewParms.portalView;
		f->target[0] = f->target[1] = 0.0f;
		f->intensity[0] = f->intensity[1] = 0.0f;
		f->probeBinary = qtrue;
		f->testTime = backEnd.refdef.time;
		f->testCount = 0;
	} else {
		++f->testCount;
	}

	f->addedFrame = backEnd.viewParms.frameCount;
	f->fogNum = fogNum;

	VectorCopy( point, f->origin );
	VectorCopy( color, f->color );
	if ( normal ) {
		VectorCopy( normal, f->normal );
	} else {
		VectorClear( f->normal );
	}

	// fade the intensity of the flare down as the
	// light surface turns away from the viewer
	VectorScale( f->color, d, f->color );

	// save info needed to test
	f->windowX = backEnd.viewParms.viewportX + window[0];
	f->windowY = backEnd.viewParms.viewportY + window[1];

	// captured now (own view) so the deferred draw doesn't size/orient off
	// whatever view is last at the 2D boundary
	f->viewportWidth = backEnd.viewParms.viewportWidth;
	f->projScaleX = backEnd.viewParms.projectionMatrix[0];
	VectorCopy( backEnd.viewParms.or.origin, f->viewOrigin );
	VectorCopy( backEnd.viewParms.or.axis[1], f->viewLeft );
	VectorCopy( backEnd.viewParms.or.axis[2], f->viewUp );

	f->eyeZ = eye[2];

#ifdef USE_REVERSED_DEPTH
	f->drawZ = (clip[2]+0.20) / clip[3];
#else
	f->drawZ = (clip[2]-0.20) / clip[3];
#endif

}


/*
==================
RB_AddDlightFlares
==================
*/
void RB_AddDlightFlares( void ) {
	dlight_t		*l;
	int				i, j, k;
	fog_t			*fog = NULL;

	if ( !r_flares->integer ) {
		return;
	}

	l = backEnd.refdef.dlights;

	if ( tr.world )
		fog = tr.world->fogs;

	for ( i = 0 ; i < backEnd.refdef.num_dlights; i++, l++ ) {

		if ( fog )
		{
			// find which fog volume the light is in
			for ( j = 1 ; j < tr.world->numfogs ; j++ ) {
				fog = &tr.world->fogs[j];
				for ( k = 0 ; k < 3 ; k++ ) {
					if ( l->origin[k] < fog->bounds[0][k] || l->origin[k] > fog->bounds[1][k] ) {
						break;
					}
				}
				if ( k == 3 ) {
					break;
				}
			}
			if ( j == tr.world->numfogs ) {
				j = 0;
			}
		}
		else
			j = 0;

		RB_AddFlare( (void *)l, j, l->origin, l->color, NULL );
	}
}

/*
===============================================================================

FLARE BACK END

===============================================================================
*/


// Probe quad width in coarse blocks. Four guarantees a 2x2 grid of block samples whatever the alignment
#define FLARE_PATCH_BLOCKS 4

// Fraction of taps that reads as fully visible. A recessed lamp always has some plane behind the housing, so saturate early
#define FLARE_PATCH_FULL_AT 0.25f

// Push block for dot.vert, in floats
#define PROBE_CENTER( e )	( ( e ) * 4 )
#define PROBE_DU( e )		( 8 + ( e ) * 4 )
#define PROBE_DV( e )		( 16 + ( e ) * 4 )
#define PROBE_EXTENT		24		// s0, t0, s1, t1
#define PROBE_PARAMS		28		// x: counter select


/*
==================
RB_ProbeScreenAxes

Rewrite one eye's in-plane unit deltas so the quad's corner moves exactly one pixel
along screen x and one along y: a pixel-aligned square always covers a pixel center,
while a plane-aligned parallelogram can straddle a corner and produce no fragment.
Near edge-on that runs off along the plane, so the quad falls back to a flat one.
==================
*/
static void RB_ProbeScreenAxes( float *push, int eye, float pixels, const float *proj )
{
	const float *c = push + PROBE_CENTER( eye );
	float *du = push + PROBE_DU( eye );
	float *dv = push + PROBE_DV( eye );
	const float w2 = c[3] * c[3];
	const float halfW = 0.5f * (float)vk.renderWidth;
	const float halfH = 0.5f * (float)vk.renderHeight;
	const float gux = ( du[0] * c[3] - c[0] * du[3] ) / w2 * halfW;
	const float guy = ( du[1] * c[3] - c[1] * du[3] ) / w2 * halfH;
	const float gvx = ( dv[0] * c[3] - c[0] * dv[3] ) / w2 * halfW;
	const float gvy = ( dv[1] * c[3] - c[1] * dv[3] ) / w2 * halfH;
	const float det = gux * gvy - guy * gvx;
	const float scale = fabsf( proj[0] ) > 1e-6f ? fabsf( proj[0] ) : 1.0f;
	const float cap = 8.0f * pixels * c[3] / ( (float)vk.renderWidth * scale );
	float ax, bx, ay, by, dx[4], dy[4];
	int k;

	push[PROBE_EXTENT + eye * 2 + 0] = pixels * 0.5f;
	push[PROBE_EXTENT + eye * 2 + 1] = pixels * 0.5f;

	if ( fabsf( det ) > 1e-12f ) {
		// a = ax u + bx v projects to one pixel along x; b likewise along y
		ax = gvy / det;
		bx = -guy / det;
		ay = -gvx / det;
		by = gux / det;
		// u and v are unit and orthogonal, so these are world lengths per pixel
		if ( sqrtf( ax * ax + bx * bx ) * pixels * 0.5f <= cap &&
			sqrtf( ay * ay + by * by ) * pixels * 0.5f <= cap ) {
			for ( k = 0; k < 4; k++ ) {
				dx[k] = ax * du[k] + bx * dv[k];
				dy[k] = ay * du[k] + by * dv[k];
			}
			Com_Memcpy( du, dx, sizeof( dx ) );
			Com_Memcpy( dv, dy, sizeof( dy ) );
			return;
		}
	}

	// edge-on fallback: flat quad at the flare's depth; one pixel is 2/W in ndc, 2w/W in clip
	Com_Memset( du, 0, 4 * sizeof( float ) );
	Com_Memset( dv, 0, 4 * sizeof( float ) );
	du[0] = c[3] / halfW;
	dv[1] = c[3] / halfH;
}


/*
==================
RB_TestFlare

Visibility comes from a probe quad in the flare's surface plane, depth tested with
writes off: uncovered fragments count themselves in a storage buffer, an untested
second draw counts them all, both read back a frame later. Under a density map a one
pixel probe flickers with head motion, so the quad spans a few blocks and visibility
is the fraction of taps passed. Per eye; dot.vert picks by gl_ViewIndex.
==================
*/
static void RB_TestFlare( flare_t *f ) {
	float		push[FLARE_PROBE_PUSH_FLOATS];
	vec3_t		normal, u, v, p;
	vec4_t		eye, clip, clipU, clipV;
	uint32_t	*slot;
	uint32_t	passed, total, offset;
	float		patch, dt, step;
	int			block, i, k;

	backEnd.pc.c_flareTests++;

	// passed and total per eye; the stride keeps the dynamic offset aligned
	offset = (f - r_flareStructs) * vk.storage_alignment;
	slot = (uint32_t*)( vk.storage.buffer_ptr + offset );

	// last frame's counts, reset here: multiview gives no ordering between the two views' invocations
	for ( i = 0; i < 2; i++ ) {
		passed = slot[i * 2 + 0];
		total = slot[i * 2 + 1];
		slot[i * 2 + 0] = 0;
		slot[i * 2 + 1] = 0;

		if ( !f->testCount ) {
			continue;
		}
		if ( f->probeBinary ) {
			f->target[i] = passed ? 1.0f : 0.0f;
		} else if ( total ) {
			f->target[i] = (float)passed / ( (float)total * FLARE_PATCH_FULL_AT );
			if ( f->target[i] > 1.0f ) {
				f->target[i] = 1.0f;
			}
		}
	}

	if ( f->testCount ) {
		f->testCount = 1;
	}

	// patch plane: the surface normal, or the sight line when the flare has none
	if ( f->normal[0] || f->normal[1] || f->normal[2] ) {
		VectorCopy( f->normal, normal );
	} else {
		VectorSubtract( f->viewOrigin, f->origin, normal );
	}
	if ( DotProduct( normal, normal ) < 0.0001f ) {
		VectorCopy( f->viewLeft, normal );
	}
	VectorNormalizeFast( normal );
	CrossProduct( normal, f->viewUp, u );
	if ( DotProduct( u, u ) < 0.0001f ) {
		// normal along the view up vector: view left is in the plane
		VectorCopy( f->viewLeft, u );
	} else {
		VectorNormalizeFast( u );
	}
	CrossProduct( normal, u, v );

	// per-eye center and unit-axis deltas through the scene's transform; clip space is linear in world space
	Com_Memset( push, 0, sizeof( push ) );
	block = 1;
	for ( i = 0; i < 2; i++ ) {
		R_TransformModelToClip( f->origin, backEnd.viewParms.world.modelMatrix,
			vk_view_eyeproj[i], eye, clip );
		VectorAdd( f->origin, u, p );
		R_TransformModelToClip( p, backEnd.viewParms.world.modelMatrix,
			vk_view_eyeproj[i], eye, clipU );
		VectorAdd( f->origin, v, p );
		R_TransformModelToClip( p, backEnd.viewParms.world.modelMatrix,
			vk_view_eyeproj[i], eye, clipV );
		for ( k = 0; k < 4; k++ ) {
			push[PROBE_DU( i ) + k] = clipU[k] - clip[k];
			push[PROBE_DV( i ) + k] = clipV[k] - clip[k];
		}

		if ( clip[3] > 0.0f ) {
			const int b = vk_foveation_block_at( i, clip[0] / clip[3], clip[1] / clip[3] );
			if ( b > block ) {
				block = b;
			}
		}

		// biased toward the viewer
#ifdef USE_REVERSED_DEPTH
		clip[2] += 0.20f;
#else
		clip[2] -= 0.20f;
#endif
		Com_Memcpy( push + PROBE_CENTER( i ), clip, sizeof( vec4_t ) );
	}

	// with a density map even the sharp island gets a patch, so the answer keeps its kind as the island moves
	if ( block <= 1 && !( vk.xr.foveationActive && vk.xr.fdmLevel > 0 ) ) {
		patch = 1.0f;
	} else {
		patch = (float)( FLARE_PATCH_BLOCKS * block );
	}
	for ( i = 0; i < 2; i++ ) {
		if ( push[PROBE_CENTER( i ) + 3] > 0.0f ) {
			RB_ProbeScreenAxes( push, i, patch, vk_view_eyeproj[i] );
		}
		// behind this eye: extents stay zero, the quad collapses and counts nothing
	}

	// six dummy vertices: the pipeline still binds location 0, but the corners come from the push block
	Com_Memset( tess.xyz, 0, 6 * sizeof( tess.xyz[0] ) );
	tess.numVertexes = 6;

#ifdef USE_VBO
	tess.vboIndex = 0;
#endif
	// invalidate descriptors
	for ( i = 0; i < VK_DESC_COUNT; i++ ) {
		vk_reset_descriptor( i );
	}
	vk_bind_geometry( TESS_XYZ );

	push[PROBE_PARAMS] = 0.0f;
	vk_draw_flare_probe( offset, push, qtrue );
	if ( patch > 1.0f ) {
		// the fraction needs the total too
		push[PROBE_PARAMS] = 1.0f;
		vk_draw_flare_probe( offset, push, qfalse );
	}
	f->probeBinary = ( patch <= 1.0f );

	// slew at r_flareFade per second, so a jump between block-grid alignments becomes a few percent a frame
	dt = ( backEnd.refdef.time - f->testTime ) * 0.001f;
	f->testTime = backEnd.refdef.time;
	if ( dt < 0.0f ) {
		dt = 0.0f;
	} else if ( dt > 0.25f ) {
		dt = 0.25f;
	}
	step = r_flareFade->value * dt;
	for ( i = 0; i < 2; i++ ) {
		if ( f->intensity[i] < f->target[i] ) {
			f->intensity[i] += step;
			if ( f->intensity[i] > f->target[i] ) {
				f->intensity[i] = f->target[i];
			}
		} else {
			f->intensity[i] -= step;
			if ( f->intensity[i] < f->target[i] ) {
				f->intensity[i] = f->target[i];
			}
		}
	}

	f->drawIntensity = MAX( f->intensity[0], f->intensity[1] );
}


// One corona of a batch: the billboard and each eye's color, worked out before the batch opens
typedef struct {
	const flare_t	*flare;
	vec3_t			left, up;
	color4ub_t		color[2];
} flareDraw_t;

static flareDraw_t	flareBatch[ MAX_FLARES ];


/*
==================
RB_SetupFlare

Fills d from f; qfalse when falloff or fog leave both eyes black. The fog math
uses tess, so this never runs while a batch is open.
==================
*/
static qboolean RB_SetupFlare( const flare_t *f, flareDraw_t *d ) {
	float			size;
	float distance, intensity, factor;
	float radius;
	vec3_t			dir;
	byte fogFactors[3] = {255, 255, 255};
	int				e, k;

	// We don't want too big values anyways when dividing by distance.
	if ( f->eyeZ > -1.0f )
		distance = 1.0f;
	else
		distance = -f->eyeZ;

	// calculate the flare size.. use the flare's own captured viewport width so a
	// deferred draw isn't mis-sized by the last (HUD-icon) view's viewParms
	size = f->viewportWidth * ( r_flareSize->value/640.0f + 8 / distance );

/*
 * This is an alternative to intensity scaling. It changes the size of the flare on screen instead
 * with growing distance. See in the description at the top why this is not the way to go.
	// size will change ~ 1/r.
	size = f->viewportWidth * (r_flareSize->value / (distance * -2.0f));
*/

/*
 * As flare sizes stay nearly constant with increasing distance we must decrease the intensity
 * to achieve a reasonable visual result. The intensity is ~ (size^2 / distance^2) which can be
 * got by considering the ratio of
 * (flaresurface on screen) : (Surface of sphere defined by flare origin and distance from flare)
 * An important requirement is:
 * intensity <= 1 for all distances.
 *
 * The formula used here to compute the intensity is as follows:
 * intensity = flareCoeff * size^2 / (distance + size*sqrt(flareCoeff))^2
 * As you can see, the intensity will have a max. of 1 when the distance is 0.
 * The coefficient flareCoeff will determine the falloff speed with increasing distance.
 */

	factor = distance + size * sqrt( r_flareCoeff->value );

	intensity = r_flareCoeff->value * size * size / ( factor * factor );

	// Calculations for fogging
	if ( tr.world && f->fogNum > 0 && f->fogNum < tr.world->numfogs )
	{
		tess.numVertexes = 1;
		VectorCopy( f->origin, tess.xyz[0] );
		tess.fogNum = f->fogNum;

		RB_CalcModulateColorsByFog( fogFactors );

		// We don't need to render the flare if colors are 0 anyways.
		if ( !(fogFactors[0] || fogFactors[1] || fogFactors[2]) )
			return qfalse;
	}

	for ( e = 0; e < 2; e++ ) {
		for ( k = 0; k < 3; k++ ) {
			d->color[e].rgba[k] = f->color[k] * f->intensity[e] * intensity * fogFactors[k];
		}
		d->color[e].rgba[3] = 255;
	}

	// falloff/fog can quantize the color to black; an additive black quad
	// contributes nothing but still pays full depth-test-disabled fill
	if ( !( d->color[0].rgba[0] | d->color[0].rgba[1] | d->color[0].rgba[2] |
			d->color[1].rgba[0] | d->color[1].rgba[1] | d->color[1].rgba[2] ) )
		return qfalse;

	// World-space billboard at the flare origin, sized to subtend the same
	// screen fraction as the classic window-space quad (`size` pixels of
	// viewportWidth at `distance`): half-width = 2 * size/W * distance * tanHalfFovX.
	// The stereo projection is applied per eye by the pipeline, so the corona
	// gets correct parallax and asymmetric-FOV placement in VR. All view metrics
	// come from the flare's own captured view (f->) so the deferred draw is not
	// mis-sized/oriented by the last (HUD-icon/2D) view's viewParms.
	radius = 2.0f * distance * ( size / f->viewportWidth ) / f->projScaleX;

	// Viewer-facing basis: the quad faces the viewer's POSITION (view-plane
	// alignment tracks head orientation instead and foreshortens at
	// peripheral gaze angles); in-plane spin follows the view's up vector.
	VectorSubtract( f->origin, f->viewOrigin, dir );
	VectorNormalizeFast( dir );
	CrossProduct( f->viewUp, dir, d->left );
	if ( DotProduct( d->left, d->left ) < 0.0001f ) {
		// sight line parallel to view up: fall back to view left
		VectorCopy( f->viewLeft, d->left );
	} else {
		VectorNormalizeFast( d->left );
	}
	CrossProduct( dir, d->left, d->up );

	VectorScale( d->left, radius, d->left );
	VectorScale( d->up, radius, d->up );

	if ( f->portalView == PV_MIRROR ) {
		VectorSubtract( vec3_origin, d->left, d->left );
	}

	d->flare = f;
	return qtrue;
}


/*
==================
RB_DrawFlareBatch

Draws the batch in one draw, or one per eye with the other eye's projection
collapsed when the eyes' fades differ. vk_view_eyeproj must hold the batch's view.

Depth handling: the flare stage pipelines have the depth test disabled
(rebaked in CreateExternalShaders), so the billboard is not swallowed
by the light-fixture surface it sits on. Occlusion is the probe's job.
==================
*/
static void RB_DrawFlareBatch( const flareDraw_t *batch, int count ) {
	float	eyeProj[2][16];
	int		e, i, eyes, fogNum;

	eyes = 1;
	for ( i = 0; i < count; i++ ) {
		if ( memcmp( &batch[i].color[0], &batch[i].color[1], sizeof( batch[i].color[0] ) ) ) {
			eyes = 2;
			break;
		}
	}

	Com_Memcpy( eyeProj, vk_view_eyeproj, sizeof( eyeProj ) );

	for ( e = 0; e < eyes; e++ ) {
		if ( eyes == 2 ) {
			// z = 2w lies past the far plane, so the other eye's layer clips every vertex
			Com_Memset( vk_view_eyeproj[1 - e], 0, sizeof( vk_view_eyeproj[0] ) );
			vk_view_eyeproj[1 - e][14] = 2.0f;
			vk_view_eyeproj[1 - e][15] = 1.0f;
			VK_PushEyeProj();
		}

		fogNum = -1;
		for ( i = 0; i < count; i++ ) {
			const color4ub_t *c = &batch[i].color[e];
			if ( !( c->rgba[0] | c->rgba[1] | c->rgba[2] ) )
				continue;
			if ( batch[i].flare->fogNum != fogNum ) {
				if ( fogNum >= 0 )
					RB_EndSurface();
				fogNum = batch[i].flare->fogNum;
				RB_BeginSurface( tr.flareShader, fogNum );
			}
			RB_AddQuadStamp( batch[i].flare->origin, batch[i].left, batch[i].up, *c );
		}
		if ( fogNum >= 0 )
			RB_EndSurface();

		if ( eyes == 2 ) {
			Com_Memcpy( vk_view_eyeproj, eyeProj, sizeof( eyeProj ) );
		}
	}

	if ( eyes == 2 ) {
		VK_PushEyeProj();
	}
}


/*
==================
RB_RenderFlares

Because flares are simulating an occular effect, they should be drawn after
everything (all views) in the entire frame has been drawn.

Because of the way portals use the depth buffer to mark off areas, the
needed information would be lost after each view, so we are forced to draw
flares after each view.

The resulting artifact is that flares in mirrors or portals don't dim properly
when occluded by something in the main view, and portal flares that should
extend past the portal edge will be overwritten.
==================
*/
void RB_RenderFlares( void ) {
	flare_t		*f;
	flare_t		**prev;
	qboolean	draw;
	int			count;

	if ( !r_flares->integer ) {
		return;
	}

	if ( vk.renderPassIndex == RENDER_PASS_SCREENMAP ) {
		return;
	}

	if ( backEnd.isHyperspace ) {
		return;
	}

	// no world depth to probe against (HUD icons, UI models)
	if ( backEnd.refdef.rdflags & RDF_NOWORLDMODEL ) {
		return;
	}

	// Reset currentEntity to world so that any previously referenced entities
	// don't have influence on the rendering of these flares (i.e. RF_ renderer flags).
	backEnd.currentEntity = &tr.worldEntity;
	backEnd.or = backEnd.viewParms.world;

	//RB_AddDlightFlares();

	// perform z buffer readback on each flare in this view
	draw = qfalse;
	prev = &r_activeFlares;
	while ( ( f = *prev ) != NULL ) {
		// throw out any flares that weren't added last frame
		if ( backEnd.viewParms.frameCount - f->addedFrame > 0 && f->portalView == backEnd.viewParms.portalView ) {
			*prev = f->next;
			f->next = r_inactiveFlares;
			r_inactiveFlares = f;
			continue;
		}

		// don't draw any here that aren't from this scene / portal
		f->drawIntensity = 0;
		if ( f->frameSceneNum == backEnd.viewParms.frameSceneNum && f->portalView == backEnd.viewParms.portalView ) {
			RB_TestFlare( f );
			// deferred draw runs after any later world view zeroes
			// drawIntensity for this flare; preserve the real intensity here
			f->deferredIntensity = f->drawIntensity;
			if ( f->testCount == 0 ) {
				// recently added, wait 1 frame for test result
			} else if ( f->drawIntensity ) {
				draw = qtrue;
			} else {
				// this flare has completely faded out, so remove it from the chain
				*prev = f->next;
				f->next = r_inactiveFlares;
				r_inactiveFlares = f;
				continue;
			}
		}

		prev = &f->next;
	}

	if ( !draw ) {
		return;		// none visible
	}

	// Main-view coronas draw later via RB_RenderDeferredFlares (after bloom's
	// bright-pass, so they aren't re-bloomed). portal/mirror views keep the
	// classic per-view timing here, or a deferred draw would leak past the
	// portal edge / lose occlusion by the main view.
	if ( backEnd.viewParms.portalView == PV_NONE ) {
		// Capture this (main) view's per-eye projections and mono modelview so
		// the deferred draw can re-establish the exact camera: by 2D-boundary
		// time backEnd.viewParms / the eyeProj UBO belong to a later view.
		Com_Memcpy( deferredEyeProj, vk_view_eyeproj, sizeof( deferredEyeProj ) );
		Com_Memcpy( deferredModelMatrix, backEnd.viewParms.world.modelMatrix, sizeof( deferredModelMatrix ) );
		return;
	}

	// Flare quads are world-space billboards: push the mono world modelview
	// (same matrix world surfaces draw with) and let the per-view eyeProj UBO,
	// still holding this view's per-eye projections, provide the stereo
	// projection. Window-space ortho must NOT be used here: the generic
	// pipelines multiply by eyeProj, which garbles pre-projected vertices.
	vk_update_mvp( backEnd.viewParms.world.modelMatrix );

	count = 0;
	for ( f = r_activeFlares ; f ; f = f->next ) {
		if ( f->frameSceneNum == backEnd.viewParms.frameSceneNum && f->portalView == backEnd.viewParms.portalView && f->drawIntensity ) {
			backEnd.pc.c_flareRenders++;
			if ( RB_SetupFlare( f, &flareBatch[count] ) )
				count++;
		}
	}
	RB_DrawFlareBatch( flareBatch, count );

	//Com_Memcpy( vk_world.modelview_transform, modelMatrix_original, sizeof( modelMatrix_original ) );
	//vk_update_mvp( NULL );
}


/*
==================
RB_RenderDeferredFlares

Draws main-view (PV_NONE) coronas once per frame at the 3D->2D boundary: in FBO
mode after the blur chain has sampled the scene (so coronas aren't
re-bloomed), in direct mode (r_fbo 0) into the still-open main pass. doneFlares
guards the once-per-frame; the draw is idempotent across the hook sites.

The coronas are world-space billboards, so unlike the engine's window-space
deferred draw this must re-establish the MAIN view's camera: by the time we run,
backEnd.viewParms and the per-view eyeProj UBO belong to a later HUD-icon or 2D
pass. We restore the main view's per-eye projections (captured in RB_RenderFlares)
into the eyeProj UBO and push the main view's mono modelview; per-flare sizing/
orientation comes from state captured in RB_AddFlare.
==================
*/
void RB_RenderDeferredFlares( void ) {
	flare_t				*f;
	const trRefEntity_t	*savedEntity;
	int					count;

	if ( !r_flares->integer || backEnd.doneFlares )
		return;

	// Skip pure-2D frames (menu/disconnect): frameCount is frozen on the last 3D
	// frame there, so stale flares would still match and paint coronas over the UI.
	if ( !backEnd.doneSurfaces )
		return;

	// checked before marking done, so a screenmap pass can't suppress the real deferred draw
	if ( vk.renderPassIndex == RENDER_PASS_SCREENMAP )
		return;

	// Deferred coronas push a world modelview; if we somehow reach here in a 2D
	// projection (e.g. the end-of-frame fallback after 2D began) vk_update_mvp
	// would ignore it and mis-transform the quad. Only draw from the 3D->2D
	// boundary where projection2D is still false.
	if ( backEnd.projection2D )
		return;

	backEnd.doneFlares = qtrue;

	// save/restore currentEntity so the following 2D batch flushes with the entity the caller expects
	savedEntity = backEnd.currentEntity;
	backEnd.currentEntity = &tr.worldEntity;

	// Re-establish the main view's per-eye projections in the eyeProj UBO. The
	// UBO now holds a later (HUD-icon/2D) view's data, so without this the
	// world-space corona would be stereo-mismatched / mis-projected in-headset.
	Com_Memcpy( vk_view_eyeproj, deferredEyeProj, sizeof( vk_view_eyeproj ) );
	VK_PushEyeProj();

	// World-space billboards: push the captured main-view mono modelview; the
	// restored per-eye eyeProj UBO supplies the stereo projection.
	vk_update_mvp( deferredModelMatrix );

	count = 0;
	for ( f = r_activeFlares ; f ; f = f->next ) {
		if ( f->portalView == PV_NONE && f->addedFrame == backEnd.viewParms.frameCount ) {
			// restore intensity zeroed by a later world view since this flare's own test
			f->drawIntensity = f->deferredIntensity;
			if ( f->drawIntensity ) {
				backEnd.pc.c_flareRenders++;
				if ( RB_SetupFlare( f, &flareBatch[count] ) )
					count++;
			}
		}
	}
	RB_DrawFlareBatch( flareBatch, count );

	// Restore MVP state so the flare camera can't leak into subsequent 2D: when
	// projection2D is already set (end-of-frame hook site) this re-pushes the 2D
	// ortho + eyeProj; otherwise it just restores the mono modelview push.
	vk_update_mvp( NULL );
	backEnd.currentEntity = savedEntity;
}


/*
==================
RB_CaptureDeferredHud

Called from RB_SurfaceSprite (back end, main view) with the HUD sprite's already-
resolved world-space billboard. Stashes the geometry plus the exact main-view camera
(per-eye projections + mono modelview) so the deferred replay is independent of
r_flares and of whatever view later owns the eyeProj UBO / MVP push.
==================
*/
void RB_CaptureDeferredHud( const vec3_t origin, const vec3_t left, const vec3_t up, color4ub_t color ) {
	VectorCopy( origin, deferredHudOrigin );
	VectorCopy( left, deferredHudLeft );
	VectorCopy( up, deferredHudUp );
	deferredHudColor = color;
	deferredHudFogNum = tess.fogNum;

	Com_Memcpy( deferredHudEyeProj, vk_view_eyeproj, sizeof( deferredHudEyeProj ) );
	Com_Memcpy( deferredHudModelMatrix, backEnd.viewParms.world.modelMatrix, sizeof( deferredHudModelMatrix ) );

	backEnd.hudDeferred = qtrue;
}


/*
==================
RB_ReportHudRect

Each eye's NDC bounds of the HUD quad, so a density map carrying its pass can keep it
sharp; an eye with a corner behind it reports none.
==================
*/
static void RB_ReportHudRect( void ) {
	float rect[4];
	vec4_t eye, clip;
	vec3_t corner;
	int e, i, k;

	for ( e = 0; e < 2; e++ ) {
		rect[0] = rect[1] = 1e9f;
		rect[2] = rect[3] = -1e9f;
		for ( i = 0; i < 4; i++ ) {
			for ( k = 0; k < 3; k++ )
				corner[k] = deferredHudOrigin[k] + ( ( i & 1 ) ? 1 : -1 ) * deferredHudLeft[k] +
					( ( i & 2 ) ? 1 : -1 ) * deferredHudUp[k];
			R_TransformModelToClip( corner, deferredHudModelMatrix, deferredHudEyeProj[e], eye, clip );
			if ( clip[3] <= 0.001f )
				break;
			rect[0] = MIN( rect[0], clip[0] / clip[3] );
			rect[1] = MIN( rect[1], clip[1] / clip[3] );
			rect[2] = MAX( rect[2], clip[0] / clip[3] );
			rect[3] = MAX( rect[3], clip[1] / clip[3] );
		}
		if ( i == 4 )
			vk_foveation_keep_sharp( e, rect );
	}
}


/*
==================
RB_DrawDeferredHud

Replays the captured in-world HUD sprite at the 3D->2D boundary, AFTER
RB_RenderDeferredFlares has drawn the corona; draws once per frame across the
hook sites and runs independent of r_flares. In FBO mode it lands in the
post-scene pass (no depth attachment, so unoccluded); in direct mode
(r_fbo 0) in the still-open main pass, where DEPTH_RANGE_WEAPON reproduces the
original inline RF_DEPTHHACK draw. Either way it composites over the corona by
drawing after it.
==================
*/
void RB_DrawDeferredHud( void ) {
	const trRefEntity_t	*savedEntity;

	if ( !backEnd.hudDeferred )
		return;

	// checked before consuming so a screenmap pass can't suppress the real replay
	if ( vk.renderPassIndex == RENDER_PASS_SCREENMAP )
		return;

	// World-space quad needs the 3D camera; never draw once the 2D ortho is active
	// (mirrors the deferred corona's guard for the end-of-frame fallback site).
	if ( backEnd.projection2D )
		return;

	// consume: draw exactly once even though both post-scene hook sites call us
	backEnd.hudDeferred = qfalse;
	RB_ReportHudRect();

	savedEntity = backEnd.currentEntity;
	backEnd.currentEntity = &tr.worldEntity;
	backEnd.or = backEnd.viewParms.world;

	// Re-establish the captured main-view per-eye projections + mono modelview: by
	// now a later HUD-icon/2D view owns the eyeProj UBO and the MVP push.
	Com_Memcpy( vk_view_eyeproj, deferredHudEyeProj, sizeof( vk_view_eyeproj ) );
	VK_PushEyeProj();
	vk_update_mvp( deferredHudModelMatrix );

	RB_BeginSurface( tr.hudShader, deferredHudFogNum );
	// weapon depth range: no-op in the post-scene pass (no depth attachment),
	// load-bearing in direct mode so world geometry can't occlude the HUD quad
	tess.depthRange = DEPTH_RANGE_WEAPON;
	RB_AddQuadStamp( deferredHudOrigin, deferredHudLeft, deferredHudUp, deferredHudColor );
	RB_EndSurface();
	// reset so nothing after inherits the weapon range
	tess.depthRange = DEPTH_RANGE_NORMAL;

	// Restore MVP so the HUD camera can't leak into subsequent 2D drawing.
	vk_update_mvp( NULL );
	backEnd.currentEntity = savedEntity;
}
