/*
 * vr_vk_renderer.c - Vulkan VR renderer implementation
 *
 * Implements the VR renderer exports for the Vulkan backend.
 * This file handles the OpenXR frame lifecycle and coordinates
 * with the Vulkan command buffer system.
 */

#include "../vrcommon/vr_renderer.h"

#include <math.h>

#include "../client/client.h"

#include "../vrcommon/common/xr_linear.h"
#include "../vrcommon/vr_base.h"
#include "../vrcommon/vr_clientinfo.h"
#include "../vrcommon/vr_events.h"
#include "../vrcommon/vr_gameplay.h"
#include "../vrcommon/vr_input.h"
#include "../vrcommon/vr_macros.h"
#include "../vrcommon/vr_math.h"
#include "../vrcommon/vr_render_loop.h"
#include "../vrcommon/vr_spaces.h"
#include "../vrcommon/vr_swapchains.h"
#include "../vrcommon/vr_types.h"

// Vulkan-specific headers
#include "vr_vk.h"
#include "vr_vk_debug.h"
#include "vr_vk_foveation.h"
#include "vr_vk_loading.h"
#include "vr_vk_swapchains.h"

extern vr_clientinfo_t vr;
extern cvar_t *vr_heightAdjust;
extern cvar_t *vr_refreshrate;
extern cvar_t *vr_refreshrates;
extern cvar_t *vr_desktopMode;
extern cvar_t *vr_virtualScreenMode;

const float hudScale = M_PI * 15.0f / 180.0f;

XrBool32 stageSupported = XR_FALSE;
XrTime lastPredictedDisplayTime = 0;
qboolean frameStarted = qfalse;
qboolean needRecenter = qtrue;
qboolean fullscreenMode = qfalse;
qboolean menuYawTracking = qfalse;

// Per-frame data held between BeginFrame and EndFrame
XrFovf fov = { 0 };
XrView views[2];
uint32_t viewCount = 2;
uint32_t swapchainColorIndex = 0;
uint32_t swapchainDepthIndex = 0;

// Forward declarations
void VR_Renderer_BeginFrame(VR_Engine* engine, XrBool32 needsRecenter);
void VR_Renderer_EndFrame(VR_Engine* engine);
void VR_Recenter(VR_Engine* engine, XrTime predictedDisplayTime);
void VR_ClearFrameBuffer(int width, int height);
void VR_UpdatePerFrameState(void);

/*
==================
ConvertToReversedDepth

Quake3e/renderervk uses reversed depth (near=1.0, far=0.0) for better depth precision.
OpenXR's XrMatrix4x4f_CreateProjectionFov produces standard Vulkan depth (near=0.0, far=1.0).
This function converts a standard projection matrix to reversed depth.

For an infinite far plane (which OpenXR uses when farZ <= nearZ):
  Standard:  m[10] = -1,     m[14] = -near
  Reversed:  m[10] =  0,     m[14] =  near
==================
*/
static void ConvertToReversedDepth(XrMatrix4x4f* matrix)
{
	// For infinite projection (standard Vulkan):
	//   m[10] = -1.0, m[14] = -nearZ
	// For reversed depth infinite projection:
	//   m[10] = 0.0,  m[14] = nearZ
	//
	// The conversion: m[10] = m[10] + 1.0, m[14] = -m[14]
	matrix->m[10] = matrix->m[10] + 1.0f;  // -1 -> 0
	matrix->m[14] = -matrix->m[14];         // -near -> near
}
XrDesktopViewConfiguration VR_GetDesktopViewConfiguration(void);


void VR_GetResolution(VR_Engine* engine, int *pWidth, int *pHeight)
{
	VR_GetSupersampledResolution(engine->appState.Instance, engine->appState.SystemId, pWidth, pHeight);
}


// Rate asked of the runtime but not yet confirmed by its change event
static float s_requestedRefreshRate = 0.0f;

/*
==================
VR_EnumerateRefreshRates

Publishes the runtime's rates in vr_refreshrates so the UI offers only those.
==================
*/
static void VR_EnumerateRefreshRates(VR_Engine* engine)
{
	PFN_xrEnumerateDisplayRefreshRatesFB xrEnumerateDisplayRefreshRatesFB = NULL;
	VR_Renderer* renderer = &engine->appState.Renderer;
	char list[256];
	uint32_t count = 0;
	uint32_t i;

	renderer->NumSupportedRefreshRates = 0;
	list[0] = '\0';

	xrGetInstanceProcAddr(engine->appState.Instance, "xrEnumerateDisplayRefreshRatesFB", (PFN_xrVoidFunction*)(&xrEnumerateDisplayRefreshRatesFB));
	if (xrEnumerateDisplayRefreshRatesFB &&
		XR_SUCCEEDED(xrEnumerateDisplayRefreshRatesFB(engine->appState.Session, 0, &count, NULL)) && count > 0)
	{
		if (count > VR_MAX_REFRESH_RATES)
		{
			count = VR_MAX_REFRESH_RATES;
		}
		if (XR_SUCCEEDED(xrEnumerateDisplayRefreshRatesFB(engine->appState.Session, count, &count, renderer->SupportedRefreshRates)))
		{
			renderer->NumSupportedRefreshRates = count;
		}
	}

	for (i = 0; i < renderer->NumSupportedRefreshRates; i++)
	{
		Q_strcat(list, sizeof(list), va("%s%g", i ? " " : "", renderer->SupportedRefreshRates[i]));
	}
	Cvar_Set2("vr_refreshrates", list, qtrue);
	Com_Printf("Supported display refresh rates: %s\n", list[0] ? list : "(not reported)");
}

// Nearest supported rate, or the rate itself when the runtime reported none
static float VR_SnapRefreshRate(const VR_Engine* engine, float rate)
{
	const VR_Renderer* renderer = &engine->appState.Renderer;
	float best = rate;
	float bestDiff = -1.0f;
	uint32_t i;

	for (i = 0; i < renderer->NumSupportedRefreshRates; i++)
	{
		const float diff = fabsf(renderer->SupportedRefreshRates[i] - rate);
		if (bestDiff < 0.0f || diff < bestDiff)
		{
			bestDiff = diff;
			best = renderer->SupportedRefreshRates[i];
		}
	}
	return best;
}

static void VR_RequestRefreshRate(VR_Engine* engine, float rate)
{
	PFN_xrRequestDisplayRefreshRateFB xrRequestDisplayRefreshRateFB;
	XrResult result;

	XR_CHECK(
		xrGetInstanceProcAddr(engine->appState.Instance, "xrRequestDisplayRefreshRateFB", (PFN_xrVoidFunction*)(&xrRequestDisplayRefreshRateFB)),
		"failed to get xrRequestDisplayRefreshRateFB func proc");

	Com_Printf("Requesting display refresh rate: %g\n", rate);
	result = xrRequestDisplayRefreshRateFB(engine->appState.Session, rate);
	if (result == XR_SUCCESS)
	{
		// Confirmed later by XR_TYPE_EVENT_DATA_DISPLAY_REFRESH_RATE_CHANGED_FB
		s_requestedRefreshRate = rate;
	}
	else
	{
		Com_Printf("Refresh rate %g refused by the runtime (%d), staying at %g\n", rate, (int)result, engine->appState.Renderer.RefreshRate);
		Cvar_SetValue("vr_refreshrate", engine->appState.Renderer.RefreshRate);
		vr_refreshrate->modified = qfalse;
		s_requestedRefreshRate = 0.0f;
	}
}

/*
==================
VR_ApplyRefreshRate

Runs at init and on every cvar change, so the setting applies live without a video restart.
==================
*/
static void VR_ApplyRefreshRate(VR_Engine* engine)
{
	const float current = engine->appState.Renderer.RefreshRate;
	float desired, snapped;

	vr_refreshrate->modified = qfalse;

	desired = vr_refreshrate->value;
	if (desired <= 0.0f)
	{
		Cvar_SetValue("vr_refreshrate", current);
		vr_refreshrate->modified = qfalse;
		return;
	}

	snapped = VR_SnapRefreshRate(engine, desired);
	if (snapped != desired)
	{
		Com_Printf("Refresh rate %g is not supported, using %g\n", desired, snapped);
		Cvar_SetValue("vr_refreshrate", snapped);
		vr_refreshrate->modified = qfalse;
	}

	if (snapped == current)
	{
		s_requestedRefreshRate = 0.0f;
		return;
	}
	if (snapped == s_requestedRefreshRate)
	{
		return; // already asked; waiting for the runtime to confirm
	}
	VR_RequestRefreshRate(engine, snapped);
}

void VR_InitRenderer(VR_Engine* engine)
{
	VR_VK_RegisterDebugCallbackIfEnabled();

	// Get and set the display refresh rate
	{
		PFN_xrGetDisplayRefreshRateFB xrGetDisplayRefreshRateFB;
		XR_CHECK(
			xrGetInstanceProcAddr(engine->appState.Instance, "xrGetDisplayRefreshRateFB", (PFN_xrVoidFunction*)(&xrGetDisplayRefreshRateFB)),
			"failed to get xrGetDisplayRefreshRateFB func proc");

		engine->appState.Renderer.RefreshRate = 0.0f;
		XR_CHECK(
			xrGetDisplayRefreshRateFB(engine->appState.Session, &engine->appState.Renderer.RefreshRate),
			"failed to get current display refresh rate");
		Com_Printf("Current System Display Refresh Rate: %f\n", engine->appState.Renderer.RefreshRate);

		VR_EnumerateRefreshRates(engine);
		s_requestedRefreshRate = 0.0f;
		VR_ApplyRefreshRate(engine);
	}

	stageSupported = VR_IsStageSpaceSupported(engine->appState.Session);

	if (engine->appState.CurrentSpace == XR_NULL_HANDLE)
	{
		XrTime nullTime = 0; // won't be used anyway
		VR_Recenter(engine, nullTime);
	}

	// Create VIEW reference space for head-locked quad layers
	{
		XrReferenceSpaceCreateInfo viewSpaceCI = {0};
		viewSpaceCI.type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO;
		viewSpaceCI.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
		viewSpaceCI.poseInReferenceSpace.orientation.w = 1.0f;  // Identity
		XR_CHECK(
			xrCreateReferenceSpace(engine->appState.Session, &viewSpaceCI, &engine->appState.ViewSpace),
			"Failed to create VIEW reference space for quad layer");
		Com_Printf("Created VIEW reference space for head-locked quad layers\n");
	}

	// Create Vulkan XR swapchains
	engine->appState.Renderer.Swapchains = VR_VK_CreateSwapchains(
		engine->appState.Instance,
		engine->appState.SystemId,
		engine->appState.Session);

	// Initialize renderer XR resources (VkImageViews, VkFramebuffers)
	// Renderer pulls swapchain info via ri.VR_Vulkan_GetSwapchainInfo()
	if (!re.InitXRResources()) {
		Com_Printf("[VR Vulkan] Warning: Failed to initialize XR resources\n");
	}
}


void VR_DestroyRenderer(VR_Engine* engine)
{
	VR_Loading_Shutdown();
	VR_VK_DestroySwapchains(&engine->appState.Renderer.Swapchains);

	// Destroy VIEW reference space
	if (engine->appState.ViewSpace != XR_NULL_HANDLE)
	{
		xrDestroySpace(engine->appState.ViewSpace);
		engine->appState.ViewSpace = XR_NULL_HANDLE;
	}
}


void VR_ProcessFrame(VR_Engine* engine)
{
	// Handle deferred swapchain recreation from vid_restart.
	// This MUST happen before xrBeginFrame (called in VR_Renderer_BeginFrame).
	// If swapchains were marked for recreation during the previous frame's Com_Frame,
	// we destroy and recreate them here at a safe point outside the XR frame lifecycle.
	VR_VK_Swapchains_HandlePendingRecreate(engine);

	const XrBool32 needsRecenter = VR_ProcessXrEvents(&engine->appState);

	if (engine->appState.SessionActive == VR_FALSE)
	{
		// If we haven't called Com_Frame() then let's at least process input
		// (specifically SDL events) so that app won't appear as stuck/deadlocked
		IN_Frame();
		return;
	}

	// A menu pick applies live, no video restart
	if (vr_refreshrate->modified)
	{
		VR_ApplyRefreshRate(engine);
	}

	VR_VK_Foveation_Frame(engine);

	VR_Renderer_BeginFrame(engine, needsRecenter);
	Com_Frame();
	VR_Renderer_EndFrame(engine);

	if (needRecenter)
	{
		VR_Recenter(engine, lastPredictedDisplayTime);
		needRecenter = qfalse;
	}
}


void VR_Renderer_RestoreState(VR_Engine* engine)
{
	if (!frameStarted)
	{
		// Frame hasn't started, no need to restore anything here
		return;
	}

	VR_UpdatePerFrameState();

	// If we need to re-start frame until `Com_Frame()` call, we need session to
	// be active to proceed
	XrBool32 needsRecenter = XR_FALSE;
	while (!engine->appState.SessionActive)
	{
		needsRecenter |= VR_ProcessXrEvents(&engine->appState);
	}

	VR_Renderer_BeginFrame(engine, needsRecenter);
}


void VR_Renderer_BeginFrame(VR_Engine* engine, XrBool32 needsRecenter)
{
	// Check if swapchains exist (they might have been destroyed by vid_restart)
	VR_SwapchainInfos* swapchains = engine->appState.Renderer.Swapchains;
	if (!swapchains) {
		// Swapchains were destroyed (vid_restart in progress), skip this frame
		return;
	}

	frameStarted = qtrue;

	lastPredictedDisplayTime = VR_WaitFrame(engine->appState.Session).predictedDisplayTime;

	if (needsRecenter)
	{
		VR_Recenter(engine, lastPredictedDisplayTime);
	}

	VR_BeginFrame(engine->appState.Session);

	const XrViewState viewState = VR_LocateViews(
		engine->appState.Session,
		lastPredictedDisplayTime,
		engine->appState.CurrentSpace,
		views,
		&viewCount);

	// Update HMD position/views
	IN_VRUpdateHMD(views, viewCount, &fov);

	// SP intermission state tracking: must be set before rendering
	// so UI code sees the correct state for scaling/offsets
	qboolean isSPIntermission = VR_IsSPIntermission();
	if (isSPIntermission && !vr.sp_intermission_active)
	{
		// First frame of SP intermission: capture anchor position
		vr.sp_intermission_active = qtrue;
		// Store yaw for HUD positioning (in degrees)
		XrQuaternionf q = views[0].pose.orientation;
		float siny_cosp = 2.0f * (q.w * q.y + q.z * q.x);
		float cosy_cosp = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);
		vr.sp_intermission_yaw = atan2f(siny_cosp, cosy_cosp) * 180.0f / (float)M_PI;
	}
	else if (!isSPIntermission && vr.sp_intermission_active)
	{
		// Exiting SP intermission: reset state
		vr.sp_intermission_active = qfalse;
	}

	// [Input] poll actions, update controller state, issue action commands
	IN_VRSyncActions(engine);
	IN_VRUpdateControllers(engine, lastPredictedDisplayTime);

	// Update zoom level after input processing so weapon_zoomLevel
	// matches weapon_zoomed (set during IN_VRUpdateControllers)
	VR_UpdatePerFrameState();

	// Images are acquired only when something is about to be drawn, so the compositor shows the last released image, not a held one

	// Set renderer params
	// Near plane must be in Quake units to match our view matrices
	float nearPlane = 4.0f;  // Match r_znear default in Quake units

	// Use Vulkan conventions for projection matrix (Y-down, Z in [0,1])
	const GraphicsAPI graphicsApi = GRAPHICS_VULKAN;

	XrMatrix4x4f vrMatrixMono, vrMatrixProjection;
	const XrFovf monoFov = { -hudScale, hudScale, hudScale, -hudScale };
	XrFovf projectionFov;
	if (vr.weapon_zoomed)
	{
		// Scope: the view the quad in VR_EndFrame shows, from the same frustum; zoom narrows the angle
		float halfTanH, halfTanV;
		VR_ScopeFrustum(&halfTanH, &halfTanV, swapchains->color.width, swapchains->color.height);
		float tanH = tanf(atanf(halfTanH) / vr.weapon_zoomLevel);
		float tanV = tanH * (float)swapchains->color.height / (float)swapchains->color.width;
		projectionFov.angleLeft = -atanf(tanH);
		projectionFov.angleRight = atanf(tanH);
		projectionFov.angleUp = atanf(tanV);
		projectionFov.angleDown = -atanf(tanV);
	}
	else
	{
		projectionFov.angleLeft = fov.angleLeft / vr.weapon_zoomLevel;
		projectionFov.angleRight = fov.angleRight / vr.weapon_zoomLevel;
		projectionFov.angleUp = fov.angleUp / vr.weapon_zoomLevel;
		projectionFov.angleDown = fov.angleDown / vr.weapon_zoomLevel;
	}
	XrMatrix4x4f_CreateProjectionFov(&vrMatrixMono, graphicsApi, monoFov, nearPlane, 0.0f);
	XrMatrix4x4f_CreateProjectionFov(&vrMatrixProjection, graphicsApi, projectionFov, nearPlane, 0.0f);

	// Create per-eye projection matrices from actual OpenXR FOVs
	XrMatrix4x4f vrMatrixEye[2];
	for (int eye = 0; eye < 2 && eye < (int)viewCount; eye++)
	{
		XrFovf eyeFov = {
			views[eye].fov.angleLeft / vr.weapon_zoomLevel,
			views[eye].fov.angleRight / vr.weapon_zoomLevel,
			views[eye].fov.angleUp / vr.weapon_zoomLevel,
			views[eye].fov.angleDown / vr.weapon_zoomLevel,
		};
		XrMatrix4x4f_CreateProjectionFov(&vrMatrixEye[eye], graphicsApi, eyeFov, nearPlane, 0.0f);
	}

	// Convert to reversed depth (near=1.0, far=0.0) for Quake3e's depth precision
	ConvertToReversedDepth(&vrMatrixMono);
	ConvertToReversedDepth(&vrMatrixProjection);
	ConvertToReversedDepth(&vrMatrixEye[0]);
	ConvertToReversedDepth(&vrMatrixEye[1]);

	// Compute combined stereo horizontal FOV for culling
	float combinedAngleLeft = views[0].fov.angleLeft / vr.weapon_zoomLevel;
	float combinedAngleRight = views[1].fov.angleRight / vr.weapon_zoomLevel;
	float combinedFovX = (fabsf(combinedAngleLeft) + fabsf(combinedAngleRight)) * 180.0f / M_PI;
	// Canted displays: each eye's FOV is centered on its own yawed axis
	combinedFovX += (fabsf(vr.eyeCantYaw[0]) + fabsf(vr.eyeCantYaw[1])) * 180.0f / M_PI;
	// Up and down may differ, so each vertical plane gets its own angle
	float fovUp = fov.angleUp / vr.weapon_zoomLevel * 180.0f / M_PI;
	float fovDown = fabsf(fov.angleDown) / vr.weapon_zoomLevel * 180.0f / M_PI;
	if (vr.weapon_zoomed)
	{
		// Cull to whichever is wider, the eyes or the scope's fixed frustum
		float scopeFovX = 2.0f * projectionFov.angleRight * 180.0f / M_PI;
		float scopeFovV = projectionFov.angleUp * 180.0f / M_PI;
		if (scopeFovX > combinedFovX)
			combinedFovX = scopeFovX;
		if (scopeFovV > fovUp)
			fovUp = scopeFovV;
		if (scopeFovV > fovDown)
			fovDown = scopeFovV;
	}

	// Calculate half-IPD in meters for frustum plane offset
	float halfIpdMeters = 0.0f;
	if (viewCount >= 2) {
		float dx = views[1].pose.position.x - views[0].pose.position.x;
		float dy = views[1].pose.position.y - views[0].pose.position.y;
		float dz = views[1].pose.position.z - views[0].pose.position.z;
		halfIpdMeters = sqrtf(dx*dx + dy*dy + dz*dz) * 0.5f;
	}

	re.SetVRHeadsetParms(vrMatrixProjection.m, vrMatrixMono.m, 0, // renderBuffer not used for VK
						 vrMatrixEye[0].m, vrMatrixEye[1].m, combinedFovX, fovUp, fovDown, halfIpdMeters);
}


void VR_Renderer_EndFrame(VR_Engine* engine)
{
	// A load is over once the main thread submits gameplay (or menu) frames again
	if (VR_Loading_Active() && (clc.state == CA_ACTIVE || clc.state == CA_CINEMATIC ||
		clc.state == CA_DISCONNECTED || clc.state == CA_UNINITIALIZED ||
		!VR_Gameplay_ShouldRenderInVirtualScreen()))
	{
		VR_Loading_Stop();
	}

	// If frame was already finished (e.g., by VR_Renderer_FinishFrame during vid_restart),
	// don't try to end it again
	if (!frameStarted) {
		return;
	}

	VR_SwapchainInfos* swapchains = engine->appState.Renderer.Swapchains;

	// Draw Virtual Screen if needed
	const int use_virtual_screen = VR_Gameplay_ShouldRenderInVirtualScreen();
	if (use_virtual_screen)
	{
		// Capture menuYaw on the first frame of the window, and re-capture
		// when the client state changes mid-window (e.g. a map load
		// beginning) so the screen appears where the player is facing
		if ((!fullscreenMode || VR_Gameplay_VirtualScreenContextChanged()) && !vr.menuYawLocked) {
			vr.menuYaw = vr.hmdorientation[YAW];
		}
		fullscreenMode = qtrue;

		// Follow mode: re-face the cylinder when the head yaw drifts far,
		// with hysteresis so it settles instead of chattering. Angular
		// thresholds are the chord-angle equivalents of the PC ladder's
		// distance ratios (settle ~2.3 deg, re-target 35 deg, snap 70 deg);
		// drift is 1%/frame of the remaining angle.
		if (vr_virtualScreenMode && vr_virtualScreenMode->integer == 1 && !vr.menuYawLocked)
		{
			float yawDelta = AngleSubtract(vr.hmdorientation[YAW], vr.menuYaw);
			float absDelta = fabsf(yawDelta);

			if (absDelta < 2.3f)
			{
				menuYawTracking = qfalse;
			}
			else if (absDelta > 35.0f || menuYawTracking)
			{
				menuYawTracking = qtrue;
				if (absDelta > 70.0f)
				{
					// Too far: snap; we probably just started or switched into the virtual screen
					vr.menuYaw = vr.hmdorientation[YAW];
				}
				else
				{
					vr.menuYaw += yawDelta * 0.01f;
				}
			}
		}
		else
		{
			menuYawTracking = qfalse;
		}
	}
	else
	{
		if (!vr.menuYawLocked) {
			vr.menuYaw = vr.hmdorientation[YAW];
		}
		fullscreenMode = qfalse;
	}

	// NOTE: Do NOT call re.WaitForRenderComplete() here!
	// The OpenXR runtime handles synchronization internally via xrReleaseSwapchainImage.
	// Waiting for our fence here causes GPU hangs on Quest because the fence never signals
	// when XR swapchains are involved. The old working version (commit 23f6c08d) did not
	// wait for fences before releasing swapchains.

	// Release the images this frame drew into, if it drew at all
	if (swapchains->color.acquired)
	{
		VR_VK_Swapchains_Release(swapchains);
	}

	// Submit layers to OpenXR
	VR_EndFrame(
		engine->appState.Session,
		swapchains,
		views,
		viewCount,
		engine->appState.CurrentSpace,
		engine->appState.ViewSpace,
		lastPredictedDisplayTime);
	VR_Loading_NoteMainFrame();

	frameStarted = qfalse;
}


void VR_Renderer_FinishFrame(VR_Engine* engine)
{
	VR_Loading_Stop();

	// If no frame is in progress, nothing to do
	if (!frameStarted) {
		return;
	}

	VR_SwapchainInfos* swapchains = engine->appState.Renderer.Swapchains;
	if (!swapchains) {
		frameStarted = qfalse;
		return;
	}

	// Release main swapchains (color and depth)
	if (swapchains->color.acquired || swapchains->depth.acquired) {
		VR_VK_Swapchains_Release(swapchains);
	}

	// End the XR frame with empty layers (we're shutting down, don't care about display)
	XrFrameEndInfo endFrameInfo = {};
	endFrameInfo.type = XR_TYPE_FRAME_END_INFO;
	endFrameInfo.displayTime = lastPredictedDisplayTime;
	endFrameInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
	endFrameInfo.layerCount = 0;
	endFrameInfo.layers = NULL;

	xrEndFrame(engine->appState.Session, &endFrameInfo);

	frameStarted = qfalse;
}


/*
==================
VR_LocateHeadInStage

The head in the runtime's own stage frame. VR_Recenter bakes it into the new
space's pose so the origin sits under the head: cameras that offset from an
absolute vr.hmdposition would otherwise pivot on the runtime's anchor.
==================
*/
static qboolean VR_LocateHeadInStage(VR_Engine* engine, XrTime predictedDisplayTime, XrVector3f* offset)
{
	XrReferenceSpaceCreateInfo rawStageCreateInfo = {0};
	XrSpaceLocation loc = {0};
	XrSpace rawStageSpace = XR_NULL_HANDLE;
	qboolean located = qfalse;

	// Identity pose: the offset belongs in the frame the space is created in
	rawStageCreateInfo.type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO;
	rawStageCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
	rawStageCreateInfo.poseInReferenceSpace.orientation.w = 1.0f;
	XR_CHECK(
		xrCreateReferenceSpace(engine->appState.Session, &rawStageCreateInfo, &rawStageSpace),
		"Failed to create reference space (raw stage)");

	loc.type = XR_TYPE_SPACE_LOCATION;
	XR_CHECK(
		xrLocateSpace(engine->appState.HeadSpace, rawStageSpace, predictedDisplayTime, &loc),
		"Failed to locate head in raw stage space");
	if (loc.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT)
	{
		offset->x = loc.pose.position.x;
		offset->y = 0.0f;	// STAGE already floors at y = 0
		offset->z = loc.pose.position.z;
		located = qtrue;
	}

	XR_CHECK(
		xrDestroySpace(rawStageSpace),
		"Failed to destroy raw stage space");

	return located;
}


void VR_Recenter(VR_Engine* engine, XrTime predictedDisplayTime)
{
	// Calculate recenter reference
	XrReferenceSpaceCreateInfo spaceCreateInfo = {0};
	XrVector3f stageOffset = {0};
	qboolean haveStageOffset = qfalse;
	spaceCreateInfo.type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO;
	spaceCreateInfo.poseInReferenceSpace.orientation.w = 1.0f;
	if (engine->appState.CurrentSpace != XR_NULL_HANDLE)
	{
		vec3_t rotation = {0, 0, 0};
		XrSpaceLocation loc = {0};
		loc.type = XR_TYPE_SPACE_LOCATION;
		XR_CHECK(
			xrLocateSpace(engine->appState.HeadSpace, engine->appState.CurrentSpace, predictedDisplayTime, &loc),
			"Failed to locate space");
		QuatToYawPitchRoll(loc.pose.orientation, rotation, vr.hmdorientation);

		vr.recenterYaw += DegreesToRadians(vr.hmdorientation[YAW]);
		spaceCreateInfo.poseInReferenceSpace.orientation.x = 0;
		spaceCreateInfo.poseInReferenceSpace.orientation.y = sin(vr.recenterYaw / 2);
		spaceCreateInfo.poseInReferenceSpace.orientation.z = 0;
		spaceCreateInfo.poseInReferenceSpace.orientation.w = cos(vr.recenterYaw / 2);

		if (stageSupported)
		{
			haveStageOffset = VR_LocateHeadInStage(engine, predictedDisplayTime, &stageOffset);
		}
	}

	// Delete previous space instances
	if (engine->appState.StageSpace != XR_NULL_HANDLE)
	{
		XR_CHECK(
			xrDestroySpace(engine->appState.StageSpace),
			"Failed to destroy stage space");
	}
	if (engine->appState.FakeStageSpace != XR_NULL_HANDLE)
	{
		XR_CHECK(
			xrDestroySpace(engine->appState.FakeStageSpace),
			"Failed to destroy fake stage space");
	}

	// Create a default stage space to use if SPACE_TYPE_STAGE is not
	// supported, or calls to xrGetReferenceSpaceBoundsRect fail.
	spaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
	spaceCreateInfo.poseInReferenceSpace.position.y = -1.6750f;
	XR_CHECK(
		xrCreateReferenceSpace(engine->appState.Session, &spaceCreateInfo, &engine->appState.FakeStageSpace),
		"Failed to create reference space (fake stage)");
	Com_Printf("Created fake stage space from local space with offset\n");
	engine->appState.CurrentSpace = engine->appState.FakeStageSpace;

	if (stageSupported)
	{
		// Centered on the head, so the origin rides with the player
		spaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
		spaceCreateInfo.poseInReferenceSpace.position.x = haveStageOffset ? stageOffset.x : 0.0f;
		spaceCreateInfo.poseInReferenceSpace.position.y = 0.0f;
		spaceCreateInfo.poseInReferenceSpace.position.z = haveStageOffset ? stageOffset.z : 0.0f;
		XR_CHECK(
			xrCreateReferenceSpace(engine->appState.Session, &spaceCreateInfo, &engine->appState.StageSpace),
			"Failed to create reference space (stage)");
		Com_Printf("Created stage space, centered on the head at (%.2f, %.2f) meters\n",
			spaceCreateInfo.poseInReferenceSpace.position.x,
			spaceCreateInfo.poseInReferenceSpace.position.z);
		engine->appState.CurrentSpace = engine->appState.StageSpace;
	}

	// Update menu orientation
	vr.menuYaw = 0;
}


void VR_ClearFrameBuffer(int width, int height)
{
	// Delegate to renderer: avoids direct graphics API calls in VR layer
	qboolean isThirdPersonSpectator = Cvar_VariableIntegerValue("vr_thirdPersonSpectator") ? qtrue : qfalse;
	re.ClearVRFramebuffer(width, height, isThirdPersonSpectator);
}


void VR_UpdatePerFrameState(void)
{
	if (vr.weapon_zoomed)
	{
		vr.weapon_zoomLevel += 0.05;
		if (vr.weapon_zoomLevel > 2.5f)
			vr.weapon_zoomLevel = 2.5f;
	}
	else
	{
		// Zoom back out quicker
		vr.weapon_zoomLevel -= 0.25f;
		if (vr.weapon_zoomLevel < 1.0f)
			vr.weapon_zoomLevel = 1.0f;
	}
}


XrDesktopViewConfiguration VR_GetDesktopViewConfiguration(void)
{
	switch (vr_desktopMode->integer)
	{
		case 0:
			return LEFT_EYE;
		case 1:
			return RIGHT_EYE;
		case 2:
			return BOTH_EYES;
	}
	return LEFT_EYE;
}


void VR_Renderer_MapLoadBegin(VR_Engine* engine)
{
	VR_Loading_Begin(engine);
}


void VR_Renderer_LoadingPump(VR_Engine* engine)
{
	VR_Loading_Pump(engine);
}


void VR_Renderer_BeginRender(VR_Engine* engine)
{
	VR_SwapchainInfos* swapchains = engine ? engine->appState.Renderer.Swapchains : NULL;

	if (!frameStarted || !swapchains || swapchains->color.acquired)
	{
		return;
	}

	VR_VK_Swapchains_Acquire(swapchains, &swapchainColorIndex, &swapchainDepthIndex);

	// Begin XR rendering: sets up Vulkan command buffer and binds XR framebuffers
	re.BeginXRFrame(swapchainColorIndex, swapchainDepthIndex);

	// Clear framebuffer
	VR_ClearFrameBuffer(swapchains->color.width, swapchains->color.height);
}


qboolean VR_Renderer_SubmitLoadingFrame(VR_Engine* engine)
{
	// Only submit frames during loading states, plus the connect screen a local map load draws
	if (clc.state != CA_LOADING && clc.state != CA_PRIMED &&
		!(VR_Loading_Active() && clc.state >= CA_CONNECTING && clc.state <= CA_PRIMED))
	{
		return qfalse;
	}

	// If no frame is in progress (e.g., after vid_restart), start one
	if (!frameStarted)
	{
		// Only try to start a frame if swapchains exist
		if (!engine || !engine->appState.Renderer.Swapchains)
		{
			return qfalse;
		}
		VR_Renderer_BeginFrame(engine, XR_FALSE);
		if (!frameStarted)
		{
			return qfalse;
		}
	}

	// End the current VR frame (this will blit to virtual screen and submit to XR)
	VR_Renderer_EndFrame(engine);

	// Start a new VR frame for the next screen update
	// This is needed because SCR_UpdateScreen will be called again during loading,
	// and it needs a valid XR frame to render into
	VR_Renderer_BeginFrame(engine, XR_FALSE);

	return qtrue;
}
