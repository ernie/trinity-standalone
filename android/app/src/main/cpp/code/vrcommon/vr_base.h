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

// Whether XR_META_vulkan_swapchain_create_info was enabled, so swapchain images can take extra Vulkan create flags.
VR_Bool VR_HasSwapchainCreateFlags( void );

// Whether XR_KHR_vulkan_swapchain_format_list was enabled, so a mutable swapchain can name its view formats.
VR_Bool VR_HasSwapchainFormatList( void );

void VR_Info_f( void );

#endif
