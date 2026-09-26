/*
 * vr_vk_loading.c - keeps the headset fed while a map load blocks the main thread
 *
 * A map load runs inside one Com_Frame for seconds. VR_Loading_Pump ends the frame in
 * progress re-submitting the color swapchain's last released image, posed where it was drawn
 * from, and begins the next. Main thread only: the runtime may use the app's VkQueue inside
 * xrBeginFrame and xrEndFrame, which must not overlap the renderer's submissions.
 */
#include "vr_vk_loading.h"

#include <time.h>

#include "vr_vk_types.h"
#include "vr_vk_swapchains.h"
#include "../vrcommon/vr_base.h"
#include "../vrcommon/vr_render_loop.h"
#include "../qcommon/qcommon.h"

// Frame state owned by vr_vk_renderer.c
extern XrTime lastPredictedDisplayTime;
extern qboolean frameStarted;
extern XrView imageViews[2];
extern uint32_t imageViewCount;
void VR_Renderer_ReleaseImages( VR_SwapchainInfos *swapchains );

// Frames the pump submits are at most this far apart
#define PUMP_INTERVAL_MS 25

static int loadingActive;
static int64_t lastFrameMs;
static int pumpFailures;


static int64_t NowMs( void )
{
	struct timespec ts;
	clock_gettime( CLOCK_MONOTONIC, &ts );
	return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}


void VR_Loading_NoteMainFrame( void )
{
	lastFrameMs = NowMs();
}


qboolean VR_Loading_Active( void )
{
	return loadingActive ? qtrue : qfalse;
}


static void ReportFailure( const char *what, XrResult result )
{
	if ( pumpFailures++ < 3 )
	{
		Com_Printf( "Loading pump: %s failed (%d)\n", what, (int)result );
	}
}


// The last released image, with the poses it was drawn from; nothing until there is one
static XrResult SubmitLoadingLayers( VR_Engine *engine, XrTime displayTime )
{
	XrCompositionLayerProjectionView elements[2];
	XrCompositionLayerProjection projection;
	const XrCompositionLayerBaseHeader *layers[1];
	uint32_t layerCount = 0;
	XrFrameEndInfo endInfo = { XR_TYPE_FRAME_END_INFO, NULL };
	VR_SwapchainInfos *swapchains = engine->appState.Renderer.Swapchains;

	if ( swapchains && swapchains->color.everReleased &&
		VR_BuildProjectionLayer( swapchains, imageViews, imageViewCount, engine->appState.CurrentSpace, &projection, elements ) )
	{
		layers[layerCount++] = (const XrCompositionLayerBaseHeader *)&projection;
	}

	endInfo.displayTime = displayTime;
	endInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
	endInfo.layerCount = layerCount;
	endInfo.layers = layerCount ? layers : NULL;
	return xrEndFrame( engine->appState.Session, &endInfo );
}


// One frame: end the main thread's frame and begin a fresh one for it, or run a whole frame if none is in progress
static void PumpFrame( VR_Engine *engine )
{
	XrSession session = engine->appState.Session;
	XrFrameWaitInfo waitInfo = { XR_TYPE_FRAME_WAIT_INFO, NULL };
	XrFrameState frameState = { XR_TYPE_FRAME_STATE, NULL };
	XrFrameBeginInfo beginInfo = { XR_TYPE_FRAME_BEGIN_INFO, NULL };
	XrResult result;

	if ( frameStarted )
	{
		result = SubmitLoadingLayers( engine, lastPredictedDisplayTime );
		if ( XR_FAILED( result ) )
		{
			ReportFailure( "xrEndFrame", result );
		}
		result = xrWaitFrame( session, &waitInfo, &frameState );
		if ( XR_SUCCEEDED( result ) )
		{
			lastPredictedDisplayTime = frameState.predictedDisplayTime;
			result = xrBeginFrame( session, &beginInfo );
		}
		if ( XR_FAILED( result ) )
		{
			// Nothing is begun for the main thread to end: drop its frame, images and all
			VR_SwapchainInfos *swapchains = engine->appState.Renderer.Swapchains;
			ReportFailure( "xrWaitFrame/xrBeginFrame", result );
			if ( swapchains && ( swapchains->color.acquired || swapchains->depth.acquired ) )
			{
				VR_Renderer_ReleaseImages( swapchains );
			}
			frameStarted = qfalse;
		}
		return;
	}

	result = xrWaitFrame( session, &waitInfo, &frameState );
	if ( XR_FAILED( result ) )
	{
		ReportFailure( "xrWaitFrame", result );
		return;
	}
	result = xrBeginFrame( session, &beginInfo );
	if ( XR_FAILED( result ) )
	{
		ReportFailure( "xrBeginFrame", result );
		return;
	}
	result = SubmitLoadingLayers( engine, frameState.predictedDisplayTime );
	if ( XR_FAILED( result ) )
	{
		ReportFailure( "xrEndFrame", result );
	}
}


void VR_Loading_Pump( VR_Engine *engine )
{
	if ( !loadingActive || !engine || !engine->appState.Renderer.Swapchains ||
		engine->appState.SessionActive != VR_TRUE )
	{
		return;
	}
	if ( NowMs() - lastFrameMs < PUMP_INTERVAL_MS )
	{
		return;
	}
	PumpFrame( engine );
	lastFrameMs = NowMs();
}


void VR_Loading_Begin( VR_Engine *engine )
{
	if ( !engine || !engine->appState.Renderer.Swapchains )
		return;
	if ( !loadingActive )
	{
		loadingActive = 1;
		pumpFailures = 0;
		lastFrameMs = NowMs();
	}
}


void VR_Loading_Stop( void )
{
	loadingActive = 0;
}


void VR_Loading_Shutdown( void )
{
	VR_Loading_Stop();
}
