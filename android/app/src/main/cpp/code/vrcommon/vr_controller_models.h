#ifndef __VR_CONTROLLER_MODELS
#define __VR_CONTROLLER_MODELS

#include "../qcommon/q_shared.h"
#include "vr_types.h"
#include "vk_xr_models.h"

// enabled: the instance carries both render model extensions
void VR_ControllerModels_Init( XrInstance instance, XrSession session, qboolean enabled );
// Before the session is destroyed
void VR_ControllerModels_Shutdown( void );
// XrEventDataInteractionRenderModelsChangedEXT arrived
void VR_ControllerModels_Changed( void );
// Once per XR frame, after xrSyncActions, with the frame's space and display time
void VR_ControllerModels_Update( XrSpace base, XrTime time, qboolean focused );
// This frame's update fetched models, which stalls it
qboolean VR_ControllerModels_Busy( void );
// NULL for an empty slot
const vkXRModel_t *VR_ControllerModel( int slot );
// What the models were parsed with; anything hung on one comes from here
void *VR_ControllerModel_Alloc( size_t size );
void VR_ControllerModels_Info( void );

#endif
