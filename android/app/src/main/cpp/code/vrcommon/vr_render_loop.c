#include "vr_render_loop.h"

#include <string.h>

#include "../qcommon/qcommon.h"
#include "vr_macros.h"
#include "vr_clientinfo.h"
#include "vr_rate_list.h"
#include "vr_gameplay.h"
#include "../vrvk/vr_vk_types.h"

extern cvar_t *vr_frameTimingLog;

// Diagnostic only: logs XR frame pacing (shouldRender transitions and
// predictedDisplayTime deltas) to the console when vr_frameTimingLog is
// nonzero. Never touches frame submission. No-op (aside from re-arming
// its own priming state) when disabled.
static void VR_LogFrameTiming( const XrFrameState *fs, qboolean enabled )
{
	static qboolean primed = qfalse;
	static XrBool32 lastShouldRender = 0;
	static XrTime lastDisplayTime = 0;
	static XrTime windowAccumTime = 0;
	static XrTime windowMaxDelta = 0;
	static XrDuration windowPeriod = 0;
	static int windowFrameCount = 0;
	static int windowLongCount = 0;
	XrTime delta;

	if ( !enabled )
	{
		// Re-arm so the next enable primes cleanly instead of logging a
		// spurious delta spanning the disabled interval.
		primed = qfalse;
		return;
	}

	if ( !primed )
	{
		primed = qtrue;
		lastShouldRender = fs->shouldRender;
		lastDisplayTime = fs->predictedDisplayTime;
		windowPeriod = fs->predictedDisplayPeriod;
		windowAccumTime = 0;
		windowMaxDelta = 0;
		windowFrameCount = 0;
		windowLongCount = 0;
		return;
	}

	if ( fs->shouldRender != lastShouldRender )
	{
		Com_Printf( "VR timing: shouldRender -> %d\n", (int)fs->shouldRender );
		lastShouldRender = fs->shouldRender;
	}

	delta = fs->predictedDisplayTime - lastDisplayTime;
	lastDisplayTime = fs->predictedDisplayTime;
	windowPeriod = fs->predictedDisplayPeriod;

	windowAccumTime += delta;
	windowFrameCount++;
	if ( delta > windowMaxDelta )
	{
		windowMaxDelta = delta;
	}
	if ( windowPeriod > 0 && delta > ( windowPeriod + windowPeriod / 2 ) )
	{
		windowLongCount++;
	}

	if ( windowAccumTime >= 1000000000LL )
	{
		Com_Printf( "VR timing: %d frames, period %.2fms, avg %.2fms, max %.2fms, long(>1.5x) %d\n",
			windowFrameCount,
			(double)windowPeriod / 1000000.0,
			( (double)windowAccumTime / (double)windowFrameCount ) / 1000000.0,
			(double)windowMaxDelta / 1000000.0,
			windowLongCount );

		windowAccumTime = 0;
		windowMaxDelta = 0;
		windowFrameCount = 0;
		windowLongCount = 0;
	}
}

// SteamVR lists only its configured rate and refuses requests while the panel runs another; the
// menus show the entry nearest the request, so the list must carry the rate the panel actually runs
static void VR_OfferMeasuredRate( XrDuration period )
{
	static XrDuration lastPeriod = 0;
	static int lastListModification = -1;
	static cvar_t *rates;
	char list[512];

	if ( !rates )
		rates = Cvar_Get( "vr_refreshrates", "", CVAR_ROM );
	if ( period <= 0 || ( period == lastPeriod && rates->modificationCount == lastListModification ) )
		return;
	lastPeriod = period;
	Q_strncpyz( list, rates->string, sizeof( list ) );
	if ( VR_RateListOffer( list, sizeof( list ), 1e9 / period ) )
		Cvar_Set2( "vr_refreshrates", list, qtrue );
	lastListModification = rates->modificationCount;
}

XrFrameState VR_WaitFrame(XrSession session)
{
	XrFrameWaitInfo waitFrameInfo = {};
	waitFrameInfo.type = XR_TYPE_FRAME_WAIT_INFO;
	waitFrameInfo.next = NULL;

	XrFrameState frameState = {};
	frameState.type = XR_TYPE_FRAME_STATE;
	frameState.next = NULL;

	XR_CHECK(
		xrWaitFrame(session, &waitFrameInfo, &frameState),
		"Failed to wait for XR frame");

	VR_LogFrameTiming( &frameState, vr_frameTimingLog->integer != 0 );
	VR_OfferMeasuredRate( frameState.predictedDisplayPeriod );

	return frameState;
}

void VR_BeginFrame(XrSession session)
{
	XrFrameBeginInfo beginFrameDesc = {};
	beginFrameDesc.type = XR_TYPE_FRAME_BEGIN_INFO;
	beginFrameDesc.next = NULL;
	XR_CHECK(
		xrBeginFrame(session, &beginFrameDesc),
		"Failed to begin XR frame");
}

XrViewState VR_LocateViews(XrSession session, XrTime predictedDisplayTime, XrSpace space, XrView* views, uint32_t* viewCount)
{
	XrViewLocateInfo projectionInfo = {};
	projectionInfo.type = XR_TYPE_VIEW_LOCATE_INFO;
	projectionInfo.next = NULL;
	projectionInfo.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
	projectionInfo.displayTime = predictedDisplayTime;
	projectionInfo.space = space;

	XrViewState viewState = {0};
	viewState.type = XR_TYPE_VIEW_STATE;
	viewState.next = NULL;

	views[0].type = views[1].type = XR_TYPE_VIEW;
	views[0].next = views[1].next = NULL;

	XR_CHECK(
		xrLocateViews(
			session,
			&projectionInfo,
			&viewState,
			*viewCount,
			viewCount,
			views),
		"Failed to locate XR views");
	
	return viewState;
}

void VR_EndFrame(XrSession session, VR_SwapchainInfos* swapchains, XrView* views, uint32_t viewCount, XrSpace worldSpace, XrSpace viewSpace, XrTime predictedDisplayTime)
{
	extern vr_clientinfo_t vr;

	// Scoped: submit only a head-locked quad sampling the cyclopean texture.
	// Quad layers carry a single pose (no per-view geometry for the compositor
	// to override per-eye) so the crosshair lands on the same world ray for
	// both eyes even when system overlays force the compositor into reprojection.
	if (vr.weapon_zoomed)
	{
		XrCompositionLayerQuad quad_layer = {};
		quad_layer.type = XR_TYPE_COMPOSITION_LAYER_QUAD;
		quad_layer.layerFlags = XR_COMPOSITION_LAYER_CORRECT_CHROMATIC_ABERRATION_BIT;
		quad_layer.space = viewSpace;
		quad_layer.eyeVisibility = XR_EYE_VISIBILITY_BOTH;

		quad_layer.subImage.swapchain = swapchains->color.swapchain;
		quad_layer.subImage.imageRect.extent.width = swapchains->color.width;
		quad_layer.subImage.imageRect.extent.height = swapchains->color.height;
		quad_layer.subImage.imageArrayIndex = 0;  // both array layers carry identical cyclopean pixels

		quad_layer.pose.orientation.w = 1.0f;
		quad_layer.pose.position.z = -VR_SCOPE_QUAD_DISTANCE;

		// The buffer's zoom-1 frustum at its true angles, the same width on every headset
		float halfTanH, halfTanV;
		VR_ScopeFrustum(&halfTanH, &halfTanV, swapchains->color.width, swapchains->color.height);
		quad_layer.size.width = 2.0f * halfTanH * VR_SCOPE_QUAD_DISTANCE;
		quad_layer.size.height = 2.0f * halfTanV * VR_SCOPE_QUAD_DISTANCE;

		const XrCompositionLayerBaseHeader* layers[1] = {
			(const XrCompositionLayerBaseHeader*)&quad_layer,
		};

		XrFrameEndInfo endFrameInfo = {};
		endFrameInfo.type = XR_TYPE_FRAME_END_INFO;
		endFrameInfo.displayTime = predictedDisplayTime;
		endFrameInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
		endFrameInfo.layerCount = 1;
		endFrameInfo.layers = layers;

		XR_CHECK(xrEndFrame(session, &endFrameInfo), "Failed to end XR frame");
		return;
	}

	XrCompositionLayerProjectionView projection_layer_elements[2];
	XrCompositionLayerProjection projection_layer;
	const XrCompositionLayerBaseHeader* layers[1];
	int layerCount = 0;

	// The layer names the color swapchain, and naming one that has never released an image is rejected
	if (swapchains->color.everReleased &&
		VR_BuildProjectionLayer(swapchains, views, viewCount, worldSpace, &projection_layer, projection_layer_elements))
	{
		layers[layerCount++] = (const XrCompositionLayerBaseHeader*)&projection_layer;
	}
	XrFrameEndInfo endFrameInfo = {};
	endFrameInfo.type = XR_TYPE_FRAME_END_INFO;
	endFrameInfo.displayTime = predictedDisplayTime;
	endFrameInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
	endFrameInfo.layerCount = layerCount;
	endFrameInfo.layers = layerCount > 0 ? layers : NULL;

	XR_CHECK(
		xrEndFrame(session, &endFrameInfo),
		"Failed to end XR frame");
}

qboolean VR_BuildProjectionLayer(VR_SwapchainInfos* swapchains, const XrView* views, uint32_t viewCount, XrSpace worldSpace,
	XrCompositionLayerProjection* layer, XrCompositionLayerProjectionView elements[2])
{
	uint32_t view;

	if (viewCount == 0)
	{
		return qfalse;
	}
	if (viewCount > 2)
	{
		viewCount = 2;
	}

	memset(elements, 0, 2 * sizeof(XrCompositionLayerProjectionView));
	for (view = 0; view < viewCount; view++)
	{
		elements[view].type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
		elements[view].pose = views[view].pose;
		elements[view].fov = views[view].fov;
		elements[view].subImage.swapchain = swapchains->color.swapchain;
		elements[view].subImage.imageRect.extent.width = swapchains->color.width;
		elements[view].subImage.imageRect.extent.height = swapchains->color.height;
		elements[view].subImage.imageArrayIndex = view;
	}

	memset(layer, 0, sizeof(*layer));
	layer->type = XR_TYPE_COMPOSITION_LAYER_PROJECTION;
	layer->layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT | XR_COMPOSITION_LAYER_CORRECT_CHROMATIC_ABERRATION_BIT;
	layer->space = worldSpace;
	layer->viewCount = viewCount;
	layer->views = elements;
	return qtrue;
}
