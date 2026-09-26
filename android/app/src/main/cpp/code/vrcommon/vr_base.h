#ifndef __VR_BASE
#define __VR_BASE

#include "vr_types.h"

VR_Engine* VR_Init( void );
VR_Engine* VR_GetEngine( void );
void VR_Destroy( VR_Engine* engine );
void VR_PrepareForShutdown( void );

void VR_EnterVR( VR_Engine* engine );
void VR_LeaveVR( VR_Engine* engine );

// "none", "fixed" or "eyetracked": what vr_foveationCaps publishes to the UI
const char* VR_FoveationCapsString( void );

// Whether XR_VALVE_frame_controller_interaction was enabled on the instance.
VR_Bool VR_HasFrameControllers( void );
// Whether XR_BD_controller_interaction was enabled on the instance.
VR_Bool VR_HasPicoControllers( void );

// Whether XR_KHR_composition_layer_cylinder was enabled; the virtual screen is a flat quad otherwise.
VR_Bool VR_HasCylinderLayers( void );

void VR_Info_f( void );

#endif
