#include "vr_input.h"

#include "../qcommon/qcommon.h"
#include "../client/keycodes.h"
#include "../client/client.h"

#include "vr_base.h"
#include "vr_clientinfo.h"
#include "vr_shared.h"
#include "vr_gameplay.h"
#include "vr_bhaptics.h"
#include "vr_graphics.h"
#include "vr_haptics.h"
#include "vr_macros.h"
#include "vr_virtual_screen.h"
#include "vr_math.h"
#include "vr_bind.h"
#include "vr_router.h"

#if __ANDROID__
#include <android/log.h>
#endif

#ifdef USE_INTERNAL_SDL
#	include "SDL.h"
#else
#	include <SDL.h>
#endif

#include <openxr/openxr.h>
#include "common/xr_linear.h"

//OpenXR
XrPath leftHandPath;
XrPath rightHandPath;
XrAction handPoseLeftAction;
XrAction handPoseRightAction;
XrAction aimPoseLeftAction;
XrAction aimPoseRightAction;
XrAction indexLeftAction;
XrAction indexRightAction;
XrAction menuAction;
XrAction bumperLeftAction;
XrAction bumperRightAction;
XrAction dpadUpAction;
XrAction dpadDownAction;
XrAction dpadLeftAction;
XrAction dpadRightAction;
XrAction viewAction;
XrAction gripClickLeftAction;
XrAction gripClickRightAction;
XrAction buttonAAction;
XrAction buttonBAction;
XrAction buttonXAction;
XrAction buttonYAction;
XrAction gripLeftAction;
XrAction gripRightAction;
XrAction trackpadLeftAction;
XrAction trackpadRightAction;
XrAction moveOnLeftJoystickAction;
XrAction moveOnRightJoystickAction;
XrAction thumbstickLeftClickAction;
XrAction thumbstickRightClickAction;
XrAction thumbrestLeftTouchAction;
XrAction thumbrestRightTouchAction;
XrAction vibrateLeftFeedback;
XrAction vibrateRightFeedback;
XrActionSet runningActionSet;
// Weapon pose, bound to grip: aim is a pointing ray, not the basis an object is held with
XrSpace leftControllerGripSpace = XR_NULL_HANDLE;
XrSpace rightControllerGripSpace = XR_NULL_HANDLE;

// Aim pose, for the menu cursor only, so it never inherits the weapon's pitch offset
XrSpace leftControllerAimSpace = XR_NULL_HANDLE;
XrSpace rightControllerAimSpace = XR_NULL_HANDLE;

qboolean inputInitialized = qfalse;
qboolean useSimpleProfile = qfalse;

extern vr_clientinfo_t vr;


// Aim poses in the world space, without vr_heightAdjust, for rays against the virtual screen
static XrPosef aimPose[2];
static qboolean aimPoseValid[2];
static qboolean aimOrientationValid[2];


extern cvar_t *vr_sensitivity;

#ifndef EPSILON
#define EPSILON 0.001f
#endif


extern cvar_t *vr_righthanded;
extern cvar_t *vr_switchThumbsticks;
extern cvar_t *vr_snapturn;
extern cvar_t *vr_directionMode;
extern cvar_t *vr_weaponPitch;

// The grip pose runs along the handle; this fixed pitch turns it into the pointing direction, vr_weaponPitch is the player's offset
#define VR_GRIP_TO_AIM_PITCH (-90.0f)

// Grip poses follow each handle's angle; measured on the headset, 0 until a controller needs one
static const struct { const char* path; const char* name; float pitch; } vrProfiles[] = {
	{ "/interaction_profiles/oculus/touch_controller", "Oculus Touch", 0.0f },
	{ "/interaction_profiles/bytedance/pico4_controller", "PICO 4", 0.0f },
	{ "/interaction_profiles/bytedance/pico4s_controller", "PICO 4 Ultra", 0.0f },
	{ "/interaction_profiles/valve/index_controller", "Valve Index", 0.0f },
	{ "/interaction_profiles/khr/simple_controller", "Simple", 0.0f },
	{ "/interaction_profiles/valve/frame_controller_valve", "Steam Frame", 20.0f },
};
static int vrCurrentProfile[2] = { -1, -1 };

static float VR_ProfilePitch( int hand )
{
	int p = vrCurrentProfile[hand];
	return p >= 0 ? vrProfiles[p].pitch : 0.0f;
}

void VR_UpdateInteractionProfiles( void )
{
	VR_Engine* engine = VR_GetEngine();
	XrPath hands[2] = { leftHandPath, rightHandPath };
	for (int hand = 0; hand < 2; hand++)
	{
		XrInteractionProfileState state = {};
		char path[XR_MAX_PATH_LENGTH];
		uint32_t length = 0;
		vrCurrentProfile[hand] = -1;
		state.type = XR_TYPE_INTERACTION_PROFILE_STATE;
		if (engine->appState.Session == XR_NULL_HANDLE || hands[hand] == XR_NULL_PATH ||
			XR_FAILED(xrGetCurrentInteractionProfile(engine->appState.Session, hands[hand], &state)) ||
			state.interactionProfile == XR_NULL_PATH ||
			XR_FAILED(xrPathToString(engine->appState.Instance, state.interactionProfile, sizeof(path), &length, path)))
		{
			continue;
		}
		for (int p = 0; p < (int)(sizeof(vrProfiles) / sizeof(vrProfiles[0])); p++)
		{
			if (!strcmp(path, vrProfiles[p].path))
				vrCurrentProfile[hand] = p;
		}
	}
}

void VR_PrintInputInfo( void )
{
	for (int hand = 0; hand < 2; hand++)
	{
		int p = vrCurrentProfile[hand];
		Com_Printf("%s hand: %s, pitch correction %g\n", hand ? "Right" : "Left",
			p >= 0 ? vrProfiles[p].name : "none reported", (double)VR_ProfilePitch(hand));
	}
}

extern cvar_t *vr_heightAdjust;
extern cvar_t *vr_twoHandedWeapons;
extern cvar_t *vr_weaponScope;
extern cvar_t *vr_thumbstickDeadzone;
extern cvar_t *vr_thumbstickFullDeflection;
extern cvar_t *vr_analogWalk;
extern cvar_t *vr_weaponSelectorMode;
extern cvar_t *vr_6dof;


void rotateAboutOrigin(float x, float y, float rotation, vec2_t out)
{
	out[0] = cosf(DEG2RAD(-rotation)) * x + sinf(DEG2RAD(-rotation)) * y;
	out[1] = cosf(DEG2RAD(-rotation)) * y - sinf(DEG2RAD(-rotation)) * x;
}

static float length(float x, float y)
{
	return sqrtf(powf(x, 2.0f) + powf(y, 2.0f));
}

void NormalizeAngles(vec3_t angles)
{
	while (angles[0] >= 90) angles[0] -= 180;
	while (angles[1] >= 180) angles[1] -= 360;
	while (angles[2] >= 180) angles[2] -= 360;
	while (angles[0] < -90) angles[0] += 180;
	while (angles[1] < -180) angles[1] += 360;
	while (angles[2] < -180) angles[2] += 360;
}

void GetAnglesFromVectors(const XrVector3f forward, const XrVector3f right, const XrVector3f up, vec3_t angles)
{
	float sr, sp, sy, cr, cp, cy;

	sp = -forward.z;

	float cp_x_cy = forward.x;
	float cp_x_sy = forward.y;
	float cp_x_sr = -right.z;
	float cp_x_cr = up.z;

	float yaw = atan2(cp_x_sy, cp_x_cy);
	float roll = atan2(cp_x_sr, cp_x_cr);

	cy = cos(yaw);
	sy = sin(yaw);
	cr = cos(roll);
	sr = sin(roll);

	if (fabs(cy) > EPSILON)
	{
		cp = cp_x_cy / cy;
	}
	else if (fabs(sy) > EPSILON)
	{
		cp = cp_x_sy / sy;
	}
	else if (fabs(sr) > EPSILON)
	{
		cp = cp_x_sr / sr;
	}
	else if (fabs(cr) > EPSILON)
	{
		cp = cp_x_cr / cr;
	}
	else
	{
		cp = cos(asin(sp));
	}

	float pitch = atan2(sp, cp);

	angles[0] = pitch / (M_PI*2.f / 360.f);
	angles[1] = yaw / (M_PI*2.f / 360.f);
	angles[2] = roll / (M_PI*2.f / 360.f);

	NormalizeAngles(angles);
}

void QuatToYawPitchRoll(XrQuaternionf q, vec3_t rotation, vec3_t out)
{
	XrMatrix4x4f mat;
	XrMatrix4x4f_CreateFromQuaternion(&mat, &q);

	if (rotation[0] != 0.0f || rotation[1] != 0.0f || rotation[2] != 0.0f)
	{
		XrMatrix4x4f rotation_matrix, multiply_result;
		XrMatrix4x4f_CreateRotation(&rotation_matrix, rotation[0], rotation[1], rotation[2]);
		XrMatrix4x4f_Multiply(&multiply_result, &mat, &rotation_matrix);
		mat = multiply_result;
	}

	XrVector4f v1 = {0, 0, -1, 0};
	XrVector4f v2 = {1, 0, 0, 0};
	XrVector4f v3 = {0, 1, 0, 0};

	XrVector4f forwardInVRSpace, rightInVRSpace, upInVRSpace;
	XrMatrix4x4f_TransformVector4f(&forwardInVRSpace, &mat, &v1);
	XrMatrix4x4f_TransformVector4f(&rightInVRSpace, &mat, &v2);
	XrMatrix4x4f_TransformVector4f(&upInVRSpace, &mat, &v3);

	XrVector3f forward = {-forwardInVRSpace.z, -forwardInVRSpace.x, forwardInVRSpace.y};
	XrVector3f right = {-rightInVRSpace.z, -rightInVRSpace.x, rightInVRSpace.y};
	XrVector3f up = {-upInVRSpace.z, -upInVRSpace.x, upInVRSpace.y};

	XrVector3f_Normalize(&forward);
	XrVector3f_Normalize(&right);
	XrVector3f_Normalize(&up);

	GetAnglesFromVectors(forward, right, up, out);
}

void VR_HapticEvent(const char* event, int position, int flags, int intensity, float angle, float yHeight )
{
	if (!VR_AreHapticsEnabled())
	{
		return;
	}

	int weaponFireChannel = vr.weapon_stabilised ? 3 : (vr_righthanded->integer ? 2 : 1);
	if (strcmp(event, "pickup_shield") == 0 ||
		strcmp(event, "pickup_weapon") == 0 ||
		strstr(event, "pickup_item") != NULL)
	{
		VR_Vibrate(100, 3, 0.6);
	}
	else if (strcmp(event, "weapon_switch") == 0)
	{
		VR_Vibrate(150, vr_righthanded->integer ? 2 : 1, 0.6);
	}
	else if (strcmp(event, "shotgun") == 0 || strcmp(event, "fireball") == 0)
	{
		VR_Vibrate(250, 3, 0.85);
	}
	else if (strcmp(event, "bullet") == 0)
	{
		VR_Vibrate(150, 3, 0.65);
	}
	else if (strcmp(event, "chainsaw_fire") == 0)
	{
		VR_Vibrate(250, weaponFireChannel, 0.9);
	}
	else if (strcmp(event, "tesla_fire") == 0 || strcmp(event, "machinegun_fire") == 0)
	{
		VR_Vibrate(150, weaponFireChannel, 0.5);
	}
	else if (strcmp(event, "plasmagun_fire") == 0)
	{
		VR_Vibrate(90, weaponFireChannel, 0.75);
	}
	else if (strcmp(event, "shotgun_fire") == 0)
	{
		VR_Vibrate(150, weaponFireChannel, 1.0);
	}
	else if (strcmp(event, "rocket_fire") == 0 ||
		strcmp(event, "railgun_fire") == 0 ||
		strcmp(event, "bfg_fire") == 0 ||
		strcmp(event, "handgrenade_fire") == 0 )
	{
		VR_Vibrate(250, weaponFireChannel, 1.0);
	}
	else if (strcmp(event, "selector_icon") == 0)
	{
		VR_Vibrate(50, (vr_righthanded->integer ? 2 : 1), 0.6);
	}
	else if (strcmp(event, "menu_move") == 0)
	{
		VR_Vibrate(30, (vr.menuLeftHanded ? 1 : 2), 0.3);
	}

#ifdef USE_BHAPTICS
	VR_Bhaptics_HandleEvent(event, position, intensity, angle, yHeight);
#endif
}

XrSpace CreateActionSpace(XrAction poseAction, XrPath subactionPath)
{
	XrActionSpaceCreateInfo asci = {};
	asci.type = XR_TYPE_ACTION_SPACE_CREATE_INFO;
	asci.action = poseAction;
	asci.poseInActionSpace.orientation.w = 1.0f;
	asci.subactionPath = subactionPath;
	XrSpace actionSpace = XR_NULL_HANDLE;
	OXR(xrCreateActionSpace(VR_GetEngine()->appState.Session, &asci, &actionSpace));
	return actionSpace;
}

XrActionSuggestedBinding ActionSuggestedBinding(XrAction action, const char* bindingString)
{
	XrActionSuggestedBinding asb;
	asb.action = action;
	XrPath bindingPath;
	OXR(xrStringToPath(VR_GetEngine()->appState.Instance, bindingString, &bindingPath));
	asb.binding = bindingPath;
	return asb;
}

XrActionSet CreateActionSet(int priority, const char* name, const char* localizedName)
{
	XrActionSetCreateInfo asci = {};
	asci.type = XR_TYPE_ACTION_SET_CREATE_INFO;
	asci.next = NULL;
	asci.priority = priority;
	strcpy(asci.actionSetName, name);
	strcpy(asci.localizedActionSetName, localizedName);
	XrActionSet actionSet = XR_NULL_HANDLE;
	OXR(xrCreateActionSet(VR_GetEngine()->appState.Instance, &asci, &actionSet));
	return actionSet;
}

XrAction CreateAction(
	XrActionSet actionSet,
	XrActionType type,
	const char* actionName,
	const char* localizedName,
	int countSubactionPaths,
	XrPath* subactionPaths)
{
	XrActionCreateInfo aci = {};
	aci.type = XR_TYPE_ACTION_CREATE_INFO;
	aci.next = NULL;
	aci.actionType = type;
	if (countSubactionPaths > 0)
	{
		aci.countSubactionPaths = countSubactionPaths;
		aci.subactionPaths = subactionPaths;
	}
	strcpy(aci.actionName, actionName);
	strcpy(aci.localizedActionName, localizedName ? localizedName : actionName);
	XrAction action = XR_NULL_HANDLE;
	OXR(xrCreateAction(actionSet, &aci, &action));
	return action;
}

qboolean ActionPoseIsActive(XrAction action, XrPath subactionPath)
{
	XrActionStateGetInfo getInfo = {};
	getInfo.type = XR_TYPE_ACTION_STATE_GET_INFO;
	getInfo.action = action;
	getInfo.subactionPath = subactionPath;

	XrActionStatePose state = {};
	state.type = XR_TYPE_ACTION_STATE_POSE;
	OXR(xrGetActionStatePose(VR_GetEngine()->appState.Session, &getInfo, &state));
	return state.isActive != XR_FALSE;
}

XrActionStateFloat GetActionStateFloat(XrAction action)
{
	XrActionStateGetInfo getInfo = {};
	getInfo.type = XR_TYPE_ACTION_STATE_GET_INFO;
	getInfo.action = action;

	XrActionStateFloat state = {};
	state.type = XR_TYPE_ACTION_STATE_FLOAT;

	OXR(xrGetActionStateFloat(VR_GetEngine()->appState.Session, &getInfo, &state));
	return state;
}

XrActionStateBoolean GetActionStateBoolean(XrAction action)
{
	XrActionStateGetInfo getInfo = {};
	getInfo.type = XR_TYPE_ACTION_STATE_GET_INFO;
	getInfo.action = action;

	XrActionStateBoolean state = {};
	state.type = XR_TYPE_ACTION_STATE_BOOLEAN;

	OXR(xrGetActionStateBoolean(VR_GetEngine()->appState.Session, &getInfo, &state));
	return state;
}

XrActionStateVector2f GetActionStateVector2(XrAction action)
{
	XrActionStateGetInfo getInfo = {};
	getInfo.type = XR_TYPE_ACTION_STATE_GET_INFO;
	getInfo.action = action;

	XrActionStateVector2f state = {};
	state.type = XR_TYPE_ACTION_STATE_VECTOR2F;

	OXR(xrGetActionStateVector2f(VR_GetEngine()->appState.Session, &getInfo, &state));
	return state;
}

static int VR_SuggestBindings( VR_Engine* engine, const char* profile, const XrActionSuggestedBinding* bindings, int count )
{
	XrPath profilePath = XR_NULL_PATH;
	OXR(xrStringToPath(engine->appState.Instance, profile, &profilePath));
	XrInteractionProfileSuggestedBinding suggested = {};
	suggested.type = XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING;
	suggested.interactionProfile = profilePath;
	suggested.suggestedBindings = bindings;
	suggested.countSuggestedBindings = count;
	XrResult result = xrSuggestInteractionProfileBindings(engine->appState.Instance, &suggested);
	if (XR_FAILED(result))
	{
		// A runtime may not know every vendor's profile; the others still bind
		Com_Printf( "OpenXR %s bindings rejected (%d)\n", profile, (int)result );
		return 0;
	}
	return 1;
}

// PICO's profiles expose the same paths as Touch
static int VR_SuggestTouchBindings( VR_Engine* engine, const char* profile )
{
	XrActionSuggestedBinding bindings[32];
	int n = 0;
	bindings[n++] = ActionSuggestedBinding(indexLeftAction, "/user/hand/left/input/trigger");
	bindings[n++] = ActionSuggestedBinding(indexRightAction, "/user/hand/right/input/trigger");
	bindings[n++] = ActionSuggestedBinding(menuAction, "/user/hand/left/input/menu/click");
	bindings[n++] = ActionSuggestedBinding(buttonXAction, "/user/hand/left/input/x/click");
	bindings[n++] = ActionSuggestedBinding(buttonYAction, "/user/hand/left/input/y/click");
	bindings[n++] = ActionSuggestedBinding(buttonAAction, "/user/hand/right/input/a/click");
	bindings[n++] = ActionSuggestedBinding(buttonBAction, "/user/hand/right/input/b/click");
	bindings[n++] = ActionSuggestedBinding(gripLeftAction, "/user/hand/left/input/squeeze/value");
	bindings[n++] = ActionSuggestedBinding(gripRightAction, "/user/hand/right/input/squeeze/value");
	bindings[n++] = ActionSuggestedBinding(moveOnLeftJoystickAction, "/user/hand/left/input/thumbstick");
	bindings[n++] = ActionSuggestedBinding(moveOnRightJoystickAction, "/user/hand/right/input/thumbstick");
	bindings[n++] = ActionSuggestedBinding(thumbstickLeftClickAction, "/user/hand/left/input/thumbstick/click");
	bindings[n++] = ActionSuggestedBinding(thumbstickRightClickAction, "/user/hand/right/input/thumbstick/click");
	bindings[n++] = ActionSuggestedBinding(thumbrestLeftTouchAction, "/user/hand/left/input/thumbrest/touch");
	bindings[n++] = ActionSuggestedBinding(thumbrestRightTouchAction, "/user/hand/right/input/thumbrest/touch");
	bindings[n++] = ActionSuggestedBinding(vibrateLeftFeedback, "/user/hand/left/output/haptic");
	bindings[n++] = ActionSuggestedBinding(vibrateRightFeedback, "/user/hand/right/output/haptic");
	bindings[n++] = ActionSuggestedBinding(handPoseLeftAction, "/user/hand/left/input/grip/pose");
	bindings[n++] = ActionSuggestedBinding(handPoseRightAction, "/user/hand/right/input/grip/pose");
	bindings[n++] = ActionSuggestedBinding(aimPoseLeftAction, "/user/hand/left/input/aim/pose");
	bindings[n++] = ActionSuggestedBinding(aimPoseRightAction, "/user/hand/right/input/aim/pose");
	return VR_SuggestBindings(engine, profile, bindings, n);
}

static int VR_SuggestIndexBindings( VR_Engine* engine )
{
	XrActionSuggestedBinding bindings[32];
	int n = 0;
	bindings[n++] = ActionSuggestedBinding(indexLeftAction, "/user/hand/left/input/trigger/value");
	bindings[n++] = ActionSuggestedBinding(indexRightAction, "/user/hand/right/input/trigger/value");
	bindings[n++] = ActionSuggestedBinding(buttonXAction, "/user/hand/left/input/a/click");
	bindings[n++] = ActionSuggestedBinding(buttonYAction, "/user/hand/left/input/b/click");
	bindings[n++] = ActionSuggestedBinding(buttonAAction, "/user/hand/right/input/a/click");
	bindings[n++] = ActionSuggestedBinding(buttonBAction, "/user/hand/right/input/b/click");
	bindings[n++] = ActionSuggestedBinding(gripLeftAction, "/user/hand/left/input/squeeze/value");
	bindings[n++] = ActionSuggestedBinding(gripRightAction, "/user/hand/right/input/squeeze/value");
	bindings[n++] = ActionSuggestedBinding(trackpadLeftAction, "/user/hand/left/input/trackpad/force");
	bindings[n++] = ActionSuggestedBinding(trackpadRightAction, "/user/hand/right/input/trackpad/force");
	bindings[n++] = ActionSuggestedBinding(moveOnLeftJoystickAction, "/user/hand/left/input/thumbstick");
	bindings[n++] = ActionSuggestedBinding(moveOnRightJoystickAction, "/user/hand/right/input/thumbstick");
	bindings[n++] = ActionSuggestedBinding(thumbstickLeftClickAction, "/user/hand/left/input/thumbstick/click");
	bindings[n++] = ActionSuggestedBinding(thumbstickRightClickAction, "/user/hand/right/input/thumbstick/click");
	bindings[n++] = ActionSuggestedBinding(vibrateLeftFeedback, "/user/hand/left/output/haptic");
	bindings[n++] = ActionSuggestedBinding(vibrateRightFeedback, "/user/hand/right/output/haptic");
	bindings[n++] = ActionSuggestedBinding(handPoseLeftAction, "/user/hand/left/input/grip/pose");
	bindings[n++] = ActionSuggestedBinding(handPoseRightAction, "/user/hand/right/input/grip/pose");
	bindings[n++] = ActionSuggestedBinding(aimPoseLeftAction, "/user/hand/left/input/aim/pose");
	bindings[n++] = ActionSuggestedBinding(aimPoseRightAction, "/user/hand/right/input/aim/pose");
	return VR_SuggestBindings(engine, "/interaction_profiles/valve/index_controller", bindings, n);
}

static int VR_SuggestSimpleBindings( VR_Engine* engine )
{
	XrActionSuggestedBinding bindings[16];
	int n = 0;
	bindings[n++] = ActionSuggestedBinding(indexLeftAction, "/user/hand/left/input/select/click");
	bindings[n++] = ActionSuggestedBinding(indexRightAction, "/user/hand/right/input/select/click");
	bindings[n++] = ActionSuggestedBinding(menuAction, "/user/hand/left/input/menu/click");
	bindings[n++] = ActionSuggestedBinding(menuAction, "/user/hand/right/input/menu/click");
	bindings[n++] = ActionSuggestedBinding(vibrateLeftFeedback, "/user/hand/left/output/haptic");
	bindings[n++] = ActionSuggestedBinding(vibrateRightFeedback, "/user/hand/right/output/haptic");
	bindings[n++] = ActionSuggestedBinding(handPoseLeftAction, "/user/hand/left/input/grip/pose");
	bindings[n++] = ActionSuggestedBinding(handPoseRightAction, "/user/hand/right/input/grip/pose");
	bindings[n++] = ActionSuggestedBinding(aimPoseLeftAction, "/user/hand/left/input/aim/pose");
	bindings[n++] = ActionSuggestedBinding(aimPoseRightAction, "/user/hand/right/input/aim/pose");
	return VR_SuggestBindings(engine, "/interaction_profiles/khr/simple_controller", bindings, n);
}

// The Frame puts A/B/X/Y and Menu on the right controller, the D-pad and View on the left
static int VR_SuggestFrameBindings( VR_Engine* engine )
{
	XrActionSuggestedBinding bindings[32];
	int n = 0;
	bindings[n++] = ActionSuggestedBinding(indexLeftAction, "/user/hand/left/input/trigger/value");
	bindings[n++] = ActionSuggestedBinding(indexRightAction, "/user/hand/right/input/trigger/value");
	bindings[n++] = ActionSuggestedBinding(menuAction, "/user/hand/right/input/menu/click");
	bindings[n++] = ActionSuggestedBinding(buttonAAction, "/user/hand/right/input/a/click");
	bindings[n++] = ActionSuggestedBinding(buttonBAction, "/user/hand/right/input/b/click");
	bindings[n++] = ActionSuggestedBinding(buttonXAction, "/user/hand/right/input/x/click");
	bindings[n++] = ActionSuggestedBinding(buttonYAction, "/user/hand/right/input/y/click");
	bindings[n++] = ActionSuggestedBinding(gripLeftAction, "/user/hand/left/input/squeeze/value");
	bindings[n++] = ActionSuggestedBinding(gripRightAction, "/user/hand/right/input/squeeze/value");
	bindings[n++] = ActionSuggestedBinding(gripClickLeftAction, "/user/hand/left/input/squeeze/click");
	bindings[n++] = ActionSuggestedBinding(gripClickRightAction, "/user/hand/right/input/squeeze/click");
	bindings[n++] = ActionSuggestedBinding(bumperLeftAction, "/user/hand/left/input/bumper/click");
	bindings[n++] = ActionSuggestedBinding(bumperRightAction, "/user/hand/right/input/bumper/click");
	bindings[n++] = ActionSuggestedBinding(dpadUpAction, "/user/hand/left/input/dpad_up/click");
	bindings[n++] = ActionSuggestedBinding(dpadDownAction, "/user/hand/left/input/dpad_down/click");
	bindings[n++] = ActionSuggestedBinding(dpadLeftAction, "/user/hand/left/input/dpad_left/click");
	bindings[n++] = ActionSuggestedBinding(dpadRightAction, "/user/hand/left/input/dpad_right/click");
	bindings[n++] = ActionSuggestedBinding(viewAction, "/user/hand/left/input/view/click");
	bindings[n++] = ActionSuggestedBinding(moveOnLeftJoystickAction, "/user/hand/left/input/thumbstick");
	bindings[n++] = ActionSuggestedBinding(moveOnRightJoystickAction, "/user/hand/right/input/thumbstick");
	bindings[n++] = ActionSuggestedBinding(thumbstickLeftClickAction, "/user/hand/left/input/thumbstick/click");
	bindings[n++] = ActionSuggestedBinding(thumbstickRightClickAction, "/user/hand/right/input/thumbstick/click");
	bindings[n++] = ActionSuggestedBinding(vibrateLeftFeedback, "/user/hand/left/output/haptic");
	bindings[n++] = ActionSuggestedBinding(vibrateRightFeedback, "/user/hand/right/output/haptic");
	bindings[n++] = ActionSuggestedBinding(handPoseLeftAction, "/user/hand/left/input/grip/pose");
	bindings[n++] = ActionSuggestedBinding(handPoseRightAction, "/user/hand/right/input/grip/pose");
	bindings[n++] = ActionSuggestedBinding(aimPoseLeftAction, "/user/hand/left/input/aim/pose");
	bindings[n++] = ActionSuggestedBinding(aimPoseRightAction, "/user/hand/right/input/aim/pose");
	return VR_SuggestBindings(engine, "/interaction_profiles/valve/frame_controller_valve", bindings, n);
}

void VR_InitInstanceInput( VR_Engine* engine )
{
	// Actions
	runningActionSet = CreateActionSet(1, "running_action_set", "Action Set used on main loop");
	indexLeftAction = CreateAction(runningActionSet, XR_ACTION_TYPE_FLOAT_INPUT, "index_left", "Index left", 0, NULL);
	indexRightAction = CreateAction(runningActionSet, XR_ACTION_TYPE_FLOAT_INPUT, "index_right", "Index right", 0, NULL);
	menuAction = CreateAction(runningActionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "menu_action", "Menu", 0, NULL);
	buttonAAction = CreateAction(runningActionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "button_a", "Button A", 0, NULL);
	buttonBAction = CreateAction(runningActionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "button_b", "Button B", 0, NULL);
	buttonXAction = CreateAction(runningActionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "button_x", "Button X", 0, NULL);
	buttonYAction = CreateAction(runningActionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "button_y", "Button Y", 0, NULL);
	gripLeftAction = CreateAction(runningActionSet, XR_ACTION_TYPE_FLOAT_INPUT, "grip_left", "Grip left", 0, NULL);
	gripRightAction = CreateAction(runningActionSet, XR_ACTION_TYPE_FLOAT_INPUT, "grip_right", "Grip right", 0, NULL);
	trackpadLeftAction = CreateAction(runningActionSet, XR_ACTION_TYPE_FLOAT_INPUT, "trackpad_left", "Trackpad left", 0, NULL);
	trackpadRightAction = CreateAction(runningActionSet, XR_ACTION_TYPE_FLOAT_INPUT, "trackpad_right", "Trackpad right", 0, NULL);
	moveOnLeftJoystickAction = CreateAction(runningActionSet, XR_ACTION_TYPE_VECTOR2F_INPUT, "move_on_left_joy", "Move on left Joy", 0, NULL);
	moveOnRightJoystickAction = CreateAction(runningActionSet, XR_ACTION_TYPE_VECTOR2F_INPUT, "move_on_right_joy", "Move on right Joy", 0, NULL);
	thumbstickLeftClickAction = CreateAction(runningActionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "thumbstick_left", "Thumbstick left", 0, NULL);
	thumbstickRightClickAction = CreateAction(runningActionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "thumbstick_right", "Thumbstick right", 0, NULL);
	thumbrestLeftTouchAction = CreateAction(runningActionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "thumbrest_left_touch", "Thumbrest Left Touch", 0, NULL);
	thumbrestRightTouchAction = CreateAction(runningActionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "thumbrest_right_touch", "Thumbrest Right Touch", 0, NULL);
	vibrateLeftFeedback = CreateAction(runningActionSet, XR_ACTION_TYPE_VIBRATION_OUTPUT, "vibrate_left_feedback", "Vibrate Left Controller Feedback", 0, NULL);
	vibrateRightFeedback = CreateAction(runningActionSet, XR_ACTION_TYPE_VIBRATION_OUTPUT, "vibrate_right_feedback", "Vibrate Right Controller Feedback", 0, NULL);
	bumperLeftAction = CreateAction(runningActionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "bumper_left", "Bumper left", 0, NULL);
	bumperRightAction = CreateAction(runningActionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "bumper_right", "Bumper right", 0, NULL);
	dpadUpAction = CreateAction(runningActionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "dpad_up", "D-pad up", 0, NULL);
	dpadDownAction = CreateAction(runningActionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "dpad_down", "D-pad down", 0, NULL);
	dpadLeftAction = CreateAction(runningActionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "dpad_left", "D-pad left", 0, NULL);
	dpadRightAction = CreateAction(runningActionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "dpad_right", "D-pad right", 0, NULL);
	viewAction = CreateAction(runningActionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "view", "View", 0, NULL);
	gripClickLeftAction = CreateAction(runningActionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "grip_click_left", "Grip click left", 0, NULL);
	gripClickRightAction = CreateAction(runningActionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "grip_click_right", "Grip click right", 0, NULL);

	OXR(xrStringToPath(engine->appState.Instance, "/user/hand/left", &leftHandPath));
	OXR(xrStringToPath(engine->appState.Instance, "/user/hand/right", &rightHandPath));
	handPoseLeftAction = CreateAction(runningActionSet, XR_ACTION_TYPE_POSE_INPUT, "hand_pose_left", NULL, 1, &leftHandPath);
	handPoseRightAction = CreateAction(runningActionSet, XR_ACTION_TYPE_POSE_INPUT, "hand_pose_right", NULL, 1, &rightHandPath);
	aimPoseLeftAction = CreateAction(runningActionSet, XR_ACTION_TYPE_POSE_INPUT, "aim_pose_left", NULL, 1, &leftHandPath);
	aimPoseRightAction = CreateAction(runningActionSet, XR_ACTION_TYPE_POSE_INPUT, "aim_pose_right", NULL, 1, &rightHandPath);

	// Every profile is suggested; the runtime binds whichever matches the connected controllers
	int accepted = 0;
	if (useSimpleProfile)
	{
		accepted += VR_SuggestSimpleBindings(engine);
	}
	else
	{
		accepted += VR_SuggestTouchBindings(engine, "/interaction_profiles/oculus/touch_controller");
		if (VR_HasPicoControllers())
		{
			accepted += VR_SuggestTouchBindings(engine, "/interaction_profiles/bytedance/pico4s_controller");
			accepted += VR_SuggestTouchBindings(engine, "/interaction_profiles/bytedance/pico4_controller");
		}
		accepted += VR_SuggestIndexBindings(engine);
		if (VR_HasFrameControllers())
			accepted += VR_SuggestFrameBindings(engine);
		accepted += VR_SuggestSimpleBindings(engine);
	}
	if (!accepted)
	{
		CHECK(XR_FALSE, "Failed to find supported controller bindings");
	}
}

void VR_InitSessionInput( VR_Engine* engine )
{
	if (inputInitialized)
	{
		return;
	}

	// a rebuilt session must not inherit holds: release them, then (re)register the router's commands
	VR_Router_Init();

	leftControllerGripSpace = CreateActionSpace(handPoseLeftAction, leftHandPath);
	rightControllerGripSpace = CreateActionSpace(handPoseRightAction, rightHandPath);
	leftControllerAimSpace = CreateActionSpace(aimPoseLeftAction, leftHandPath);
	rightControllerAimSpace = CreateActionSpace(aimPoseRightAction, rightHandPath);

	XrSessionActionSetsAttachInfo attachInfo = {};
	attachInfo.type = XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO;
	attachInfo.next = NULL;
	attachInfo.countActionSets = 1;
	attachInfo.actionSets = &runningActionSet;
	XR_CHECK(xrAttachSessionActionSets(engine->appState.Session, &attachInfo), "");

	VR_UpdateInteractionProfiles();

	inputInitialized = qtrue;
}

void VR_DestroySessionInput( VR_Engine* engine )
{
	VR_Router_Reset();
	// This will allow to recreate session-specific OpenXR input objects
	inputInitialized = qfalse;
	vrCurrentProfile[0] = vrCurrentProfile[1] = -1;
}

/*
==================
IN_VRScreenCursor

Where a controller's aim ray meets the virtual screen, in 640x480 menu coordinates.
==================
*/
static qboolean IN_VRScreenCursor( int hand, float *x, float *y )
{
	const XrVector3f ahead = { 0.0f, 0.0f, -1.0f };
	XrVector3f direction;
	float origin[3], dir[3];

	if ( !aimPoseValid[hand] )
	{
		return qfalse;
	}
	XrQuaternionf_RotateVector3f( &direction, &aimPose[hand].orientation, &ahead );
	origin[0] = aimPose[hand].position.x;
	origin[1] = aimPose[hand].position.y;
	origin[2] = aimPose[hand].position.z;
	dir[0] = direction.x;
	dir[1] = direction.y;
	dir[2] = direction.z;
	return VR_VirtualScreen_Hit( origin, dir, x, y );
}

/* Draws one hand's ray this frame, toward its cursor. qfalse when the hand has no aim or no screen is up. */
qboolean IN_VRShowPointer( int hand, int cursorX, int cursorY, qboolean *onScreen )
{
	const XrVector3f ahead = { 0.0f, 0.0f, -1.0f };
	XrVector3f direction;
	float origin[3], dir[3];

	*onScreen = qfalse;
	if ( !aimPoseValid[hand] )
	{
		return qfalse;
	}
	XrQuaternionf_RotateVector3f( &direction, &aimPose[hand].orientation, &ahead );
	origin[0] = aimPose[hand].position.x;
	origin[1] = aimPose[hand].position.y;
	origin[2] = aimPose[hand].position.z;
	dir[0] = direction.x;
	dir[1] = direction.y;
	dir[2] = direction.z;
	// the keyboard tells the hands apart by color: the left one blue, the right one red
	return VR_VirtualScreen_ShowPointer( hand, origin, dir, cursorX, cursorY, hand == 0 && VKeyboard_IsActive(), onScreen );
}

/* One hand's pointer: its ray on the virtual screen (a miss holds the cursor), else its aim angles on the HUD plane. */
static void IN_VRCursor( int hand, const vec3_t aim, int *x, int *y )
{
	int targetX = *x, targetY = *y;

	if ( vr.virtual_screen )
	{
		float hitX, hitY;
		if ( IN_VRScreenCursor( hand, &hitX, &hitY ) )
		{
			targetX = (int)hitX;
			targetY = (int)hitY;
		}
	}
	else
	{
		// During SP intermission the HUD is world-fixed, so the anchored yaw is the reference
		const float referenceYaw = vr.sp_intermission_active ? vr.sp_intermission_yaw : vr.menuYaw;
		const float yaw = Com_Clamp( -85, 85, AngleSubtract( aim[YAW], referenceYaw ) );
		const float pitch = Com_Clamp( -85, 85, aim[PITCH] );
		targetX = (int)Com_Clamp( -8000, 8000, 320 - tanf( yaw * (float)M_PI / 180 ) * 800 );
		targetY = (int)Com_Clamp( -8000, 8000, 240 + tanf( pitch * (float)M_PI / 180 ) * 800 );
	}
	// Smooth toward the target so rapid hand or head motion does not jitter the pointer
	*x = (int)( 0.5f * targetX + 0.5f * *x );
	*y = (int)( 0.5f * targetY + 0.5f * *y );
}

static void IN_VRController( qboolean isRightController, XrPosef pose )
{
	//Set gun angles - We need to calculate all those we might need (including adjustments) for the client to then take its pick
	vec3_t rotation = {0};
	if (isRightController == (vr_righthanded->integer != 0))
	{
		//Set gun angles - We need to calculate all those we might need (including adjustments) for the client to then take its pick
		rotation[PITCH] = VR_GRIP_TO_AIM_PITCH + VR_ProfilePitch(isRightController ? 1 : 0) + vr_weaponPitch->value;
		QuatToYawPitchRoll(pose.orientation, rotation, vr.weaponangles);

		VectorSubtract(vr.weaponangles_last, vr.weaponangles, vr.weaponangles_delta);
		VectorCopy(vr.weaponangles, vr.weaponangles_last);

		///Weapon location relative to view
		vr.weaponposition[0] = pose.position.x;
		vr.weaponposition[1] = pose.position.y + vr_heightAdjust->value;
		vr.weaponposition[2] = pose.position.z;

		VectorCopy(vr.weaponoffset_last[1], vr.weaponoffset_last[0]);
		VectorCopy(vr.weaponoffset, vr.weaponoffset_last[1]);
		VectorSubtract(vr.weaponposition, vr.hmdposition, vr.weaponoffset);
	}
	else
	{
		rotation[PITCH] = VR_GRIP_TO_AIM_PITCH + VR_ProfilePitch(isRightController ? 1 : 0) + vr_weaponPitch->value;
		QuatToYawPitchRoll(pose.orientation, rotation, vr.offhandangles);
		// Steering follows the corrected grip angles too: "forward" is how the hand is held, not the aim ray
		VectorCopy(vr.offhandangles, vr.offhandangles2);

		///location relative to view
		vr.offhandposition[0] = pose.position.x;
		vr.offhandposition[1] = pose.position.y + vr_heightAdjust->value;
		vr.offhandposition[2] = pose.position.z;

		VectorCopy(vr.offhandoffset_last[1], vr.offhandoffset_last[0]);
		VectorCopy(vr.offhandoffset, vr.offhandoffset_last[1]);
		VectorSubtract(vr.offhandposition, vr.hmdposition, vr.offhandoffset);
	}

	// The cursor follows whichever context owns the pointer (menu, text entry, scoreboard)
	if (VR_Router_PointerLayer())
	{
		vr.weapon_zoomed = qfalse;
		if (vr.menuCursorActive)
		{
			// Both hands' pointers, so a click from either hand lands where that hand points
			const int menuHand = vr.menuLeftHanded ? 0 : 1;
			const qboolean menuWeapon = (vr_righthanded->integer != 0) == (menuHand == 1);
			IN_VRCursor(menuHand, menuWeapon ? vr.weaponaimangles : vr.offhandaimangles, &vr.menuCursorX, &vr.menuCursorY);
			IN_VRCursor(1 - menuHand, menuWeapon ? vr.offhandaimangles : vr.weaponaimangles, &vr.offhandCursorX, &vr.offhandCursorY);

			// UI_MOUSE_EVENT updates hover; stick navigation owns the selection while it runs
			if ((Key_GetCatcher() & KEYCATCH_UI) && vr.pointerMode != VR_POINTER_STICK && !VKeyboard_IsActive() &&
				!vr.weapon_adjust && !vr.menuYawLocked)
			{
				CL_MouseEvent(0, 0, com_frameTime);
			}
		}
		if (vr.scoreboardCursorActive)
		{
			if (vr.virtual_screen)
			{
				vr.scoreboardCursorX = vr.menuCursorX;
				vr.scoreboardCursorY = vr.menuCursorY;
			}
			else
			{
				const int menuHand = vr.menuLeftHanded ? 0 : 1;
				const float *aim = (vr_righthanded->integer != 0) == (menuHand == 1) ? vr.weaponaimangles : vr.offhandaimangles;
				vr.scoreboardCursorX = (int)Com_Clamp( 0, 640,
					320 - tanf( Com_Clamp( -85, 85, AngleSubtract( aim[YAW], vr.menuYaw ) ) * (float)M_PI / 180 ) * 400 );
				vr.scoreboardCursorY = (int)Com_Clamp( 0, 480,
					240 + tanf( Com_Clamp( -85, 85, aim[PITCH] ) * (float)M_PI / 180 ) * 400 );
			}
		}
	}
	else
	{
		vr.weapon_zoomed = vr_weaponScope->integer &&
												vr.weapon_stabilised &&
												(cl.snap.ps.weapon == WP_RAILGUN) &&
												(VectorLength(vr.weaponoffset) < 0.24f) &&
												cl.snap.ps.stats[STAT_HEALTH] > 0;

		if (vr_twoHandedWeapons->integer && vr.weapon_stabilised)
		{
			if (vr_twoHandedWeapons->integer == 2) // Virtual gun stock
			{
				// Offset to the appropriate eye a little bit
				vec2_t xy;
				rotateAboutOrigin(Cvar_VariableValue("cg_stereoSeparation") / 2.0f, 0.0f, -vr.hmdorientation[YAW], xy);
				float x = vr.offhandposition[0] - (vr.hmdposition[0] + xy[0]);
				float y = vr.offhandposition[1] - (vr.hmdposition[1] - 0.1f); // Use a point lower
				float z = vr.offhandposition[2] - (vr.hmdposition[2] + xy[1]);

				float zxDist = length(x, z);

				if (zxDist != 0.0f && z != 0.0f)
				{
					VectorSet(vr.weaponangles, -RadiansToDegrees(atanf(y / zxDist)), -RadiansToDegrees(atan2f(x, -z)), 0);
				}
			}
			else // Basic two-handed
			{
				// Apply smoothing to the weapon hand
				vec3_t smooth_weaponoffset;
				VectorAdd(vr.weaponoffset, vr.weaponoffset_last[0], smooth_weaponoffset);
				VectorAdd(smooth_weaponoffset, vr.weaponoffset_last[1],smooth_weaponoffset);
				VectorScale(smooth_weaponoffset, 1.0f/3.0f, smooth_weaponoffset);

				vec3_t vec;
				VectorSubtract(vr.offhandoffset, smooth_weaponoffset, vec);

				float zxDist = length(vec[0], vec[2]);

				if (zxDist != 0.0f && vec[2] != 0.0f)
				{
					VectorSet(vr.weaponangles, -RadiansToDegrees(atanf(vec[1] / zxDist)), -RadiansToDegrees(atan2f(vec[0], -vec[2])), vr.weaponangles[ROLL] / 2.0f); // Dampen roll on stabilised weapon
				}
			}
		}
	}
}

// The runtime's FOV, before VR_PublishFov decides what the frame sees
static float rawFovX, rawFovUp, rawFovDown;

/*
==================
VR_PublishFov

The virtual screen is a monitor: the player's cg_fov across a symmetric 4:3
crop, so nothing on it follows the runtime's per-frame FOV.
==================
*/
static void VR_PublishFov( void )
{
	if ( vr.virtual_screen )
	{
		const float halfSpan = 0.5f * ( rawFovUp - rawFovDown );
		float fovX = Cvar_VariableValue( "cg_fov" );

		if ( fovX < 1.0f )
			fovX = 90.0f;
		else if ( fovX > 160.0f )
			fovX = 160.0f;
		vr.fov_x = fovX;
		vr.fov_angle_up = halfSpan;
		vr.fov_angle_down = -halfSpan;
	}
	else
	{
		vr.fov_x = rawFovX;
		vr.fov_angle_up = rawFovUp;
		vr.fov_angle_down = rawFovDown;
	}
}

void VR_RefreshDerivedModeState( void )
{
	// every frame: single_player only arrives at the modules' first sync-out
	vr.use_6dof = vr.single_player && vr_6dof->integer;

	vr.follow_mode = VR_FollowModeFor( Cvar_VariableIntegerValue( "cg_followMode" ), tvPlay.active );

	vr.virtual_screen = VR_Gameplay_ShouldRenderInVirtualScreen();
	vr.first_person_following = vr.virtual_screen && VR_IsFollowingInFirstPerson();
	vr.in_menu = VR_IsInMenu();

	VR_PublishFov();
}

/* The engine's per-hand sample from this port's actions; one-hand buttons land on the hand that carries them. */
static void VR_SampleHands( clXRHandInput_t hands[2] )
{
	XrAction trigger[2] = { indexLeftAction, indexRightAction }, grip[2] = { gripLeftAction, gripRightAction };
	XrAction stick[2] = { moveOnLeftJoystickAction, moveOnRightJoystickAction };
	XrAction click[2] = { thumbstickLeftClickAction, thumbstickRightClickAction };
	XrAction rest[2] = { thumbrestLeftTouchAction, thumbrestRightTouchAction };
	XrAction bumper[2] = { bumperLeftAction, bumperRightAction }, pad[2] = { trackpadLeftAction, trackpadRightAction };
	XrAction gripClick[2] = { gripClickLeftAction, gripClickRightAction };
	const int frame = vrCurrentProfile[1] == CL_XRP_FRAME || vrCurrentProfile[0] == CL_XRP_FRAME;
	int h;

	memset( hands, 0, sizeof( clXRHandInput_t ) * 2 );
	for ( h = 0; h < 2; h++ ) {
		XrActionStateFloat t = GetActionStateFloat( trigger[h] ), g = GetActionStateFloat( grip[h] );
		XrActionStateVector2f s = GetActionStateVector2( stick[h] );
		hands[h].profile = vrCurrentProfile[h];
		hands[h].grip.positionValid = VR_GetEngine()->appState.TrackedController[h].Active;
		hands[h].aim.orientationValid = aimOrientationValid[h];
		hands[h].active = t.isActive || g.isActive || s.isActive;
		hands[h].trigger = t.currentState;
		hands[h].squeeze = g.isActive ? g.currentState : 0;
		hands[h].stick[0] = s.currentState.x;
		hands[h].stick[1] = s.currentState.y;
		hands[h].trackpad = GetActionStateFloat( pad[h] ).currentState;
		if ( GetActionStateBoolean( click[h] ).currentState ) hands[h].buttons |= CL_XRI_STICK_BUTTON;
		if ( GetActionStateBoolean( rest[h] ).currentState ) hands[h].buttons |= CL_XRI_THUMBREST_BUTTON;
		if ( GetActionStateBoolean( bumper[h] ).currentState ) hands[h].buttons |= CL_XRI_BUMPER_BUTTON;
		if ( GetActionStateBoolean( gripClick[h] ).currentState ) hands[h].buttons |= CL_XRI_SQUEEZE_CLICK_BUTTON;
	}
	/* Touch/PICO/Index: left X/Y and right A/B are each hand's primary/secondary; the Frame keeps A/B/X/Y on the right. */
	if ( frame ) {
		if ( GetActionStateBoolean( buttonAAction ).currentState ) hands[1].buttons |= CL_XRI_PRIMARY_BUTTON;
		if ( GetActionStateBoolean( buttonBAction ).currentState ) hands[1].buttons |= CL_XRI_SECONDARY_BUTTON;
		if ( GetActionStateBoolean( buttonXAction ).currentState ) hands[1].buttons |= CL_XRI_X_BUTTON;
		if ( GetActionStateBoolean( buttonYAction ).currentState ) hands[1].buttons |= CL_XRI_Y_BUTTON;
		if ( GetActionStateBoolean( menuAction ).currentState ) hands[1].buttons |= CL_XRI_MENU_BUTTON;
		if ( GetActionStateBoolean( viewAction ).currentState ) hands[0].buttons |= CL_XRI_VIEW_BUTTON;
		if ( GetActionStateBoolean( dpadUpAction ).currentState ) hands[0].buttons |= CL_XRI_DPAD_UP_BUTTON;
		if ( GetActionStateBoolean( dpadDownAction ).currentState ) hands[0].buttons |= CL_XRI_DPAD_DOWN_BUTTON;
		if ( GetActionStateBoolean( dpadLeftAction ).currentState ) hands[0].buttons |= CL_XRI_DPAD_LEFT_BUTTON;
		if ( GetActionStateBoolean( dpadRightAction ).currentState ) hands[0].buttons |= CL_XRI_DPAD_RIGHT_BUTTON;
	} else {
		if ( GetActionStateBoolean( buttonXAction ).currentState ) hands[0].buttons |= CL_XRI_PRIMARY_BUTTON;
		if ( GetActionStateBoolean( buttonYAction ).currentState ) hands[0].buttons |= CL_XRI_SECONDARY_BUTTON;
		if ( GetActionStateBoolean( buttonAAction ).currentState ) hands[1].buttons |= CL_XRI_PRIMARY_BUTTON;
		if ( GetActionStateBoolean( buttonBAction ).currentState ) hands[1].buttons |= CL_XRI_SECONDARY_BUTTON;
		/* Touch's menu is on the left. Simple binds menu on both hands into one action, so it reports on both:
		 * an inactive hand is skipped, and a lone Simple controller may be either hand. */
		if ( GetActionStateBoolean( menuAction ).currentState ) {
			hands[0].buttons |= CL_XRI_MENU_BUTTON;
			if ( vrCurrentProfile[0] == CL_XRP_SIMPLE || vrCurrentProfile[1] == CL_XRP_SIMPLE )
				hands[1].buttons |= CL_XRI_MENU_BUTTON;
		}
	}
}

void VR_ProcessInputActions( void )
{
	VR_RefreshDerivedModeState();

	vr.right_handed = vr_righthanded->integer != 0;

#ifdef USE_BHAPTICS
	VR_Bhaptics_UpdateEnabled();
#endif

	VR_ProcessHaptics();

	{
		clXRHandInput_t hands[2];
		VR_SampleHands( hands );
		if ( VR_GetEngine()->appState.Focused )
			VR_Router_Frame( hands );
		else
			// the system has input focus: release every hold from the frame, never from the event handler
			VR_Router_Reset();
	}
}

void IN_VRSyncActions( VR_Engine* engine )
{
	// sync action data
	XrActiveActionSet activeActionSet = {};
	activeActionSet.actionSet = runningActionSet;
	activeActionSet.subactionPath = XR_NULL_PATH;

	XrActionsSyncInfo syncInfo = {};
	syncInfo.type = XR_TYPE_ACTIONS_SYNC_INFO;
	syncInfo.next = NULL;
	syncInfo.countActiveActionSets = 1;
	syncInfo.activeActionSets = &activeActionSet;
	XR_CHECK(xrSyncActions(engine->appState.Session, &syncInfo), "failed to sync actions");
}

// Aim pose angles with no pitch offset: they drive the cursor, never the weapon
static void IN_VRControllerAim( qboolean isRightController, XrPosef pose )
{
	vec3_t rotation = {0};

	aimPose[isRightController ? 1 : 0] = pose;

	if (isRightController == (vr_righthanded->integer != 0))
	{
		QuatToYawPitchRoll(pose.orientation, rotation, vr.weaponaimangles);
	}
	else
	{
		QuatToYawPitchRoll(pose.orientation, rotation, vr.offhandaimangles);
	}
}

void IN_VRUpdateControllers( VR_Engine* engine, XrTime predictedDisplayTime )
{
	//get controller poses
	XrAction controller[] = {handPoseLeftAction, handPoseRightAction};
	XrPath subactionPath[] = {leftHandPath, rightHandPath};
	XrSpace controllerSpace[] = {leftControllerGripSpace, rightControllerGripSpace};
	for (int i = 0; i < 2; i++)
	{
		if (ActionPoseIsActive(controller[i], subactionPath[i]))
		{
			XrSpaceLocation loc = {};
			loc.type = XR_TYPE_SPACE_LOCATION;
			OXR(xrLocateSpace(controllerSpace[i], engine->appState.CurrentSpace, predictedDisplayTime, &loc));

			engine->appState.TrackedController[i].Active = (loc.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) != 0;
			engine->appState.TrackedController[i].Pose = loc.pose;
		}
		else
		{
			XrPosef posef_identity = {};
			posef_identity.orientation.w = 1.0;

			engine->appState.TrackedController[i].Active = VR_FALSE;
			engine->appState.TrackedController[i].Pose = posef_identity;
		}
	}

	{
		XrAction aimAction[] = {aimPoseLeftAction, aimPoseRightAction};
		XrSpace aimSpace[] = {leftControllerAimSpace, rightControllerAimSpace};
		int i;

		for (i = 0; i < 2; i++)
		{
			aimPoseValid[i] = qfalse;
			aimOrientationValid[i] = qfalse;

			XrSpaceLocation loc = {};
			loc.type = XR_TYPE_SPACE_LOCATION;

			if (aimSpace[i] == XR_NULL_HANDLE || !ActionPoseIsActive(aimAction[i], subactionPath[i]))
			{
				continue;
			}
			OXR(xrLocateSpace(aimSpace[i], engine->appState.CurrentSpace, predictedDisplayTime, &loc));
			if ((loc.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) != 0)
			{
				IN_VRControllerAim(i == 1 ? qtrue : qfalse, loc.pose);
			}
			// The ray starts at the controller, so an untracked position would cast it from a stale origin
			aimOrientationValid[i] = (loc.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) != 0;
			aimPoseValid[i] = (loc.locationFlags & (XR_SPACE_LOCATION_ORIENTATION_VALID_BIT | XR_SPACE_LOCATION_POSITION_VALID_BIT)) ==
				(XR_SPACE_LOCATION_ORIENTATION_VALID_BIT | XR_SPACE_LOCATION_POSITION_VALID_BIT) ? qtrue : qfalse;
		}
	}

	//apply controller poses
	if (engine->appState.TrackedController[0].Active)
	{
		IN_VRController(qfalse, engine->appState.TrackedController[0].Pose);
	}
	if (engine->appState.TrackedController[1].Active)
	{
		IN_VRController(qtrue, engine->appState.TrackedController[1].Pose);
	}
}

void IN_VRUpdateHMD( XrView* views, uint32_t viewCount, XrFovf* fov )
{
	// Set FOV
	memset(fov, 0, sizeof(XrFovf));
	for (uint32_t view = 0; view < viewCount; view++)
	{
		fov->angleLeft += views[view].fov.angleLeft / (float)(viewCount);
		fov->angleRight += views[view].fov.angleRight / (float)(viewCount);
		fov->angleUp += views[view].fov.angleUp / (float)(viewCount);
		fov->angleDown += views[view].fov.angleDown / (float)(viewCount);
	}
	rawFovX = (fabs(fov->angleLeft) + fabs(fov->angleRight)) * 180.0f / M_PI;
	vr.fov_y = (fabs(fov->angleUp) + fabs(fov->angleDown)) * 180.0f / M_PI;
	// Store FOV angles in radians for projection center calculations
	rawFovUp = fov->angleUp;
	rawFovDown = fov->angleDown;
	VR_PublishFov();
	vr.fov_angle_left = fov->angleLeft;
	vr.fov_angle_right = fov->angleRight;

	// Store per-eye FOV angles for asymmetric stereo rendering
	for (uint32_t eye = 0; eye < viewCount && eye < 2; eye++)
	{
		vr.eye_fov_angle_left[eye] = views[eye].fov.angleLeft;
		vr.eye_fov_angle_right[eye] = views[eye].fov.angleRight;
	}

	// Get center HMD pose (view center) and center rotation (middle rotation)
	XrVector3f hmdPosition = views[0].pose.position;
	XrQuaternionf hmdRotation = views[0].pose.orientation;
	if (viewCount > 1)
	{
		CHECK(viewCount == 2, "Unexpected number of views");
		XrVector3f_Lerp(&hmdPosition, &views[0].pose.position, &views[1].pose.position, 0.5f);
		XrQuaternionf_Lerp(&hmdRotation, &views[0].pose.orientation, &views[1].pose.orientation, 0.5f);
	}

	// Last position and orientation
	vec3_t hmdposition_last, hmdorientation_last;
	VectorCopy(vr.hmdposition, hmdposition_last);
	VectorCopy(vr.hmdorientation, hmdorientation_last);

	// We extract Yaw, Pitch, Roll instead of directly using the orientation
	// to allow "additional" yaw manipulation with mouse/controller.
	vec3_t rotation = {0, 0, 0};
	QuatToYawPitchRoll(hmdRotation, rotation, vr.hmdorientation);

	// Position
	VectorSet(vr.hmdposition, hmdPosition.x, hmdPosition.y + vr_heightAdjust->value, hmdPosition.z);

	// Per-eye positions (in VR/HMD space, includes height adjust)
	for (uint32_t eye = 0; eye < viewCount && eye < 2; eye++)
	{
		VectorSet(vr.hmdposition_eye[eye],
			views[eye].pose.position.x,
			views[eye].pose.position.y + vr_heightAdjust->value,
			views[eye].pose.position.z);
	}

	// Store raw OpenXR poses for direct renderer access (view matrix construction)
	// Include height adjustment in the pose position
	for (uint32_t eye = 0; eye < viewCount && eye < 2; eye++)
	{
		vr.eyePose[eye].orientation.x = views[eye].pose.orientation.x;
		vr.eyePose[eye].orientation.y = views[eye].pose.orientation.y;
		vr.eyePose[eye].orientation.z = views[eye].pose.orientation.z;
		vr.eyePose[eye].orientation.w = views[eye].pose.orientation.w;
		vr.eyePose[eye].position.x = views[eye].pose.position.x;
		vr.eyePose[eye].position.y = views[eye].pose.position.y + vr_heightAdjust->value;
		vr.eyePose[eye].position.z = views[eye].pose.position.z;
	}

	// Per-eye pose in head-local axes; canted displays yaw each eye by the cant angle, Quest reports identity
	for (uint32_t eye = 0; eye < 2; eye++)
	{
		vr.eyeLocalOffset[eye].x = vr.eyeLocalOffset[eye].y = vr.eyeLocalOffset[eye].z = 0.0f;
		vr.eyeLocalRotation[eye].x = vr.eyeLocalRotation[eye].y = vr.eyeLocalRotation[eye].z = 0.0f;
		vr.eyeLocalRotation[eye].w = 1.0f;
		vr.eyeCantYaw[eye] = 0.0f;
	}
	if (viewCount == 2)
	{
		XrQuaternionf centerInv;
		XrQuaternionf_Invert(&centerInv, &hmdRotation);
		for (uint32_t eye = 0; eye < 2; eye++)
		{
			const XrVector3f worldDiff = {
				views[eye].pose.position.x - hmdPosition.x,
				views[eye].pose.position.y - hmdPosition.y,
				views[eye].pose.position.z - hmdPosition.z };
			XrVector3f local;
			XrQuaternionf_RotateVector3f(&local, &centerInv, &worldDiff);
			vr.eyeLocalOffset[eye].x = local.x;
			vr.eyeLocalOffset[eye].y = local.y;
			vr.eyeLocalOffset[eye].z = local.z;

			// rel = centerInv * eye (apply eye first, then centerInv): eye-local -> center-local
			XrQuaternionf rel;
			XrQuaternionf_Multiply(&rel, &views[eye].pose.orientation, &centerInv);
			XrQuaternionf_Normalize(&rel);
			vr.eyeLocalRotation[eye].x = rel.x;
			vr.eyeLocalRotation[eye].y = rel.y;
			vr.eyeLocalRotation[eye].z = rel.z;
			vr.eyeLocalRotation[eye].w = rel.w;

			// Yaw about head-local +Y: where the eye's forward (0,0,-1) ends up
			const XrVector3f localForward = { 0.0f, 0.0f, -1.0f };
			XrVector3f f;
			XrQuaternionf_RotateVector3f(&f, &rel, &localForward);
			vr.eyeCantYaw[eye] = atan2f(-f.x, -f.z);
		}

		static qboolean eyeGeometryLogged = qfalse;
		if (!eyeGeometryLogged)
		{
			eyeGeometryLogged = qtrue;
			for (uint32_t eye = 0; eye < 2; eye++)
			{
				Com_Printf("VR eye %u: offset (%.4f, %.4f, %.4f) m  cant yaw %.2f deg  fov L %.1f R %.1f U %.1f D %.1f deg  quat (%.4f, %.4f, %.4f, %.4f)\n",
					eye,
					vr.eyeLocalOffset[eye].x, vr.eyeLocalOffset[eye].y, vr.eyeLocalOffset[eye].z,
					vr.eyeCantYaw[eye] * 180.0f / M_PI,
					views[eye].fov.angleLeft * 180.0f / M_PI, views[eye].fov.angleRight * 180.0f / M_PI,
					views[eye].fov.angleUp * 180.0f / M_PI, views[eye].fov.angleDown * 180.0f / M_PI,
					views[eye].pose.orientation.x, views[eye].pose.orientation.y,
					views[eye].pose.orientation.z, views[eye].pose.orientation.w);
			}
		}
	}

	// Debug logging removed - consolidated in vr_renderer.c

	//Position delta
	VectorSubtract(hmdposition_last, vr.hmdposition, vr.hmdposition_delta);

	//Orientation
	VectorSubtract(hmdorientation_last, vr.hmdorientation, vr.hmdorientation_delta);

	// View yaw delta
	const float clientview_yaw = vr.clientviewangles[YAW] - vr.hmdorientation[YAW];
	vr.clientview_yaw_delta = vr.clientview_yaw_last - clientview_yaw;
	vr.clientview_yaw_last = clientview_yaw;
}
