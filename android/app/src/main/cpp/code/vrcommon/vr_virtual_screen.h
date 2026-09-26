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

void VR_VirtualScreen_Update( const XrView *views, uint32_t viewCount );
void VR_VirtualScreen_Reanchor( void );
qboolean VR_VirtualScreen_GetDraw( vrScreenDraw_t *out );
qboolean VR_VirtualScreen_Hit( const float origin[3], const float direction[3], float *x, float *y );

#endif
