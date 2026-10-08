#ifndef __VR_CLIENTINFO
#define __VR_CLIENTINFO

#include "../qcommon/q_shared.h"
#include "vr_safe_types.h"

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

typedef struct {
	float fov_x;
	float fov_y;
	float fov_angle_up;    // radians, positive = up
	float fov_angle_down;  // radians, negative = down
	float fov_angle_left;  // radians, negative = left
	float fov_angle_right; // radians, positive = right

	// Radians; separate per eye because the stereo frusta are asymmetric
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
	qboolean recenter_follow_camera;
	float snapTurnYaw; // set by cgame

	int clientNum;
	vec3_t clientviewangles; //read by cgame
	float clientview_yaw_last; // previous frame's yaw; delta source only
	float clientview_yaw_delta;

	vec3_t hmdposition;
	vec3_t hmdposition_eye[2]; //per-eye positions from OpenXR (in HMD/VR space, not Quake space)
	vrPosef_t eyePose[2]; // Raw OpenXR per-eye poses for direct view matrix construction
	// Per-eye pose relative to the center pose, in OpenXR head-local axes (x=right, y=up, z=back)
	vrVector3f_t eyeLocalOffset[2]; // meters
	vrQuaternionf_t eyeLocalRotation[2]; // identity on Quest, +/- the cant angle on canted displays
	float eyeCantYaw[2]; // signed yaw of eyeLocalRotation in radians; centers the foveation map
	vec3_t hmdorigin; //used to recenter the mp 6DoF playspace
	vec3_t hmdposition_delta;

	vec3_t hmdorientation;
	vec3_t hmdorientation_delta;

	vec3_t weaponangles;
	vec3_t calculated_weaponangles; //Angle from the view origin to the point the controller is pointing at
	vec3_t weaponangles_last; // Don't use this, it is just for calculating delta!
	vec3_t weaponangles_delta;

	vec3_t weaponoffset;
	vec3_t weaponoffset_last[2];
	vec3_t weaponposition;

	vec3_t offhandangles;

	// Runtime aim pose without vr_weaponPitch, so tuning the weapon leaves the menu cursor put
	vec3_t weaponaimangles;
	vec3_t offhandaimangles;
	vec3_t offhandangles2;
	vec3_t offhandoffset;
	vec3_t offhandoffset_last[2];
	vec3_t offhandposition;

	vec2_t thumbstick_location[2]; // by role: VR_STICK_MOVE, VR_STICK_TURN

	qboolean walking;	// analog walk/run: true => assert BUTTON_WALKING (silent walk, no footsteps)

	float menuYaw;
	qboolean menuYawLocked;	// prevent renderer from overwriting menuYaw (used during timeline scrub)
	int menuCursorX;                // engine-computed menu cursor (640x480 virtual)
	int menuCursorY;
	int scoreboardCursorX;          // engine-computed scoreboard/vote cursor
	int scoreboardCursorY;
	qboolean menuCursorActive;      // module wants engine menu-cursor tracking
	qboolean scoreboardCursorActive;// module wants engine scoreboard-cursor tracking
	int pointerMode;                // VR_POINTER_*: who presents the menu selection (synced to modules)
	qboolean menuLeftHanded;
	int offhandCursorX;             // 640x480 virtual coords
	int offhandCursorY;
	qboolean vkbOffhandTriggerDown; // only meaningful while the keyboard is up

	float recenterYaw;

	// SP intermission HUD anchoring (world-fixed UI during podium view)
	qboolean sp_intermission_active;
	float sp_intermission_yaw;  // The yaw angle the HUD is anchored to (degrees)

	float sp_intermission_hud_origin[3];    // absolute, not relative to the view
	float sp_intermission_hud_radius;
} vr_clientinfo_t;

#endif
