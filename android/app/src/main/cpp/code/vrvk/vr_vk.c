/*
 * VR Vulkan Integration
 *
 * Implements XR_KHR_vulkan_enable2 workflow for OpenXR/Vulkan integration.
 * This module creates and manages Vulkan instance and device that OpenXR requires,
 * which are then passed to renderervk for use.
 */

// IMPORTANT: Vulkan headers and XR_USE_GRAPHICS_API_VULKAN must be defined
// BEFORE any OpenXR includes (including through vr_types.h via vr_vk.h)
#include <vulkan/vulkan.h>
#define XR_USE_GRAPHICS_API_VULKAN

#include "vr_vk.h"
#include "../vrcommon/vr_base.h"
#include "../vrcommon/vr_macros.h"
#include "vr_vk_debug.h"

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#if __ANDROID__
#include <android/log.h>
#endif

// Global VR Vulkan state
VR_VulkanState vr_vk;

static VR_Bool vr_vk_initialized = VR_FALSE;

// stdout/stderr never reach logcat on Android
static void VR_VK_LogLine(const char* line)
{
#if __ANDROID__
    __android_log_print(ANDROID_LOG_INFO, "VRVK", "%s", line);
#else
    fprintf(stderr, "[VRVK] %s\n", line);
#endif
}

// Function pointers for XR_KHR_vulkan_enable2
// Note: enable2 does NOT provide xrGetVulkanInstanceExtensionsKHR or xrGetVulkanDeviceExtensionsKHR
// Those are from the older XR_KHR_vulkan_enable extension. With enable2, the runtime adds
// required extensions via xrCreateVulkanInstanceKHR and xrCreateVulkanDeviceKHR.
static PFN_xrGetVulkanGraphicsRequirements2KHR xrGetVulkanGraphicsRequirements2KHR = NULL;
static PFN_xrGetVulkanGraphicsDevice2KHR xrGetVulkanGraphicsDevice2KHR = NULL;
static PFN_xrCreateVulkanInstanceKHR xrCreateVulkanInstanceKHR = NULL;
static PFN_xrCreateVulkanDeviceKHR xrCreateVulkanDeviceKHR = NULL;

// Vulkan 1.1 function pointers (needed for Android where we link against Vulkan 1.0)
static PFN_vkGetPhysicalDeviceFeatures2 pfn_vkGetPhysicalDeviceFeatures2 = NULL;
static PFN_vkGetPhysicalDeviceProperties2 pfn_vkGetPhysicalDeviceProperties2 = NULL;

// The NDK headers predate the EXT promotion
#define VR_EXT_FRAGMENT_DENSITY_MAP_OFFSET_EXTENSION_NAME "VK_EXT_fragment_density_map_offset"

// Forward declarations
static VR_Bool LoadXrVulkanFunctions(XrInstance xrInstance);
static void VR_Vulkan_QueryFragmentDensityMap(void);
static void VR_Vulkan_QueryFragmentDensityMapOffset(void);

// Get the Vulkan graphics extension name for OpenXR instance creation
const char* VR_VK_GetGraphicsExtensionName(void)
{
	return XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME;
}

// Get Vulkan graphics requirements from OpenXR
// This is a wrapper that populates VR_VK_GraphicsRequirements
XrResult VR_VK_GetGraphicsRequirements(XrInstance instance, XrSystemId systemId,
                                        VR_VK_GraphicsRequirements* requirements)
{
	// First ensure we have the function pointer
	if (!xrGetVulkanGraphicsRequirements2KHR) {
		if (!LoadXrVulkanFunctions(instance)) {
			return XR_ERROR_FUNCTION_UNSUPPORTED;
		}
	}

	requirements->requirements.type = XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN2_KHR;
	requirements->requirements.next = NULL;

	return xrGetVulkanGraphicsRequirements2KHR(instance, systemId, &requirements->requirements);
}

// Print graphics requirements debug info
void VR_VK_PrintGraphicsRequirements(const VR_VK_GraphicsRequirements* requirements)
{
	// Use XR version macros: XrVersion is encoded differently than VkVersion
	fprintf(stderr, "[OpenXR] Vulkan version requirements: [%d.%d.%d, %d.%d.%d]\n",
		XR_VERSION_MAJOR(requirements->requirements.minApiVersionSupported),
		XR_VERSION_MINOR(requirements->requirements.minApiVersionSupported),
		(int)XR_VERSION_PATCH(requirements->requirements.minApiVersionSupported),
		XR_VERSION_MAJOR(requirements->requirements.maxApiVersionSupported),
		XR_VERSION_MINOR(requirements->requirements.maxApiVersionSupported),
		(int)XR_VERSION_PATCH(requirements->requirements.maxApiVersionSupported));
}

// Load XR Vulkan extension function pointers for XR_KHR_vulkan_enable2
static VR_Bool LoadXrVulkanFunctions(XrInstance xrInstance)
{
    XrResult result;

    result = xrGetInstanceProcAddr(xrInstance, "xrGetVulkanGraphicsRequirements2KHR",
                                    (PFN_xrVoidFunction*)&xrGetVulkanGraphicsRequirements2KHR);
    if (XR_FAILED(result)) {
        fprintf(stderr, "[VRVK] Failed to get xrGetVulkanGraphicsRequirements2KHR\n");
        return VR_FALSE;
    }

    result = xrGetInstanceProcAddr(xrInstance, "xrGetVulkanGraphicsDevice2KHR",
                                    (PFN_xrVoidFunction*)&xrGetVulkanGraphicsDevice2KHR);
    if (XR_FAILED(result)) {
        fprintf(stderr, "[VRVK] Failed to get xrGetVulkanGraphicsDevice2KHR\n");
        return VR_FALSE;
    }

    result = xrGetInstanceProcAddr(xrInstance, "xrCreateVulkanInstanceKHR",
                                    (PFN_xrVoidFunction*)&xrCreateVulkanInstanceKHR);
    if (XR_FAILED(result)) {
        fprintf(stderr, "[VRVK] Failed to get xrCreateVulkanInstanceKHR\n");
        return VR_FALSE;
    }

    result = xrGetInstanceProcAddr(xrInstance, "xrCreateVulkanDeviceKHR",
                                    (PFN_xrVoidFunction*)&xrCreateVulkanDeviceKHR);
    if (XR_FAILED(result)) {
        fprintf(stderr, "[VRVK] Failed to get xrCreateVulkanDeviceKHR\n");
        return VR_FALSE;
    }

    return VR_TRUE;
}

VR_Bool VR_Vulkan_IsInitialized(void)
{
    return vr_vk_initialized;
}

const VR_VulkanDeviceInfo* VR_Vulkan_GetDeviceInfo(void)
{
    if (!vr_vk_initialized) {
        return NULL;
    }

    // Return pointer to static info populated from vr_vk globals
    static VR_VulkanDeviceInfo info;
    info.instance = vr_vk.instance;
    info.physicalDevice = vr_vk.physicalDevice;
    info.device = vr_vk.device;
    info.queue = vr_vk.queue;
    info.queueFamilyIndex = vr_vk.queueFamilyIndex;
    info.fragmentDensityMap = vr_vk.fragmentDensityMapSupported && vr_vk.fragmentDensityMapNonSubsampled;
    info.debugMarkers = vr_vk.debugMarkersEnabled;
    info.minDensityTexelWidth = vr_vk.minFragmentDensityTexelSize.width;
    info.minDensityTexelHeight = vr_vk.minFragmentDensityTexelSize.height;
    info.tileProperties = vr_vk.tilePropertiesSupported;
    info.fragmentDensityMapOffset = vr_vk.fragmentDensityMapOffsetSupported;
    info.densityOffsetGranularityWidth = vr_vk.fragmentDensityOffsetGranularity.width;
    info.densityOffsetGranularityHeight = vr_vk.fragmentDensityOffsetGranularity.height;
    info.imageFormatList = vr_vk.imageFormatListEnabled;
    return &info;
}

const VR_VulkanSwapchainInfo* VR_Vulkan_GetSwapchainInfo(void)
{
    VR_Engine* engine = VR_GetEngine();
    if (!engine || !engine->appState.Renderer.Swapchains) {
        return NULL;
    }

    VR_SwapchainInfos* swapchains = engine->appState.Renderer.Swapchains;

    // Return pointer to static info populated from swapchain data
    static VR_VulkanSwapchainInfo info;

    // Color swapchain
    info.colorFormat = swapchains->color.format;
    info.colorWidth = swapchains->color.width;
    info.colorHeight = swapchains->color.height;
    info.colorArraySize = swapchains->color.arraySize;
    info.colorImageCount = swapchains->color.imageCount;
    info.colorImages = swapchains->color.images;
    info.colorUsage = swapchains->color.usage;

    // Depth swapchain
    info.depthFormat = swapchains->depth.format;
    info.depthWidth = swapchains->depth.width;
    info.depthHeight = swapchains->depth.height;
    info.depthArraySize = swapchains->depth.arraySize;
    info.depthImageCount = swapchains->depth.imageCount;
    info.depthImages = swapchains->depth.images;

    info.foveationImages = swapchains->color.foveationImages;
    info.foveationWidth = swapchains->color.foveationWidth;
    info.foveationHeight = swapchains->color.foveationHeight;
    info.densityMapOffsetImages = VR_Vulkan_SwapchainsTakeDensityMapOffsets();

    return &info;
}

XrResult VR_Vulkan_CheckRequirements(XrInstance xrInstance, XrSystemId systemId)
{
    if (!LoadXrVulkanFunctions(xrInstance)) {
        return XR_ERROR_FUNCTION_UNSUPPORTED;
    }

    XrGraphicsRequirementsVulkan2KHR requirements = {
        .type = XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN2_KHR,
        .next = NULL,
    };

    XrResult result = xrGetVulkanGraphicsRequirements2KHR(xrInstance, systemId, &requirements);
    if (XR_FAILED(result)) {
        fprintf(stderr, "[VRVK] Failed to get graphics requirements: %s\n",
                GetXRErrorString(result));
        return result;
    }

    vr_vk.minApiVersionSupported = requirements.minApiVersionSupported;
    vr_vk.maxApiVersionSupported = requirements.maxApiVersionSupported;

    fprintf(stdout, "[VRVK] Graphics Requirements:\n");
    fprintf(stdout, "  Min Vulkan API: %d.%d.%d\n",
            XR_VERSION_MAJOR(requirements.minApiVersionSupported),
            XR_VERSION_MINOR(requirements.minApiVersionSupported),
            XR_VERSION_PATCH(requirements.minApiVersionSupported));
    fprintf(stdout, "  Max Vulkan API: %d.%d.%d\n",
            XR_VERSION_MAJOR(requirements.maxApiVersionSupported),
            XR_VERSION_MINOR(requirements.maxApiVersionSupported),
            XR_VERSION_PATCH(requirements.maxApiVersionSupported));

    // We require Vulkan 1.1 for multiview: use XR version encoding for comparison
    XrVersion ourVersion = XR_MAKE_VERSION(1, 1, 0);

    // If runtime reports 0 for min/max, it means any version is acceptable
    if (requirements.minApiVersionSupported != 0 && ourVersion < requirements.minApiVersionSupported) {
        fprintf(stderr, "[VRVK] Vulkan 1.1 required but runtime requires higher version\n");
        return XR_ERROR_GRAPHICS_REQUIREMENTS_CALL_MISSING;
    }
    if (requirements.maxApiVersionSupported != 0 && ourVersion > requirements.maxApiVersionSupported) {
        fprintf(stderr, "[VRVK] Warning: Our Vulkan 1.1 exceeds runtime's max version\n");
        // Continue anyway, usually works
    }

    return XR_SUCCESS;
}

// Note: VR_Vulkan_GetInstanceExtensions() removed: not needed with XR_KHR_vulkan_enable2
// The enable2 extension uses xrCreateVulkanInstanceKHR which automatically adds required extensions

XrResult VR_Vulkan_CreateInstance(XrInstance xrInstance, XrSystemId systemId)
{
    // With XR_KHR_vulkan_enable2, we only need to provide our own extensions.
    // The runtime will add any additional required extensions during xrCreateVulkanInstanceKHR.

    // Build extension list dynamically
    const char* extensions[4];  // Max possible extensions
    uint32_t extensionCount = 0;

    // VK_EXT_debug_marker requires this; the loader implements it, so its presence does not prove a layer is loaded
    qboolean debugReportAvailable = qfalse;
    {
        uint32_t availCount = 0;
        vkEnumerateInstanceExtensionProperties(NULL, &availCount, NULL);
        if (availCount > 0) {
            VkExtensionProperties* avail = (VkExtensionProperties*)malloc(
                availCount * sizeof(VkExtensionProperties));
            vkEnumerateInstanceExtensionProperties(NULL, &availCount, avail);
            for (uint32_t i = 0; i < availCount; i++) {
                if (strcmp(avail[i].extensionName, VK_EXT_DEBUG_REPORT_EXTENSION_NAME) == 0) {
                    debugReportAvailable = qtrue;
                    break;
                }
            }
            free(avail);
        }
        if (debugReportAvailable) {
            extensions[extensionCount++] = VK_EXT_DEBUG_REPORT_EXTENSION_NAME;
        }
        fprintf(stdout, "[VRVK] VK_EXT_debug_report %s\n",
            debugReportAvailable ? "available, object names enabled" : "absent, objects will print as bare handles");
    }

    // Application info
    VkApplicationInfo appInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext = NULL,
        .pApplicationName = "Trinity",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "Trinity Standalone",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_1,
    };

    // Instance create info
    VkInstanceCreateInfo instanceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = NULL,
        .enabledExtensionCount = extensionCount,
        .ppEnabledExtensionNames = extensionCount > 0 ? extensions : NULL,
    };

    // No layer named here: the loader injects one via gpu_debug_layers, and naming it would request it twice

    // Use xrCreateVulkanInstanceKHR to create instance with XR-required extensions
    // The runtime adds any extensions it needs automatically
    XrVulkanInstanceCreateInfoKHR xrCreateInfo = {
        .type = XR_TYPE_VULKAN_INSTANCE_CREATE_INFO_KHR,
        .next = NULL,
        .systemId = systemId,
        .createFlags = 0,
        .pfnGetInstanceProcAddr = vkGetInstanceProcAddr,
        .vulkanCreateInfo = &instanceCreateInfo,
        .vulkanAllocator = NULL,
    };

    VkResult vkResult;
    XrResult result = xrCreateVulkanInstanceKHR(xrInstance, &xrCreateInfo, &vr_vk.instance, &vkResult);

    if (XR_FAILED(result)) {
        fprintf(stderr, "[VRVK] xrCreateVulkanInstanceKHR failed: %s\n",
                GetXRErrorString(result));
        return result;
    }

    if (vkResult != VK_SUCCESS) {
        fprintf(stderr, "[VRVK] vkCreateInstance failed: %d\n", vkResult);
        return XR_ERROR_INITIALIZATION_FAILED;
    }

    // Load Vulkan 1.1 function pointers (needed for Android where we link against Vulkan 1.0)
    pfn_vkGetPhysicalDeviceFeatures2 = (PFN_vkGetPhysicalDeviceFeatures2)
        vkGetInstanceProcAddr(vr_vk.instance, "vkGetPhysicalDeviceFeatures2");
    pfn_vkGetPhysicalDeviceProperties2 = (PFN_vkGetPhysicalDeviceProperties2)
        vkGetInstanceProcAddr(vr_vk.instance, "vkGetPhysicalDeviceProperties2");

    if (!pfn_vkGetPhysicalDeviceFeatures2 || !pfn_vkGetPhysicalDeviceProperties2) {
        fprintf(stderr, "[VRVK] Failed to load Vulkan 1.1 functions\n");
        return XR_ERROR_INITIALIZATION_FAILED;
    }

    fprintf(stdout, "[VRVK] VkInstance created successfully\n");
    return XR_SUCCESS;
}

XrResult VR_Vulkan_GetPhysicalDevice(XrInstance xrInstance, XrSystemId systemId)
{
    XrVulkanGraphicsDeviceGetInfoKHR deviceGetInfo = {
        .type = XR_TYPE_VULKAN_GRAPHICS_DEVICE_GET_INFO_KHR,
        .next = NULL,
        .systemId = systemId,
        .vulkanInstance = vr_vk.instance,
    };

    XrResult result = xrGetVulkanGraphicsDevice2KHR(xrInstance, &deviceGetInfo,
                                                     &vr_vk.physicalDevice);
    if (XR_FAILED(result)) {
        fprintf(stderr, "[VRVK] Failed to get XR physical device: %s\n",
                GetXRErrorString(result));
        return result;
    }

    // Cache device properties
    vkGetPhysicalDeviceProperties(vr_vk.physicalDevice, &vr_vk.deviceProperties);
    vkGetPhysicalDeviceMemoryProperties(vr_vk.physicalDevice, &vr_vk.memoryProperties);
    vkGetPhysicalDeviceFeatures(vr_vk.physicalDevice, &vr_vk.deviceFeatures);

    fprintf(stdout, "[VRVK] XR Physical Device: %s\n", vr_vk.deviceProperties.deviceName);
    fprintf(stdout, "  Driver Version: %d.%d.%d\n",
            VK_API_VERSION_MAJOR(vr_vk.deviceProperties.driverVersion),
            VK_API_VERSION_MINOR(vr_vk.deviceProperties.driverVersion),
            VK_API_VERSION_PATCH(vr_vk.deviceProperties.driverVersion));
    fprintf(stdout, "  API Version: %d.%d.%d\n",
            VK_API_VERSION_MAJOR(vr_vk.deviceProperties.apiVersion),
            VK_API_VERSION_MINOR(vr_vk.deviceProperties.apiVersion),
            VK_API_VERSION_PATCH(vr_vk.deviceProperties.apiVersion));

    // Check multiview support
    VkPhysicalDeviceMultiviewFeatures multiviewFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES,
        .pNext = NULL,
    };
    VkPhysicalDeviceFeatures2 features2 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &multiviewFeatures,
    };
    pfn_vkGetPhysicalDeviceFeatures2(vr_vk.physicalDevice, &features2);

    vr_vk.multiviewSupported = multiviewFeatures.multiview ? VR_TRUE : VR_FALSE;

    if (!vr_vk.multiviewSupported) {
        fprintf(stderr, "[VRVK] ERROR: Device does not support multiview!\n");
        return XR_ERROR_GRAPHICS_DEVICE_INVALID;
    }

    // Get max multiview view count
    VkPhysicalDeviceMultiviewProperties multiviewProps = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_PROPERTIES,
        .pNext = NULL,
    };
    VkPhysicalDeviceProperties2 props2 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = &multiviewProps,
    };
    pfn_vkGetPhysicalDeviceProperties2(vr_vk.physicalDevice, &props2);

    vr_vk.maxMultiviewViewCount = multiviewProps.maxMultiviewViewCount;
    fprintf(stdout, "  Multiview: supported (max views: %d)\n", vr_vk.maxMultiviewViewCount);

    VR_Vulkan_QueryFragmentDensityMap();

    return XR_SUCCESS;
}

static VR_Bool VR_Vulkan_HasDeviceExtension(const char* name)
{
    uint32_t count = 0;
    VkExtensionProperties* props;
    VR_Bool found = VR_FALSE;
    uint32_t i;

    if (vkEnumerateDeviceExtensionProperties(vr_vk.physicalDevice, NULL, &count, NULL) != VK_SUCCESS || count == 0) {
        return VR_FALSE;
    }
    props = (VkExtensionProperties*)malloc(sizeof(VkExtensionProperties) * count);
    if (!props) {
        return VR_FALSE;
    }
    if (vkEnumerateDeviceExtensionProperties(vr_vk.physicalDevice, NULL, &count, props) == VK_SUCCESS) {
        for (i = 0; i < count; i++) {
            if (strcmp(props[i].extensionName, name) == 0) {
                found = VR_TRUE;
                break;
            }
        }
    }
    free(props);
    return found;
}

/*
==================
VR_Vulkan_QueryFragmentDensityMap

Skipped when the runtime has no foveation, leaving such a device untouched.
==================
*/
static void VR_Vulkan_QueryFragmentDensityMap(void)
{
    const VR_Engine* engine = VR_GetEngine();
    VkPhysicalDeviceFragmentDensityMapFeaturesEXT fdmFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_FEATURES_EXT,
        .pNext = NULL,
    };
    VkPhysicalDeviceFeatures2 features2 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &fdmFeatures,
    };
    VkPhysicalDeviceFragmentDensityMapPropertiesEXT fdmProps = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_PROPERTIES_EXT,
        .pNext = NULL,
    };
    VkPhysicalDeviceProperties2 props2 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = &fdmProps,
    };

    vr_vk.fragmentDensityMapSupported = VR_FALSE;
    vr_vk.fragmentDensityMapNonSubsampled = VR_FALSE;
    vr_vk.tilePropertiesSupported = VR_FALSE;

    if (!engine || !engine->foveation.ExtFoveation || !engine->foveation.ExtVulkan) {
        fprintf(stdout, "  Fragment density map: not needed (runtime has no foveation)\n");
        return;
    }
    if (!VR_Vulkan_HasDeviceExtension(VK_EXT_FRAGMENT_DENSITY_MAP_EXTENSION_NAME)) {
        fprintf(stdout, "  Fragment density map: device lacks %s\n", VK_EXT_FRAGMENT_DENSITY_MAP_EXTENSION_NAME);
        return;
    }

    pfn_vkGetPhysicalDeviceFeatures2(vr_vk.physicalDevice, &features2);
    pfn_vkGetPhysicalDeviceProperties2(vr_vk.physicalDevice, &props2);

    vr_vk.fragmentDensityMapSupported = fdmFeatures.fragmentDensityMap ? VR_TRUE : VR_FALSE;
    vr_vk.fragmentDensityMapNonSubsampled = fdmFeatures.fragmentDensityMapNonSubsampledImages ? VR_TRUE : VR_FALSE;
    vr_vk.fragmentDensityMap2Supported = VR_Vulkan_HasDeviceExtension(VK_EXT_FRAGMENT_DENSITY_MAP_2_EXTENSION_NAME);
    vr_vk.minFragmentDensityTexelSize = fdmProps.minFragmentDensityTexelSize;
    vr_vk.maxFragmentDensityTexelSize = fdmProps.maxFragmentDensityTexelSize;

    VR_VK_LogLine(va("Fragment density map: %s (dynamic %s, non-subsampled images %s, density map 2 %s, texel size %ux%u..%ux%u)",
        vr_vk.fragmentDensityMapSupported ? "supported" : "unsupported",
        fdmFeatures.fragmentDensityMapDynamic ? "yes" : "no",
        vr_vk.fragmentDensityMapNonSubsampled ? "yes" : "no",
        vr_vk.fragmentDensityMap2Supported ? "yes" : "no",
        fdmProps.minFragmentDensityTexelSize.width, fdmProps.minFragmentDensityTexelSize.height,
        fdmProps.maxFragmentDensityTexelSize.width, fdmProps.maxFragmentDensityTexelSize.height));

    // The tiler applies a density map one bin at a time, so the bin is the map's real resolution
    if (vr_vk.fragmentDensityMapSupported && VR_Vulkan_HasDeviceExtension(VK_QCOM_TILE_PROPERTIES_EXTENSION_NAME)) {
        VkPhysicalDeviceTilePropertiesFeaturesQCOM tileFeatures = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TILE_PROPERTIES_FEATURES_QCOM,
            .pNext = NULL,
        };
        VkPhysicalDeviceFeatures2 tileFeatures2 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
            .pNext = &tileFeatures,
        };

        pfn_vkGetPhysicalDeviceFeatures2(vr_vk.physicalDevice, &tileFeatures2);
        vr_vk.tilePropertiesSupported = tileFeatures.tileProperties ? VR_TRUE : VR_FALSE;
        VR_VK_LogLine(va("Tile properties: %s",
            vr_vk.tilePropertiesSupported ? "supported, bin size will be reported" : "feature off"));
    } else if (vr_vk.fragmentDensityMapSupported) {
        VR_VK_LogLine("Tile properties: absent");
    }

    VR_Vulkan_QueryFragmentDensityMapOffset();
}

/*
==================
VR_Vulkan_QueryFragmentDensityMapOffset

The EXT promoted the QCOM extension without changing its structures, so the QCOM
types, whose names the NDK headers carry, describe either.
==================
*/
static void VR_Vulkan_QueryFragmentDensityMapOffset(void)
{
    VkPhysicalDeviceFragmentDensityMapOffsetFeaturesQCOM offsetFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_OFFSET_FEATURES_QCOM,
        .pNext = NULL,
    };
    VkPhysicalDeviceFeatures2 features2 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &offsetFeatures,
    };
    VkPhysicalDeviceFragmentDensityMapOffsetPropertiesQCOM offsetProps = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_OFFSET_PROPERTIES_QCOM,
        .pNext = NULL,
    };
    VkPhysicalDeviceProperties2 props2 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = &offsetProps,
    };

    vr_vk.fragmentDensityMapOffsetSupported = VR_FALSE;
    vr_vk.fragmentDensityMapOffsetExtension = NULL;

    if (!vr_vk.fragmentDensityMapSupported || !vr_vk.fragmentDensityMapNonSubsampled) {
        return;
    }
    if (VR_Vulkan_HasDeviceExtension(VR_EXT_FRAGMENT_DENSITY_MAP_OFFSET_EXTENSION_NAME)) {
        vr_vk.fragmentDensityMapOffsetExtension = VR_EXT_FRAGMENT_DENSITY_MAP_OFFSET_EXTENSION_NAME;
    } else if (VR_Vulkan_HasDeviceExtension(VK_QCOM_FRAGMENT_DENSITY_MAP_OFFSET_EXTENSION_NAME)) {
        vr_vk.fragmentDensityMapOffsetExtension = VK_QCOM_FRAGMENT_DENSITY_MAP_OFFSET_EXTENSION_NAME;
    }
    if (!vr_vk.fragmentDensityMapOffsetExtension ||
        !VR_Vulkan_HasDeviceExtension(VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME)) {
        VR_VK_LogLine("Fragment density map offsets: absent");
        return;
    }

    pfn_vkGetPhysicalDeviceFeatures2(vr_vk.physicalDevice, &features2);
    pfn_vkGetPhysicalDeviceProperties2(vr_vk.physicalDevice, &props2);

    vr_vk.fragmentDensityMapOffsetSupported = offsetFeatures.fragmentDensityMapOffset ? VR_TRUE : VR_FALSE;
    vr_vk.fragmentDensityOffsetGranularity = offsetProps.fragmentDensityOffsetGranularity;
    VR_VK_LogLine(va("Fragment density map offsets: %s (%s, granularity %ux%u)",
        vr_vk.fragmentDensityMapOffsetSupported ? "supported" : "feature off",
        vr_vk.fragmentDensityMapOffsetExtension,
        vr_vk.fragmentDensityOffsetGranularity.width, vr_vk.fragmentDensityOffsetGranularity.height));
}

XrResult VR_Vulkan_CreateDevice(XrInstance xrInstance, XrSystemId systemId)
{
    // Our required extensions: runtime will add any additional ones via xrCreateVulkanDeviceKHR
    const char* extensions[10] = {
        VK_KHR_MULTIVIEW_EXTENSION_NAME,  // For stereo rendering
    };
    uint32_t extensionCount = 1;

    // Meta recommends density map 2 alongside: the map is read with less latency on Adreno
    if (vr_vk.fragmentDensityMapSupported) {
        extensions[extensionCount++] = VK_EXT_FRAGMENT_DENSITY_MAP_EXTENSION_NAME;
        if (vr_vk.fragmentDensityMap2Supported) {
            extensions[extensionCount++] = VK_EXT_FRAGMENT_DENSITY_MAP_2_EXTENSION_NAME;
        }
        if (vr_vk.tilePropertiesSupported) {
            extensions[extensionCount++] = VK_QCOM_TILE_PROPERTIES_EXTENSION_NAME;
        }
        if (vr_vk.fragmentDensityMapOffsetSupported) {
            extensions[extensionCount++] = vr_vk.fragmentDensityMapOffsetExtension;
            extensions[extensionCount++] = VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME;
        }
    }

    // Only a loaded validation layer offers this; it is what makes SET_OBJECT_NAME reach the layer
    vr_vk.debugMarkersEnabled = VR_Vulkan_HasDeviceExtension(VK_EXT_DEBUG_MARKER_EXTENSION_NAME);
    if (vr_vk.debugMarkersEnabled) {
        extensions[extensionCount++] = VK_EXT_DEBUG_MARKER_EXTENSION_NAME;
    }
    __android_log_print(ANDROID_LOG_INFO, "VRVK", "VK_EXT_debug_marker %s",
        vr_vk.debugMarkersEnabled ? "enabled, Vulkan objects will be named" : "absent, objects print as bare handles");

    // A mutable image that names its view formats keeps the driver's framebuffer compression
    vr_vk.imageFormatListEnabled = VR_Vulkan_HasDeviceExtension(VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME);
    if (vr_vk.imageFormatListEnabled) {
        extensions[extensionCount++] = VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME;
    }
    VR_VK_LogLine(va("VK_KHR_image_format_list %s",
        vr_vk.imageFormatListEnabled ? "enabled" : "absent, the virtual screen image stays uncompressed"));

    // Find graphics queue family
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(vr_vk.physicalDevice, &queueFamilyCount, NULL);

    VkQueueFamilyProperties* queueFamilies = (VkQueueFamilyProperties*)malloc(
        sizeof(VkQueueFamilyProperties) * queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(vr_vk.physicalDevice, &queueFamilyCount, queueFamilies);

    vr_vk.queueFamilyIndex = UINT32_MAX;
    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            vr_vk.queueFamilyIndex = i;
            break;
        }
    }
    free(queueFamilies);

    if (vr_vk.queueFamilyIndex == UINT32_MAX) {
        fprintf(stderr, "[VRVK] No graphics queue family found\n");
        return XR_ERROR_GRAPHICS_DEVICE_INVALID;
    }

    fprintf(stdout, "[VRVK] Using queue family %d for graphics\n", vr_vk.queueFamilyIndex);

    // Enable multiview feature
    VkPhysicalDeviceMultiviewFeatures multiviewFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES,
        .pNext = NULL,
        .multiview = VK_TRUE,
        .multiviewGeometryShader = VK_FALSE,
        .multiviewTessellationShader = VK_FALSE,
    };

#ifdef _WIN32
    // Enable maintenance4 feature (relaxes push constant validation): PCVR only
    VkPhysicalDeviceMaintenance4Features maintenance4Features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_4_FEATURES,
        .pNext = NULL,
        .maintenance4 = VK_TRUE,
    };
    multiviewFeatures.pNext = &maintenance4Features;  // Chain maintenance4 on Windows
#endif

    VkPhysicalDeviceFragmentDensityMapFeaturesEXT fdmFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_FEATURES_EXT,
        .pNext = NULL,
    };
    if (vr_vk.fragmentDensityMapSupported) {
        fdmFeatures.pNext = multiviewFeatures.pNext;
        multiviewFeatures.pNext = &fdmFeatures;
    }
    VkPhysicalDeviceFragmentDensityMapOffsetFeaturesQCOM offsetFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_OFFSET_FEATURES_QCOM,
        .pNext = NULL,
    };
    if (vr_vk.fragmentDensityMapOffsetSupported) {
        offsetFeatures.pNext = multiviewFeatures.pNext;
        multiviewFeatures.pNext = &offsetFeatures;
    }
    VkPhysicalDeviceTilePropertiesFeaturesQCOM tileFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TILE_PROPERTIES_FEATURES_QCOM,
        .pNext = NULL,
    };
    if (vr_vk.fragmentDensityMapSupported && vr_vk.tilePropertiesSupported) {
        tileFeatures.pNext = multiviewFeatures.pNext;
        multiviewFeatures.pNext = &tileFeatures;
    }

    VkPhysicalDeviceFeatures2 features2 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &multiviewFeatures,
    };
    // Get all supported features
    pfn_vkGetPhysicalDeviceFeatures2(vr_vk.physicalDevice, &features2);
    // Ensure required features are enabled after querying
    multiviewFeatures.multiview = VK_TRUE;
#ifdef _WIN32
    maintenance4Features.maintenance4 = VK_TRUE;
#endif
    if (vr_vk.fragmentDensityMapSupported) {
        // Dynamic (read at submit) is not needed: the map is final before the render pass begins
        fdmFeatures.fragmentDensityMap = VK_TRUE;
        fdmFeatures.fragmentDensityMapDynamic = VK_FALSE;
        fdmFeatures.fragmentDensityMapNonSubsampledImages = vr_vk.fragmentDensityMapNonSubsampled ? VK_TRUE : VK_FALSE;
    }
    if (vr_vk.fragmentDensityMapOffsetSupported) {
        offsetFeatures.fragmentDensityMapOffset = VK_TRUE;
    }
    if (vr_vk.fragmentDensityMapSupported && vr_vk.tilePropertiesSupported) {
        tileFeatures.tileProperties = VK_TRUE;
    }

    // Queue create info
    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .queueFamilyIndex = vr_vk.queueFamilyIndex,
        .queueCount = 1,
        .pQueuePriorities = &queuePriority,
    };

    // Device create info
    VkDeviceCreateInfo deviceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &features2,
        .flags = 0,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queueCreateInfo,
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = NULL,
        .enabledExtensionCount = extensionCount,
        .ppEnabledExtensionNames = extensions,
        .pEnabledFeatures = NULL, // Using features2 instead
    };

    // Use xrCreateVulkanDeviceKHR to create device with XR-required extensions
    XrVulkanDeviceCreateInfoKHR xrDeviceCreateInfo = {
        .type = XR_TYPE_VULKAN_DEVICE_CREATE_INFO_KHR,
        .next = NULL,
        .systemId = systemId,
        .createFlags = 0,
        .pfnGetInstanceProcAddr = vkGetInstanceProcAddr,
        .vulkanPhysicalDevice = vr_vk.physicalDevice,
        .vulkanCreateInfo = &deviceCreateInfo,
        .vulkanAllocator = NULL,
    };

    VkResult vkResult;
    XrResult result = xrCreateVulkanDeviceKHR(xrInstance, &xrDeviceCreateInfo, &vr_vk.device, &vkResult);

    if (XR_FAILED(result)) {
        fprintf(stderr, "[VRVK] xrCreateVulkanDeviceKHR failed: %s\n",
                GetXRErrorString(result));
        return result;
    }

    if (vkResult != VK_SUCCESS) {
        fprintf(stderr, "[VRVK] vkCreateDevice failed: %d\n", vkResult);
        return XR_ERROR_INITIALIZATION_FAILED;
    }

    // Get the graphics queue
    vkGetDeviceQueue(vr_vk.device, vr_vk.queueFamilyIndex, 0, &vr_vk.queue);

    fprintf(stdout, "[VRVK] VkDevice: %p, queue family: %d, queue: %p\n",
        (void*)vr_vk.device, vr_vk.queueFamilyIndex, (void*)vr_vk.queue);

    if (vr_vk.queue == VK_NULL_HANDLE) {
        fprintf(stderr, "[VRVK] ERROR: Failed to get Vulkan queue!\n");
        return XR_ERROR_INITIALIZATION_FAILED;
    }

    vr_vk_initialized = VR_TRUE;

    fprintf(stdout, "[VRVK] VkDevice created successfully\n");
    fprintf(stdout, "[VRVK] Vulkan initialization complete\n");

    return XR_SUCCESS;
}

void VR_Vulkan_Shutdown(void)
{
    if (!vr_vk_initialized) {
        return;
    }

    fprintf(stdout, "[VRVK] Shutting down Vulkan...\n");

    // NOTE: We do NOT destroy vr_vk.device or vr_vk.instance here.
    // The renderer (renderervk) pulls these handles via VR_Vulkan_GetDeviceInfo()
    // and is responsible for destroying them in vk_shutdown().
    // We only clear our state to mark ourselves as uninitialized.

    memset(&vr_vk, 0, sizeof(vr_vk));
    vr_vk_initialized = VR_FALSE;

    // Clear XR function pointers: they become invalid when XrInstance is destroyed
    xrGetVulkanGraphicsRequirements2KHR = NULL;
    xrGetVulkanGraphicsDevice2KHR = NULL;
    xrCreateVulkanInstanceKHR = NULL;
    xrCreateVulkanDeviceKHR = NULL;

    fprintf(stdout, "[VRVK] Vulkan shutdown complete\n");
}

// XR Swapchain format selection
VkFormat VR_Vulkan_SelectColorFormat(const int64_t* formats, uint32_t count)
{
    // Preference order for color (sRGB preferred for gamma-correct rendering)
    const VkFormat preferred[] = {
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_FORMAT_B8G8R8A8_SRGB,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_B8G8R8A8_UNORM,
    };

    for (uint32_t p = 0; p < sizeof(preferred) / sizeof(preferred[0]); p++) {
        for (uint32_t f = 0; f < count; f++) {
            if (formats[f] == (int64_t)preferred[p]) {
                return preferred[p];
            }
        }
    }

    // Use first available as last resort
    return (VkFormat)formats[0];
}

VkFormat VR_Vulkan_SelectDepthFormat(const int64_t* formats, uint32_t count)
{
    // Preference order for depth
#ifdef __ANDROID__
    // Mobile: prefer D24 for GPU efficiency
    const VkFormat preferred[] = {
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
    };
#else
    // Desktop: prefer 32-bit float for reversed depth precision
    const VkFormat preferred[] = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT,
    };
#endif

    for (uint32_t p = 0; p < sizeof(preferred) / sizeof(preferred[0]); p++) {
        for (uint32_t f = 0; f < count; f++) {
            if (formats[f] == (int64_t)preferred[p]) {
                return preferred[p];
            }
        }
    }

    return (VkFormat)formats[0];
}

VR_Bool VR_Vulkan_SwapchainsTakeDensityMapOffsets(void)
{
    return (vr_vk.fragmentDensityMapOffsetSupported && VR_HasSwapchainCreateFlags()) ? VR_TRUE : VR_FALSE;
}

XrResult VR_Vulkan_CreateSwapchain(XrSession session, VkFormat format,
                                    uint32_t width, uint32_t height,
                                    uint32_t arraySize, XrSwapchainUsageFlags usage,
                                    XrBool32 foveated,
                                    XrSwapchain* swapchain)
{
    // Simple version without format list
    return VR_Vulkan_CreateSwapchainWithFormatList(session, format, width, height,
                                                    arraySize, usage, NULL, 0, foveated, swapchain);
}

XrResult VR_Vulkan_CreateSwapchainWithFormatList(XrSession session, VkFormat format,
                                                  uint32_t width, uint32_t height,
                                                  uint32_t arraySize, XrSwapchainUsageFlags usage,
                                                  const VkFormat* viewFormats, uint32_t viewFormatCount,
                                                  XrBool32 foveated,
                                                  XrSwapchain* swapchain)
{
    // Build the next chain: we'll chain structs together
    void* nextChain = NULL;

    // Format list info (XR_KHR_vulkan_swapchain_format_list extension)
    // This tells the runtime which formats we'll use for image views, helping it
    // avoid adding unnecessary usage flags like VK_IMAGE_USAGE_STORAGE_BIT
    XrVulkanSwapchainFormatListCreateInfoKHR formatListInfo = {
        .type = XR_TYPE_VULKAN_SWAPCHAIN_FORMAT_LIST_CREATE_INFO_KHR,
        .next = NULL,
        .viewFormatCount = viewFormatCount,
        .viewFormats = viewFormats,
    };
    if (viewFormats && viewFormatCount > 0) {
        formatListInfo.next = nextChain;
        nextChain = &formatListInfo;
    }

    XrSwapchainCreateInfoFoveationFB foveationInfo = {
        .type = XR_TYPE_SWAPCHAIN_CREATE_INFO_FOVEATION_FB,
        .next = NULL,
        .flags = XR_SWAPCHAIN_CREATE_FOVEATION_FRAGMENT_DENSITY_MAP_BIT_FB,
    };
    if (foveated) {
        foveationInfo.next = nextChain;
        nextChain = &foveationInfo;
    }

    // Direct rendering draws the foveated scene into these images, and an offset pass needs every attachment flagged
    XrVulkanSwapchainCreateInfoMETA vulkanInfo = {
        .type = XR_TYPE_VULKAN_SWAPCHAIN_CREATE_INFO_META,
        .next = NULL,
        .additionalCreateFlags = VK_IMAGE_CREATE_FRAGMENT_DENSITY_MAP_OFFSET_BIT_QCOM,
        .additionalUsageFlags = 0,
    };
    if (VR_Vulkan_SwapchainsTakeDensityMapOffsets()) {
        vulkanInfo.next = nextChain;
        nextChain = &vulkanInfo;
    }

    XrSwapchainCreateInfo createInfo = {
        .type = XR_TYPE_SWAPCHAIN_CREATE_INFO,
        .next = nextChain,
        .createFlags = 0,
        .usageFlags = usage,
        .format = (int64_t)format,
        .sampleCount = 1,
        .width = width,
        .height = height,
        .faceCount = 1,
        .arraySize = arraySize,
        .mipCount = 1,
    };

    return xrCreateSwapchain(session, &createInfo, swapchain);
}

XrResult VR_Vulkan_GetSwapchainImages(XrSwapchain swapchain,
                                       VkImage** images, uint32_t* imageCount,
                                       VkImage** foveationImages,
                                       uint32_t* foveationWidth, uint32_t* foveationHeight)
{
    XrSwapchainImageFoveationVulkanFB* xrFoveation = NULL;

    if (foveationImages) {
        *foveationImages = NULL;
    }
    if (foveationWidth) {
        *foveationWidth = 0;
    }
    if (foveationHeight) {
        *foveationHeight = 0;
    }

    // Get count
    XrResult result = xrEnumerateSwapchainImages(swapchain, 0, imageCount, NULL);
    if (XR_FAILED(result)) {
        return result;
    }

    // Allocate XrSwapchainImageVulkanKHR array
    XrSwapchainImageVulkanKHR* xrImages = (XrSwapchainImageVulkanKHR*)malloc(
        sizeof(XrSwapchainImageVulkanKHR) * (*imageCount));
    if (!xrImages) {
        return XR_ERROR_OUT_OF_MEMORY;
    }

    if (foveationImages) {
        xrFoveation = (XrSwapchainImageFoveationVulkanFB*)calloc(*imageCount, sizeof(XrSwapchainImageFoveationVulkanFB));
        if (!xrFoveation) {
            free(xrImages);
            return XR_ERROR_OUT_OF_MEMORY;
        }
    }

    for (uint32_t i = 0; i < *imageCount; i++) {
        xrImages[i].type = XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR;
        xrImages[i].next = NULL;
        if (xrFoveation) {
            xrFoveation[i].type = XR_TYPE_SWAPCHAIN_IMAGE_FOVEATION_VULKAN_FB;
            xrFoveation[i].next = NULL;
            xrFoveation[i].image = VK_NULL_HANDLE;
            xrImages[i].next = &xrFoveation[i];
        }
    }

    result = xrEnumerateSwapchainImages(swapchain, *imageCount, imageCount,
                                         (XrSwapchainImageBaseHeader*)xrImages);

    if (XR_SUCCEEDED(result)) {
        // Extract VkImage handles
        *images = (VkImage*)malloc(sizeof(VkImage) * (*imageCount));
        if (*images) {
            for (uint32_t i = 0; i < *imageCount; i++) {
                (*images)[i] = xrImages[i].image;
            }
        } else {
            result = XR_ERROR_OUT_OF_MEMORY;
        }
    }

    if (XR_SUCCEEDED(result) && xrFoveation) {
        uint32_t filled = 0;
        for (uint32_t i = 0; i < *imageCount; i++) {
            if (xrFoveation[i].image != VK_NULL_HANDLE && xrFoveation[i].width > 0 && xrFoveation[i].height > 0) {
                filled++;
            }
        }
        if (filled == *imageCount) {
            *foveationImages = (VkImage*)malloc(sizeof(VkImage) * (*imageCount));
            if (*foveationImages) {
                for (uint32_t i = 0; i < *imageCount; i++) {
                    (*foveationImages)[i] = xrFoveation[i].image;
                }
                if (foveationWidth) {
                    *foveationWidth = xrFoveation[0].width;
                }
                if (foveationHeight) {
                    *foveationHeight = xrFoveation[0].height;
                }
            }
        } else {
            // Per-image dump tells a late or partial answer apart from none at all
            uint32_t i;
            VR_VK_LogLine(va("Runtime returned density maps for %u of %u swapchain images; ignoring them",
                filled, *imageCount));
            for (i = 0; i < *imageCount && i < 4; i++) {
                VR_VK_LogLine(va("  image %u: density map %p, %ux%u", i,
                    (void*)xrFoveation[i].image, xrFoveation[i].width, xrFoveation[i].height));
            }
        }
    }

    free(xrFoveation);
    free(xrImages);
    return result;
}

// VkImageView/VkFramebuffer creation moved to renderer (see vk_create_xr_image_views in vk.c)
// VR layer only provides XrSwapchain handles and VkImage handles

// ============================================================================
// VR_Graphics interface implementation for Vulkan
// These functions are called from vrcommon code and provide the graphics-specific
// behavior for Vulkan builds.
// ============================================================================

// Cached graphics requirements
static VR_VK_GraphicsRequirements s_vkRequirements;
static VR_Bool s_requirementsFetched = VR_FALSE;

const char* VR_Graphics_GetExtensionName(void)
{
	return VR_VK_GetGraphicsExtensionName();
}

XrResult VR_Graphics_GetRequirements(XrInstance instance, XrSystemId systemId)
{
	XrResult result = VR_VK_GetGraphicsRequirements(instance, systemId, &s_vkRequirements);
	if (XR_SUCCEEDED(result)) {
		s_requirementsFetched = VR_TRUE;
	}
	return result;
}

void VR_Graphics_PrintRequirements(void)
{
	if (s_requirementsFetched) {
		VR_VK_PrintGraphicsRequirements(&s_vkRequirements);
	}
}

void VR_Graphics_Init(XrInstance instance, XrSystemId systemId)
{
	fprintf(stdout, "[VRVK] Initializing Vulkan via XR_KHR_vulkan_enable2...\n");

	// Load XR Vulkan function pointers
	if (!LoadXrVulkanFunctions(instance)) {
		fprintf(stderr, "[VRVK] Failed to load XR Vulkan functions\n");
		return;
	}

	// Check requirements
	XrResult result = VR_Vulkan_CheckRequirements(instance, systemId);
	if (XR_FAILED(result)) {
		fprintf(stderr, "[VRVK] Failed to check Vulkan requirements\n");
		return;
	}

	// Create VkInstance via xrCreateVulkanInstanceKHR
	result = VR_Vulkan_CreateInstance(instance, systemId);
	if (XR_FAILED(result)) {
		fprintf(stderr, "[VRVK] Failed to create VkInstance\n");
		return;
	}

	// Get the physical device that OpenXR requires
	result = VR_Vulkan_GetPhysicalDevice(instance, systemId);
	if (XR_FAILED(result)) {
		fprintf(stderr, "[VRVK] Failed to get physical device\n");
		return;
	}

	// Create VkDevice via xrCreateVulkanDeviceKHR
	result = VR_Vulkan_CreateDevice(instance, systemId);
	if (XR_FAILED(result)) {
		fprintf(stderr, "[VRVK] Failed to create VkDevice\n");
		return;
	}

	fprintf(stdout, "[VRVK] Vulkan initialization complete\n");
}

void VR_Graphics_Shutdown(void)
{
	VR_Vulkan_Shutdown();
	s_requirementsFetched = VR_FALSE;
}

XrBool32 VR_Graphics_SupportsFoveation(void)
{
	return vr_vk_initialized && vr_vk.fragmentDensityMapSupported && vr_vk.fragmentDensityMapNonSubsampled;
}

void VR_Graphics_InvalidateFunctionPointers(void)
{
	// Clear XR function pointers before XrInstance is destroyed.
	// These were obtained via xrGetInstanceProcAddr and become invalid
	// after xrDestroyInstance. This prevents use-after-free crashes
	// when VR_Graphics_GetRequirements is called after vid_restart.
	xrGetVulkanGraphicsRequirements2KHR = NULL;
	xrGetVulkanGraphicsDevice2KHR = NULL;
	xrCreateVulkanInstanceKHR = NULL;
	xrCreateVulkanDeviceKHR = NULL;
	s_requirementsFetched = VR_FALSE;
}
