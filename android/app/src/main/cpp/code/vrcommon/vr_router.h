#ifndef VR_ROUTER_H
#define VR_ROUTER_H
/* Runs VR controller keys through the binding contexts: sample, resolve the stack, dispatch hold events. */
#include "../qcommon/q_shared.h"
#include "vr_bind.h"

void VR_Router_Init( void );
void VR_Router_Reset( void );
/* Bindings menu: hands the next fresh press to the UI as its key code; any keyboard or mouse press cancels. */
void VR_Router_BindCapture( void );
void VR_Router_CancelCapture( void );
void VR_Router_Frame( const clXRHandInput_t hands[2] );
/* The latest sample VR_Router_Frame took, NULL before the first or after a reset. */
const clXRHandInput_t *VR_Router_Hands( void );
/* A menu, text entry or scoreboard owns the pointer. */
qboolean VR_Router_PointerLayer( void );
/* No movement or smooth turn reaches the game while this holds. */
qboolean VR_Router_ModalLayer( void );
/* The VR usercmd: role-stick movement, bound key state, weapon angles and the head pose (the engine's builder). */
void VR_Router_ApplyMove( usercmd_t *cmd );

#endif
