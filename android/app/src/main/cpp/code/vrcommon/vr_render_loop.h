#ifndef __VR_RENDER_LOOP
#define __VR_RENDER_LOOP

#include "../qcommon/q_shared.h"
#include "vr_types.h"

XrFrameState VR_WaitFrame(XrSession session);
void VR_BeginFrame(XrSession session);
XrViewState VR_LocateViews(XrSession session, XrTime predictedDisplayTime, XrSpace space, XrView* views, uint32_t* viewCount);
void VR_EndFrame(XrSession session, VR_SwapchainInfos* swapchains, XrView* views, uint32_t viewCount, XrSpace worldSpace, XrSpace viewSpace, XrTime predictedDisplayTime);
// The color swapchain's image as a projection layer, posed where that image was drawn from
qboolean VR_BuildProjectionLayer(VR_SwapchainInfos* swapchains, const XrView* views, uint32_t viewCount, XrSpace worldSpace,
	XrCompositionLayerProjection* layer, XrCompositionLayerProjectionView elements[2]);

#endif
