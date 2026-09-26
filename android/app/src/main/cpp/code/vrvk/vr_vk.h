#ifndef __VR_VULKAN_H
#define __VR_VULKAN_H

#include <vulkan/vulkan.h>
#include "../vrcommon/vr_types.h"
#include "vr_vk_types.h"

// Get the Vulkan graphics extension name for OpenXR instance creation
const char* VR_VK_GetGraphicsExtensionName(void);

// Get Vulkan graphics requirements from OpenXR
// Must be called after VR_Init() creates the XR instance
XrResult VR_VK_GetGraphicsRequirements(XrInstance instance, XrSystemId systemId,
                                        VR_VK_GraphicsRequirements* requirements);

// Print graphics requirements debug info
void VR_VK_PrintGraphicsRequirements(const VR_VK_GraphicsRequirements* requirements);

// Vulkan state managed by VR layer for OpenXR integration
// This is created before renderervk initialization and passed to it
typedef struct {
    VkInstance instance;
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    VkQueue queue;
    uint32_t queueFamilyIndex;

    // Graphics requirements from OpenXR
    uint32_t minApiVersionSupported;
    uint32_t maxApiVersionSupported;

    // Device properties (cached for use by renderer)
    VkPhysicalDeviceProperties deviceProperties;
    VkPhysicalDeviceMemoryProperties memoryProperties;
    VkPhysicalDeviceFeatures deviceFeatures;

    // Multiview support (required for stereo rendering)
    VR_Bool multiviewSupported;
    uint32_t maxMultiviewViewCount;

    // VK_EXT_fragment_density_map (foveated rendering)
    VR_Bool fragmentDensityMapSupported;
    VR_Bool fragmentDensityMapNonSubsampled;   // regular images may sit in a density map render pass
    VR_Bool fragmentDensityMap2Supported;      // Meta recommends VK_EXT_fragment_density_map2 alongside
    VkExtent2D minFragmentDensityTexelSize;
    VkExtent2D maxFragmentDensityTexelSize;

    VR_Bool tilePropertiesSupported;  // VK_QCOM_tile_properties was available, so the bin size can be read back

    // Density map offsets slide the tiler's bins with the gaze; ended through render pass 2
    VR_Bool fragmentDensityMapOffsetSupported;
    const char* fragmentDensityMapOffsetExtension;  // the EXT name where offered, else the QCOM original
    VkExtent2D fragmentDensityOffsetGranularity;

    VR_Bool debugMarkersEnabled;   // VK_EXT_debug_marker was available and was requested
    VR_Bool imageFormatListEnabled;  // VK_KHR_image_format_list was available and was requested
} VR_VulkanState;

// Global VR Vulkan state
extern VR_VulkanState vr_vk;

// Initialization functions: called in sequence during VR startup
// These implement the XR_KHR_vulkan_enable2 workflow

// Check graphics requirements before creating Vulkan instance
XrResult VR_Vulkan_CheckRequirements(XrInstance xrInstance, XrSystemId systemId);

// Create VkInstance (runtime adds required extensions via xrCreateVulkanInstanceKHR)
XrResult VR_Vulkan_CreateInstance(XrInstance xrInstance, XrSystemId systemId);

// Get the physical device that OpenXR requires
XrResult VR_Vulkan_GetPhysicalDevice(XrInstance xrInstance, XrSystemId systemId);

// Create VkDevice with VK_KHR_multiview (runtime adds required extensions via xrCreateVulkanDeviceKHR)
XrResult VR_Vulkan_CreateDevice(XrInstance xrInstance, XrSystemId systemId);

// Cleanup
void VR_Vulkan_Shutdown(void);

// Helper to check if Vulkan VR is initialized
VR_Bool VR_Vulkan_IsInitialized(void);

// Minimal device info for renderer initialization (pull model)
// This is what the renderer needs from the VR layer's XR_KHR_vulkan_enable2 workflow
typedef struct {
    VkInstance instance;
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    VkQueue queue;
    uint32_t queueFamilyIndex;
    VR_Bool fragmentDensityMap;    // enabled, with non-subsampled images allowed in its render passes
    VR_Bool debugMarkers;          // VK_EXT_debug_marker enabled, so object names reach a loaded layer
    // Finest granularity the hardware reads; the renderer writes its map on this grid
    uint32_t minDensityTexelWidth;
    uint32_t minDensityTexelHeight;
    VR_Bool tileProperties;        // VK_QCOM_tile_properties enabled, so the bin size can be read back
    VR_Bool fragmentDensityMapOffset;  // offsets and VK_KHR_create_renderpass2 enabled
    uint32_t densityOffsetGranularityWidth;
    uint32_t densityOffsetGranularityHeight;
    VR_Bool imageFormatList;       // VK_KHR_image_format_list enabled, so mutable images can name their view formats
} VR_VulkanDeviceInfo;

// Get the XR-created Vulkan device for renderer initialization
// Returns NULL if VR Vulkan is not yet initialized
const VR_VulkanDeviceInfo* VR_Vulkan_GetDeviceInfo(void);

// Swapchain info for renderer initialization (pull model)
typedef struct {
    // Color (multiview stereo, arraySize=2)
    VkFormat colorFormat;
    uint32_t colorWidth, colorHeight, colorArraySize, colorImageCount;
    VkImage* colorImages;          // NOT owned: from OpenXR
    XrSwapchainUsageFlags colorUsage;

    // Depth (multiview stereo, arraySize=2)
    VkFormat depthFormat;
    uint32_t depthWidth, depthHeight, depthArraySize, depthImageCount;
    VkImage* depthImages;

    // Density map per color image (XR_FB_foveation_vulkan), NULL without foveation
    VkImage* foveationImages;      // NOT owned: from OpenXR
    uint32_t foveationWidth, foveationHeight;

    VR_Bool densityMapOffsetImages;  // color and depth images carry the density map offset create flag
} VR_VulkanSwapchainInfo;

// Whether swapchains are created with the density map offset flag (device offsets plus XR_META_vulkan_swapchain_create_info)
VR_Bool VR_Vulkan_SwapchainsTakeDensityMapOffsets(void);

// Get the XR swapchain info for renderer initialization
// Returns NULL if swapchains are not yet created
const VR_VulkanSwapchainInfo* VR_Vulkan_GetSwapchainInfo(void);

// XR Swapchain types are now defined in vr_vk_types.h:
// - VR_VK_SwapchainInfo - minimal VR layer swapchain info (XrSwapchain + VkImages)
// - VR_SwapchainInfos_s - concrete VR_SwapchainInfos for Vulkan
//
// VkImageViews, VkFramebuffers, virtual screen resources, etc. are owned by
// the renderer and stored in VkXrResources (see renderervk/vk.h)

// Swapchain format selection
VkFormat VR_Vulkan_SelectColorFormat(const int64_t* formats, uint32_t count);
VkFormat VR_Vulkan_SelectDepthFormat(const int64_t* formats, uint32_t count);

// Swapchain creation and management
// foveated needs XR_FB_foveation enabled; it asks for a density map per image
XrResult VR_Vulkan_CreateSwapchain(XrSession session, VkFormat format,
                                    uint32_t width, uint32_t height,
                                    uint32_t arraySize, XrSwapchainUsageFlags usage,
                                    XrBool32 foveated,
                                    XrSwapchain* swapchain);

// Create swapchain with format list (XR_KHR_vulkan_swapchain_format_list)
// This allows specifying which formats will be used for image views,
// helping the runtime avoid adding unnecessary usage flags like STORAGE_BIT
XrResult VR_Vulkan_CreateSwapchainWithFormatList(XrSession session, VkFormat format,
                                                  uint32_t width, uint32_t height,
                                                  uint32_t arraySize, XrSwapchainUsageFlags usage,
                                                  const VkFormat* viewFormats, uint32_t viewFormatCount,
                                                  XrBool32 foveated,
                                                  XrSwapchain* swapchain);

// The foveation out-params may be NULL; a swapchain created without foveation yields none
XrResult VR_Vulkan_GetSwapchainImages(XrSwapchain swapchain,
                                       VkImage** images, uint32_t* imageCount,
                                       VkImage** foveationImages,
                                       uint32_t* foveationWidth, uint32_t* foveationHeight);

// VkImageView/VkFramebuffer creation in renderer (see vk_create_xr_image_views in vk.c)

#endif // __VR_VULKAN_H
