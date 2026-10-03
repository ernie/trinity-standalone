#include "vr_router.h"

#include "../client/client.h"
#include "../client/cl_vr_bind.h"
#include "vr_clientinfo.h"
#include "vr_haptics.h"
#include "vr_input.h"
#include "vr_virtual_screen.h"

extern vr_clientinfo_t vr;
extern cvar_t *vr_righthanded;
extern cvar_t *vr_switchThumbsticks;
extern cvar_t *vr_snapturn;
extern cvar_t *vr_triggerSensitivity;
extern cvar_t *vr_thumbstickDeadzone;
extern cvar_t *vr_weaponSelectorMode;
extern cvar_t *vr_gripThreshold;
extern cvar_t *vr_trackpadThreshold;
extern cvar_t *vr_directionMode;
extern cvar_t *vr_thumbstickFullDeflection;
extern cvar_t *vr_analogWalk;
extern cvar_t *vr_sendRollToServer;
extern cvar_t *vr_sensitivity;
extern cvar_t *vr_controllerModels;

/* The latest controller sample and when it arrived; the command builder trusts it for 250 ms. */
static struct {
	clXRHandInput_t hands[2];
	qboolean valid;
	int time, previousTime;
} input;

/* Controller keys: the hold machine owns every press until its release, whatever the context does meanwhile. */
static vrHolds_t holds;
static qboolean holdsReady;
static vrStack_t stack;
static unsigned char keysNow[VRK_COUNT];
static int weaponSelectHeld, stabiliseHeld;
static int clickKey[VRK_COUNT], navKey[VRK_COUNT], navAnchorX, navAnchorY;

static void VR_Router_HoldsReady( void ) {
	if ( !holdsReady ) {
		VR_HoldsInit( &holds );
		holdsReady = qtrue;
	}
}

/* Button commands set their input state now; the rest run from the command buffer. */
static qboolean VR_Router_Immediate( const char *word ) {
	static const char *words[] = {"+attack", "+forward", "+back", "+moveleft", "+moveright", "+moveup", "+movedown",
								  "+left", "+right", "+lookup", "+lookdown", "+strafe", "+speed", "+mlook", "+key",
								  "+vr_click", "+menunav", "+weapon_select", "+weapon_stabilise", "+alt", "+vote_yes",
								  "+vote_no", "turnleft", "turnright", "uturn", "vr_recenter"};
	unsigned i;
	char plus[64];
	Com_sprintf( plus, sizeof( plus ), "+%s", word[0] == '-' || word[0] == '+' ? word + 1 : word );
	if ( !Q_strncmp( plus, "+button", 7 ) )
		return qtrue;
	for ( i = 0; i < ARRAY_LEN( words ); i++ )
		if ( !Q_stricmp( words[i][0] == '+' ? plus : word, words[i] ) )
			return qtrue;
	return qfalse;
}

static void VR_Router_Run( const vrBindEvent_t *e, qboolean resetting ) {
	char buf[VR_BINDING_MAX], cmd[MAX_STRING_CHARS], word[64], *p, *end;
	int i;
	Q_strncpyz( buf, e->binding, sizeof( buf ) );
	for ( p = buf; p; p = end ) {
		end = strchr( p, ';' );
		if ( end )
			*end++ = '\0';
		while ( *p == ' ' )
			p++;
		if ( !*p )
			continue;
		if ( *p == '+' ) {
			/* Losing input mid-scrub cancels it instead of seeking to wherever the pointer was. */
			if ( resetting && !Q_stricmpn( p, "+tv_scrub", 9 ) && (p[9] == '\0' || p[9] == ' ') )
				Q_strncpyz( cmd, "tv_scrub_cancel", sizeof( cmd ) );
			else
				Com_sprintf( cmd, sizeof( cmd ), "%c%s %d %d", e->type == VRE_PRESS ? '+' : '-', p + 1,
							 K_VR_WPN_TRIGGER + e->key, com_frameTime );
		} else if ( e->type == VRE_PRESS )
			Q_strncpyz( cmd, p, sizeof( cmd ) );
		else
			continue;
		for ( i = 0; cmd[i] && cmd[i] != ' ' && i < (int)sizeof( word ) - 1; i++ )
			word[i] = cmd[i];
		word[i] = '\0';
		if ( VR_Router_Immediate( word ) )
			Cmd_ExecuteString( cmd );
		else
			Cbuf_AddText( va( "%s\n", cmd ) );
	}
}

static void VR_Router_RunEvents( const vrBindEvent_t *events, int count, qboolean resetting ) {
	int i;
	for ( i = 0; i < count; i++ )
		VR_Router_Run( &events[i], resetting );
}

/* The VR key that ran this command, from the key number the dispatcher appends; -1 when typed at the console. */
static int VR_Router_SourceKey( void ) {
	int key = Cmd_Argc() >= 3 ? atoi( Cmd_Argv( Cmd_Argc() - 2 ) ) - K_VR_WPN_TRIGGER : -1;
	return key >= 0 && key < VRK_COUNT ? key : -1;
}

static void VR_Router_KeyDown_f( void ) {
	int target = Key_StringToKeynum( Cmd_Argv( 1 ) );
	if ( target > 0 )
		CL_KeyEvent( target, qtrue, com_frameTime );
}
static void VR_Router_KeyUp_f( void ) {
	int target = Key_StringToKeynum( Cmd_Argv( 1 ) );
	if ( target > 0 )
		CL_KeyEvent( target, qfalse, com_frameTime );
}

/* Whether each hand's drawn ray met the virtual screen, as of the last frame it was drawn. */
static qboolean pointerOnScreen[2];

static void VR_Router_Click( qboolean down ) {
	const int key = VR_Router_SourceKey(), slot = key >= 0 ? key : 0;
	const int menuHand = vr.menuLeftHanded ? 0 : 1;
	const int hand = key >= 0 ? VR_KeyHand( (vrKey_t)key, vr.right_handed, vr_switchThumbsticks->integer ) : menuHand;
	if ( !down ) {
		if ( clickKey[slot] < 0 ) {
			VKeyboard_HandleOffhandKey( qfalse );
			vr.vkbOffhandTriggerDown = qfalse;
		} else if ( clickKey[slot] )
			CL_KeyEvent( clickKey[slot], qfalse, com_frameTime );
		clickKey[slot] = 0;
		return;
	}
	if ( VKeyboard_IsActive() && hand != menuHand ) {
		if ( vr.pointerMode == VR_POINTER_DRAWN && !pointerOnScreen[hand] )
			return;
		vr.vkbOffhandTriggerDown = qtrue;
		VKeyboard_HandleOffhandKey( qtrue );
		VR_Vibrate( 200, hand + 1, 0.8f );
		clickKey[slot] = -1;
		return;
	}
	if ( hand != menuHand ) {
		/* The clicking hand becomes the pointer; its cursor was tracked as the other one. */
		int x = vr.menuCursorX, y = vr.menuCursorY;
		vr.menuCursorX = vr.offhandCursorX;
		vr.menuCursorY = vr.offhandCursorY;
		vr.offhandCursorX = x;
		vr.offhandCursorY = y;
		vr.menuLeftHanded = hand == 0;
		if ( Key_GetCatcher() & KEYCATCH_UI )
			CL_MouseEvent( 0, 0, com_frameTime );
		/* nothing showed where that hand pointed, ray or cursor, so this press only takes the pointer over */
		if ( vr.pointerMode != VR_POINTER_STICK ) {
			VR_Vibrate( 200, hand + 1, 0.8f );
			return;
		}
	}
	/* a ray that is off the screen clicks nothing */
	if ( vr.pointerMode == VR_POINTER_DRAWN && !pointerOnScreen[hand] )
		return;
	if ( !vr.menuCursorActive && vr.pointerMode != VR_POINTER_STICK )
		return;
	clickKey[slot] = vr.pointerMode == VR_POINTER_STICK && !VKeyboard_IsActive() ? K_ENTER : K_MOUSE1;
	CL_KeyEvent( clickKey[slot], qtrue, com_frameTime );
	VR_Vibrate( 200, (vr.menuLeftHanded ? 0 : 1) + 1, 0.8f );
}
static void VR_Router_ClickDown_f( void ) {
	VR_Router_Click( qtrue );
}
static void VR_Router_ClickUp_f( void ) {
	VR_Router_Click( qfalse );
}

static void VR_Router_Nav( qboolean down ) {
	const int key = VR_Router_SourceKey(), slot = key >= 0 ? key : 0;
	const char *dir = Cmd_Argv( 1 );
	const qboolean console = (Key_GetCatcher() & KEYCATCH_CONSOLE) != 0;
	int target;
	if ( !down ) {
		if ( navKey[slot] )
			CL_KeyEvent( navKey[slot], qfalse, com_frameTime );
		navKey[slot] = 0;
		return;
	}
	if ( !Q_stricmp( dir, "up" ) )
		target = console ? K_PGUP : K_UPARROW;
	else if ( !Q_stricmp( dir, "down" ) )
		target = console ? K_PGDN : K_DOWNARROW;
	else if ( !Q_stricmp( dir, "left" ) )
		target = K_LEFTARROW;
	else if ( !Q_stricmp( dir, "right" ) )
		target = K_RIGHTARROW;
	else
		return;
	if ( vr.pointerMode != VR_POINTER_STICK ) {
		navAnchorX = vr.menuCursorX;
		navAnchorY = vr.menuCursorY;
	}
	vr.pointerMode = VR_POINTER_STICK;
	navKey[slot] = target;
	CL_KeyEvent( target, qtrue, com_frameTime );
}
static void VR_Router_NavDown_f( void ) {
	VR_Router_Nav( qtrue );
}
static void VR_Router_NavUp_f( void ) {
	VR_Router_Nav( qfalse );
}

static void VR_Router_SelectDown_f( void ) {
	weaponSelectHeld++;
}
static void VR_Router_SelectUp_f( void ) {
	if ( weaponSelectHeld > 0 )
		weaponSelectHeld--;
}
static void VR_Router_StabiliseDown_f( void ) {
	stabiliseHeld++;
}
static void VR_Router_StabiliseUp_f( void ) {
	if ( stabiliseHeld > 0 )
		stabiliseHeld--;
}
/* Alt is read from the holds; the command only has to exist. */
static void VR_Router_Alt_f( void ) {
}
static void VR_Router_VoteYesDown_f( void ) {
	vr.vote_holding = 1;
}
static void VR_Router_VoteNoDown_f( void ) {
	vr.vote_holding = -1;
}
static void VR_Router_VoteUp_f( void ) {
	vr.vote_holding = 0;
}

static int VR_Router_SnapAngle( void ) {
	return vr_snapturn->integer > 1 ? vr_snapturn->integer : 45;
}
static void VR_Router_TurnLeft_f( void ) {
	if ( vr_snapturn->integer > 0 )
		CL_SnapTurn( -VR_Router_SnapAngle() );
}
static void VR_Router_TurnRight_f( void ) {
	if ( vr_snapturn->integer > 0 )
		CL_SnapTurn( VR_Router_SnapAngle() );
}
static void VR_Router_UTurn_f( void ) {
	CL_SnapTurn( 180 );
}

static void VR_Router_Recenter_f( void ) {
	vr.menuYaw = vr.hmdorientation[YAW];
	VR_VirtualScreen_Reanchor();
}

/* Bindings-menu capture arms on the next input frame, so releasing holds never re-enters the UI from its own call. */
static enum { CAPTURE_IDLE, CAPTURE_ARMING, CAPTURE_ARMED } capture;

void VR_Router_BindCapture( void ) {
	if ( Key_GetCatcher() & KEYCATCH_UI )
		capture = CAPTURE_ARMING;
}

void VR_Router_CancelCapture( void ) {
	capture = CAPTURE_IDLE;
}

/* While armed the UI gets the gesture's key code once its buttons are let go, instead of a binding; qtrue while capture owns the keys. */
static qboolean VR_Router_Capture( void ) {
	vrBindEvent_t events[VRK_COUNT];
	const char *global;
	int key;
	if ( capture == CAPTURE_IDLE )
		return qfalse;
	if ( !uivm || !(Key_GetCatcher() & KEYCATCH_UI) ) {
		capture = CAPTURE_IDLE;
		return qfalse;
	}
	if ( capture == CAPTURE_ARMING ) {
		VR_Router_RunEvents( events, VR_ReleaseAll( &holds, events, ARRAY_LEN( events ) ), qtrue );
		capture = CAPTURE_ARMED;
	}
	key = VR_CaptureKey( &holds, keysNow );
	if ( key < 0 )
		return qtrue;
	capture = CAPTURE_IDLE;
	global = CL_VRBind_Lookup( VRC_GLOBAL, 0, (vrKey_t)key, NULL );
	VM_Call( uivm, 2, UI_KEY_EVENT, global && !Q_stricmp( global, "+key ESCAPE" ) ? K_ESCAPE : K_VR_WPN_TRIGGER + key,
			 qtrue );
	return qtrue;
}

void VR_Router_Reset( void ) {
	vrBindEvent_t events[VRK_COUNT];
	VR_Router_HoldsReady();
	capture = CAPTURE_IDLE;
	VR_Router_RunEvents( events, VR_ReleaseAll( &holds, events, VRK_COUNT ), qtrue );
	memset( &input, 0, sizeof( input ) );
	memset( keysNow, 0, sizeof( keysNow ) );
	weaponSelectHeld = stabiliseHeld = 0;
	vr.weapon_select = vr.weapon_select_using_thumbstick = vr.weapon_select_autoclose = qfalse;
	vr.weapon_stabilised = vr.walking = vr.vkbOffhandTriggerDown = qfalse;
	vr.vote_holding = 0;
	vr.pointerMode = VR_POINTER_CURSOR;
	vr.thumbstick_location[0][0] = vr.thumbstick_location[0][1] = 0;
	vr.thumbstick_location[1][0] = vr.thumbstick_location[1][1] = 0;
}

void VR_Router_Init( void ) {
	static const struct {
		const char *name;
		xcommand_t fn;
	} commands[] = {{"+key", VR_Router_KeyDown_f},
					{"-key", VR_Router_KeyUp_f},
					{"+vr_click", VR_Router_ClickDown_f},
					{"-vr_click", VR_Router_ClickUp_f},
					{"+menunav", VR_Router_NavDown_f},
					{"-menunav", VR_Router_NavUp_f},
					{"+weapon_select", VR_Router_SelectDown_f},
					{"-weapon_select", VR_Router_SelectUp_f},
					{"+weapon_stabilise", VR_Router_StabiliseDown_f},
					{"-weapon_stabilise", VR_Router_StabiliseUp_f},
					{"+alt", VR_Router_Alt_f},
					{"-alt", VR_Router_Alt_f},
					{"+vote_yes", VR_Router_VoteYesDown_f},
					{"-vote_yes", VR_Router_VoteUp_f},
					{"+vote_no", VR_Router_VoteNoDown_f},
					{"-vote_no", VR_Router_VoteUp_f},
					{"turnleft", VR_Router_TurnLeft_f},
					{"turnright", VR_Router_TurnRight_f},
					{"uturn", VR_Router_UTurn_f},
					{"vr_recenter", VR_Router_Recenter_f}};
	unsigned i;
	VR_Router_Reset();
	for ( i = 0; i < ARRAY_LEN( commands ); i++ ) {
		Cmd_RemoveCommand( commands[i].name );
		Cmd_AddCommand( commands[i].name, commands[i].fn );
	}
	vr.menuCursorX = vr.offhandCursorX = 320;
	vr.menuCursorY = vr.offhandCursorY = 240;
}

const clXRHandInput_t *VR_Router_Hands( void ) {
	return input.valid ? input.hands : NULL;
}

qboolean VR_Router_PointerLayer( void ) {
	return VR_StackHas( &stack, VRC_MENU ) || VR_StackHas( &stack, VRC_SCOREBOARD );
}

/* Movement and smooth turn only reach the game from gameplay or an overlay on it. */
qboolean VR_Router_ModalLayer( void ) {
	return VR_Router_PointerLayer() || VR_StackHas( &stack, VRC_ADJUST ) || VR_StackHas( &stack, VRC_SCRUB );
}

static void VR_Router_Signals( vrSignals_t *s ) {
	const int catcher = Key_GetCatcher();
	memset( s, 0, sizeof( *s ) );
	s->textEntry = VKeyboard_IsActive();
	s->menu = (catcher & (KEYCATCH_UI | KEYCATCH_CONSOLE)) != 0;
	s->offline = clc.state != CA_ACTIVE;
	s->adjust = vr.weapon_adjust;
	s->scrub = vr.menuYawLocked;
	/* The flag can outlive the cgame catcher that Escape strips; the catcher decides. */
	s->scoreboard = vr.scoreboardCursorActive && (catcher & KEYCATCH_CGAME);
	s->vote = vr.vote_active;
	s->wheel = vr.weapon_select;
	s->tv = tvPlay.active;
	s->demo = clc.demoplaying;
	s->following = (cl.snap.ps.pm_flags & PMF_FOLLOW) != 0;
	s->intermission = cl.snap.ps.pm_type == PM_INTERMISSION;
	s->dead = cl.snap.ps.stats[STAT_HEALTH] <= 0;
	s->spectating = cl.snap.ps.persistant[PERS_TEAM] == TEAM_SPECTATOR;
}

void VR_Router_Frame( const clXRHandInput_t hands[2] ) {
	qboolean wheel, priorWheel, menu;
	int i;
	vrSignals_t signals;
	vrThresholds_t thresholds;
	vrBindEvent_t events[VRK_COUNT * 2];
	unsigned char previous[VRK_COUNT];
	VR_Router_HoldsReady();
	memcpy( input.hands, hands, sizeof( input.hands ) );
	input.valid = qtrue;
	input.previousTime = input.time;
	input.time = cls.realtime;
	CL_VRBind_SetProfile( hands[1].profile >= 0 ? hands[1].profile : hands[0].profile );
	VR_Router_Signals( &signals );
	VR_ResolveStack( &signals, &stack );
	menu = VR_StackHas( &stack, VRC_MENU );
	thresholds.triggerPress = 1 - vr_triggerSensitivity->value;
	thresholds.triggerRelease = thresholds.triggerPress - .25f > .1f ? thresholds.triggerPress - .25f : .1f;
	thresholds.gripPress = vr_gripThreshold->value;
	thresholds.gripRelease = thresholds.gripPress - .1f;
	thresholds.padPress = vr_trackpadThreshold->value;
	thresholds.padRelease = thresholds.padPress - .1f;
	thresholds.stickPress = .5f;
	thresholds.stickRelease = .35f;
	thresholds.deadzone = vr_thumbstickDeadzone->value;
	memcpy( previous, keysNow, sizeof( previous ) );
	VR_SampleKeys( hands, vr.right_handed, vr_switchThumbsticks->integer, &thresholds, previous, keysNow );
	VR_RoleSticks( hands, vr.right_handed, vr_switchThumbsticks->integer, thresholds.deadzone,
				   vr.thumbstick_location[VR_STICK_MOVE], vr.thumbstick_location[VR_STICK_TURN] );
	{
		const int alt = VR_HoldsBound( &holds, "+alt" );
		if ( VR_StickTaken( &stack, alt, VRK_MOVESTICK, CL_VRBind_Lookup, NULL ) )
			vr.thumbstick_location[VR_STICK_MOVE][0] = vr.thumbstick_location[VR_STICK_MOVE][1] = 0;
		if ( VR_StickTaken( &stack, alt, VRK_TURNSTICK, CL_VRBind_Lookup, NULL ) )
			vr.thumbstick_location[VR_STICK_TURN][0] = vr.thumbstick_location[VR_STICK_TURN][1] = 0;
	}
	{
		const int mapping = vr.right_handed | (vr_switchThumbsticks->integer != 0) << 1;
		VR_Router_RunEvents( events, VR_HoldsSetMapping( &holds, mapping, events, ARRAY_LEN( events ) ), qfalse );
	}
	if ( !VR_Router_Capture() )
		VR_Router_RunEvents( events,
							 VR_UpdateHolds( &holds, keysNow, &stack, CL_VRBind_Lookup, NULL, com_frameTime, events,
											 ARRAY_LEN( events ) ),
							 qfalse );
	if ( vr.pointerMode == VR_POINTER_STICK ) {
		qboolean held = qfalse;
		for ( i = 0; i < VRK_COUNT; i++ )
			held |= navKey[i] != 0;
		if ( !menu )
			vr.pointerMode = VR_POINTER_CURSOR;
		else if ( !held ) {
			int dx = vr.menuCursorX - navAnchorX, dy = vr.menuCursorY - navAnchorY;
			if ( dx * dx + dy * dy > 3600 )
				vr.pointerMode = VR_POINTER_CURSOR;
		}
	}
	/* Outside stick navigation the mode says who draws the cursor: this frame's ray and pool of light, or the
	 * module, as it always does with vr_controllerModels off. The off hand points too while the keyboard is up,
	 * since its trigger types. */
	if ( vr.pointerMode != VR_POINTER_STICK ) {
		const int menuHand = vr.menuLeftHanded ? 0 : 1;
		qboolean drawn = qfalse;
		if ( vr_controllerModels->integer && VR_Router_PointerLayer() && vr.virtual_screen && vr.menuCursorActive &&
			 !vr.weapon_adjust && !vr.menuYawLocked ) {
			drawn = IN_VRShowPointer( menuHand, vr.menuCursorX, vr.menuCursorY, &pointerOnScreen[menuHand] );
			if ( VKeyboard_IsActive() )
				IN_VRShowPointer( 1 - menuHand, vr.offhandCursorX, vr.offhandCursorY, &pointerOnScreen[1 - menuHand] );
		}
		vr.pointerMode = drawn ? VR_POINTER_DRAWN : VR_POINTER_CURSOR;
	}
	priorWheel = vr.weapon_select;
	wheel = weaponSelectHeld > 0 && !clc.demoplaying && !(cl.snap.ps.pm_flags & PMF_FOLLOW);
	vr.weapon_select = wheel;
	vr.weapon_select_using_thumbstick = wheel && vr_weaponSelectorMode->integer == WS_HMD;
	vr.weapon_select_autoclose =
		vr.weapon_select_using_thumbstick && (keysNow[VRK_TURNSTICK_UP] || keysNow[VRK_TURNSTICK_DOWN] ||
											  keysNow[VRK_TURNSTICK_LEFT] || keysNow[VRK_TURNSTICK_RIGHT]);
	if ( priorWheel && !wheel && !menu )
		Cbuf_AddText( "weapon_select\n" );
	vr.weapon_stabilised = qfalse;
	if ( stabiliseHeld > 0 && hands[0].grip.positionValid && hands[1].grip.positionValid ) {
		vec3_t distance;
		VectorSubtract( vr.weaponposition, vr.offhandposition, distance );
		vr.weapon_stabilised = VectorLength( distance ) < .4f;
	}
	if ( vr_snapturn->integer <= 0 && !VR_Router_ModalLayer() && !Key_GetCatcher() && input.previousTime &&
		 !vr.weapon_select_using_thumbstick ) {
		float elapsed = Com_Clamp( 0, 100, input.time - input.previousTime ) * .001f;
		/* VR sensitivity is independent of mouse sensitivity; 100 is normal speed.
		 * The reference full-stick rate is 32767 * .022 degrees/second. */
		cl.viewangles[YAW] -= vr.thumbstick_location[VR_STICK_TURN][0] * (32767.0f * .022f) *
							  (vr_sensitivity->value / 100.0f) * elapsed;
	}
}

static void VR_Router_FinalizePose( usercmd_t *cmd ) {
	int pitch, yaw;

	/* Head orientation rides in bits 12-25, which only a 32-bit command carries. */
	if ( !clc.serverSupportsVR )
		return;
	pitch = (int)((Com_Clamp( -80, 80, vr.hmdorientation[PITCH] ) + 90) * 127 / 180);
	yaw = (int)((Com_Clamp( -80, 80, AngleSubtract( vr.hmdorientation[YAW], vr.weaponangles[YAW] ) ) + 90) * 127 /
				180);
	cmd->buttons |= (pitch & 127) << 12 | (yaw & 127) << 19;
}

void VR_Router_ApplyMove( usercmd_t *cmd ) {
	float side, forward, angle, x, magnitude;
	vec3_t angles;
	usercmd_t keys;
	int primary, i, catcher;
	/* Read every frame so a press made while neutral cannot fire afterwards. */
	memset( &keys, 0, sizeof( keys ) );
	CL_VRInput_KeyState( &keys );
	memset( cmd, 0, sizeof( *cmd ) );
	cmd->weapon = cl.cgameUserCmdValue;
	cmd->serverTime = cl.serverTime;
	for ( i = 0; i < 3; i++ )
		cmd->angles[i] = ANGLE2SHORT( cl.viewangles[i] );
	/* The catcher owns actions, but command metadata identifies the
	 * headset to the server while a menu, chat, or scoreboard is open. */
	catcher = Key_GetCatcher();
	if ( catcher )
		cmd->buttons |= BUTTON_TALK;
	if ( !input.valid || cls.realtime - input.time > 250 )
		return;
	if ( catcher || VR_Router_ModalLayer() || vr.weapon_adjust || vr.menuYawLocked ||
		(vr.virtual_screen && !vr.first_person_following) ) {
		VR_Router_FinalizePose( cmd );
		return;
	}
	vr.clientNum = cl.snap.ps.clientNum;
	primary = vr.right_handed ? 1 : 0;
	side = vr.thumbstick_location[VR_STICK_MOVE][0];
	forward = vr.thumbstick_location[VR_STICK_MOVE][1];
	angle = vr_directionMode->integer ? vr.offhandangles[YAW] : vr.hmdorientation[YAW];
	if ( vr.use_6dof )
		angle -= vr.hmdorientation[YAW];
	angle *= (float)M_PI / 180;
	x = side;
	side = cosf( angle ) * x - sinf( angle ) * forward;
	forward = cosf( angle ) * forward + sinf( angle ) * x;
	if ( vr.use_6dof && input.previousTime && input.time > input.previousTime ) {
		/* Convert room-scale meters per millisecond to command movement units. */
		float factor = 10000.0f / (72.0f * (input.time - input.previousTime));
		float px = -vr.hmdposition_delta[0] * factor, py = vr.hmdposition_delta[2] * factor;
		float yaw = -vr.hmdorientation[YAW] * (float)M_PI / 180;
		side += cosf( yaw ) * px - sinf( yaw ) * py;
		forward += cosf( yaw ) * py + sinf( yaw ) * px;
	}
	magnitude = sqrtf( side * side + forward * forward );
	if ( magnitude >= vr_thumbstickFullDeflection->value ) {
		float maximum = fmaxf( fabsf( side ), fabsf( forward ) );
		if ( maximum > 0 ) {
			side /= maximum;
			forward /= maximum;
		}
	}
	cmd->rightmove = (signed char)Com_Clamp( -127, 127, side * 127 + keys.rightmove );
	cmd->forwardmove = (signed char)Com_Clamp( -127, 127, forward * 127 + keys.forwardmove );
	cmd->upmove = keys.upmove;
	if ( vr.use_6dof && cl.snap.ps.pm_type == PM_SPECTATOR && !(cl.snap.ps.pm_flags & PMF_FOLLOW) ) {
		float pitch = vr.offhandangles[PITCH] * (float)M_PI / 180;
		int original = cmd->forwardmove;
		cmd->forwardmove = (signed char)Com_Clamp( -127, 127, original * cosf( pitch ) );
		cmd->upmove = (signed char)Com_Clamp( -127, 127, cmd->upmove - original * sinf( pitch ) );
	}
	cmd->buttons |= keys.buttons & 0xFFF & ~BUTTON_TALK; // bits 12-25 carry the head pose
	if ( !input.hands[primary].aim.orientationValid || !input.hands[primary].grip.positionValid )
		cmd->buttons &= ~BUTTON_ATTACK;
	if ( vr_analogWalk->integer ) {
		int speed = abs( cmd->rightmove ) > abs( cmd->forwardmove ) ? abs( cmd->rightmove ) : abs( cmd->forwardmove );
		if ( vr.walking && speed > 64 )
			vr.walking = qfalse;
		else if ( !vr.walking && speed < 64 - .04f * 127 )
			vr.walking = qtrue;
	} else
		vr.walking = qfalse;
	if ( vr.walking || (keys.buttons & BUTTON_WALKING) )
		cmd->buttons |= BUTTON_WALKING;
	if ( !vr.use_6dof && input.hands[primary].aim.orientationValid ) {
		if ( vr.realign > 0 && --vr.realign == 0 )
			VectorCopy( vr.hmdposition, vr.hmdorigin );
		VectorCopy( vr.calculated_weaponangles, angles );
		angles[PITCH] -= SHORT2ANGLE( cl.snap.ps.delta_angles[PITCH] );
		angles[YAW] += cl.viewangles[YAW] - vr.hmdorientation[YAW];
		// Servers reading 32-bit commands take head roll from the angles; this option sends it
		// to the rest.
		angles[ROLL] = (vr_sendRollToServer->integer || clc.serverSupportsVR)
							? Com_Clamp( -60, 60, vr.hmdorientation[ROLL] )
							: 0;
		for ( i = 0; i < 3; i++ )
			cmd->angles[i] = ANGLE2SHORT( angles[i] );
		angle = -vr.calculated_weaponangles[YAW] * (float)M_PI / 180;
		x = cmd->rightmove;
		cmd->rightmove = (signed char)Com_Clamp( -127, 127, cosf( angle ) * x - sinf( angle ) * cmd->forwardmove );
		cmd->forwardmove =
			(signed char)Com_Clamp( -127, 127, cosf( angle ) * cmd->forwardmove + sinf( angle ) * x );
	}
	VR_Router_FinalizePose( cmd );
}
