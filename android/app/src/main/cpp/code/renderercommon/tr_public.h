/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
#ifndef __TR_PUBLIC_H
#define __TR_PUBLIC_H

#include "tr_types.h"

#define	REF_API_VERSION		9

//
// Shutdown codes for renderer shutdown
//
typedef enum {
	REF_KEEP_CONTEXT,    // don't destroy window/context (vid_restart)
	REF_KEEP_WINDOW,     // destroy context, keep window
	REF_DESTROY_WINDOW,  // destroy window and context
	REF_UNLOAD_DLL       // full cleanup for DLL unload
} refShutdownCode_t;

//
// these are the functions exported by the refresh module
//
typedef struct {
	// called before the library is unloaded
	// REF_KEEP_CONTEXT: reconfiguring, keep window and context
	// REF_DESTROY_WINDOW or higher: full shutdown
	void	(*Shutdown)( refShutdownCode_t code );

	// All data that will be used in a level should be
	// registered before rendering any frames to prevent disk hits,
	// but they can still be registered at a later time
	// if necessary.
	//
	// BeginRegistration makes any existing media pointers invalid
	// and returns the current gl configuration, including screen width
	// and height, which can be used by the client to intelligently
	// size display elements
	void	(*BeginRegistration)( glconfig_t *config );
	qhandle_t (*RegisterModel)( const char *name );
	qhandle_t (*RegisterSkin)( const char *name );
	qhandle_t (*RegisterShader)( const char *name );
	qhandle_t (*RegisterShaderNoMip)( const char *name );
	void	(*LoadWorld)( const char *name );

	// the vis data is a large enough block of data that we go to the trouble
	// of sharing it with the clipmodel subsystem
	void	(*SetWorldVisData)( const byte *vis );

	// EndRegistration will draw a tiny polygon with each texture, forcing
	// them to be loaded into card memory
	void	(*EndRegistration)( void );

	// a scene is built up by calls to R_ClearScene and the various R_Add functions.
	// Nothing is drawn until R_RenderScene is called.
	void	(*ClearScene)( void );
	void	(*AddRefEntityToScene)( const refEntity_t *re );
	void	(*AddPolyToScene)( qhandle_t hShader , int numVerts, const polyVert_t *verts, int num );
	int		(*LightForPoint)( vec3_t point, vec3_t ambientLight, vec3_t directedLight, vec3_t lightDir );
	void	(*AddLightToScene)( const vec3_t org, float intensity, float r, float g, float b );
	void	(*AddAdditiveLightToScene)( const vec3_t org, float intensity, float r, float g, float b );
	void	(*AddLinearLightToScene)( const vec3_t start, const vec3_t end, float intensity, float r, float g, float b );
	void	(*RenderScene)( const refdef_t *fd );
	void	(*HUDBufferStart)( qboolean clear );
	void	(*HUDBufferEnd)( void );
	void	(*SceneComplete)( void );

	void	(*SetColor)( const float *rgba );	// NULL = 1,1,1,1
	void	(*DrawStretchPic) ( float x, float y, float w, float h, 
		float s1, float t1, float s2, float t2, qhandle_t hShader );	// 0 = white

	// Draw images for cinematic rendering, pass as 32 bit rgba
	void	(*DrawStretchRaw) (int x, int y, int w, int h, int cols, int rows, const byte *data, int client, qboolean dirty);
	void	(*UploadCinematic) (int w, int h, int cols, int rows, const byte *data, int client, qboolean dirty);

	void	(*BeginFrame)( stereoFrame_t stereoFrame );

	// if the pointers are not NULL, timing info will be returned
	void	(*EndFrame)( int *frontEndMsec, int *backEndMsec );

	// renderervk features
	void	(*SetColorMappings)( void );
	void	(*ThrottleBackend)( void );
	qboolean (*CanMinimize)( void );
	void	(*GetConfig)( glconfig_t *config );
	qboolean (*VertexLighting)( void );
	void	(*SyncRender)( void );

	void	(*SetVRHeadsetParms)( const float projectionMatrix[16], const float nonVRProjectionMatrix[16], int renderBuffer,
								  const float projectionEye0[16], const float projectionEye1[16],
								  float combinedFovX, float fovUp, float fovDown, float halfIpdMeters );

	// VR framebuffer operations: called from vrcommon, implemented by each renderer
	qboolean (*InitXRResources)( void );  // Initialize XR resources after swapchains created (Vulkan)
	// Authored density map. Centers are per-eye ndc; fovTan is each eye's frustum as
	// {tanLeft, tanRight, tanUp, tanDown}, which is what turns a texel into an eccentricity
	void (*SetFoveation)( int level, qboolean eyeTracked, const float centers[2][2], const float fovTan[2][4] );
	void	(*BeginXRFrame)( uint32_t colorIndex, uint32_t depthIndex );  // Begin XR frame with swapchain indices (Vulkan)
	void	(*ClearVRFramebuffer)( int width, int height, qboolean isThirdPersonSpectator );
	void	(*WaitForRenderComplete)( void );  // Wait for GPU to finish current frame (Vulkan needs explicit sync)

	int		(*MarkFragments)( int numPoints, const vec3_t *points, const vec3_t projection,
				   int maxPoints, vec3_t pointBuffer, int maxFragments, markFragment_t *fragmentBuffer );

	int		(*LerpTag)( orientation_t *tag,  qhandle_t model, int startFrame, int endFrame, 
					 float frac, const char *tagName );
	void	(*ModelBounds)( qhandle_t model, vec3_t mins, vec3_t maxs );

#ifdef __USEA3D
	void    (*A3D_RenderGeometry) (void *pVoidA3D, void *pVoidGeom, void *pVoidMat, void *pVoidGeomStatus);
#endif
	void	(*RegisterFont)(const char *fontName, int pointSize, fontInfo_t *font);
	void	(*RemapShader)(const char *oldShader, const char *newShader, const char *offsetTime);
	qboolean (*GetEntityToken)( char *buffer, int size );
	qboolean (*inPVS)( const vec3_t p1, const vec3_t p2 );

	void (*TakeVideoFrame)( int h, int w, byte* captureBuffer, byte *encodeBuffer, qboolean motionJpeg );

	// enhanced blood decals: radial projected decal onto nearby world surfaces
	void	(*AddSpritePolyToScene)( qhandle_t hShader, const vec3_t origin, float width, float height, float rotation, const byte *rgba );
	// polys with view gating: RF_FIRST_PERSON / RF_THIRD_PERSON honored the
	// way refents honor them, so first-person effect polys stay out of
	// mirrors and mirror-only ones out of the main view
	void	(*AddPolysToScene2)( qhandle_t hShader, int numVerts, const polyVert_t *verts, int num, int renderfx );
	void	(*ProjectDecal)( const vec3_t origin, float size, float reach, float orientation,
				qhandle_t hShader, const float rgba[4], int lifeTime );
	void	(*ClearDecals)( void );
} refexport_t;

//
// these are the functions imported by the refresh module
//
typedef struct {
	// print message on the local console
	void	(QDECL *Printf)( int printLevel, const char *fmt, ...) Q_PRINTF_FUNC(2, 3);

	// abort the game
	void	(QDECL *Error)( int errorLevel, const char *fmt, ...) Q_NO_RETURN Q_PRINTF_FUNC(2, 3);

	// milliseconds should only be used for profiling, never
	// for anything game related.  Get time from the refdef
	int		(*Milliseconds)( void );

	// stack based memory allocation for per-level things that
	// won't be freed
#ifdef HUNK_DEBUG
	void	*(*Hunk_AllocDebug)( int size, ha_pref pref, char *label, char *file, int line );
#else
	void	*(*Hunk_Alloc)( int size, ha_pref pref );
#endif
	void	*(*Hunk_AllocateTempMemory)( int size );
	void	(*Hunk_FreeTempMemory)( void *block );

	// dynamic memory allocator for things that need to be freed
	void	*(*Malloc)( int bytes );
	void	(*Free)( void *buf );

	cvar_t	*(*Cvar_Get)( const char *name, const char *value, int flags );
	void	(*Cvar_Set)( const char *name, const char *value );
	void	(*Cvar_SetValue) (const char *name, float value);
	void	(*Cvar_CheckRange)( cvar_t *cv, float minVal, float maxVal, qboolean shouldBeIntegral );
	void	(*Cvar_SetDescription)( cvar_t *cv, const char *description );

	int		(*Cvar_VariableIntegerValue) (const char *var_name);
	char	*(*Cvar_VariableString) (const char *var_name);
	void	(*Cvar_VariableStringBuffer) (const char *var_name, char *buffer, int bufsize);

	void	(*Cvar_SetGroup)( cvar_t *var, cvarGroup_t group );
	int		(*Cvar_CheckGroup)( cvarGroup_t group );
	void	(*Cvar_ResetGroup)( cvarGroup_t group, qboolean resetModifiedFlags );

	void	(*Cmd_AddCommand)( const char *name, void(*cmd)(void) );
	void	(*Cmd_RemoveCommand)( const char *name );

	int		(*Cmd_Argc) (void);
	char	*(*Cmd_Argv) (int i);

	void	(*Cmd_ExecuteText) (int exec_when, const char *text);

	byte	*(*CM_ClusterPVS)(int cluster);

	// collision trace for shadow clipping
	void	(*CM_PointTrace)( trace_t *results, const vec3_t start, const vec3_t end,
						int brushmask );

	// visualization for debugging collision detection
	void	(*CM_DrawDebugSurface)( void (*drawPoly)(int color, int numPoints, float *points) );

	// a -1 return means the file does not exist
	// NULL can be passed for buf to just determine existence
	int		(*FS_FileIsInPAK)( const char *name, int *pCheckSum );
	long		(*FS_ReadFile)( const char *name, void **buf );
	void	(*FS_FreeFile)( void *buf );
	char **	(*FS_ListFiles)( const char *name, const char *extension, int *numfilesfound );
	void	(*FS_FreeFileList)( char **filelist );
	void	(*FS_WriteFile)( const char *qpath, const void *buffer, int size );
	qboolean (*FS_FileExists)( const char *file );

	// cinematic stuff
	void	(*CIN_UploadCinematic)(int handle);
	int		(*CIN_PlayCinematic)( const char *arg0, int xpos, int ypos, int width, int height, int bits);
	e_status (*CIN_RunCinematic) (int handle);

	void	(*CL_WriteAVIVideoFrame)( const byte *buffer, int size );

	// input event handling
	void	(*IN_Init)( void *windowData );
	void	(*IN_Shutdown)( void );
	void	(*IN_Restart)( void );

	// math
	long    (*ftol)(float f);

	// system stuff
	void	(*Sys_SetEnv)( const char *name, const char *value );
	void	(*Sys_GLimpSafeInit)( void );
	void	(*Sys_GLimpInit)( void );
	qboolean (*Sys_LowPhysicalMemory)( void );

	// Time utilities
	int	(*Com_RealTime)( qtime_t *qtime );

	// memory cleanup (Quake3e pattern)
	void	(*FreeAll)( void );

	// OpenGL platform functions
	void	(*GLimp_Init)( glconfig_t *config );
	void	(*GLimp_Shutdown)( qboolean unloadDLL );
	void	(*GLimp_EndFrame)( void );
	void	(*GLimp_InitGamma)( glconfig_t *config );
	void	(*GLimp_SetGamma)( unsigned char red[256], unsigned char green[256], unsigned char blue[256] );
	void*	(*GL_GetProcAddress)( const char *name );
	void	(*GLimp_InitVR)( void );  // Initialize VR session and renderer after graphics init

	// Vulkan platform functions
	void	(*VKimp_Init)( glconfig_t *config );
	void	(*VKimp_Shutdown)( qboolean unloadDLL );
	void*	(*VK_GetInstanceProcAddr)( void *instance, const char *name );
	qboolean (*VK_CreateSurface)( void *instance, void *pSurface );

	// VR Vulkan device accessor (pull model for XR_KHR_vulkan_enable2)
	// Returns const VR_VulkanDeviceInfo* (void* to avoid Vulkan header dependency)
	const void* (*VR_Vulkan_GetDeviceInfo)( void );

	// VR Vulkan swapchain accessor (pull model for XR swapchain initialization)
	// Returns const VR_VulkanSwapchainInfo* (void* to avoid Vulkan header dependency)
	const void* (*VR_Vulkan_GetSwapchainInfo)( void );
} refimport_t;

extern	refimport_t	ri;

// this is the only function actually exported at the linker level
// If the module can't init to a valid rendering state, NULL will be
// returned.
#ifdef USE_RENDERER_DLOPEN
typedef	refexport_t* (QDECL *GetRefAPI_t) (int apiVersion, refimport_t * rimp);
#else
refexport_t*GetRefAPI( int apiVersion, refimport_t *rimp );
#endif

#endif	// __TR_PUBLIC_H
