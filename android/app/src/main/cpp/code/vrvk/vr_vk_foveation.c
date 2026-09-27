/*
 * vr_vk_foveation.c - Foveated rendering profiles (XR_FB_foveation family)
 *
 * See vr_vk_foveation.h.
 */

#include "vr_vk_foveation.h"
#include "vr_vk.h"
#include "vr_vk_types.h"

#include "../vrcommon/vr_base.h"
#include "../vrcommon/vr_clientinfo.h"
#include "../vrcommon/vr_debug.h"
#include "../vrcommon/vr_graphics.h"

#include "../client/client.h"
#include "../qcommon/qcommon.h"

#include <stdlib.h>

extern cvar_t* vr_foveation;
extern cvar_t* vr_foveationStrength;
extern vr_clientinfo_t vr;

static PFN_xrCreateFoveationProfileFB pfnCreateFoveationProfileFB = NULL;
static PFN_xrDestroyFoveationProfileFB pfnDestroyFoveationProfileFB = NULL;
static PFN_xrUpdateSwapchainFB pfnUpdateSwapchainFB = NULL;
static PFN_xrGetFoveationEyeTrackedStateMETA pfnGetFoveationEyeTrackedStateMETA = NULL;
static XrInstance s_functionsInstance = XR_NULL_HANDLE;

// Sharp island center per eye in NDC: the last valid gaze, or the fixed center when eye tracking is off
static float s_gazeCenter[2][2] = { { 0.0f, 0.0f }, { 0.0f, 0.0f } };

/*
==================
VR_VK_Foveation_FixedCenter

Where the fixed island sits, in the gaze's NDC: horizontally on the direction both eyes
share, vertically on the angular bisector of the vertical field. A headset that gives more
field one way than the other is saying where its designer expects the eye to go, so the
island follows that lean; a symmetric field leaves it on the optical axis. The Quest 3
reaches 44 degrees up and 55 down, which puts the island 5.5 degrees below the axis.
==================
*/
static void VR_VK_Foveation_FixedCenter(float centers[2][2])
{
	const float tanUp = tanf(vr.fov_angle_up);
	const float tanDown = tanf(vr.fov_angle_down);
	const float spanY = tanUp - tanDown;
	// The angular bisector of the vertical field
	const float midTan = tanf(0.5f * (vr.fov_angle_up + vr.fov_angle_down));
	int eye;

	for (eye = 0; eye < 2; ++eye)
	{
		const float tanLeft = tanf(vr.eye_fov_angle_left[eye]);
		const float tanRight = tanf(vr.eye_fov_angle_right[eye]);
		const float spanX = tanRight - tanLeft;
		// Where head-forward falls in this eye's frame: a canted panel carries its own axis
		// outward, and the island belongs on the direction both eyes share
		const float forwardTan = tanf(vr.eyeCantYaw[eye]);

		centers[eye][0] = (fabsf(spanX) > 1e-6f) ? (2.0f * forwardTan - tanLeft - tanRight) / spanX : 0.0f;
		centers[eye][1] = (fabsf(spanY) > 1e-6f) ? (tanUp + tanDown - 2.0f * midTan) / spanY : 0.0f;
	}
}

// Re-applied by the per-frame hook whenever the swapchain is replaced (session start, vid_restart)
static XrSwapchain s_appliedSwapchain = XR_NULL_HANDLE;

// Kept alive: XR_META_foveation_eye_tracked wants the profile re-handed to the swapchain each frame to recenter the map
static XrFoveationProfileFB s_eyeTrackedProfile = XR_NULL_HANDLE;
static XrSession s_eyeTrackedProfileSession = XR_NULL_HANDLE;
static qboolean s_eyeTrackedUpdateFailed = qfalse;

static void VR_VK_Foveation_DropEyeTrackedProfile(const VR_Engine* engine)
{
	if (s_eyeTrackedProfile != XR_NULL_HANDLE)
	{
		// A session that is gone took its profile along
		if (engine->appState.Session == s_eyeTrackedProfileSession && pfnDestroyFoveationProfileFB)
		{
			pfnDestroyFoveationProfileFB(s_eyeTrackedProfile);
		}
		s_eyeTrackedProfile = XR_NULL_HANDLE;
		s_eyeTrackedProfileSession = XR_NULL_HANDLE;
	}
	s_eyeTrackedUpdateFailed = qfalse;
}

static qboolean VR_VK_Foveation_LoadFunctions(const VR_Engine* engine)
{
	XrInstance instance = engine->appState.Instance;

	if (instance == XR_NULL_HANDLE)
	{
		return qfalse;
	}
	if (s_functionsInstance == instance && pfnCreateFoveationProfileFB && pfnDestroyFoveationProfileFB && pfnUpdateSwapchainFB)
	{
		return qtrue;
	}

	// A new instance means the session that owned any kept profile is gone
	s_eyeTrackedProfile = XR_NULL_HANDLE;
	s_eyeTrackedProfileSession = XR_NULL_HANDLE;
	s_eyeTrackedUpdateFailed = qfalse;

	pfnCreateFoveationProfileFB = NULL;
	pfnDestroyFoveationProfileFB = NULL;
	pfnUpdateSwapchainFB = NULL;
	pfnGetFoveationEyeTrackedStateMETA = NULL;

	xrGetInstanceProcAddr(instance, "xrCreateFoveationProfileFB", (PFN_xrVoidFunction*)&pfnCreateFoveationProfileFB);
	xrGetInstanceProcAddr(instance, "xrDestroyFoveationProfileFB", (PFN_xrVoidFunction*)&pfnDestroyFoveationProfileFB);
	xrGetInstanceProcAddr(instance, "xrUpdateSwapchainFB", (PFN_xrVoidFunction*)&pfnUpdateSwapchainFB);
	if (engine->foveation.ExtEyeTracked)
	{
		xrGetInstanceProcAddr(instance, "xrGetFoveationEyeTrackedStateMETA", (PFN_xrVoidFunction*)&pfnGetFoveationEyeTrackedStateMETA);
	}

	if (!pfnCreateFoveationProfileFB || !pfnDestroyFoveationProfileFB || !pfnUpdateSwapchainFB)
	{
		Com_Printf("foveation: runtime advertises the extensions but not their entry points\n");
		return qfalse;
	}

	s_functionsInstance = instance;
	return qtrue;
}

qboolean VR_VK_Foveation_SwapchainWanted(void)
{
	const VR_Engine* engine = VR_GetEngine();

	return engine != NULL &&
		engine->foveation.Caps != VR_FOVEATION_CAPS_NONE &&
		VR_Graphics_SupportsFoveation();
}

// The renderer writes its own map; the runtime's level only matters before gaze is reported, or where the renderer has none
static XrFoveationLevelFB VR_VK_Foveation_XrLevel(int mode, int strength)
{
	if (mode == VR_FOVEATION_OFF)
	{
		return XR_FOVEATION_LEVEL_NONE_FB;
	}
	switch (strength)
	{
		case VR_FOVEATION_STRENGTH_LOW:    return XR_FOVEATION_LEVEL_LOW_FB;
		case VR_FOVEATION_STRENGTH_MEDIUM: return XR_FOVEATION_LEVEL_MEDIUM_FB;
		default:                           return XR_FOVEATION_LEVEL_HIGH_FB;
	}
}

/*
==================
VR_VK_Foveation_UpdateProfile

A fixed profile is destroyed right after xrUpdateSwapchainFB, which copies what it
needs; an eye tracked one is kept for the per-frame re-apply.
==================
*/
static qboolean VR_VK_Foveation_UpdateProfile(VR_Engine* engine, XrSwapchain swapchain, int mode, int strength, qboolean eyeTracked)
{
	XrFoveationEyeTrackedProfileCreateInfoMETA eyeTrackedInfo;
	XrFoveationLevelProfileCreateInfoFB levelInfo;
	XrFoveationProfileCreateInfoFB profileInfo;
	XrSwapchainStateFoveationFB state;
	XrFoveationProfileFB profile = XR_NULL_HANDLE;
	XrResult result;

	VR_VK_Foveation_DropEyeTrackedProfile(engine);

	memset(&eyeTrackedInfo, 0, sizeof(eyeTrackedInfo));
	eyeTrackedInfo.type = XR_TYPE_FOVEATION_EYE_TRACKED_PROFILE_CREATE_INFO_META;
	eyeTrackedInfo.next = NULL;
	eyeTrackedInfo.flags = 0;

	memset(&levelInfo, 0, sizeof(levelInfo));
	levelInfo.type = XR_TYPE_FOVEATION_LEVEL_PROFILE_CREATE_INFO_FB;
	levelInfo.next = eyeTracked ? &eyeTrackedInfo : NULL;
	levelInfo.level = VR_VK_Foveation_XrLevel(mode, strength);
	levelInfo.verticalOffset = 0.0f;
	levelInfo.dynamic = XR_FOVEATION_DYNAMIC_DISABLED_FB;

	memset(&profileInfo, 0, sizeof(profileInfo));
	profileInfo.type = XR_TYPE_FOVEATION_PROFILE_CREATE_INFO_FB;
	profileInfo.next = &levelInfo;

	result = pfnCreateFoveationProfileFB(engine->appState.Session, &profileInfo, &profile);
	if (XR_FAILED(result))
	{
		Com_Printf("foveation: xrCreateFoveationProfileFB failed for strength %d%s: %s\n",
			strength, eyeTracked ? " (eye tracked)" : "", GetXRErrorString(result));
		return qfalse;
	}

	memset(&state, 0, sizeof(state));
	state.type = XR_TYPE_SWAPCHAIN_STATE_FOVEATION_FB;
	state.next = NULL;
	state.flags = 0;
	state.profile = profile;

	result = pfnUpdateSwapchainFB(swapchain, (const XrSwapchainStateBaseHeaderFB*)&state);
	if (XR_FAILED(result))
	{
		pfnDestroyFoveationProfileFB(profile);
		Com_Printf("foveation: xrUpdateSwapchainFB failed for strength %d%s: %s\n",
			strength, eyeTracked ? " (eye tracked)" : "", GetXRErrorString(result));
		return qfalse;
	}
	if (eyeTracked)
	{
		s_eyeTrackedProfile = profile;
		s_eyeTrackedProfileSession = engine->appState.Session;
	}
	else
	{
		pfnDestroyFoveationProfileFB(profile);
	}
	return qtrue;
}

static void VR_VK_Foveation_RequestPatternUpdate(VR_Engine* engine, XrSwapchain swapchain)
{
	XrSwapchainStateFoveationFB state;
	XrResult result;

	if (s_eyeTrackedProfile == XR_NULL_HANDLE || s_eyeTrackedUpdateFailed ||
		swapchain == XR_NULL_HANDLE || engine->appState.Session != s_eyeTrackedProfileSession)
	{
		return;
	}

	memset(&state, 0, sizeof(state));
	state.type = XR_TYPE_SWAPCHAIN_STATE_FOVEATION_FB;
	state.next = NULL;
	state.flags = 0;
	state.profile = s_eyeTrackedProfile;

	result = pfnUpdateSwapchainFB(swapchain, (const XrSwapchainStateBaseHeaderFB*)&state);
	if (XR_FAILED(result))
	{
		// Say so once; the map then stays where the last update left it
		s_eyeTrackedUpdateFailed = qtrue;
		Com_Printf("foveation: per-frame xrUpdateSwapchainFB failed: %s\n", GetXRErrorString(result));
	}
}

qboolean VR_VK_Foveation_ApplyToSwapchain(VR_Engine* engine, XrSwapchain swapchain)
{
	int mode, strength;
	qboolean eyeTracked;

	if (!vr_foveation || !vr_foveationStrength)
	{
		return qfalse;
	}
	vr_foveation->modified = qfalse;
	vr_foveationStrength->modified = qfalse;
	s_appliedSwapchain = swapchain;
	engine->appState.Renderer.FoveationMode = VR_FOVEATION_OFF;
	engine->appState.Renderer.FoveationStrength = VR_FOVEATION_STRENGTH_MEDIUM;
	engine->appState.Renderer.FoveationEyeTracked = VR_FALSE;

	if (swapchain == XR_NULL_HANDLE || !VR_VK_Foveation_SwapchainWanted() || !VR_VK_Foveation_LoadFunctions(engine))
	{
		return qfalse;
	}

	mode = vr_foveation->integer;
	if (mode < VR_FOVEATION_OFF) mode = VR_FOVEATION_OFF;
	else if (mode > VR_FOVEATION_EYE_TRACKED) mode = VR_FOVEATION_EYE_TRACKED;

	strength = vr_foveationStrength->integer;
	if (strength < VR_FOVEATION_STRENGTH_LOW) strength = VR_FOVEATION_STRENGTH_LOW;
	else if (strength > VR_FOVEATION_STRENGTH_HIGH) strength = VR_FOVEATION_STRENGTH_HIGH;

	eyeTracked = (mode == VR_FOVEATION_EYE_TRACKED);

	if (eyeTracked && engine->foveation.Caps != VR_FOVEATION_CAPS_EYE_TRACKED)
	{
		Com_Printf("foveation: this headset cannot follow the eyes, using fixed\n");
		mode = VR_FOVEATION_FIXED;
		eyeTracked = qfalse;
		Cvar_SetValue("vr_foveation", mode);
		vr_foveation->modified = qfalse;
	}

	if (!VR_VK_Foveation_UpdateProfile(engine, swapchain, mode, strength, eyeTracked))
	{
		if (!eyeTracked)
		{
			return qfalse;
		}
		Com_Printf("foveation: runtime refused the eye tracked profile, using fixed\n");
		mode = VR_FOVEATION_FIXED;
		eyeTracked = qfalse;
		Cvar_SetValue("vr_foveation", mode);
		vr_foveation->modified = qfalse;
		if (!VR_VK_Foveation_UpdateProfile(engine, swapchain, mode, strength, eyeTracked))
		{
			return qfalse;
		}
	}

	engine->appState.Renderer.FoveationMode = mode;
	engine->appState.Renderer.FoveationStrength = strength;
	engine->appState.Renderer.FoveationEyeTracked = eyeTracked ? VR_TRUE : VR_FALSE;

	Com_Printf("foveation: %s, strength %d\n",
		mode == VR_FOVEATION_OFF ? "off" : eyeTracked ? "eye tracked" : "fixed", strength);
	return qtrue;
}

/*
==================
VR_VK_Foveation_RefreshDensityMaps

Re-enumerate for the density maps alone; the image handles never change.
==================
*/
static qboolean VR_VK_Foveation_RefreshDensityMaps(VR_VK_SwapchainInfo* color)
{
	VkImage* images = NULL;
	VkImage* maps = NULL;
	uint32_t count = 0;
	uint32_t width = 0;
	uint32_t height = 0;

	if (XR_FAILED(VR_Vulkan_GetSwapchainImages(color->swapchain, &images, &count, &maps, &width, &height)))
	{
		Com_Printf("foveation: re-enumerating swapchain images failed\n");
		return qfalse;
	}
	free(images);

	if (!maps)
	{
		Com_Printf("foveation: runtime still reports no density maps after the profile was applied\n");
		return qfalse;
	}
	if (count != color->imageCount)
	{
		Com_Printf("foveation: swapchain image count changed from %u to %u, ignoring density maps\n", color->imageCount, count);
		free(maps);
		return qfalse;
	}

	free(color->foveationImages);
	color->foveationImages = maps;
	color->foveationWidth = width;
	color->foveationHeight = height;
	Com_Printf("foveation: density maps available after the profile was applied (%u images, %ux%u texels)\n",
		count, width, height);
	return qtrue;
}

void VR_VK_Foveation_Disarm(VR_Engine* engine, XrSwapchain swapchain)
{
	if (swapchain == XR_NULL_HANDLE || !VR_VK_Foveation_LoadFunctions(engine))
	{
		return;
	}
	if (VR_VK_Foveation_UpdateProfile(engine, swapchain, VR_FOVEATION_OFF, VR_FOVEATION_STRENGTH_MEDIUM, qfalse))
	{
		Com_Printf("foveation: no density maps for this swapchain, profile reset to none\n");
	}
	engine->appState.Renderer.FoveationMode = VR_FOVEATION_OFF;
	engine->appState.Renderer.FoveationEyeTracked = VR_FALSE;
}

qboolean VR_VK_Foveation_Apply(VR_Engine* engine)
{
	VR_SwapchainInfos* swapchains = engine->appState.Renderer.Swapchains;
	qboolean hadMaps;

	if (!swapchains)
	{
		if (vr_foveation)
		{
			vr_foveation->modified = qfalse;
		}
		return qfalse;
	}

	hadMaps = swapchains->color.foveationImages != NULL;
	if (!VR_VK_Foveation_ApplyToSwapchain(engine, swapchains->color.swapchain))
	{
		return qfalse;
	}

	// A runtime that allocates maps lazily hands them out only now; the renderer's own map applies the level regardless
	if (!hadMaps && engine->appState.Renderer.FoveationMode != VR_FOVEATION_OFF)
	{
		return VR_VK_Foveation_RefreshDensityMaps(&swapchains->color);
	}
	return qfalse;
}

static qboolean VR_VK_Foveation_SampleGaze(VR_Engine* engine, float centers[2][2])
{
	XrFoveationEyeTrackedStateMETA state;
	int valid;

	if (!pfnGetFoveationEyeTrackedStateMETA || engine->appState.Session == XR_NULL_HANDLE)
	{
		return qfalse;
	}

	memset(&state, 0, sizeof(state));
	state.type = XR_TYPE_FOVEATION_EYE_TRACKED_STATE_META;
	state.next = NULL;
	if (XR_FAILED(pfnGetFoveationEyeTrackedStateMETA(engine->appState.Session, &state)))
	{
		return qfalse;
	}

	valid = (state.flags & XR_FOVEATION_EYE_TRACKED_STATE_VALID_BIT_META) ? 1 : 0;
	if (valid)
	{
		centers[0][0] = state.foveationCenter[0].x;
		centers[0][1] = state.foveationCenter[0].y;
		centers[1][0] = state.foveationCenter[1].x;
		centers[1][1] = state.foveationCenter[1].y;
	}

	return valid ? qtrue : qfalse;
}

void VR_VK_Foveation_Frame(VR_Engine* engine)
{
	const VR_SwapchainInfos* swapchains = engine->appState.Renderer.Swapchains;
	const qboolean newSwapchain = swapchains && swapchains->color.swapchain != s_appliedSwapchain;

	if (vr_foveation && vr_foveationStrength &&
		(vr_foveation->modified || vr_foveationStrength->modified || newSwapchain))
	{
		if (VR_VK_Foveation_Apply(engine))
		{
			Com_Printf("foveation: rebuilding renderer resources for the density maps\n");
			re.InitXRResources();
		}
	}
	// An invalid gaze (a blink) keeps the last good centers instead of snapping to the middle
	{
		const qboolean eyeTracked = engine->appState.Renderer.FoveationEyeTracked ? qtrue : qfalse;
		// Strength zero shades every pixel: menus fill the eye buffer, so foveating them only softens text
		const qboolean foveate = ( engine->appState.Renderer.FoveationMode != VR_FOVEATION_OFF ) &&
			!vr.virtual_screen;
		const int strength = foveate ? engine->appState.Renderer.FoveationStrength : 0;

		if (eyeTracked && swapchains)
		{
			// Meta's extension asks for this right before the gaze is read
			VR_VK_Foveation_RequestPatternUpdate(engine, swapchains->color.swapchain);
			VR_VK_Foveation_SampleGaze(engine, s_gazeCenter);
		}
		else
		{
			// Nothing to follow, so sit on the axis the eye looks down
			VR_VK_Foveation_FixedCenter(s_gazeCenter);
		}
		if (re.SetFoveation)
		{
			// The renderer draws the falloff in each eye's own frustum
			float fovTan[2][4];
			int eye;

			for (eye = 0; eye < 2; ++eye)
			{
				fovTan[eye][0] = tanf(vr.eye_fov_angle_left[eye]);
				fovTan[eye][1] = tanf(vr.eye_fov_angle_right[eye]);
				fovTan[eye][2] = tanf(vr.fov_angle_up);
				fovTan[eye][3] = tanf(vr.fov_angle_down);
			}
			re.SetFoveation(strength, eyeTracked, (const float (*)[2])s_gazeCenter,
				(const float (*)[4])fovTan);
		}
	}
}
