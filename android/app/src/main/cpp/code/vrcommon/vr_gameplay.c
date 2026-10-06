#include "vr_gameplay.h"

#include "../client/client.h"
#include "vr_clientinfo.h"

void VR_Gameplay_OpenMenuAndPauseIfPossible(void)
{
	if ( clc.state == CA_ACTIVE && !clc.demoplaying && !Cvar_VariableValue ("cl_paused") )
	{
		Cbuf_ExecuteText(EXEC_APPEND, "togglemenu\n");
	}
}

qboolean VR_IsFollowingInFirstPerson( void )
{
	extern vr_clientinfo_t vr;

	return (
		(((cl.snap.ps.pm_flags & PMF_FOLLOW) || clc.demoplaying)) &&
		(vr.follow_mode == VRFM_FIRSTPERSON)
	);
}

qboolean VR_IsInMenu( void )
{
	int keyCatcher = Key_GetCatcher();
	return (keyCatcher & (KEYCATCH_UI | KEYCATCH_CONSOLE)) != 0;
}

// The console counts as a menu for gating, but it answers to different keys
// than the UI does, so anything synthesising keys has to tell the two apart.
qboolean VR_IsInConsole( void )
{
	return (Key_GetCatcher() & KEYCATCH_CONSOLE) != 0;
}

qboolean VR_IsSPIntermission( void )
{
	return (cl.snap.ps.pm_type == PM_INTERMISSION) &&
	       (clc.serverGametype == GT_SINGLE_PLAYER);
}

qboolean VR_Gameplay_ShouldRenderInVirtualScreen( void )
{
	// The SP postgame UI draws on the podium HUD; the console reads on the virtual screen everywhere
	if ( VR_IsInConsole() || ( VR_IsInMenu() && !VR_IsSPIntermission() ) )
	{
		return qtrue;
	}

	// Intermission without menu is rendered in-world
	if ( cl.snap.ps.pm_type == PM_INTERMISSION )
	{
		return qfalse;
	}

	return (
		clc.state == CA_CINEMATIC ||
		clc.state != CA_ACTIVE ||
		VR_IsFollowingInFirstPerson()
	);
}

qboolean VR_ShouldDisableStereo( void )
{
	extern vr_clientinfo_t vr;

	// Disable stereo when rendering to virtual screen
	if (VR_Gameplay_ShouldRenderInVirtualScreen())
	{
		return qtrue;
	}

	// Disable stereo when weapon is zoomed (scope view)
	// This renders from center viewpoint so mono quad layer blit is correct
	if (vr.weapon_zoomed)
	{
		return qtrue;
	}

	return qfalse;
}

/*
==================
VR_ScopeFrustum

Half-tangents of the cyclopean scope view at zoom 1. The quad is the same width on
every headset, so the scope circle and the minimal HUD keep their angles and wider
displays get black beside it. The height follows the buffer aspect for square pixels.
==================
*/
void VR_ScopeFrustum( float *halfTanH, float *halfTanV, int width, int height )
{
	*halfTanH = VR_SCOPE_HALF_TAN_H;
	*halfTanV = VR_SCOPE_HALF_TAN_H * (float)height / (float)width;
}

/*
==================
VR_Gameplay_VirtualScreenContextChanged

Detects client connection-state transitions. Callers use it while the
virtual screen is up to re-anchor when the screen's context changes
mid-window (e.g. a map load beginning), so the loading screen appears where
the player is facing instead of at the stale menu anchor.

Single consumer per frame: the edge is consumed on read, so a second
same-frame caller would starve the first.
==================
*/
qboolean VR_Gameplay_VirtualScreenContextChanged( void )
{
	static connstate_t lastState = CA_UNINITIALIZED;
	qboolean changed = ( clc.state != lastState );
	lastState = clc.state;
	return changed;
}
