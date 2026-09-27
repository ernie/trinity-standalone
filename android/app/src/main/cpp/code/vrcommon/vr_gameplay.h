#ifndef __VR_GAMEPLAY
#define __VR_GAMEPLAY

#include "../qcommon/q_shared.h"

void VR_Gameplay_OpenMenuAndPauseIfPossible( void );
qboolean VR_IsFollowingInFirstPerson( void );
qboolean VR_IsInMenu( void );
qboolean VR_IsInConsole( void );
qboolean VR_Gameplay_ShouldRenderInVirtualScreen( void );
qboolean VR_Gameplay_VirtualScreenContextChanged( void );
qboolean VR_IsSPIntermission( void );
qboolean VR_ShouldDisableStereo( void );

// The head-locked scope quad: meters from the head, and half-width per meter (the Quest 3
// buffer aspect times its 1 m half-height, where the scope art and HUD were tuned)
#define VR_SCOPE_QUAD_DISTANCE 1.0f
#define VR_SCOPE_HALF_TAN_H 0.935f
// The scope circle's diameter over the buffer width, as cgame's CG_DrawWeapReticle masks it (indentX 0.16)
#define VR_SCOPE_CIRCLE_WIDTH 0.68f
void VR_ScopeFrustum( float *halfTanH, float *halfTanV, int width, int height );

#endif
