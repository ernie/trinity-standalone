/*
 * vr_controller_models.c - the runtime's controller models, shown beside the virtual screen
 *
 * Fetched and posed once per XR frame while the screen is up; the renderer draws what it finds here.
 */
#include "vr_controller_models.h"

#include <stdlib.h>

#include "../qcommon/qcommon.h"
#include "vr_clientinfo.h"

extern vr_clientinfo_t vr;
extern cvar_t *vr_controllerModels;

static vkXRModels_t s_models;
static qboolean s_enabled;
static qboolean s_changed;
static qboolean s_busy;
static XrResult s_failure;
// Kept across sessions: the renderer tells one load from another by it
static unsigned s_serial;


static void VR_ControllerModels_Log( const char *line )
{
	Com_Printf( "%s\n", line );
}


void *VR_ControllerModel_Alloc( size_t size )
{
	// Plain malloc: the models outlive the renderer's own memory, which every map load frees
	return malloc( size );
}


void VR_ControllerModels_Init( XrInstance instance, XrSession session, qboolean enabled )
{
	const XrResult result = VK_XRModels_Init( &s_models, instance, session, xrGetInstanceProcAddr, enabled,
		VR_ControllerModel_Alloc, free, VR_ControllerModels_Log );

	s_enabled = enabled && XR_SUCCEEDED( result );
	s_changed = s_busy = qfalse;
	s_failure = result;
	s_models.serial = s_serial;
	if ( XR_FAILED( result ) )
	{
		Com_Printf( S_COLOR_YELLOW "OpenXR controller models unavailable (%d)\n", (int)result );
	}
}


void VR_ControllerModels_Shutdown( void )
{
	s_serial = s_models.serial;
	VK_XRModels_Shutdown( &s_models );
	s_enabled = s_changed = s_busy = qfalse;
}


void VR_ControllerModels_Changed( void )
{
	s_changed = qtrue;
}


// Models load and are located only while the screen that shows them is up, so a controller waking
// mid-match costs nothing until the next menu
void VR_ControllerModels_Update( XrSpace base, XrTime time, qboolean focused )
{
	const qboolean wanted = vr.virtual_screen && vr_controllerModels && vr_controllerModels->integer;

	s_busy = wanted && s_changed;
	if ( s_busy )
	{
		s_changed = qfalse;
		VK_XRModels_Refresh( &s_models );
	}
	if ( VK_XRModels_Update( &s_models, base, time, wanted && focused ) )
	{
		s_busy = qtrue;
	}
}


qboolean VR_ControllerModels_Busy( void )
{
	return s_busy;
}


const vkXRModel_t *VR_ControllerModel( int slot )
{
	return slot >= 0 && slot < VK_XR_MODELS_MAX && s_models.models[slot].handle ? &s_models.models[slot] : NULL;
}


void VR_ControllerModels_Info( void )
{
	int slot, loaded = 0, tracked = 0, triangles = 0;

	if ( XR_FAILED( s_failure ) )
	{
		Com_Printf( "Controller models: unavailable (%d)\n", (int)s_failure );
		return;
	}
	if ( !s_enabled )
	{
		Com_Printf( "Controller models: not offered by the runtime\n" );
		return;
	}
	for ( slot = 0; slot < VK_XR_MODELS_MAX; slot++ )
	{
		if ( s_models.models[slot].handle )
		{
			loaded++;
			tracked += s_models.models[slot].drawable;
			triangles += s_models.models[slot].model.triangleCount;
		}
	}
	Com_Printf( "Controller models: %d loaded (%d triangles), %d tracked%s\n", loaded, triangles, tracked,
		vr_controllerModels && vr_controllerModels->integer ? "" : "; turned off" );
}
