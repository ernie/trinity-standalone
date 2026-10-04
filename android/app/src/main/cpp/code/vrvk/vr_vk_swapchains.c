/*
 * vr_vk_swapchains.c - Vulkan XR swapchain management
 *
 * VR layer swapchain operations for Vulkan. Creates and manages XrSwapchains
 * and provides access to VkImage handles. VkImageViews and VkFramebuffers
 * are created by the renderer (see VkXrResources in renderervk/vk.h).
 */

#include "vr_vk_swapchains.h"
#include "vr_vk.h"
#include "vr_vk_foveation.h"

#include "../vrcommon/vr_base.h"
#include "../client/client.h"
#include "../qcommon/qcommon.h"
#include "../vrcommon/vr_macros.h"
#include "../vrcommon/vr_swapchains.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#if __ANDROID__
#include <android/log.h>
#define LOG_TAG "VRVK"
#define ALOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#endif

//
// Internal helpers
//

// Helper to get the UNORM equivalent of an sRGB format
static VkFormat vk_get_unorm_format(VkFormat srgbFormat)
{
	switch (srgbFormat) {
		case VK_FORMAT_R8G8B8A8_SRGB:  return VK_FORMAT_R8G8B8A8_UNORM;
		case VK_FORMAT_B8G8R8A8_SRGB:  return VK_FORMAT_B8G8R8A8_UNORM;
		case VK_FORMAT_A8B8G8R8_SRGB_PACK32:  return VK_FORMAT_A8B8G8R8_UNORM_PACK32;
		default: return srgbFormat;  // Return as-is if not sRGB
	}
}

/*
 * Mutable (UNORM views) unless r_fbo is on and the runtime lacks XR_KHR_vulkan_swapchain_format_list: an Adreno
 * keeps a mutable image compressed only when told its view formats, the Quest runtime does not offer the extension,
 * and its uncompressed eye buffers cost about a third of the frame. A plain sRGB swapchain takes the renderer's
 * sRGB route (vk_xr_srgb_target); direct mode needs the UNORM views, so it stays mutable, and
 * VR_VK_Swapchains_CheckMode replaces a swapchain that stops fitting r_fbo at the next restart.
 */
static XrBool32 VR_VK_ColorSwapchainMutable(void)
{
	return (!Cvar_VariableIntegerValue("r_fbo") || VR_HasSwapchainFormatList()) ? XR_TRUE : XR_FALSE;
}

static void VR_VK_CreateSwapchain(
	XrSession session,
	XrBool32 isColor,
	XrBool32 mutableFormat,  // Allow creating UNORM views for gamma pass
	VkFormat format,
	uint32_t width,
	uint32_t height,
	uint32_t arraySize,
	VR_VK_SwapchainInfo* info)
{
	// TRANSFER_SRC serves screenshots, TRANSFER_DST runtime compatibility
	XrSwapchainUsageFlags usage = isColor
		? (XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT)
		: (XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);

	if (mutableFormat) {
		usage |= XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT;
	}

	XrResult result;

	const XrBool32 foveated = isColor && VR_VK_Foveation_SwapchainWanted();

	// Meta's own integrations create foveated swapchains without transfer usage; screenshots check before relying on it
	if (foveated) {
		usage &= ~(XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT);
	}

	// Names both view formats so the driver can keep the mutable image compressed
	if (isColor && mutableFormat && VR_HasSwapchainFormatList()) {
		VkFormat viewFormats[2] = {
			format,                        // sRGB for normal rendering
			vk_get_unorm_format(format)    // UNORM for gamma pass (bypass sRGB conversion)
		};

		ALOGI("Creating color swapchain with format list: sRGB=0x%x, UNORM=0x%x, foveated=%s",
			(unsigned int)viewFormats[0], (unsigned int)viewFormats[1], foveated ? "yes" : "no");

		result = VR_Vulkan_CreateSwapchainWithFormatList(session, format, width, height,
			arraySize, usage, viewFormats, 2, foveated, &info->swapchain);
	} else {
		result = VR_Vulkan_CreateSwapchain(session, format, width, height,
			arraySize, usage, foveated, &info->swapchain);
	}

	CHECK(!XR_FAILED(result), isColor ? "Failed to create color swapchain" : "Failed to create depth swapchain");

	info->format = format;
	info->width = width;
	info->height = height;
	info->arraySize = arraySize;
	info->usage = usage;
	info->acquired = XR_FALSE;  // Not acquired yet
	info->foveationImages = NULL;
	info->foveationWidth = 0;
	info->foveationHeight = 0;

	// Before enumerating images: a runtime may allocate the maps only once it knows a profile
	if (foveated) {
		VR_VK_Foveation_ApplyToSwapchain(VR_GetEngine(), info->swapchain);
	}

	result = VR_Vulkan_GetSwapchainImages(info->swapchain, &info->images, &info->imageCount,
		foveated ? &info->foveationImages : NULL,
		foveated ? &info->foveationWidth : NULL,
		foveated ? &info->foveationHeight : NULL);
	CHECK(!XR_FAILED(result), "Failed to get swapchain images");

	if (foveated) {
		if (info->foveationImages) {
			ALOGI("Fragment density maps: %u images, %ux%u texels for %ux%u pixels",
				info->imageCount, info->foveationWidth, info->foveationHeight, width, height);
		} else {
			// Meta's runtime ignores the create info chain under the legacy profile it gives OpenXR 1.0.0 apps (see vr_base.c)
			ALOGE("Runtime accepted the foveated swapchain but returned no density maps");
			VR_VK_Foveation_Disarm(VR_GetEngine(), info->swapchain);
		}
	}
}

static void VR_VK_DestroySwapchain(VR_VK_SwapchainInfo* info)
{
	if (!info) {
		return;
	}

	// Release any acquired image before destroying the swapchain.
	// OpenXR requires all acquired images to be released before xrDestroySwapchain.
	if (info->swapchain != XR_NULL_HANDLE && info->acquired) {
		XrSwapchainImageReleaseInfo releaseInfo = {XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO, NULL};
		xrReleaseSwapchainImage(info->swapchain, &releaseInfo);
		info->acquired = XR_FALSE;
	}

	// Free images array (we don't own the VkImages themselves: OpenXR does)
	if (info->images) {
		free(info->images);
		info->images = NULL;
	}
	if (info->foveationImages) {
		free(info->foveationImages);
		info->foveationImages = NULL;
	}
	info->foveationWidth = 0;
	info->foveationHeight = 0;
	info->imageCount = 0;

	if (info->swapchain != XR_NULL_HANDLE) {
		XR_CHECK(
			xrDestroySwapchain(info->swapchain),
			"Failed to destroy XR swapchain");
		info->swapchain = XR_NULL_HANDLE;
	}
}

//
// Public API
//

VR_SwapchainInfos* VR_VK_CreateSwapchains(XrInstance instance, XrSystemId systemId, XrSession session)
{
	// Get best view configuration
	const XrViewConfigurationType viewConfigurationType = VR_GetBestViewConfiguration(instance, systemId);
	CHECK(
		viewConfigurationType != XR_VIEW_CONFIGURATION_TYPE_MAX_ENUM,
		"No required view configuration type supported");

	// Get view configuration views
	XrViewConfigurationView* views = NULL;
	const uint32_t viewCount = VR_GetViewConfigurationViews(instance, systemId, viewConfigurationType, &views);

	// Sanity check: all views must have same resolution for multiview rendering
	for (uint32_t idx = 0; idx < viewCount; ++idx) {
		CHECK(
			views[0].recommendedImageRectWidth == views[idx].recommendedImageRectWidth,
			"Failed sanity check for same image sizes in Vulkan Multiview rendering");
		CHECK(
			views[0].recommendedImageRectHeight == views[idx].recommendedImageRectHeight,
			"Failed sanity check for same image sizes in Vulkan Multiview rendering");
	}

	// Get available swapchain formats
	int64_t* formats = NULL;
	const uint32_t formatCount = VR_GetSwapchainFormats(session, &formats);

	// Select best formats for Vulkan
	const VkFormat colorFormat = VR_Vulkan_SelectColorFormat(formats, formatCount);
	const VkFormat depthFormat = VR_Vulkan_SelectDepthFormat(formats, formatCount);
	free(formats);

	ALOGI("Chosen VK formats: {color: 0x%x, depth: 0x%x}",
		(unsigned int)colorFormat, (unsigned int)depthFormat);

	// Calculate supersampled resolution
	int supersampledWidth = views[0].recommendedImageRectWidth;
	int supersampledHeight = views[0].recommendedImageRectHeight;
	VR_GetSupersampledResolution(instance, systemId, &supersampledWidth, &supersampledHeight);

	// Allocate swapchain info structure
	VR_SwapchainInfos* swapchains = calloc(1, sizeof(VR_SwapchainInfos));
	if (!swapchains) {
		free(views);
		return NULL;
	}
	swapchains->viewCount = viewCount;

	// Create color swapchain (multiview - 2 layers for stereo)
	VR_VK_CreateSwapchain(
		session,
		XR_TRUE,  // isColor
		VR_VK_ColorSwapchainMutable(),
		colorFormat,
		supersampledWidth,
		supersampledHeight,
		viewCount,  // arraySize = 2 for stereo
		&swapchains->color);

	ALOGI("Created color swapchain: %dx%d, %u images, %u layers",
		swapchains->color.width, swapchains->color.height,
		swapchains->color.imageCount, swapchains->color.arraySize);

	// Create depth swapchain (multiview - 2 layers for stereo)
	VR_VK_CreateSwapchain(
		session,
		XR_FALSE,  // isColor
		XR_FALSE,  // mutableFormat - not needed for depth
		depthFormat,
		supersampledWidth,
		supersampledHeight,
		viewCount,  // arraySize = 2 for stereo
		&swapchains->depth);

	ALOGI("Created depth swapchain: %dx%d, %u images, %u layers",
		swapchains->depth.width, swapchains->depth.height,
		swapchains->depth.imageCount, swapchains->depth.arraySize);

	free(views);
	return swapchains;
}

void VR_VK_DestroySwapchains(VR_SwapchainInfos** swapchainsPtr)
{
	if (!swapchainsPtr || !*swapchainsPtr) {
		return;
	}

	VR_SwapchainInfos* swapchains = *swapchainsPtr;

	// Destroy swapchains (VR layer only owns XrSwapchain handles)
	// VkImageViews and VkFramebuffers are destroyed by the renderer
	VR_VK_DestroySwapchain(&swapchains->depth);
	VR_VK_DestroySwapchain(&swapchains->color);

	free(swapchains);
	*swapchainsPtr = NULL;
}

void VR_VK_Swapchains_Acquire(VR_SwapchainInfos* swapchains, uint32_t* colorIndex, uint32_t* depthIndex)
{
	if (!swapchains) {
		return;
	}

	XrSwapchain xrSwapchains[2] = {swapchains->color.swapchain, swapchains->depth.swapchain};
	uint32_t* indices[2] = {colorIndex, depthIndex};

	for (uint32_t idx = 0; idx < 2; ++idx) {
		XrSwapchainImageAcquireInfo acquireInfo = {XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO, NULL};

		XR_CHECK(
			xrAcquireSwapchainImage(xrSwapchains[idx], &acquireInfo, indices[idx]),
			"Failed to acquire swapchain image");

		XrSwapchainImageWaitInfo waitInfo = {
			.type = XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO,
			.next = NULL,
			.timeout = XR_INFINITE_DURATION
		};

		CHECK(
			!XR_FAILED(xrWaitSwapchainImage(xrSwapchains[idx], &waitInfo)),
			"Failed to wait for swapchain image");
	}

	// Mark swapchains as acquired for cleanup tracking
	swapchains->color.acquired = XR_TRUE;
	swapchains->depth.acquired = XR_TRUE;
}

void VR_VK_Swapchains_Release(VR_SwapchainInfos* swapchains)
{
	if (!swapchains) {
		return;
	}

	XrSwapchain xrSwapchains[2] = {swapchains->color.swapchain, swapchains->depth.swapchain};

	for (uint32_t idx = 0; idx < 2; ++idx) {
		XrSwapchainImageReleaseInfo releaseInfo = {XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO, NULL};
		XR_CHECK(
			xrReleaseSwapchainImage(xrSwapchains[idx], &releaseInfo),
			"Failed to release swapchain image");
	}

	// Mark swapchains as released
	swapchains->color.acquired = XR_FALSE;
	swapchains->depth.acquired = XR_FALSE;
	swapchains->color.everReleased = XR_TRUE;
	swapchains->depth.everReleased = XR_TRUE;
}

//
// Accessors for renderer to get swapchain info
//

const VR_VK_SwapchainInfo* VR_VK_GetColorSwapchain(const VR_SwapchainInfos* swapchains)
{
	return swapchains ? &swapchains->color : NULL;
}

const VR_VK_SwapchainInfo* VR_VK_GetDepthSwapchain(const VR_SwapchainInfos* swapchains)
{
	return swapchains ? &swapchains->depth : NULL;
}

// Swapchains are replaced only at the start of a frame, before xrBeginFrame, once something has asked for it

static qboolean g_SwapchainRecreateRequested = qfalse;

void VR_VK_Swapchains_RequestRecreate(void)
{
	g_SwapchainRecreateRequested = qtrue;
}

void VR_VK_Swapchains_CheckMode(void)
{
	const VR_Engine* engine = VR_GetEngine();
	const VR_SwapchainInfos* swapchains = engine ? engine->appState.Renderer.Swapchains : NULL;

	// A vid_restart keeps the swapchains, so one built for the other r_fbo mode is replaced next frame
	if (swapchains &&
		((swapchains->color.usage & XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT) != 0) != (VR_VK_ColorSwapchainMutable() != XR_FALSE)) {
		g_SwapchainRecreateRequested = qtrue;
	}
}

qboolean VR_VK_Swapchains_HandlePendingRecreate(VR_Engine* engine)
{
	static qboolean createFailed;
	if (!g_SwapchainRecreateRequested) {
		return qfalse;
	}

	if (!engine || engine->appState.Session == XR_NULL_HANDLE) {
		return qfalse; // the request waits for a session
	}

	// Submitted frames may still draw into the images, and the renderer's views of them must go first
	if (re.ReleaseXRResources) {
		re.ReleaseXRResources();
	}

	if (engine->appState.Renderer.Swapchains) {
		VR_VK_DestroySwapchains(&engine->appState.Renderer.Swapchains);
	}

	engine->appState.Renderer.Swapchains = VR_VK_CreateSwapchains(
		engine->appState.Instance,
		engine->appState.SystemId,
		engine->appState.Session);

	if (!engine->appState.Renderer.Swapchains) {
		if (!createFailed)
			ALOGE("VR_VK_Swapchains_HandlePendingRecreate: Failed to create swapchains; retrying each frame");
		createFailed = qtrue;
		return qfalse;
	}

	createFailed = qfalse;
	g_SwapchainRecreateRequested = qfalse;
	return qtrue;
}
