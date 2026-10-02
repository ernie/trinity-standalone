//
// cl_keyboard.c -- on-screen virtual keyboard for VR
//
// This keyboard is part of the client and sends key events through the
// normal input path (CL_KeyEvent/CL_CharEvent), so it works with both
// the console and UI menus automatically.
//
// It'd be better to use the OpenXR keyboard, but we're not there yet.
//

#include "client.h"
#include "../vrcommon/vr_clientinfo.h"

extern vr_clientinfo_t vr;

#define KEYBOARD_ROWS			4
#define KEYBOARD_KEY_WIDTH		36
#define KEYBOARD_KEY_HEIGHT		32
#define KEYBOARD_KEY_SPACING	4
#define KEYBOARD_ROW_SPACING	4
#define KEYBOARD_PADDING		12
#define KEYBOARD_START_Y		308
#define KEYBOARD_TEXT_SIZE		10	// 120% of SMALLCHAR_WIDTH (8)
#define ICON_ARROW_SIZE			18	// arrow head triangles on composite icons
#define ICON_LINE_SIZE			14	// horizontal line, vertical bar on composite icons
#define ICON_GLYPH_OVERLAP		4	// overlap between adjacent glyphs to close gaps
#define SHIFT_CHAR_SPACING		8	// tighter kerning for SHIFT/CAPS labels

// Special key codes (internal to keyboard)
#define VKEY_NONE		0
#define VKEY_SHIFT		1
#define VKEY_BACKSPACE	2
#define VKEY_SYMBOLS	3
#define VKEY_SPACE		4
#define VKEY_ENTER		5
#define VKEY_LEFT		6
#define VKEY_RIGHT		7
#define VKEY_UP			8
#define VKEY_DOWN		9
#define VKEY_TAB		10

// Time window for double-tap shift to enable caps lock (milliseconds)
#define CAPSLOCK_DOUBLE_TAP_TIME	400

// Key repeat timing (milliseconds)
#define KEY_REPEAT_DELAY	500		// initial delay before repeat starts
#define KEY_REPEAT_RATE		80		// interval between repeats (~12.5/sec)

// Keyboard modes
#define MODE_LOWERCASE	0
#define MODE_UPPERCASE	1
#define MODE_SYMBOLS1	2
#define MODE_SYMBOLS2	3

// Hand indices for repeat state ownership
#define VKB_HAND_PRIMARY	0
#define VKB_HAND_OFFHAND	1

// Key definition
typedef struct {
	char		lowercase;
	char		uppercase;
	char		symbol1;
	char		symbol2;
	int			special;
	float		width;
	const char	*label;
	const char	*symbolLabel;
} vKeyDef_t;

// Keyboard state
typedef struct {
	qboolean	active;
	int			mode;
	qboolean	capsLock;
	int			lastShiftTime;
	sfxHandle_t	clickSound;
	// Key repeat state (last-key-wins: only one hand repeats at a time)
	vKeyDef_t	*repeatKey;			// key currently being held (NULL if none)
	int			repeatChar;			// resolved character at initial press (0 for special keys)
	int			repeatSpecial;		// VKEY_* code at initial press (0 for regular chars)
	int			repeatPressTime;	// cls.realtime when key was first pressed
	int			repeatLastTime;		// cls.realtime when last repeat fired
	qboolean	repeatStarted;		// past initial delay?
	int			repeatHand;			// which hand owns repeat (VKB_HAND_PRIMARY or VKB_HAND_OFFHAND)
} vKeyboardState_t;

static vKeyboardState_t vkb;

// Row 0: Q W E R T Y U I O P (letters) / 1-0 (sym1) / [ ] { } # % ^ * + = (sym2)
static vKeyDef_t vkbRow0[] = {
	{'q', 'Q', '1', '[', 0, 1.0f, NULL, NULL},
	{'w', 'W', '2', ']', 0, 1.0f, NULL, NULL},
	{'e', 'E', '3', '{', 0, 1.0f, NULL, NULL},
	{'r', 'R', '4', '}', 0, 1.0f, NULL, NULL},
	{'t', 'T', '5', '#', 0, 1.0f, NULL, NULL},
	{'y', 'Y', '6', '%', 0, 1.0f, NULL, NULL},
	{'u', 'U', '7', '^', 0, 1.0f, NULL, NULL},
	{'i', 'I', '8', '*', 0, 1.0f, NULL, NULL},
	{'o', 'O', '9', '+', 0, 1.0f, NULL, NULL},
	{'p', 'P', '0', '=', 0, 1.0f, NULL, NULL},
	{0, 0, 0, 0, 0, 0, NULL, NULL}
};

// Row 1: [Tab/CON] A S D F G H J K L
static vKeyDef_t vkbRow1[] = {
	{0, 0, 0, 0, VKEY_TAB, 1.5f, "TAB", "TAB"},
	{'a', 'A', '-', '_', 0, 1.0f, NULL, NULL},
	{'s', 'S', '/', '\\', 0, 1.0f, NULL, NULL},
	{'d', 'D', ':', '|', 0, 1.0f, NULL, NULL},
	{'f', 'F', ';', ';', 0, 1.0f, NULL, NULL},
	{'g', 'G', '(', '<', 0, 1.0f, NULL, NULL},
	{'h', 'H', ')', '>', 0, 1.0f, NULL, NULL},
	{'j', 'J', '$', '`', 0, 1.0f, NULL, NULL},
	{'k', 'K', '&', '/', 0, 1.0f, NULL, NULL},
	{'l', 'L', '@', '-', 0, 1.0f, NULL, NULL},
	{0, 0, 0, 0, 0, 0, NULL, NULL}
};

// Row 2: [Shift/#+=] Z X C V B N M [Backspace]
static vKeyDef_t vkbRow2[] = {
	{0, 0, 0, 0, VKEY_SHIFT, 1.5f, NULL, "#+="},
	{'z', 'Z', '.', '.', 0, 1.0f, NULL, NULL},
	{'x', 'X', ',', ',', 0, 1.0f, NULL, NULL},
	{'c', 'C', '?', '?', 0, 1.0f, NULL, NULL},
	{'v', 'V', '!', '!', 0, 1.0f, NULL, NULL},
	{'b', 'B', '\'', '\'', 0, 1.0f, NULL, NULL},
	{'n', 'N', '"', '"', 0, 1.0f, NULL, NULL},
	{'m', 'M', '=', ':', 0, 1.0f, NULL, NULL},
	{0, 0, 0, 0, VKEY_BACKSPACE, 1.5f, NULL, NULL},
	{0, 0, 0, 0, 0, 0, NULL, NULL}
};

// Row 3: [123/ABC] [Space] [arrows] [Done]
static vKeyDef_t vkbRow3[] = {
	{0, 0, 0, 0, VKEY_SYMBOLS, 1.3f, "123", "ABC"},
	{0, 0, 0, 0, VKEY_SPACE, 4.5f, " ", " "},
	{0, 0, 0, 0, VKEY_LEFT, 1.0f, NULL, NULL},
	{0, 0, 0, 0, VKEY_UP, 1.0f, NULL, NULL},
	{0, 0, 0, 0, VKEY_DOWN, 1.0f, NULL, NULL},
	{0, 0, 0, 0, VKEY_RIGHT, 1.0f, NULL, NULL},
	{0, 0, 0, 0, VKEY_ENTER, 1.3f, NULL, NULL},
	{0, 0, 0, 0, 0, 0, NULL, NULL}
};

static vKeyDef_t *vkbRows[KEYBOARD_ROWS] = { vkbRow0, vkbRow1, vkbRow2, vkbRow3 };

// Icon identifiers
#define ICON_NONE		0
#define ICON_BACKSPACE	1
#define ICON_LEFT		2
#define ICON_RIGHT		3
#define ICON_UP			4
#define ICON_DOWN		5
#define ICON_SHIFT		6
#define ICON_CAPSLOCK	7
#define ICON_ENTER		8
#define ICON_TAB		9

// Forward declarations for functions called before their definition
static void VKeyboard_FireAction( int ch, int special );
static void VKeyboard_ProcessKeyPress( vKeyDef_t *keyDef, int handIndex );

/*
=================
VKeyboard_DrawGlyph

Draw a single bigchars glyph at a specific position and size.
=================
*/
static void VKeyboard_DrawGlyph( int ch, int x, int y, int size, vec4_t color ) {
	char str[2] = { (char)ch, '\0' };
	SCR_DrawStringExtNoShadow( x, y, size, str, color, qtrue, qtrue );
}

/*
=================
VKeyboard_DrawIcon
=================
*/
static void VKeyboard_DrawIcon( int icon, int x, int y, int w, int h, vec4_t color ) {
	int cx = x + w/2;
	int cy = y + h/2;

	switch (icon) {
		case ICON_LEFT:
		case ICON_RIGHT:
		case ICON_UP:
		case ICON_DOWN:
			{
				static const unsigned char glyphs[] = {
					[ICON_LEFT]  = 136,
					[ICON_RIGHT] = 141,
					[ICON_UP]    = 135,
					[ICON_DOWN]  = 134,
				};
				VKeyboard_DrawGlyph( glyphs[icon],
					cx - KEYBOARD_TEXT_SIZE / 2,
					cy - KEYBOARD_TEXT_SIZE / 2,
					KEYBOARD_TEXT_SIZE, color );
			}
			break;

		case ICON_BACKSPACE:
			{
				// Left-pointing arrow: triangle + horizontal line (nudge left)
				int totalW = ICON_ARROW_SIZE + ICON_LINE_SIZE - ICON_GLYPH_OVERLAP;
				int startX = cx - totalW / 2 - 2;
				VKeyboard_DrawGlyph( 136, startX,
					cy - ICON_ARROW_SIZE / 2, ICON_ARROW_SIZE, color );
				VKeyboard_DrawGlyph( 30, startX + ICON_ARROW_SIZE - ICON_GLYPH_OVERLAP,
					cy - ICON_LINE_SIZE / 2, ICON_LINE_SIZE, color );
			}
			break;

		case ICON_ENTER:
			{
				// Right-pointing arrow: horizontal line + triangle (nudge right)
				int totalW = ICON_LINE_SIZE + ICON_ARROW_SIZE - ICON_GLYPH_OVERLAP;
				int startX = cx - totalW / 2 + 2;
				VKeyboard_DrawGlyph( 30, startX,
					cy - ICON_LINE_SIZE / 2, ICON_LINE_SIZE, color );
				VKeyboard_DrawGlyph( 141, startX + ICON_LINE_SIZE - ICON_GLYPH_OVERLAP,
					cy - ICON_ARROW_SIZE / 2, ICON_ARROW_SIZE, color );
			}
			break;

		case ICON_TAB:
			{
				// Right-pointing arrow + tab stop: line + triangle + vertical bar
				// Glyph 21 is left-hugging so most of its cell is empty on the right;
				// reduce its contribution to totalW to get a better visual center
				int barVisual = ICON_LINE_SIZE / 3;
				int totalW = ICON_LINE_SIZE + ICON_ARROW_SIZE + barVisual - ICON_GLYPH_OVERLAP * 2;
				int startX = cx - totalW / 2;
				VKeyboard_DrawGlyph( 30, startX,
					cy - ICON_LINE_SIZE / 2, ICON_LINE_SIZE, color );
				VKeyboard_DrawGlyph( 141, startX + ICON_LINE_SIZE - ICON_GLYPH_OVERLAP,
					cy - ICON_ARROW_SIZE / 2, ICON_ARROW_SIZE, color );
				VKeyboard_DrawGlyph( 21, startX + ICON_LINE_SIZE + ICON_ARROW_SIZE - ICON_GLYPH_OVERLAP * 2,
					cy - ICON_LINE_SIZE / 2, ICON_LINE_SIZE, color );
			}
			break;

		case ICON_SHIFT:
		case ICON_CAPSLOCK:
			{
				const char *label = (icon == ICON_SHIFT) ? "SHIFT" : "CAPS";
				int i, len = (int)strlen(label);
				int totalW = len * SHIFT_CHAR_SPACING;
				int startX = cx - totalW / 2;
				for (i = 0; i < len; i++) {
					VKeyboard_DrawGlyph( label[i],
						startX + i * SHIFT_CHAR_SPACING,
						cy - KEYBOARD_TEXT_SIZE / 2,
						KEYBOARD_TEXT_SIZE, color );
				}
			}
			break;
	}
}

/*
=================
VKeyboard_GetKeyIcon
=================
*/
static int VKeyboard_GetKeyIcon( vKeyDef_t *key ) {
	if (!key->special) {
		return ICON_NONE;
	}

	switch (key->special) {
		case VKEY_BACKSPACE:
			return ICON_BACKSPACE;
		case VKEY_LEFT:
			return ICON_LEFT;
		case VKEY_RIGHT:
			return ICON_RIGHT;
		case VKEY_UP:
			return ICON_UP;
		case VKEY_DOWN:
			return ICON_DOWN;
		case VKEY_SHIFT:
			if (vkb.mode == MODE_LOWERCASE || vkb.mode == MODE_UPPERCASE) {
				if (vkb.capsLock) {
					return ICON_CAPSLOCK;
				}
				return ICON_SHIFT;
			}
			return ICON_NONE;
		case VKEY_ENTER:
			return ICON_ENTER;
		case VKEY_TAB:
			return ICON_TAB;
		default:
			return ICON_NONE;
	}
}

static int VKeyboard_GetRowWidth( vKeyDef_t *row ) {
	int width = 0;
	int i;
	int count = 0;
	for (i = 0; row[i].width > 0; i++) {
		width += (int)(row[i].width * KEYBOARD_KEY_WIDTH);
		count++;
	}
	if (count > 1) {
		width += (count - 1) * KEYBOARD_KEY_SPACING;
	}
	return width;
}

static int VKeyboard_GetMaxRowWidth( void ) {
	int maxWidth = 0;
	int i;
	for (i = 0; i < KEYBOARD_ROWS; i++) {
		int w = VKeyboard_GetRowWidth(vkbRows[i]);
		if (w > maxWidth) maxWidth = w;
	}
	return maxWidth;
}

static int VKeyboard_GetStartX( int rowWidth ) {
	return (SCREEN_WIDTH - rowWidth) / 2;
}

/*
=================
VKeyboard_Show
=================
*/
void VKeyboard_Show( void ) {
	vkb.active = qtrue;
	vkb.mode = MODE_LOWERCASE;
	vkb.capsLock = qfalse;
	vkb.lastShiftTime = 0;
	vkb.repeatKey = NULL;
	// Register click sound if not already registered
	if ( !vkb.clickSound ) {
		vkb.clickSound = S_RegisterSound( "sound/misc/click.wav", qfalse );
	}
}

/*
=================
VKeyboard_Hide
=================
*/
void VKeyboard_Hide( void ) {
	vkb.active = qfalse;
	vkb.repeatKey = NULL;
	vkb.clickSound = 0;	// stale across sound-subsystem reset (mod switch, snd_restart)
	vr.vkbOffhandTriggerDown = qfalse;
}

/*
=================
VKeyboard_IsActive
=================
*/
qboolean VKeyboard_IsActive( void ) {
	return vkb.active;
}

/*
=================
VKeyboard_GetKeyAt
=================
*/
static vKeyDef_t* VKeyboard_GetKeyAt( int x, int y ) {
	int row;
	int keyX, keyY, keyW;
	int i;

	for (row = 0; row < KEYBOARD_ROWS; row++) {
		int rowWidth = VKeyboard_GetRowWidth(vkbRows[row]);
		int startX = VKeyboard_GetStartX(rowWidth);

		keyY = KEYBOARD_START_Y + row * (KEYBOARD_KEY_HEIGHT + KEYBOARD_ROW_SPACING);

		if (y < keyY || y > keyY + KEYBOARD_KEY_HEIGHT) {
			continue;
		}

		keyX = startX;
		for (i = 0; vkbRows[row][i].width > 0; i++) {
			vKeyDef_t *key = &vkbRows[row][i];
			keyW = (int)(key->width * KEYBOARD_KEY_WIDTH);

			if (x >= keyX && x <= keyX + keyW) {
				return key;
			}

			keyX += keyW + KEYBOARD_KEY_SPACING;
		}
	}

	return NULL;
}

/*
=================
VKeyboard_IsInKeyboardArea

Returns qtrue if the given point is within the keyboard background area
(the tinted overlay), even if not directly on a key.
=================
*/
static qboolean VKeyboard_IsInKeyboardArea( int x, int y ) {
	int maxWidth = VKeyboard_GetMaxRowWidth();
	int totalHeight = KEYBOARD_ROWS * KEYBOARD_KEY_HEIGHT + (KEYBOARD_ROWS - 1) * KEYBOARD_ROW_SPACING;
	int bgX = VKeyboard_GetStartX(maxWidth) - KEYBOARD_PADDING;
	int bgY = KEYBOARD_START_Y - KEYBOARD_PADDING;
	int bgW = maxWidth + KEYBOARD_PADDING * 2;
	int bgH = totalHeight + KEYBOARD_PADDING * 2;

	return (x >= bgX && x <= bgX + bgW && y >= bgY && y <= bgY + bgH);
}

/*
=================
VKeyboard_Draw
=================
*/
void VKeyboard_Draw( void ) {
	int row;
	int keyX, keyY, keyW;
	int i;
	int maxWidth = VKeyboard_GetMaxRowWidth();
	int totalHeight = KEYBOARD_ROWS * KEYBOARD_KEY_HEIGHT + (KEYBOARD_ROWS - 1) * KEYBOARD_ROW_SPACING;
	int bgX = VKeyboard_GetStartX(maxWidth) - KEYBOARD_PADDING;
	int bgY = KEYBOARD_START_Y - KEYBOARD_PADDING;
	int bgW = maxWidth + KEYBOARD_PADDING * 2;
	int bgH = totalHeight + KEYBOARD_PADDING * 2;

	vec4_t bgColor = {0.1f, 0.1f, 0.12f, 0.95f};
	vec4_t keyColor = {0.25f, 0.25f, 0.28f, 1.0f};
	vec4_t keyHoverColor = {0.4f, 0.4f, 0.45f, 1.0f};
	vec4_t keyPressedColor = {0.35f, 0.35f, 0.4f, 1.0f};
	vec4_t keyActiveColor = {0.3f, 0.5f, 0.8f, 1.0f};
	vec4_t keySpecialColor = {0.2f, 0.2f, 0.22f, 1.0f};
	vec4_t keyEnterColor = {0.2f, 0.35f, 0.6f, 1.0f};
	vec4_t keyEnterHoverColor = {0.3f, 0.45f, 0.7f, 1.0f};
	vec4_t textColor = {1.0f, 1.0f, 1.0f, 1.0f};
	vec4_t textDimColor = {0.7f, 0.7f, 0.7f, 1.0f};
	char str[2];
	vKeyDef_t *hoverKey, *offhandHoverKey;
	int cursorX, cursorY;
	int offhandCursorX, offhandCursorY;

	if (!vkb.active) {
		return;
	}

	// Get primary cursor position from VR
	if (vr.menuCursorActive) {
		cursorX = vr.menuCursorX;
		cursorY = vr.menuCursorY;
	} else {
		cursorX = SCREEN_WIDTH / 2;
		cursorY = SCREEN_HEIGHT / 2;
	}

	// Get offhand cursor position
	offhandCursorX = vr.offhandCursorX;
	offhandCursorY = vr.offhandCursorY;

	hoverKey = VKeyboard_GetKeyAt(cursorX, cursorY);
	offhandHoverKey = VKeyboard_GetKeyAt(offhandCursorX, offhandCursorY);

	// Key repeat logic (last-key-wins: check the owning hand's trigger and hover)
	if (vkb.repeatKey) {
		qboolean triggerDown;
		vKeyDef_t *ownerHoverKey;

		if (vkb.repeatHand == VKB_HAND_PRIMARY) {
			triggerDown = keys[K_MOUSE1].down;
			ownerHoverKey = hoverKey;
		} else {
			triggerDown = vr.vkbOffhandTriggerDown;
			ownerHoverKey = offhandHoverKey;
		}

		if (!triggerDown || ownerHoverKey != vkb.repeatKey) {
			vkb.repeatKey = NULL;
		} else {
			int elapsed = cls.realtime - vkb.repeatPressTime;
			if (!vkb.repeatStarted) {
				if (elapsed >= KEY_REPEAT_DELAY) {
					vkb.repeatStarted = qtrue;
					vkb.repeatLastTime = cls.realtime;
					VKeyboard_FireAction(vkb.repeatChar, vkb.repeatSpecial);
				}
			} else if (cls.realtime - vkb.repeatLastTime >= KEY_REPEAT_RATE) {
				if (cls.realtime - vkb.repeatLastTime > KEY_REPEAT_RATE * 3) {
					vkb.repeatLastTime = cls.realtime - KEY_REPEAT_RATE;
				}
				vkb.repeatLastTime += KEY_REPEAT_RATE;
				VKeyboard_FireAction(vkb.repeatChar, vkb.repeatSpecial);
			}
		}
	}

	// Draw background
	SCR_FillRect(bgX, bgY, bgW, bgH, bgColor);

	// Draw keys
	for (row = 0; row < KEYBOARD_ROWS; row++) {
		int rowWidth = VKeyboard_GetRowWidth(vkbRows[row]);
		int startX = VKeyboard_GetStartX(rowWidth);

		keyX = startX;
		keyY = KEYBOARD_START_Y + row * (KEYBOARD_KEY_HEIGHT + KEYBOARD_ROW_SPACING);

		for (i = 0; vkbRows[row][i].width > 0; i++) {
			vKeyDef_t *key = &vkbRows[row][i];
			qboolean isHoveredPrimary = (key == hoverKey);
			qboolean isHoveredOffhand = (key == offhandHoverKey);
			qboolean isHovered = isHoveredPrimary || isHoveredOffhand;
			qboolean isPressed = (isHoveredPrimary && keys[K_MOUSE1].down) ||
			                     (isHoveredOffhand && vr.vkbOffhandTriggerDown);
			qboolean isActive = qfalse;
			qboolean isSpecial = (key->special != 0);
			vec4_t *color;
			const char *label = NULL;
			char ch = 0;

			keyW = (int)(key->width * KEYBOARD_KEY_WIDTH);

			if (key->special == VKEY_SHIFT) {
				isActive = (vkb.mode == MODE_UPPERCASE) || vkb.capsLock || (vkb.mode == MODE_SYMBOLS2);
			} else if (key->special == VKEY_SYMBOLS) {
				isActive = (vkb.mode == MODE_SYMBOLS1) || (vkb.mode == MODE_SYMBOLS2);
			}

			if (isActive) {
				color = &keyActiveColor;
			} else if (key->special == VKEY_ENTER) {
				color = isHovered ? &keyEnterHoverColor : &keyEnterColor;
			} else if (isPressed) {
				color = &keyPressedColor;
			} else if (isHovered) {
				color = &keyHoverColor;
			} else if (isSpecial) {
				color = &keySpecialColor;
			} else {
				color = &keyColor;
			}

			SCR_FillRect(keyX, keyY, keyW, KEYBOARD_KEY_HEIGHT, *color);

			{
				int icon = VKeyboard_GetKeyIcon(key);

				if (icon != ICON_NONE) {
					VKeyboard_DrawIcon(icon, keyX, keyY, keyW, KEYBOARD_KEY_HEIGHT,
						isSpecial ? textDimColor : textColor);
				} else if (key->special) {
					if ((vkb.mode == MODE_SYMBOLS1 || vkb.mode == MODE_SYMBOLS2) && key->symbolLabel) {
						label = key->symbolLabel;
					} else {
						label = key->label;
					}
					if (label) {
						SCR_DrawStringExtNoShadow(keyX + keyW/2 - strlen(label) * KEYBOARD_TEXT_SIZE/2,
							keyY + KEYBOARD_KEY_HEIGHT/2 - KEYBOARD_TEXT_SIZE/2,
							KEYBOARD_TEXT_SIZE, label, isSpecial ? textDimColor : textColor, qfalse, qfalse);
					}
				} else {
					if (vkb.mode == MODE_SYMBOLS2) {
						ch = key->symbol2;
					} else if (vkb.mode == MODE_SYMBOLS1) {
						ch = key->symbol1;
					} else if (vkb.mode == MODE_UPPERCASE || vkb.capsLock) {
						ch = key->uppercase;
					} else {
						ch = key->lowercase;
					}
					if (ch) {
						str[0] = ch;
						str[1] = 0;
						SCR_DrawStringExtNoShadow(keyX + keyW/2 - KEYBOARD_TEXT_SIZE/2,
							keyY + KEYBOARD_KEY_HEIGHT/2 - KEYBOARD_TEXT_SIZE/2,
							KEYBOARD_TEXT_SIZE, str, textColor, qfalse, qfalse);
					}
				}
			}

			keyX += keyW + KEYBOARD_KEY_SPACING;
		}
	}

	// Draw cursors on top of keyboard as colored dots, unless each hand's ray already shows where it points
	// Blue = left physical hand, Red = right physical hand
	if ( vr.pointerMode != VR_POINTER_DRAWN )
	{
		#define CURSOR_DOT_SIZE	6

		// menuLeftHanded == true means left physical hand drives the primary cursor
		vec4_t colorLeft  = {0.3f, 0.5f, 1.0f, 1.0f};
		vec4_t colorRight = {1.0f, 0.3f, 0.3f, 1.0f};
		float *primaryColor = vr.menuLeftHanded ? colorLeft : colorRight;
		float *offhandColor = vr.menuLeftHanded ? colorRight : colorLeft;

		// Offhand cursor (draw first, behind primary)
		SCR_FillRect(offhandCursorX - CURSOR_DOT_SIZE / 2, offhandCursorY - CURSOR_DOT_SIZE / 2,
			CURSOR_DOT_SIZE, CURSOR_DOT_SIZE, offhandColor);

		// Primary cursor (on top)
		SCR_FillRect(cursorX - CURSOR_DOT_SIZE / 2, cursorY - CURSOR_DOT_SIZE / 2,
			CURSOR_DOT_SIZE, CURSOR_DOT_SIZE, primaryColor);
	}
}

/*
=================
VKeyboard_PlayClickSound
=================
*/
static void VKeyboard_PlayClickSound( void ) {
	if ( vkb.clickSound ) {
		S_StartLocalSound( vkb.clickSound, CHAN_LOCAL_SOUND );
	}
}

/*
=================
VKeyboard_SendChar

Send a character through the normal input path
=================
*/
static void VKeyboard_SendChar( int ch ) {
	CL_CharEvent( ch );
}

/*
=================
VKeyboard_SendKey

Send a key event through the normal input path
=================
*/
static void VKeyboard_SendKey( int key, qboolean down ) {
	CL_KeyEvent( key, down, cls.realtime );
}

/*
=================
VKeyboard_FireAction

Fires a key action (with click sound) without mode side effects.
Called by both initial press and repeat.
=================
*/
static void VKeyboard_FireAction( int ch, int special ) {
	if ( vkb.clickSound ) {
		S_StartLocalSound( vkb.clickSound, CHAN_LOCAL_SOUND );
	}

	if (ch) {
		VKeyboard_SendChar(ch);
		return;
	}

	switch (special) {
		case VKEY_BACKSPACE:
			VKeyboard_SendChar('h' - 'a' + 1);
			break;
		case VKEY_SPACE:
			VKeyboard_SendChar(' ');
			break;
		case VKEY_LEFT:
			VKeyboard_SendKey(K_LEFTARROW, qtrue);
			VKeyboard_SendKey(K_LEFTARROW, qfalse);
			break;
		case VKEY_RIGHT:
			VKeyboard_SendKey(K_RIGHTARROW, qtrue);
			VKeyboard_SendKey(K_RIGHTARROW, qfalse);
			break;
		case VKEY_UP:
			VKeyboard_SendKey(K_UPARROW, qtrue);
			VKeyboard_SendKey(K_UPARROW, qfalse);
			break;
		case VKEY_DOWN:
			VKeyboard_SendKey(K_DOWNARROW, qtrue);
			VKeyboard_SendKey(K_DOWNARROW, qfalse);
			break;
		case VKEY_TAB:
			VKeyboard_SendKey(K_TAB, qtrue);
			VKeyboard_SendKey(K_TAB, qfalse);
			break;
		case VKEY_ENTER:
			VKeyboard_SendKey(K_ENTER, qtrue);
			VKeyboard_SendKey(K_ENTER, qfalse);
			break;
	}
}

/*
=================
VKeyboard_ProcessKeyPress

Shared key press logic for both primary and offhand controllers.
Handles mode changes, fires the key action, and sets up repeat state.
The handIndex parameter (VKB_HAND_PRIMARY/VKB_HAND_OFFHAND) controls
which hand owns the repeat (last-key-wins).
=================
*/
static void VKeyboard_ProcessKeyPress( vKeyDef_t *keyDef, int handIndex ) {
	char ch;

	if (keyDef->special) {
		switch (keyDef->special) {
			case VKEY_SHIFT:
				VKeyboard_PlayClickSound();
				if (vkb.mode == MODE_SYMBOLS1) {
					vkb.mode = MODE_SYMBOLS2;
				} else if (vkb.mode == MODE_SYMBOLS2) {
					vkb.mode = MODE_SYMBOLS1;
				} else if (vkb.capsLock) {
					vkb.capsLock = qfalse;
					vkb.mode = MODE_LOWERCASE;
					vkb.lastShiftTime = 0;
				} else if (vkb.mode == MODE_UPPERCASE) {
					if (cls.realtime - vkb.lastShiftTime < CAPSLOCK_DOUBLE_TAP_TIME) {
						vkb.capsLock = qtrue;
						vkb.lastShiftTime = 0;
					} else {
						vkb.mode = MODE_LOWERCASE;
						vkb.lastShiftTime = 0;
					}
				} else {
					vkb.mode = MODE_UPPERCASE;
					vkb.lastShiftTime = cls.realtime;
				}
				vkb.repeatKey = NULL;
				return;

			case VKEY_SYMBOLS:
				VKeyboard_PlayClickSound();
				if (vkb.mode == MODE_SYMBOLS1 || vkb.mode == MODE_SYMBOLS2) {
					vkb.mode = vkb.capsLock ? MODE_UPPERCASE : MODE_LOWERCASE;
				} else {
					vkb.mode = MODE_SYMBOLS1;
				}
				vkb.repeatKey = NULL;
				return;

			case VKEY_ENTER:
				// In console, keep keyboard open for multiple commands
				// Otherwise (UI menu only), dismiss keyboard after Enter
				if (!(Key_GetCatcher() & KEYCATCH_CONSOLE)) {
					VKeyboard_PlayClickSound();
					VKeyboard_SendKey(K_ENTER, qtrue);
					VKeyboard_SendKey(K_ENTER, qfalse);
					VKeyboard_Hide();
					vkb.repeatKey = NULL;
					return;
				}
				// Fall through to repeatable key handling for console mode
			case VKEY_BACKSPACE:
			case VKEY_SPACE:
			case VKEY_LEFT:
			case VKEY_RIGHT:
			case VKEY_UP:
			case VKEY_DOWN:
			case VKEY_TAB:
				VKeyboard_FireAction(0, keyDef->special);
				vkb.repeatKey = keyDef;
				vkb.repeatChar = 0;
				vkb.repeatSpecial = keyDef->special;
				vkb.repeatPressTime = cls.realtime;
				vkb.repeatLastTime = cls.realtime;
				vkb.repeatStarted = qfalse;
				vkb.repeatHand = handIndex;
				return;
		}
		vkb.repeatKey = NULL;
		return;
	}

	// Regular character
	ch = 0;
	if (vkb.mode == MODE_SYMBOLS2) {
		ch = keyDef->symbol2;
	} else if (vkb.mode == MODE_SYMBOLS1) {
		ch = keyDef->symbol1;
	} else if (vkb.mode == MODE_UPPERCASE || vkb.capsLock) {
		ch = keyDef->uppercase;
	} else {
		ch = keyDef->lowercase;
	}

	if (ch) {
		VKeyboard_FireAction(ch, 0);
		// Set up repeat BEFORE shift revert so repeatChar captures the current char
		vkb.repeatKey = keyDef;
		vkb.repeatChar = ch;
		vkb.repeatSpecial = 0;
		vkb.repeatPressTime = cls.realtime;
		vkb.repeatLastTime = cls.realtime;
		vkb.repeatStarted = qfalse;
		vkb.repeatHand = handIndex;
		// Shift revert (after repeat state is captured)
		if (vkb.mode == MODE_UPPERCASE && !vkb.capsLock) {
			vkb.mode = MODE_LOWERCASE;
		}
	}
}

/*
=================
VKeyboard_DismissWithConsole

Dismiss the keyboard, and if console is active, close it too.
=================
*/
static void VKeyboard_DismissWithConsole( void ) {
	VKeyboard_Hide();
	if (Key_GetCatcher() & KEYCATCH_CONSOLE) {
		Con_ToggleConsole_f();
	}
}

/*
=================
VKeyboard_HandleKey

Returns qtrue if the keyboard handled this key event (primary hand via K_MOUSE1).
=================
*/
qboolean VKeyboard_HandleKey( int key ) {
	vKeyDef_t *keyDef;
	int cursorX, cursorY;

	if (!vkb.active) {
		return qfalse;
	}

	if (key == K_ESCAPE || key == K_MENU) {
		VKeyboard_DismissWithConsole();
		return qtrue;
	}

	if (key != K_MOUSE1) {
		return qfalse;
	}

	// Get cursor position from VR
	if (vr.menuCursorActive) {
		cursorX = vr.menuCursorX;
		cursorY = vr.menuCursorY;
	} else {
		cursorX = SCREEN_WIDTH / 2;
		cursorY = SCREEN_HEIGHT / 2;
	}

	keyDef = VKeyboard_GetKeyAt(cursorX, cursorY);
	if (!keyDef) {
		if (VKeyboard_IsInKeyboardArea(cursorX, cursorY)) {
			return qtrue;	// near miss
		}
		VKeyboard_DismissWithConsole();
		return qtrue;
	}

	VKeyboard_ProcessKeyPress(keyDef, VKB_HAND_PRIMARY);
	return qtrue;
}

/*
=================
VKeyboard_HandleOffhandKey

Called directly from vr_input.c when the offhand trigger is pressed/released
while the keyboard is active. Uses the offhand cursor position for hit testing.
=================
*/
void VKeyboard_HandleOffhandKey( qboolean down ) {
	vKeyDef_t *keyDef;

	if (!vkb.active) {
		return;
	}

	if (!down) {
		// Trigger released: clear repeat if offhand owns it
		if (vkb.repeatHand == VKB_HAND_OFFHAND) {
			vkb.repeatKey = NULL;
		}
		return;
	}

	// Trigger pressed: hit test at offhand cursor position
	keyDef = VKeyboard_GetKeyAt(vr.offhandCursorX, vr.offhandCursorY);
	if (!keyDef) {
		if (VKeyboard_IsInKeyboardArea(vr.offhandCursorX, vr.offhandCursorY)) {
			return;		// near miss
		}
		VKeyboard_DismissWithConsole();
		return;
	}

	VKeyboard_ProcessKeyPress(keyDef, VKB_HAND_OFFHAND);
}
