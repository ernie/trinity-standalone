/*
 * vr_controller_models.c - the runtime's controller models, shown beside the virtual screen
 *
 * Fetched and posed once per XR frame while the screen is up; the renderer draws what it finds here.
 */
#include "vr_controller_models.h"

#include <stdlib.h>

#include <string.h>

#include "../qcommon/qcommon.h"
#include "vr_clientinfo.h"
#include "vr_float.h"
#include "vr_input_types.h"
#include "vr_meta_pose.h"
#include "vr_router.h"

extern vr_clientinfo_t vr;
extern cvar_t *vr_controllerModels;
extern XrSpace leftControllerGripSpace;
extern XrSpace rightControllerGripSpace;

static vkXRModels_t s_models;
static qboolean s_enabled;
static qboolean s_changed;
static qboolean s_busy;
static XrResult s_failure;
// Kept across sessions: the renderer tells one load from another by it
static unsigned s_serial;

// Meta's own models (XR_FB_render_model), for a runtime without the EXT pair: one model per hand, shown at the
// grip pose. The extension names no moving parts; the input nodes Meta's own loader animates are found by name
// and posed from the hand's input.
typedef struct {
	vkXRModel_t slot;
	XrRenderModelPropertiesFB properties;
	int map[VR_META_INPUT_COUNT];
	vrModelNodeState_t states[VR_META_INPUT_COUNT];
} metaModel_t;
static metaModel_t s_meta[2]; // left, right
static qboolean s_metaEnabled;
static qboolean s_metaLoaded; // the one fetch, on the first focused frame: earlier calls get XR_ERROR_CALL_ORDER_INVALID
static XrInstance s_metaInstance;
static XrSession s_metaSession;
static PFN_xrEnumerateRenderModelPathsFB s_metaPaths;
static PFN_xrGetRenderModelPropertiesFB s_metaProperties;
static PFN_xrLoadRenderModelFB s_metaLoad;
static const char *const s_metaPathNames[2] = { "/model_fb/controller/left", "/model_fb/controller/right" };


static void VR_ControllerModels_Log( const char *line )
{
	Com_Printf( "%s\n", line );
}


void *VR_ControllerModel_Alloc( size_t size )
{
	// Plain malloc: the models outlive the renderer's own memory, which every map load frees
	return malloc( size );
}


static void VR_MetaModels_Free( void )
{
	int hand;

	for ( hand = 0; hand < 2; hand++ )
	{
		if ( s_meta[hand].slot.model.block )
		{
			VR_ModelFree( &s_meta[hand].slot.model, free );
		}
	}
	memset( s_meta, 0, sizeof( s_meta ) );
}


// The input nodes follow a hand's sample; none leaves them at rest
static void VR_MetaModel_Pose( metaModel_t *m, const clXRHandInput_t *hand )
{
	vrMetaInput_t input;

	memset( &input, 0, sizeof( input ) );
	if ( hand )
	{
		input.buttonAX = ( hand->buttons & CL_XRI_PRIMARY_BUTTON ) != 0;
		input.buttonBY = ( hand->buttons & CL_XRI_SECONDARY_BUTTON ) != 0;
		input.menu = ( hand->buttons & CL_XRI_MENU_BUTTON ) != 0;
		input.trigger = hand->trigger;
		input.grip = hand->squeeze;
		input.stick[0] = hand->stick[0];
		input.stick[1] = hand->stick[1];
	}
	VR_MetaPoseInputs( &m->slot.model, m->map, &input, m->states );
}


// One hand's model: properties for its key, the file in two calls, then the shared parser
static void VR_MetaModel_Load( int hand, XrPath path )
{
	metaModel_t *m = &s_meta[hand];
	XrRenderModelCapabilitiesRequestFB capabilities;
	XrRenderModelLoadInfoFB loadInfo;
	XrRenderModelBufferFB buffer;
	XrResult result;
	unsigned char *glb = NULL;
	uint32_t glbSize = 0;
	qboolean parsed;
	int i;

	// Which glTF subsets we take; the parser reads the core format either way
	memset( &capabilities, 0, sizeof( capabilities ) );
	capabilities.type = XR_TYPE_RENDER_MODEL_CAPABILITIES_REQUEST_FB;
	capabilities.flags = XR_RENDER_MODEL_SUPPORTS_GLTF_2_0_SUBSET_1_BIT_FB | XR_RENDER_MODEL_SUPPORTS_GLTF_2_0_SUBSET_2_BIT_FB;
	memset( &m->properties, 0, sizeof( m->properties ) );
	m->properties.type = XR_TYPE_RENDER_MODEL_PROPERTIES_FB;
	m->properties.next = &capabilities;
	result = s_metaProperties( s_metaSession, path, &m->properties );
	m->properties.next = NULL;
	if ( result != XR_SUCCESS || m->properties.modelKey == XR_NULL_RENDER_MODEL_KEY_FB )
	{
		Com_Printf( S_COLOR_YELLOW "Meta controller model %s: no properties (%d)\n", s_metaPathNames[hand], (int)result );
		return;
	}
	memset( &loadInfo, 0, sizeof( loadInfo ) );
	loadInfo.type = XR_TYPE_RENDER_MODEL_LOAD_INFO_FB;
	loadInfo.modelKey = m->properties.modelKey;
	memset( &buffer, 0, sizeof( buffer ) );
	buffer.type = XR_TYPE_RENDER_MODEL_BUFFER_FB;
	result = s_metaLoad( s_metaSession, &loadInfo, &buffer );
	if ( result == XR_SUCCESS && ( !buffer.bufferCountOutput || buffer.bufferCountOutput > VK_XR_MODEL_MAX_BYTES ) )
	{
		result = XR_ERROR_SIZE_INSUFFICIENT;
	}
	if ( result == XR_SUCCESS )
	{
		glbSize = buffer.bufferCountOutput;
		glb = malloc( glbSize );
		buffer.bufferCapacityInput = glbSize;
		buffer.buffer = glb;
		result = glb ? s_metaLoad( s_metaSession, &loadInfo, &buffer ) : XR_ERROR_OUT_OF_MEMORY;
	}
	if ( result != XR_SUCCESS )
	{
		Com_Printf( S_COLOR_YELLOW "Meta controller model %s: the file could not be read (%d)\n", s_metaPathNames[hand], (int)result );
		free( glb );
		return;
	}
	parsed = VR_ModelParse( glb, glbSize, VR_ControllerModel_Alloc, free, &m->slot.model );
	free( glb );
	if ( !parsed )
	{
		Com_Printf( S_COLOR_YELLOW "Meta controller model %s: its %u-byte file is malformed or over %d triangles\n",
			s_metaPathNames[hand], (unsigned)glbSize, VR_MODEL_MAX_TRIANGLES );
		return;
	}
	// The battery gauge is a flat quad over the body that only the runtime's own level display makes sense of
	for ( i = 0; i < m->slot.model.nodeCount; i++ )
	{
		if ( strstr( m->slot.model.nodes[i].name, "batteryIndicatorQuad" ) )
		{
			m->slot.model.nodes[i].mesh = -1;
		}
	}
	VR_MetaBindInputs( &m->slot.model, m->map );
	m->slot.map = m->map;
	m->slot.states = m->states;
	m->slot.nodeCount = VR_META_INPUT_COUNT;
	VR_MetaModel_Pose( m, NULL );
	// The renderer caches assets by this id and the packed size; the path is the identity here
	Q_strncpyz( (char *)m->slot.cacheId, hand ? "meta-right" : "meta-left", sizeof( m->slot.cacheId ) );
	m->slot.serial = ++s_serial;
	Com_Printf( "Meta controller model %s: %s, %d triangles, glTF subset%s%s\n", s_metaPathNames[hand],
		m->properties.modelName, m->slot.model.triangleCount,
		( m->properties.flags & XR_RENDER_MODEL_SUPPORTS_GLTF_2_0_SUBSET_1_BIT_FB ) ? " 1" : "",
		( m->properties.flags & XR_RENDER_MODEL_SUPPORTS_GLTF_2_0_SUBSET_2_BIT_FB ) ? " 2" : "" );
}


static void VR_MetaModels_Init( XrInstance instance, XrSession session )
{
	s_metaInstance = instance;
	s_metaSession = session;
	s_metaLoaded = qfalse;
	if ( xrGetInstanceProcAddr( instance, "xrEnumerateRenderModelPathsFB", (PFN_xrVoidFunction *)&s_metaPaths ) != XR_SUCCESS ||
		xrGetInstanceProcAddr( instance, "xrGetRenderModelPropertiesFB", (PFN_xrVoidFunction *)&s_metaProperties ) != XR_SUCCESS ||
		xrGetInstanceProcAddr( instance, "xrLoadRenderModelFB", (PFN_xrVoidFunction *)&s_metaLoad ) != XR_SUCCESS )
	{
		Com_Printf( S_COLOR_YELLOW "Meta controller models: the runtime lists the extension without its functions\n" );
		s_metaLoaded = qtrue;
	}
}


static void VR_MetaModels_Load( void )
{
	const XrInstance instance = s_metaInstance;
	const XrSession session = s_metaSession;
	XrRenderModelPathInfoFB *paths;
	uint32_t count = 0, i;
	int hand;

	s_metaLoaded = qtrue;
	if ( s_metaPaths( session, 0, &count, NULL ) != XR_SUCCESS || !count )
	{
		Com_Printf( S_COLOR_YELLOW "Meta controller models: none offered\n" );
		return;
	}
	paths = calloc( count, sizeof( *paths ) );
	if ( !paths )
	{
		return;
	}
	for ( i = 0; i < count; i++ )
	{
		paths[i].type = XR_TYPE_RENDER_MODEL_PATH_INFO_FB;
	}
	if ( s_metaPaths( session, count, &count, paths ) == XR_SUCCESS )
	{
		for ( i = 0; i < count; i++ )
		{
			char name[XR_MAX_PATH_LENGTH];
			uint32_t length = 0;

			if ( xrPathToString( instance, paths[i].path, sizeof( name ), &length, name ) != XR_SUCCESS )
			{
				continue;
			}
			for ( hand = 0; hand < 2; hand++ )
			{
				if ( !strcmp( name, s_metaPathNames[hand] ) && !s_meta[hand].slot.model.block )
				{
					VR_MetaModel_Load( hand, paths[i].path );
				}
			}
		}
	}
	free( paths );
}


void VR_ControllerModels_Init( XrInstance instance, XrSession session, qboolean enabled, qboolean meta )
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
	s_metaEnabled = meta;
	if ( meta )
	{
		VR_MetaModels_Init( instance, session );
	}
}


void VR_ControllerModels_Shutdown( void )
{
	s_serial = s_models.serial > s_serial ? s_models.serial : s_serial;
	VK_XRModels_Shutdown( &s_models );
	VR_MetaModels_Free();
	s_enabled = s_changed = s_busy = s_metaEnabled = s_metaLoaded = qfalse;
	s_metaInstance = XR_NULL_HANDLE;
	s_metaSession = XR_NULL_HANDLE;
}


void VR_ControllerModels_Changed( void )
{
	s_changed = qtrue;
}


// Meta's models follow the grip pose, as the runtime's own hands do; buttons, triggers and stick follow the frame's input
static void VR_MetaModels_Update( XrSpace base, XrTime time, qboolean shown )
{
	const XrSpaceLocationFlags tracked = XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT |
		XR_SPACE_LOCATION_POSITION_TRACKED_BIT | XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT;
	const clXRHandInput_t *hands = VR_Router_Hands();
	int hand;

	if ( shown && !s_metaLoaded )
	{
		VR_MetaModels_Load();
		s_busy = qtrue;
	}
	for ( hand = 0; hand < 2; hand++ )
	{
		vkXRModel_t *slot = &s_meta[hand].slot;
		const XrSpace grip = hand ? rightControllerGripSpace : leftControllerGripSpace;
		XrSpaceLocation location;

		slot->drawable = 0;
		if ( !slot->model.block || !shown || grip == XR_NULL_HANDLE || base == XR_NULL_HANDLE || time <= 0 )
		{
			continue;
		}
		memset( &location, 0, sizeof( location ) );
		location.type = XR_TYPE_SPACE_LOCATION;
		if ( xrLocateSpace( grip, base, time, &location ) != XR_SUCCESS || ( location.locationFlags & tracked ) != tracked ||
			!VR_FloatsFinite( (const float *)&location.pose.position, 3 ) ||
			!VR_FloatsFinite( (const float *)&location.pose.orientation, 4 ) )
		{
			continue;
		}
		memcpy( slot->root.position, &location.pose.position, sizeof( slot->root.position ) );
		memcpy( slot->root.orientation, &location.pose.orientation, sizeof( slot->root.orientation ) );
		slot->drawable = 1;
		VR_MetaModel_Pose( &s_meta[hand], hands ? &hands[hand] : NULL );
	}
}


void VR_ControllerModels_Update( XrSpace base, XrTime time, qboolean focused )
{
	const qboolean wanted = vr.virtual_screen && vr_controllerModels && vr_controllerModels->integer;

	if ( s_metaEnabled )
	{
		s_busy = qfalse;
		VR_MetaModels_Update( base, time, wanted && focused );
		return;
	}
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
	if ( s_metaEnabled )
	{
		return slot >= 0 && slot < 2 && s_meta[slot].slot.model.block ? &s_meta[slot].slot : NULL;
	}
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
	if ( s_metaEnabled )
	{
		for ( slot = 0; slot < 2; slot++ )
		{
			if ( s_meta[slot].slot.model.block )
			{
				loaded++;
				tracked += s_meta[slot].slot.drawable;
				triangles += s_meta[slot].slot.model.triangleCount;
			}
		}
		Com_Printf( "Controller models: Meta's own, %d loaded (%d triangles), %d tracked%s\n", loaded, triangles, tracked,
			vr_controllerModels && vr_controllerModels->integer ? "" : "; turned off" );
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
