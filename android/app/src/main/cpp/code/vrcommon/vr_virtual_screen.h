#ifndef __VR_VIRTUAL_SCREEN
#define __VR_VIRTUAL_SCREEN

#include "../qcommon/q_shared.h"
#include "vr_types.h"
#include "vr_screen_geometry.h"

// One frame's virtual screen, in world meters
typedef struct {
	float eyeProj[2][16];		// world to clip, per view
	float screenModel[16];
	float reflectModel[16];
	float floorModel[16];
	vrScreenGeometry_t unit;	// the scale-1 shape the mesh is built from
} vrScreenDraw_t;

// One hand's pointer on the screen, in world meters
typedef struct {
	qboolean active;
	qboolean pool;			// the ray is on the screen: its cursor is a pool of light there
	qboolean blue;			// drawn blue instead of red
	float origin[3], end[3];
	float poolPoint[3];		// the cursor on the screen's surface
	float poolSide[3];		// the screen's tangent there, unit length
} vrScreenPointer_t;

void VR_VirtualScreen_Update( const XrView *views, uint32_t viewCount );
void VR_VirtualScreen_Reanchor( void );
qboolean VR_VirtualScreen_GetDraw( vrScreenDraw_t *out );
qboolean VR_VirtualScreen_Hit( const float origin[3], const float direction[3], float *x, float *y );
// A ray from origin along direction, drawn this frame only: it ends on the cursor (640x480 menu coordinates)
// when it is on the screen and runs as long along its aim when it is not, blue instead of red when asked. Returns
// qfalse while no screen is up.
qboolean VR_VirtualScreen_ShowPointer( int hand, const float origin[3], const float direction[3], int cursorX, int cursorY,
	qboolean blue, qboolean *onScreen );
// NULL while the hand has none
const vrScreenPointer_t *VR_VirtualScreen_Pointer( int hand );
// The screen's height in meters and the point midway between the eyes
float VR_VirtualScreen_Height( void );
// Per-eye tangent spans of the located views, averaged; false until a frame has located them
qboolean VR_VirtualScreen_EyeTangents( float *tanWidth, float *tanHeight );
void VR_VirtualScreen_Head( float head[3] );

#endif
