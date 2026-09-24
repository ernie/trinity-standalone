#include "vr_cvars.h"

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"
#include "vr_base.h"


cvar_t *vr_worldscale = NULL;
cvar_t *vr_worldscaleScaler = NULL;
cvar_t *vr_hudDepth = NULL;
cvar_t *vr_currentHudDepth = NULL;
cvar_t *vr_righthanded = NULL;
cvar_t *vr_switchThumbsticks = NULL;
cvar_t *vr_snapturn = NULL;
cvar_t *vr_sensitivity = NULL;
cvar_t *vr_heightAdjust = NULL;
cvar_t *vr_directionMode = NULL;
cvar_t *vr_weaponPitch = NULL;
cvar_t *vr_twoHandedWeapons = NULL;
cvar_t *vr_showItemInHand = NULL;
cvar_t *vr_refreshrate = NULL;
cvar_t *vr_refreshrates = NULL;
cvar_t *vr_foveation = NULL;
cvar_t *vr_foveationStrength = NULL;
cvar_t *vr_foveationCaps = NULL;
cvar_t *vr_superSampling = NULL;
cvar_t *vr_weaponScope = NULL;
cvar_t *vr_6dof = NULL;
cvar_t *vr_rollWhenHit = NULL;
cvar_t *vr_hudYOffset = NULL;
cvar_t *vr_hudScale = NULL;
cvar_t *vr_sendRollToServer = NULL;
cvar_t *vr_lasersight = NULL;
cvar_t *vr_hapticIntensity = NULL;
cvar_t *vr_bhaptics = NULL;
cvar_t *vr_comfortVignette = NULL;
cvar_t *vr_weaponSelectorMode = NULL;
cvar_t *vr_weaponSelectorWithHud = NULL;
cvar_t *vr_goreLevel = NULL;
cvar_t *vr_hudDrawStatus = NULL;
cvar_t *vr_currentHudDrawStatus = NULL;
cvar_t *vr_showConsoleMessages = NULL;
cvar_t *vr_desktopMode = NULL;
cvar_t *vr_virtualScreenMode = NULL;
cvar_t *vr_screenCurvature = NULL;
cvar_t *vr_thumbstickDeadzone = NULL;
cvar_t *vr_thumbstickFullDeflection = NULL;
cvar_t *vr_weaponAdjust = NULL;
cvar_t *vr_triggerSensitivity = NULL;
cvar_t *vr_analogWalk = NULL;
cvar_t *vr_frameTimingLog = NULL;

void VR_InitCvars( void )
{
	Cvar_Get ("skip_ioq3_credits", "0.0", CVAR_ARCHIVE);
	Cvar_Get ("vr_platform", "standalone", CVAR_ROM);	// advertise the VR platform to UI modules
	vr_worldscale = Cvar_Get ("vr_worldscale", "32.0", CVAR_ARCHIVE);
	vr_worldscaleScaler = Cvar_Get ("vr_worldscaleScaler", "1.0", CVAR_ARCHIVE);
	vr_hudDepth = Cvar_Get ("vr_hudDepth", "3", CVAR_ARCHIVE);
	Cvar_CheckRange( vr_hudDepth, 0, 5, qtrue );
	vr_righthanded = Cvar_Get ("vr_righthanded", "1", CVAR_ARCHIVE);
	vr_switchThumbsticks = Cvar_Get ("vr_switchThumbsticks", "0", CVAR_ARCHIVE);
	vr_snapturn = Cvar_Get ("vr_snapturn", "45", CVAR_ARCHIVE);
	vr_sensitivity = Cvar_Get ("vr_sensitivity", "100", CVAR_ARCHIVE);
	vr_directionMode = Cvar_Get ("vr_directionMode", "1", CVAR_ARCHIVE); // 0 = HMD, 1 = Off-hand
	// Degrees on top of the fixed VR_GRIP_TO_AIM_PITCH correction; zero is no personal adjustment
	vr_weaponPitch = Cvar_Get ("vr_weaponPitch", "0", CVAR_ARCHIVE);
	vr_heightAdjust = Cvar_Get ("vr_heightAdjust", "0.0", CVAR_ARCHIVE);
	vr_twoHandedWeapons = Cvar_Get ("vr_twoHandedWeapons", "0", CVAR_ARCHIVE);
	vr_showItemInHand = Cvar_Get ("vr_showItemInHand", "1", CVAR_ARCHIVE);
	vr_refreshrate = Cvar_Get ("vr_refreshrate", "90", CVAR_ARCHIVE);
	vr_refreshrates = Cvar_Get ("vr_refreshrates", "", CVAR_ROM);	// space-separated rates the runtime supports, for the UI
	// 0 off, 1 fixed, 2 eye tracked; applied live. Eye tracked by default: a headset without it drops to fixed and writes back.
	vr_foveation = Cvar_Get ("vr_foveation", "2", CVAR_ARCHIVE);
	Cvar_CheckRange( vr_foveation, 0, 2, qtrue );
	// 1 low, 2 medium, 3 high: applies to fixed and eye tracked alike
	vr_foveationStrength = Cvar_Get ("vr_foveationStrength", "2", CVAR_ARCHIVE);
	Cvar_CheckRange( vr_foveationStrength, 1, 3, qtrue );
	// none / fixed / eyetracked: what the runtime can do, decided at instance creation, for the UI
	vr_foveationCaps = Cvar_Get ("vr_foveationCaps", "none", CVAR_ROM);
	Cvar_Set2( "vr_foveationCaps", VR_FoveationCapsString(), qtrue );
	vr_superSampling = Cvar_Get ("vr_superSampling", "1.0", CVAR_ARCHIVE);
	vr_weaponScope = Cvar_Get ("vr_weaponScope", "1", CVAR_ARCHIVE);
	vr_6dof = Cvar_Get ("vr_6dof", "1", CVAR_ARCHIVE); // 0 - fake 6DoF in SP, 1 - true 6DoF in SP (requires enhanced physics coefficients)
	vr_rollWhenHit = Cvar_Get ("vr_rollWhenHit", "0", CVAR_ARCHIVE);
	vr_hudYOffset = Cvar_Get ("vr_hudYOffset", "0", CVAR_ARCHIVE);
	vr_hudScale = Cvar_Get ("vr_hudScale", "1.0", CVAR_ARCHIVE);
	vr_sendRollToServer = Cvar_Get ("vr_sendRollToServer", "1", CVAR_ARCHIVE);
	vr_lasersight = Cvar_Get ("vr_lasersight", "0", CVAR_ARCHIVE);
	vr_hapticIntensity = Cvar_Get ("vr_hapticIntensity", "0.5", CVAR_ARCHIVE);
	vr_bhaptics = Cvar_Get ("vr_bhaptics", "0", CVAR_ARCHIVE);
	vr_comfortVignette = Cvar_Get ("vr_comfortVignette", "0.0", CVAR_ARCHIVE);
	vr_weaponSelectorMode = Cvar_Get ("vr_weaponSelectorMode", "0", CVAR_ARCHIVE);
	vr_weaponSelectorWithHud = Cvar_Get ("vr_weaponSelectorWithHud", "0", CVAR_ARCHIVE);
	vr_goreLevel = Cvar_Get ("vr_goreLevel", "2", CVAR_ARCHIVE);
	vr_hudDrawStatus = Cvar_Get ("vr_hudDrawStatus", "1", CVAR_ARCHIVE); // 0 - no hud, 1 - in-world hud, 2 - performance (static HUD)
	vr_currentHudDrawStatus = Cvar_Get ("vr_currentHudDrawStatus", "1", 0); // 0 - no hud, 1 - in-world hud, 2 - performance (static HUD)
	vr_currentHudDepth = Cvar_Get ("vr_currentHudDepth", "3", 0 );  // Runtime copy, not archived
	vr_showConsoleMessages = Cvar_Get ("vr_showConsoleMessages", "1", CVAR_ARCHIVE);
	vr_desktopMode = Cvar_Get ("vr_desktopMode", "0", CVAR_ARCHIVE); // 0 - left eye, 1 - right eye, 2 - both eyes
	vr_virtualScreenMode = Cvar_Get ("vr_virtualScreenMode", "0", CVAR_ARCHIVE); // 0 - fixed, 1 - follow
	vr_screenCurvature = Cvar_Get ("vr_screenCurvature", "0.5", CVAR_ARCHIVE); // cylinder layer curvature (0.0 = flat, 1.0 = max curve)
	vr_thumbstickDeadzone = Cvar_Get ("vr_thumbstickDeadzone", "0.15", CVAR_ARCHIVE);
	vr_thumbstickFullDeflection = Cvar_Get ("vr_thumbstickFullDeflection", "0.85", CVAR_ARCHIVE);
	vr_weaponAdjust = Cvar_Get ("vr_weaponAdjust", "0", CVAR_ARCHIVE);
	vr_triggerSensitivity = Cvar_Get ("vr_triggerSensitivity", "0.25", CVAR_ARCHIVE);
	Cvar_CheckRange( vr_triggerSensitivity, 0.1f, 0.9f, qfalse );
	vr_analogWalk = Cvar_Get ("vr_analogWalk", "1", CVAR_ARCHIVE); // 0 - classic always-run, 1 - silent walk below run speed
	vr_frameTimingLog = Cvar_Get ("vr_frameTimingLog", "0", 0); // diagnostic: log XR frame pacing (shouldRender/predictedDisplayTime) to console

	// Values are:  scale,right,up,forward,pitch,yaw,roll
	// VALUES PROVIDED BY SkillFur - Thank-you!
	Cvar_Get ("vr_weapon_adjustment_1", "1,-4.0,7,-10,-20,-15,0", CVAR_ARCHIVE);
	Cvar_Get ("vr_weapon_adjustment_2", "0.8,-3.0,5.5,0,0,0,0", CVAR_ARCHIVE);
	Cvar_Get ("vr_weapon_adjustment_3", "0.8,-3.3,8,3.7,0,0,0", CVAR_ARCHIVE); // shotgun
	Cvar_Get ("vr_weapon_adjustment_4", "0.75,-5.4,6.5,-4,0,0,0", CVAR_ARCHIVE);
	Cvar_Get ("vr_weapon_adjustment_5", "0.8,-5.2,6,7.5,0,0,0", CVAR_ARCHIVE);
	Cvar_Get ("vr_weapon_adjustment_6", "0.8,-3.3,6,7,0,0,0", CVAR_ARCHIVE);
	Cvar_Get ("vr_weapon_adjustment_7", "0.8,-5.5,6,0,0,0,0", CVAR_ARCHIVE);
	Cvar_Get ("vr_weapon_adjustment_8", "0.8,-4.5,6,1.5,0,0,0", CVAR_ARCHIVE);
	Cvar_Get ("vr_weapon_adjustment_9", "0.8,-5.5,6,0,0,0,0", CVAR_ARCHIVE);
	Cvar_Get ("vr_weapon_adjustment_10", "0.8,-2.75,6,-1.25,0,0,0", CVAR_ARCHIVE);

	//Team Arena Weapons
	Cvar_Get ("vr_weapon_adjustment_11", "0.8,-5.5,6,0,0,0,0", CVAR_ARCHIVE);
	Cvar_Get ("vr_weapon_adjustment_12", "0.8,-5.5,6,0,0,0,0", CVAR_ARCHIVE);
	Cvar_Get ("vr_weapon_adjustment_13", "0.8,-5.5,6,0,0,0,0", CVAR_ARCHIVE);

	// Control schemas
	Cvar_Get ("vr_uturn", "0", CVAR_ARCHIVE);
	Cvar_Get ("vr_controlSchema", "1", CVAR_ARCHIVE);
	qboolean uturnEnabled = Cvar_VariableValue( "vr_uturn" ) != 0;
	int controlSchema = (int)Cvar_VariableValue( "vr_controlSchema" ) % 3;
	if (controlSchema == 0)
	{
		// Default schema (weapon wheel on grip)
		Cvar_Get ("vr_button_map_RTHUMBLEFT", "turnleft", CVAR_ARCHIVE); // turn left
		Cvar_Get ("vr_button_map_RTHUMBRIGHT", "turnright", CVAR_ARCHIVE); // turn right
		Cvar_Get ("vr_button_map_RTHUMBFORWARD", "", CVAR_ARCHIVE); // unmapped
		if (uturnEnabled)
		{
			Cvar_Get ("vr_button_map_RTHUMBBACK", "uturn", CVAR_ARCHIVE); // u-turn
		}
		else
		{
			Cvar_Get ("vr_button_map_RTHUMBBACK", "", CVAR_ARCHIVE); // unmapped
		}
		Cvar_Get ("vr_button_map_PRIMARYGRIP", "+weapon_select", CVAR_ARCHIVE); // weapon selector
		Cvar_Get ("vr_button_map_PRIMARYTHUMBSTICK", "", CVAR_ARCHIVE); // unmapped
		Cvar_Get ("vr_button_map_RTHUMBFORWARD_ALT", "", CVAR_ARCHIVE); // unmapped
		Cvar_Get ("vr_button_map_RTHUMBRIGHT_ALT", "", CVAR_ARCHIVE); // unmapped
		Cvar_Get ("vr_button_map_RTHUMBBACK_ALT", "", CVAR_ARCHIVE); // unmapped
		Cvar_Get ("vr_button_map_RTHUMBLEFT_ALT", "", CVAR_ARCHIVE); // unmapped
	}
	else if (controlSchema == 1)
	{
		// Weapon wheel on thumbstick - all directions as weapon select (useful for HMD wheel)
		Cvar_Get ("vr_button_map_RTHUMBFORWARD", "+weapon_select", CVAR_ARCHIVE);
		Cvar_Get ("vr_button_map_RTHUMBRIGHT", "+weapon_select", CVAR_ARCHIVE);
		Cvar_Get ("vr_button_map_RTHUMBBACK", "+weapon_select", CVAR_ARCHIVE);
		Cvar_Get ("vr_button_map_RTHUMBLEFT", "+weapon_select", CVAR_ARCHIVE);
		Cvar_Get ("vr_button_map_PRIMARYTHUMBSTICK", "+weapon_select", CVAR_ARCHIVE);
		Cvar_Get ("vr_button_map_PRIMARYGRIP", "+alt", CVAR_ARCHIVE); // switch to alt layout
		Cvar_Get ("vr_button_map_RTHUMBLEFT_ALT", "turnleft", CVAR_ARCHIVE); // turn left
		Cvar_Get ("vr_button_map_RTHUMBRIGHT_ALT", "turnright", CVAR_ARCHIVE); // turn right
		Cvar_Get ("vr_button_map_RTHUMBFORWARD_ALT", "blank", CVAR_ARCHIVE);
		if (uturnEnabled)
		{
			Cvar_Get ("vr_button_map_RTHUMBBACK_ALT", "uturn", CVAR_ARCHIVE);
		}
		else
		{
			Cvar_Get ("vr_button_map_RTHUMBBACK_ALT", "blank", CVAR_ARCHIVE);
		}
	}
	else
	{
		// Weapon wheel disabled - only prev/next weapon switch is active
		Cvar_Get ("vr_button_map_RTHUMBLEFT", "turnleft", CVAR_ARCHIVE); // turn left
		Cvar_Get ("vr_button_map_RTHUMBRIGHT", "turnright", CVAR_ARCHIVE); // turn right
		Cvar_Get ("vr_button_map_RTHUMBFORWARD", "weapnext", CVAR_ARCHIVE); // next weapon
		if (uturnEnabled)
		{
			Cvar_Get ("vr_button_map_RTHUMBBACK", "uturn", CVAR_ARCHIVE); // u-turn
		}
		else
		{
			Cvar_Get ("vr_button_map_RTHUMBBACK", "weapprev", CVAR_ARCHIVE); // previous weapon
		}
		Cvar_Get ("vr_button_map_PRIMARYGRIP", "+alt", CVAR_ARCHIVE); // switch to alt layout
		Cvar_Get ("vr_button_map_PRIMARYTHUMBSTICK", "", CVAR_ARCHIVE); // unmapped
		Cvar_Get ("vr_button_map_RTHUMBFORWARD_ALT", "", CVAR_ARCHIVE); // unmapped
		Cvar_Get ("vr_button_map_RTHUMBRIGHT_ALT", "", CVAR_ARCHIVE); // unmapped
		Cvar_Get ("vr_button_map_RTHUMBBACK_ALT", "", CVAR_ARCHIVE); // unmapped
		Cvar_Get ("vr_button_map_RTHUMBLEFT_ALT", "", CVAR_ARCHIVE); // unmapped
	}

	// Map face buttons based on thumbstick assigned to movement
	// (cannot move and jump/crouch with same thumb at same time)
	qboolean switchThumbsticks = Cvar_VariableValue( "vr_switchThumbsticks" ) != 0;
	if (switchThumbsticks)
	{
		Cvar_Get ("vr_button_map_A", "+button2", CVAR_ARCHIVE); // Use Item
		Cvar_Get ("vr_button_map_B", "togglemenu", CVAR_ARCHIVE); // Menu
		Cvar_Get ("vr_button_map_X", "+moveup", CVAR_ARCHIVE); // Jump
		Cvar_Get ("vr_button_map_Y", "+movedown", CVAR_ARCHIVE); // Crouch
	}
	else
	{
		Cvar_Get ("vr_button_map_A", "+moveup", CVAR_ARCHIVE); // Jump
		Cvar_Get ("vr_button_map_B", "+movedown", CVAR_ARCHIVE); // Crouch
		Cvar_Get ("vr_button_map_X", "+button2", CVAR_ARCHIVE); // Use Item
		Cvar_Get ("vr_button_map_Y", "togglemenu", CVAR_ARCHIVE); // Menu
	}

	//Remaining button mapping (buttons not affected by schemas)
	Cvar_Get ("vr_button_map_A_ALT", "", CVAR_ARCHIVE); // unmapped
	Cvar_Get ("vr_button_map_B_ALT", "", CVAR_ARCHIVE); // unmapped
	Cvar_Get ("vr_button_map_X_ALT", "", CVAR_ARCHIVE); // unmapped
	Cvar_Get ("vr_button_map_Y_ALT", "", CVAR_ARCHIVE); // unmapped
	Cvar_Get ("vr_button_map_SECONDARYTHUMBSTICK", "+scores", CVAR_ARCHIVE); // Scoreboard
	Cvar_Get ("vr_button_map_SECONDARYTHUMBSTICK_ALT", "", CVAR_ARCHIVE); // unmapped
	Cvar_Get ("vr_button_map_PRIMARYTHUMBSTICK_ALT", "", CVAR_ARCHIVE); // unmapped
	Cvar_Get ("vr_button_map_SECONDARYTRIGGER", "+moveup", CVAR_ARCHIVE); // Also Jump
	Cvar_Get ("vr_button_map_SECONDARYTRIGGER_ALT", "", CVAR_ARCHIVE); // unmapped
	Cvar_Get ("vr_button_map_PRIMARYTRIGGER", "+attack", CVAR_ARCHIVE); // Fire
	Cvar_Get ("vr_button_map_PRIMARYTRIGGER_ALT", "", CVAR_ARCHIVE); // unmapped
	Cvar_Get ("vr_button_map_SECONDARYGRIP", "+weapon_stabilise", CVAR_ARCHIVE); // Weapon stabilisation
	Cvar_Get ("vr_button_map_SECONDARYGRIP_ALT", "", CVAR_ARCHIVE); // unmapped
	Cvar_Get ("vr_button_map_PRIMARYGRIP_ALT", "", CVAR_ARCHIVE); // unmapped

	Cvar_Get ("vr_button_map_PRIMARYTRACKPAD", "+scores", CVAR_ARCHIVE); // unmapped
	Cvar_Get ("vr_button_map_PRIMARYTRACKPAD_ALT", "", CVAR_ARCHIVE); // unmapped
	Cvar_Get ("vr_button_map_SECONDARYTRACKPAD", "togglemenu", CVAR_ARCHIVE); // unmapped
	Cvar_Get ("vr_button_map_SECONDARYTRACKPAD_ALT", "", CVAR_ARCHIVE); // unmapped
	Cvar_Get ("vr_button_map_PRIMARYTHUMBREST", "+alt", CVAR_ARCHIVE); // Alt modifier
	Cvar_Get ("vr_button_map_PRIMARYTHUMBREST_ALT", "", CVAR_ARCHIVE); // unmapped
	Cvar_Get ("vr_button_map_SECONDARYTHUMBREST", "+alt", CVAR_ARCHIVE); // Alt modifier
	Cvar_Get ("vr_button_map_SECONDARYTHUMBREST_ALT", "", CVAR_ARCHIVE); // unmapped
}
