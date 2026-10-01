/*
 * vr_swapchains.c - Common OpenXR swapchain utilities
 *
 * Graphics-API-agnostic OpenXR swapchain operations.
 */

#include "vr_swapchains.h"
#include "vr_macros.h"
#include "../qcommon/q_shared.h"

#include <stdlib.h>
#include <stdio.h>

#ifdef __ANDROID__
#include <sys/system_properties.h>
#endif

// External cvars used for resolution calculations
extern cvar_t *vr_superSampling;

//
// View configuration enumeration
//

XrViewConfigurationType VR_GetBestViewConfiguration(XrInstance instance, XrSystemId systemId)
{
	uint32_t viewConfigurationCount = 0;
	XR_CHECK(
		xrEnumerateViewConfigurations(instance, systemId, 0, &viewConfigurationCount, NULL),
		"Failed to count View Configurations");

	XrViewConfigurationType* viewConfigurations = calloc(viewConfigurationCount, sizeof(XrViewConfigurationType));
	XR_CHECK(
		xrEnumerateViewConfigurations(instance, systemId, viewConfigurationCount, &viewConfigurationCount, viewConfigurations),
		"Failed to enumerate View Configurations");

	for (uint32_t idx = 0; idx < viewConfigurationCount; ++idx)
	{
		if (viewConfigurations[idx] == XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO)
		{
			free(viewConfigurations);
			return XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
		}
	}

	free(viewConfigurations);
	return XR_VIEW_CONFIGURATION_TYPE_MAX_ENUM;
}

uint32_t VR_GetViewConfigurationViews(
	XrInstance instance,
	XrSystemId systemId,
	XrViewConfigurationType viewConfig,
	XrViewConfigurationView** views)
{
	uint32_t viewCount = 0;
	XR_CHECK(
		xrEnumerateViewConfigurationViews(instance, systemId, viewConfig, 0, &viewCount, NULL),
		"Failed to count ViewConfiguration Views");

	*views = calloc(viewCount, sizeof(XrViewConfigurationView));
	for (uint32_t idx = 0; idx < viewCount; ++idx)
	{
		(*views)[idx].type = XR_TYPE_VIEW_CONFIGURATION_VIEW;
	}
	XR_CHECK(
		xrEnumerateViewConfigurationViews(instance, systemId, viewConfig, viewCount, &viewCount, *views),
		"Failed to enumerate ViewConfiguration Views");

	return viewCount;
}

//
// Swapchain format enumeration
//

uint32_t VR_GetSwapchainFormats(XrSession session, int64_t** formats)
{
	uint32_t formatCount = 0;
	XR_CHECK(
		xrEnumerateSwapchainFormats(session, 0, &formatCount, NULL),
		"Failed to count Swapchain formats");

	*formats = calloc(formatCount, sizeof(int64_t));
	XR_CHECK(
		xrEnumerateSwapchainFormats(session, formatCount, &formatCount, *formats),
		"Failed to enumerate Swapchain formats");

	return formatCount;
}

XrBool32 VR_HasFormatInList(int64_t* formats, uint32_t formatCount, int64_t format)
{
	for (uint32_t idx = 0; idx < formatCount; ++idx)
	{
		if (formats[idx] == format)
		{
			return XR_TRUE;
		}
	}
	return XR_FALSE;
}

//
// Resolution helpers
//

#ifdef __ANDROID__
// Helper to read Android system properties (used for ADB resolution override)
static int VR_GetSystemPropertyInt(const char* name, int defaultValue)
{
	char value[PROP_VALUE_MAX] = {0};
	if (__system_property_get(name, value) > 0)
	{
		int result = atoi(value);
		if (result > 0)
		{
			return result;
		}
	}
	return defaultValue;
}
#endif

float VR_GetSupersamplingFactor(void)
{
	// Get supersampling factor from cvar with sane limits
	float supersampling = vr_superSampling ? vr_superSampling->value : 1.0f;

	if (supersampling < 0.5f) {
		supersampling = 0.5f;
	} else if (supersampling > 4.0f) {
		supersampling = 4.0f;
	}
	return supersampling;
}

void VR_GetRecommendedResolution(
	XrInstance instance,
	XrSystemId systemId,
	int* width,
	int* height,
	int* maxWidth,
	int* maxHeight)
{
	const XrViewConfigurationType viewConfigurationType = VR_GetBestViewConfiguration(instance, systemId);
	CHECK(
		viewConfigurationType != XR_VIEW_CONFIGURATION_TYPE_MAX_ENUM,
		"No required view configuration type supported");

	XrViewConfigurationView* views = NULL;
	VR_GetViewConfigurationViews(instance, systemId, viewConfigurationType, &views);

	*width = views[0].recommendedImageRectWidth;
	*height = views[0].recommendedImageRectHeight;

	if (maxWidth)
	{
		*maxWidth = views[0].maxImageRectWidth;
	}
	if (maxHeight)
	{
		*maxHeight = views[0].maxImageRectHeight;
	}

	free(views);
}

void VR_GetBaseResolution(
	XrInstance instance,
	XrSystemId systemId,
	int* width,
	int* height,
	int* maxWidth,
	int* maxHeight)
{
	VR_GetRecommendedResolution(instance, systemId, width, height, maxWidth, maxHeight);

#ifdef __ANDROID__
	// Check for ADB resolution overrides (set by QGO, SideQuest, or manual adb setprop)
	int adbWidth = VR_GetSystemPropertyInt("debug.oculus.textureWidth", 0);
	int adbHeight = VR_GetSystemPropertyInt("debug.oculus.textureHeight", 0);

	if (adbWidth > 0 && adbHeight > 0)
	{
		Com_Printf("Using ADB override resolution: %dx%d (OpenXR recommended: %dx%d)\n",
			adbWidth, adbHeight, *width, *height);
		*width = adbWidth;
		*height = adbHeight;
	}
#endif
}

void VR_GetSupersampledResolution(
	XrInstance instance,
	XrSystemId systemId,
	int* width,
	int* height)
{
	int maxWidth = 0, maxHeight = 0, baseWidth, baseHeight;
	VR_GetBaseResolution(instance, systemId, &baseWidth, &baseHeight, &maxWidth, &maxHeight);

	const float supersamplingFactor = VR_GetSupersamplingFactor();
	int supersampledWidth = (int)(baseWidth * supersamplingFactor);
	int supersampledHeight = (int)(baseHeight * supersamplingFactor);

	if (supersampledWidth > maxWidth || supersampledHeight > maxHeight)
	{
		const float adjustedSupersamplingFactor = MIN(maxWidth / (float)baseWidth, maxHeight / (float)baseHeight);
		supersampledWidth = (int)(baseWidth * adjustedSupersamplingFactor);
		supersampledHeight = (int)(baseHeight * adjustedSupersamplingFactor);
	}

	*width = supersampledWidth;
	*height = supersampledHeight;
}
