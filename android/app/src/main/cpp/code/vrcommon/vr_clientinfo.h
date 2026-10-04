#ifndef __VR_CLIENTINFO
#define __VR_CLIENTINFO

#include "../qcommon/q_shared.h"
#include "vr_safe_types.h"

#define NUM_WEAPON_SAMPLES      10

// OpenXR-compatible pose types for vr_clientinfo (avoiding OpenXR header dependency)
// These match the memory layout of XrVector3f, XrQuaternionf, and XrPosef exactly
typedef struct {
	float x;
	float y;
	float z;
} vrVector3f_t;

typedef struct {
	float x;
	float y;
	float z;
	float w;
} vrQuaternionf_t;

typedef struct {
	vrQuaternionf_t orientation;
	vrVector3f_t position;
} vrPosef_t;

// thumbstick_location[] holds the sticks by role, deadzone-processed
#define VR_STICK_MOVE 0
#define VR_STICK_TURN 1

// pointerMode
#define VR_POINTER_CURSOR 0 // the ray moves the hover; the module draws its cursor
#define VR_POINTER_STICK  1 // a thumbstick moves the selection: no hover, no cursor, no ray
#define VR_POINTER_DRAWN  2 // the ray moves the hover; the engine draws the ray and its pool of light

typedef struct {
	float fov_x;
	float fov_y;
	float fov_angle_up;    // Raw OpenXR FOV angle in radians (positive = up)
	float fov_angle_down;  // Raw OpenXR FOV angle in radians (negative = down)
	float fov_angle_left;  // Raw OpenXR FOV angle in radians (negative = left)
	float fov_angle_right; // Raw OpenXR FOV angle in radians (positive = right)

	// Per-eye FOV angles in radians (for asymmetric stereo rendering)
	float eye_fov_angle_left[2];   // [0]=left eye, [1]=right eye
	float eye_fov_angle_right[2];  // [0]=left eye, [1]=right eye

	qboolean weapon_stabilised;
	qboolean weapon_zoomed;
	float weapon_zoomLevel;
	qboolean right_handed;
	qboolean virtual_screen;
	qboolean first_person_following;
	qboolean in_menu;
	qboolean local_server;
	qboolean single_player;
	qboolean use_6dof;
	VR_FollowMode follow_mode;
	qboolean weapon_select;
	qboolean weapon_select_autoclose;
	qboolean weapon_select_using_thumbstick;
	qboolean weapon_adjust;
	qboolean no_crosshair;
	qboolean vote_active;           // true when any yes/no dialog is visible (vote, team vote, TVD offer)
	int vote_holding;               // 0=none, 1=A held (yes), -1=B held (no): set by engine, read by cgame

	int realign; // used to realign the 6DoF playspace in a multiplayer game
	qboolean recenter_follow_camera; // flag to trigger camera recentering in follow mode
	float snapTurnYaw; // yaw rotation to apply (like CL_SnapTurn), set by cgame

	int clientNum;
	vec3_t clientviewangles; //orientation in the client - we use this in the cgame
	float clientview_yaw_last; // Don't use this, it is just for calculating delta!
	float clientview_yaw_delta;

	vec3_t hmdposition;
	vec3_t hmdposition_eye[2]; //per-eye positions from OpenXR (in HMD/VR space, not Quake space)
	vrPosef_t eyePose[2]; // Raw OpenXR per-eye poses for direct view matrix construction
	// Per-eye pose relative to the center pose, in OpenXR head-local axes (x=right, y=up, z=back)
	vrVector3f_t eyeLocalOffset[2]; // meters
	vrQuaternionf_t eyeLocalRotation[2]; // identity on Quest, +/- the cant angle on canted displays
	float eyeCantYaw[2]; // signed yaw of eyeLocalRotation in radians; widens the culling frustum
	vec3_t hmdorigin; //used to recenter the mp 6DoF playspace
	vec3_t hmdposition_delta;

	vec3_t hmdorientation;
	vec3_t hmdorientation_delta;

	vec3_t weaponangles;
	vec3_t calculated_weaponangles; //Calculated as the angle required to hit the point that the controller is pointing at, but coming from the view origin
	vec3_t weaponangles_last; // Don't use this, it is just for calculating delta!
	vec3_t weaponangles_delta;

	vec3_t weaponoffset;
	vec3_t weaponoffset_last[2];
	vec3_t weaponposition;

	vec3_t offhandangles;

	// From the runtime's aim pose, free of vr_weaponPitch, so the menu cursor does not move when the weapon is tuned
	vec3_t weaponaimangles;
	vec3_t offhandaimangles;
	vec3_t offhandangles2;
	vec3_t offhandoffset;
	vec3_t offhandoffset_last[2];
	vec3_t offhandposition;

	vec2_t thumbstick_location[2]; //left / right thumbstick locations - used in cgame

	qboolean walking;	// analog walk/run: true => assert BUTTON_WALKING (silent walk, no footsteps)

	float menuYaw;
	qboolean menuYawLocked;
	int menuCursorX;                // engine-computed menu cursor (640x480 virtual)
	int menuCursorY;
	int scoreboardCursorX;          // engine-computed scoreboard/vote cursor
	int scoreboardCursorY;
	qboolean menuCursorActive;      // module wants engine menu-cursor tracking
	qboolean scoreboardCursorActive;// module wants engine scoreboard-cursor tracking
	int pointerMode;                // VR_POINTER_*: who presents the menu selection (synced to modules)
	qboolean menuLeftHanded;
	int offhandCursorX;             // offhand cursor X (640x480 virtual coords)
	int offhandCursorY;             // offhand cursor Y (640x480 virtual coords)
	qboolean vkbOffhandTriggerDown; // offhand trigger held while keyboard active

	float recenterYaw;

	// SP intermission HUD anchoring (world-fixed UI during podium view)
	qboolean sp_intermission_active;
	float sp_intermission_yaw;  // The yaw angle the HUD is anchored to (degrees)

	// SP intermission HUD sprite positioning (world-locked at podium)
	float sp_intermission_hud_origin[3];    // Absolute world position for HUD sprite
	float sp_intermission_hud_radius;       // Fixed radius for HUD sprite
} vr_clientinfo_t;

#endif
