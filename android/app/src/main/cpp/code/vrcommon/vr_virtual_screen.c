/*
 * vr_virtual_screen.c - where the virtual screen stands, and what the renderer and cursor need of it
 *
 * The screen is placed once per XR frame from the located views; the renderer
 * draws it that frame and the menu cursor hits it.
 */
#include "vr_virtual_screen.h"

#include <string.h>
#include <math.h>

#include "vr_clientinfo.h"
#include "vr_gameplay.h"
#include "common/xr_linear.h"

extern vr_clientinfo_t vr;
extern cvar_t *vr_virtualScreenMode;
extern cvar_t *vr_screenCurvature;

// The grid spans this many meters on a side, centered on the space origin
#define FLOOR_SIZE 30.0f

static vrScreenAnchor_t s_anchor;
static vrScreenGeometry_t s_screen;
static XrView s_views[2];
static uint32_t s_viewCount;
static qboolean s_reanchor;
static vrScreenPointer_t s_pointers[2];


static void VR_VirtualScreen_Place( qboolean visible, qboolean contextChanged )
{
	const XrPosef *left = &s_views[0].pose;
	const XrPosef *right = &s_views[s_viewCount > 1 ? 1 : 0].pose;
	const float head[3] = {
		( left->position.x + right->position.x ) * 0.5f,
		( left->position.y + right->position.y ) * 0.5f,
		( left->position.z + right->position.z ) * 0.5f,
	};
	const float orientation[4] = { left->orientation.x, left->orientation.y, left->orientation.z, left->orientation.w };
	const qboolean follow = vr_virtualScreenMode && vr_virtualScreenMode->integer == 1 && !vr.menuYawLocked;

	if ( visible && ( s_reanchor || ( contextChanged && !vr.menuYawLocked ) ) )
	{
		s_anchor.initialized = 0;
	}
	s_reanchor = qfalse;

	VR_ScreenUpdate( &s_anchor, &s_screen, head, orientation, visible, follow,
		vr_screenCurvature ? vr_screenCurvature->value : VR_SCREEN_REFERENCE_CURVATURE );

	if ( s_screen.visible && !vr.menuYawLocked )
	{
		vr.menuYaw = RAD2DEG( s_screen.yaw );
	}
}


void VR_VirtualScreen_Update( const XrView *views, uint32_t viewCount )
{
	// Read every frame: the edge is consumed on read and must track the state continuously
	const qboolean contextChanged = VR_Gameplay_VirtualScreenContextChanged();

	// The router names this frame's pointers after its input runs; a frame without input has none
	s_pointers[0].active = s_pointers[1].active = qfalse;

	if ( viewCount == 0 )
	{
		return;
	}
	s_viewCount = viewCount > 2 ? 2 : viewCount;
	memcpy( s_views, views, s_viewCount * sizeof( XrView ) );

	VR_VirtualScreen_Place( VR_Gameplay_ShouldRenderInVirtualScreen(), contextChanged );
}


void VR_VirtualScreen_Reanchor( void )
{
	s_reanchor = qtrue;
}


qboolean VR_VirtualScreen_GetDraw( vrScreenDraw_t *out )
{
	const XrVector3f one = { 1.0f, 1.0f, 1.0f };
	float c, s;
	int e;

	if ( !vr.virtual_screen || vr.weapon_zoomed || s_viewCount == 0 )
	{
		return qfalse;
	}
	// The screen came up during this frame's Com_Frame, after the placement ran
	if ( !s_screen.visible )
	{
		VR_VirtualScreen_Place( qtrue, qfalse );
	}

	for ( e = 0; e < 2; e++ )
	{
		const XrView *view = &s_views[e < (int)s_viewCount ? e : 0];
		XrMatrix4x4f projection, pose, eyeView, eyeProj;

		XrMatrix4x4f_CreateProjectionFov( &projection, GRAPHICS_VULKAN, view->fov, 0.05f, 100.0f );
		XrMatrix4x4f_CreateTranslationRotationScale( &pose, &view->pose.position, &view->pose.orientation, &one );
		XrMatrix4x4f_InvertRigidBody( &eyeView, &pose );
		XrMatrix4x4f_Multiply( &eyeProj, &projection, &eyeView );
		memcpy( out->eyeProj[e], eyeProj.m, sizeof( eyeProj.m ) );
	}

	VR_ScreenModelMatrix( &s_screen, 0, out->screenModel );
	VR_ScreenModelMatrix( &s_screen, 1, out->reflectModel );
	VR_ScreenShape( &out->unit, s_screen.curvature, 1.0f );

	// Aligned with the runtime's stage rather than the recentered yaw
	c = cosf( -vr.recenterYaw );
	s = sinf( -vr.recenterYaw );
	memset( out->floorModel, 0, sizeof( out->floorModel ) );
	out->floorModel[0] = c * FLOOR_SIZE;
	out->floorModel[2] = -s * FLOOR_SIZE;
	out->floorModel[5] = 1.0f;
	out->floorModel[8] = s * FLOOR_SIZE;
	out->floorModel[10] = c * FLOOR_SIZE;
	out->floorModel[15] = 1.0f;

	return qtrue;
}


qboolean VR_VirtualScreen_ShowPointer( int hand, const float origin[3], const float direction[3], int cursorX, int cursorY,
	qboolean blue, qboolean *onScreen )
{
	vrScreenPointer_t *pointer = &s_pointers[hand];
	const float u = cursorX / 640.0f, v = cursorY / 480.0f;
	float uv[2], left[3], right[3], length;
	int i;

	*onScreen = qfalse;
	if ( !s_screen.visible )
	{
		return qfalse;
	}
	*onScreen = VR_ScreenRay( &s_screen, origin, direction, uv ) ? qtrue : qfalse;
	memcpy( pointer->origin, origin, sizeof( pointer->origin ) );
	pointer->pool = qfalse;
	pointer->blue = blue;
	if ( *onScreen )
	{
		// The drawn ray ends on the cursor, which trails the aim by its smoothing
		VR_ScreenPoint( &s_screen, u, v, pointer->poolPoint );
		VR_ScreenPoint( &s_screen, u - 0.01f, v, left );
		VR_ScreenPoint( &s_screen, u + 0.01f, v, right );
		for ( i = 0; i < 3; i++ )
		{
			pointer->end[i] = pointer->poolPoint[i];
			pointer->poolSide[i] = right[i] - left[i];
		}
		length = sqrtf( pointer->poolSide[0] * pointer->poolSide[0] + pointer->poolSide[1] * pointer->poolSide[1] +
			pointer->poolSide[2] * pointer->poolSide[2] );
		if ( length >= 0.000001f )
		{
			for ( i = 0; i < 3; i++ )
			{
				pointer->poolSide[i] /= length;
			}
			pointer->pool = qtrue;
		}
	}
	else
	{
		const float reach = VR_ScreenReach( &s_screen, origin, direction );
		for ( i = 0; i < 3; i++ )
		{
			pointer->end[i] = origin[i] + direction[i] * reach;
		}
	}
	pointer->active = qtrue;
	return qtrue;
}


const vrScreenPointer_t *VR_VirtualScreen_Pointer( int hand )
{
	return s_pointers[hand].active ? &s_pointers[hand] : NULL;
}


float VR_VirtualScreen_Height( void )
{
	return s_screen.height;
}


void VR_VirtualScreen_Head( float head[3] )
{
	const XrVector3f *left = &s_views[0].pose.position;
	const XrVector3f *right = &s_views[s_viewCount > 1 ? 1 : 0].pose.position;

	head[0] = ( left->x + right->x ) * 0.5f;
	head[1] = ( left->y + right->y ) * 0.5f;
	head[2] = ( left->z + right->z ) * 0.5f;
}


qboolean VR_VirtualScreen_Hit( const float origin[3], const float direction[3], float *x, float *y )
{
	float uv[2];

	if ( !VR_ScreenRay( &s_screen, origin, direction, uv ) )
	{
		return qfalse;
	}
	*x = uv[0] * 640.0f;
	*y = uv[1] * 480.0f;
	return qtrue;
}

qboolean VR_VirtualScreen_EyeTangents( float *tanWidth, float *tanHeight )
{
	float w = 0, h = 0;
	uint32_t e;

	for ( e = 0; e < s_viewCount; e++ )
	{
		w += tanf( s_views[e].fov.angleRight ) - tanf( s_views[e].fov.angleLeft );
		h += tanf( s_views[e].fov.angleUp ) - tanf( s_views[e].fov.angleDown );
	}
	if ( !s_viewCount || !( w > 0 ) || !( h > 0 ) )
	{
		return qfalse;
	}
	*tanWidth = w / s_viewCount;
	*tanHeight = h / s_viewCount;
	return qtrue;
}
