#include <inttypes.h>

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"
#include "vr_base.h"
#include "vr_clientinfo.h"
#include "vr_debug.h"

#include "vr_bhaptics.h"
#include "vr_controller_models.h"
#include "vr_debug.h"
#include "vr_input.h"
#include "vr_instance.h"
#include "vr_macros.h"
#include "vr_session.h"
#include "../vrcommon/vr_graphics.h"

#if __ANDROID__
#include <assert.h>
#include <android/log.h>
#include <unistd.h>
#endif

static VR_Engine vr_engine;
vr_clientinfo_t vr;

qboolean vr_initialized = qfalse;
qboolean vr_shutdown = qfalse;

// Required extensions first, optional extensions only if the runtime advertises them.
#define MAX_REQUIRED_EXTENSIONS 24
static const char* requiredExtensionNames[MAX_REQUIRED_EXTENSIONS];
static uint32_t numRequiredExtensions = 0;
static qboolean frameControllersEnabled = qfalse;
static qboolean picoControllersEnabled = qfalse;
static qboolean swapchainCreateFlagsEnabled = qfalse;
static qboolean swapchainFormatListEnabled = qfalse;
// XR_EXT_render_model and XR_EXT_interaction_render_model are both enabled
static qboolean controllerModelsEnabled = qfalse;
// XR_FB_render_model is enabled: Meta's own controller models, for a runtime without the EXT pair
static qboolean metaModelsEnabled = qfalse;
// Why the runtime turned down an instance with them, when it lists them
static XrResult controllerModelsRefused = XR_SUCCESS;
// What the instance was created for: 1.1, or 1.0 on a runtime without it
static XrVersion instanceApiVersion = 0;

// Instance extensions the runtime advertises, enumerated once per VR_Init
static XrExtensionProperties* s_instanceExtensions = NULL;
static uint32_t s_numInstanceExtensions = 0;

static void VR_LogLine(const char* line)
{
#if __ANDROID__
	__android_log_print(ANDROID_LOG_INFO, "OpenXR", "%s", line);
#else
	fprintf(stderr, "[OpenXR] %s\n", line);
#endif
}

// Logged as one block so a headset's capabilities can be read off a single logcat capture
static void VR_EnumerateInstanceExtensions(void)
{
	uint32_t count = 0;
	uint32_t i;
	char line[256];

	free(s_instanceExtensions);
	s_instanceExtensions = NULL;
	s_numInstanceExtensions = 0;

	if (xrEnumerateInstanceExtensionProperties(NULL, 0, &count, NULL) != XR_SUCCESS || count == 0) {
		VR_LogLine("instance extensions: none enumerated");
		return;
	}

	s_instanceExtensions = (XrExtensionProperties*)malloc(sizeof(XrExtensionProperties) * count);
	if (!s_instanceExtensions) {
		return;
	}
	for (i = 0; i < count; ++i) {
		s_instanceExtensions[i].type = XR_TYPE_EXTENSION_PROPERTIES;
		s_instanceExtensions[i].next = NULL;
	}
	if (xrEnumerateInstanceExtensionProperties(NULL, count, &count, s_instanceExtensions) != XR_SUCCESS) {
		free(s_instanceExtensions);
		s_instanceExtensions = NULL;
		return;
	}
	s_numInstanceExtensions = count;

	Com_sprintf(line, sizeof(line), "instance extensions (%u):", count);
	VR_LogLine(line);
	for (i = 0; i < count; ++i) {
		Com_sprintf(line, sizeof(line), "  %s (v%u)",
			s_instanceExtensions[i].extensionName, s_instanceExtensions[i].extensionVersion);
		VR_LogLine(line);
	}
}

static qboolean VR_HasInstanceExtension(const char* name)
{
	uint32_t i;
	for (i = 0; i < s_numInstanceExtensions; ++i) {
		if (strcmp(s_instanceExtensions[i].extensionName, name) == 0) {
			return qtrue;
		}
	}
	return qfalse;
}

static qboolean VR_AddOptionalExtension(const char* name)
{
	if (numRequiredExtensions < MAX_REQUIRED_EXTENSIONS && VR_HasInstanceExtension(name)) {
		requiredExtensionNames[numRequiredExtensions++] = name;
		return qtrue;
	}
	return qfalse;
}

// Each layer is enabled only when all of the one beneath it is, so what is enabled here is usable
static void VR_BuildFoveationExtensions(VR_Foveation* fov)
{
	memset(fov, 0, sizeof(*fov));

	if (VR_HasInstanceExtension(XR_FB_FOVEATION_EXTENSION_NAME) &&
		VR_HasInstanceExtension(XR_FB_SWAPCHAIN_UPDATE_STATE_EXTENSION_NAME) &&
		VR_HasInstanceExtension(XR_FB_FOVEATION_CONFIGURATION_EXTENSION_NAME) &&
		VR_HasInstanceExtension(XR_FB_FOVEATION_VULKAN_EXTENSION_NAME) &&
		numRequiredExtensions + 4 <= MAX_REQUIRED_EXTENSIONS)
	{
		fov->ExtFoveation = VR_AddOptionalExtension(XR_FB_SWAPCHAIN_UPDATE_STATE_EXTENSION_NAME) &&
			VR_AddOptionalExtension(XR_FB_FOVEATION_EXTENSION_NAME);
		fov->ExtConfiguration = VR_AddOptionalExtension(XR_FB_FOVEATION_CONFIGURATION_EXTENSION_NAME);
		fov->ExtVulkan = VR_AddOptionalExtension(XR_FB_FOVEATION_VULKAN_EXTENSION_NAME);
	}

	if (fov->ExtFoveation && fov->ExtConfiguration && fov->ExtVulkan)
	{
		fov->ExtEyeTracked = VR_AddOptionalExtension(XR_META_FOVEATION_EYE_TRACKED_EXTENSION_NAME);
	}
}

static void VR_BuildExtensionList(void)
{
	numRequiredExtensions = 0;
	// Graphics API extension is provided by vrvk
	requiredExtensionNames[numRequiredExtensions++] = VR_Graphics_GetExtensionName();
	requiredExtensionNames[numRequiredExtensions++] = XR_EXT_DEBUG_UTILS_EXTENSION_NAME;
	requiredExtensionNames[numRequiredExtensions++] = XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME;
#if __ANDROID__
	// Android requires this extension to pass Java context during instance creation
	requiredExtensionNames[numRequiredExtensions++] = XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME;
	VR_AddOptionalExtension(XR_EXT_PERFORMANCE_SETTINGS_EXTENSION_NAME);
	VR_AddOptionalExtension(XR_KHR_ANDROID_THREAD_SETTINGS_EXTENSION_NAME);
#endif
	// Names the view formats of a mutable swapchain so the driver can keep the image compressed
	swapchainFormatListEnabled = VR_AddOptionalExtension("XR_KHR_vulkan_swapchain_format_list");
	// Color-accurate wide gamut on Quest panels that support it (e.g. Quest Pro QD-OLED);
	// keeps the runtime from treating our sRGB/Rec709 content as P3 and oversaturating.
	qboolean haveColorSpace = (numRequiredExtensions < MAX_REQUIRED_EXTENSIONS &&
		VR_HasInstanceExtension("XR_FB_color_space"));
	if (haveColorSpace)
	{
		requiredExtensionNames[numRequiredExtensions++] = "XR_FB_color_space";
	}
#if __ANDROID__
	__android_log_print(ANDROID_LOG_INFO, "OpenXR", "XR_FB_color_space advertised: %s", haveColorSpace ? "yes" : "no");
#endif

	frameControllersEnabled = VR_AddOptionalExtension("XR_VALVE_frame_controller_interaction");
	// Without it the PICO runtime treats the PICO profiles as unsupported and emulates Touch, as the engine avoids.
	picoControllersEnabled = VR_AddOptionalExtension("XR_BD_controller_interaction");
	metaModelsEnabled = VR_AddOptionalExtension(XR_FB_RENDER_MODEL_EXTENSION_NAME);

	VR_BuildFoveationExtensions(&vr_engine.foveation);

	// Lets swapchain images carry the density map offset flag, so direct rendering can use offsets too
	swapchainCreateFlagsEnabled = VR_AddOptionalExtension("XR_META_vulkan_swapchain_create_info");
}

VR_Bool VR_HasFrameControllers(void)
{
	return frameControllersEnabled ? VR_TRUE : VR_FALSE;
}

VR_Bool VR_HasPicoControllers(void)
{
	return picoControllersEnabled ? VR_TRUE : VR_FALSE;
}

VR_Bool VR_HasSwapchainCreateFlags(void)
{
	return swapchainCreateFlagsEnabled ? VR_TRUE : VR_FALSE;
}

VR_Bool VR_HasSwapchainFormatList(void)
{
	return swapchainFormatListEnabled ? VR_TRUE : VR_FALSE;
}

static void VR_PrintEyeInfo( void )
{
	Com_Printf("Eye-tracked foveation: %s\n", vr_engine.foveation.SystemEyeTracked ? "supported" : "not supported");
}

void VR_Info_f( void )
{
	XrInstanceProperties props;
	if (vr_engine.appState.Instance == XR_NULL_HANDLE)
	{
		Com_Printf("OpenXR is not running\n");
		return;
	}
	memset(&props, 0, sizeof(props));
	props.type = XR_TYPE_INSTANCE_PROPERTIES;
	if (XR_SUCCEEDED(xrGetInstanceProperties(vr_engine.appState.Instance, &props)))
	{
		Com_Printf("Runtime: %s %u.%u.%u\n", props.runtimeName, XR_VERSION_MAJOR(props.runtimeVersion),
			XR_VERSION_MINOR(props.runtimeVersion), XR_VERSION_PATCH(props.runtimeVersion));
	}
	Com_Printf("OpenXR API: %u.%u\n", XR_VERSION_MAJOR(instanceApiVersion), XR_VERSION_MINOR(instanceApiVersion));
	Com_Printf("Enabled extensions:\n");
	for (uint32_t i = 0; i < numRequiredExtensions; i++)
		Com_Printf("  %s\n", requiredExtensionNames[i]);
	VR_PrintEyeInfo();
	if (controllerModelsRefused)
		Com_Printf("Controller models: refused by the runtime (%d)\n", (int)controllerModelsRefused);
	else
		VR_ControllerModels_Info();
	VR_PrintInputInfo();
}

static void VR_DecideFoveationCaps(void)
{
	VR_Foveation* fov = &vr_engine.foveation;
	const qboolean deviceOk = VR_Graphics_SupportsFoveation() ? qtrue : qfalse;
	char line[320];

	fov->SystemEyeTracked = fov->ExtEyeTracked &&
		vr_engine.systemProperties.FoveationEyeTracked.supportsFoveationEyeTracked;

	if (fov->ExtFoveation && fov->ExtConfiguration && fov->ExtVulkan && deviceOk)
	{
		fov->Caps = fov->SystemEyeTracked ? VR_FOVEATION_CAPS_EYE_TRACKED : VR_FOVEATION_CAPS_FIXED;
	}
	else
	{
		fov->Caps = VR_FOVEATION_CAPS_NONE;
	}

	Com_sprintf(line, sizeof(line),
		"foveation: %s (XR_FB_foveation %s, XR_FB_foveation_configuration %s, XR_FB_foveation_vulkan %s, XR_META_foveation_eye_tracked %s, system eye tracked %s, device density map %s)",
		VR_FoveationCapsString(),
		fov->ExtFoveation ? "yes" : "no",
		fov->ExtConfiguration ? "yes" : "no",
		fov->ExtVulkan ? "yes" : "no",
		fov->ExtEyeTracked ? "yes" : "no",
		fov->SystemEyeTracked ? "yes" : "no",
		deviceOk ? "yes" : "no");
	VR_LogLine(line);
}

const char* VR_FoveationCapsString(void)
{
	switch (vr_engine.foveation.Caps)
	{
		case VR_FOVEATION_CAPS_FIXED:       return "fixed";
		case VR_FOVEATION_CAPS_EYE_TRACKED: return "eyetracked";
		default:                            return "none";
	}
}

// xrCreateInstance failures that no other request would get past
static qboolean VR_InstanceUnavailable(XrResult result)
{
	return result == XR_ERROR_RUNTIME_UNAVAILABLE || result == XR_ERROR_LIMIT_REACHED ||
		result == XR_ERROR_OUT_OF_MEMORY || result == XR_ERROR_INSTANCE_LOST;
}

// OpenXR 1.1 first (Meta hands a 1.0.0 app a legacy profile that ignores swapchain create chains), 1.0 if refused;
// the render model extensions ride on the end, and a runtime that lists them yet refuses the instance gets a retry without them.
static XrResult VR_CreateInstanceForModels(const char* appName, XrInstance* instance)
{
	const uint32_t without = numRequiredExtensions;
	const qboolean wantModels = VR_HasInstanceExtension(XR_EXT_RENDER_MODEL_EXTENSION_NAME) &&
		VR_HasInstanceExtension(XR_EXT_INTERACTION_RENDER_MODEL_EXTENSION_NAME) &&
		without + 3 <= MAX_REQUIRED_EXTENSIONS;
	const qboolean uuid = VR_HasInstanceExtension(XR_EXT_UUID_EXTENSION_NAME);
	XrResult result = XR_ERROR_RUNTIME_FAILURE;
	int attempt;

	controllerModelsEnabled = qfalse;
	controllerModelsRefused = XR_SUCCESS;
	for (attempt = 0; attempt < 2; attempt++)
	{
		const XrVersion apiVersion = attempt ? XR_API_VERSION_1_0 : XR_API_VERSION_1_1;
		XrResult refused = XR_SUCCESS;

		numRequiredExtensions = without;
		if (wantModels)
		{
			// Render models need OpenXR 1.1 or this extension
			if (attempt && uuid)
			{
				requiredExtensionNames[numRequiredExtensions++] = XR_EXT_UUID_EXTENSION_NAME;
			}
			requiredExtensionNames[numRequiredExtensions++] = XR_EXT_RENDER_MODEL_EXTENSION_NAME;
			requiredExtensionNames[numRequiredExtensions++] = XR_EXT_INTERACTION_RENDER_MODEL_EXTENSION_NAME;
		}
		result = VR_CreateInstance(appName, apiVersion, numRequiredExtensions, requiredExtensionNames, instance);
		// A runtime may refuse the extensions with any code
		if (XR_FAILED(result) && wantModels && result != XR_ERROR_API_VERSION_UNSUPPORTED &&
			!VR_InstanceUnavailable(result))
		{
			refused = result;
			numRequiredExtensions = without;
			result = VR_CreateInstance(appName, apiVersion, numRequiredExtensions, requiredExtensionNames, instance);
		}
		if (XR_SUCCEEDED(result))
		{
			controllerModelsEnabled = wantModels && !refused;
			controllerModelsRefused = refused;
			instanceApiVersion = apiVersion;
			break;
		}
		if (VR_InstanceUnavailable(result))
		{
			break;
		}
	}
	return result;
}

// Part of init
void VR_InitInstanceInput( VR_Engine* );

VR_Engine* VR_Init( void )
{
	if (vr_initialized || vr_shutdown)
	{
		return &vr_engine;
	}

	memset(&vr_engine, 0, sizeof(vr_engine));
	memset(&vr, 0, sizeof(vr));

#if __ANDROID__
	// The Android OpenXR loader must be initialized before enumerating instance
	// extensions, or the enumeration comes back empty. Idempotent: VR_CreateInstance
	// calls it again as a no-op.
	VR_InitializeLoaderAndroid();
#endif

	VR_EnumerateInstanceExtensions();
	VR_BuildExtensionList();

	fprintf(stderr, "[OpenXR] Initializing OpenXR instance and system...\n");

	const qboolean listApiLayers = qfalse;
	if (listApiLayers)
	{
		VR_ListAPILayers();
	}

	// Create the OpenXR instance.
	const char* appName = "Trinity";
	XR_CHECK(
		VR_CreateInstanceForModels(appName, &vr_engine.appState.Instance),
		"Failed to create OpenXR instance");
	{
		char line[128];
		Com_sprintf(line, sizeof(line), "instance: OpenXR %u.%u, controller models %s (%d)",
			XR_VERSION_MAJOR(instanceApiVersion), XR_VERSION_MINOR(instanceApiVersion),
			controllerModelsEnabled ? "enabled" : controllerModelsRefused ? "refused" : "not offered",
			(int)controllerModelsRefused);
		VR_LogLine(line);
	}

	XrInstanceProperties instanceInfo;
	instanceInfo.type = XR_TYPE_INSTANCE_PROPERTIES;
	instanceInfo.next = NULL;
	XR_CHECK(xrGetInstanceProperties(vr_engine.appState.Instance, &instanceInfo), "Failed to query OpenXR instance properties");
	fprintf(stdout, "[OpenXR] Runtime: %s | Version: %u.%u.%u\n",
		instanceInfo.runtimeName,
		XR_VERSION_MAJOR(instanceInfo.runtimeVersion),
		XR_VERSION_MINOR(instanceInfo.runtimeVersion),
		XR_VERSION_PATCH(instanceInfo.runtimeVersion));

	VR_CreateDebugUtilsMessenger(vr_engine.appState.Instance, &vr_engine.appState.DebugUtilsMessenger);

	XR_CHECK(
		VR_GetHMDSystem(vr_engine.appState.Instance, &vr_engine.appState.SystemId), 
		"Failed to get OpenXR system ID");


	VR_GetSystemProperties(vr_engine.appState.Instance, vr_engine.appState.SystemId, &vr_engine.systemProperties,
		vr_engine.foveation.ExtEyeTracked);

	// Get graphics requirements via the graphics-specific implementation
	XR_CHECK(
		VR_Graphics_GetRequirements(vr_engine.appState.Instance, vr_engine.appState.SystemId),
		"Failed to get graphics requirements");

	// Print graphics requirements debug info
	VR_Graphics_PrintRequirements();

	// Initialize graphics subsystem (Vulkan: creates VkInstance/VkDevice, OpenGL: no-op)
	VR_Graphics_Init(vr_engine.appState.Instance, vr_engine.appState.SystemId);

	// Needs both the runtime's extensions and a device that can read the density map
	VR_DecideFoveationCaps();

	fprintf(stderr,
		"[OpenXR] system properties:\n"
		"  System name: %s\n"
		"  Tracking: {position: %s, orientation: %s}\n"
		"  Graphics: {maxLayerCount: %d, maxSwapchainResolution: %dx%d}\n",
		vr_engine.systemProperties.SystemProperties.systemName,
		vr_engine.systemProperties.SystemProperties.trackingProperties.positionTracking ? "yes" : "no",
		vr_engine.systemProperties.SystemProperties.trackingProperties.orientationTracking ? "yes" : "no",
		vr_engine.systemProperties.SystemProperties.graphicsProperties.maxLayerCount,
		vr_engine.systemProperties.SystemProperties.graphicsProperties.maxSwapchainImageWidth,
		vr_engine.systemProperties.SystemProperties.graphicsProperties.maxSwapchainImageHeight);

	// We're done
	fprintf(stderr,
		"[OpenXR] Instance and system succesfully initialized:\n"
		"  - Instance: %p\n"
		"  - System: %" PRIu64 "\n\n",
		vr_engine.appState.Instance,
		(uint64_t)vr_engine.appState.SystemId);

	vr_initialized = qtrue;
	VR_InitInstanceInput(&vr_engine);

	return &vr_engine;
}

VR_Engine* VR_GetEngine( void )
{
	return &vr_engine;
}

void VR_Destroy( VR_Engine* engine )
{
	if (engine == &vr_engine)
	{
#ifdef USE_BHAPTICS
		VR_Bhaptics_Shutdown();
#endif
		// Invalidate XR function pointers before destroying XrInstance: they were
		// obtained via xrGetInstanceProcAddr and become invalid after xrDestroyInstance.
		// Note: We do NOT call VR_Graphics_Shutdown() here because the renderer still
		// needs the VkDevice/VkInstance. The renderer will destroy them in vk_shutdown().
		VR_Graphics_InvalidateFunctionPointers();

		VR_DestroyDebugUtilsMessenger(engine->appState.Instance, &engine->appState.DebugUtilsMessenger);
		xrDestroyInstance(engine->appState.Instance);
		memset(&vr_engine, 0, sizeof(vr_engine));
		free(s_instanceExtensions);
		s_instanceExtensions = NULL;
		s_numInstanceExtensions = 0;
	}
	vr_initialized = qfalse;
}

void VR_PrepareForShutdown( void )
{
	vr_shutdown = qtrue;
}

void VR_EnterVR( VR_Engine* engine )
{
	if (engine->appState.Session)
	{
		fprintf(stderr, "VR_EnterVR called with existing session");
		return;
	}

	fprintf(stderr, "[OpenXR] Creating XR session and reference space\n");

	// Create the OpenXR Session.
	XR_CHECK(
		VR_CreateSession(engine->appState.Instance, engine->appState.SystemId, &engine->appState.Session),
		"Failed to create XR session");
	VR_ControllerModels_Init(engine->appState.Instance, engine->appState.Session, controllerModelsEnabled,
		metaModelsEnabled && !controllerModelsEnabled);

	// Create a space to the first path
	XrReferenceSpaceCreateInfo spaceCreateInfo = {};
	spaceCreateInfo.type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO;
	spaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
	spaceCreateInfo.poseInReferenceSpace.orientation.w = 1.0f;
	XR_CHECK(
		xrCreateReferenceSpace(engine->appState.Session, &spaceCreateInfo, &engine->appState.HeadSpace),
		"Failed to create reference space (HEAD/VIEW)");

	fprintf(stderr, "[OpenXR] XR session and reference space created\n\n");
}

void VR_LeaveVR( VR_Engine* engine )
{
	if (engine->appState.Session) 
	{
		fprintf(stderr, "[OpenXR] Destroying XR session and reference spaces\n");

		VR_ControllerModels_Shutdown();

		XR_CHECK(
			xrDestroySpace(engine->appState.HeadSpace),
			"Failed to destroy reference space (HEAD/VIEW)");
		engine->appState.HeadSpace = XR_NULL_HANDLE;

		// StageSpace is optional.
		if (engine->appState.StageSpace != XR_NULL_HANDLE)
		{
			XR_CHECK(
				xrDestroySpace(engine->appState.StageSpace),
				"Failed to destroy reference space (STAGE)");
			engine->appState.StageSpace = XR_NULL_HANDLE;
		}
		XR_CHECK(
			xrDestroySpace(engine->appState.FakeStageSpace),
			"Failed to destroy reference space (FAKE STAGE)");
		engine->appState.FakeStageSpace = XR_NULL_HANDLE;
		engine->appState.CurrentSpace = XR_NULL_HANDLE;

		XR_CHECK(
			xrDestroySession(engine->appState.Session),
			"Failed to destroy XR session");
		engine->appState.Session = NULL;

		engine->appState.SessionActive = VR_FALSE;
		engine->appState.Visible = VR_FALSE;
		engine->appState.Focused = VR_FALSE;

		fprintf(stderr, "[OpenXR] XR session and reference spaces destroyed\n");
	}
}

//#endif
