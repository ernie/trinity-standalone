#pragma once

// Disable MSVC warning for anonymous structs/unions (standard extension)
#ifdef _MSC_VER
#pragma warning(disable: 4201)
#endif

#include <vulkan/vulkan.h>
#include "tr_common.h"

#define MAX_SWAPCHAIN_IMAGES 8
#define MIN_SWAPCHAIN_IMAGES_IMM 3
#define MIN_SWAPCHAIN_IMAGES_FIFO   3
#define MIN_SWAPCHAIN_IMAGES_FIFO_0 4
#define MIN_SWAPCHAIN_IMAGES_MAILBOX 3

#define MAX_VK_SAMPLERS 32
#define MAX_VK_PIPELINES ((1024 + 128)*2)

#define VERTEX_BUFFER_SIZE     (4 * 1024 * 1024)  /* by default */
#define VERTEX_BUFFER_SIZE_HI  (8 * 1024 * 1024)

#define STAGING_BUFFER_SIZE    (2 * 1024 * 1024)  /* by default */
#define STAGING_BUFFER_SIZE_HI (24 * 1024 * 1024) /* enough for max.texture size upload with all mip levels at once */

#define IMAGE_CHUNK_SIZE (32 * 1024 * 1024)
#define MAX_IMAGE_CHUNKS 56

#define NUM_COMMAND_BUFFERS 2	// number of command buffers / render semaphores / framebuffer sets

#define USE_REVERSED_DEPTH

//#define USE_UPLOAD_QUEUE

#define VK_NUM_BLOOM_PASSES 4

// HUD buffer dimensions for HUD mode 1 (in-world sprite)
#define HUD_BUFFER_WIDTH  1280
#define HUD_BUFFER_HEIGHT 960

#ifndef _DEBUG
#define USE_DEDICATED_ALLOCATION
#endif
//#define MIN_IMAGE_ALIGN (128*1024)
#define MAX_ATTACHMENTS_IN_POOL (8+VK_NUM_BLOOM_PASSES*2) // depth + msaa + msaa-resolve + depth-resolve + screenmap.msaa + screenmap.resolve + screenmap.depth + bloom_extract + blur pairs

#define VK_DESC_STORAGE      0
#define VK_DESC_UNIFORM      0
#define VK_DESC_TEXTURE0     1
#define VK_DESC_TEXTURE1     2
#define VK_DESC_TEXTURE2     3
#define VK_DESC_FOG_COLLAPSE 4
#define VK_DESC_COUNT        5

#define VK_DESC_TEXTURE_BASE VK_DESC_TEXTURE0
#define VK_DESC_FOG_ONLY     VK_DESC_TEXTURE1
#define VK_DESC_FOG_DLIGHT   VK_DESC_TEXTURE1

// dot.vert push block; fills the 128-byte vertex push range of vk.pipeline_layout_storage
#define FLARE_PROBE_PUSH_FLOATS 32

typedef enum {
	TYPE_COLOR_BLACK,
	TYPE_COLOR_WHITE,
	TYPE_COLOR_GREEN,
	TYPE_COLOR_RED,
	TYPE_FOG_ONLY,
	TYPE_DOT,

	TYPE_SIGNLE_TEXTURE_LIGHTING,
	TYPE_SIGNLE_TEXTURE_LIGHTING_LINEAR,

	TYPE_SIGNLE_TEXTURE_DF,

	TYPE_GENERIC_BEGIN, // start of non-env/env shader pairs
	TYPE_SIGNLE_TEXTURE = TYPE_GENERIC_BEGIN,
	TYPE_SIGNLE_TEXTURE_ENV,

	TYPE_SIGNLE_TEXTURE_IDENTITY,
	TYPE_SIGNLE_TEXTURE_IDENTITY_ENV,

	TYPE_SIGNLE_TEXTURE_FIXED_COLOR,
	TYPE_SIGNLE_TEXTURE_FIXED_COLOR_ENV,

	TYPE_SIGNLE_TEXTURE_ENT_COLOR,
	TYPE_SIGNLE_TEXTURE_ENT_COLOR_ENV,

	TYPE_MULTI_TEXTURE_ADD2_IDENTITY,
	TYPE_MULTI_TEXTURE_ADD2_IDENTITY_ENV,
	TYPE_MULTI_TEXTURE_MUL2_IDENTITY,
	TYPE_MULTI_TEXTURE_MUL2_IDENTITY_ENV,

	TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR,
	TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR_ENV,
	TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR,
	TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR_ENV,

	TYPE_MULTI_TEXTURE_MUL2,
	TYPE_MULTI_TEXTURE_MUL2_ENV,
	TYPE_MULTI_TEXTURE_ADD2_1_1,
	TYPE_MULTI_TEXTURE_ADD2_1_1_ENV,
	TYPE_MULTI_TEXTURE_ADD2,
	TYPE_MULTI_TEXTURE_ADD2_ENV,

	TYPE_MULTI_TEXTURE_MUL3,
	TYPE_MULTI_TEXTURE_MUL3_ENV,
	TYPE_MULTI_TEXTURE_ADD3_1_1,
	TYPE_MULTI_TEXTURE_ADD3_1_1_ENV,
	TYPE_MULTI_TEXTURE_ADD3,
	TYPE_MULTI_TEXTURE_ADD3_ENV,

	TYPE_BLEND2_ADD,
	TYPE_BLEND2_ADD_ENV,
	TYPE_BLEND2_MUL,
	TYPE_BLEND2_MUL_ENV,
	TYPE_BLEND2_ALPHA,
	TYPE_BLEND2_ALPHA_ENV,
	TYPE_BLEND2_ONE_MINUS_ALPHA,
	TYPE_BLEND2_ONE_MINUS_ALPHA_ENV,
	TYPE_BLEND2_MIX_ALPHA,
	TYPE_BLEND2_MIX_ALPHA_ENV,

	TYPE_BLEND2_MIX_ONE_MINUS_ALPHA,
	TYPE_BLEND2_MIX_ONE_MINUS_ALPHA_ENV,

	TYPE_BLEND2_DST_COLOR_SRC_ALPHA,
	TYPE_BLEND2_DST_COLOR_SRC_ALPHA_ENV,

	TYPE_BLEND3_ADD,
	TYPE_BLEND3_ADD_ENV,
	TYPE_BLEND3_MUL,
	TYPE_BLEND3_MUL_ENV,
	TYPE_BLEND3_ALPHA,
	TYPE_BLEND3_ALPHA_ENV,
	TYPE_BLEND3_ONE_MINUS_ALPHA,
	TYPE_BLEND3_ONE_MINUS_ALPHA_ENV,
	TYPE_BLEND3_MIX_ALPHA,
	TYPE_BLEND3_MIX_ALPHA_ENV,
	TYPE_BLEND3_MIX_ONE_MINUS_ALPHA,
	TYPE_BLEND3_MIX_ONE_MINUS_ALPHA_ENV,

	TYPE_BLEND3_DST_COLOR_SRC_ALPHA,
	TYPE_BLEND3_DST_COLOR_SRC_ALPHA_ENV,

	TYPE_GENERIC_END = TYPE_BLEND3_MIX_ONE_MINUS_ALPHA_ENV

} Vk_Shader_Type;

// used with cg_shadows == 2
typedef enum {
	SHADOW_DISABLED,
	SHADOW_EDGES,
	SHADOW_FS_QUAD,
} Vk_Shadow_Phase;

typedef enum {
	TRIANGLE_LIST = 0,
	TRIANGLE_STRIP,
	LINE_LIST,
	POINT_LIST
} Vk_Primitive_Topology;

typedef enum {
	DEPTH_RANGE_NORMAL,		// [0..1]
	DEPTH_RANGE_ZERO,		// [0..0]
	DEPTH_RANGE_ONE,		// [1..1]
	DEPTH_RANGE_WEAPON,		// [0..0.3]
	DEPTH_RANGE_COUNT
}  Vk_Depth_Range;

typedef struct {
	VkSamplerAddressMode address_mode; // clamp/repeat texture addressing mode
	int gl_mag_filter;		// GL_XXX mag filter
	int gl_min_filter;		// GL_XXX min filter
	qboolean max_lod_1_0;	// fixed 1.0 lod
	qboolean noAnisotropy;
} Vk_Sampler_Def;

typedef enum {
	RENDER_PASS_MAIN = 0,
	RENDER_PASS_SCREENMAP,
	RENDER_PASS_POST_BLOOM,
	RENDER_PASS_HUD,            // HUD buffer (1280x960) for HUD mode 1 sprite
	RENDER_PASS_MAIN_WITH_POST, // the FBO scene pass (fov_scene), drawn alone before the post-scene and output passes
	RENDER_PASS_POST_SCENE,     // post-scene pass over the stored scene image: coronas, HUD and 2D before gamma
	RENDER_PASS_COUNT
} renderPass_t;

typedef struct {
	Vk_Shader_Type shader_type;
	unsigned int state_bits; // GLS_XXX flags
	cullType_t face_culling;
	qboolean polygon_offset;
	qboolean mirror;
	Vk_Shadow_Phase shadow_phase;
	Vk_Primitive_Topology primitives;
	int line_width;
	int fog_stage; // off, fog-in / fog-out
	int abs_light;
	int allow_discard;
	int hud_coverage; // 0: material alpha, 1: opaque coverage, 2: preserve coverage
	int scene_alpha; // scene image alpha: 0 stays 1, 1 written for a later stage that reads it, 2 written back to 1
	int acff; // none, rgb, rgba, alpha
	int stencil_mark; // mark pixels with stencil bit 0x80 (for shadow exclusion)
	struct {
		byte rgb;
		byte alpha;
	} color;
} Vk_Pipeline_Def;

typedef struct VK_Pipeline {
	Vk_Pipeline_Def def;
	VkPipeline handle[ RENDER_PASS_COUNT ];
} VK_Pipeline_t;

// this structure must be in sync with shader uniforms!
typedef struct vkUniform_s {
	// light/env parameters:
	vec4_t eyePos;				// vertex
	union {
		struct {
			vec4_t pos;			// vertex: light origin
			vec4_t color;		// fragment: rgb + 1/(r*r)
			vec4_t vector;		// fragment: linear dynamic light
		} light;
		struct {
			vec4_t color[3];	// ent.color[3]
		} ent;
	};
	// fog parameters:
	vec4_t fogDistanceVector;	// vertex
	vec4_t fogDepthVector;		// vertex
	vec4_t fogEyeT;				// vertex
	vec4_t fogColor;			// fragment
} vkUniform_t;

#define TESS_XYZ   (1)
#define TESS_RGBA0 (2)
#define TESS_RGBA1 (4)
#define TESS_RGBA2 (8)
#define TESS_ST0   (16)
#define TESS_ST1   (32)
#define TESS_ST2   (64)
#define TESS_NNN   (128)
#define TESS_VPOS  (256)  // uniform with eyePos
#define TESS_ENV   (512)  // mark shader stage with environment mapping
#define TESS_ENT0  (1024) // uniform with ent.color[0]
#define TESS_ENT1  (2048) // uniform with ent.color[1]
#define TESS_ENT2  (4096) // uniform with ent.color[2]
//
// Initialization.
//

// Initializes VK_Instance structure.
// After calling this function we get fully functional vulkan subsystem.
void vk_initialize( void );

// Called after initialization or renderer restart
void vk_init_descriptors( void );

// Shutdown vulkan subsystem by releasing resources acquired by Vk_Instance.
void vk_shutdown( refShutdownCode_t code );

// Releases vulkan resources allocated during program execution.
// This effectively puts vulkan subsystem into initial state (the state we have after vk_initialize call).
void vk_release_resources( void );

void vk_wait_idle( void );
void vk_queue_wait_idle( void );

//
// Resources allocation.
//
void vk_create_image( image_t *image, int width, int height, int mip_levels );
void vk_upload_image_data( image_t *image, int x, int y, int width, int height, int miplevels, byte *pixels, int size, qboolean update );
void vk_update_descriptor_set( image_t *image, qboolean mipmap );
void vk_destroy_image_resources( VkImage *image, VkImageView *imageView );
void vk_update_attachment_descriptors( void );
void vk_destroy_samplers( void );

uint32_t vk_find_pipeline_ext( uint32_t base, const Vk_Pipeline_Def *def, qboolean use );
void vk_get_pipeline_def( uint32_t pipeline, Vk_Pipeline_Def *def );

void vk_create_post_process_pipelines( void );
void vk_create_pipelines( void );

//
// Rendering setup.
//

void vk_clear_color( const vec4_t color );
void vk_clear_depth( qboolean clear_stencil );

// Frame functions
void vk_begin_frame( uint32_t colorIndex, uint32_t depthIndex );
void vk_end_frame( void );
void vk_finish_frame( void );  // Force-end an interrupted frame (for shutdown)
void vk_discard_frame( void ); // End an interrupted frame without submitting it (shutdown)

void vk_end_render_pass( void );
void vk_begin_main_render_pass( void );
void vk_begin_hud_render_pass( qboolean clear );
void vk_end_hud_render_pass( void );
void vk_finish_subpass_post( void );
void vk_end_post_scene_pass( void );
qboolean vk_create_hud_buffer( void );
void vk_shutdown_xr_resources( void );  // Cleanup XR-related resources

void vk_bind_pipeline( uint32_t pipeline );
void vk_bind_index( void );
void vk_bind_index_ext( const int numIndexes, const uint32_t*indexes );
void vk_bind_geometry( uint32_t flags );
void vk_bind_lighting( int stage, int bundle );
void vk_draw_geometry( Vk_Depth_Range depth_range, qboolean indexed );
// Flare probe quad from a FLARE_PROBE_PUSH_FLOATS push block; an untested draw counts every fragment
void vk_draw_flare_probe( uint32_t storage_offset, const float *push, qboolean depthTested );

void vk_read_pixels( byte* buffer, uint32_t width, uint32_t height ); // screenshots

qboolean vk_init_xr_resources( void );  // Initialize XR swapchain resources

// Foveated rendering: the density map the renderer writes itself
void vk_destroy_authored_fdm( void );
void vk_update_authored_fdm( void );
void vk_set_foveation( int level, qboolean eyeTracked, const float centers[2][2], const float fovTan[2][4] );
// Fragment edge in pixels (1..16) the frame's density map asks for at an ndc position in one eye; 1 when not foveating
int vk_foveation_block_at( int eye, float ndcX, float ndcY );
// Keep what a draw covers at full density while it lands inside a fixed map's pass: NDC bounds in one eye,
// the current view's viewport, or the bounds of everything an overlay HUD bracket draws in 2D
void vk_foveation_keep_sharp( int eye, const float rect[4] );
void vk_foveation_keep_sharp_view( void );
void vk_foveation_hud_begin( void );
void vk_foveation_keep_sharp_hud( float x, float y, float w, float h );
void vk_foveation_hud_end( void );

qboolean vk_alloc_vbo( const byte *vbo_data, int vbo_size );
void vk_update_mvp( const float *m );

extern float vk_view_eyeproj[2][16];
void vk_set_view_eyeproj( void );
uint32_t VK_PushEyeProj( void );

uint32_t vk_tess_index( uint32_t numIndexes, const void *src );
void vk_bind_index_buffer( VkBuffer buffer, uint32_t offset );
#ifdef USE_VBO
void vk_draw_indexed( uint32_t indexCount, uint32_t firstIndex );
#endif
void vk_reset_descriptor( int index );
void vk_update_descriptor( int index, VkDescriptorSet descriptor );
void vk_update_descriptor_offset( int index, uint32_t offset );

void vk_update_post_process_pipelines( void );

const char *vk_format_string( VkFormat format );

void VBO_PrepareQueues( void );
void VBO_RenderIBOItems( void );
void VBO_ClearQueue( void );

typedef struct vk_tess_s {
	VkCommandBuffer command_buffer;
	VkCommandBuffer hud_command_buffer;  // the HUD buffer's own, submitted ahead of command_buffer
	qboolean		hud_begun;

	VkSemaphore image_acquired;
	uint32_t	swapchain_image_index;
	qboolean	swapchain_image_acquired;
#ifdef USE_UPLOAD_QUEUE
	VkSemaphore rendering_finished2;
#endif
	VkFence rendering_finished_fence;
	qboolean waitForFence;

	VkBuffer vertex_buffer;
	byte *vertex_buffer_ptr; // pointer to mapped vertex buffer
	uint32_t vertex_buffer_offset; // VkDeviceSize

	VkDescriptorSet uniform_descriptor;
	uint32_t		uniform_read_offset;
	uint32_t		eyeproj_offset;	// dynamic offset for set 0 binding 1 (per-view eyeProj)
	float			eyeproj_cache[32];		// ring content at eyeproj_offset, to skip redundant pushes
	qboolean		eyeproj_cache_valid;	// per-frame: reset at begin_frame and on ring resize
	VkDeviceSize	buf_offset[8];
	VkDeviceSize	vbo_offset[8];

	VkBuffer		curr_index_buffer;
	uint32_t		curr_index_offset;

	struct {
		uint32_t		start, end;
		VkDescriptorSet	current[5]; // 0:uniform, 1:color0, 2:color1, 3:color2, 4:fog
		uint32_t		offset[1]; // 0 (uniform)
	} descriptor_set;

	Vk_Depth_Range		depth_range;
	VkPipeline			last_pipeline;

	uint32_t num_indexes; // value from most recent vk_bind_index() call

	VkRect2D scissor_rect;
} vk_tess_t;


// Forward declaration for VR layer swapchain info (typedef in vr_vk_types.h)
struct VR_VK_SwapchainInfo_s;


// What a density map was drawn from, less the gaze and the HUD carve
typedef struct {
	int level;
	qboolean eyeTracked, scope;
	float fovTan[2][4];
	uint32_t tileWidth, tileHeight;
} vkFdmInputs_t;

// XR-specific resources that don't have Quake3e equivalents
// VR layer provides XrSwapchain handles and VkImages via VR_VK_SwapchainInfo.
// Renderer creates and owns VkImageViews, VkFramebuffers for XR swapchains.
// Note: FBO intermediate buffers (color_image, bloom_image[], etc.) use
// the existing Quake3e fields in Vk_Instance, made multiview-compatible.
typedef struct {
	// Pointers to VR layer swapchain info (for accessing VkImages)
	const struct VR_VK_SwapchainInfo_s* colorInfo;
	const struct VR_VK_SwapchainInfo_s* depthInfo;

	// VkImageViews for XR swapchain images (created by renderer)
	VkImageView colorViews[MAX_SWAPCHAIN_IMAGES];    // Multiview array views (sRGB format)
	VkImageView gammaViews[MAX_SWAPCHAIN_IMAGES];    // UNORM views for gamma pass
	VkImageView depthViews[MAX_SWAPCHAIN_IMAGES];    // Multiview array views

	// Direct XR swapchain framebuffers (used when FBO is NOT active)
	VkFramebuffer framebuffers[MAX_SWAPCHAIN_IMAGES];

	// HUD buffer (1280x960, single layer) for HUD mode 1 sprite
	VkImage hudImage;
	VkImageView hudView;           // 2D_ARRAY view for framebuffer
	VkImageView hudSamplerView;    // 2D view for descriptor/sampling
	VkFramebuffer hudFramebuffer;
	VkDescriptorSet hudDescriptor;
	VkDeviceMemory hudMemory;

	// HUD depth buffer for proper 3D model rendering (e.g., character heads)
	VkImage hudDepthImage;
	VkImageView hudDepthView;
	VkDeviceMemory hudDepthMemory;

	// Current XR swapchain state
	uint32_t colorIndex;
	uint32_t depthIndex;

	// XR resolution
	uint32_t width;
	uint32_t height;

	// Runtime density maps (XR_FB_foveation_vulkan), one per color image, attached to the scene passes when foveationActive
	qboolean fdmSupported;          // device feature enabled by the VR layer
	qboolean tileProperties;        // VK_QCOM_tile_properties enabled, so the bin size can be read back
	uint32_t tileWidth;             // bin the tiler chose for the scene pass, 0 while unknown
	uint32_t tileHeight;
	qboolean tileAssumed;           // mirrored from Turnip's tiling rather than reported
	qboolean foveationActive;
	VkImageView foveationViews[MAX_SWAPCHAIN_IMAGES];
	uint32_t foveationWidth;
	uint32_t foveationHeight;

	// Our own density map, shared by every swapchain image and rewritten in the frame that changes it
	qboolean fdmAuthored;
	qboolean fdmHostRead;           // the driver reads the map as the pass is recorded (Turnip)
	VkImage fdmImage;
	VkDeviceMemory fdmMemory;
	VkBuffer fdmStaging[NUM_COMMAND_BUFFERS];  // one per frame in flight
	VkDeviceMemory fdmStagingMemory[NUM_COMMAND_BUFFERS];
	void *fdmStagingMapped[NUM_COMMAND_BUFFERS];
	byte *fdmScratch;               // the map as this frame draws it
	byte *fdmCurrent;               // the map the image holds
	qboolean fdmUploaded;
	vkFdmInputs_t fdmDrawn;         // what the held map was drawn from
	uint32_t fdmLayers;
	uint32_t fdmTexelWidth;
	uint32_t fdmTexelHeight;

	// What the map should describe, pushed in by the VR layer each frame
	int fdmLevel;                   // VR_FOVEATION_OFF..HIGH, or EYE_TRACKED
	qboolean fdmEyeTracked;
	qboolean fdmScope;              // the scope's view, masked outside its circle
	float fdmCenter[2][2];          // per eye, normalized device coordinates
	float fdmFovTan[2][4];          // per eye frustum: tangents of left, right, up, down

	// Offset mode: the map holds still around a reference point and the scene pass ends with the
	// gaze's offset from it, so the tiler slides its bins with the eye
	qboolean fdmOffsetSupported;    // device enabled the offsets and render pass 2
	qboolean fdmOffsets;            // this map is in offset mode
	uint32_t fdmOffsetGranularity[2];
	int32_t fdmRef[2][2];           // per eye, framebuffer pixel the map's sharp region is drawn around
	int32_t fdmOffset[2][2];        // per eye, the offset the current scene pass ends with
	qboolean mapPassOpen;           // the open pass is the scene pass carrying the density map

	// Masks of map texels per layer, in framebuffer position, under the HUD drawn inside the map's
	// pass: marked this frame, and held from the last for this frame's map
	size_t fdmSharpBytes;
	byte *fdmSharpMarked;
	byte *fdmSharpHeld;

	// Virtual screen: the finished frame's 4:3 crop with a mip chain for the reflection's blur
	VkImage vscreenImage;
	VkDeviceMemory vscreenMemory;
	VkImageView vscreenView;        // UNORM, so sampling returns the encoded bytes
	VkImageView vscreenMip0View;    // UNORM, the capture pass's attachment
	VkSampler vscreenSampler;
	VkSampler vscreenSourceSampler; // nearest: the capture is an exact texel copy
	VkDescriptorSet vscreenDescriptor;  // from the shared pool; reallocated after a pool reset
	VkDescriptorSet vscreenSourceDescriptor[MAX_SWAPCHAIN_IMAGES];  // swapchain gammaViews, same pool rules
	uint32_t vscreenX, vscreenY;    // crop's top left in layer 0; constant, screen frames publish a symmetric FOV
	uint32_t vscreenWidth, vscreenHeight, vscreenMips;
	VkFramebuffer vscreenCaptureFramebuffer;
	VkFramebuffer vscreenFramebuffers[MAX_SWAPCHAIN_IMAGES];
	VkPipeline vscreenCapturePipeline;
	VkPipeline vscreenPipeline;
	VkPipeline vscreenReflectPipeline;
	VkPipeline floorGridPipeline;

	// Initialization state
	qboolean initialized;
} VkXrResources;

// Vk_Instance contains engine-specific vulkan resources that persist entire renderer lifetime.
// This structure is initialized/deinitialized by vk_initialize/vk_shutdown functions correspondingly.
typedef struct {
	VkPhysicalDevice physical_device;
	VkSurfaceFormatKHR base_format;
	VkSurfaceFormatKHR present_format;

	uint32_t queue_family_index;
	VkDevice device;
	VkQueue queue;

	VkSwapchainKHR swapchain;
	uint32_t swapchain_image_count;
	VkImage swapchain_images[MAX_SWAPCHAIN_IMAGES];
	VkSemaphore swapchain_rendering_finished[MAX_SWAPCHAIN_IMAGES];
	//uint32_t swapchain_image_index;

	// Generic linear sampler (used for post-processing, etc.)
	VkSampler linearSampler;

	VkCommandPool command_pool;
#ifdef USE_UPLOAD_QUEUE
	VkCommandBuffer staging_command_buffer;
#endif

	VkDeviceMemory image_memory[ MAX_ATTACHMENTS_IN_POOL ];
	uint32_t image_memory_count;

	struct {
		VkRenderPass main;        // Multiview main rendering (clears framebuffer)
		VkRenderPass screenmap;
		VkRenderPass gamma;       // Multiview gamma correction (if r_fbo)
		VkRenderPass blur[VK_NUM_BLOOM_PASSES*2]; // Multiview blur passes
		VkRenderPass post_bloom;  // Multiview post-bloom blend
		VkRenderPass hudBuffer;       // HUD buffer (1280x960, single layer, color+depth), color loaded
		VkRenderPass hudBufferClear;  // same, color cleared on load: the first HUD pass of a frame
		// Post pass into the swapchain, one subpass: the composite (bloom) or gamma quad over the stored scene
		VkRenderPass main_with_bloom;
		VkRenderPass main_with_gamma;
		// Foveated split (vk.fovSplit): scene alone with the density map; post_scene then draws coronas, HUD and 2D
		// into the stored scene image before gamma
		VkRenderPass fov_scene;
		VkRenderPass post_scene;
		// Virtual screen: clears the swapchain and draws the screen back into it, no density map
		VkRenderPass virtualScreen;
		// Virtual screen capture: single view, samples layer 0's crop into the screen texture's mip 0
		VkRenderPass virtualScreenCapture;
	} render_pass;

	VkDescriptorPool descriptor_pool;
	VkDescriptorSetLayout set_layout_sampler;	// combined image sampler
	VkDescriptorSetLayout set_layout_uniform;	// dynamic uniform buffer
	VkDescriptorSetLayout set_layout_storage;	// feedback buffer
	VkDescriptorSetLayout set_layout_4samplers;			// 4 combined image samplers for bloom blur

	VkPipelineLayout pipeline_layout;			// main shaders (64-byte mono modelview push; per-eye projection in set 0 binding 1)
	VkPipelineLayout pipeline_layout_storage;	// flare test shader layout
	VkPipelineLayout pipeline_layout_post_process;	// post-processing
	VkPipelineLayout pipeline_layout_blend;		// post-processing
	// Post pass: set 0 is a combined image sampler on the stored scene
	VkPipelineLayout pipeline_layout_fov_composite;
	VkPipelineLayout pipeline_layout_fov_gamma;
	VkPipelineLayout pipeline_layout_foveation_debug;  // gaze, bin origin and bin size, pushed

	VkDescriptorSet color_descriptor;

	VkImage color_image;
	VkImageView color_image_view;

	VkImage bloom_image[1+VK_NUM_BLOOM_PASSES*2];
	VkImageView bloom_image_view[1+VK_NUM_BLOOM_PASSES*2];

	VkDescriptorSet bloom_image_descriptor[1+VK_NUM_BLOOM_PASSES*2];
	VkDescriptorSet bloom_blur_combined_descriptor;		// Combined descriptor for all 4 blur results

	VkImage depth_image;
	VkImageView depth_image_view;

	VkImage msaa_image;
	VkImageView msaa_image_view;

	// Subpass optimization: transient images (tile-local post-processing)
	struct {
		// Non-MSAA path: transient scene color and depth
		VkImage scene_image;
		VkImageView scene_view;
		VkDeviceMemory scene_memory;

		VkImage depth_image;
		VkImageView depth_view;
		VkDeviceMemory depth_memory;

		// MSAA path: transient MSAA color, resolve target, and depth
		VkImage msaa_image;
		VkImageView msaa_view;
		VkDeviceMemory msaa_memory;

		VkImage resolve_image;
		VkImageView resolve_view;
		VkDeviceMemory resolve_memory;

		// Combined image sampler on the stored scene
		VkDescriptorSet scene_descriptor;
	} transient;

	// screenMap
	struct {
		VkDescriptorSet color_descriptor;
		VkImage color_image;
		VkImageView color_image_view;

		VkImage color_image_msaa;
		VkImageView color_image_view_msaa;

		VkImage depth_image;
		VkImageView depth_image_view;

	} screenMap;

	struct {
		VkImage image;
		VkImageView image_view;
	} capture;

	struct {
		VkFramebuffer blur[VK_NUM_BLOOM_PASSES*2];
		VkFramebuffer post_bloom;   // For post-bloom blend pass (color-only, no depth)
		VkFramebuffer main;         // FBO mode: single framebuffer for main rendering
		VkFramebuffer gamma[MAX_SWAPCHAIN_IMAGES];
		VkFramebuffer screenmap;
		// Subpass optimization framebuffers (per swapchain image)
		VkFramebuffer main_with_bloom[MAX_SWAPCHAIN_IMAGES];
		VkFramebuffer main_with_gamma[MAX_SWAPCHAIN_IMAGES];
		VkFramebuffer fov_scene[MAX_SWAPCHAIN_IMAGES];  // foveated split: scene pass, per density map
		VkFramebuffer post_scene;  // the stored scene image, shared by every swapchain image
	} framebuffers;

#ifdef USE_UPLOAD_QUEUE
	VkSemaphore rendering_finished;	// reference to vk.cmd->rendering_finished2
	VkSemaphore image_uploaded2;
	VkSemaphore image_uploaded;		// reference to vk.image_uploaded2
#endif

	vk_tess_t tess[ NUM_COMMAND_BUFFERS ], *cmd;
	int cmd_index;

	struct {
		VkBuffer		buffer;
		byte			*buffer_ptr;
		VkDeviceMemory	memory;
		VkDescriptorSet	descriptor;
	} storage;

	uint32_t uniform_item_size;
	uint32_t uniform_alignment;
	uint32_t storage_alignment;

	struct {
		VkBuffer vertex_buffer;
		VkDeviceMemory	buffer_memory;
	} vbo;

	// host visible memory that holds vertex, index and uniform data
	VkDeviceMemory geometry_buffer_memory;
	VkDeviceSize geometry_buffer_size;
	VkDeviceSize geometry_buffer_size_new;

	// statistics
	struct {
		VkDeviceSize vertex_buffer_max;
		uint32_t push_size;
		uint32_t push_size_max;
	} stats;

	//
	// Shader modules.
	// All 3D shaders use multiview for VR stereo rendering.
	//
	struct {
		struct {
			VkShaderModule gen[3][2][2][2]; // tx[0,1,2], cl[0,1] env0[0,1] fog[0,1]
			VkShaderModule ident1[2][2][2]; // tx[0,1], env0[0,1] fog[0,1]
			VkShaderModule fixed[2][2][2];  // tx[0,1], env0[0,1] fog[0,1]
			VkShaderModule light[2];        // fog[0,1]
		} vert;
		struct {
			VkShaderModule gen0_df;
			VkShaderModule gen[3][2][2]; // tx[0,1,2] cl[0,1] fog[0,1]
			VkShaderModule ident1[2][2]; // tx[0,1], fog[0,1]
			VkShaderModule fixed[2][2];  // tx[0,1], fog[0,1]
			VkShaderModule ent[1][2];    // tx[0], fog[0,1]
			VkShaderModule light[2][2];  // linear[0,1] fog[0,1]
		} frag;

		VkShaderModule color_fs;
		VkShaderModule color_vs;  // multiview

		VkShaderModule blur_fs;
		VkShaderModule blur_extract_fs;  // first blur pass of the foveated split, bloom extract folded in
		VkShaderModule blend_fs;

		VkShaderModule gamma_fs;
		VkShaderModule gamma_vs;
		VkShaderModule foveationdebug_fs;  // reuses gamma_vs for its fullscreen quad

		VkShaderModule vscreen_vs;          // virtual screen, reflection and floor share it
		VkShaderModule vscreen_fs;
		VkShaderModule vscreen_reflect_fs;
		VkShaderModule floor_grid_fs;
		VkShaderModule vscreen_capture_vs;
		VkShaderModule vscreen_capture_fs;

		VkShaderModule fog_fs;  // multiview
		VkShaderModule fog_vs;  // multiview

		VkShaderModule dot_fs;      // flare probe counters (dot.frag)
		VkShaderModule dot_vs;      // flare probe patch (dot.vert)

		// Post pass shaders; they sample the stored scene
		VkShaderModule final_composite_fov_fs;
		VkShaderModule gamma_fov_fs;
	} modules;

	VkPipelineCache pipelineCache;

	VK_Pipeline_t pipelines[ MAX_VK_PIPELINES ];
	uint32_t pipelines_count;
	uint32_t pipelines_world_base;

	// pipeline statistics
	int32_t pipeline_create_count;

	//
	// Standard pipelines.
	//
	uint32_t skybox_pipeline;

	// dim 0: 0 - front side, 1 - back size
	// dim 1: 0 - normal view, 1 - mirror view
	uint32_t shadow_volume_pipelines[2][2];
	uint32_t shadow_finish_pipeline;

	// dim 0 is based on fogPass_t: 0 - corresponds to FP_EQUAL, 1 - corresponds to FP_LE.
	// dim 1 is directly a cullType_t enum value.
	// dim 2 is a polygon offset value (0 - off, 1 - on).
	uint32_t fog_pipelines[2][3][2];

	// dim 0 is based on dlight additive flag: 0 - not additive, 1 - additive
	// dim 1 is directly a cullType_t enum value.
	// dim 2 is a polygon offset value (0 - off, 1 - on).
#ifdef USE_LEGACY_DLIGHTS
	uint32_t dlight_pipelines[2][3][2];
#endif

	// cullType[3], polygonOffset[2], fogStage[2], absLight[2]
#ifdef USE_PMLIGHT
	uint32_t dlight_pipelines_x[3][2][2][2];
	uint32_t dlight1_pipelines_x[3][2][2][2];
#endif

	// debug visualization pipelines
	uint32_t tris_debug_pipeline;
	uint32_t tris_mirror_debug_pipeline;
	uint32_t tris_debug_green_pipeline;
	uint32_t tris_mirror_debug_green_pipeline;
	uint32_t tris_debug_red_pipeline;
	uint32_t tris_mirror_debug_red_pipeline;

	uint32_t normals_debug_pipeline;
	uint32_t surface_debug_pipeline_solid;
	uint32_t surface_debug_pipeline_outline;
	uint32_t images_debug_pipeline;
	uint32_t images_debug_pipeline2;
	uint32_t surface_beam_pipeline;
	uint32_t surface_axis_pipeline;
	uint32_t dot_pipeline;        // flare probe, depth tested: counts uncovered fragments
	uint32_t dot_total_pipeline;  // flare probe, no depth test: counts all fragments

	// Post-processing pipelines (multiview)
	VkPipeline blur_pipeline[VK_NUM_BLOOM_PASSES*2];  // Blur passes for bloom

	// Output pass pipelines
	VkPipeline final_composite_subpass_pipeline;  // composite + gamma
	VkPipeline gamma_subpass_pipeline;            // gamma only (no bloom)
	// Built lazily on first r_foveationDebug enable, against whichever pass carries the map
	VkPipeline foveation_debug_pipeline;
	VkRenderPass foveation_debug_pass;

	uint32_t frame_count;
	qboolean active;
	qboolean wideLines;
	qboolean samplerAnisotropy;
	qboolean fragmentStores;
	qboolean dedicatedAllocation;
	qboolean debugMarkers;
	qboolean imageFormatList;      // VK_KHR_image_format_list enabled by the VR layer
	qboolean multiviewSupported;   // VK_KHR_multiview available
	qboolean depthClamp;           // depth clamp for z-fail shadow volumes

	float maxAnisotropy;
	float maxLod;

	VkFormat color_format;
	VkFormat capture_format;
	VkFormat depth_format;
	VkFormat bloom_format;

	VkImageLayout initSwapchainLayout;

	qboolean clearAttachment;		// requires VK_IMAGE_USAGE_TRANSFER_DST_BIT for swapchains
	qboolean fboActive;
	qboolean blitEnabled;
	qboolean msaaActive;

	qboolean offscreenRender;

	qboolean windowAdjusted;
	int		blitX0;
	int		blitY0;
	int		blitFilter;

	uint32_t renderWidth;
	uint32_t renderHeight;

	float renderScaleX;
	float renderScaleY;

	renderPass_t renderPassIndex;
	qboolean inRenderPass;		// true when actually inside a render pass

	// HUD brackets record into vk.cmd->hud_command_buffer, submitted ahead of the frame's, so an open scene pass is never resumed
	qboolean inHudCommandBuffer;
	struct {
		VkCommandBuffer commandBuffer;
		qboolean inRenderPass;
		renderPass_t renderPassIndex;
		uint32_t renderWidth, renderHeight;
		float renderScaleX, renderScaleY;
	} hudSaved;
	qboolean recordingCommands;	// true when command buffer is recording (between Begin/End)
	qboolean descriptorsReady;	// qfalse between vk_release_resources() and vk_init_descriptors(): pool contents are dead

	// Subpass optimization: track when HUD rendering completes the combined pass
	// If HUD rendering is requested while the scene pass is open,
	// the post-scene pass opens first, then the HUD proceeds.
	qboolean subpassPostDone;			// true when vk_finish_subpass_post() already ran this frame

	uint32_t screenMapWidth;
	uint32_t screenMapHeight;
	uint32_t screenMapSamples;

	uint32_t image_chunk_size;

	uint32_t maxBoundDescriptorSets;

#ifdef USE_UPLOAD_QUEUE
	VkFence aux_fence;
	qboolean aux_fence_wait;
#endif

	struct staging_buffer_s {
		VkBuffer handle;
		VkDeviceMemory memory;
		VkDeviceSize size;
		byte *ptr; // pointer to mapped staging buffer
#ifdef USE_UPLOAD_QUEUE
		VkDeviceSize offset;
#endif
	} staging_buffer;

	struct samplers_s {
		int count;
		Vk_Sampler_Def def[MAX_VK_SAMPLERS];
		VkSampler handle[MAX_VK_SAMPLERS];
		int filter_min;
		int filter_max;
	} samplers;

	struct defaults_t {
		VkDeviceSize staging_size;
		VkDeviceSize geometry_size;
	} defaults;

	qboolean xrMode;
	VkInstance xrInstance;

	// XR swapchain resources (renderer-owned views/framebuffers for XR images)
	VkXrResources xr;

} Vk_Instance;

typedef struct {
	VkDeviceMemory memory;
	VkDeviceSize used;
} ImageChunk;

// Vk_World contains vulkan resources/state requested by the game code.
// It is reinitialized on a map change.
typedef struct {
	//
	// Memory allocations.
	//
	int num_image_chunks;
	ImageChunk image_chunks[MAX_IMAGE_CHUNKS];

	//
	// State.
	//

	// Descriptor sets corresponding to bound texture images.
	//VkDescriptorSet current_descriptor_sets[ MAX_TEXTURE_UNITS ];

	// This flag is used to decide whether framebuffer's depth attachment should be cleared
	// with vmCmdClearAttachment (dirty_depth_attachment != 0), or it have just been
	// cleared by render pass instance clear op (dirty_depth_attachment == 0).
	int dirty_depth_attachment;

	float modelview_transform[16];

	// Per-eye projection matrices from OpenXR (set via RE_SetVRHeadsetParms)
	float projectionEye[2][16];
} Vk_World;

extern Vk_Instance	vk;				// shouldn't be cleared during ref re-init

// True while the frame's post-scene pass is open. Both fields are required: a closed pass leaves the index behind.
#define VK_IN_POST_SCENE()	( vk.inRenderPass && vk.renderPassIndex == RENDER_PASS_POST_SCENE )
extern Vk_World		vk_world;		// this data is cleared during ref re-init
