/*
 * vr_swapchains.h - Common OpenXR swapchain utilities
 *
 * Graphics-API-agnostic OpenXR swapchain operations.
 * These functions only call OpenXR APIs and don't touch OpenGL or Vulkan.
 */

#ifndef __VR_SWAPCHAINS_COMMON
#define __VR_SWAPCHAINS_COMMON

#include "vr_types.h"

// Desktop mirror view configuration
typedef enum
{
	LEFT_EYE = 1,
	RIGHT_EYE = 2,
	BOTH_EYES = 3,
} XrDesktopViewConfiguration;

// View configuration enumeration
XrViewConfigurationType VR_GetBestViewConfiguration(XrInstance instance, XrSystemId systemId);

uint32_t VR_GetViewConfigurationViews(
	XrInstance instance,
	XrSystemId systemId,
	XrViewConfigurationType viewConfig,
	XrViewConfigurationView** views);

// Swapchain format enumeration
uint32_t VR_GetSwapchainFormats(XrSession session, int64_t** formats);
XrBool32 VR_HasFormatInList(int64_t* formats, uint32_t formatCount, int64_t format);

// Resolution helpers
float VR_GetSupersamplingFactor(void);

void VR_GetRecommendedResolution(
	XrInstance instance,
	XrSystemId systemId,
	int* width,
	int* height,
	int* maxWidth,
	int* maxHeight);

void VR_GetSupersampledResolution(
	XrInstance instance,
	XrSystemId systemId,
	int* width,
	int* height);

// The size supersampling scales: the runtime's recommendation, or the ADB override when one is set.
void VR_GetBaseResolution(
	XrInstance instance,
	XrSystemId systemId,
	int* width,
	int* height,
	int* maxWidth,
	int* maxHeight);

#endif // __VR_SWAPCHAINS_COMMON
