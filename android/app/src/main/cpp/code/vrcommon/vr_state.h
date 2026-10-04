#ifndef TRINITY_VR_STATE_H
#define TRINITY_VR_STATE_H

#include "vr_shared.h"

#define VR_WRITER_CGAME 0
#define VR_WRITER_GAME 1
#define VR_WRITER_UI 2

void VR_SharedSyncIn( vr_shared_t *state, int structSize );
void VR_SharedSyncOut( const vr_shared_t *state, int writer, int structSize );
void VR_SharedModuleUnloaded( int writer );

#endif
