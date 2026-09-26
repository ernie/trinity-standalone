#include "vr_render_loop.h"

#include <string.h>
#include <math.h>

#include "../qcommon/qcommon.h"
#include "vr_macros.h"
#include "vr_clientinfo.h"
#include "vr_rate_list.h"
#include "vr_gameplay.h"
#include "vr_cvars.h"
#include "vr_base.h"
#include "../vrvk/vr_vk_types.h"
#include "common/xr_linear.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static inline float radians(float degrees) {
	return degrees * (float)M_PI / 180.0f;
}

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
	extern cvar_t* vr_screenCurvature;

	// Check if we should render to virtual screen (menus, spectator mode)
	qboolean useVirtualScreen = VR_Gameplay_ShouldRenderInVirtualScreen();

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

	XrCompositionLayerProjectionView projection_layer_elements[2] = {};
	for (uint32_t view = 0; view < viewCount; view++)
	{
		memset(&projection_layer_elements[view], 0, sizeof(XrCompositionLayerProjectionView));
		projection_layer_elements[view].type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
		projection_layer_elements[view].pose = views[view].pose;
		projection_layer_elements[view].fov = views[view].fov;

		memset(&projection_layer_elements[view].subImage, 0, sizeof(XrSwapchainSubImage));
		projection_layer_elements[view].subImage.swapchain = swapchains->color.swapchain;
		projection_layer_elements[view].subImage.imageRect.offset.x = 0;
		projection_layer_elements[view].subImage.imageRect.offset.y = 0;
		projection_layer_elements[view].subImage.imageRect.extent.width = swapchains->color.width;
		projection_layer_elements[view].subImage.imageRect.extent.height = swapchains->color.height;
		projection_layer_elements[view].subImage.imageArrayIndex = view;
	}

	XrCompositionLayerProjection projection_layer = {};
	projection_layer.type = XR_TYPE_COMPOSITION_LAYER_PROJECTION;
	projection_layer.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
	projection_layer.layerFlags |= XR_COMPOSITION_LAYER_CORRECT_CHROMATIC_ABERRATION_BIT;
	projection_layer.space = worldSpace;
	projection_layer.viewCount = viewCount;
	projection_layer.views = projection_layer_elements;

	// Virtual screen layer (menus, spectator mode)
	VR_ScreenLayer screen_layer;
	qboolean haveScreen = useVirtualScreen &&
		VR_BuildVirtualScreenLayer(swapchains, views, viewCount, worldSpace, &screen_layer);

	// Submit layers
	const XrCompositionLayerBaseHeader* layers[2];
	int layerCount = 0;

	// Both layers name the color swapchain, and naming one that has never released an image is rejected
	if (!swapchains->color.everReleased)
	{
		layerCount = 0;
	}
	else if (haveScreen)
	{
		// Virtual screen mode: use the screen layer instead of projection
		layers[layerCount++] = &screen_layer.header;
	}
	else
	{
		// Normal gameplay: use projection layer
		if (viewCount > 0)
		{
			layers[layerCount++] = (const XrCompositionLayerBaseHeader*)&projection_layer;
		}
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

/*
==================
VR_BuildVirtualScreenLayer

The cylinder showing the color swapchain's centered 4:3 crop as the virtual screen
(menus, loading, spectating). The loading pump submits it too, showing the last
released image.
==================
*/
qboolean VR_BuildVirtualScreenLayer(VR_SwapchainInfos* swapchains, const XrView* views, uint32_t viewCount, XrSpace worldSpace, VR_ScreenLayer* out)
{
	extern vr_clientinfo_t vr;
	extern cvar_t* vr_screenCurvature;
	XrCompositionLayerCylinderKHR cylinder_layer = {};

	if (viewCount == 0)
	{
		return qfalse;
	}
	{
		int width = swapchains->color.width;
		int height = swapchains->color.height;

		cylinder_layer.type = XR_TYPE_COMPOSITION_LAYER_CYLINDER_KHR;
		cylinder_layer.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
		cylinder_layer.space = worldSpace;
		cylinder_layer.eyeVisibility = XR_EYE_VISIBILITY_BOTH;

		// The client draws the virtual screen into a centred 4:3 region of the
		// eye buffer: 2D is scaled to that region and the scene is rendered with
		// a symmetric FOV to be cropped down to it. Sample that region rather
		// than the whole buffer, or the full framebuffer is squished onto a 4:3
		// surface. The cylinder samples the sub-rect directly, so no blit.
		int srcWidth, srcHeight, srcX, srcY;
		int heightFromWidth = (width * 3) / 4;	// 4:3 height using the full width
		int widthFromHeight = (height * 4) / 3;	// 4:3 width using the full height

		if (heightFromWidth <= height)
		{
			// Width-limited: full width fits with a 4:3 height
			srcWidth = width;
			srcHeight = heightFromWidth;
			srcX = 0;
		}
		else
		{
			// Height-limited (very wide buffers): constrain width to fit 4:3
			srcHeight = height;
			srcWidth = widthFromHeight;
			srcX = (width - srcWidth) / 2;
		}

		// Centre the crop on the optical rather than the geometric centre: the
		// headset's FOV is asymmetric (more down-look than up-look) and the
		// client offsets its 2D by the same amount, so a geometric crop would
		// shear the two apart. Y grows downward here, so the sign is not
		// flipped the way a GL blit would need.
		srcY = (height - srcHeight) / 2;
		{
			float tanUp = tanf(vr.fov_angle_up);
			float tanDown = tanf(vr.fov_angle_down);
			float tanHeight = tanUp - tanDown;

			if (fabsf(tanHeight) > 0.001f)
			{
				float m9 = (tanUp + tanDown) / tanHeight;
				srcY += (int)( 240.0f * m9 * (srcHeight / 480.0f) );
			}
		}
		if (srcY < 0)
			srcY = 0;
		if (srcY > height - srcHeight)
			srcY = height - srcHeight;

		memset(&cylinder_layer.subImage, 0, sizeof(XrSwapchainSubImage));
		cylinder_layer.subImage.swapchain = swapchains->color.swapchain;
		cylinder_layer.subImage.imageRect.offset.x = srcX;
		cylinder_layer.subImage.imageRect.offset.y = srcY;
		cylinder_layer.subImage.imageRect.extent.width = srcWidth;
		cylinder_layer.subImage.imageRect.extent.height = srcHeight;
		cylinder_layer.subImage.imageArrayIndex = 0;  // Cylinder uses single image, not array

		// Position cylinder in front of player at menuYaw direction
		const XrVector3f axis = {0.0f, 1.0f, 0.0f};

		// Screen curvature: 1.0 = very curved, 0.0 = nearly flat
		float curvature = vr_screenCurvature ? vr_screenCurvature->value : 0.5f;
		const float refRadius = 8.0f;
		const float refCentralAngle = (float)M_PI * 0.5f;
		const float arcLength = refRadius * refCentralAngle;
		const float refDistance = 4.0f;

		// Vary radius, adjust centralAngle to keep arc length constant,
		// and adjust distance to keep surface at same position
		float radius = 4.0f + (1.0f - curvature) * 12.0f;
		float centralAngle = arcLength / radius;
		float axisDistance = refDistance + (refRadius - radius);

		XrVector3f pos = {
			views[0].pose.position.x - sinf(radians(vr.menuYaw)) * axisDistance,
			-0.25f,
			views[0].pose.position.z - cosf(radians(vr.menuYaw)) * axisDistance
		};
		cylinder_layer.pose.position = pos;
		XrQuaternionf_CreateFromAxisAngle(&cylinder_layer.pose.orientation, &axis, radians(vr.menuYaw));

		cylinder_layer.radius = radius;
		cylinder_layer.centralAngle = centralAngle;
		// The crop is 4:3 in both pixels and angle; the eye-buffer aspect would stretch the screen
		cylinder_layer.aspectRatio = (float)srcWidth / (float)srcHeight;

		if (!VR_HasCylinderLayers())
		{
			// Flat stand-in at the arc's midpoint, which sits radius past the cylinder's axis
			XrCompositionLayerQuad quad_layer = {};
			float surfaceDistance = axisDistance + radius;

			quad_layer.type = XR_TYPE_COMPOSITION_LAYER_QUAD;
			quad_layer.layerFlags = cylinder_layer.layerFlags;
			quad_layer.space = worldSpace;
			quad_layer.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
			quad_layer.subImage = cylinder_layer.subImage;
			quad_layer.pose.orientation = cylinder_layer.pose.orientation;
			quad_layer.pose.position.x = views[0].pose.position.x - sinf(radians(vr.menuYaw)) * surfaceDistance;
			quad_layer.pose.position.y = -0.25f;
			quad_layer.pose.position.z = views[0].pose.position.z - cosf(radians(vr.menuYaw)) * surfaceDistance;
			quad_layer.size.width = arcLength;
			quad_layer.size.height = arcLength / cylinder_layer.aspectRatio;
			out->quad = quad_layer;
			return qtrue;
		}
	}

	out->cylinder = cylinder_layer;
	return qtrue;
}
