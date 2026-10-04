/*
 * vr_vk_swapchains.h - Vulkan XR swapchain management
 *
 * VR layer swapchain operations for Vulkan. Creates and manages XrSwapchains
 * and provides access to VkImage handles. VkImageViews and VkFramebuffers
 * are created by the renderer (see VkXrResources in renderervk/vk.h).
 */

#ifndef __VR_VK_SWAPCHAINS
#define __VR_VK_SWAPCHAINS

#include "../qcommon/q_shared.h"
#include "../vrcommon/vr_types.h"
#include "../vrcommon/vr_swapchains.h"  // Common OpenXR utilities
#include "vr_vk_types.h"

// Common utilities are in vrcommon/vr_swapchains.h:
// - XrDesktopViewConfiguration enum
// - VR_GetSupersamplingFactor()
// - VR_GetRecommendedResolution()
// - VR_GetSupersampledResolution()
// - VR_GetBestViewConfiguration()
// - VR_GetViewConfigurationViews()
// - VR_GetSwapchainFormats()
// - VR_HasFormatInList()

// Create all XR swapchains for VR rendering
// Creates: color (2-layer), depth (2-layer)
// Returns NULL on failure
VR_SwapchainInfos* VR_VK_CreateSwapchains(XrInstance instance, XrSystemId systemId, XrSession session);

// Destroy all swapchains and free resources
void VR_VK_DestroySwapchains(VR_SwapchainInfos** swapchains);

// Acquire swapchain images for rendering
// Returns indices into the swapchain image arrays
void VR_VK_Swapchains_Acquire(VR_SwapchainInfos* swapchains, uint32_t* colorIndex, uint32_t* depthIndex);

// Release swapchain images after rendering
void VR_VK_Swapchains_Release(VR_SwapchainInfos* swapchains);

// Get swapchain info for renderer
// The renderer uses these to create VkImageViews and VkFramebuffers
const VR_VK_SwapchainInfo* VR_VK_GetColorSwapchain(const VR_SwapchainInfos* swapchains);
const VR_VK_SwapchainInfo* VR_VK_GetDepthSwapchain(const VR_SwapchainInfos* swapchains);

// Deferred swapchain recreation (for vid_restart safety)
// Call RequestRecreate during vid_restart to defer recreation to a safe point.
// Call HandlePendingRecreate at the start of each frame before xrBeginFrame.
void VR_VK_Swapchains_RequestRecreate(void);
// Called at each renderer restart: requests a recreate when the color swapchain no longer fits the r_fbo mode
void VR_VK_Swapchains_CheckMode(void);
qboolean VR_VK_Swapchains_HandlePendingRecreate(VR_Engine* engine);

#endif // __VR_VK_SWAPCHAINS
