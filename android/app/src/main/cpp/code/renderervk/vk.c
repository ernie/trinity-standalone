#include "tr_local.h"
#include "vk.h"
#include "../vrvk/vr_vk.h"  // For VR_VulkanDeviceInfo (pull model)
#include "../vrcommon/vr_clientinfo.h"
#include "../vrcommon/vr_gameplay.h"  // For VR_ShouldDisableStereo

// VR client state accessible from renderer
extern vr_clientinfo_t vr;

// Forward declarations for subpass post-processing
void vk_finish_subpass_post( void );
void vk_end_post_scene_subpass( void );

#if defined (_DEBUG)
#if defined (__ANDROID__)
#define USE_VK_VALIDATION
#include <android/log.h> // for android debug callback
#endif
#endif

static int vkSamples = VK_SAMPLE_COUNT_1_BIT;
static int vkMaxSamples = VK_SAMPLE_COUNT_1_BIT;

static VkInstance vk_instance = VK_NULL_HANDLE;
static VkSurfaceKHR vk_surface = VK_NULL_HANDLE;

#ifdef USE_VK_VALIDATION
static VkDebugReportCallbackEXT vk_debug_callback = VK_NULL_HANDLE;
#endif

//
// Vulkan API functions used by the renderer.
//
static PFN_vkCreateInstance								qvkCreateInstance;
static PFN_vkEnumerateInstanceExtensionProperties		qvkEnumerateInstanceExtensionProperties;

static PFN_vkCreateDevice								qvkCreateDevice;
static PFN_vkDestroyInstance							qvkDestroyInstance;
static PFN_vkEnumerateDeviceExtensionProperties			qvkEnumerateDeviceExtensionProperties;
static PFN_vkEnumeratePhysicalDevices					qvkEnumeratePhysicalDevices;
static PFN_vkGetDeviceProcAddr							qvkGetDeviceProcAddr;
static PFN_vkGetPhysicalDeviceFeatures					qvkGetPhysicalDeviceFeatures;
static PFN_vkGetPhysicalDeviceFeatures2					qvkGetPhysicalDeviceFeatures2;
static PFN_vkGetPhysicalDeviceFormatProperties			qvkGetPhysicalDeviceFormatProperties;
static PFN_vkGetPhysicalDeviceMemoryProperties			qvkGetPhysicalDeviceMemoryProperties;
static PFN_vkGetPhysicalDeviceProperties				qvkGetPhysicalDeviceProperties;
static PFN_vkGetPhysicalDeviceQueueFamilyProperties		qvkGetPhysicalDeviceQueueFamilyProperties;
static PFN_vkDestroySurfaceKHR							qvkDestroySurfaceKHR;
static PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR	qvkGetPhysicalDeviceSurfaceCapabilitiesKHR;
static PFN_vkGetPhysicalDeviceSurfaceFormatsKHR			qvkGetPhysicalDeviceSurfaceFormatsKHR;
static PFN_vkGetPhysicalDeviceSurfacePresentModesKHR	qvkGetPhysicalDeviceSurfacePresentModesKHR;
static PFN_vkGetPhysicalDeviceSurfaceSupportKHR			qvkGetPhysicalDeviceSurfaceSupportKHR;
#ifdef USE_VK_VALIDATION
static PFN_vkCreateDebugReportCallbackEXT				qvkCreateDebugReportCallbackEXT;
static PFN_vkDestroyDebugReportCallbackEXT				qvkDestroyDebugReportCallbackEXT;
#endif
static PFN_vkAllocateCommandBuffers						qvkAllocateCommandBuffers;
static PFN_vkAllocateDescriptorSets						qvkAllocateDescriptorSets;
static PFN_vkAllocateMemory								qvkAllocateMemory;
static PFN_vkBeginCommandBuffer							qvkBeginCommandBuffer;
static PFN_vkBindBufferMemory							qvkBindBufferMemory;
static PFN_vkBindImageMemory							qvkBindImageMemory;
static PFN_vkCmdBeginRenderPass							qvkCmdBeginRenderPass;
static PFN_vkCmdBindDescriptorSets						qvkCmdBindDescriptorSets;
static PFN_vkCmdBindIndexBuffer							qvkCmdBindIndexBuffer;
static PFN_vkCmdBindPipeline							qvkCmdBindPipeline;
static PFN_vkCmdBindVertexBuffers						qvkCmdBindVertexBuffers;
static PFN_vkCmdBlitImage								qvkCmdBlitImage;
static PFN_vkCmdClearAttachments						qvkCmdClearAttachments;
static PFN_vkCmdClearColorImage							qvkCmdClearColorImage;
static PFN_vkCmdClearDepthStencilImage					qvkCmdClearDepthStencilImage;
static PFN_vkCmdCopyBuffer								qvkCmdCopyBuffer;
static PFN_vkCmdCopyBufferToImage						qvkCmdCopyBufferToImage;
static PFN_vkCmdCopyImage								qvkCmdCopyImage;
static PFN_vkCmdDraw									qvkCmdDraw;
static PFN_vkCmdDrawIndexed								qvkCmdDrawIndexed;
static PFN_vkCmdEndRenderPass							qvkCmdEndRenderPass;
static PFN_vkCmdNextSubpass								qvkCmdNextSubpass;
static PFN_vkCmdPipelineBarrier							qvkCmdPipelineBarrier;
static PFN_vkCmdPushConstants							qvkCmdPushConstants;
static PFN_vkCmdSetDepthBias							qvkCmdSetDepthBias;
static PFN_vkCmdSetScissor								qvkCmdSetScissor;
static PFN_vkCmdSetViewport								qvkCmdSetViewport;
static PFN_vkCreateBuffer								qvkCreateBuffer;
static PFN_vkCreateCommandPool							qvkCreateCommandPool;
static PFN_vkCreateDescriptorPool						qvkCreateDescriptorPool;
static PFN_vkCreateDescriptorSetLayout					qvkCreateDescriptorSetLayout;
static PFN_vkCreateFence								qvkCreateFence;
static PFN_vkCreateFramebuffer							qvkCreateFramebuffer;
static PFN_vkCreateGraphicsPipelines					qvkCreateGraphicsPipelines;
static PFN_vkCreateImage								qvkCreateImage;
static PFN_vkCreateImageView							qvkCreateImageView;
static PFN_vkCreatePipelineLayout						qvkCreatePipelineLayout;
static PFN_vkCreatePipelineCache						qvkCreatePipelineCache;
static PFN_vkCreateRenderPass							qvkCreateRenderPass;
static PFN_vkCreateSampler								qvkCreateSampler;
static PFN_vkCreateSemaphore							qvkCreateSemaphore;
static PFN_vkCreateShaderModule							qvkCreateShaderModule;
static PFN_vkDestroyBuffer								qvkDestroyBuffer;
static PFN_vkDestroyCommandPool							qvkDestroyCommandPool;
static PFN_vkDestroyDescriptorPool						qvkDestroyDescriptorPool;
static PFN_vkDestroyDescriptorSetLayout					qvkDestroyDescriptorSetLayout;
static PFN_vkDestroyDevice								qvkDestroyDevice;
static PFN_vkDestroyFence								qvkDestroyFence;
static PFN_vkDestroyFramebuffer							qvkDestroyFramebuffer;
static PFN_vkDestroyImage								qvkDestroyImage;
static PFN_vkDestroyImageView							qvkDestroyImageView;
static PFN_vkDestroyPipeline							qvkDestroyPipeline;
static PFN_vkDestroyPipelineCache						qvkDestroyPipelineCache;
static PFN_vkDestroyPipelineLayout						qvkDestroyPipelineLayout;
static PFN_vkDestroyRenderPass							qvkDestroyRenderPass;
static PFN_vkDestroySampler								qvkDestroySampler;
static PFN_vkDestroySemaphore							qvkDestroySemaphore;
static PFN_vkDestroyShaderModule						qvkDestroyShaderModule;
static PFN_vkDeviceWaitIdle								qvkDeviceWaitIdle;
static PFN_vkEndCommandBuffer							qvkEndCommandBuffer;
static PFN_vkFlushMappedMemoryRanges					qvkFlushMappedMemoryRanges;
static PFN_vkFreeCommandBuffers							qvkFreeCommandBuffers;
static PFN_vkFreeDescriptorSets							qvkFreeDescriptorSets;
static PFN_vkFreeMemory									qvkFreeMemory;
static PFN_vkGetBufferMemoryRequirements				qvkGetBufferMemoryRequirements;
static PFN_vkGetDeviceQueue								qvkGetDeviceQueue;
static PFN_vkGetImageMemoryRequirements					qvkGetImageMemoryRequirements;
static PFN_vkGetImageSubresourceLayout					qvkGetImageSubresourceLayout;
static PFN_vkInvalidateMappedMemoryRanges				qvkInvalidateMappedMemoryRanges;
static PFN_vkMapMemory									qvkMapMemory;
static PFN_vkQueueSubmit								qvkQueueSubmit;
static PFN_vkQueueWaitIdle								qvkQueueWaitIdle;
static PFN_vkResetCommandBuffer							qvkResetCommandBuffer;
static PFN_vkResetDescriptorPool						qvkResetDescriptorPool;
static PFN_vkResetFences								qvkResetFences;
static PFN_vkUnmapMemory								qvkUnmapMemory;
static PFN_vkUpdateDescriptorSets						qvkUpdateDescriptorSets;
static PFN_vkWaitForFences								qvkWaitForFences;

static PFN_vkGetBufferMemoryRequirements2KHR			qvkGetBufferMemoryRequirements2KHR;
static PFN_vkGetImageMemoryRequirements2KHR				qvkGetImageMemoryRequirements2KHR;

static PFN_vkDebugMarkerSetObjectNameEXT				qvkDebugMarkerSetObjectNameEXT;
static PFN_vkGetFramebufferTilePropertiesQCOM			qvkGetFramebufferTilePropertiesQCOM;

////////////////////////////////////////////////////////////////////////////

// forward declarations
VkPipeline create_pipeline( const Vk_Pipeline_Def *def, renderPass_t renderPassIndex, uint32_t def_index );

static uint32_t find_memory_type( uint32_t memory_type_bits, VkMemoryPropertyFlags properties ) {
	VkPhysicalDeviceMemoryProperties memory_properties;
	uint32_t i;

	qvkGetPhysicalDeviceMemoryProperties( vk.physical_device, &memory_properties );

	for ( i = 0; i < memory_properties.memoryTypeCount; i++ ) {
		if ((memory_type_bits & (1 << i)) != 0 &&
			(memory_properties.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}
	ri.Error( ERR_FATAL, "Vulkan: failed to find matching memory type with requested properties" );
	return ~0U;
}


static uint32_t find_memory_type2( uint32_t memory_type_bits, VkMemoryPropertyFlags properties, VkMemoryPropertyFlags *outprops ) {
	VkPhysicalDeviceMemoryProperties memory_properties;
	uint32_t i;

	qvkGetPhysicalDeviceMemoryProperties( vk.physical_device, &memory_properties );

	for ( i = 0; i < memory_properties.memoryTypeCount; i++ ) {
		if ( (memory_type_bits & (1 << i)) != 0 && (memory_properties.memoryTypes[i].propertyFlags & properties) == properties ) {
			if ( outprops ) {
				*outprops = memory_properties.memoryTypes[i].propertyFlags;
			}
			return i;
		}
	}

	return ~0U;
}



#define CASE_STR(x) case (x): return #x

const char *vk_format_string( VkFormat format )
{
	static char buf[16];

	switch ( format ) {
		// color formats
		CASE_STR( VK_FORMAT_R5G5B5A1_UNORM_PACK16 );
		CASE_STR( VK_FORMAT_B5G5R5A1_UNORM_PACK16 );
		CASE_STR( VK_FORMAT_R5G6B5_UNORM_PACK16 );
		CASE_STR( VK_FORMAT_B5G6R5_UNORM_PACK16 );
		CASE_STR( VK_FORMAT_B8G8R8A8_SRGB );
		CASE_STR( VK_FORMAT_R8G8B8A8_SRGB );
		CASE_STR( VK_FORMAT_B8G8R8A8_SNORM );
		CASE_STR( VK_FORMAT_R8G8B8A8_SNORM );
		CASE_STR( VK_FORMAT_B8G8R8A8_UNORM );
		CASE_STR( VK_FORMAT_R8G8B8A8_UNORM );
		CASE_STR( VK_FORMAT_B4G4R4A4_UNORM_PACK16 );
		CASE_STR( VK_FORMAT_R4G4B4A4_UNORM_PACK16 );
		CASE_STR( VK_FORMAT_R16G16B16A16_UNORM );
		CASE_STR( VK_FORMAT_A2B10G10R10_UNORM_PACK32 );
		CASE_STR( VK_FORMAT_A2R10G10B10_UNORM_PACK32 );
		CASE_STR( VK_FORMAT_B10G11R11_UFLOAT_PACK32 );
		// depth formats
		CASE_STR( VK_FORMAT_D16_UNORM );
		CASE_STR( VK_FORMAT_D16_UNORM_S8_UINT );
		CASE_STR( VK_FORMAT_X8_D24_UNORM_PACK32 );
		CASE_STR( VK_FORMAT_D24_UNORM_S8_UINT );
		CASE_STR( VK_FORMAT_D32_SFLOAT );
		CASE_STR( VK_FORMAT_D32_SFLOAT_S8_UINT );
	default:
		Com_sprintf( buf, sizeof( buf ), "#%i", format );
		return buf;
	}
}


static const char *vk_result_string( VkResult code ) {
	static char buffer[32];

	switch ( code ) {
		CASE_STR( VK_SUCCESS );
		CASE_STR( VK_NOT_READY );
		CASE_STR( VK_TIMEOUT );
		CASE_STR( VK_EVENT_SET );
		CASE_STR( VK_EVENT_RESET );
		CASE_STR( VK_INCOMPLETE );
		CASE_STR( VK_ERROR_OUT_OF_HOST_MEMORY );
		CASE_STR( VK_ERROR_OUT_OF_DEVICE_MEMORY );
		CASE_STR( VK_ERROR_INITIALIZATION_FAILED );
		CASE_STR( VK_ERROR_DEVICE_LOST );
		CASE_STR( VK_ERROR_MEMORY_MAP_FAILED );
		CASE_STR( VK_ERROR_LAYER_NOT_PRESENT );
		CASE_STR( VK_ERROR_EXTENSION_NOT_PRESENT );
		CASE_STR( VK_ERROR_FEATURE_NOT_PRESENT );
		CASE_STR( VK_ERROR_INCOMPATIBLE_DRIVER );
		CASE_STR( VK_ERROR_TOO_MANY_OBJECTS );
		CASE_STR( VK_ERROR_FORMAT_NOT_SUPPORTED );
		CASE_STR( VK_ERROR_FRAGMENTED_POOL );
		CASE_STR( VK_ERROR_UNKNOWN );
		CASE_STR( VK_ERROR_OUT_OF_POOL_MEMORY );
		CASE_STR( VK_ERROR_INVALID_EXTERNAL_HANDLE );
		CASE_STR( VK_ERROR_FRAGMENTATION );
		CASE_STR( VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS );
		CASE_STR( VK_ERROR_SURFACE_LOST_KHR );
		CASE_STR( VK_ERROR_NATIVE_WINDOW_IN_USE_KHR );
		CASE_STR( VK_SUBOPTIMAL_KHR );
		CASE_STR( VK_ERROR_OUT_OF_DATE_KHR );
		CASE_STR( VK_ERROR_INCOMPATIBLE_DISPLAY_KHR );
		CASE_STR( VK_ERROR_VALIDATION_FAILED_EXT );
		CASE_STR( VK_ERROR_INVALID_SHADER_NV );
		CASE_STR( VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT );
		CASE_STR( VK_ERROR_NOT_PERMITTED_EXT );
		CASE_STR( VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT );
		CASE_STR( VK_THREAD_IDLE_KHR );
		CASE_STR( VK_THREAD_DONE_KHR );
		CASE_STR( VK_OPERATION_DEFERRED_KHR );
		CASE_STR( VK_OPERATION_NOT_DEFERRED_KHR );
		CASE_STR( VK_PIPELINE_COMPILE_REQUIRED_EXT );
	default:
		sprintf( buffer, "code %i", code );
		return buffer;
	}
}
#undef CASE_STR

#define VK_CHECK( function_call ) { \
	VkResult res = function_call; \
	if ( res < 0 ) { \
		ri.Error( ERR_FATAL, "Vulkan: %s returned %s", #function_call, vk_result_string( res ) ); \
	} \
}


/*
static VkFlags get_composite_alpha( VkCompositeAlphaFlagsKHR flags )
{
	const VkCompositeAlphaFlagBitsKHR compositeFlags[] = {
		VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
		VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
		VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR
	};
	int i;

	for ( i = 1; i < ARRAY_LEN( compositeFlags ); i++ ) {
		if ( flags & compositeFlags[i] ) {
			return compositeFlags[i];
		}
	}

	return compositeFlags[0];
}
*/


static VkCommandBuffer begin_command_buffer( void )
{
	VkCommandBufferBeginInfo begin_info;
	VkCommandBufferAllocateInfo alloc_info;
	VkCommandBuffer command_buffer;

	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc_info.pNext = NULL;
	alloc_info.commandPool = vk.command_pool;
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc_info.commandBufferCount = 1;
	VK_CHECK( qvkAllocateCommandBuffers( vk.device, &alloc_info, &command_buffer ) );

	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.pNext = NULL;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	begin_info.pInheritanceInfo = NULL;

	VK_CHECK( qvkBeginCommandBuffer( command_buffer, &begin_info ) );

	return command_buffer;
}


static void end_command_buffer( VkCommandBuffer command_buffer, const char *location )
{
#ifdef USE_UPLOAD_QUEUE
	const VkPipelineStageFlags wait_dst_stage_mask = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
	VkSemaphore waits;
#endif
	VkSubmitInfo submit_info;
	VkCommandBuffer cmdbuf[1];

	cmdbuf[0] = command_buffer;

	VK_CHECK( qvkEndCommandBuffer( command_buffer ) );

	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.pNext = NULL;
#ifdef USE_UPLOAD_QUEUE
	if ( vk.rendering_finished != VK_NULL_HANDLE ) {
		waits = vk.rendering_finished;
		vk.rendering_finished = VK_NULL_HANDLE;
		submit_info.waitSemaphoreCount = 1;
		submit_info.pWaitSemaphores = &waits;
		submit_info.pWaitDstStageMask = &wait_dst_stage_mask;
	} else 
#endif
	{
		submit_info.waitSemaphoreCount = 0;
		submit_info.pWaitSemaphores = NULL;
		submit_info.pWaitDstStageMask = NULL;
	}

	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = cmdbuf;
	submit_info.signalSemaphoreCount = 0;
	submit_info.pSignalSemaphores = NULL;

	VK_CHECK( qvkQueueSubmit( vk.queue, 1, &submit_info, VK_NULL_HANDLE ) );

	vk_queue_wait_idle();

	qvkFreeCommandBuffers( vk.device, vk.command_pool, 1, cmdbuf );
}


// Forward declaration for virtual screen rendering (defined later in file)


// Forward declaration for record_image_layout_transition
static void record_image_layout_transition( VkCommandBuffer command_buffer, VkImage image, VkImageAspectFlags image_aspect_flags,
	VkImageLayout old_layout, VkImageLayout new_layout, uint32_t src_stage_override, uint32_t dst_stage_override );


static void record_image_layout_transition( VkCommandBuffer command_buffer, VkImage image, VkImageAspectFlags image_aspect_flags,
	VkImageLayout old_layout, VkImageLayout new_layout, uint32_t src_stage_override, uint32_t dst_stage_override ) {
	VkImageMemoryBarrier barrier;
	uint32_t src_stage, dst_stage;

	switch ( old_layout ) {
		case VK_IMAGE_LAYOUT_UNDEFINED:
			if ( src_stage_override != 0 )
				src_stage = src_stage_override;
			else
				src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			barrier.srcAccessMask = VK_ACCESS_NONE;
			break;
		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
			src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			break;
		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			src_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
			break;
		case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
			src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			barrier.srcAccessMask = VK_ACCESS_NONE;
			break;
		case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
			src_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			break;
		default:
			ri.Error( ERR_DROP, "unsupported old layout %i", old_layout );
			src_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
			barrier.srcAccessMask = VK_ACCESS_NONE;
			break;
	}

	switch ( new_layout ) {
		case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
			dst_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
			dst_stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
			barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
			dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			barrier.dstAccessMask = VK_ACCESS_NONE;
			break;
		case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
			dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			break;
		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
			break;
		default:
			ri.Error( ERR_DROP, "unsupported new layout %i", new_layout);
			dst_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
			barrier.dstAccessMask = VK_ACCESS_NONE;
			break;
	}


	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.pNext = NULL;
	//barrier.srcAccessMask = src_access_flags;
	//barrier.dstAccessMask = dst_access_flags;
	barrier.oldLayout = old_layout;
	barrier.newLayout = new_layout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = image_aspect_flags;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

	qvkCmdPipelineBarrier( command_buffer, src_stage, dst_stage, 0, 0, NULL, 0, NULL, 1, &barrier );
}


// debug markers
#define SET_OBJECT_NAME(obj,objName,objType) vk_set_object_name( (uint64_t)(obj), (objName), (objType) )

static void vk_set_object_name( uint64_t obj, const char *objName, VkDebugReportObjectTypeEXT objType )
{
	if ( qvkDebugMarkerSetObjectNameEXT && obj )
	{
		VkDebugMarkerObjectNameInfoEXT info;
		info.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT;
		info.pNext = NULL;
		info.objectType = objType;
		info.object = obj;
		info.pObjectName = objName;
		qvkDebugMarkerSetObjectNameEXT( vk.device, &info );
	}
}


// Forward declarations: defined later in file
static VkFormat vk_get_unorm_format( VkFormat format );
static VkSampler vk_find_sampler( const Vk_Sampler_Def *def );


// VK_EXT_fragment_density_map: the map is the last attachment of every scene pass,
// referenced by VkRenderPassFragmentDensityMapCreateInfoEXT rather than by a subpass.
#define VK_FDM_FORMAT VK_FORMAT_R8G8_UNORM

static void vk_describe_fdm_attachment( VkAttachmentDescription *att )
{
	Com_Memset( att, 0, sizeof( *att ) );
	att->format = VK_FDM_FORMAT;
	att->samples = VK_SAMPLE_COUNT_1_BIT;
	att->loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	att->storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	att->stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	att->stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	att->initialLayout = VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT;
	att->finalLayout = VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT;
}

// The caller's attachment array must have room for one more entry
static void vk_chain_fdm_attachment( VkRenderPassCreateInfo *desc, VkRenderPassFragmentDensityMapCreateInfoEXT *fdmInfo,
	VkAttachmentDescription *attachments, uint32_t attachmentCount )
{
	vk_describe_fdm_attachment( &attachments[attachmentCount] );

	Com_Memset( fdmInfo, 0, sizeof( *fdmInfo ) );
	fdmInfo->sType = VK_STRUCTURE_TYPE_RENDER_PASS_FRAGMENT_DENSITY_MAP_CREATE_INFO_EXT;
	fdmInfo->pNext = desc->pNext;
	fdmInfo->fragmentDensityMapAttachment.attachment = attachmentCount;
	fdmInfo->fragmentDensityMapAttachment.layout = VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT;

	desc->pNext = fdmInfo;
	desc->attachmentCount = attachmentCount + 1;
}


/*
 * The scene draws alone in fov_scene and is stored; the post pass samples it --
 * sampled, not an input attachment, because Adreno displaces input-attachment
 * reads under a density map. The post pass carries no map and measured the same.
 */
static void vk_create_fov_split_render_passes( void )
{
	VkDevice device = vk.device;
	const qboolean useBloom = ( r_bloom && r_bloom->integer );
	const qboolean foveated = vk.xr.foveationActive;
	const VkFormat swapchainFormat = vk_get_unorm_format( vk.color_format );
	VkRenderPassMultiviewCreateInfo multiviewInfo;
	VkRenderPassFragmentDensityMapCreateInfoEXT fdmInfo;
	uint32_t viewMasks[3] = { 0b11, 0b11, 0b11 };
	uint32_t correlationMask = 0b11;
	VkAttachmentDescription attachments[4];
	VkSubpassDescription subpasses[3];
	VkSubpassDependency dependencies[4];
	VkRenderPassCreateInfo desc;
	VkAttachmentReference colorRef, depthRef, resolveRef, finalColorRef;
	uint32_t attachmentCount;

	if ( !vk.multiviewSupported ) {
		ri.Printf( PRINT_WARNING, "The split render passes need multiview support (skipping)\n" );
		return;
	}

	ri.Printf( PRINT_ALL, "Creating split render passes (bloom: %s, %s, density map: %s)...\n",
		useBloom ? "yes" : "no", vk.msaaActive ? "MSAA" : "non-MSAA", foveated ? "yes" : "no" );

	Com_Memset( &multiviewInfo, 0, sizeof( multiviewInfo ) );
	multiviewInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO;
	multiviewInfo.pViewMasks = viewMasks;
	multiviewInfo.correlationMaskCount = 1;
	multiviewInfo.pCorrelationMasks = &correlationMask;

	// --- Scene pass: foveated, one subpass ---
	Com_Memset( attachments, 0, sizeof( attachments ) );
	Com_Memset( subpasses, 0, sizeof( subpasses ) );
	Com_Memset( dependencies, 0, sizeof( dependencies ) );

	if ( vk.msaaActive ) {
		// [0] MSAA scene (transient), [1] scene image (resolve target, stored), [2] MSAA depth (transient)
		attachments[0].format = vk.color_format;
		attachments[0].samples = vkSamples;
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		attachments[1].format = vk.color_format;
		attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachments[1].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		attachments[2].format = vk.depth_format;
		attachments[2].samples = vkSamples;
		attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[2].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[2].stencilLoadOp = glConfig.stencilBits ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[2].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachments[2].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		colorRef.attachment = 0;
		colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		resolveRef.attachment = 1;
		resolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		depthRef.attachment = 2;
		depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		subpasses[0].pResolveAttachments = &resolveRef;
		attachmentCount = 3;
	} else {
		// [0] scene image (stored), [1] depth (transient)
		attachments[0].format = vk.color_format;
		attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		attachments[1].format = vk.depth_format;
		attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[1].stencilLoadOp = glConfig.stencilBits ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		colorRef.attachment = 0;
		colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		depthRef.attachment = 1;
		depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		attachmentCount = 2;
	}
	subpasses[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpasses[0].colorAttachmentCount = 1;
	subpasses[0].pColorAttachments = &colorRef;
	subpasses[0].pDepthStencilAttachment = &depthRef;

	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass = 0;
	dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependencies[0].srcAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	// The post pass samples the stored scene
	dependencies[1].srcSubpass = 0;
	dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[1].dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT | VK_ACCESS_SHADER_READ_BIT;
	dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	multiviewInfo.subpassCount = 1;

	Com_Memset( &desc, 0, sizeof( desc ) );
	desc.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	desc.pNext = &multiviewInfo;
	desc.attachmentCount = attachmentCount;
	desc.pAttachments = attachments;
	desc.subpassCount = 1;
	desc.pSubpasses = subpasses;
	desc.dependencyCount = 2;
	desc.pDependencies = dependencies;

	// Only the scene pass carries a map, and only where the device has one
	if ( foveated ) {
		vk_chain_fdm_attachment( &desc, &fdmInfo, attachments, attachmentCount );
	}

	VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.fov_scene ) );
	SET_OBJECT_NAME( vk.render_pass.fov_scene, "render pass - foveated scene", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );

	// --- Post pass: samples the stored scene ---
	Com_Memset( attachments, 0, sizeof( attachments ) );
	Com_Memset( subpasses, 0, sizeof( subpasses ) );
	Com_Memset( dependencies, 0, sizeof( dependencies ) );
	attachmentCount = 0;

	// Same shape with and without bloom: the first blur pass does the extract

	// [0] swapchain (UNORM view, the shaders output gamma-corrected values)
	attachments[attachmentCount].format = swapchainFormat;
	attachments[attachmentCount].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[attachmentCount].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[attachmentCount].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[attachmentCount].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[attachmentCount].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[attachmentCount].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[attachmentCount].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	finalColorRef.attachment = attachmentCount;
	finalColorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	attachmentCount++;

	// Subpass 0: composite + gamma with bloom, gamma alone without (sampled scene -> swapchain)
	subpasses[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpasses[0].colorAttachmentCount = 1;
	subpasses[0].pColorAttachments = &finalColorRef;

	// Subpass 1: post 2D (alpha blend onto swapchain)
	subpasses[1].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpasses[1].colorAttachmentCount = 1;
	subpasses[1].pColorAttachments = &finalColorRef;

	// External -> 0: the scene pass finished storing before the first sample of it
	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass = 0;
	dependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	dependencies[0].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[0].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
	dependencies[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	// 0 -> 1 keeps the 2D writes ordered behind the composite or gamma quad
	dependencies[1].srcSubpass = 0;
	dependencies[1].dstSubpass = 1;
	dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[1].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[1].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	dependencies[2].srcSubpass = 1;
	dependencies[2].dstSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[2].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[2].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	dependencies[2].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[2].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	dependencies[2].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	multiviewInfo.subpassCount = 2;
	desc.subpassCount = 2;
	desc.dependencyCount = 3;

	desc.pNext = &multiviewInfo;
	desc.attachmentCount = attachmentCount;
	desc.pAttachments = attachments;
	desc.pSubpasses = subpasses;
	desc.pDependencies = dependencies;

	if ( useBloom ) {
		VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.main_with_bloom ) );
		SET_OBJECT_NAME( vk.render_pass.main_with_bloom, "render pass - foveated post with bloom", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );
	} else {
		VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.main_with_gamma ) );
		SET_OBJECT_NAME( vk.render_pass.main_with_gamma, "render pass - foveated post with gamma", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );
	}

	ri.Printf( PRINT_ALL, "...split render passes created (scene: %u attachments%s, post: %u attachments, %u subpasses)\n",
		vk.msaaActive ? 3u : 2u, foveated ? " plus density map" : "", attachmentCount, desc.subpassCount );
}


/*
 * Destroy transient images used for subpass optimization.
 * Called during shutdown and before recreating with new dimensions on reinit.
 */
static void vk_destroy_subpass_transient_images( void )
{
	// Non-MSAA scene color
	if ( vk.transient.scene_image != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, vk.transient.scene_view, NULL );
		qvkDestroyImage( vk.device, vk.transient.scene_image, NULL );
		vk.transient.scene_view = VK_NULL_HANDLE;
		vk.transient.scene_image = VK_NULL_HANDLE;
	}
	if ( vk.transient.scene_memory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.transient.scene_memory, NULL );
		vk.transient.scene_memory = VK_NULL_HANDLE;
	}

	// Depth
	if ( vk.transient.depth_image != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, vk.transient.depth_view, NULL );
		qvkDestroyImage( vk.device, vk.transient.depth_image, NULL );
		vk.transient.depth_view = VK_NULL_HANDLE;
		vk.transient.depth_image = VK_NULL_HANDLE;
	}
	if ( vk.transient.depth_memory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.transient.depth_memory, NULL );
		vk.transient.depth_memory = VK_NULL_HANDLE;
	}

	// MSAA scene color
	if ( vk.transient.msaa_image != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, vk.transient.msaa_view, NULL );
		qvkDestroyImage( vk.device, vk.transient.msaa_image, NULL );
		vk.transient.msaa_view = VK_NULL_HANDLE;
		vk.transient.msaa_image = VK_NULL_HANDLE;
	}
	if ( vk.transient.msaa_memory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.transient.msaa_memory, NULL );
		vk.transient.msaa_memory = VK_NULL_HANDLE;
	}

	// MSAA resolve target
	if ( vk.transient.resolve_image != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, vk.transient.resolve_view, NULL );
		qvkDestroyImage( vk.device, vk.transient.resolve_image, NULL );
		vk.transient.resolve_view = VK_NULL_HANDLE;
		vk.transient.resolve_image = VK_NULL_HANDLE;
	}
	if ( vk.transient.resolve_memory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.transient.resolve_memory, NULL );
		vk.transient.resolve_memory = VK_NULL_HANDLE;
	}

	// Descriptors are freed when the pool is reset, just clear the handles
	vk.transient.scene_descriptor = VK_NULL_HANDLE;
}


/*
 * Create transient images for subpass optimization.
 * These images stay in tile memory and never hit DRAM.
 *
 * Uses VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT
 * with VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT for optimal tile-based GPU performance.
 */
static void vk_create_subpass_transient_images( void )
{
	VkImageCreateInfo imageInfo;
	VkImageViewCreateInfo viewInfo;
	VkMemoryRequirements memReqs;
	VkMemoryAllocateInfo allocInfo;
	uint32_t memoryType;

	if ( !vk.multiviewSupported )
		return;

	ri.Printf( PRINT_ALL, "Creating transient images (%dx%d, %s, stored scene)...\n",
		glConfig.vidWidth, glConfig.vidHeight, vk.msaaActive ? "MSAA" : "non-MSAA" );

	// Common image creation settings
	Com_Memset( &imageInfo, 0, sizeof( imageInfo ) );
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = glConfig.vidWidth;
	imageInfo.extent.height = glConfig.vidHeight;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 2;  // Stereo multiview
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	// Common view creation settings
	Com_Memset( &viewInfo, 0, sizeof( viewInfo ) );
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
	viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 2;

	Com_Memset( &allocInfo, 0, sizeof( allocInfo ) );
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;

	if ( vk.msaaActive )
	{
		// MSAA path: create MSAA scene, resolve target, and depth

		// [1] MSAA scene color - TRANSIENT only (no INPUT_ATTACHMENT since we read from resolve)
		imageInfo.format = vk.color_format;
		imageInfo.samples = vkSamples;
		imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
		VK_CHECK( qvkCreateImage( vk.device, &imageInfo, NULL, &vk.transient.msaa_image ) );

		qvkGetImageMemoryRequirements( vk.device, vk.transient.msaa_image, &memReqs );
		memoryType = find_memory_type2( memReqs.memoryTypeBits,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT, NULL );
		if ( memoryType == ~0U )
			memoryType = find_memory_type( memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
		allocInfo.allocationSize = memReqs.size;
		allocInfo.memoryTypeIndex = memoryType;
		VK_CHECK( qvkAllocateMemory( vk.device, &allocInfo, NULL, &vk.transient.msaa_memory ) );
		VK_CHECK( qvkBindImageMemory( vk.device, vk.transient.msaa_image, vk.transient.msaa_memory, 0 ) );

		viewInfo.image = vk.transient.msaa_image;
		viewInfo.format = vk.color_format;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		VK_CHECK( qvkCreateImageView( vk.device, &viewInfo, NULL, &vk.transient.msaa_view ) );
		SET_OBJECT_NAME( vk.transient.msaa_image, "transient MSAA scene color", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );

		// [2] Resolve target - stored, sampled by the post pass; direct mode resolves into the swapchain.
		if ( vk.fboActive ) {
			imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
			imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
			VK_CHECK( qvkCreateImage( vk.device, &imageInfo, NULL, &vk.transient.resolve_image ) );

			qvkGetImageMemoryRequirements( vk.device, vk.transient.resolve_image, &memReqs );
			memoryType = find_memory_type( memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
			allocInfo.allocationSize = memReqs.size;
			allocInfo.memoryTypeIndex = memoryType;
			VK_CHECK( qvkAllocateMemory( vk.device, &allocInfo, NULL, &vk.transient.resolve_memory ) );
			VK_CHECK( qvkBindImageMemory( vk.device, vk.transient.resolve_image, vk.transient.resolve_memory, 0 ) );

			viewInfo.image = vk.transient.resolve_image;
			VK_CHECK( qvkCreateImageView( vk.device, &viewInfo, NULL, &vk.transient.resolve_view ) );
			SET_OBJECT_NAME( vk.transient.resolve_image, "stored scene resolve target", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
		}

		// [3] MSAA depth - TRANSIENT
		imageInfo.format = vk.depth_format;
		imageInfo.samples = vkSamples;
		imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
		VK_CHECK( qvkCreateImage( vk.device, &imageInfo, NULL, &vk.transient.depth_image ) );

		qvkGetImageMemoryRequirements( vk.device, vk.transient.depth_image, &memReqs );
		memoryType = find_memory_type2( memReqs.memoryTypeBits,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT, NULL );
		if ( memoryType == ~0U )
			memoryType = find_memory_type( memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
		allocInfo.allocationSize = memReqs.size;
		allocInfo.memoryTypeIndex = memoryType;
		VK_CHECK( qvkAllocateMemory( vk.device, &allocInfo, NULL, &vk.transient.depth_memory ) );
		VK_CHECK( qvkBindImageMemory( vk.device, vk.transient.depth_image, vk.transient.depth_memory, 0 ) );

		viewInfo.image = vk.transient.depth_image;
		viewInfo.format = vk.depth_format;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		if ( glConfig.stencilBits > 0 )
			viewInfo.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
		VK_CHECK( qvkCreateImageView( vk.device, &viewInfo, NULL, &vk.transient.depth_view ) );
		SET_OBJECT_NAME( vk.transient.depth_image, "transient MSAA depth", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );

		ri.Printf( PRINT_ALL, "...transient images created (MSAA path)\n" );
	}
	else
	{
		// Non-MSAA path: create scene color and depth

		// [1] Scene color - stored, sampled by the post pass; direct mode draws into the swapchain.
		if ( vk.fboActive ) {
			imageInfo.format = vk.color_format;
			imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
			imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
			VK_CHECK( qvkCreateImage( vk.device, &imageInfo, NULL, &vk.transient.scene_image ) );

			qvkGetImageMemoryRequirements( vk.device, vk.transient.scene_image, &memReqs );
			memoryType = find_memory_type( memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
			allocInfo.allocationSize = memReqs.size;
			allocInfo.memoryTypeIndex = memoryType;
			VK_CHECK( qvkAllocateMemory( vk.device, &allocInfo, NULL, &vk.transient.scene_memory ) );
			VK_CHECK( qvkBindImageMemory( vk.device, vk.transient.scene_image, vk.transient.scene_memory, 0 ) );

			viewInfo.image = vk.transient.scene_image;
			viewInfo.format = vk.color_format;
			viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			VK_CHECK( qvkCreateImageView( vk.device, &viewInfo, NULL, &vk.transient.scene_view ) );
			SET_OBJECT_NAME( vk.transient.scene_image, "stored scene color", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
		}

		// [2] Depth - TRANSIENT
		imageInfo.format = vk.depth_format;
		imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
		VK_CHECK( qvkCreateImage( vk.device, &imageInfo, NULL, &vk.transient.depth_image ) );

		qvkGetImageMemoryRequirements( vk.device, vk.transient.depth_image, &memReqs );
		memoryType = find_memory_type2( memReqs.memoryTypeBits,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT, NULL );
		if ( memoryType == ~0U )
			memoryType = find_memory_type( memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
		allocInfo.allocationSize = memReqs.size;
		allocInfo.memoryTypeIndex = memoryType;
		VK_CHECK( qvkAllocateMemory( vk.device, &allocInfo, NULL, &vk.transient.depth_memory ) );
		VK_CHECK( qvkBindImageMemory( vk.device, vk.transient.depth_image, vk.transient.depth_memory, 0 ) );

		viewInfo.image = vk.transient.depth_image;
		viewInfo.format = vk.depth_format;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		if ( glConfig.stencilBits > 0 )
			viewInfo.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
		VK_CHECK( qvkCreateImageView( vk.device, &viewInfo, NULL, &vk.transient.depth_view ) );
		SET_OBJECT_NAME( vk.transient.depth_image, "transient depth", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );

		ri.Printf( PRINT_ALL, "...transient images created (non-MSAA path)\n" );
	}

	// The post pass and the first blur pass sample the stored scene
	if ( vk.fboActive ) {
		VkDescriptorSetAllocateInfo descAlloc;
		VkDescriptorImageInfo imageInfoDesc;
		VkWriteDescriptorSet writeDesc;
		Vk_Sampler_Def samplerDef;

		Com_Memset( &samplerDef, 0, sizeof( samplerDef ) );
		samplerDef.gl_mag_filter = GL_LINEAR;
		samplerDef.gl_min_filter = GL_LINEAR;
		samplerDef.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerDef.max_lod_1_0 = qtrue;
		samplerDef.noAnisotropy = qtrue;

		descAlloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		descAlloc.pNext = NULL;
		descAlloc.descriptorPool = vk.descriptor_pool;
		descAlloc.descriptorSetCount = 1;
		descAlloc.pSetLayouts = &vk.set_layout_sampler;
		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &descAlloc, &vk.transient.scene_descriptor ) );

		imageInfoDesc.sampler = vk_find_sampler( &samplerDef );
		imageInfoDesc.imageView = vk.msaaActive ? vk.transient.resolve_view : vk.transient.scene_view;
		imageInfoDesc.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		writeDesc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDesc.pNext = NULL;
		writeDesc.dstSet = vk.transient.scene_descriptor;
		writeDesc.dstBinding = 0;
		writeDesc.dstArrayElement = 0;
		writeDesc.descriptorCount = 1;
		writeDesc.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writeDesc.pImageInfo = &imageInfoDesc;
		writeDesc.pBufferInfo = NULL;
		writeDesc.pTexelBufferView = NULL;

		qvkUpdateDescriptorSets( vk.device, 1, &writeDesc, 0, NULL );

		SET_OBJECT_NAME( (uint64_t)vk.transient.scene_descriptor, "scene sampler descriptor", VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT );
		ri.Printf( PRINT_ALL, "...scene sampler descriptor allocated\n" );
	}
}


static void vk_create_render_passes( void )
{
	VkAttachmentDescription attachments[3]; // color | depth | msaa color
	VkAttachmentReference colorResolveRef;
	VkAttachmentReference colorRef0;
	VkAttachmentReference depthRef0;
	VkSubpassDescription subpass;
	VkSubpassDependency deps[3];
	VkRenderPassCreateInfo desc;
	VkFormat depth_format;
	VkDevice device;
	uint32_t i;

	depth_format = vk.depth_format;
	device = vk.device;

	// Common subpass dependencies: used by all render passes
	Com_Memset( &deps, 0, sizeof( deps ) );

	// deps[0]: External -> subpass 0 (wait for previous operations before color/depth output)
	// Includes depth stages so an earlier frame's depth work finishes before this pass clears
	deps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	deps[0].dstSubpass = 0;
	deps[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
	                        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	deps[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
	                        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	deps[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT |
	                         VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	deps[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
	                         VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
	                         VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
	                         VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	deps[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	// deps[1]: Subpass 0 -> external (wait for color/depth writes before shader reads)
	deps[1].srcSubpass = 0;
	deps[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	deps[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
	                        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	deps[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	deps[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
	                         VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	deps[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	deps[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	// deps[2]: External -> subpass 0 (color output ordering, used by gamma/bloom passes)
	deps[2].srcSubpass = VK_SUBPASS_EXTERNAL;
	deps[2].dstSubpass = 0;
	deps[2].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	deps[2].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	deps[2].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
	deps[2].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	deps[2].dependencyFlags = 0;

	// Screenmap render pass: only needed for FBO mode (r_fbo=1)
	// In non-FBO mode, we skip directly to XR multiview passes
	if ( r_fbo->integer )
	{
		desc.dependencyCount = 2;
		desc.pDependencies = &deps[0];

		// screenmap resolve/color buffer
		attachments[0].flags = 0;
		attachments[0].format = vk.color_format;
		attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
#ifdef USE_BUFFER_CLEAR
		if ( vk.screenMapSamples > VK_SAMPLE_COUNT_1_BIT )
			attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		else
			attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
#else
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; // Assuming this will be completely overwritten
#endif
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;   // needed for next render pass
		attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		// screenmap depth buffer
		attachments[1].flags = 0;
		attachments[1].format = depth_format;
		attachments[1].samples = vk.screenMapSamples;
		attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // Need empty depth buffer before use
		attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		colorRef0.attachment = 0;
		colorRef0.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		depthRef0.attachment = 1;
		depthRef0.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		Com_Memset( &subpass, 0, sizeof( subpass ) );
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorRef0;
		subpass.pDepthStencilAttachment = &depthRef0;

		Com_Memset( &desc, 0, sizeof( desc ) );
		desc.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = 0;
		desc.pAttachments = attachments;
		desc.pSubpasses = &subpass;
		desc.subpassCount = 1;
		desc.attachmentCount = 2;
		desc.dependencyCount = 2;
		desc.pDependencies = deps;

		if ( vk.screenMapSamples > VK_SAMPLE_COUNT_1_BIT ) {

			attachments[2].flags = 0;
			attachments[2].format = vk.color_format;
			attachments[2].samples = vk.screenMapSamples;
#ifdef USE_BUFFER_CLEAR
			attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
#else
			attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
#endif
			attachments[2].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachments[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachments[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachments[2].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			attachments[2].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			desc.attachmentCount = 3;

			colorRef0.attachment = 2; // screenmap msaa image attachment
			colorRef0.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			colorResolveRef.attachment = 0; // screenmap resolve image attachment
			colorResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			subpass.pResolveAttachments = &colorResolveRef;
		}

		VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.screenmap ) );

		SET_OBJECT_NAME( vk.render_pass.screenmap, "render pass - screenmap", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );
	}
	/*
	 * XR Multiview Render Passes (VK_KHR_multiview)
	 *
	 * Main pass attachments:
	 *   [0] Color resolve target (1x samples) - STORE for gamma pass
	 *   [1] Depth (multisampled if MSAA) - never stored
	 *   [2] MSAA color (only if MSAA active) - never stored, resolved into [0]
	 *
	 * Uses reversed depth (near=1.0, far=0.0) for precision.
	 */
	ri.Printf( PRINT_ALL, "Checking multiview support: %d\n", vk.multiviewSupported );
	if ( vk.multiviewSupported )
	{
		ri.Printf( PRINT_ALL, "Creating XR multiview render passes (MSAA: %s)...\n",
			vk.msaaActive ? "yes" : "no" );
		VkRenderPassMultiviewCreateInfo multiviewInfo;
		uint32_t viewMask = 0b11;        // Both eyes (views 0 and 1)
		uint32_t correlationMask = 0b11; // Optimization: views are correlated

		Com_Memset( &multiviewInfo, 0, sizeof( multiviewInfo ) );
		multiviewInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO;
		multiviewInfo.subpassCount = 1;
		multiviewInfo.pViewMasks = &viewMask;
		multiviewInfo.correlationMaskCount = 1;
		multiviewInfo.pCorrelationMasks = &correlationMask;

		// Attachment 0: Color resolve target
		attachments[0].flags = 0;
		attachments[0].format = vk.color_format;
		attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
		if ( vk.msaaActive ) {
			attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; // Written by resolve
		} else {
#ifdef USE_BUFFER_CLEAR
			attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
#else
			attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
#endif
		}
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		// Attachment 1: Depth
		attachments[1].flags = 0;
		attachments[1].format = depth_format;
		attachments[1].samples = vk.msaaActive ? vkSamples : VK_SAMPLE_COUNT_1_BIT;
		attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[1].stencilLoadOp = glConfig.stencilBits ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		colorRef0.attachment = 0;
		colorRef0.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		depthRef0.attachment = 1;
		depthRef0.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		Com_Memset( &subpass, 0, sizeof( subpass ) );
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorRef0;
		subpass.pDepthStencilAttachment = &depthRef0;

		Com_Memset( &desc, 0, sizeof( desc ) );
		desc.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		desc.pNext = &multiviewInfo;
		desc.flags = 0;
		desc.pAttachments = attachments;
		desc.attachmentCount = 2;
		desc.pSubpasses = &subpass;
		desc.subpassCount = 1;
		desc.dependencyCount = 2;
		desc.pDependencies = deps;

		if ( vk.msaaActive )
		{
			// Attachment 2: MSAA color (render target, resolves to [0])
			attachments[2].flags = 0;
			attachments[2].format = vk.color_format;
			attachments[2].samples = vkSamples;
#ifdef USE_BUFFER_CLEAR
			attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
#else
			attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
#endif
			attachments[2].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachments[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachments[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachments[2].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			attachments[2].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			desc.attachmentCount = 3;
			colorRef0.attachment = 2;         // Render to MSAA
			colorRef0.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			colorResolveRef.attachment = 0;   // Resolve to non-MSAA
			colorResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			subpass.pResolveAttachments = &colorResolveRef;
		}

		VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.main ) );
		SET_OBJECT_NAME( vk.render_pass.main, "render pass - XR main (multiview)", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );
		ri.Printf( PRINT_ALL, "Created main render pass: %p (attachments: %d)\n", (void*)vk.render_pass.main, desc.attachmentCount );

		// Reset for non-MSAA passes
		subpass.pResolveAttachments = NULL;
		colorRef0.attachment = 0;

		// Gamma pass: outputs to XR swapchain (placeholder format, recreated later with UNORM)
		attachments[0].flags = 0;
		attachments[0].format = vk.color_format;
		attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		desc.attachmentCount = 1;
		subpass.pDepthStencilAttachment = NULL;
		desc.dependencyCount = 1;
		desc.pDependencies = &deps[2];

		VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.gamma ) );
		SET_OBJECT_NAME( vk.render_pass.gamma, "render pass - XR gamma (multiview)", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );

		// Bloom passes (multiview)
		attachments[0].format = vk.bloom_format;
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		for ( i = 0; i < ARRAY_LEN( vk.render_pass.blur ); i++ ) {
			VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.blur[i] ) );
			SET_OBJECT_NAME( vk.render_pass.blur[i], va( "render pass - XR blur %i (multiview)", i ), VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );
		}

		// Post-bloom: blends bloom back into FBO color
		if ( vk.msaaActive ) {
			// MSAA: [0]=resolve, [1]=depth, [2]=MSAA color
			attachments[0].format = vk.color_format;
			attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
			attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			attachments[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			attachments[1].format = depth_format;
			attachments[1].samples = vkSamples;
			attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachments[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

			attachments[2].format = vk.color_format;
			attachments[2].samples = vkSamples;
			attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			attachments[2].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachments[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachments[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachments[2].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			attachments[2].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			desc.attachmentCount = 3;
			desc.dependencyCount = 2;
			desc.pDependencies = deps;
			colorRef0.attachment = 2;
			colorRef0.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			colorResolveRef.attachment = 0;
			colorResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			subpass.pDepthStencilAttachment = &depthRef0;
			subpass.pResolveAttachments = &colorResolveRef;
		} else {
			// Non-MSAA: color only
			attachments[0].format = vk.color_format;
			attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			attachments[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			desc.attachmentCount = 1;
			desc.dependencyCount = 1;
			desc.pDependencies = &deps[2];
		}

		VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.post_bloom ) );
		SET_OBJECT_NAME( vk.render_pass.post_bloom, "render pass - XR post bloom (multiview)", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );

		ri.Printf( PRINT_ALL, "...XR multiview render passes created\n" );
	}

	/*
	 * HUD Buffer Render Pass (1280x960, single layer, no multiview)
	 * Used for HUD mode 1 (in-world sprite). Depth needed for 3D models.
	 * [0] Color: LOAD preserves content (1-frame latency)
	 * [1] Depth: CLEAR each frame, never stored
	 */
	{
		VkRenderPassMultiviewCreateInfo hudMultiviewInfo;
		VkSubpassDependency hudDeps[2];
		VkAttachmentReference hudDepthRef;
		uint32_t hudViewMask = 0b01;
		uint32_t hudCorrelationMask = 0b01;

		ri.Printf( PRINT_ALL, "Creating HUD buffer render pass (color_format=0x%x, depth_format=0x%x, multiview)...\n",
			vk.color_format, depth_format );

		attachments[0].flags = 0;
		attachments[0].format = vk.color_format;
		attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		// Cleared every HUD pass and never read after, so it never leaves tile memory
		attachments[1].flags = 0;
		attachments[1].format = depth_format;
		attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		colorRef0.attachment = 0;
		colorRef0.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		hudDepthRef.attachment = 1;
		hudDepthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		Com_Memset( &subpass, 0, sizeof( subpass ) );
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorRef0;
		subpass.pDepthStencilAttachment = &hudDepthRef;

		// Dependencies: wait for sampling before load, complete writes before sampling
		hudDeps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
		hudDeps[0].dstSubpass = 0;
		hudDeps[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		hudDeps[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		hudDeps[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		hudDeps[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		// Not by-region: the source is the previous frame sampling this texture anywhere in the eye framebuffer
		hudDeps[0].dependencyFlags = 0;

		hudDeps[1].srcSubpass = 0;
		hudDeps[1].dstSubpass = VK_SUBPASS_EXTERNAL;
		hudDeps[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
		                          VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
		                          VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		hudDeps[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		hudDeps[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
		                           VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		hudDeps[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		// Mirrored: the frame command buffer samples it at arbitrary coordinates
		hudDeps[1].dependencyFlags = 0;

		Com_Memset( &desc, 0, sizeof( desc ) );
		desc.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = 0;
		desc.pAttachments = attachments;
		desc.attachmentCount = 2;
		desc.pSubpasses = &subpass;
		desc.subpassCount = 1;
		desc.dependencyCount = 2;
		desc.pDependencies = hudDeps;

		VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.hudBuffer ) );
		SET_OBJECT_NAME( vk.render_pass.hudBuffer, "render pass - HUD buffer", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );

		// Same attachments and samples as above, so the pipelines and framebuffer serve both
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		VK_CHECK( qvkCreateRenderPass( device, &desc, NULL, &vk.render_pass.hudBufferClear ) );
		SET_OBJECT_NAME( vk.render_pass.hudBufferClear, "render pass - HUD buffer, cleared", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );

		ri.Printf( PRINT_ALL, "...HUD buffer render passes created\n" );
	}
}


static void allocate_and_bind_image_memory(VkImage image) {
	VkMemoryRequirements memory_requirements;
	VkDeviceSize alignment;
	ImageChunk *chunk;
	int i;

	qvkGetImageMemoryRequirements(vk.device, image, &memory_requirements);

	if ( memory_requirements.size > vk.image_chunk_size ) {
		ri.Error( ERR_FATAL, "Vulkan: could not allocate memory, image is too large (%ikbytes).",
			(int)(memory_requirements.size/1024) );
	}

	chunk = NULL;

	// Try to find an existing chunk of sufficient capacity.
	alignment = memory_requirements.alignment;
	for ( i = 0; i < vk_world.num_image_chunks; i++ ) {
		// ensure that memory region has proper alignment
		VkDeviceSize offset = PAD( vk_world.image_chunks[i].used, alignment );

		if ( offset + memory_requirements.size <= vk.image_chunk_size ) {
			chunk = &vk_world.image_chunks[i];
			chunk->used = offset + memory_requirements.size;
			break;
		}
	}

	// Allocate a new chunk in case we couldn't find suitable existing chunk.
	if (chunk == NULL) {
		VkMemoryAllocateInfo alloc_info;
		VkDeviceMemory memory;

		if (vk_world.num_image_chunks >= MAX_IMAGE_CHUNKS) {
			ri.Error(ERR_FATAL, "Vulkan: image chunk limit has been reached" );
		}

		alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		alloc_info.pNext = NULL;
		alloc_info.allocationSize = vk.image_chunk_size;
		alloc_info.memoryTypeIndex = find_memory_type( memory_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );

		VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &memory ) );

		chunk = &vk_world.image_chunks[vk_world.num_image_chunks];
		chunk->memory = memory;
		chunk->used = memory_requirements.size;

		SET_OBJECT_NAME( memory, va( "image memory chunk %i", vk_world.num_image_chunks ), VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT );

		vk_world.num_image_chunks++;
	}

	VK_CHECK(qvkBindImageMemory(vk.device, image, chunk->memory, chunk->used - memory_requirements.size));
}


static void vk_clean_staging_buffer( void )
{
	if ( vk.staging_buffer.handle != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk.staging_buffer.handle, NULL );
		vk.staging_buffer.handle = VK_NULL_HANDLE;
	}

	//if ( vk.staging_buffer.ptr != NULL ) 
	//	qvkUnmapMemory( vk.device, vk.staging_buffer.memory ) {
	//	vk.staging_buffer.ptr = NULL;
	//}

	if ( vk.staging_buffer.memory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.staging_buffer.memory, NULL );
		vk.staging_buffer.memory = VK_NULL_HANDLE;
	}

	vk.staging_buffer.ptr = NULL;
	vk.staging_buffer.size = 0;
#ifdef USE_UPLOAD_QUEUE
	vk.staging_buffer.offset = 0;
#endif
}


#ifdef USE_UPLOAD_QUEUE
static qboolean vk_wait_staging_buffer( void )
{
	if ( vk.aux_fence_wait ) {
		VkResult res = qvkWaitForFences( vk.device, 1, &vk.aux_fence, VK_TRUE, 5 * 1000000000ULL );
		if ( res != VK_SUCCESS ) {
			ri.Error( ERR_FATAL, "vkWaitForFences() failed with %s at %s", vk_result_string( res ), __func__ );
		}
		qvkResetFences( vk.device, 1, &vk.aux_fence );
		VK_CHECK( qvkResetCommandBuffer( vk.staging_command_buffer, 0 ) );
		vk.staging_buffer.offset = 0; // Reset for reuse after fence confirms upload complete
		vk.aux_fence_wait = qfalse;
		return qtrue;
	} else {
		return qfalse;
	}
}


static void vk_flush_staging_buffer( qboolean final )
{
	const VkPipelineStageFlags wait_dst_stage_mask = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
	VkSemaphore waits;
	VkSubmitInfo submit_info;
	VkResult res;

	if ( vk.staging_buffer.offset == 0 ) {
		return;
	}

	//ri.Printf( PRINT_WARNING, S_COLOR_CYAN ">>> flush %i bytes (final=%i)<<<\n", (int)vk_world.staging_buffer_offset, final );

	vk.staging_buffer.offset = 0;

	VK_CHECK( qvkEndCommandBuffer( vk.staging_command_buffer ) );

	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.pNext = NULL;

	if ( vk.rendering_finished != VK_NULL_HANDLE ) {
		// first call after previous queue submission?
		waits = vk.rendering_finished;
		vk.rendering_finished = VK_NULL_HANDLE;
		submit_info.waitSemaphoreCount = 1;
		submit_info.pWaitSemaphores = &waits;
		submit_info.pWaitDstStageMask = &wait_dst_stage_mask;
	} else {
		submit_info.waitSemaphoreCount = 0;
		submit_info.pWaitSemaphores = NULL;
		submit_info.pWaitDstStageMask = NULL;
	}

	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &vk.staging_command_buffer;

	if ( vk.image_uploaded != VK_NULL_HANDLE ) {
		ri.Error( ERR_FATAL, "Vulkan: incorrect state during image upload" );
	}
	if ( final ) {
		// final submission before recording
		submit_info.signalSemaphoreCount = 1;
		submit_info.pSignalSemaphores = &vk.image_uploaded2;
		vk.image_uploaded = vk.image_uploaded2;
		VK_CHECK( qvkQueueSubmit( vk.queue, 1, &submit_info, vk.aux_fence ) );
		vk.aux_fence_wait = qtrue;
	} else {
		// if submission before another upload then do explicit wait
		submit_info.signalSemaphoreCount = 0;
		submit_info.pSignalSemaphores = NULL;
		VK_CHECK( qvkQueueSubmit( vk.queue, 1, &submit_info, vk.aux_fence ) );
		res = qvkWaitForFences( vk.device, 1, &vk.aux_fence, VK_TRUE, 5 * 1000000000ULL );
		if ( res != VK_SUCCESS ) {
			ri.Error( ERR_FATAL, "vkWaitForFences() failed with %s at %s", vk_result_string( res ), __func__ );
		}
		qvkResetFences( vk.device, 1, &vk.aux_fence );
		VK_CHECK( qvkResetCommandBuffer( vk.staging_command_buffer, 0 ) );
	}
}
#endif // USE_UPLOAD_QUEUE


static void vk_alloc_staging_buffer( VkDeviceSize size )
{
	VkBufferCreateInfo buffer_desc;
	VkMemoryRequirements memory_requirements;
	VkMemoryAllocateInfo alloc_info;
	uint32_t memory_type;
	void *data;

	vk_clean_staging_buffer();

	vk.staging_buffer.size = MAX( size, STAGING_BUFFER_SIZE );
	vk.staging_buffer.size = PAD( vk.staging_buffer.size, 1024 * 1024 );

	buffer_desc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buffer_desc.pNext = NULL;
	buffer_desc.flags = 0;
	buffer_desc.size = vk.staging_buffer.size;
	buffer_desc.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	buffer_desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	buffer_desc.queueFamilyIndexCount = 0;
	buffer_desc.pQueueFamilyIndices = NULL;
	VK_CHECK(qvkCreateBuffer(vk.device, &buffer_desc, NULL, &vk.staging_buffer.handle));

	qvkGetBufferMemoryRequirements( vk.device, vk.staging_buffer.handle, &memory_requirements );

	memory_type = find_memory_type( memory_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );

	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.pNext = NULL;
	alloc_info.allocationSize = memory_requirements.size;
	alloc_info.memoryTypeIndex = memory_type;

	VK_CHECK(qvkAllocateMemory(vk.device, &alloc_info, NULL, &vk.staging_buffer.memory));
	VK_CHECK(qvkBindBufferMemory(vk.device, vk.staging_buffer.handle, vk.staging_buffer.memory, 0));

	VK_CHECK(qvkMapMemory(vk.device, vk.staging_buffer.memory, 0, VK_WHOLE_SIZE, 0, &data));
	vk.staging_buffer.ptr = (byte*)data;
#ifdef USE_UPLOAD_QUEUE
	vk.staging_buffer.offset = 0;
#endif
	SET_OBJECT_NAME( vk.staging_buffer.handle, "staging buffer", VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT );
	SET_OBJECT_NAME( vk.staging_buffer.memory, "staging buffer memory", VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT );
}


#ifdef USE_VK_VALIDATION
static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT object_type, uint64_t object, size_t location,
	int32_t message_code, const char* layer_prefix, const char* message, void* user_data) {
#ifdef __ANDROID__
	int priority = ANDROID_LOG_INFO;
	if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) {
		priority = ANDROID_LOG_ERROR;
	} else if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT) {
		priority = ANDROID_LOG_WARN;
	}
	__android_log_print(priority, "VkValidation", "[%s] %s", layer_prefix, message);
#endif
	return VK_FALSE;
}
#endif



static VkFormat get_depth_format( VkPhysicalDevice physical_device ) {
	VkFormatProperties props;
	VkFormat formats[2];
	int i;

	if ( glConfig.stencilBits > 0 ) {
		formats[0] = glConfig.depthBits == 16 ? VK_FORMAT_D16_UNORM_S8_UINT : VK_FORMAT_D24_UNORM_S8_UINT;
		formats[1] = VK_FORMAT_D32_SFLOAT_S8_UINT;
	} else {
		formats[0] = glConfig.depthBits == 16 ? VK_FORMAT_D16_UNORM : VK_FORMAT_X8_D24_UNORM_PACK32;
		formats[1] = VK_FORMAT_D32_SFLOAT;
	}

	for ( i = 0; i < ARRAY_LEN( formats ); i++ ) {
		qvkGetPhysicalDeviceFormatProperties( physical_device, formats[i], &props );
		if ( ( props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT ) != 0 ) {
			return formats[i];
		}
	}

	ri.Error( ERR_FATAL, "get_depth_format: failed to find depth attachment format" );
	return VK_FORMAT_UNDEFINED; // never get here
}


// Check if we can use vkCmdBlitImage for the given source and destination image formats.
static qboolean vk_blit_enabled( VkPhysicalDevice physical_device, const VkFormat srcFormat, const VkFormat dstFormat )
{
	VkFormatProperties formatProps;

	qvkGetPhysicalDeviceFormatProperties( physical_device, srcFormat, &formatProps );
	if ( ( formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT ) == 0 ) {
		return qfalse;
	}

	qvkGetPhysicalDeviceFormatProperties( physical_device, dstFormat, &formatProps );
	if ( ( formatProps.linearTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT ) == 0 ) {
		return qfalse;
	}

	return qtrue;
}


static VkFormat get_hdr_format( VkFormat base_format )
{
	if ( r_fbo->integer == 0 ) {
		return base_format;
	}

	switch ( r_hdr->integer ) {
		case -1: return VK_FORMAT_B4G4R4A4_UNORM_PACK16;
		case 1: return VK_FORMAT_R16G16B16A16_UNORM;
		default: return base_format;
	}
}


/*
 * vk_get_unorm_format - Convert sRGB format to corresponding UNORM format
 *
 * When writing to an sRGB framebuffer attachment, Vulkan automatically applies
 * linear-to-sRGB conversion. For the gamma pass, we want to write sRGB-encoded
 * values directly (as Quake3e does), so we use UNORM views to disable this
 * automatic conversion.
 */
static VkFormat vk_get_unorm_format( VkFormat format )
{
	switch ( format ) {
		case VK_FORMAT_R8G8B8A8_SRGB:  return VK_FORMAT_R8G8B8A8_UNORM;
		case VK_FORMAT_B8G8R8A8_SRGB:  return VK_FORMAT_B8G8R8A8_UNORM;
		case VK_FORMAT_A8B8G8R8_SRGB_PACK32:  return VK_FORMAT_A8B8G8R8_UNORM_PACK32;
		default: return format;  // Already UNORM or other format
	}
}




static void setup_surface_formats( VkPhysicalDevice physical_device )
{
	vk.depth_format = get_depth_format( physical_device );

	vk.color_format = get_hdr_format( vk.base_format.format );

	vk.capture_format = VK_FORMAT_R8G8B8A8_UNORM;

	vk.bloom_format = vk.base_format.format;

	vk.blitEnabled = vk_blit_enabled( physical_device, vk.color_format, vk.capture_format );

	if ( !vk.blitEnabled )
	{
		vk.capture_format = vk.color_format;
	}
}


static const char *renderer_name( const VkPhysicalDeviceProperties *props ) {
	static char buf[sizeof( props->deviceName ) + 64];
	const char *device_type;

	switch ( props->deviceType ) {
		case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: device_type = "Integrated"; break;
		case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: device_type = "Discrete"; break;
		case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: device_type = "Virtual"; break;
		case VK_PHYSICAL_DEVICE_TYPE_CPU: device_type = "CPU"; break;
		default: device_type = "OTHER"; break;
	}

	Com_sprintf( buf, sizeof( buf ), "%s %s, 0x%04x",
		device_type, props->deviceName, props->deviceID );

	return buf;
}


#define INIT_INSTANCE_FUNCTION(func) \
	q##func = /*(PFN_ ## func)*/ ri.VK_GetInstanceProcAddr(vk_instance, #func); \
	if (q##func == NULL) {											\
		ri.Error(ERR_FATAL, "Failed to find entrypoint %s", #func);	\
	}

#define INIT_INSTANCE_FUNCTION_EXT(func) \
	q##func = /*(PFN_ ## func)*/ ri.VK_GetInstanceProcAddr(vk_instance, #func);


#define INIT_DEVICE_FUNCTION(func) \
	q##func = (PFN_ ## func) qvkGetDeviceProcAddr(vk.device, #func);\
	if (q##func == NULL) {											\
		ri.Error(ERR_FATAL, "Failed to find entrypoint %s", #func);	\
	}

#define INIT_DEVICE_FUNCTION_EXT(func) \
	q##func = (PFN_ ## func) qvkGetDeviceProcAddr(vk.device, #func);


static void vk_destroy_instance( void ) {
#ifdef USE_VK_VALIDATION
	// Destroy debug callback before instance
	if ( vk_debug_callback != VK_NULL_HANDLE ) {
		if ( qvkDestroyDebugReportCallbackEXT != NULL ) {
			qvkDestroyDebugReportCallbackEXT( vk_instance, vk_debug_callback, NULL );
		}
		vk_debug_callback = VK_NULL_HANDLE;
	}
#endif

	// Destroy the desktop mirror surface (we created this)
	if ( vk_surface != VK_NULL_HANDLE ) {
		if ( qvkDestroySurfaceKHR != NULL ) {
			qvkDestroySurfaceKHR( vk_instance, vk_surface, NULL );
		}
		vk_surface = VK_NULL_HANDLE;
	}

	// DO NOT destroy vk_instance: it's owned by the VR layer (XR_KHR_vulkan_enable2)
	// The VR layer's VR_Vulkan_Shutdown() handles instance/device destruction
	vk_instance = VK_NULL_HANDLE;
}


static void init_vulkan_library( void )
{
	const VR_VulkanDeviceInfo* xrDevice =
		(const VR_VulkanDeviceInfo*)ri.VR_Vulkan_GetDeviceInfo();

	if ( !xrDevice || xrDevice->device == VK_NULL_HANDLE ) {
		ri.Error( ERR_FATAL, "[VK] VR Vulkan device not available" );
		return;
	}

	Com_Memset( &vk, 0, sizeof( vk ) );

	// Set up from XR-provided objects
	vk.xrMode = qtrue;
	vk.xrInstance = xrDevice->instance;
	vk.physical_device = xrDevice->physicalDevice;
	vk.device = xrDevice->device;
	vk.queue_family_index = xrDevice->queueFamilyIndex;
	vk_instance = xrDevice->instance;
	// VK_EXT_fragment_density_map is enabled by the VR layer when the runtime can foveate
	vk.xr.fdmSupported = xrDevice->fragmentDensityMap ? qtrue : qfalse;
	vk.xr.tileProperties = xrDevice->tileProperties ? qtrue : qfalse;
	// Gates the vkDebugMarkerSetObjectNameEXT load below, and so every SET_OBJECT_NAME
	vk.debugMarkers = xrDevice->debugMarkers ? qtrue : qfalse;
	// The density map is written at the finest granularity the hardware reads
	vk.xr.fdmTexelWidth = xrDevice->minDensityTexelWidth;
	vk.xr.fdmTexelHeight = xrDevice->minDensityTexelHeight;

	ri.Printf( PRINT_ALL, "[VK] Using VR-provided Vulkan device (fragment density map: %s)\n",
		vk.xr.fdmSupported ? "yes" : "no" );

	// Load instance-level function pointers from the XR-provided instance
	INIT_INSTANCE_FUNCTION( vkCreateDevice )
	INIT_INSTANCE_FUNCTION( vkDestroyInstance )
	INIT_INSTANCE_FUNCTION( vkEnumerateDeviceExtensionProperties )
	INIT_INSTANCE_FUNCTION( vkEnumeratePhysicalDevices )
	INIT_INSTANCE_FUNCTION( vkGetDeviceProcAddr )
	INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceFeatures )
	INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceFeatures2 )
	INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceFormatProperties )
	INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceMemoryProperties )
	INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceProperties )
	INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceQueueFamilyProperties )

#ifdef USE_VK_VALIDATION
	// Load debug callback extension functions
	INIT_INSTANCE_FUNCTION_EXT( vkCreateDebugReportCallbackEXT )
	INIT_INSTANCE_FUNCTION_EXT( vkDestroyDebugReportCallbackEXT )

	// Register validation layer callback
	if ( qvkCreateDebugReportCallbackEXT ) {
		VkDebugReportCallbackCreateInfoEXT debug_callback_info = {
			.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT,
			.pNext = NULL,
			.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT,
			.pfnCallback = debug_callback,
			.pUserData = NULL,
		};
		VkResult result = qvkCreateDebugReportCallbackEXT( vk_instance, &debug_callback_info, NULL, &vk_debug_callback );
		if ( result == VK_SUCCESS ) {
			ri.Printf( PRINT_ALL, "[VK] Validation layer callback registered\n" );
		} else {
			ri.Printf( PRINT_WARNING, "[VK] Failed to register validation callback: %d\n", result );
		}
	} else {
		ri.Printf( PRINT_WARNING, "[VK] vkCreateDebugReportCallbackEXT not available - validation layers may not be installed\n" );
	}
#endif

	// Quest/Android: no desktop surface, VR renders directly to XR swapchains
	vk_surface = VK_NULL_HANDLE;
	// Use a common VR format (R8G8B8A8_SRGB is typical for Quest)
	vk.base_format.format = VK_FORMAT_R8G8B8A8_SRGB;
	vk.base_format.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	vk.present_format = vk.base_format;
	setup_surface_formats( vk.physical_device );

	//
	// Get device level functions.
	//
	INIT_DEVICE_FUNCTION(vkAllocateCommandBuffers)
	INIT_DEVICE_FUNCTION(vkAllocateDescriptorSets)
	INIT_DEVICE_FUNCTION(vkAllocateMemory)
	INIT_DEVICE_FUNCTION(vkBeginCommandBuffer)
	INIT_DEVICE_FUNCTION(vkBindBufferMemory)
	INIT_DEVICE_FUNCTION(vkBindImageMemory)
	INIT_DEVICE_FUNCTION(vkCmdBeginRenderPass)
	INIT_DEVICE_FUNCTION(vkCmdBindDescriptorSets)
	INIT_DEVICE_FUNCTION(vkCmdBindIndexBuffer)
	INIT_DEVICE_FUNCTION(vkCmdBindPipeline)
	INIT_DEVICE_FUNCTION(vkCmdBindVertexBuffers)
	INIT_DEVICE_FUNCTION(vkCmdBlitImage)
	INIT_DEVICE_FUNCTION(vkCmdClearAttachments)
	INIT_DEVICE_FUNCTION(vkCmdClearColorImage)
	INIT_DEVICE_FUNCTION(vkCmdClearDepthStencilImage)
	INIT_DEVICE_FUNCTION(vkCmdCopyBuffer)
	INIT_DEVICE_FUNCTION(vkCmdCopyBufferToImage)
	INIT_DEVICE_FUNCTION(vkCmdCopyImage)
	INIT_DEVICE_FUNCTION(vkCmdDraw)
	INIT_DEVICE_FUNCTION(vkCmdDrawIndexed)
	INIT_DEVICE_FUNCTION(vkCmdEndRenderPass)
	INIT_DEVICE_FUNCTION(vkCmdNextSubpass)
	INIT_DEVICE_FUNCTION(vkCmdPipelineBarrier)
	INIT_DEVICE_FUNCTION(vkCmdPushConstants)
	INIT_DEVICE_FUNCTION(vkCmdSetDepthBias)
	INIT_DEVICE_FUNCTION(vkCmdSetScissor)
	INIT_DEVICE_FUNCTION(vkCmdSetViewport)
	INIT_DEVICE_FUNCTION(vkCreateBuffer)
	INIT_DEVICE_FUNCTION(vkCreateCommandPool)
	INIT_DEVICE_FUNCTION(vkCreateDescriptorPool)
	INIT_DEVICE_FUNCTION(vkCreateDescriptorSetLayout)
	INIT_DEVICE_FUNCTION(vkCreateFence)
	INIT_DEVICE_FUNCTION(vkCreateFramebuffer)
	INIT_DEVICE_FUNCTION(vkCreateGraphicsPipelines)
	INIT_DEVICE_FUNCTION(vkCreateImage)
	INIT_DEVICE_FUNCTION(vkCreateImageView)
	INIT_DEVICE_FUNCTION(vkCreatePipelineCache)
	INIT_DEVICE_FUNCTION(vkCreatePipelineLayout)
	INIT_DEVICE_FUNCTION(vkCreateRenderPass)
	INIT_DEVICE_FUNCTION(vkCreateSampler)
	INIT_DEVICE_FUNCTION(vkCreateSemaphore)
	INIT_DEVICE_FUNCTION(vkCreateShaderModule)
	INIT_DEVICE_FUNCTION(vkDestroyBuffer)
	INIT_DEVICE_FUNCTION(vkDestroyCommandPool)
	INIT_DEVICE_FUNCTION(vkDestroyDescriptorPool)
	INIT_DEVICE_FUNCTION(vkDestroyDescriptorSetLayout)
	INIT_DEVICE_FUNCTION(vkDestroyDevice)
	INIT_DEVICE_FUNCTION(vkDestroyFence)
	INIT_DEVICE_FUNCTION(vkDestroyFramebuffer)
	INIT_DEVICE_FUNCTION(vkDestroyImage)
	INIT_DEVICE_FUNCTION(vkDestroyImageView)
	INIT_DEVICE_FUNCTION(vkDestroyPipeline)
	INIT_DEVICE_FUNCTION(vkDestroyPipelineCache)
	INIT_DEVICE_FUNCTION(vkDestroyPipelineLayout)
	INIT_DEVICE_FUNCTION(vkDestroyRenderPass)
	INIT_DEVICE_FUNCTION(vkDestroySampler)
	INIT_DEVICE_FUNCTION(vkDestroySemaphore)
	INIT_DEVICE_FUNCTION(vkDestroyShaderModule)
	INIT_DEVICE_FUNCTION(vkDeviceWaitIdle)
	INIT_DEVICE_FUNCTION(vkEndCommandBuffer)
	INIT_DEVICE_FUNCTION(vkFlushMappedMemoryRanges)
	INIT_DEVICE_FUNCTION(vkFreeCommandBuffers)
	INIT_DEVICE_FUNCTION(vkFreeDescriptorSets)
	INIT_DEVICE_FUNCTION(vkFreeMemory)
	INIT_DEVICE_FUNCTION(vkGetBufferMemoryRequirements)
	INIT_DEVICE_FUNCTION(vkGetDeviceQueue)
	INIT_DEVICE_FUNCTION(vkGetImageMemoryRequirements)
	INIT_DEVICE_FUNCTION(vkGetImageSubresourceLayout)
	INIT_DEVICE_FUNCTION(vkInvalidateMappedMemoryRanges)
	INIT_DEVICE_FUNCTION(vkMapMemory)
	INIT_DEVICE_FUNCTION(vkQueueSubmit)
	INIT_DEVICE_FUNCTION(vkQueueWaitIdle)
	INIT_DEVICE_FUNCTION(vkResetCommandBuffer)
	INIT_DEVICE_FUNCTION(vkResetDescriptorPool)
	INIT_DEVICE_FUNCTION(vkResetFences)
	INIT_DEVICE_FUNCTION(vkUnmapMemory)
	INIT_DEVICE_FUNCTION(vkUpdateDescriptorSets)
	INIT_DEVICE_FUNCTION(vkWaitForFences)

	if ( vk.dedicatedAllocation ) {
		INIT_DEVICE_FUNCTION_EXT(vkGetBufferMemoryRequirements2KHR);
		INIT_DEVICE_FUNCTION_EXT(vkGetImageMemoryRequirements2KHR);
		if ( !qvkGetBufferMemoryRequirements2KHR || !qvkGetImageMemoryRequirements2KHR ) {
			vk.dedicatedAllocation = qfalse;
		}
	}

	if ( vk.debugMarkers ) {
		INIT_DEVICE_FUNCTION_EXT(vkDebugMarkerSetObjectNameEXT)
	}

	if ( vk.xr.tileProperties ) {
		INIT_DEVICE_FUNCTION_EXT(vkGetFramebufferTilePropertiesQCOM)
	}

	// Check multiview support for VR single-pass stereo rendering
	if ( qvkGetPhysicalDeviceFeatures2 ) {
		VkPhysicalDeviceMultiviewFeatures multiviewFeatures = {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES,
			.pNext = NULL,
		};
		VkPhysicalDeviceFeatures2 features2 = {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
			.pNext = &multiviewFeatures,
		};
		qvkGetPhysicalDeviceFeatures2( vk.physical_device, &features2 );
		vk.multiviewSupported = multiviewFeatures.multiview ? qtrue : qfalse;
		vk.samplerAnisotropy = features2.features.samplerAnisotropy ? qtrue : qfalse;
		// depth clamp is enabled at device creation (vrvk passes the full queried
		// features2); record support here for the z-fail shadow pipeline.
		vk.depthClamp = features2.features.depthClamp ? qtrue : qfalse;
		// storage-buffer writes from vertex+fragment stages are likewise already
		// enabled at device creation (vrvk passes the full queried features2); the
		// flare visibility probe (dot.vert/dot.frag) needs both. Without this flag
		// R_ClearFlares never initializes the flare pool and no flare can ever be
		// allocated.
		vk.fragmentStores = ( features2.features.fragmentStoresAndAtomics &&
			features2.features.vertexPipelineStoresAndAtomics ) ? qtrue : qfalse;
		ri.Printf( PRINT_ALL, "...VK_KHR_multiview: %s\n",
			vk.multiviewSupported ? "supported" : "not supported" );
		ri.Printf( PRINT_ALL, "...samplerAnisotropy: %s\n",
			vk.samplerAnisotropy ? "supported" : "not supported" );
		ri.Printf( PRINT_ALL, "...fragmentStores: %s\n",
			vk.fragmentStores ? "supported" : "not supported" );
	} else {
		VkPhysicalDeviceFeatures features;
		qvkGetPhysicalDeviceFeatures( vk.physical_device, &features );
		vk.samplerAnisotropy = features.samplerAnisotropy ? qtrue : qfalse;
		vk.depthClamp = features.depthClamp ? qtrue : qfalse;
		vk.fragmentStores = ( features.fragmentStoresAndAtomics &&
			features.vertexPipelineStoresAndAtomics ) ? qtrue : qfalse;
		vk.multiviewSupported = qfalse;
		ri.Printf( PRINT_WARNING, "...vkGetPhysicalDeviceFeatures2 not available, multiview disabled\n" );
		ri.Printf( PRINT_ALL, "...samplerAnisotropy: %s\n",
			vk.samplerAnisotropy ? "supported" : "not supported" );
	}
}

#undef INIT_INSTANCE_FUNCTION
#undef INIT_DEVICE_FUNCTION
#undef INIT_DEVICE_FUNCTION_EXT

static void deinit_instance_functions( void )
{
	qvkCreateInstance = NULL;
	qvkEnumerateInstanceExtensionProperties = NULL;

	// instance functions:
	qvkCreateDevice = NULL;
	qvkDestroyInstance = NULL;
	qvkEnumerateDeviceExtensionProperties = NULL;
	qvkEnumeratePhysicalDevices = NULL;
	qvkGetDeviceProcAddr = NULL;
	qvkGetPhysicalDeviceFeatures = NULL;
	qvkGetPhysicalDeviceFeatures2 = NULL;
	qvkGetPhysicalDeviceFormatProperties = NULL;
	qvkGetPhysicalDeviceMemoryProperties = NULL;
	qvkGetPhysicalDeviceProperties = NULL;
	qvkGetPhysicalDeviceQueueFamilyProperties = NULL;
	qvkDestroySurfaceKHR = NULL;
	qvkGetPhysicalDeviceSurfaceCapabilitiesKHR = NULL;
	qvkGetPhysicalDeviceSurfaceFormatsKHR = NULL;
	qvkGetPhysicalDeviceSurfacePresentModesKHR = NULL;
	qvkGetPhysicalDeviceSurfaceSupportKHR = NULL;
#ifdef USE_VK_VALIDATION
	qvkCreateDebugReportCallbackEXT = NULL;
	qvkDestroyDebugReportCallbackEXT = NULL;
#endif
}


static void deinit_device_functions( void )
{
	// device functions:
	qvkAllocateCommandBuffers					= NULL;
	qvkAllocateDescriptorSets					= NULL;
	qvkAllocateMemory							= NULL;
	qvkBeginCommandBuffer						= NULL;
	qvkBindBufferMemory							= NULL;
	qvkBindImageMemory							= NULL;
	qvkCmdBeginRenderPass						= NULL;
	qvkCmdBindDescriptorSets					= NULL;
	qvkCmdBindIndexBuffer						= NULL;
	qvkCmdBindPipeline							= NULL;
	qvkCmdBindVertexBuffers						= NULL;
	qvkCmdBlitImage								= NULL;
	qvkCmdClearAttachments						= NULL;
	qvkCmdClearColorImage						= NULL;
	qvkCmdClearDepthStencilImage				= NULL;
	qvkCmdCopyBuffer							= NULL;
	qvkCmdCopyBufferToImage						= NULL;
	qvkCmdCopyImage								= NULL;
	qvkCmdDraw									= NULL;
	qvkCmdDrawIndexed							= NULL;
	qvkCmdEndRenderPass							= NULL;
	qvkCmdNextSubpass							= NULL;
	qvkCmdPipelineBarrier						= NULL;
	qvkCmdPushConstants							= NULL;
	qvkCmdSetDepthBias							= NULL;
	qvkCmdSetScissor							= NULL;
	qvkCmdSetViewport							= NULL;
	qvkCreateBuffer								= NULL;
	qvkCreateCommandPool						= NULL;
	qvkCreateDescriptorPool						= NULL;
	qvkCreateDescriptorSetLayout				= NULL;
	qvkCreateFence								= NULL;
	qvkCreateFramebuffer						= NULL;
	qvkCreateGraphicsPipelines					= NULL;
	qvkCreateImage								= NULL;
	qvkCreateImageView							= NULL;
	qvkCreatePipelineCache						= NULL;
	qvkCreatePipelineLayout						= NULL;
	qvkCreateRenderPass							= NULL;
	qvkCreateSampler							= NULL;
	qvkCreateSemaphore							= NULL;
	qvkCreateShaderModule						= NULL;
	qvkDestroyBuffer							= NULL;
	qvkDestroyCommandPool						= NULL;
	qvkDestroyDescriptorPool					= NULL;
	qvkDestroyDescriptorSetLayout				= NULL;
	qvkDestroyDevice							= NULL;
	qvkDestroyFence								= NULL;
	qvkDestroyFramebuffer						= NULL;
	qvkDestroyImage								= NULL;
	qvkDestroyImageView							= NULL;
	qvkDestroyPipeline							= NULL;
	qvkDestroyPipelineCache						= NULL;
	qvkDestroyPipelineLayout					= NULL;
	qvkDestroyRenderPass						= NULL;
	qvkDestroySampler							= NULL;
	qvkDestroySemaphore							= NULL;
	qvkDestroyShaderModule						= NULL;
	qvkDeviceWaitIdle							= NULL;
	qvkEndCommandBuffer							= NULL;
	qvkFlushMappedMemoryRanges					= NULL;
	qvkFreeCommandBuffers						= NULL;
	qvkFreeDescriptorSets						= NULL;
	qvkFreeMemory								= NULL;
	qvkGetBufferMemoryRequirements				= NULL;
	qvkGetDeviceQueue							= NULL;
	qvkGetImageMemoryRequirements				= NULL;
	qvkGetImageSubresourceLayout				= NULL;
	qvkInvalidateMappedMemoryRanges				= NULL;
	qvkMapMemory								= NULL;
	qvkQueueSubmit								= NULL;
	qvkQueueWaitIdle							= NULL;
	qvkResetCommandBuffer						= NULL;
	qvkResetDescriptorPool						= NULL;
	qvkResetFences								= NULL;
	qvkUnmapMemory								= NULL;
	qvkUpdateDescriptorSets						= NULL;
	qvkWaitForFences							= NULL;

	qvkGetBufferMemoryRequirements2KHR			= NULL;
	qvkGetImageMemoryRequirements2KHR			= NULL;

	qvkDebugMarkerSetObjectNameEXT				= NULL;
	qvkGetFramebufferTilePropertiesQCOM			= NULL;
}


static VkShaderModule SHADER_MODULE(const uint8_t *bytes, const int count) {
	VkShaderModuleCreateInfo desc;
	VkShaderModule module;

	if ( count % 4 != 0 ) {
		ri.Error( ERR_FATAL, "Vulkan: SPIR-V binary buffer size is not a multiple of 4" );
	}

	desc.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.codeSize = count;
	desc.pCode = (const uint32_t*)bytes;

	VK_CHECK(qvkCreateShaderModule(vk.device, &desc, NULL, &module));

	return module;
}


static void vk_create_layout_binding( int binding, VkDescriptorType type, VkShaderStageFlags flags, VkDescriptorSetLayout *layout )
{
	VkDescriptorSetLayoutBinding bind;
	VkDescriptorSetLayoutCreateInfo desc;

	bind.binding = binding;
	bind.descriptorType = type;
	bind.descriptorCount = 1;
	bind.stageFlags = flags;
	bind.pImmutableSamplers = NULL;

	desc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.bindingCount = 1;
	desc.pBindings = &bind;

	VK_CHECK( qvkCreateDescriptorSetLayout(vk.device, &desc, NULL, layout ) );
}


static void vk_create_4sampler_layout( VkDescriptorSetLayout *layout )
{
	VkDescriptorSetLayoutBinding bindings[4];
	VkDescriptorSetLayoutCreateInfo desc;
	int i;

	for ( i = 0; i < 4; i++ ) {
		bindings[i].binding = i;
		bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		bindings[i].descriptorCount = 1;
		bindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		bindings[i].pImmutableSamplers = NULL;
	}

	desc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.bindingCount = 4;
	desc.pBindings = bindings;

	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &desc, NULL, layout ) );
}


void vk_update_uniform_descriptor( VkDescriptorSet descriptor, VkBuffer buffer )
{
	VkDescriptorBufferInfo info[2];
	VkWriteDescriptorSet desc[2];
	int i;

	info[0].buffer = buffer;
	info[0].offset = 0;
	info[0].range = sizeof( vkUniform_t );

	info[1].buffer = buffer;
	info[1].offset = 0;
	info[1].range = sizeof( float ) * 32; // eyeProj[2]

	for ( i = 0; i < 2; i++ ) {
		desc[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		desc[i].pNext = NULL;
		desc[i].dstSet = descriptor;
		desc[i].dstBinding = i;
		desc[i].dstArrayElement = 0;
		desc[i].descriptorCount = 1;
		desc[i].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		desc[i].pImageInfo = NULL;
		desc[i].pBufferInfo = &info[i];
		desc[i].pTexelBufferView = NULL;
	}

	qvkUpdateDescriptorSets( vk.device, 2, desc, 0, NULL );
}


static VkSampler vk_find_sampler( const Vk_Sampler_Def *def ) {
	VkSamplerAddressMode address_mode;
	VkSamplerCreateInfo desc;
	VkSampler sampler;
	VkFilter mag_filter;
	VkFilter min_filter;
	VkSamplerMipmapMode mipmap_mode;
	float maxLod;
	int i;

	// Look for sampler among existing samplers.
	for ( i = 0; i < vk.samplers.count; i++ ) {
		const Vk_Sampler_Def *cur_def = &vk.samplers.def[i];
		if ( memcmp( cur_def, def, sizeof( *def ) ) == 0 ) {
			return vk.samplers.handle[i];
		}
	}

	// Create new sampler.
	if ( vk.samplers.count >= MAX_VK_SAMPLERS ) {
		ri.Error( ERR_DROP, "vk_find_sampler: MAX_VK_SAMPLERS hit\n" );
		// return VK_NULL_HANDLE;
	}

	address_mode = def->address_mode;

	if (def->gl_mag_filter == GL_NEAREST) {
		mag_filter = VK_FILTER_NEAREST;
	} else if (def->gl_mag_filter == GL_LINEAR) {
		mag_filter = VK_FILTER_LINEAR;
	} else {
		ri.Error(ERR_FATAL, "vk_find_sampler: invalid gl_mag_filter");
		return VK_NULL_HANDLE;
	}

	maxLod = vk.maxLod;

	if (def->gl_min_filter == GL_NEAREST) {
		min_filter = VK_FILTER_NEAREST;
		mipmap_mode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		maxLod = 0.25f; // used to emulate OpenGL's GL_LINEAR/GL_NEAREST minification filter
	} else if (def->gl_min_filter == GL_LINEAR) {
		min_filter = VK_FILTER_LINEAR;
		mipmap_mode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		maxLod = 0.25f; // used to emulate OpenGL's GL_LINEAR/GL_NEAREST minification filter
	} else if (def->gl_min_filter == GL_NEAREST_MIPMAP_NEAREST) {
		min_filter = VK_FILTER_NEAREST;
		mipmap_mode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	} else if (def->gl_min_filter == GL_LINEAR_MIPMAP_NEAREST) {
		min_filter = VK_FILTER_LINEAR;
		mipmap_mode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	} else if (def->gl_min_filter == GL_NEAREST_MIPMAP_LINEAR) {
		min_filter = VK_FILTER_NEAREST;
		mipmap_mode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	} else if (def->gl_min_filter == GL_LINEAR_MIPMAP_LINEAR) {
		min_filter = VK_FILTER_LINEAR;
		mipmap_mode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	} else {
		ri.Error(ERR_FATAL, "vk_find_sampler: invalid gl_min_filter");
		return VK_NULL_HANDLE;
	}

	if ( def->max_lod_1_0 ) {
		maxLod = 1.0f;
	}

	desc.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.magFilter = mag_filter;
	desc.minFilter = min_filter;
	desc.mipmapMode = mipmap_mode;
	desc.addressModeU = address_mode;
	desc.addressModeV = address_mode;
	desc.addressModeW = address_mode;
	desc.mipLodBias = 0.0f;

	if ( def->noAnisotropy || mag_filter == VK_FILTER_NEAREST || maxLod < 1.0f ) {
		desc.anisotropyEnable = VK_FALSE;
		desc.maxAnisotropy = 1.0f;
	} else {
		desc.anisotropyEnable = (r_ext_texture_filter_anisotropic->integer && vk.samplerAnisotropy) ? VK_TRUE : VK_FALSE;
		if ( desc.anisotropyEnable ) {
			desc.maxAnisotropy = MIN( r_ext_max_anisotropy->integer, vk.maxAnisotropy );
		}
	}

	desc.compareEnable = VK_FALSE;
	desc.compareOp = VK_COMPARE_OP_ALWAYS;
	desc.minLod = 0.0f;
	desc.maxLod = (maxLod == vk.maxLod) ? VK_LOD_CLAMP_NONE : maxLod;
	desc.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
	desc.unnormalizedCoordinates = VK_FALSE;

	VK_CHECK( qvkCreateSampler( vk.device, &desc, NULL, &sampler ) );

	SET_OBJECT_NAME( sampler, va( "image sampler %i", vk.samplers.count ), VK_DEBUG_REPORT_OBJECT_TYPE_SAMPLER_EXT );

	vk.samplers.def[ vk.samplers.count ] = *def;
	vk.samplers.handle[ vk.samplers.count ] = sampler;
	vk.samplers.count++;

	return sampler;
}


void vk_destroy_samplers( void )
{
	int i;

	for ( i = 0; i < vk.samplers.count; i++ ) {
		qvkDestroySampler( vk.device, vk.samplers.handle[i], NULL );
		memset( &vk.samplers.def[i], 0x0, sizeof( vk.samplers.def[i] ) );
		vk.samplers.handle[i] = VK_NULL_HANDLE;
	}

	vk.samplers.count = 0;
}


void vk_update_attachment_descriptors( void ) {
	VkDescriptorImageInfo info;
	VkWriteDescriptorSet desc;
	Vk_Sampler_Def sd;

	Com_Memset( &sd, 0, sizeof( sd ) );
	sd.gl_mag_filter = sd.gl_min_filter = vk.blitFilter;
	sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sd.max_lod_1_0 = qtrue;
	sd.noAnisotropy = qtrue;

	info.sampler = vk_find_sampler( &sd );
	info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	desc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	desc.dstBinding = 0;
	desc.dstArrayElement = 0;
	desc.descriptorCount = 1;
	desc.pNext = NULL;
	desc.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	desc.pImageInfo = &info;
	desc.pBufferInfo = NULL;
	desc.pTexelBufferView = NULL;

	// Note: vk.color_descriptor removed: subpass mode uses input attachments

	// screenmap
	if ( vk.screenMap.color_image_view && vk.screenMap.color_descriptor != VK_NULL_HANDLE )
	{
		sd.gl_mag_filter = sd.gl_min_filter = GL_LINEAR;
		sd.max_lod_1_0 = qfalse;
		sd.noAnisotropy = qtrue;

		info.sampler = vk_find_sampler( &sd );
		info.imageView = vk.screenMap.color_image_view;
		desc.dstSet = vk.screenMap.color_descriptor;

		qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );
	}

	// bloom images
	if ( r_bloom->integer )
	{
		uint32_t i;
		for ( i = 0; i < ARRAY_LEN( vk.bloom_image_descriptor ); i++ )
		{
			if ( vk.bloom_image_view[i] == VK_NULL_HANDLE ||
				 vk.bloom_image_descriptor[i] == VK_NULL_HANDLE )
				continue;

			info.imageView = vk.bloom_image_view[i];
			desc.dstSet = vk.bloom_image_descriptor[i];

			qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );
		}
	}
}


static void vk_update_bloom_blur_combined_descriptor( void )
{
	VkDescriptorImageInfo imageInfo[4];
	VkWriteDescriptorSet writes[4];
	Vk_Sampler_Def samplerDef;
	int i;
	int blurResultIndices[4] = { 2, 4, 6, 8 };  // vertical outputs; framebuffers.blur[n] targets view[n+1]

	if ( vk.bloom_blur_combined_descriptor == VK_NULL_HANDLE )
		return;

	Com_Memset( &samplerDef, 0, sizeof( samplerDef ) );
	samplerDef.gl_mag_filter = GL_LINEAR;
	samplerDef.gl_min_filter = GL_LINEAR;
	samplerDef.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerDef.max_lod_1_0 = qtrue;
	samplerDef.noAnisotropy = qtrue;

	for ( i = 0; i < 4; i++ ) {
		int idx = blurResultIndices[i];
		if ( vk.bloom_image_view[idx] == VK_NULL_HANDLE )
			return;  // Not ready yet

		Com_Memset( &imageInfo[i], 0, sizeof( imageInfo[i] ) );
		imageInfo[i].sampler = vk_find_sampler( &samplerDef );
		imageInfo[i].imageView = vk.bloom_image_view[idx];
		imageInfo[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		Com_Memset( &writes[i], 0, sizeof( writes[i] ) );
		writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[i].dstSet = vk.bloom_blur_combined_descriptor;
		writes[i].dstBinding = i;
		writes[i].dstArrayElement = 0;
		writes[i].descriptorCount = 1;
		writes[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[i].pImageInfo = &imageInfo[i];
	}

	qvkUpdateDescriptorSets( vk.device, 4, writes, 0, NULL );
}


void vk_init_descriptors( void )
{
	VkDescriptorSetAllocateInfo alloc;
	VkDescriptorBufferInfo info;
	VkWriteDescriptorSet desc;
	uint32_t i;

	alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc.pNext = NULL;
	alloc.descriptorPool = vk.descriptor_pool;
	alloc.descriptorSetCount = 1;
	alloc.pSetLayouts = &vk.set_layout_storage;

	VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.storage.descriptor ) );

	info.buffer = vk.storage.buffer;
	info.offset = 0;
	info.range = 2 * sizeof( uint32_t );  // passed and total, see dot.frag

	desc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	desc.dstSet = vk.storage.descriptor;
	desc.dstBinding = 0;
	desc.dstArrayElement = 0;
	desc.descriptorCount = 1;
	desc.pNext = NULL;
	desc.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
	desc.pImageInfo = NULL;
	desc.pBufferInfo = &info;
	desc.pTexelBufferView = NULL;

	qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );

	// allocated and update descriptor set
	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ )
	{
		alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		alloc.pNext = NULL;
		alloc.descriptorPool = vk.descriptor_pool;
		alloc.descriptorSetCount = 1;
		alloc.pSetLayouts = &vk.set_layout_uniform;

		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.tess[i].uniform_descriptor ) );

		vk_update_uniform_descriptor( vk.tess[ i ].uniform_descriptor, vk.tess[ i ].vertex_buffer );

		SET_OBJECT_NAME( vk.tess[ i ].uniform_descriptor, va( "uniform descriptor %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT );
	}

	// FBO mode: allocate descriptors for bloom blur passes and screenmap (portal/mirror)
	// Note: vk.color_descriptor removed: subpass mode uses input attachments instead
	if ( vk.fboActive )
	{
		alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		alloc.pNext = NULL;
		alloc.descriptorPool = vk.descriptor_pool;
		alloc.descriptorSetCount = 1;
		alloc.pSetLayouts = &vk.set_layout_sampler;

		if ( r_bloom->integer )
		{
			for ( i = 0; i < ARRAY_LEN( vk.bloom_image_descriptor ); i++ )
			{
				VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.bloom_image_descriptor[i] ) );
			}
		}

		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.screenMap.color_descriptor ) ); // screenmap

		vk_update_attachment_descriptors();
	}

	vk.descriptorsReady = qtrue;
}


static void vk_release_geometry_buffers( void )
{
	int i;

	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		qvkDestroyBuffer( vk.device, vk.tess[i].vertex_buffer, NULL );
		vk.tess[i].vertex_buffer = VK_NULL_HANDLE;
	}

	qvkFreeMemory( vk.device, vk.geometry_buffer_memory, NULL );
	vk.geometry_buffer_memory = VK_NULL_HANDLE;
}


static void vk_create_geometry_buffers( VkDeviceSize size )
{
	VkMemoryRequirements vb_memory_requirements;
	VkMemoryAllocateInfo alloc_info;
	VkBufferCreateInfo desc;
	VkDeviceSize vertex_buffer_offset;
	uint32_t memory_type_bits;
	uint32_t memory_type;
	void *data;
	int i;

	desc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	desc.queueFamilyIndexCount = 0;
	desc.pQueueFamilyIndices = NULL;

	Com_Memset( &vb_memory_requirements, 0, sizeof( vb_memory_requirements ) );

	for ( i = 0 ; i < NUM_COMMAND_BUFFERS; i++ ) {
		desc.size = size;
		desc.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		VK_CHECK( qvkCreateBuffer( vk.device, &desc, NULL, &vk.tess[i].vertex_buffer ) );

		qvkGetBufferMemoryRequirements( vk.device, vk.tess[i].vertex_buffer, &vb_memory_requirements );
	}

	memory_type_bits = vb_memory_requirements.memoryTypeBits;
	memory_type = find_memory_type( memory_type_bits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );

	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.pNext = NULL;
	alloc_info.allocationSize = vb_memory_requirements.size * NUM_COMMAND_BUFFERS;
	alloc_info.memoryTypeIndex = memory_type;

	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &vk.geometry_buffer_memory ) );
	VK_CHECK( qvkMapMemory( vk.device, vk.geometry_buffer_memory, 0, VK_WHOLE_SIZE, 0, &data ) );

	vertex_buffer_offset = 0;

	for ( i = 0 ; i < NUM_COMMAND_BUFFERS; i++ ) {
		qvkBindBufferMemory( vk.device, vk.tess[i].vertex_buffer, vk.geometry_buffer_memory, vertex_buffer_offset );
		vk.tess[i].vertex_buffer_ptr = (byte*)data + vertex_buffer_offset;
		vk.tess[i].vertex_buffer_offset = 0;
		vertex_buffer_offset += vb_memory_requirements.size;

		SET_OBJECT_NAME( vk.tess[i].vertex_buffer, va( "geometry buffer %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT );
	}

	SET_OBJECT_NAME( vk.geometry_buffer_memory, "geometry buffer memory", VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT );

	vk.geometry_buffer_size = vb_memory_requirements.size;

	Com_Memset( &vk.stats, 0, sizeof( vk.stats ) );
}


static void vk_create_storage_buffer( uint32_t size )
{
	VkMemoryRequirements memory_requirements;
	VkMemoryAllocateInfo alloc_info;
	VkBufferCreateInfo desc;
	uint32_t memory_type_bits;
	uint32_t memory_type;

	desc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	desc.queueFamilyIndexCount = 0;
	desc.pQueueFamilyIndices = NULL;

	Com_Memset( &memory_requirements, 0, sizeof( memory_requirements ) );

	desc.size = size;
	desc.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	VK_CHECK( qvkCreateBuffer( vk.device, &desc, NULL, &vk.storage.buffer ) );

	qvkGetBufferMemoryRequirements( vk.device, vk.storage.buffer, &memory_requirements );

	memory_type_bits = memory_requirements.memoryTypeBits;
	memory_type = find_memory_type( memory_type_bits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );

	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.pNext = NULL;
	alloc_info.allocationSize = memory_requirements.size;
	alloc_info.memoryTypeIndex = memory_type;

	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &vk.storage.memory ) );
	VK_CHECK( qvkMapMemory( vk.device, vk.storage.memory, 0, VK_WHOLE_SIZE, 0, (void**)&vk.storage.buffer_ptr ) );

	Com_Memset( vk.storage.buffer_ptr, 0, memory_requirements.size );

	qvkBindBufferMemory( vk.device, vk.storage.buffer, vk.storage.memory, 0 );

	SET_OBJECT_NAME( vk.storage.buffer, "storage buffer", VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT );
	SET_OBJECT_NAME( vk.storage.descriptor, "storage buffer", VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT );
	SET_OBJECT_NAME( vk.storage.memory, "storage buffer memory", VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT );
}


#ifdef USE_VBO
void vk_release_vbo( void )
{
	if ( vk.vbo.vertex_buffer )
		qvkDestroyBuffer( vk.device, vk.vbo.vertex_buffer, NULL );
	vk.vbo.vertex_buffer = VK_NULL_HANDLE;

	if ( vk.vbo.buffer_memory )
		qvkFreeMemory( vk.device, vk.vbo.buffer_memory, NULL );
	vk.vbo.buffer_memory = VK_NULL_HANDLE;
}


qboolean vk_alloc_vbo( const byte *vbo_data, int vbo_size )
{
	VkMemoryRequirements vb_mem_reqs;
	VkMemoryAllocateInfo alloc_info;
	VkBufferCreateInfo desc;
	VkDeviceSize vertex_buffer_offset;
	VkDeviceSize allocationSize;
	uint32_t memory_type_bits;
	VkCommandBuffer command_buffer;
	VkBufferCopy copyRegion[1];
	VkDeviceSize uploadDone;

	vk_release_vbo();

	desc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	desc.queueFamilyIndexCount = 0;
	desc.pQueueFamilyIndices = NULL;

	// device-local buffer
	desc.size = vbo_size;
	desc.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	VK_CHECK( qvkCreateBuffer( vk.device, &desc, NULL, &vk.vbo.vertex_buffer ) );

	// memory requirements
	qvkGetBufferMemoryRequirements( vk.device, vk.vbo.vertex_buffer, &vb_mem_reqs );
	vertex_buffer_offset = 0;
	allocationSize = vertex_buffer_offset + vb_mem_reqs.size;
	memory_type_bits = vb_mem_reqs.memoryTypeBits;

	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.pNext = NULL;
	alloc_info.allocationSize = allocationSize;
	alloc_info.memoryTypeIndex = find_memory_type( memory_type_bits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &vk.vbo.buffer_memory ) );
	qvkBindBufferMemory( vk.device, vk.vbo.vertex_buffer, vk.vbo.buffer_memory, vertex_buffer_offset );

	// staging buffers

#ifdef USE_UPLOAD_QUEUE
	vk_flush_staging_buffer( qfalse );
#endif
	// utilize existing staging buffer
	uploadDone = 0;
	while ( uploadDone < vbo_size ) {
		VkDeviceSize uploadSize = vk.staging_buffer.size;
		if ( uploadDone + uploadSize > vbo_size ) {
			uploadSize = vbo_size - uploadDone;
		}
		memcpy(vk.staging_buffer.ptr + 0, vbo_data + uploadDone, uploadSize);
		command_buffer = begin_command_buffer();
		copyRegion[0].srcOffset = 0;
		copyRegion[0].dstOffset = uploadDone;
		copyRegion[0].size = uploadSize;
		qvkCmdCopyBuffer( command_buffer, vk.staging_buffer.handle, vk.vbo.vertex_buffer, 1, &copyRegion[0] );
		end_command_buffer( command_buffer, __func__ );
		uploadDone += uploadSize;
	}

	SET_OBJECT_NAME( vk.vbo.vertex_buffer, "static VBO", VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT );
	SET_OBJECT_NAME( vk.vbo.buffer_memory, "static VBO memory", VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT );

	return qtrue;
}
#endif

#include "shaders/spirv/shader_data.c"
#define SHADER_MODULE(name) SHADER_MODULE(name,sizeof(name))

static void vk_create_shader_modules( void )
{
	int i, j, k, l;

	vk.modules.vert.gen[0][0][0][0] = SHADER_MODULE( vert_tx0 );
	vk.modules.vert.gen[0][0][0][1] = SHADER_MODULE( vert_tx0_fog );
	vk.modules.vert.gen[0][0][1][0] = SHADER_MODULE( vert_tx0_env );
	vk.modules.vert.gen[0][0][1][1] = SHADER_MODULE( vert_tx0_env_fog );

	vk.modules.vert.gen[1][0][0][0] = SHADER_MODULE( vert_tx1 );
	vk.modules.vert.gen[1][0][0][1] = SHADER_MODULE( vert_tx1_fog );
	vk.modules.vert.gen[1][0][1][0] = SHADER_MODULE( vert_tx1_env );
	vk.modules.vert.gen[1][0][1][1] = SHADER_MODULE( vert_tx1_env_fog );

	vk.modules.vert.gen[1][1][0][0] = SHADER_MODULE( vert_tx1_cl );
	vk.modules.vert.gen[1][1][0][1] = SHADER_MODULE( vert_tx1_cl_fog );
	vk.modules.vert.gen[1][1][1][0] = SHADER_MODULE( vert_tx1_cl_env );
	vk.modules.vert.gen[1][1][1][1] = SHADER_MODULE( vert_tx1_cl_env_fog );

	vk.modules.vert.gen[2][0][0][0] = SHADER_MODULE( vert_tx2 );
	vk.modules.vert.gen[2][0][0][1] = SHADER_MODULE( vert_tx2_fog );
	vk.modules.vert.gen[2][0][1][0] = SHADER_MODULE( vert_tx2_env );
	vk.modules.vert.gen[2][0][1][1] = SHADER_MODULE( vert_tx2_env_fog );

	vk.modules.vert.gen[2][1][0][0] = SHADER_MODULE( vert_tx2_cl );
	vk.modules.vert.gen[2][1][0][1] = SHADER_MODULE( vert_tx2_cl_fog );
	vk.modules.vert.gen[2][1][1][0] = SHADER_MODULE( vert_tx2_cl_env );
	vk.modules.vert.gen[2][1][1][1] = SHADER_MODULE( vert_tx2_cl_env_fog );

	for ( i = 0; i < 3; i++ ) {
		const char *tx[] = { "single", "double", "triple" };
		const char *cl[] = { "", "+cl" };
		const char *env[] = { "", "+env" };
		const char *fog[] = { "", "+fog" };
		for ( j = 0; j < 2; j++ ) {
			for ( k = 0; k < 2; k++ ) {
				for ( l = 0; l < 2; l++ ) {
					const char *s = va( "%s-texture%s%s%s vertex module", tx[i], cl[j], env[k], fog[l] );
					SET_OBJECT_NAME( vk.modules.vert.gen[i][j][k][l], s, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
				}
			}
		}
	}

	// specialized depth-fragment shader
	vk.modules.frag.gen0_df = SHADER_MODULE( frag_tx0_df );
	SET_OBJECT_NAME( vk.modules.frag.gen0_df, "single-texture df fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );

	// fixed-color (1.0) shader modules
	vk.modules.vert.ident1[0][0][0] = SHADER_MODULE( vert_tx0_ident1 );
	vk.modules.vert.ident1[0][0][1] = SHADER_MODULE( vert_tx0_ident1_fog );
	vk.modules.vert.ident1[0][1][0] = SHADER_MODULE( vert_tx0_ident1_env );
	vk.modules.vert.ident1[0][1][1] = SHADER_MODULE( vert_tx0_ident1_env_fog );
	vk.modules.vert.ident1[1][0][0] = SHADER_MODULE( vert_tx1_ident1 );
	vk.modules.vert.ident1[1][0][1] = SHADER_MODULE( vert_tx1_ident1_fog );
	vk.modules.vert.ident1[1][1][0] = SHADER_MODULE( vert_tx1_ident1_env );
	vk.modules.vert.ident1[1][1][1] = SHADER_MODULE( vert_tx1_ident1_env_fog );
	for ( i = 0; i < 2; i++ ) {
		const char *tx[] = { "single", "double" };
		const char *env[] = { "", "+env" };
		const char *fog[] = { "", "+fog" };
		for ( j = 0; j < 2; j++ ) {
			for ( k = 0; k < 2; k++ ) {
				const char *s = va( "%s-texture identity%s%s vertex module", tx[i], env[j], fog[k] );
				SET_OBJECT_NAME( vk.modules.vert.ident1[i][j][k], s, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
			}
		}
	}

	vk.modules.frag.ident1[0][0] = SHADER_MODULE( frag_tx0_ident1 );
	vk.modules.frag.ident1[0][1] = SHADER_MODULE( frag_tx0_ident1_fog );
	vk.modules.frag.ident1[1][0] = SHADER_MODULE( frag_tx1_ident1 );
	vk.modules.frag.ident1[1][1] = SHADER_MODULE( frag_tx1_ident1_fog );
	for ( i = 0; i < 2; i++ ) {
		const char *tx[] = { "single", "double" };
		const char *fog[] = { "", "+fog" };
		for ( j = 0; j < 2; j++ ) {
			const char *s = va( "%s-texture identity%s fragment module", tx[i], fog[j] );
			SET_OBJECT_NAME( vk.modules.frag.ident1[i][j], s, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
		}
	}

	vk.modules.vert.fixed[0][0][0] = SHADER_MODULE( vert_tx0_fixed );
	vk.modules.vert.fixed[0][0][1] = SHADER_MODULE( vert_tx0_fixed_fog );
	vk.modules.vert.fixed[0][1][0] = SHADER_MODULE( vert_tx0_fixed_env );
	vk.modules.vert.fixed[0][1][1] = SHADER_MODULE( vert_tx0_fixed_env_fog );
	vk.modules.vert.fixed[1][0][0] = SHADER_MODULE( vert_tx1_fixed );
	vk.modules.vert.fixed[1][0][1] = SHADER_MODULE( vert_tx1_fixed_fog );
	vk.modules.vert.fixed[1][1][0] = SHADER_MODULE( vert_tx1_fixed_env );
	vk.modules.vert.fixed[1][1][1] = SHADER_MODULE( vert_tx1_fixed_env_fog );
	for ( i = 0; i < 2; i++ ) {
		const char *tx[] = { "single", "double" };
		const char *env[] = { "", "+env" };
		const char *fog[] = { "", "+fog" };
		for ( j = 0; j < 2; j++ ) {
			for ( k = 0; k < 2; k++ ) {
				const char *s = va( "%s-texture fixed-color%s%s vertex module", tx[i], env[j], fog[k] );
				SET_OBJECT_NAME( vk.modules.vert.fixed[i][j][k], s, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
			}
		}
	}

	vk.modules.frag.fixed[0][0] = SHADER_MODULE( frag_tx0_fixed );
	vk.modules.frag.fixed[0][1] = SHADER_MODULE( frag_tx0_fixed_fog );
	vk.modules.frag.fixed[1][0] = SHADER_MODULE( frag_tx1_fixed );
	vk.modules.frag.fixed[1][1] = SHADER_MODULE( frag_tx1_fixed_fog );
	for ( i = 0; i < 2; i++ ) {
		const char *tx[] = { "single", "double" };
		const char *fog[] = { "", "+fog" };
		for ( j = 0; j < 2; j++ ) {
			const char *s = va( "%s-texture fixed-color%s fragment module", tx[i], fog[j] );
			SET_OBJECT_NAME( vk.modules.frag.fixed[i][j], s, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
		}
	}

	vk.modules.frag.ent[0][0] = SHADER_MODULE( frag_tx0_ent );
	vk.modules.frag.ent[0][1] = SHADER_MODULE( frag_tx0_ent_fog );
	//vk.modules.frag.ent[1][0] = SHADER_MODULE( frag_tx1_ent );
	//vk.modules.frag.ent[1][1] = SHADER_MODULE( frag_tx1_ent_fog );
	for ( i = 0; i < 1; i++ ) {
		const char *tx[] = { "single" /*, "double" */};
		const char *fog[] = { "", "+fog" };
		for ( j = 0; j < 2; j++ ) {
			const char *s = va( "%s-texture entity-color%s fragment module", tx[i], fog[j] );
			SET_OBJECT_NAME( vk.modules.frag.ent[i][j], s, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
		}
	}

	vk.modules.frag.gen[0][0][0] = SHADER_MODULE( frag_tx0 );
	vk.modules.frag.gen[0][0][1] = SHADER_MODULE( frag_tx0_fog );

	vk.modules.frag.gen[1][0][0] = SHADER_MODULE( frag_tx1 );
	vk.modules.frag.gen[1][0][1] = SHADER_MODULE( frag_tx1_fog );

	vk.modules.frag.gen[1][1][0] = SHADER_MODULE( frag_tx1_cl );
	vk.modules.frag.gen[1][1][1] = SHADER_MODULE( frag_tx1_cl_fog );

	vk.modules.frag.gen[2][0][0] = SHADER_MODULE( frag_tx2 );
	vk.modules.frag.gen[2][0][1] = SHADER_MODULE( frag_tx2_fog );

	vk.modules.frag.gen[2][1][0] = SHADER_MODULE( frag_tx2_cl );
	vk.modules.frag.gen[2][1][1] = SHADER_MODULE( frag_tx2_cl_fog );

	for ( i = 0; i < 3; i++ ) {
		const char *tx[] = { "single", "double", "triple" };
		const char *cl[] = { "", "+cl" };
		const char *fog[] = { "", "+fog" };
		for ( j = 0; j < 2; j++ ) {
			for ( k = 0; k < 2; k++ ) {
				const char *s = va( "%s-texture%s%s fragment module", tx[i], cl[j], fog[k] );
				SET_OBJECT_NAME( vk.modules.frag.gen[i][j][k], s, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
			}
		}
	}


	vk.modules.vert.light[0] = SHADER_MODULE( vert_light );
	vk.modules.vert.light[1] = SHADER_MODULE( vert_light_fog );
	SET_OBJECT_NAME( vk.modules.vert.light[0], "light vertex module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.vert.light[1], "light fog vertex module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );

	vk.modules.frag.light[0][0] = SHADER_MODULE( frag_light );
	vk.modules.frag.light[0][1] = SHADER_MODULE( frag_light_fog );
	vk.modules.frag.light[1][0] = SHADER_MODULE( frag_light_line );
	vk.modules.frag.light[1][1] = SHADER_MODULE( frag_light_line_fog );
	SET_OBJECT_NAME( vk.modules.frag.light[0][0], "light fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.frag.light[0][1], "light fog fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.frag.light[1][0], "linear light fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.frag.light[1][1], "linear light fog fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );

	vk.modules.color_fs = SHADER_MODULE( color_frag_spv );
	vk.modules.color_vs = SHADER_MODULE( color_vert_spv );

	SET_OBJECT_NAME( vk.modules.color_vs, "single-color vertex module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.color_fs, "single-color fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );

	vk.modules.fog_vs = SHADER_MODULE( fog_vert_spv );
	vk.modules.fog_fs = SHADER_MODULE( fog_frag_spv );

	SET_OBJECT_NAME( vk.modules.fog_vs, "fog-only vertex module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.fog_fs, "fog-only fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );

	vk.modules.dot_vs = SHADER_MODULE( dot_vert_spv );
	vk.modules.dot_fs = SHADER_MODULE( dot_frag_spv );

	SET_OBJECT_NAME( vk.modules.dot_vs, "dot vertex module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.dot_fs, "dot fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );

	vk.modules.bloom_fs = SHADER_MODULE( bloom_frag_spv );
	vk.modules.blur_fs = SHADER_MODULE( blur_frag_spv );
	vk.modules.blur_extract_fs = SHADER_MODULE( blur_extract_frag_spv );
	vk.modules.blend_fs = SHADER_MODULE( blend_frag_spv );

	SET_OBJECT_NAME( vk.modules.bloom_fs, "bloom extraction fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.blur_fs, "gaussian blur fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.blend_fs, "final bloom blend fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );

	vk.modules.gamma_fs = SHADER_MODULE( gamma_frag_spv );
	vk.modules.gamma_vs = SHADER_MODULE( gamma_vert_spv );
	vk.modules.foveationdebug_fs = SHADER_MODULE( foveationdebug_frag_spv );
	SET_OBJECT_NAME( vk.modules.foveationdebug_fs, "foveation debug fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );

	SET_OBJECT_NAME( vk.modules.gamma_fs, "gamma post-processing fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.gamma_vs, "gamma post-processing vertex module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );

	// The post pass shaders
	vk.modules.final_composite_fov_fs = SHADER_MODULE( final_composite_fov_frag_spv );
	vk.modules.gamma_fov_fs = SHADER_MODULE( gamma_fov_frag_spv );
	SET_OBJECT_NAME( vk.modules.final_composite_fov_fs, "final composite fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	SET_OBJECT_NAME( vk.modules.gamma_fov_fs, "gamma fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
}


static void vk_alloc_persistent_pipelines( void )
{
	unsigned int state_bits;
	Vk_Pipeline_Def def;

	// Ensure we're creating pipelines for the main render pass
	vk.renderPassIndex = RENDER_PASS_MAIN;

	// skybox
	{
		Com_Memset(&def, 0, sizeof(def));
		def.shader_type = TYPE_SIGNLE_TEXTURE_FIXED_COLOR;
		def.color.rgb = tr.identityLightByte;
		def.color.alpha = tr.identityLightByte;
		def.face_culling = CT_FRONT_SIDED;
		def.polygon_offset = qfalse;
		def.mirror = qfalse;
		vk.skybox_pipeline = vk_find_pipeline_ext( 0, &def, qtrue );
	}

	// stencil shadows
	{
		cullType_t cull_types[2] = { CT_FRONT_SIDED, CT_BACK_SIDED };
		qboolean mirror_flags[2] = { qfalse, qtrue };
		int i, j;

		Com_Memset(&def, 0, sizeof(def));
		def.polygon_offset = qfalse;
		def.state_bits = 0;
		def.shader_type = TYPE_SIGNLE_TEXTURE;
		def.shadow_phase = SHADOW_EDGES;

		for (i = 0; i < 2; i++) {
			def.face_culling = cull_types[i];
			for (j = 0; j < 2; j++) {
				def.mirror = mirror_flags[j];
				vk.shadow_volume_pipelines[i][j] = vk_find_pipeline_ext( 0, &def, r_shadows->integer ? qtrue: qfalse );
			}
		}
	}
	{
		Com_Memset( &def, 0, sizeof( def ) );
		def.face_culling = CT_FRONT_SIDED;
		def.polygon_offset = qfalse;
		def.state_bits = GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO | GLS_DEPTHTEST_DISABLE;
		def.shader_type = TYPE_SIGNLE_TEXTURE;
		def.mirror = qfalse;
		def.shadow_phase = SHADOW_FS_QUAD;
		def.primitives = TRIANGLE_STRIP;
		vk.shadow_finish_pipeline = vk_find_pipeline_ext( 0, &def, r_shadows->integer ? qtrue: qfalse );
	}

	// fog and dlights
	{
		unsigned int fog_state_bits[2] = {
			GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA | GLS_DEPTHFUNC_EQUAL, // fogPass == FP_EQUAL
			GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA // fogPass == FP_LE
		};
		unsigned int dlight_state_bits[2] = {
			GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL,	// modulated
			GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL			// additive
		};
		qboolean polygon_offset[2] = { qfalse, qtrue };
		int i, j, k;
#ifdef USE_PMLIGHT
		int l;
#endif

		Com_Memset(&def, 0, sizeof(def));
		def.shader_type = TYPE_SIGNLE_TEXTURE;
		def.mirror = qfalse;

		for ( i = 0; i < 2; i++ ) {
			unsigned fog_state = fog_state_bits[ i ];
			unsigned dlight_state = dlight_state_bits[ i ];

			for ( j = 0; j < 3; j++ ) {
				def.face_culling = j; // cullType_t value

				for ( k = 0; k < 2; k++ ) {
					def.polygon_offset = polygon_offset[ k ];
#ifdef USE_FOG_ONLY
					def.shader_type = TYPE_FOG_ONLY;
#else
					def.shader_type = TYPE_SIGNLE_TEXTURE;
#endif
					def.state_bits = fog_state;
					vk.fog_pipelines[ i ][ j ][ k ] = vk_find_pipeline_ext( 0, &def, qtrue );

					def.shader_type = TYPE_SIGNLE_TEXTURE;
					def.state_bits = dlight_state;
#ifdef USE_LEGACY_DLIGHTS
#ifdef USE_PMLIGHT
					vk.dlight_pipelines[ i ][ j ][ k ] = vk_find_pipeline_ext( 0, &def, r_dlightMode->integer == 0 ? qtrue : qfalse );
#else
					vk.dlight_pipelines[ i ][ j ][ k ] = vk_find_pipeline_ext( 0, &def, qtrue );
#endif
#endif
				}
			}
		}

#ifdef USE_PMLIGHT
		def.state_bits = GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL;
		//def.shader_type = TYPE_SIGNLE_TEXTURE_LIGHTING;
		for (i = 0; i < 3; i++) { // cullType
			def.face_culling = i;
			for ( j = 0; j < 2; j++ ) { // polygonOffset
				def.polygon_offset = polygon_offset[j];
				for ( k = 0; k < 2; k++ ) {
					def.fog_stage = k; // fogStage
					for ( l = 0; l < 2; l++ ) {
						def.abs_light = l;
						def.shader_type = TYPE_SIGNLE_TEXTURE_LIGHTING;
						vk.dlight_pipelines_x[i][j][k][l] = vk_find_pipeline_ext( 0, &def, qfalse );
						def.shader_type = TYPE_SIGNLE_TEXTURE_LIGHTING_LINEAR;
						vk.dlight1_pipelines_x[i][j][k][l] = vk_find_pipeline_ext( 0, &def, qfalse );
					}
				}
			}
		}
#endif // USE_PMLIGHT
	}

	// RT_BEAM surface
	{
		Com_Memset(&def, 0, sizeof(def));
		def.state_bits = GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE;
		def.face_culling = CT_FRONT_SIDED;
		def.primitives = TRIANGLE_STRIP;
		vk.surface_beam_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );
	}

	// axis for missing models
	{
		Com_Memset( &def, 0, sizeof( def ) );
		def.state_bits = GLS_DEFAULT;
		def.shader_type = TYPE_SIGNLE_TEXTURE;
		def.face_culling = CT_TWO_SIDED;
		def.primitives = LINE_LIST;
		if ( vk.wideLines )
			def.line_width = 3;
		vk.surface_axis_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );
	}

	// flare visibility probe (RB_TestFlare): drawn depth tested to count uncovered fragments, then untested for the total
	if ( vk.fragmentStores )
	{
		Com_Memset( &def, 0, sizeof( def ) );
		def.face_culling = CT_TWO_SIDED;
		def.shader_type = TYPE_DOT;
		def.primitives = TRIANGLE_LIST;
		vk.dot_pipeline = vk_find_pipeline_ext( 0, &def, qtrue );

		def.state_bits = GLS_DEPTHTEST_DISABLE;
		vk.dot_total_pipeline = vk_find_pipeline_ext( 0, &def, qtrue );
	}

	// DrawTris()
	state_bits = GLS_POLYMODE_LINE | GLS_DEPTHMASK_TRUE;
	{
		Com_Memset(&def, 0, sizeof(def));
		def.state_bits = state_bits;
		def.shader_type = TYPE_COLOR_WHITE;
		def.face_culling = CT_FRONT_SIDED;
		vk.tris_debug_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );
	}
	{
		Com_Memset(&def, 0, sizeof(def));
		def.state_bits = state_bits;
		def.shader_type = TYPE_COLOR_WHITE;
		def.face_culling = CT_BACK_SIDED;
		vk.tris_mirror_debug_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );
	}
	{
		Com_Memset(&def, 0, sizeof(def));
		def.state_bits = state_bits;
		def.shader_type = TYPE_COLOR_GREEN;
		def.face_culling = CT_FRONT_SIDED;
		vk.tris_debug_green_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );
	}
	{
		Com_Memset(&def, 0, sizeof(def));
		def.state_bits = state_bits;
		def.shader_type = TYPE_COLOR_GREEN;
		def.face_culling = CT_BACK_SIDED;
		vk.tris_mirror_debug_green_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );
	}
	{
		Com_Memset(&def, 0, sizeof(def));
		def.state_bits = state_bits;
		def.shader_type = TYPE_COLOR_RED;
		def.face_culling = CT_FRONT_SIDED;
		vk.tris_debug_red_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );
	}
	{
		Com_Memset(&def, 0, sizeof(def));
		def.state_bits = state_bits;
		def.shader_type = TYPE_COLOR_RED;
		def.face_culling = CT_BACK_SIDED;
		vk.tris_mirror_debug_red_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );
	}

	// DrawNormals()
	{
		Com_Memset(&def, 0, sizeof(def));
		def.state_bits = GLS_DEPTHMASK_TRUE;
		def.shader_type = TYPE_SIGNLE_TEXTURE;
		def.primitives = LINE_LIST;
		vk.normals_debug_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );
	}

	// RB_DebugPolygon()
	{
		Com_Memset(&def, 0, sizeof(def));
		def.state_bits = GLS_DEPTHMASK_TRUE | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE;
		def.shader_type = TYPE_SIGNLE_TEXTURE;
		vk.surface_debug_pipeline_solid = vk_find_pipeline_ext( 0, &def, qfalse );
	}
	{
		Com_Memset(&def, 0, sizeof(def));
		def.state_bits = GLS_POLYMODE_LINE | GLS_DEPTHMASK_TRUE | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE;
		def.shader_type = TYPE_SIGNLE_TEXTURE;
		def.primitives = LINE_LIST;
		vk.surface_debug_pipeline_outline = vk_find_pipeline_ext( 0, &def, qfalse );
	}

	// RB_ShowImages
	{
		Com_Memset(&def, 0, sizeof(def));
		def.state_bits = GLS_DEPTHTEST_DISABLE | GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
		def.shader_type = TYPE_SIGNLE_TEXTURE;
		def.primitives = TRIANGLE_STRIP;
		vk.images_debug_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );

		def.state_bits = GLS_DEPTHTEST_DISABLE;
		def.shader_type = TYPE_COLOR_BLACK;
		def.primitives = TRIANGLE_STRIP;
		vk.images_debug_pipeline2 = vk_find_pipeline_ext( 0, &def, qfalse );
	}
}

static void vk_invalidate_post_bloom_2d_pipelines( void )
{
	uint32_t i, invalidated = 0;

	// Wait for GPU to finish using any pipelines we're about to destroy
	vk_wait_idle();

	for ( i = 0; i < vk.pipelines_count; i++ ) {
		if ( vk.pipelines[i].handle[RENDER_PASS_POST_SCENE_2D] != VK_NULL_HANDLE ) {
			qvkDestroyPipeline( vk.device, vk.pipelines[i].handle[RENDER_PASS_POST_SCENE_2D], NULL );
			vk.pipelines[i].handle[RENDER_PASS_POST_SCENE_2D] = VK_NULL_HANDLE;
			invalidated++;
		}
	}

	if ( invalidated > 0 ) {
		ri.Printf( PRINT_ALL, "Invalidated %u post-bloom 2D pipelines for post-process update\n", invalidated );
	}
}

void vk_update_post_process_pipelines( void )
{
	if ( vk.xr.initialized ) {
		vk_create_post_process_pipelines();
		vk_invalidate_post_bloom_2d_pipelines();
	}
}


typedef struct vk_attach_desc_s  {
	VkImage descriptor;
	VkImageView *image_view;
	VkImageUsageFlags usage;
	VkMemoryRequirements reqs;
	uint32_t memoryTypeIndex;
	VkDeviceSize  memory_offset;
	// for layout transition:
	VkImageAspectFlags aspect_flags;
	VkImageLayout image_layout;
	VkFormat image_format;
} vk_attach_desc_t;

static vk_attach_desc_t attachments[ MAX_ATTACHMENTS_IN_POOL ];
static uint32_t num_attachments = 0;


static void vk_clear_attachment_pool( void )
{
	num_attachments = 0;
}


static void vk_alloc_attachments( void )
{
	VkImageViewCreateInfo view_desc;
	VkMemoryDedicatedAllocateInfoKHR alloc_info2;
	VkMemoryAllocateInfo alloc_info;
	VkCommandBuffer command_buffer;
	VkDeviceMemory memory;
	VkDeviceSize offset;
	uint32_t memoryTypeBits;
	uint32_t memoryTypeIndex;
	uint32_t i;

	if ( num_attachments == 0 ) {
		return;
	}

	if ( vk.image_memory_count >= ARRAY_LEN( vk.image_memory ) ) {
		ri.Error( ERR_DROP, "vk.image_memory_count == %i", (int)ARRAY_LEN( vk.image_memory ) );
	}

	memoryTypeBits = ~0U;
	offset = 0;

	for ( i = 0; i < num_attachments; i++ ) {
#ifdef MIN_IMAGE_ALIGN
		VkDeviceSize alignment = MAX( attachments[ i ].reqs.alignment, MIN_IMAGE_ALIGN );
#else
		VkDeviceSize alignment = attachments[ i ].reqs.alignment;
#endif
		memoryTypeBits &= attachments[ i ].reqs.memoryTypeBits;
		offset = PAD( offset, alignment );
		attachments[ i ].memory_offset = offset;
		offset += attachments[ i ].reqs.size;
#ifdef _DEBUG
		ri.Printf( PRINT_ALL, S_COLOR_CYAN "[%i] type %i, size %i, align %i\n", i,
			attachments[ i ].reqs.memoryTypeBits,
			(int)attachments[ i ].reqs.size,
			(int)attachments[ i ].reqs.alignment );
#endif
	}

	if ( num_attachments == 1 && attachments[ 0 ].usage & VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT ) {
		// try lazy memory
		memoryTypeIndex = find_memory_type2( memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT, NULL );
		if ( memoryTypeIndex == ~0U ) {
			memoryTypeIndex = find_memory_type( memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
		}
	} else {
		memoryTypeIndex = find_memory_type( memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	}

#ifdef _DEBUG
	ri.Printf( PRINT_ALL, "memory type bits: %04x\n", memoryTypeBits );
	ri.Printf( PRINT_ALL, "memory type index: %04x\n", memoryTypeIndex );
	ri.Printf( PRINT_ALL, "total size: %i\n", (int)offset );
#endif

	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.pNext = NULL;
	alloc_info.allocationSize = offset;
	alloc_info.memoryTypeIndex = memoryTypeIndex;

	if ( num_attachments == 1 ) {
		if ( vk.dedicatedAllocation ) {
			Com_Memset( &alloc_info2, 0, sizeof( alloc_info2 ) );
			alloc_info2.sType =  VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO_KHR;
			alloc_info2.image = attachments[ 0 ].descriptor;
			alloc_info.pNext = &alloc_info2;
		}
	}

	// allocate and bind memory
	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &memory ) );

	vk.image_memory[ vk.image_memory_count++ ] = memory;

	for ( i = 0; i < num_attachments; i++ ) {

		VK_CHECK( qvkBindImageMemory( vk.device, attachments[i].descriptor, memory, attachments[i].memory_offset ) );

		// create image view (2D array for stereo multiview)
		view_desc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view_desc.pNext = NULL;
		view_desc.flags = 0;
		view_desc.image = attachments[ i ].descriptor;
		view_desc.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;  // Multiview stereo
		view_desc.format = attachments[ i ].image_format;
		view_desc.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		view_desc.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		view_desc.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		view_desc.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		view_desc.subresourceRange.aspectMask = attachments[ i ].aspect_flags;
		view_desc.subresourceRange.baseMipLevel = 0;
		view_desc.subresourceRange.levelCount = 1;
		view_desc.subresourceRange.baseArrayLayer = 0;
		view_desc.subresourceRange.layerCount = 2;  // Stereo (left + right)

		VK_CHECK( qvkCreateImageView( vk.device, &view_desc, NULL, attachments[ i ].image_view ) );
	}

	// perform layout transition
	command_buffer = begin_command_buffer();
	for ( i = 0; i < num_attachments; i++ ) {
		record_image_layout_transition( command_buffer,
			attachments[i].descriptor,
			attachments[i].aspect_flags,
			VK_IMAGE_LAYOUT_UNDEFINED, // old_layout
			attachments[i].image_layout,
			0, 0 );
	}
	end_command_buffer( command_buffer, __func__ );

	num_attachments = 0;
}


static void vk_add_attachment_desc( VkImage desc, VkImageView *image_view, VkImageUsageFlags usage, VkMemoryRequirements *reqs, VkFormat image_format, VkImageAspectFlags aspect_flags, VkImageLayout image_layout )
{
	if ( num_attachments >= ARRAY_LEN( attachments ) ) {
		ri.Error( ERR_FATAL, "Attachments array overflow" );
	} else {
		attachments[ num_attachments ].descriptor = desc;
		attachments[ num_attachments ].image_view = image_view;
		attachments[ num_attachments ].usage = usage;
		attachments[ num_attachments ].reqs = *reqs;
		attachments[ num_attachments ].aspect_flags = aspect_flags;
		attachments[ num_attachments ].image_layout = image_layout;
		attachments[ num_attachments ].image_format = image_format;
		attachments[ num_attachments ].memory_offset = 0;
		num_attachments++;
	}
}


static void vk_get_image_memory_erquirements( VkImage image, VkMemoryRequirements *memory_requirements )
{
	if ( vk.dedicatedAllocation ) {
		VkMemoryRequirements2KHR memory_requirements2;
		VkImageMemoryRequirementsInfo2KHR image_requirements2;
		VkMemoryDedicatedRequirementsKHR mem_req2;

		Com_Memset( &mem_req2, 0, sizeof( mem_req2 ) );
		mem_req2.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS_KHR;

		image_requirements2.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2_KHR;
		image_requirements2.image = image;
		image_requirements2.pNext = NULL;

		Com_Memset( &memory_requirements2, 0, sizeof( memory_requirements2 ) );
		memory_requirements2.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2_KHR;
		memory_requirements2.pNext = &mem_req2;

		qvkGetImageMemoryRequirements2KHR( vk.device, &image_requirements2, &memory_requirements2 );

		*memory_requirements = memory_requirements2.memoryRequirements;
	} else {
		qvkGetImageMemoryRequirements( vk.device, image, memory_requirements );
	}
}


static void create_color_attachment( uint32_t width, uint32_t height, VkSampleCountFlagBits samples, VkFormat format,
	VkImageUsageFlags usage, VkImage *image, VkImageView *image_view, VkImageLayout image_layout, qboolean multisample )
{
	VkImageCreateInfo create_desc;
	VkMemoryRequirements memory_requirements;

	if ( multisample && !( usage & VK_IMAGE_USAGE_SAMPLED_BIT ) )
		usage |= VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;

	// create color image
	// For XR multiview rendering, we always need 2 array layers (stereo)
	create_desc.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	create_desc.pNext = NULL;
	create_desc.flags = 0;
	create_desc.imageType = VK_IMAGE_TYPE_2D;
	create_desc.format = format;
	create_desc.extent.width = width;
	create_desc.extent.height = height;
	create_desc.extent.depth = 1;
	create_desc.mipLevels = 1;
	create_desc.arrayLayers = 2;  // Stereo multiview
	create_desc.samples = samples;
	create_desc.tiling = VK_IMAGE_TILING_OPTIMAL;
	create_desc.usage = usage;
	create_desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	create_desc.queueFamilyIndexCount = 0;
	create_desc.pQueueFamilyIndices = NULL;
	create_desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	VK_CHECK( qvkCreateImage( vk.device, &create_desc, NULL, image ) );

	vk_get_image_memory_erquirements( *image, &memory_requirements );

	vk_add_attachment_desc( *image, image_view, usage, &memory_requirements, format, VK_IMAGE_ASPECT_COLOR_BIT, image_layout );
}


static void create_depth_attachment( uint32_t width, uint32_t height, VkSampleCountFlagBits samples, VkImage *image, VkImageView *image_view, qboolean allowTransient )
{
	VkImageCreateInfo create_desc;
	VkMemoryRequirements memory_requirements;
	VkImageAspectFlags image_aspect_flags;

	// create depth image
	// For XR multiview rendering, we always need 2 array layers (stereo)
	create_desc.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	create_desc.pNext = NULL;
	create_desc.flags = 0;
	create_desc.imageType = VK_IMAGE_TYPE_2D;
	create_desc.format = vk.depth_format;
	create_desc.extent.width = width;
	create_desc.extent.height = height;
	create_desc.extent.depth = 1;
	create_desc.mipLevels = 1;
	create_desc.arrayLayers = 2;  // Stereo multiview
	create_desc.samples = samples;
	create_desc.tiling = VK_IMAGE_TILING_OPTIMAL;
	create_desc.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	if ( allowTransient ) {
		create_desc.usage |= VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
	}
	create_desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	create_desc.queueFamilyIndexCount = 0;
	create_desc.pQueueFamilyIndices = NULL;
	create_desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	image_aspect_flags = VK_IMAGE_ASPECT_DEPTH_BIT;
	if ( glConfig.stencilBits > 0 )
		image_aspect_flags |= VK_IMAGE_ASPECT_STENCIL_BIT;

	VK_CHECK( qvkCreateImage( vk.device, &create_desc, NULL, image ) );

	vk_get_image_memory_erquirements( *image, &memory_requirements );

	vk_add_attachment_desc( *image, image_view, create_desc.usage, &memory_requirements, vk.depth_format, image_aspect_flags, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL );
}


static void vk_create_attachments( void )
{
	uint32_t i;

	vk_clear_attachment_pool();

	// It looks like resulting performance depends from order you're creating/allocating
	// memory for attachments in vulkan i.e. similar images grouped together will provide best results
	// so [resolve0][resolve1][msaa0][msaa1][depth0][depth1] is most optimal
	// while cases like [resolve0][depth0][color0][...] is the worst

	// TODO: preallocate first image chunk in attachment' memory pool?
	{
		VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

		// bloom images (multiview-compatible 2-layer arrays)
		// The first blur pass extracts as it samples the stored scene, so [0] is never read: leave it null.
		if ( r_bloom->integer ) {
			uint32_t width = gls.captureWidth;
			uint32_t height = gls.captureHeight;

			for ( i = 1; i < ARRAY_LEN( vk.bloom_image ); i += 2 ) {
				width /= 2;
				height /= 2;
				create_color_attachment( width, height, VK_SAMPLE_COUNT_1_BIT, vk.bloom_format,
					usage, &vk.bloom_image[i+0], &vk.bloom_image_view[i+0], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse );

				create_color_attachment( width, height, VK_SAMPLE_COUNT_1_BIT, vk.bloom_format,
					usage, &vk.bloom_image[i+1], &vk.bloom_image_view[i+1], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse );
			}
		}

		// Screenmap images for portal/mirror rendering: only for FBO mode
		// Note: Legacy vk.color_image, vk.msaa_image, vk.depth_image removed -
		// subpass optimization uses vk.transient.* images instead
		if ( vk.fboActive ) {
			// screenmap-msaa
			if ( vk.screenMapSamples > VK_SAMPLE_COUNT_1_BIT ) {
				create_color_attachment( vk.screenMapWidth, vk.screenMapHeight, vk.screenMapSamples, vk.color_format,
					VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &vk.screenMap.color_image_msaa, &vk.screenMap.color_image_view_msaa, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, qtrue );
			}

			// screenmap/msaa-resolve
			create_color_attachment( vk.screenMapWidth, vk.screenMapHeight, VK_SAMPLE_COUNT_1_BIT, vk.color_format,
				usage, &vk.screenMap.color_image, &vk.screenMap.color_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, qfalse );

			// screenmap depth
			create_depth_attachment( vk.screenMapWidth, vk.screenMapHeight, vk.screenMapSamples, &vk.screenMap.depth_image, &vk.screenMap.depth_image_view, qtrue );
		}

	}

	//vk_alloc_attachments();

	// Note: Legacy vk.depth_image removed: subpass optimization uses vk.transient.depth_image instead

	vk_alloc_attachments();

	for ( i = 0; i < vk.image_memory_count; i++ )
	{
		SET_OBJECT_NAME( vk.image_memory[i], va( "framebuffer memory chunk %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT );
	}

	// Note: Legacy vk.depth_image and vk.color_image debug names removed -
	// subpass optimization uses vk.transient.* images which are named in create_subpass_transient_images()

	SET_OBJECT_NAME( vk.capture.image, "capture image", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
	SET_OBJECT_NAME( vk.capture.image_view, "capture image view", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );

	for ( i = 0; i < ARRAY_LEN( vk.bloom_image ); i++ )
	{
		SET_OBJECT_NAME( vk.bloom_image[i], va( "bloom attachment %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
		SET_OBJECT_NAME( vk.bloom_image_view[i], va( "bloom attachment %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
	}
}


static void vk_create_framebuffers( void )
{
	VkImageView attachments[3];
	VkFramebufferCreateInfo desc;
	uint32_t n;

	desc.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.pAttachments = attachments;
	desc.layers = 1;  // Multiview handles stereo layers via view mask

	// Note: Legacy vk.framebuffers.main and vk.framebuffers.post_bloom removed -
	// subpass optimization uses vk.framebuffers.main_with_bloom/main_with_gamma instead,
	// created later in vk_create_subpass_framebuffers() once XR swapchains are available.
	// For direct mode (r_fbo=0), XR framebuffers are created in vk_create_xr_framebuffers().

	// Screenmap and bloom framebuffers: only needed for FBO mode (r_fbo=1)
	if ( vk.fboActive ) {
		// screenmap - used for portal/mirror rendering
		desc.renderPass = vk.render_pass.screenmap;
		desc.attachmentCount = 2;
		desc.width = vk.screenMapWidth;
		desc.height = vk.screenMapHeight;
		attachments[0] = vk.screenMap.color_image_view;
		attachments[1] = vk.screenMap.depth_image_view;
		if ( vk.screenMapSamples > VK_SAMPLE_COUNT_1_BIT )
		{
			desc.attachmentCount = 3;
			attachments[2] = vk.screenMap.color_image_view_msaa;
		}
		VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.screenmap ) );
		SET_OBJECT_NAME( vk.framebuffers.screenmap, "framebuffer - screenmap", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );

		// Blur framebuffers: still used by vk_finish_subpass_post() to prepare bloom for next frame
		if ( r_bloom->integer )
		{
			uint32_t width = gls.captureWidth;
			uint32_t height = gls.captureHeight;

			for ( n = 0; n < ARRAY_LEN( vk.framebuffers.blur ); n += 2 )
			{
				width /= 2;
				height /= 2;

				desc.renderPass = vk.render_pass.blur[n];
				desc.width = width;
				desc.height = height;
				desc.attachmentCount = 1;

				attachments[0] = vk.bloom_image_view[n+0+1];
				VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.blur[n+0] ) );

				attachments[0] = vk.bloom_image_view[n+1+1];
				VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers.blur[n+1] ) );

				SET_OBJECT_NAME( vk.framebuffers.blur[n+0], va( "framebuffer - blur %i", n+0 ), VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
				SET_OBJECT_NAME( vk.framebuffers.blur[n+1], va( "framebuffer - blur %i", n+1 ), VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
			}
		}
	}
}


static void vk_create_sync_primitives( void ) {
	VkSemaphoreCreateInfo desc;
	VkFenceCreateInfo fence_desc;
	uint32_t i;

	desc.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;

#ifdef USE_UPLOAD_QUEUE
	VK_CHECK( qvkCreateSemaphore( vk.device, &desc, NULL, &vk.image_uploaded2 ) );
#endif

	// all commands submitted
	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ )
	{
		desc.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = 0;

		// swapchain image acquired
		VK_CHECK( qvkCreateSemaphore( vk.device, &desc, NULL, &vk.tess[i].image_acquired ) );

#ifdef USE_UPLOAD_QUEUE
		// second semaphore to synchronize additional tasks (e.g. image upload)
		VK_CHECK( qvkCreateSemaphore( vk.device, &desc, NULL, &vk.tess[i].rendering_finished2 ) );
#endif
		fence_desc.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fence_desc.pNext = NULL;
		//fence_desc.flags = VK_FENCE_CREATE_SIGNALED_BIT; // so it can be used to start rendering
		fence_desc.flags = 0; // non-signalled state

		VK_CHECK( qvkCreateFence( vk.device, &fence_desc, NULL, &vk.tess[i].rendering_finished_fence ) );
		vk.tess[i].waitForFence = qfalse;

		SET_OBJECT_NAME( vk.tess[i].image_acquired, va( "image_acquired semaphore %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_SEMAPHORE_EXT );
#ifdef USE_UPLOAD_QUEUE
		SET_OBJECT_NAME( vk.tess[i].rendering_finished2, va( "rendering_finished2 semaphore %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_SEMAPHORE_EXT );
#endif
		SET_OBJECT_NAME( vk.tess[i].rendering_finished_fence, va( "rendering_finished fence %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_FENCE_EXT );
	}

	fence_desc.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fence_desc.pNext = NULL;
	fence_desc.flags = 0;

#ifdef USE_UPLOAD_QUEUE
	VK_CHECK( qvkCreateFence( vk.device, &fence_desc, NULL, &vk.aux_fence ) );
	SET_OBJECT_NAME( vk.aux_fence, "aux fence", VK_DEBUG_REPORT_OBJECT_TYPE_FENCE_EXT );

	vk.rendering_finished = VK_NULL_HANDLE;
	vk.image_uploaded = VK_NULL_HANDLE;
	vk.aux_fence_wait = qfalse;
#endif

	// Generic linear sampler used for post-processing, etc.
	{
		VkSamplerCreateInfo samplerCI;
		Com_Memset( &samplerCI, 0, sizeof( samplerCI ) );
		samplerCI.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerCI.magFilter = VK_FILTER_LINEAR;
		samplerCI.minFilter = VK_FILTER_LINEAR;
		samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

		VK_CHECK( qvkCreateSampler( vk.device, &samplerCI, NULL, &vk.linearSampler ) );
		SET_OBJECT_NAME( vk.linearSampler, "Linear sampler", VK_DEBUG_REPORT_OBJECT_TYPE_SAMPLER_EXT );
	}
}


static void vk_destroy_sync_primitives( void  ) {
	uint32_t i;

#ifdef USE_UPLOAD_QUEUE
	qvkDestroySemaphore( vk.device, vk.image_uploaded2, NULL );
#endif

	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		qvkDestroySemaphore( vk.device, vk.tess[i].image_acquired, NULL );
#ifdef USE_UPLOAD_QUEUE
		qvkDestroySemaphore( vk.device, vk.tess[i].rendering_finished2, NULL );
#endif
		qvkDestroyFence( vk.device, vk.tess[i].rendering_finished_fence, NULL );
		vk.tess[i].waitForFence = qfalse;
		vk.tess[i].swapchain_image_acquired = qfalse;
	}

#ifdef USE_UPLOAD_QUEUE
	qvkDestroyFence( vk.device, vk.aux_fence, NULL );

	vk.rendering_finished = VK_NULL_HANDLE;
	vk.image_uploaded = VK_NULL_HANDLE;
#endif

	// Generic linear sampler
	if ( vk.linearSampler != VK_NULL_HANDLE ) {
		qvkDestroySampler( vk.device, vk.linearSampler, NULL );
		vk.linearSampler = VK_NULL_HANDLE;
	}
}


static void vk_destroy_framebuffers( void ) {
	uint32_t n;

	if ( vk.framebuffers.main != VK_NULL_HANDLE ) {
		qvkDestroyFramebuffer( vk.device, vk.framebuffers.main, NULL );
		vk.framebuffers.main = VK_NULL_HANDLE;
	}

	for ( n = 0; n < vk.swapchain_image_count; n++ ) {
		if ( vk.framebuffers.gamma[n] != VK_NULL_HANDLE ) {
			qvkDestroyFramebuffer( vk.device, vk.framebuffers.gamma[n], NULL );
			vk.framebuffers.gamma[n] = VK_NULL_HANDLE;
		}
	}

	if ( vk.framebuffers.screenmap != VK_NULL_HANDLE ) {
		qvkDestroyFramebuffer( vk.device, vk.framebuffers.screenmap, NULL );
		vk.framebuffers.screenmap = VK_NULL_HANDLE;
	}

	for ( n = 0; n < ARRAY_LEN( vk.framebuffers.blur ); n++ ) {
		if ( vk.framebuffers.blur[n] != VK_NULL_HANDLE ) {
			qvkDestroyFramebuffer( vk.device, vk.framebuffers.blur[n], NULL );
			vk.framebuffers.blur[n] = VK_NULL_HANDLE;
		}
	}

	if ( vk.framebuffers.post_bloom != VK_NULL_HANDLE ) {
		qvkDestroyFramebuffer( vk.device, vk.framebuffers.post_bloom, NULL );
		vk.framebuffers.post_bloom = VK_NULL_HANDLE;
	}
}


static void vk_destroy_attachments( void );
static void vk_destroy_render_passes( void );
static void vk_destroy_pipelines( qboolean resetCount );
static qboolean vk_reallocate_xr_fbo_descriptors( void );
static void vk_destroy_hud_buffer( void );
static void vk_destroy_subpass_render_passes( void );
static void vk_destroy_subpass_framebuffers( void );
static void vk_destroy_post_process_pipelines( void );


static void vk_set_render_scale( void )
{
	if ( gls.windowWidth != glConfig.vidWidth || gls.windowHeight != glConfig.vidHeight )
	{
		if ( r_renderScale->integer > 0 )
		{
			int scaleMode = r_renderScale->integer - 1;
			if ( scaleMode & 1 )
			{
				// preserve aspect ratio (black bars on sides)
				float windowAspect = (float) gls.windowWidth / (float) gls.windowHeight;
				float renderAspect = (float) glConfig.vidWidth / (float) glConfig.vidHeight;
				if ( windowAspect >= renderAspect )
				{
					float scale = (float)gls.windowHeight / ( float ) glConfig.vidHeight;
					int bias = ( gls.windowWidth - scale * (float) glConfig.vidWidth ) / 2;
					vk.blitX0 += bias;
				}
				else
				{
					float scale = (float)gls.windowWidth / ( float ) glConfig.vidWidth;
					int bias = ( gls.windowHeight - scale * (float) glConfig.vidHeight ) / 2;
					vk.blitY0 += bias;
				}
			}
			// linear filtering
			if ( scaleMode & 2 )
				vk.blitFilter = GL_LINEAR;
			else
				vk.blitFilter = GL_NEAREST;
		}

		vk.windowAdjusted = qtrue;
	}

}


void vk_initialize( void )
{
	char buf[64], driver_version[64];
	const char *vendor_name;
	VkPhysicalDeviceProperties props;
	uint32_t major;
	uint32_t minor;
	uint32_t patch;
	uint32_t maxSize;
	uint32_t i;

	init_vulkan_library();

	qvkGetDeviceQueue( vk.device, vk.queue_family_index, 0, &vk.queue );

	ri.Printf( PRINT_ALL, "[VK] vk_initialize: device=%p, queue_family=%d, queue=%p\n",
		(void*)vk.device, vk.queue_family_index, (void*)vk.queue );

	if ( vk.queue == VK_NULL_HANDLE ) {
		ri.Error( ERR_FATAL, "[VK] Failed to get Vulkan queue!" );
	}

	qvkGetPhysicalDeviceProperties( vk.physical_device, &props );

	vk.cmd = vk.tess + 0;

	// identity until the first RB_BeginDrawingView() computes real per-view matrices
	Com_Memset( vk_view_eyeproj, 0, sizeof( vk_view_eyeproj ) );
	vk_view_eyeproj[0][0] = vk_view_eyeproj[0][5] = vk_view_eyeproj[0][10] = vk_view_eyeproj[0][15] = 1.0f;
	vk_view_eyeproj[1][0] = vk_view_eyeproj[1][5] = vk_view_eyeproj[1][10] = vk_view_eyeproj[1][15] = 1.0f;

	vk.uniform_alignment = props.limits.minUniformBufferOffsetAlignment;
	vk.uniform_item_size = PAD( (uint32_t)sizeof( vkUniform_t ), vk.uniform_alignment );

	// for flare visibility tests: two counters a flare, passed and total
	vk.storage_alignment = MAX( props.limits.minStorageBufferOffsetAlignment, 2 * sizeof( uint32_t ) );

	vk.maxAnisotropy = props.limits.maxSamplerAnisotropy;
	ri.Printf( PRINT_ALL, "...max anisotropy: %.0f\n", vk.maxAnisotropy );

	vk.blitFilter = GL_NEAREST;
	vk.windowAdjusted = qfalse;
	vk.blitX0 = vk.blitY0 = 0;

	vk_set_render_scale();

	// FBO mode: enables post-processing (gamma, bloom) but adds overhead
	// r_fbo 0 = direct rendering to XR swapchain (faster, no post-processing)
	// r_fbo 1 = FBO with post-processing (slower but has bloom/gamma)
	vk.fboActive = ( r_fbo->integer != 0 ) ? qtrue : qfalse;
	// MSAA in direct mode too: a transient multisampled color resolves into the swapchain
	if ( r_ext_multisample->integer ) {
		vk.msaaActive = qtrue;
	}

	// multisampling
	// Use framebuffer sample count limits, not sampled image limits
	// These determine what sample counts are supported for color/depth attachments
	vkMaxSamples = MIN( props.limits.framebufferColorSampleCounts, props.limits.framebufferDepthSampleCounts );
	if ( glConfig.stencilBits > 0 ) {
		vkMaxSamples = MIN( vkMaxSamples, props.limits.framebufferStencilSampleCounts );
	}

	if ( vk.msaaActive ) {
		VkSampleCountFlags mask = vkMaxSamples;
		vkSamples = MAX( log2pad( r_ext_multisample->integer, 1 ), VK_SAMPLE_COUNT_2_BIT );
		while ( vkSamples > mask )
				vkSamples >>= 1;
		ri.Printf( PRINT_ALL, "...using %ix MSAA\n", vkSamples );
	} else {
		vkSamples = VK_SAMPLE_COUNT_1_BIT;
	}

	vk.screenMapSamples = MIN( vkMaxSamples, VK_SAMPLE_COUNT_4_BIT );

	vk.screenMapWidth = (float) glConfig.vidWidth / 16.0;
	if ( vk.screenMapWidth < 4 )
		vk.screenMapWidth = 4;

	vk.screenMapHeight = (float) glConfig.vidHeight / 16.0;
	if ( vk.screenMapHeight < 4 )
		vk.screenMapHeight = 4;

	vk.defaults.geometry_size = VERTEX_BUFFER_SIZE;
	vk.defaults.staging_size = STAGING_BUFFER_SIZE;

	// get memory size & defaults
	{
		VkPhysicalDeviceMemoryProperties props;
		VkDeviceSize maxDedicatedSize = 0;
		VkDeviceSize maxBARSize = 0;
		qvkGetPhysicalDeviceMemoryProperties( vk.physical_device, &props );
		for ( i = 0; i < props.memoryTypeCount; i++ ) {
			if ( props.memoryTypes[i].propertyFlags == VK_MEMORY_HEAP_DEVICE_LOCAL_BIT ) {
				maxDedicatedSize = props.memoryHeaps[props.memoryTypes[i].heapIndex].size;
			}
			else if ( props.memoryTypes[i].propertyFlags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT ) {
				if ( maxDedicatedSize == 0 || props.memoryHeaps[props.memoryTypes[i].heapIndex].size > maxDedicatedSize ) {
					maxDedicatedSize = props.memoryHeaps[props.memoryTypes[i].heapIndex].size;
				}
			}
			if ( props.memoryTypes[i].propertyFlags == (VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) ) {
				maxBARSize = props.memoryHeaps[props.memoryTypes[i].heapIndex].size;
			}
			else if ( (props.memoryTypes[i].propertyFlags & (VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) == (VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) ) {
				if ( maxBARSize == 0 ) {
					maxBARSize = props.memoryHeaps[props.memoryTypes[i].heapIndex].size;
				}
			}
		}

		if ( maxDedicatedSize != 0 ) {
			ri.Printf( PRINT_ALL, "...device memory size: %iMB\n", (int)((maxDedicatedSize + (1024 * 1024) - 1) / (1024 * 1024)) );
		}
		if ( maxBARSize != 0 ) {
			if ( maxBARSize >= 128 * 1024 * 1024 ) {
				// user larger buffers to avoid potential reallocations
				vk.defaults.geometry_size = VERTEX_BUFFER_SIZE_HI;
				vk.defaults.staging_size = STAGING_BUFFER_SIZE_HI;
			}
#ifdef _DEBUG
			ri.Printf( PRINT_ALL, "...BAR memory size: %iMB\n", (int)((maxBARSize + (1024 * 1024) - 1) / (1024 * 1024)) );
#endif
		}
	}

	// fill glConfig information

	// maxTextureSize must not exceed IMAGE_CHUNK_SIZE
	maxSize = sqrtf( IMAGE_CHUNK_SIZE / 4 );
	// round down to next power of 2
	glConfig.maxTextureSize = MIN( props.limits.maxImageDimension2D, log2pad( maxSize, 0 ) );

	if ( glConfig.maxTextureSize > MAX_TEXTURE_SIZE )
		glConfig.maxTextureSize = MAX_TEXTURE_SIZE; // ResampleTexture() relies on that maximum

	// default chunk size, may be doubled on demand
	vk.image_chunk_size = IMAGE_CHUNK_SIZE;

	vk.maxLod = 1 + Q_log2( glConfig.maxTextureSize );

	if ( props.limits.maxPerStageDescriptorSamplers != 0xFFFFFFFF )
		glConfig.numTextureUnits = props.limits.maxPerStageDescriptorSamplers;
	else
		glConfig.numTextureUnits = props.limits.maxBoundDescriptorSets;
	if ( glConfig.numTextureUnits > MAX_TEXTURE_UNITS )
		glConfig.numTextureUnits = MAX_TEXTURE_UNITS;

	vk.maxBoundDescriptorSets = props.limits.maxBoundDescriptorSets;

	if ( r_ext_texture_env_add->integer != 0 )
		glConfig.textureEnvAddAvailable = qtrue;
	else
		glConfig.textureEnvAddAvailable = qfalse;

	glConfig.textureCompression = TC_NONE;

	major = VK_VERSION_MAJOR(props.apiVersion);
	minor = VK_VERSION_MINOR(props.apiVersion);
	patch = VK_VERSION_PATCH(props.apiVersion);

	// decode driver version
	switch ( props.vendorID ) {
		case 0x10DE: // NVidia
			Com_sprintf( driver_version, sizeof( driver_version ), "%i.%i.%i.%i",
				(props.driverVersion >> 22) & 0x3FF,
				(props.driverVersion >> 14) & 0x0FF,
				(props.driverVersion >> 6) & 0x0FF,
				(props.driverVersion >> 0) & 0x03F );
			break;
#ifdef _WIN32
		case 0x8086: // Intel
			Com_sprintf( driver_version, sizeof( driver_version ), "%i.%i",
				(props.driverVersion >> 14),
				(props.driverVersion >> 0) & 0x3FFF );
			break;
#endif
		default:
			Com_sprintf( driver_version, sizeof( driver_version ), "%i.%i.%i",
				(props.driverVersion >> 22),
				(props.driverVersion >> 12) & 0x3FF,
				(props.driverVersion >> 0) & 0xFFF );
	}

	Com_sprintf( glConfig.version_string, sizeof( glConfig.version_string ), "API: %i.%i.%i, Driver: %s",
		major, minor, patch, driver_version );

	vk.offscreenRender = qtrue;

	if ( props.vendorID == 0x1002 ) {
		vendor_name = "Advanced Micro Devices, Inc.";
	} else if ( props.vendorID == 0x106B ) {
		vendor_name = "Apple Inc.";
	} else if ( props.vendorID == 0x10DE ) {
		// https://github.com/SaschaWillems/Vulkan/issues/493
		// we can't render to offscreen presentation surfaces on nvidia
		vk.offscreenRender = qfalse;
		vendor_name = "NVIDIA";
	} else if ( props.vendorID == 0x14E4 ) {
		vendor_name = "Broadcom Inc.";
	} else if ( props.vendorID == 0x1AE0 ) {
		vendor_name = "Google Inc.";
	} else if ( props.vendorID == 0x8086 ) {
		vendor_name = "Intel Corporation";
	} else if ( props.vendorID == VK_VENDOR_ID_MESA ) {
		vendor_name = "MESA";
	} else {
		Com_sprintf( buf, sizeof( buf ), "VendorID: %04x", props.vendorID );
		vendor_name = buf;
	}

	Q_strncpyz( glConfig.vendor_string, vendor_name, sizeof( glConfig.vendor_string ) );
	Q_strncpyz( glConfig.renderer_string, renderer_name( &props ), sizeof( glConfig.renderer_string ) );

	// The device goes unnamed: the layer's debug_marker tracker never holds a VkDevice, so naming it always reports 01492.

	// do early texture mode setup to avoid redundant descriptor updates in GL_SetDefaultState()
	vk.samplers.filter_min = -1;
	vk.samplers.filter_max = -1;
	GL_TextureMode( r_textureMode->string );
	r_textureMode->modified = qfalse;

	//
	// Sync primitives.
	//
	vk_create_sync_primitives();

	//
	// Command pool.
	//
	{
		VkCommandPoolCreateInfo desc;

		desc.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		desc.queueFamilyIndex = vk.queue_family_index;

		VK_CHECK( qvkCreateCommandPool( vk.device, &desc, NULL, &vk.command_pool ) );

		SET_OBJECT_NAME( vk.command_pool, "command pool", VK_DEBUG_REPORT_OBJECT_TYPE_COMMAND_POOL_EXT );
	}

#ifdef USE_UPLOAD_QUEUE
	{
		VkCommandBufferAllocateInfo alloc_info;

		alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		alloc_info.pNext = NULL;
		alloc_info.commandPool = vk.command_pool;
		alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		alloc_info.commandBufferCount = 1;

		VK_CHECK( qvkAllocateCommandBuffers( vk.device, &alloc_info, &vk.staging_command_buffer ) );
		SET_OBJECT_NAME( vk.staging_command_buffer, "staging cmd", VK_DEBUG_REPORT_OBJECT_TYPE_COMMAND_BUFFER_EXT );
	}
#endif

	//
	// Command buffers and color attachments.
	//
	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ )
	{
		VkCommandBufferAllocateInfo alloc_info;

		alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		alloc_info.pNext = NULL;
		alloc_info.commandPool = vk.command_pool;
		alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		alloc_info.commandBufferCount = 1;

		VK_CHECK( qvkAllocateCommandBuffers( vk.device, &alloc_info, &vk.tess[i].command_buffer ) );
		SET_OBJECT_NAME( vk.tess[i].command_buffer, va( "tess cmd %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_COMMAND_BUFFER_EXT );

		// the HUD buffer's own, submitted ahead of the frame's (vk_begin_hud_render_pass)
		VK_CHECK( qvkAllocateCommandBuffers( vk.device, &alloc_info, &vk.tess[i].hud_command_buffer ) );
		SET_OBJECT_NAME( vk.tess[i].hud_command_buffer, va( "tess hud cmd %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_COMMAND_BUFFER_EXT );
		vk.tess[i].hud_begun = qfalse;
	}

#ifdef USE_UPLOAD_QUEUE
	ri.Printf( PRINT_ALL, "Command buffers: staging=%p tess0=%p tess1=%p\n",
		(void *)vk.staging_command_buffer, (void *)vk.tess[0].command_buffer,
		(void *)vk.tess[1].command_buffer );
#else
	ri.Printf( PRINT_ALL, "Command buffers: tess0=%p tess1=%p\n",
		(void *)vk.tess[0].command_buffer, (void *)vk.tess[1].command_buffer );
#endif

	//
	// Descriptor pool.
	//
	{
		VkDescriptorPoolSize pool_size[4];
		VkDescriptorPoolCreateInfo desc;
		uint32_t i, maxSets;

		pool_size[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		pool_size[0].descriptorCount = MAX_DRAWIMAGES + 1 + 1 + 1 + VK_NUM_BLOOM_PASSES * 2 + 4; // color, screenmap, bloom descriptors, +4 for combined blur descriptor

		pool_size[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		pool_size[1].descriptorCount = NUM_COMMAND_BUFFERS * 2; // binding 0 (fog/dlight) + binding 1 (per-view eyeProj)

		pool_size[2].type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		pool_size[2].descriptorCount = NUM_COMMAND_BUFFERS + 1;	// input set is allocated by both the post-reset realloc and vk_init_xr_resources per pool generation

		pool_size[3].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
		pool_size[3].descriptorCount = 1;

		for ( i = 0, maxSets = 0; i < ARRAY_LEN( pool_size ); i++ ) {
			maxSets += pool_size[i].descriptorCount;
		}

		desc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = 0;
		desc.maxSets = maxSets;
		desc.poolSizeCount = ARRAY_LEN( pool_size );
		desc.pPoolSizes = pool_size;

		VK_CHECK( qvkCreateDescriptorPool( vk.device, &desc, NULL, &vk.descriptor_pool ) );
	}

	//
	// Descriptor set layout.
	//
	vk_create_layout_binding( 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, &vk.set_layout_sampler );
	{
		VkDescriptorSetLayoutBinding b[2];
		VkDescriptorSetLayoutCreateInfo ci;

		b[0].binding = 0;
		b[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		b[0].descriptorCount = 1;
		b[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT;
		b[0].pImmutableSamplers = NULL;

		// per-view eyeProj[2] (P_eye * E'_eye), written once per view
		b[1].binding = 1;
		b[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		b[1].descriptorCount = 1;
		b[1].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		b[1].pImmutableSamplers = NULL;

		ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		ci.pNext = NULL;
		ci.flags = 0;
		ci.bindingCount = 2;
		ci.pBindings = b;
		VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &ci, NULL, &vk.set_layout_uniform ) );
	}
	vk_create_layout_binding( 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT, &vk.set_layout_storage );
	vk_create_4sampler_layout( &vk.set_layout_4samplers );

	//
	// Pipeline layouts.
	//
	{
		VkDescriptorSetLayout set_layouts[6];
		VkPipelineLayoutCreateInfo desc;
		VkPushConstantRange push_range;

		// vertex stage: mono modelview; per-eye projection lives in set 0 binding 1
		push_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		push_range.offset = 0;
		push_range.size = 64; // 16 floats

		// standard pipelines
		set_layouts[0] = vk.set_layout_uniform; // fog/dlight parameters
		set_layouts[1] = vk.set_layout_sampler; // diffuse
		set_layouts[2] = vk.set_layout_sampler; // lightmap / fog-only
		set_layouts[3] = vk.set_layout_sampler; // blend
		set_layouts[4] = vk.set_layout_sampler; // collapsed fog texture

		desc.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = 0;
		desc.setLayoutCount = (vk.maxBoundDescriptorSets >= VK_DESC_COUNT) ? VK_DESC_COUNT : 4;
		desc.pSetLayouts = set_layouts;
		desc.pushConstantRangeCount = 1;
		desc.pPushConstantRanges = &push_range;

		VK_CHECK(qvkCreatePipelineLayout(vk.device, &desc, NULL, &vk.pipeline_layout));

		// flare test pipeline: the probe pushes its own block (dot.vert), not the mono modelview
		VkPushConstantRange probe_range;
		probe_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		probe_range.offset = 0;
		probe_range.size = FLARE_PROBE_PUSH_FLOATS * sizeof( float );

		set_layouts[0] = vk.set_layout_storage; // dynamic storage buffer

		desc.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = 0;
		desc.setLayoutCount = 1;
		desc.pSetLayouts = set_layouts;
		desc.pushConstantRangeCount = 1;
		desc.pPushConstantRanges = &probe_range;

		VK_CHECK( qvkCreatePipelineLayout( vk.device, &desc, NULL, &vk.pipeline_layout_storage ) );

		// post-processing pipeline
		set_layouts[0] = vk.set_layout_sampler; // sampler
		set_layouts[1] = vk.set_layout_sampler; // sampler
		set_layouts[2] = vk.set_layout_sampler; // sampler
		set_layouts[3] = vk.set_layout_sampler; // sampler

		desc.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = 0;
		desc.setLayoutCount = 1;
		desc.pSetLayouts = set_layouts;
		desc.pushConstantRangeCount = 0;
		desc.pPushConstantRanges = NULL;

		VK_CHECK( qvkCreatePipelineLayout( vk.device, &desc, NULL, &vk.pipeline_layout_post_process ) );

		desc.setLayoutCount = VK_NUM_BLOOM_PASSES;

		VK_CHECK( qvkCreatePipelineLayout( vk.device, &desc, NULL, &vk.pipeline_layout_blend ) );

		SET_OBJECT_NAME( vk.pipeline_layout, "pipeline layout - main", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT );
		SET_OBJECT_NAME( vk.pipeline_layout_post_process, "pipeline layout - post-processing", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT );
		SET_OBJECT_NAME( vk.pipeline_layout_blend, "pipeline layout - blend", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT );

		// Post pass layouts: the stored scene as a sampler in set 0
		set_layouts[0] = vk.set_layout_sampler;
		set_layouts[1] = vk.set_layout_4samplers; // all 4 blur results in one descriptor set

		desc.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = 0;
		desc.pSetLayouts = set_layouts;
		desc.pushConstantRangeCount = 0;
		desc.pPushConstantRanges = NULL;

		// Composite: the scene plus the blur chain
		desc.setLayoutCount = 2;
		VK_CHECK( qvkCreatePipelineLayout( vk.device, &desc, NULL, &vk.pipeline_layout_fov_composite ) );

		// Gamma alone: just the scene
		desc.setLayoutCount = 1;
		VK_CHECK( qvkCreatePipelineLayout( vk.device, &desc, NULL, &vk.pipeline_layout_fov_gamma ) );

		SET_OBJECT_NAME( vk.pipeline_layout_fov_gamma, "pipeline layout - gamma", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT );
		SET_OBJECT_NAME( vk.pipeline_layout_fov_composite, "pipeline layout - composite", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT );
	}

	vk.geometry_buffer_size_new = vk.defaults.geometry_size;
	vk_create_geometry_buffers( vk.geometry_buffer_size_new );
	vk.geometry_buffer_size_new = 0;

	vk_create_storage_buffer( MAX_FLARES * vk.storage_alignment );

	vk_create_shader_modules();

	{
		VkPipelineCacheCreateInfo ci;
		Com_Memset( &ci, 0, sizeof( ci ) );
		ci.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
		VK_CHECK( qvkCreatePipelineCache( vk.device, &ci, NULL, &vk.pipelineCache ) );
	}

	vk.renderPassIndex = RENDER_PASS_MAIN; // default render pass

	// Quest uses XR swapchains managed by OpenXR runtime, no desktop swapchain
	// Set swapchain_image_count to 1 so vk_create_framebuffers runs its loop at least once
	// (the loop creates FBO framebuffers which don't depend on desktop swapchain images)
	ri.Printf( PRINT_ALL, "Skipping desktop swapchain (Quest uses XR swapchains)\n" );
	vk.swapchain_image_count = 1;
	vk.initSwapchainLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	// color/depth attachments
	ri.Printf( PRINT_ALL, "Creating attachments...\n" );
	fflush( stdout );
	vk_create_attachments();
	ri.Printf( PRINT_ALL, "Attachments created\n" );
	fflush( stdout );

	// transient images for subpass optimization (always created for tile-based GPUs)
	ri.Printf( PRINT_ALL, "Creating subpass transient images...\n" );
	fflush( stdout );
	vk_create_subpass_transient_images();
	ri.Printf( PRINT_ALL, "Subpass transient images created\n" );
	fflush( stdout );

	// renderpasses
	ri.Printf( PRINT_ALL, "Creating render passes...\n" );
	fflush( stdout );
	vk_create_render_passes();
	vk_create_fov_split_render_passes();
	ri.Printf( PRINT_ALL, "Render passes created\n" );
	fflush( stdout );

	// framebuffers for each swapchain image
	ri.Printf( PRINT_ALL, "Creating framebuffers...\n" );
	fflush( stdout );
	vk_create_framebuffers();
	ri.Printf( PRINT_ALL, "Framebuffers created\n" );
	fflush( stdout );

	// preallocate staging buffer
	if ( vk.defaults.staging_size == STAGING_BUFFER_SIZE_HI ) {
		vk_alloc_staging_buffer( vk.defaults.staging_size );
	}

	ri.Printf( PRINT_ALL, "Vulkan initialization complete\n" );
	fflush( stdout );
	vk.active = qtrue;
}


void vk_create_pipelines( void )
{
	ri.Printf( PRINT_ALL, "vk_create_pipelines: creating base pipelines, renderPassIndex=%d, main=%p\n",
		vk.renderPassIndex, (void*)vk.render_pass.main );

	vk_alloc_persistent_pipelines();

	ri.Printf( PRINT_ALL, "vk_create_pipelines: created %d base pipelines\n", vk.pipelines_count );

	vk.pipelines_world_base = vk.pipelines_count;
}


static void vk_destroy_attachments( void )
{
	uint32_t i;

	// [0] is never created; the chain starts at 1
	for ( i = 0; i < ARRAY_LEN( vk.bloom_image ); i++ ) {
		if ( vk.bloom_image[i] == VK_NULL_HANDLE ) {
			continue;
		}
		qvkDestroyImage( vk.device, vk.bloom_image[i], NULL );
		qvkDestroyImageView( vk.device, vk.bloom_image_view[i], NULL );
		vk.bloom_image[i] = VK_NULL_HANDLE;
		vk.bloom_image_view[i] = VK_NULL_HANDLE;
	}

	// Legacy FBO images (no longer created in subpass mode, kept for direct mode cleanup)
	if ( vk.color_image ) {
		qvkDestroyImage( vk.device, vk.color_image, NULL );
		qvkDestroyImageView( vk.device, vk.color_image_view, NULL );
		vk.color_image = VK_NULL_HANDLE;
		vk.color_image_view = VK_NULL_HANDLE;
	}

	if ( vk.msaa_image ) {
		qvkDestroyImage( vk.device, vk.msaa_image, NULL );
		qvkDestroyImageView( vk.device, vk.msaa_image_view, NULL );
		vk.msaa_image = VK_NULL_HANDLE;
		vk.msaa_image_view = VK_NULL_HANDLE;
	}

	if ( vk.depth_image ) {
		qvkDestroyImage( vk.device, vk.depth_image, NULL );
		qvkDestroyImageView( vk.device, vk.depth_image_view, NULL );
		vk.depth_image = VK_NULL_HANDLE;
		vk.depth_image_view = VK_NULL_HANDLE;
	}

	if ( vk.screenMap.color_image ) {
		qvkDestroyImage( vk.device, vk.screenMap.color_image, NULL );
		qvkDestroyImageView( vk.device, vk.screenMap.color_image_view, NULL );
		vk.screenMap.color_image = VK_NULL_HANDLE;
		vk.screenMap.color_image_view = VK_NULL_HANDLE;
	}

	if ( vk.screenMap.color_image_msaa ) {
		qvkDestroyImage( vk.device, vk.screenMap.color_image_msaa, NULL );
		qvkDestroyImageView( vk.device, vk.screenMap.color_image_view_msaa, NULL );
		vk.screenMap.color_image_msaa = VK_NULL_HANDLE;
		vk.screenMap.color_image_view_msaa = VK_NULL_HANDLE;
	}

	if ( vk.screenMap.depth_image ) {
		qvkDestroyImage( vk.device, vk.screenMap.depth_image, NULL );
		qvkDestroyImageView( vk.device, vk.screenMap.depth_image_view, NULL );
		vk.screenMap.depth_image = VK_NULL_HANDLE;
		vk.screenMap.depth_image_view = VK_NULL_HANDLE;
	}

	if ( vk.capture.image ) {
		qvkDestroyImage( vk.device, vk.capture.image, NULL );
		qvkDestroyImageView( vk.device, vk.capture.image_view, NULL );
		vk.capture.image = VK_NULL_HANDLE;
		vk.capture.image_view = VK_NULL_HANDLE;
	}

	// Transient images for subpass optimization
	vk_destroy_subpass_transient_images();

	for ( i = 0; i < vk.image_memory_count; i++ ) {
		qvkFreeMemory( vk.device, vk.image_memory[i], NULL );
	}

	vk.image_memory_count = 0;
}


static void vk_destroy_render_passes( void )
{
	uint32_t i;

	// Main multiview render passes
	if ( vk.render_pass.main != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.main, NULL );
		vk.render_pass.main = VK_NULL_HANDLE;
	}

	if ( vk.render_pass.screenmap != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.screenmap, NULL );
		vk.render_pass.screenmap = VK_NULL_HANDLE;
	}

	// Post-processing render passes (multiview)
	if ( vk.render_pass.gamma != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.gamma, NULL );
		vk.render_pass.gamma = VK_NULL_HANDLE;
	}

	for ( i = 0; i < ARRAY_LEN( vk.render_pass.blur ); i++ ) {
		if ( vk.render_pass.blur[i] != VK_NULL_HANDLE ) {
			qvkDestroyRenderPass( vk.device, vk.render_pass.blur[i], NULL );
			vk.render_pass.blur[i] = VK_NULL_HANDLE;
		}
	}

	if ( vk.render_pass.post_bloom != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.post_bloom, NULL );
		vk.render_pass.post_bloom = VK_NULL_HANDLE;
	}

	// HUD buffer render passes
	if ( vk.render_pass.hudBuffer != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.hudBuffer, NULL );
		vk.render_pass.hudBuffer = VK_NULL_HANDLE;
	}
	if ( vk.render_pass.hudBufferClear != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.hudBufferClear, NULL );
		vk.render_pass.hudBufferClear = VK_NULL_HANDLE;
	}

	// Subpass optimization render passes
	vk_destroy_subpass_render_passes();
}


static void vk_destroy_subpass_render_passes( void )
{
	if ( vk.render_pass.fov_scene != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.fov_scene, NULL );
		vk.render_pass.fov_scene = VK_NULL_HANDLE;
	}

	if ( vk.render_pass.main_with_bloom != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.main_with_bloom, NULL );
		vk.render_pass.main_with_bloom = VK_NULL_HANDLE;
	}

	if ( vk.render_pass.main_with_gamma != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.main_with_gamma, NULL );
		vk.render_pass.main_with_gamma = VK_NULL_HANDLE;
	}
}


static void vk_destroy_pipelines( qboolean resetCounter )
{
	uint32_t i, j;

	for ( i = 0; i < vk.pipelines_count; i++ ) {
		for ( j = 0; j < RENDER_PASS_COUNT; j++ ) {
			if ( vk.pipelines[i].handle[j] != VK_NULL_HANDLE ) {
				qvkDestroyPipeline( vk.device, vk.pipelines[i].handle[j], NULL );
				vk.pipelines[i].handle[j] = VK_NULL_HANDLE;
				vk.pipeline_create_count--;
			}
		}
	}

	if ( resetCounter ) {
		Com_Memset( &vk.pipelines, 0, sizeof( vk.pipelines ) );
		vk.pipelines_count = 0;
	}

	for ( i = 0; i < ARRAY_LEN( vk.blur_pipeline ); i++ ) {
		if ( vk.blur_pipeline[i] != VK_NULL_HANDLE ) {
			qvkDestroyPipeline( vk.device, vk.blur_pipeline[i], NULL );
			vk.blur_pipeline[i] = VK_NULL_HANDLE;
		}
	}
}


void vk_shutdown( refShutdownCode_t code )
{
	int i, j, k, l;

	if ( !vk.active ) { // not fully initialized
		goto __cleanup;
	}

	// Note: vk_shutdown_xr_resources() is called earlier in RE_Shutdown(),
	// before VKimp_Shutdown() destroys XR swapchains. This ensures VkImageViews
	// are destroyed while XR swapchain images are still valid.

	vk_destroy_framebuffers();

	vk_destroy_pipelines( qtrue ); // reset counter

	vk_destroy_render_passes();

	vk_destroy_attachments();

	if ( vk.pipelineCache != VK_NULL_HANDLE ) {
		qvkDestroyPipelineCache( vk.device, vk.pipelineCache, NULL );
		vk.pipelineCache = VK_NULL_HANDLE;
	}

	qvkDestroyCommandPool( vk.device, vk.command_pool, NULL );

	qvkDestroyDescriptorPool(vk.device, vk.descriptor_pool, NULL);

	qvkDestroyDescriptorSetLayout(vk.device, vk.set_layout_sampler, NULL);
	qvkDestroyDescriptorSetLayout(vk.device, vk.set_layout_uniform, NULL);
	qvkDestroyDescriptorSetLayout(vk.device, vk.set_layout_storage, NULL);
	if ( vk.set_layout_4samplers != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout(vk.device, vk.set_layout_4samplers, NULL);
		vk.set_layout_4samplers = VK_NULL_HANDLE;
	}

	qvkDestroyPipelineLayout(vk.device, vk.pipeline_layout, NULL);
	qvkDestroyPipelineLayout(vk.device, vk.pipeline_layout_storage, NULL);
	qvkDestroyPipelineLayout(vk.device, vk.pipeline_layout_post_process, NULL);
	qvkDestroyPipelineLayout(vk.device, vk.pipeline_layout_blend, NULL);

	// Post pass pipeline layouts
	if ( vk.pipeline_layout_fov_composite != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vk.pipeline_layout_fov_composite, NULL );
		vk.pipeline_layout_fov_composite = VK_NULL_HANDLE;
	}
	if ( vk.pipeline_layout_fov_gamma != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vk.pipeline_layout_fov_gamma, NULL );
		vk.pipeline_layout_fov_gamma = VK_NULL_HANDLE;
	}

#ifdef USE_VBO
	vk_release_vbo();
#endif

	vk_clean_staging_buffer();

	vk_release_geometry_buffers();

	vk_destroy_samplers();

	vk_destroy_sync_primitives();

	qvkDestroyBuffer( vk.device, vk.storage.buffer, NULL );
	qvkFreeMemory( vk.device, vk.storage.memory, NULL );

	for ( i = 0; i < 3; i++ ) {
		for ( j = 0; j < 2; j++ ) {
			for ( k = 0; k < 2; k++ ) {
				for ( l = 0; l < 2; l++ ) {
					if ( vk.modules.vert.gen[i][j][k][l] != VK_NULL_HANDLE ) {
						qvkDestroyShaderModule( vk.device, vk.modules.vert.gen[i][j][k][l], NULL );
						vk.modules.vert.gen[i][j][k][l] = VK_NULL_HANDLE;
					}
				}
			}
		}
	}
	for ( i = 0; i < 3; i++ ) {
		for ( j = 0; j < 2; j++ ) {
			for ( k = 0; k < 2; k++ ) {
				if ( vk.modules.frag.gen[i][j][k] != VK_NULL_HANDLE ) {
					qvkDestroyShaderModule( vk.device, vk.modules.frag.gen[i][j][k], NULL );
					vk.modules.frag.gen[i][j][k] = VK_NULL_HANDLE;
				}
			}
		}
	}
	for ( i = 0; i < 2; i++ ) {
		if ( vk.modules.vert.light[i] != VK_NULL_HANDLE ) {
			qvkDestroyShaderModule( vk.device, vk.modules.vert.light[i], NULL );
			vk.modules.vert.light[i] = VK_NULL_HANDLE;
		}
		for ( j = 0; j < 2; j++ ) {
			if ( vk.modules.frag.light[i][j] != VK_NULL_HANDLE ) {
				qvkDestroyShaderModule( vk.device, vk.modules.frag.light[i][j], NULL );
				vk.modules.frag.light[i][j] = VK_NULL_HANDLE;
			}
		}
	}

	for ( i = 0; i < 2; i++ ) {
		for ( j = 0; j < 2; j++ ) {
			for ( k = 0; k < 2; k++ ) {
				qvkDestroyShaderModule( vk.device, vk.modules.vert.ident1[i][j][k], NULL );
				vk.modules.vert.ident1[i][j][k] = VK_NULL_HANDLE;
			}
			qvkDestroyShaderModule( vk.device, vk.modules.frag.ident1[i][j], NULL );
			vk.modules.frag.ident1[i][j] = VK_NULL_HANDLE;
		}
	}

	for ( i = 0; i < 2; i++ ) {
		for ( j = 0; j < 2; j++ ) {
			for ( k = 0; k < 2; k++ ) {
				qvkDestroyShaderModule( vk.device, vk.modules.vert.fixed[i][j][k], NULL );
				vk.modules.vert.fixed[i][j][k] = VK_NULL_HANDLE;
			}
			qvkDestroyShaderModule( vk.device, vk.modules.frag.fixed[i][j], NULL );
			vk.modules.frag.fixed[i][j] = VK_NULL_HANDLE;
		}
	}

	for ( i = 0; i < 1; i++ ) {
		for ( j = 0; j < 2; j++ ) {
			qvkDestroyShaderModule( vk.device, vk.modules.frag.ent[i][j], NULL );
			vk.modules.frag.ent[i][j] = VK_NULL_HANDLE;
		}
	}

	qvkDestroyShaderModule( vk.device, vk.modules.frag.gen0_df, NULL );

	qvkDestroyShaderModule( vk.device, vk.modules.color_fs, NULL );
	qvkDestroyShaderModule( vk.device, vk.modules.color_vs, NULL );

	qvkDestroyShaderModule(vk.device, vk.modules.fog_vs, NULL);
	qvkDestroyShaderModule(vk.device, vk.modules.fog_fs, NULL);

	qvkDestroyShaderModule(vk.device, vk.modules.dot_vs, NULL);
	qvkDestroyShaderModule(vk.device, vk.modules.dot_fs, NULL);

	qvkDestroyShaderModule(vk.device, vk.modules.bloom_fs, NULL);
	qvkDestroyShaderModule(vk.device, vk.modules.blur_fs, NULL);
	qvkDestroyShaderModule(vk.device, vk.modules.blur_extract_fs, NULL);
	qvkDestroyShaderModule(vk.device, vk.modules.blend_fs, NULL);

	qvkDestroyShaderModule(vk.device, vk.modules.gamma_vs, NULL);
	qvkDestroyShaderModule(vk.device, vk.modules.gamma_fs, NULL);
	qvkDestroyShaderModule(vk.device, vk.modules.foveationdebug_fs, NULL);

	// Null when the density map feature is absent, which vkDestroyShaderModule allows
	qvkDestroyShaderModule(vk.device, vk.modules.final_composite_fov_fs, NULL);
	qvkDestroyShaderModule(vk.device, vk.modules.gamma_fov_fs, NULL);

__cleanup:
	if ( vk.device != VK_NULL_HANDLE ) {
		if ( !vk.xrMode ) {
			// Only destroy device if we created it (non-XR mode)
			qvkDestroyDevice( vk.device, NULL );
		}
		// In XR mode, VR layer owns the device: VR_Vulkan_Shutdown() handles destruction
	}

	deinit_device_functions();

	Com_Memset( &vk, 0, sizeof( vk ) );
	Com_Memset( &vk_world, 0, sizeof( vk_world ) );
	
	if ( code != REF_KEEP_CONTEXT ) {
		vk_destroy_instance();
		deinit_instance_functions();
	}
}


void vk_wait_idle( void )
{
	VK_CHECK( qvkDeviceWaitIdle( vk.device ) );
}


void vk_queue_wait_idle( void )
{
	VK_CHECK( qvkQueueWaitIdle( vk.queue ) );
}


// Precondition: callers must end (discard or finish) any in-flight frame
// first: the pool reset below frees every set, and descriptorsReady only
// guards frames that haven't started yet.
void vk_release_resources( void ) {
	int i, j;

	vk_wait_idle();

	for (i = 0; i < vk_world.num_image_chunks; i++)
		qvkFreeMemory(vk.device, vk_world.image_chunks[i].memory, NULL);

	vk_clean_staging_buffer();

	// vk_destroy_samplers();

	for ( i = vk.pipelines_world_base; i < vk.pipelines_count; i++ ) {
		for ( j = 0; j < RENDER_PASS_COUNT; j++ ) {
			if ( vk.pipelines[i].handle[j] != VK_NULL_HANDLE ) {
				qvkDestroyPipeline( vk.device, vk.pipelines[i].handle[j], NULL );
				vk.pipelines[i].handle[j] = VK_NULL_HANDLE;
				vk.pipeline_create_count--;
			}
		}
		Com_Memset( &vk.pipelines[i], 0, sizeof( vk.pipelines[0] ) );
	}
	vk.pipelines_count = vk.pipelines_world_base;

	VK_CHECK( qvkResetDescriptorPool( vk.device, vk.descriptor_pool, 0 ) );

	// Pool reset freed every set. vk_reallocate_xr_fbo_descriptors() below
	// restores only the bloom-chain sampler sets; the rest (storage, tess
	// uniform, screenmap) stay dead until R_Init reruns vk_init_descriptors().
	// NULL them so a gate bypass binds NULL rather than a freed handle.
	vk.descriptorsReady = qfalse;
	vk.storage.descriptor = VK_NULL_HANDLE;
	vk.screenMap.color_descriptor = VK_NULL_HANDLE;
	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		vk.tess[i].uniform_descriptor = VK_NULL_HANDLE;
	}
	// vk_reallocate_xr_fbo_descriptors() below only refills these when bloom
	// is currently enabled; if it's off, leave them NULL rather than freed
	for ( i = 0; i < ARRAY_LEN( vk.bloom_image_descriptor ); i++ ) {
		vk.bloom_image_descriptor[i] = VK_NULL_HANDLE;
	}
	vk.bloom_blur_combined_descriptor = VK_NULL_HANDLE;
	vk.xr.hudDescriptor = VK_NULL_HANDLE;

	// Reallocate the bloom-chain descriptor sets invalidated by the pool reset
	// (quest has no desktop mirror or virtual screen mirror descriptors)
	vk_reallocate_xr_fbo_descriptors();

	if ( vk_world.num_image_chunks > 1 ) {
		// if we allocated more than 2 image chunks - use doubled default size
		vk.image_chunk_size = (IMAGE_CHUNK_SIZE * 2);
	}

	Com_Memset( &vk_world, 0, sizeof( vk_world ) );

	// Reset geometry buffers offsets
	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		vk.tess[i].uniform_read_offset = 0;
		vk.tess[i].vertex_buffer_offset = 0;
	}

	Com_Memset( vk.cmd->buf_offset, 0, sizeof( vk.cmd->buf_offset ) );
	Com_Memset( vk.cmd->vbo_offset, 0, sizeof( vk.cmd->vbo_offset ) );

	Com_Memset( &vk.stats, 0, sizeof( vk.stats ) );
}


void vk_create_image( image_t *image, int width, int height, int mip_levels ) {

	VkFormat format = image->internalFormat;

	if ( image->handle ) {
		qvkDestroyImage( vk.device, image->handle, NULL );
		image->handle = VK_NULL_HANDLE;
	}

	if ( image->view ) {
		qvkDestroyImageView( vk.device, image->view, NULL );
		image->view = VK_NULL_HANDLE;
	}

	// create image
	{
		VkImageCreateInfo desc;

		desc.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = 0;
		desc.imageType = VK_IMAGE_TYPE_2D;
		desc.format = format;
		desc.extent.width = width;
		desc.extent.height = height;
		desc.extent.depth = 1;
		desc.mipLevels = mip_levels;
		desc.arrayLayers = 1;
		desc.samples = VK_SAMPLE_COUNT_1_BIT;
		desc.tiling = VK_IMAGE_TILING_OPTIMAL;
		desc.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		desc.queueFamilyIndexCount = 0;
		desc.pQueueFamilyIndices = NULL;
		desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		VK_CHECK( qvkCreateImage( vk.device, &desc, NULL, &image->handle ) );

		allocate_and_bind_image_memory( image->handle );
	}

	// create image view
	{
		VkImageViewCreateInfo desc;

		desc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = 0;
		desc.image = image->handle;
		desc.viewType = VK_IMAGE_VIEW_TYPE_2D;
		desc.format = format;
		desc.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		desc.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		desc.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		desc.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		desc.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		desc.subresourceRange.baseMipLevel = 0;
		desc.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
		desc.subresourceRange.baseArrayLayer = 0;
		desc.subresourceRange.layerCount = 1;

		VK_CHECK( qvkCreateImageView( vk.device, &desc, NULL, &image->view ) );
	}

	// create associated descriptor set
	if ( image->descriptor == VK_NULL_HANDLE ) {
		VkDescriptorSetAllocateInfo desc;

		desc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		desc.pNext = NULL;
		desc.descriptorPool = vk.descriptor_pool;
		desc.descriptorSetCount = 1;
		desc.pSetLayouts = &vk.set_layout_sampler;

		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &desc, &image->descriptor ) );
	}

	vk_update_descriptor_set( image, mip_levels > 1 ? qtrue : qfalse );

	SET_OBJECT_NAME( image->handle, image->imgName, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
	SET_OBJECT_NAME( image->view, image->imgName, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
	SET_OBJECT_NAME( image->descriptor, image->imgName, VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT );
}


static byte *resample_image_data( const int target_format, byte *data, const int data_size, int *bytes_per_pixel )
{
	byte* buffer;
	uint16_t* p;
	int i, n;

	switch ( target_format ) {
	case VK_FORMAT_B4G4R4A4_UNORM_PACK16:
		buffer = (byte*)ri.Hunk_AllocateTempMemory( data_size / 2 );
		p = (uint16_t*)buffer;
		for ( i = 0; i < data_size; i += 4, p++ ) {
			byte r = data[i + 0];
			byte g = data[i + 1];
			byte b = data[i + 2];
			byte a = data[i + 3];
			*p = (uint32_t)((a / 255.0) * 15.0 + 0.5) |
				((uint32_t)((r / 255.0) * 15.0 + 0.5) << 4) |
				((uint32_t)((g / 255.0) * 15.0 + 0.5) << 8) |
				((uint32_t)((b / 255.0) * 15.0 + 0.5) << 12);
		}
		*bytes_per_pixel = 2;
		return buffer; // must be freed after upload!

	case VK_FORMAT_A1R5G5B5_UNORM_PACK16:
		buffer = (byte*)ri.Hunk_AllocateTempMemory( data_size / 2 );
		p = (uint16_t*)buffer;
		for ( i = 0; i < data_size; i += 4, p++ ) {
			byte r = data[i + 0];
			byte g = data[i + 1];
			byte b = data[i + 2];
			*p = (uint32_t)((b / 255.0) * 31.0 + 0.5) |
				((uint32_t)((g / 255.0) * 31.0 + 0.5) << 5) |
				((uint32_t)((r / 255.0) * 31.0 + 0.5) << 10) |
				(1 << 15);
		}
		*bytes_per_pixel = 2;
		return buffer; // must be freed after upload!

	case VK_FORMAT_B8G8R8A8_UNORM:
		buffer = (byte*)ri.Hunk_AllocateTempMemory( data_size );
		for ( i = 0; i < data_size; i += 4 ) {
			buffer[i + 0] = data[i + 2];
			buffer[i + 1] = data[i + 1];
			buffer[i + 2] = data[i + 0];
			buffer[i + 3] = data[i + 3];
		}
		*bytes_per_pixel = 4;
		return buffer;

	case VK_FORMAT_R8G8B8_UNORM: {
		buffer = (byte*)ri.Hunk_AllocateTempMemory( (data_size * 3) / 4 );
		for ( i = 0, n = 0; i < data_size; i += 4, n += 3 ) {
			buffer[n + 0] = data[i + 0];
			buffer[n + 1] = data[i + 1];
			buffer[n + 2] = data[i + 2];
		}
		*bytes_per_pixel = 3;
		return buffer;
	}

	default:
		*bytes_per_pixel = 4;
		return data;
	}
}


void vk_upload_image_data( image_t *image, int x, int y, int width, int height, int mipmaps, byte *pixels, int size, qboolean update ) {

	VkCommandBuffer   command_buffer;
	VkBufferImageCopy regions[16];
	VkBufferImageCopy region;
	byte *buf;
	int n;

	int num_regions = 0;
	int buffer_size = 0;

	buf = resample_image_data( image->internalFormat, pixels, size, &n /*bpp*/ );

	while (qtrue) {
		Com_Memset(&region, 0, sizeof(region));
		region.bufferOffset = buffer_size;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = num_regions;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;
		region.imageOffset.x = x;
		region.imageOffset.y = y;
		region.imageOffset.z = 0;
		region.imageExtent.width = width;
		region.imageExtent.height = height;
		region.imageExtent.depth = 1;

		regions[num_regions] = region;
		num_regions++;

		buffer_size += width * height * n;

		if ( num_regions >= mipmaps || (width == 1 && height == 1) || num_regions >= ARRAY_LEN( regions ) )
			break;

		x >>= 1;
		y >>= 1;

		width >>= 1;
		if (width < 1) width = 1;

		height >>= 1;
		if (height < 1) height = 1;
	}

#ifdef USE_UPLOAD_QUEUE
	if ( vk_wait_staging_buffer() ) {
		// wait for vkQueueSubmit() completion before new upload
	}

	if ( vk.staging_buffer.size - vk.staging_buffer.offset < buffer_size ) {
		// try to flush staging buffer and reset offset
		vk_flush_staging_buffer( qfalse );
	}

	if ( vk.staging_buffer.size /* - vk_world.staging_buffer_offset */ < buffer_size ) {
		// if still not enough - reallocate staging buffer
		vk_alloc_staging_buffer( buffer_size );
	}

	for ( n = 0; n < num_regions; n++ ) {
		regions[n].bufferOffset += vk.staging_buffer.offset;
	}

	Com_Memcpy( vk.staging_buffer.ptr + vk.staging_buffer.offset, buf, buffer_size );

	if ( vk.staging_buffer.offset == 0 ) {
		VkCommandBufferBeginInfo begin_info;
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin_info.pNext = NULL;
		begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		begin_info.pInheritanceInfo = NULL;
		VK_CHECK( qvkBeginCommandBuffer( vk.staging_command_buffer, &begin_info ) );
	}

	//ri.Printf( PRINT_WARNING, "batch @%6i + %i %s \n", (int)vk_world.staging_buffer_offset, (int)buffer_size, image->imgName );
	vk.staging_buffer.offset += buffer_size;

	command_buffer = vk.staging_command_buffer;

	if ( update ) {
		record_image_layout_transition( command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 0 );
	} else {
		record_image_layout_transition( command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_HOST_BIT, 0 );
	}

	qvkCmdCopyBufferToImage( command_buffer, vk.staging_buffer.handle, image->handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, num_regions, regions );

	// final transition after upload comleted
	record_image_layout_transition( command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0 );
#else
	if ( vk.staging_buffer.size < buffer_size ) {
		vk_alloc_staging_buffer( buffer_size );
	}

	Com_Memcpy( vk.staging_buffer.ptr, buf, buffer_size );

	command_buffer = begin_command_buffer();
	// record_buffer_memory_barrier( command_buffer, vk_world.staging_buffer, VK_WHOLE_SIZE, 0, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT );
	if ( update ) {
		record_image_layout_transition( command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 0 );
	} else {
		record_image_layout_transition( command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_HOST_BIT, 0 );
	}
	qvkCmdCopyBufferToImage( command_buffer, vk.staging_buffer.handle, image->handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, num_regions, regions );
	record_image_layout_transition( command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0 );
	end_command_buffer( command_buffer, __func__ );
#endif

	if ( buf != pixels ) {
		ri.Hunk_FreeTempMemory( buf );
	}
}


void vk_update_descriptor_set( image_t *image, qboolean mipmap ) {
	Vk_Sampler_Def sampler_def;
	VkDescriptorImageInfo image_info;
	VkWriteDescriptorSet descriptor_write;

	Com_Memset( &sampler_def, 0, sizeof( sampler_def ) );

	sampler_def.address_mode = image->wrapClampMode;

	if ( mipmap ) {
		sampler_def.gl_mag_filter = gl_filter_max;
		sampler_def.gl_min_filter = gl_filter_min;
	} else {
		sampler_def.gl_mag_filter = GL_LINEAR;
		sampler_def.gl_min_filter = GL_LINEAR;
		// no anisotropy without mipmaps
		sampler_def.noAnisotropy = qtrue;
	}

	image_info.sampler = vk_find_sampler( &sampler_def );
	image_info.imageView = image->view;
	image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptor_write.dstSet = image->descriptor;
	descriptor_write.dstBinding = 0;
	descriptor_write.dstArrayElement = 0;
	descriptor_write.descriptorCount = 1;
	descriptor_write.pNext = NULL;
	descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptor_write.pImageInfo = &image_info;
	descriptor_write.pBufferInfo = NULL;
	descriptor_write.pTexelBufferView = NULL;

	qvkUpdateDescriptorSets( vk.device, 1, &descriptor_write, 0, NULL );
}


void vk_destroy_image_resources( VkImage *image, VkImageView *imageView )
{
	if ( image != NULL ) {
		if ( *image != VK_NULL_HANDLE ) {
			qvkDestroyImage( vk.device, *image, NULL );
			*image = VK_NULL_HANDLE;
		}
	}
	if ( imageView != NULL ) {
		if ( *imageView != VK_NULL_HANDLE ) {
			qvkDestroyImageView( vk.device, *imageView, NULL );
			*imageView = VK_NULL_HANDLE;
		}
	}
}


static void set_shader_stage_desc(VkPipelineShaderStageCreateInfo *desc, VkShaderStageFlagBits stage, VkShaderModule shader_module, const char *entry) {
	desc->sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	desc->pNext = NULL;
	desc->flags = 0;
	desc->stage = stage;
	desc->module = shader_module;
	desc->pName = entry;
	desc->pSpecializationInfo = NULL;
}


#define FORMAT_DEPTH(format, r_bits, g_bits, b_bits) case(VK_FORMAT_##format): *r = r_bits; *b = b_bits; *g = g_bits; return qtrue;
static qboolean vk_surface_format_color_depth( VkFormat format, int *r, int *g, int *b ) {
	switch (format) {
		// Common formats from https://vulkan.gpuinfo.org/listsurfaceformats.php
		FORMAT_DEPTH(B8G8R8A8_UNORM, 255, 255, 255)
			FORMAT_DEPTH(B8G8R8A8_SRGB, 255, 255, 255)
			FORMAT_DEPTH(A2B10G10R10_UNORM_PACK32, 1023, 1023, 1023)
			FORMAT_DEPTH(R8G8B8A8_UNORM, 255, 255, 255)
			FORMAT_DEPTH(R8G8B8A8_SRGB, 255, 255, 255)
			FORMAT_DEPTH(A2R10G10B10_UNORM_PACK32, 1023, 1023, 1023)
			FORMAT_DEPTH(R5G6B5_UNORM_PACK16, 31, 63, 31)
			FORMAT_DEPTH(R8G8B8A8_SNORM, 255, 255, 255)
			FORMAT_DEPTH(A8B8G8R8_UNORM_PACK32, 255, 255, 255)
			FORMAT_DEPTH(A8B8G8R8_SNORM_PACK32, 255, 255, 255)
			FORMAT_DEPTH(A8B8G8R8_SRGB_PACK32, 255, 255, 255)
			FORMAT_DEPTH(R16G16B16A16_UNORM, 65535, 65535, 65535)
			FORMAT_DEPTH(R16G16B16A16_SNORM, 65535, 65535, 65535)
			FORMAT_DEPTH(B5G6R5_UNORM_PACK16, 31, 63, 31)
			FORMAT_DEPTH(B8G8R8A8_SNORM, 255, 255, 255)
			FORMAT_DEPTH(R4G4B4A4_UNORM_PACK16, 15, 15, 15)
			FORMAT_DEPTH(B4G4R4A4_UNORM_PACK16, 15, 15, 15)
			FORMAT_DEPTH(A1R5G5B5_UNORM_PACK16, 31, 31, 31)
			FORMAT_DEPTH(R5G5B5A1_UNORM_PACK16, 31, 31, 31)
			FORMAT_DEPTH(B5G5R5A1_UNORM_PACK16, 31, 31, 31)
	default:
		*r = 255; *g = 255; *b = 255; return qfalse;
	}
}


/*
 * vk_create_post_process_pipelines - Create post-processing pipelines for multiview
 *
 * Creates gamma, bloom extract, blur, and blend pipelines using multiview render passes.
 * Called when r_fbo is active.
 */
void vk_create_post_process_pipelines( void )
{
	VkPipelineShaderStageCreateInfo shader_stages[2];
	VkPipelineVertexInputStateCreateInfo vertex_input_state;
	VkPipelineInputAssemblyStateCreateInfo input_assembly_state;
	VkPipelineRasterizationStateCreateInfo rasterization_state;
	VkPipelineDepthStencilStateCreateInfo depth_stencil_state;
	VkPipelineViewportStateCreateInfo viewport_state;
	VkPipelineMultisampleStateCreateInfo multisample_state;
	VkPipelineColorBlendStateCreateInfo blend_state;
	VkPipelineColorBlendAttachmentState attachment_blend_state;
	VkGraphicsPipelineCreateInfo create_info;
	VkViewport viewport;
	VkRect2D scissor;
	VkSpecializationMapEntry spec_entries[11];
	VkSpecializationInfo frag_spec_info;
	uint32_t i, width, height;

	struct FragSpecData {
		float gamma;
		float overbright;
		float greyscale;
		float bloom_threshold;
		float bloom_intensity;
		int bloom_threshold_mode;
		int bloom_modulate;
		int dither;
		int depth_r;
		int depth_g;
		int depth_b;
	} frag_spec_data;

	// Note: This is called during vk_init_xr_resources() before vk.xr.initialized is set
	// So we check the prerequisites directly instead of vk.xr.initialized
	if ( vk.xr.width == 0 || vk.xr.height == 0 ) {
		ri.Printf( PRINT_WARNING, "vk_create_post_process_pipelines: XR dimensions not set\n" );
		return;
	}

	if ( vk.render_pass.gamma == VK_NULL_HANDLE ) {
		ri.Printf( PRINT_WARNING, "vk_create_post_process_pipelines: gamma render pass not created\n" );
		return;
	}

	width = vk.xr.width;
	height = vk.xr.height;

	// Common vertex input state (no vertex input for fullscreen quad)
	vertex_input_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertex_input_state.pNext = NULL;
	vertex_input_state.flags = 0;
	vertex_input_state.vertexBindingDescriptionCount = 0;
	vertex_input_state.pVertexBindingDescriptions = NULL;
	vertex_input_state.vertexAttributeDescriptionCount = 0;
	vertex_input_state.pVertexAttributeDescriptions = NULL;

	// Common input assembly state
	input_assembly_state.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly_state.pNext = NULL;
	input_assembly_state.flags = 0;
	input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
	input_assembly_state.primitiveRestartEnable = VK_FALSE;

	// Common rasterization state
	rasterization_state.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterization_state.pNext = NULL;
	rasterization_state.flags = 0;
	rasterization_state.depthClampEnable = VK_FALSE;
	rasterization_state.rasterizerDiscardEnable = VK_FALSE;
	rasterization_state.polygonMode = VK_POLYGON_MODE_FILL;
	rasterization_state.cullMode = VK_CULL_MODE_NONE;
	rasterization_state.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterization_state.depthBiasEnable = VK_FALSE;
	rasterization_state.depthBiasConstantFactor = 0.0f;
	rasterization_state.depthBiasClamp = 0.0f;
	rasterization_state.depthBiasSlopeFactor = 0.0f;
	rasterization_state.lineWidth = 1.0f;

	// Common multisample state
	multisample_state.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample_state.pNext = NULL;
	multisample_state.flags = 0;
	multisample_state.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisample_state.sampleShadingEnable = VK_FALSE;
	multisample_state.minSampleShading = 1.0f;
	multisample_state.pSampleMask = NULL;
	multisample_state.alphaToCoverageEnable = VK_FALSE;
	multisample_state.alphaToOneEnable = VK_FALSE;

	// Common depth/stencil state (disabled)
	Com_Memset( &depth_stencil_state, 0, sizeof( depth_stencil_state ) );
	depth_stencil_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depth_stencil_state.depthTestEnable = VK_FALSE;
	depth_stencil_state.depthWriteEnable = VK_FALSE;
	depth_stencil_state.depthCompareOp = VK_COMPARE_OP_NEVER;

	// Setup specialization constants for gamma/bloom
	frag_spec_data.gamma = 1.0 / (r_gamma->value);
	frag_spec_data.overbright = (float)(1 << tr.overbrightBits);
	frag_spec_data.greyscale = r_greyscale->value;
	ri.Printf( PRINT_ALL, "Gamma pipeline specialization: gamma=%.3f (r_gamma=%.2f), overbright=%.1f (bits=%d), greyscale=%.2f\n",
		frag_spec_data.gamma, r_gamma->value, frag_spec_data.overbright, tr.overbrightBits, frag_spec_data.greyscale );
	frag_spec_data.bloom_threshold = r_bloom_threshold->value;
	// Compensate for multiview doubling effect in bloom blend pass
	frag_spec_data.bloom_intensity = r_bloom_intensity->value * 0.5f;
	frag_spec_data.bloom_threshold_mode = r_bloom_threshold_mode->integer;
	frag_spec_data.bloom_modulate = r_bloom_modulate->integer;
	frag_spec_data.dither = r_dither->integer;

	// Get color depth from XR swapchain format
	if ( vk.xr.colorInfo && !vk_surface_format_color_depth( vk.xr.colorInfo->format,
			&frag_spec_data.depth_r, &frag_spec_data.depth_g, &frag_spec_data.depth_b ) ) {
		frag_spec_data.depth_r = frag_spec_data.depth_g = frag_spec_data.depth_b = 255;
	}

	spec_entries[0].constantID = 0;
	spec_entries[0].offset = offsetof( struct FragSpecData, gamma );
	spec_entries[0].size = sizeof( frag_spec_data.gamma );

	spec_entries[1].constantID = 1;
	spec_entries[1].offset = offsetof( struct FragSpecData, overbright );
	spec_entries[1].size = sizeof( frag_spec_data.overbright );

	spec_entries[2].constantID = 2;
	spec_entries[2].offset = offsetof( struct FragSpecData, greyscale );
	spec_entries[2].size = sizeof( frag_spec_data.greyscale );

	spec_entries[3].constantID = 3;
	spec_entries[3].offset = offsetof( struct FragSpecData, bloom_threshold );
	spec_entries[3].size = sizeof( frag_spec_data.bloom_threshold );

	spec_entries[4].constantID = 4;
	spec_entries[4].offset = offsetof( struct FragSpecData, bloom_intensity );
	spec_entries[4].size = sizeof( frag_spec_data.bloom_intensity );

	spec_entries[5].constantID = 5;
	spec_entries[5].offset = offsetof( struct FragSpecData, bloom_threshold_mode );
	spec_entries[5].size = sizeof( frag_spec_data.bloom_threshold_mode );

	spec_entries[6].constantID = 6;
	spec_entries[6].offset = offsetof( struct FragSpecData, bloom_modulate );
	spec_entries[6].size = sizeof( frag_spec_data.bloom_modulate );

	spec_entries[7].constantID = 7;
	spec_entries[7].offset = offsetof( struct FragSpecData, dither );
	spec_entries[7].size = sizeof( frag_spec_data.dither );

	spec_entries[8].constantID = 8;
	spec_entries[8].offset = offsetof( struct FragSpecData, depth_r );
	spec_entries[8].size = sizeof( frag_spec_data.depth_r );

	spec_entries[9].constantID = 9;
	spec_entries[9].offset = offsetof( struct FragSpecData, depth_g );
	spec_entries[9].size = sizeof( frag_spec_data.depth_g );

	spec_entries[10].constantID = 10;
	spec_entries[10].offset = offsetof( struct FragSpecData, depth_b );
	spec_entries[10].size = sizeof( frag_spec_data.depth_b );

	frag_spec_info.mapEntryCount = 11;
	frag_spec_info.pMapEntries = spec_entries;
	frag_spec_info.dataSize = sizeof( frag_spec_data );
	frag_spec_info.pData = &frag_spec_data;

	// Common pipeline create info template
	Com_Memset( &create_info, 0, sizeof( create_info ) );
	create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	create_info.stageCount = 2;
	create_info.pStages = shader_stages;
	create_info.pVertexInputState = &vertex_input_state;
	create_info.pInputAssemblyState = &input_assembly_state;
	create_info.pRasterizationState = &rasterization_state;
	create_info.pMultisampleState = &multisample_state;
	create_info.pDepthStencilState = &depth_stencil_state;

	// Set up common viewport/scissor/blend state (used by both old and new paths)
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)width;
	viewport.height = (float)height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent.width = width;
	scissor.extent.height = height;

	viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_state.pNext = NULL;
	viewport_state.flags = 0;
	viewport_state.viewportCount = 1;
	viewport_state.pViewports = &viewport;
	viewport_state.scissorCount = 1;
	viewport_state.pScissors = &scissor;

	Com_Memset( &attachment_blend_state, 0, sizeof( attachment_blend_state ) );
	attachment_blend_state.blendEnable = VK_FALSE;
	attachment_blend_state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
											VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	blend_state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blend_state.pNext = NULL;
	blend_state.flags = 0;
	blend_state.logicOpEnable = VK_FALSE;
	blend_state.logicOp = VK_LOGIC_OP_COPY;
	blend_state.attachmentCount = 1;
	blend_state.pAttachments = &attachment_blend_state;
	Com_Memset( blend_state.blendConstants, 0, sizeof( blend_state.blendConstants ) );

	create_info.pViewportState = &viewport_state;
	create_info.pColorBlendState = &blend_state;

	// =====================================================================
	// 1. XR Blur Pipelines - used for bloom processing
	// (Blur passes run after combined render pass to prepare bloom for next frame)
	// =====================================================================
	if ( r_bloom->integer ) {
		for ( i = 0; i < VK_NUM_BLOOM_PASSES * 2; i++ ) {
			float blur_spec_data[3];
			VkSpecializationMapEntry blur_spec_entries[3];
			VkSpecializationInfo blur_spec_info;
			uint32_t blur_width, blur_height;
			qboolean horizontal = (i % 2) == 0;

			if ( vk.blur_pipeline[i] != VK_NULL_HANDLE ) {
				vk_wait_idle();
				qvkDestroyPipeline( vk.device, vk.blur_pipeline[i], NULL );
				vk.blur_pipeline[i] = VK_NULL_HANDLE;
			}

			// Sized from the bloom images this renders into, which vk_create_attachments and
			// the blur framebuffers both derive from gls.capture, as does vk.renderWidth at
			// record time. width/height above is the swapchain, for the passes that target it.
			blur_width = gls.captureWidth / ( 2 << ( i / 2 ) );
			blur_height = gls.captureHeight / ( 2 << ( i / 2 ) );

			// Offsets are in source texels; the horizontal passes downsample 2:1
			blur_spec_data[0] = 1.2f / (float)( blur_width * 2 );  // x offset
			blur_spec_data[1] = 1.2f / (float)blur_height;         // y offset
			blur_spec_data[2] = 1.0f;                              // intensity

			if ( horizontal ) {
				blur_spec_data[1] = 0.0f;
			} else {
				blur_spec_data[0] = 0.0f;
			}

			blur_spec_entries[0].constantID = 0;
			blur_spec_entries[0].offset = 0;
			blur_spec_entries[0].size = sizeof( float );

			blur_spec_entries[1].constantID = 1;
			blur_spec_entries[1].offset = sizeof( float );
			blur_spec_entries[1].size = sizeof( float );

			blur_spec_entries[2].constantID = 2;
			blur_spec_entries[2].offset = 2 * sizeof( float );
			blur_spec_entries[2].size = sizeof( float );

			blur_spec_info.mapEntryCount = 3;
			blur_spec_info.pMapEntries = blur_spec_entries;
			blur_spec_info.dataSize = sizeof( blur_spec_data );
			blur_spec_info.pData = blur_spec_data;

			set_shader_stage_desc( shader_stages+0, VK_SHADER_STAGE_VERTEX_BIT, vk.modules.gamma_vs, "main" );
			set_shader_stage_desc( shader_stages+1, VK_SHADER_STAGE_FRAGMENT_BIT, vk.modules.blur_fs, "main" );
			shader_stages[1].pSpecializationInfo = &blur_spec_info;

			// Foveated split: the first blur pass extracts as it samples the stored scene (blur.frag USE_EXTRACT)
			struct BlurExtractSpec {
				float blur[3];
				float threshold;
				int mode;
				int modulate;
			} blur_extract_data;
			VkSpecializationMapEntry blur_extract_entries[6];
			VkSpecializationInfo blur_extract_info;

			if ( i == 0 ) {
				Com_Memcpy( blur_extract_data.blur, blur_spec_data, sizeof( blur_spec_data ) );
				blur_extract_data.threshold = frag_spec_data.bloom_threshold;
				blur_extract_data.mode = frag_spec_data.bloom_threshold_mode;
				blur_extract_data.modulate = frag_spec_data.bloom_modulate;

				Com_Memcpy( blur_extract_entries, blur_spec_entries, sizeof( blur_spec_entries ) );
				blur_extract_entries[3].constantID = 3;
				blur_extract_entries[3].offset = offsetof( struct BlurExtractSpec, threshold );
				blur_extract_entries[3].size = sizeof( blur_extract_data.threshold );
				blur_extract_entries[4].constantID = 5;
				blur_extract_entries[4].offset = offsetof( struct BlurExtractSpec, mode );
				blur_extract_entries[4].size = sizeof( blur_extract_data.mode );
				blur_extract_entries[5].constantID = 6;
				blur_extract_entries[5].offset = offsetof( struct BlurExtractSpec, modulate );
				blur_extract_entries[5].size = sizeof( blur_extract_data.modulate );

				blur_extract_info.mapEntryCount = 6;
				blur_extract_info.pMapEntries = blur_extract_entries;
				blur_extract_info.dataSize = sizeof( blur_extract_data );
				blur_extract_info.pData = &blur_extract_data;

				set_shader_stage_desc( shader_stages+1, VK_SHADER_STAGE_FRAGMENT_BIT, vk.modules.blur_extract_fs, "main" );
				shader_stages[1].pSpecializationInfo = &blur_extract_info;
			}

			viewport.width = (float)blur_width;
			viewport.height = (float)blur_height;
			scissor.extent.width = blur_width;
			scissor.extent.height = blur_height;

			create_info.layout = vk.pipeline_layout_post_process;
			create_info.renderPass = vk.render_pass.blur[i];

			VK_CHECK( qvkCreateGraphicsPipelines( vk.device, VK_NULL_HANDLE, 1, &create_info, NULL, &vk.blur_pipeline[i] ) );
			SET_OBJECT_NAME( vk.blur_pipeline[i], va( "XR %s blur pipeline %i", horizontal ? "horizontal" : "vertical", i/2 + 1 ), VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );
		}
	}

	// =====================================================================
	// 2. Subpass Post-Processing Pipelines
	// =====================================================================
	{
		VkRenderPass subpassRenderPass;

		// Reset blend state to non-blending for subpass pipelines
		attachment_blend_state.blendEnable = VK_FALSE;
		multisample_state.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

		// Reset viewport/scissor to full resolution
		viewport.width = (float)width;
		viewport.height = (float)height;
		scissor.extent.width = width;
		scissor.extent.height = height;

		if ( r_bloom->integer && vk.render_pass.main_with_bloom != VK_NULL_HANDLE ) {
			subpassRenderPass = vk.render_pass.main_with_bloom;

			if ( vk.final_composite_subpass_pipeline != VK_NULL_HANDLE ) {
				vk_wait_idle();
				qvkDestroyPipeline( vk.device, vk.final_composite_subpass_pipeline, NULL );
				vk.final_composite_subpass_pipeline = VK_NULL_HANDLE;
			}

			set_shader_stage_desc( shader_stages+0, VK_SHADER_STAGE_VERTEX_BIT, vk.modules.gamma_vs, "main" );
			set_shader_stage_desc( shader_stages+1, VK_SHADER_STAGE_FRAGMENT_BIT, vk.modules.final_composite_fov_fs, "main" );
			shader_stages[1].pSpecializationInfo = &frag_spec_info;

			create_info.renderPass = subpassRenderPass;
			create_info.layout = vk.pipeline_layout_fov_composite;
			create_info.subpass = 0;

			VK_CHECK( qvkCreateGraphicsPipelines( vk.device, VK_NULL_HANDLE, 1, &create_info, NULL, &vk.final_composite_subpass_pipeline ) );
			SET_OBJECT_NAME( vk.final_composite_subpass_pipeline, "final composite pipeline", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );
		}
		else if ( vk.render_pass.main_with_gamma != VK_NULL_HANDLE ) {
			subpassRenderPass = vk.render_pass.main_with_gamma;

			if ( vk.gamma_subpass_pipeline != VK_NULL_HANDLE ) {
				vk_wait_idle();
				qvkDestroyPipeline( vk.device, vk.gamma_subpass_pipeline, NULL );
				vk.gamma_subpass_pipeline = VK_NULL_HANDLE;
			}

			set_shader_stage_desc( shader_stages+0, VK_SHADER_STAGE_VERTEX_BIT, vk.modules.gamma_vs, "main" );
			set_shader_stage_desc( shader_stages+1, VK_SHADER_STAGE_FRAGMENT_BIT, vk.modules.gamma_fov_fs, "main" );
			shader_stages[1].pSpecializationInfo = &frag_spec_info;

			create_info.renderPass = subpassRenderPass;
			create_info.layout = vk.pipeline_layout_fov_gamma;
			create_info.subpass = 0;

			VK_CHECK( qvkCreateGraphicsPipelines( vk.device, VK_NULL_HANDLE, 1, &create_info, NULL, &vk.gamma_subpass_pipeline ) );
			SET_OBJECT_NAME( vk.gamma_subpass_pipeline, "gamma pipeline", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );
		}

		// Reset subpass to 0 for future pipeline creation
		create_info.subpass = 0;
	}
}


/*
==================
vk_create_foveation_debug_pipeline

The r_foveationDebug tint, built on first enable rather than alongside the rest: a view
nobody asks for should cost the binary its shader and no more. The first frame after the
cvar goes on hitches, which is the right trade.

It belongs to whichever pass carries the density map -- fov_scene under r_fbo 1, main
under r_fbo 0 -- since that is where gl_FragSizeEXT reports the map's fragment.
==================
*/
static void vk_create_foveation_debug_pipeline( VkRenderPass renderPass )
{
	VkPipelineShaderStageCreateInfo shader_stages[2];
	VkPipelineVertexInputStateCreateInfo vertex_input_state;
	VkPipelineInputAssemblyStateCreateInfo input_assembly_state;
	VkPipelineRasterizationStateCreateInfo rasterization_state;
	VkPipelineDepthStencilStateCreateInfo depth_stencil_state;
	VkPipelineViewportStateCreateInfo viewport_state;
	VkPipelineMultisampleStateCreateInfo multisample_state;
	VkPipelineColorBlendStateCreateInfo blend_state;
	VkPipelineColorBlendAttachmentState attachment_blend_state;
	VkGraphicsPipelineCreateInfo create_info;
	VkViewport viewport;
	VkRect2D scissor;

	Com_Memset( &vertex_input_state, 0, sizeof( vertex_input_state ) );
	vertex_input_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

	Com_Memset( &input_assembly_state, 0, sizeof( input_assembly_state ) );
	input_assembly_state.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

	Com_Memset( &rasterization_state, 0, sizeof( rasterization_state ) );
	rasterization_state.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterization_state.polygonMode = VK_POLYGON_MODE_FILL;
	rasterization_state.cullMode = VK_CULL_MODE_NONE;
	rasterization_state.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterization_state.lineWidth = 1.0f;

	// The scene pass's sample count
	Com_Memset( &multisample_state, 0, sizeof( multisample_state ) );
	multisample_state.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample_state.rasterizationSamples = vk.msaaActive ? vkSamples : VK_SAMPLE_COUNT_1_BIT;
	multisample_state.minSampleShading = 1.0f;

	// Goes over everything already in the pass: nothing to test or write
	Com_Memset( &depth_stencil_state, 0, sizeof( depth_stencil_state ) );
	depth_stencil_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depth_stencil_state.depthCompareOp = VK_COMPARE_OP_NEVER;

	set_shader_stage_desc( shader_stages+0, VK_SHADER_STAGE_VERTEX_BIT, vk.modules.gamma_vs, "main" );
	set_shader_stage_desc( shader_stages+1, VK_SHADER_STAGE_FRAGMENT_BIT, vk.modules.foveationdebug_fs, "main" );

	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)vk.xr.width;
	viewport.height = (float)vk.xr.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent.width = vk.xr.width;
	scissor.extent.height = vk.xr.height;

	Com_Memset( &viewport_state, 0, sizeof( viewport_state ) );
	viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_state.viewportCount = 1;
	viewport_state.pViewports = &viewport;
	viewport_state.scissorCount = 1;
	viewport_state.pScissors = &scissor;

	// Tints rather than replaces
	Com_Memset( &attachment_blend_state, 0, sizeof( attachment_blend_state ) );
	attachment_blend_state.blendEnable = VK_TRUE;
	attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	attachment_blend_state.colorBlendOp = VK_BLEND_OP_ADD;
	attachment_blend_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	attachment_blend_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	attachment_blend_state.alphaBlendOp = VK_BLEND_OP_ADD;
	attachment_blend_state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	Com_Memset( &blend_state, 0, sizeof( blend_state ) );
	blend_state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blend_state.attachmentCount = 1;
	blend_state.pAttachments = &attachment_blend_state;

	Com_Memset( &create_info, 0, sizeof( create_info ) );
	create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	create_info.stageCount = 2;
	create_info.pStages = shader_stages;
	create_info.pVertexInputState = &vertex_input_state;
	create_info.pInputAssemblyState = &input_assembly_state;
	create_info.pViewportState = &viewport_state;
	create_info.pRasterizationState = &rasterization_state;
	create_info.pMultisampleState = &multisample_state;
	create_info.pDepthStencilState = &depth_stencil_state;
	create_info.pColorBlendState = &blend_state;
	create_info.layout = vk.pipeline_layout_post_process;
	create_info.renderPass = renderPass;
	create_info.subpass = 0;

	if ( qvkCreateGraphicsPipelines( vk.device, VK_NULL_HANDLE, 1, &create_info, NULL,
			&vk.foveation_debug_pipeline ) != VK_SUCCESS ) {
		ri.Printf( PRINT_WARNING, "r_foveationDebug: the tint pipeline could not be created\n" );
		vk.foveation_debug_pipeline = VK_NULL_HANDLE;
		return;
	}
	vk.foveation_debug_pass = renderPass;
	SET_OBJECT_NAME( vk.foveation_debug_pipeline, "foveation debug tint pipeline", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );
}

/*
==================
vk_draw_foveation_debug

Last thing inside the foveated pass, so the tint covers whatever the frame drew.
==================
*/
void vk_draw_foveation_debug( void )
{
	// The pass that carries the map: the split scene pass, or the main pass in direct mode
	VkRenderPass pass = vk.fboActive ? vk.render_pass.fov_scene : vk.render_pass.main;

	if ( !r_foveationDebug->integer || !vk.xr.foveationActive || !vk.inRenderPass ) {
		return;
	}
	if ( pass == VK_NULL_HANDLE || vk.cmd == NULL || vk.cmd->command_buffer == VK_NULL_HANDLE ) {
		return;
	}

	// A pass that has been recreated under us leaves the pipeline pointing at nothing
	if ( vk.foveation_debug_pipeline != VK_NULL_HANDLE && vk.foveation_debug_pass != pass ) {
		vk_wait_idle();
		qvkDestroyPipeline( vk.device, vk.foveation_debug_pipeline, NULL );
		vk.foveation_debug_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.foveation_debug_pipeline == VK_NULL_HANDLE ) {
		vk_create_foveation_debug_pipeline( pass );
	}
	if ( vk.foveation_debug_pipeline == VK_NULL_HANDLE ) {
		return;
	}

	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.foveation_debug_pipeline );
	qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
	vk.cmd->last_pipeline = VK_NULL_HANDLE;
}

/*
 * vk_destroy_post_process_pipelines - Clean up post-processing pipelines
 */
static void vk_destroy_post_process_pipelines( void )
{
	uint32_t i;

	if ( vk.foveation_debug_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.foveation_debug_pipeline, NULL );
		vk.foveation_debug_pipeline = VK_NULL_HANDLE;
		vk.foveation_debug_pass = VK_NULL_HANDLE;
	}

	for ( i = 0; i < VK_NUM_BLOOM_PASSES * 2; i++ ) {
		if ( vk.blur_pipeline[i] != VK_NULL_HANDLE ) {
			qvkDestroyPipeline( vk.device, vk.blur_pipeline[i], NULL );
			vk.blur_pipeline[i] = VK_NULL_HANDLE;
		}
	}

	// Post pass pipelines
	if ( vk.final_composite_subpass_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.final_composite_subpass_pipeline, NULL );
		vk.final_composite_subpass_pipeline = VK_NULL_HANDLE;
	}

	if ( vk.gamma_subpass_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.gamma_subpass_pipeline, NULL );
		vk.gamma_subpass_pipeline = VK_NULL_HANDLE;
	}
}


static VkVertexInputBindingDescription bindings[8];
static VkVertexInputAttributeDescription attribs[8];
static uint32_t num_binds;
static uint32_t num_attrs;

static void push_bind( uint32_t binding, uint32_t stride )
{
	bindings[ num_binds ].binding = binding;
	bindings[ num_binds ].stride = stride;
	bindings[ num_binds ].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	num_binds++;
}

static void push_attr( uint32_t location, uint32_t binding, VkFormat format )
{
	attribs[ num_attrs ].location = location;
	attribs[ num_attrs ].binding = binding;
	attribs[ num_attrs ].format = format;
	attribs[ num_attrs ].offset = 0;
	num_attrs++;
}


VkPipeline create_pipeline( const Vk_Pipeline_Def *def, renderPass_t renderPassIndex, uint32_t def_index ) {
	VkShaderModule *vs_module = NULL;
	VkShaderModule *fs_module = NULL;
	//int32_t vert_spec_data[1]; // clippping
	floatint_t frag_spec_data[19]; // 0:alpha-test-func, 1:alpha-test-value, 2:depth-fragment, 3:alpha-to-coverage, 4:color_mode, 5:abs_light, 6:multitexture mode, 7:discard mode, 8: ident.color, 9 - ident.alpha, 10 - acff, 11 - force_opaque_alpha (HUD 3D), 12 - post_bloom_gamma, 13 - post_bloom_obScale, 14 - post_bloom_greyscale, 15 - post_bloom_dither, 16-18 - post_bloom_depth_rgb
	VkSpecializationMapEntry spec_entries[20];
	//VkSpecializationInfo vert_spec_info;
	VkSpecializationInfo frag_spec_info;
	VkPipelineVertexInputStateCreateInfo vertex_input_state;
	VkPipelineInputAssemblyStateCreateInfo input_assembly_state;
	VkPipelineRasterizationStateCreateInfo rasterization_state;
	VkPipelineViewportStateCreateInfo viewport_state;
	VkPipelineMultisampleStateCreateInfo multisample_state;
	VkPipelineDepthStencilStateCreateInfo depth_stencil_state;
	VkPipelineColorBlendStateCreateInfo blend_state;
	VkPipelineColorBlendAttachmentState attachment_blend_state;
	VkPipelineDynamicStateCreateInfo dynamic_state;
	VkDynamicState dynamic_state_array[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkGraphicsPipelineCreateInfo create_info;
	VkPipeline pipeline;
	VkPipelineShaderStageCreateInfo shader_stages[2];
	VkBool32 alphaToCoverage = VK_FALSE;
	unsigned int atest_bits;
	unsigned int state_bits = def->state_bits;

	// All shaders use multiview.
	// gl_ViewIndex is 0 for single-layer targets.

	switch ( def->shader_type ) {

		case TYPE_SIGNLE_TEXTURE_LIGHTING:
			vs_module = &vk.modules.vert.light[0];
			fs_module = &vk.modules.frag.light[0][0];
			break;

		case TYPE_SIGNLE_TEXTURE_LIGHTING_LINEAR:
			vs_module = &vk.modules.vert.light[0];
			fs_module = &vk.modules.frag.light[1][0];
			break;

		case TYPE_SIGNLE_TEXTURE_DF:
			state_bits |= GLS_DEPTHMASK_TRUE;
			vs_module = &vk.modules.vert.ident1[0][0][0];
			fs_module = &vk.modules.frag.gen0_df;
			break;

		case TYPE_SIGNLE_TEXTURE_FIXED_COLOR:
			vs_module = &vk.modules.vert.fixed[0][0][0];
			fs_module = &vk.modules.frag.fixed[0][0];
			break;

		case TYPE_SIGNLE_TEXTURE_FIXED_COLOR_ENV:
			vs_module = &vk.modules.vert.fixed[0][1][0];
			fs_module = &vk.modules.frag.fixed[0][0];
			break;

		case TYPE_SIGNLE_TEXTURE_ENT_COLOR:
			vs_module = &vk.modules.vert.fixed[0][0][0];
			fs_module = &vk.modules.frag.ent[0][0];
			break;

		case TYPE_SIGNLE_TEXTURE_ENT_COLOR_ENV:
			vs_module = &vk.modules.vert.fixed[0][1][0];
			fs_module = &vk.modules.frag.ent[0][0];
			break;

		case TYPE_SIGNLE_TEXTURE:
			vs_module = &vk.modules.vert.gen[0][0][0][0];
			fs_module = &vk.modules.frag.gen[0][0][0];
			break;

		case TYPE_SIGNLE_TEXTURE_ENV:
			vs_module = &vk.modules.vert.gen[0][0][1][0];
			fs_module = &vk.modules.frag.gen[0][0][0];
			break;

		case TYPE_SIGNLE_TEXTURE_IDENTITY:
			vs_module = &vk.modules.vert.ident1[0][0][0];
			fs_module = &vk.modules.frag.ident1[0][0];
			break;

		case TYPE_SIGNLE_TEXTURE_IDENTITY_ENV:
			vs_module = &vk.modules.vert.ident1[0][1][0];
			fs_module = &vk.modules.frag.ident1[0][0];
			break;

		case TYPE_MULTI_TEXTURE_ADD2_IDENTITY:
		case TYPE_MULTI_TEXTURE_MUL2_IDENTITY:
			vs_module = &vk.modules.vert.ident1[1][0][0];
			fs_module = &vk.modules.frag.ident1[1][0];
			break;

		case TYPE_MULTI_TEXTURE_ADD2_IDENTITY_ENV:
		case TYPE_MULTI_TEXTURE_MUL2_IDENTITY_ENV:
			vs_module = &vk.modules.vert.ident1[1][1][0];
			fs_module = &vk.modules.frag.ident1[1][0];
			break;

		case TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR:
		case TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR:
			vs_module = &vk.modules.vert.fixed[1][0][0];
			fs_module = &vk.modules.frag.fixed[1][0];
			break;

		case TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR_ENV:
		case TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR_ENV:
			vs_module = &vk.modules.vert.fixed[1][1][0];
			fs_module = &vk.modules.frag.fixed[1][0];
			break;

		case TYPE_MULTI_TEXTURE_MUL2:
		case TYPE_MULTI_TEXTURE_ADD2_1_1:
		case TYPE_MULTI_TEXTURE_ADD2:
			vs_module = &vk.modules.vert.gen[1][0][0][0];
			fs_module = &vk.modules.frag.gen[1][0][0];
			break;

		case TYPE_MULTI_TEXTURE_MUL2_ENV:
		case TYPE_MULTI_TEXTURE_ADD2_1_1_ENV:
		case TYPE_MULTI_TEXTURE_ADD2_ENV:
			vs_module = &vk.modules.vert.gen[1][0][1][0];
			fs_module = &vk.modules.frag.gen[1][0][0];
			break;

		case TYPE_MULTI_TEXTURE_MUL3:
		case TYPE_MULTI_TEXTURE_ADD3_1_1:
		case TYPE_MULTI_TEXTURE_ADD3:
			vs_module = &vk.modules.vert.gen[2][0][0][0];
			fs_module = &vk.modules.frag.gen[2][0][0];
			break;

		case TYPE_MULTI_TEXTURE_MUL3_ENV:
		case TYPE_MULTI_TEXTURE_ADD3_1_1_ENV:
		case TYPE_MULTI_TEXTURE_ADD3_ENV:
			vs_module = &vk.modules.vert.gen[2][0][1][0];
			fs_module = &vk.modules.frag.gen[2][0][0];
			break;

		case TYPE_BLEND2_ADD:
		case TYPE_BLEND2_MUL:
		case TYPE_BLEND2_ALPHA:
		case TYPE_BLEND2_ONE_MINUS_ALPHA:
		case TYPE_BLEND2_MIX_ALPHA:
		case TYPE_BLEND2_MIX_ONE_MINUS_ALPHA:
		case TYPE_BLEND2_DST_COLOR_SRC_ALPHA:
			vs_module = &vk.modules.vert.gen[1][1][0][0];
			fs_module = &vk.modules.frag.gen[1][1][0];
			break;

		case TYPE_BLEND2_ADD_ENV:
		case TYPE_BLEND2_MUL_ENV:
		case TYPE_BLEND2_ALPHA_ENV:
		case TYPE_BLEND2_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND2_MIX_ALPHA_ENV:
		case TYPE_BLEND2_MIX_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND2_DST_COLOR_SRC_ALPHA_ENV:
			vs_module = &vk.modules.vert.gen[1][1][1][0];
			fs_module = &vk.modules.frag.gen[1][1][0];
			break;

		case TYPE_BLEND3_ADD:
		case TYPE_BLEND3_MUL:
		case TYPE_BLEND3_ALPHA:
		case TYPE_BLEND3_ONE_MINUS_ALPHA:
		case TYPE_BLEND3_MIX_ALPHA:
		case TYPE_BLEND3_MIX_ONE_MINUS_ALPHA:
		case TYPE_BLEND3_DST_COLOR_SRC_ALPHA:
			vs_module = &vk.modules.vert.gen[2][1][0][0];
			fs_module = &vk.modules.frag.gen[2][1][0];
			break;

		case TYPE_BLEND3_ADD_ENV:
		case TYPE_BLEND3_MUL_ENV:
		case TYPE_BLEND3_ALPHA_ENV:
		case TYPE_BLEND3_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND3_MIX_ALPHA_ENV:
		case TYPE_BLEND3_MIX_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND3_DST_COLOR_SRC_ALPHA_ENV:
			vs_module = &vk.modules.vert.gen[2][1][1][0];
			fs_module = &vk.modules.frag.gen[2][1][0];
			break;

		case TYPE_COLOR_BLACK:
		case TYPE_COLOR_WHITE:
		case TYPE_COLOR_GREEN:
		case TYPE_COLOR_RED:
			vs_module = &vk.modules.color_vs;
			fs_module = &vk.modules.color_fs;
			break;

		case TYPE_FOG_ONLY:
			vs_module = &vk.modules.fog_vs;
			fs_module = &vk.modules.fog_fs;
			break;

		case TYPE_DOT:
			vs_module = &vk.modules.dot_vs;
			fs_module = &vk.modules.dot_fs;
			break;

		default:
			ri.Error(ERR_DROP, "create_pipeline: unknown shader type %i\n", def->shader_type);
			return 0;
	}

	if ( def->fog_stage ) {
		switch ( def->shader_type ) {
			case TYPE_FOG_ONLY:
			case TYPE_DOT:
			case TYPE_SIGNLE_TEXTURE_DF:
			case TYPE_COLOR_BLACK:
			case TYPE_COLOR_WHITE:
			case TYPE_COLOR_GREEN:
			case TYPE_COLOR_RED:
				break;
			default:
				// switch to fogged modules
				vs_module++;
				fs_module++;
				break;
		}
	}

	set_shader_stage_desc(shader_stages+0, VK_SHADER_STAGE_VERTEX_BIT, *vs_module, "main");
	set_shader_stage_desc(shader_stages+1, VK_SHADER_STAGE_FRAGMENT_BIT, *fs_module, "main");

	//Com_Memset( vert_spec_data, 0, sizeof( vert_spec_data ) );
	Com_Memset( frag_spec_data, 0, sizeof( frag_spec_data ) );

	//vert_spec_data[0] = def->clipping_plane ? 1 : 0;

	// fragment shader specialization data
	atest_bits = state_bits & GLS_ATEST_BITS;
	switch ( atest_bits ) {
		case GLS_ATEST_GT_0:
			frag_spec_data[0].i = 1; // not equal
			frag_spec_data[1].f = 0.0f;
			break;
		case GLS_ATEST_LT_80:
			frag_spec_data[0].i = 2; // less than
			frag_spec_data[1].f = 0.5f;
			break;
		case GLS_ATEST_GE_80:
			frag_spec_data[0].i = 3; // greater or equal
			frag_spec_data[1].f = 0.5f;
			break;
		default:
			frag_spec_data[0].i = 0;
			frag_spec_data[1].f = 0.0f;
			break;
	};

	// depth fragment threshold
	frag_spec_data[2].f = 0.85f;

	// constant color
	switch ( def->shader_type ) {
		default: frag_spec_data[4].i = 0; break;
		case TYPE_COLOR_WHITE: frag_spec_data[4].i = 1; break;
		case TYPE_COLOR_GREEN: frag_spec_data[4].i = 2; break;
		case TYPE_COLOR_RED:   frag_spec_data[4].i = 3; break;
	}

	// abs lighting
	switch ( def->shader_type ) {
		case TYPE_SIGNLE_TEXTURE_LIGHTING:
		case TYPE_SIGNLE_TEXTURE_LIGHTING_LINEAR:
			frag_spec_data[5].i = def->abs_light ? 1 : 0;
		default:
			break;
	}

	// multutexture mode
	switch ( def->shader_type ) {
		case TYPE_MULTI_TEXTURE_MUL2_IDENTITY:
		case TYPE_MULTI_TEXTURE_MUL2_IDENTITY_ENV:
		case TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR:
		case TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR_ENV:
		case TYPE_MULTI_TEXTURE_MUL2:
		case TYPE_MULTI_TEXTURE_MUL2_ENV:
		case TYPE_MULTI_TEXTURE_MUL3:
		case TYPE_MULTI_TEXTURE_MUL3_ENV:
		case TYPE_BLEND2_MUL:
		case TYPE_BLEND2_MUL_ENV:
		case TYPE_BLEND3_MUL:
		case TYPE_BLEND3_MUL_ENV:
			frag_spec_data[6].i = 0;
			break;

		case TYPE_MULTI_TEXTURE_ADD2_IDENTITY:
		case TYPE_MULTI_TEXTURE_ADD2_IDENTITY_ENV:
		case TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR:
		case TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR_ENV:
		case TYPE_MULTI_TEXTURE_ADD2_1_1:
		case TYPE_MULTI_TEXTURE_ADD2_1_1_ENV:
		case TYPE_MULTI_TEXTURE_ADD3_1_1:
		case TYPE_MULTI_TEXTURE_ADD3_1_1_ENV:
			frag_spec_data[6].i = 1;
			break;

		case TYPE_MULTI_TEXTURE_ADD2:
		case TYPE_MULTI_TEXTURE_ADD2_ENV:
		case TYPE_MULTI_TEXTURE_ADD3:
		case TYPE_MULTI_TEXTURE_ADD3_ENV:
		case TYPE_BLEND2_ADD:
		case TYPE_BLEND2_ADD_ENV:
		case TYPE_BLEND3_ADD:
		case TYPE_BLEND3_ADD_ENV:
			frag_spec_data[6].i = 2;
			break;

		case TYPE_BLEND2_ALPHA:
		case TYPE_BLEND2_ALPHA_ENV:
		case TYPE_BLEND3_ALPHA:
		case TYPE_BLEND3_ALPHA_ENV:
			frag_spec_data[6].i = 3;
			break;

		case TYPE_BLEND2_ONE_MINUS_ALPHA:
		case TYPE_BLEND2_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND3_ONE_MINUS_ALPHA:
		case TYPE_BLEND3_ONE_MINUS_ALPHA_ENV:
			frag_spec_data[6].i = 4;
			break;

		case TYPE_BLEND2_MIX_ALPHA:
		case TYPE_BLEND2_MIX_ALPHA_ENV:
		case TYPE_BLEND3_MIX_ALPHA:
		case TYPE_BLEND3_MIX_ALPHA_ENV:
			frag_spec_data[6].i = 5;
			break;

		case TYPE_BLEND2_MIX_ONE_MINUS_ALPHA:
		case TYPE_BLEND2_MIX_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND3_MIX_ONE_MINUS_ALPHA:
		case TYPE_BLEND3_MIX_ONE_MINUS_ALPHA_ENV:
			frag_spec_data[6].i = 6;
			break;

		case TYPE_BLEND2_DST_COLOR_SRC_ALPHA:
		case TYPE_BLEND2_DST_COLOR_SRC_ALPHA_ENV:
		case TYPE_BLEND3_DST_COLOR_SRC_ALPHA:
		case TYPE_BLEND3_DST_COLOR_SRC_ALPHA_ENV:
			frag_spec_data[6].i = 7;
			break;

		default:
			break;
	}

	frag_spec_data[8].f = ((float)def->color.rgb) / 255.0;
	frag_spec_data[9].f = ((float)def->color.alpha) / 255.0;

	if ( def->fog_stage ) {
		frag_spec_data[10].i = def->acff;
	} else {
		frag_spec_data[10].i = 0;
	}

	//
	// vertex module specialization data
	//
	shader_stages[0].pSpecializationInfo = NULL;

	//
	// fragment module specialization data
	//

	spec_entries[1].constantID = 0;  // alpha-test-function
	spec_entries[1].offset = 0 * sizeof( int32_t );
	spec_entries[1].size = sizeof( int32_t );

	spec_entries[2].constantID = 1; // alpha-test-value
	spec_entries[2].offset = 1 * sizeof( int32_t );
	spec_entries[2].size = sizeof( float );

	spec_entries[3].constantID = 2; // depth-fragment
	spec_entries[3].offset = 2 * sizeof( int32_t );
	spec_entries[3].size = sizeof( float );

	spec_entries[4].constantID = 3; // alpha-to-coverage
	spec_entries[4].offset = 3 * sizeof( int32_t );
	spec_entries[4].size = sizeof( int32_t );

	spec_entries[5].constantID = 4; // color_mode
	spec_entries[5].offset = 4 * sizeof( int32_t );
	spec_entries[5].size = sizeof( int32_t );

	spec_entries[6].constantID = 5; // abs_light
	spec_entries[6].offset = 5 * sizeof( int32_t );
	spec_entries[6].size = sizeof( int32_t );

	spec_entries[7].constantID = 6; // multitexture mode
	spec_entries[7].offset = 6 * sizeof( int32_t );
	spec_entries[7].size = sizeof( int32_t );

	spec_entries[8].constantID = 7; // discard mode
	spec_entries[8].offset = 7 * sizeof( int32_t );
	spec_entries[8].size = sizeof( int32_t );

	spec_entries[9].constantID = 8; // fixed color
	spec_entries[9].offset = 8 * sizeof( int32_t );
	spec_entries[9].size = sizeof( float );

	spec_entries[10].constantID = 9; // fixed alpha
	spec_entries[10].offset = 9 * sizeof( int32_t );
	spec_entries[10].size = sizeof( float );

	spec_entries[11].constantID = 10; // acff
	spec_entries[11].offset = 10 * sizeof( int32_t );
	spec_entries[11].size = sizeof( int32_t );

	spec_entries[12].constantID = 11; // force_opaque_alpha (HUD 3D)
	spec_entries[12].offset = 11 * sizeof( int32_t );
	spec_entries[12].size = sizeof( int32_t );

	spec_entries[13].constantID = 12; // post_bloom_gamma
	spec_entries[13].offset = 12 * sizeof( int32_t );
	spec_entries[13].size = sizeof( float );

	spec_entries[14].constantID = 13; // post_bloom_obScale
	spec_entries[14].offset = 13 * sizeof( int32_t );
	spec_entries[14].size = sizeof( float );

	spec_entries[15].constantID = 14; // post_bloom_greyscale
	spec_entries[15].offset = 14 * sizeof( int32_t );
	spec_entries[15].size = sizeof( float );

	spec_entries[16].constantID = 15; // post_bloom_dither
	spec_entries[16].offset = 15 * sizeof( int32_t );
	spec_entries[16].size = sizeof( int32_t );

	spec_entries[17].constantID = 16; // post_bloom_depth_r
	spec_entries[17].offset = 16 * sizeof( int32_t );
	spec_entries[17].size = sizeof( int32_t );

	spec_entries[18].constantID = 17; // post_bloom_depth_g
	spec_entries[18].offset = 17 * sizeof( int32_t );
	spec_entries[18].size = sizeof( int32_t );

	spec_entries[19].constantID = 18; // post_bloom_depth_b
	spec_entries[19].offset = 18 * sizeof( int32_t );
	spec_entries[19].size = sizeof( int32_t );

	// For non-blended HUD render pass stages, force alpha=1.0
	// This ensures 3D models (player heads, weapon icons) are fully opaque
	if (renderPassIndex == RENDER_PASS_HUD && !(state_bits & (GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS))) {
		frag_spec_data[11].i = 1;
	} else {
		frag_spec_data[11].i = 0;
	}

	// For post-bloom 2D subpass, apply full post-processing in fragment shader
	// since this content bypasses the gamma subpass
	// Matches gamma subpass: greyscale -> pow(color, 1/r_gamma) * obScale -> dither
	if (renderPassIndex == RENDER_PASS_POST_SCENE_2D) {
		frag_spec_data[12].f = 1.0f / r_gamma->value;
		frag_spec_data[13].f = (float)(1 << tr.overbrightBits);
		frag_spec_data[14].f = r_greyscale->value;
		frag_spec_data[15].i = r_dither->integer;
		// Get color depth from XR swapchain format for dithering
		int depth_r, depth_g, depth_b;
		if (!vk.xr.colorInfo || !vk_surface_format_color_depth(vk.xr.colorInfo->format, &depth_r, &depth_g, &depth_b)) {
			depth_r = depth_g = depth_b = 255;
		}
		frag_spec_data[16].i = depth_r;
		frag_spec_data[17].i = depth_g;
		frag_spec_data[18].i = depth_b;
	} else {
		frag_spec_data[12].f = 1.0f; // No gamma correction for other passes
		frag_spec_data[13].f = 1.0f;
		frag_spec_data[14].f = 0.0f; // No greyscale
		frag_spec_data[15].i = 0;    // No dithering
		frag_spec_data[16].i = 255;  // Default color depth
		frag_spec_data[17].i = 255;
		frag_spec_data[18].i = 255;
	}

	frag_spec_info.mapEntryCount = 19;
	frag_spec_info.pMapEntries = spec_entries + 1;
	frag_spec_info.dataSize = sizeof( int32_t ) * 19;
	frag_spec_info.pData = &frag_spec_data[0];
	shader_stages[1].pSpecializationInfo = &frag_spec_info;

	//
	// Vertex input
	//
	num_binds = num_attrs = 0;
	switch ( def->shader_type ) {

		case TYPE_FOG_ONLY:
		case TYPE_DOT:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			break;

		case TYPE_COLOR_BLACK:
		case TYPE_COLOR_WHITE:
		case TYPE_COLOR_GREEN:
		case TYPE_COLOR_RED:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			break;

		case TYPE_SIGNLE_TEXTURE_DF:
		case TYPE_SIGNLE_TEXTURE_IDENTITY:
		case TYPE_SIGNLE_TEXTURE_FIXED_COLOR:
		case TYPE_SIGNLE_TEXTURE_ENT_COLOR:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
			break;

		case TYPE_SIGNLE_TEXTURE:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( color4ub_t ) );				// color array
			push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R8G8B8A8_UNORM );
			push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
			break;

		case TYPE_SIGNLE_TEXTURE_ENV:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( color4ub_t ) );				// color array
			//push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_bind( 5, sizeof( vec4_t ) );					// normals
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R8G8B8A8_UNORM );
			//push_attr( 2, 2, VK_FORMAT_R8G8B8A8_UNORM );
			push_attr( 5, 5, VK_FORMAT_R32G32B32A32_SFLOAT );
			break;

		case TYPE_SIGNLE_TEXTURE_IDENTITY_ENV:
		case TYPE_SIGNLE_TEXTURE_FIXED_COLOR_ENV:
		case TYPE_SIGNLE_TEXTURE_ENT_COLOR_ENV:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 5, sizeof( vec4_t ) );					// normals
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 5, 5, VK_FORMAT_R32G32B32A32_SFLOAT );
			break;

		case TYPE_SIGNLE_TEXTURE_LIGHTING:
		case TYPE_SIGNLE_TEXTURE_LIGHTING_LINEAR:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( vec2_t ) );					// st0 array
			push_bind( 2, sizeof( vec4_t ) );					// normals array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 2, 2, VK_FORMAT_R32G32B32A32_SFLOAT );
			break;

		case TYPE_MULTI_TEXTURE_MUL2_IDENTITY:
		case TYPE_MULTI_TEXTURE_ADD2_IDENTITY:
		case TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR:
		case TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_bind( 3, sizeof( vec2_t ) );					// st1 array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 3, 3, VK_FORMAT_R32G32_SFLOAT );
			break;

		case TYPE_MULTI_TEXTURE_MUL2_IDENTITY_ENV:
		case TYPE_MULTI_TEXTURE_ADD2_IDENTITY_ENV:
		case TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR_ENV:
		case TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR_ENV:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 3, sizeof( vec2_t ) );					// st1 array
			push_bind( 5, sizeof( vec4_t ) );					// normals
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 3, 3, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 5, 5, VK_FORMAT_R32G32B32A32_SFLOAT );
			break;

		case TYPE_MULTI_TEXTURE_MUL2:
		case TYPE_MULTI_TEXTURE_ADD2_1_1:
		case TYPE_MULTI_TEXTURE_ADD2:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( color4ub_t ) );				// color array
			push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_bind( 3, sizeof( vec2_t ) );					// st1 array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R8G8B8A8_UNORM );
			push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 3, 3, VK_FORMAT_R32G32_SFLOAT );
			break;

		case TYPE_MULTI_TEXTURE_MUL2_ENV:
		case TYPE_MULTI_TEXTURE_ADD2_1_1_ENV:
		case TYPE_MULTI_TEXTURE_ADD2_ENV:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( color4ub_t ) );				// color array
			//push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_bind( 3, sizeof( vec2_t ) );					// st1 array
			push_bind( 5, sizeof( vec4_t ) );					// normals
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R8G8B8A8_UNORM );
			//push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 3, 3, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 5, 5, VK_FORMAT_R32G32B32A32_SFLOAT );
			break;

		case TYPE_MULTI_TEXTURE_MUL3:
		case TYPE_MULTI_TEXTURE_ADD3_1_1:
		case TYPE_MULTI_TEXTURE_ADD3:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( color4ub_t ) );				// color array
			push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_bind( 3, sizeof( vec2_t ) );					// st1 array
			push_bind( 4, sizeof( vec2_t ) );					// st2 array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R8G8B8A8_UNORM );
			push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 3, 3, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 4, 4, VK_FORMAT_R32G32_SFLOAT );
			break;

		case TYPE_MULTI_TEXTURE_MUL3_ENV:
		case TYPE_MULTI_TEXTURE_ADD3_1_1_ENV:
		case TYPE_MULTI_TEXTURE_ADD3_ENV:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( color4ub_t ) );				// color array
			//push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_bind( 3, sizeof( vec2_t ) );					// st1 array
			push_bind( 4, sizeof( vec2_t ) );					// st2 array
			push_bind( 5, sizeof( vec4_t ) );					// normals
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R8G8B8A8_UNORM );
			//push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 3, 3, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 4, 4, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 5, 5, VK_FORMAT_R32G32B32A32_SFLOAT );
			break;

		case TYPE_BLEND2_ADD:
		case TYPE_BLEND2_MUL:
		case TYPE_BLEND2_ALPHA:
		case TYPE_BLEND2_ONE_MINUS_ALPHA:
		case TYPE_BLEND2_MIX_ALPHA:
		case TYPE_BLEND2_MIX_ONE_MINUS_ALPHA:
		case TYPE_BLEND2_DST_COLOR_SRC_ALPHA:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( color4ub_t ) );				// color0 array
			push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_bind( 3, sizeof( vec2_t ) );					// st1 array
			push_bind( 6, sizeof( color4ub_t ) );				// color1 array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R8G8B8A8_UNORM );
			push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 3, 3, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 6, 6, VK_FORMAT_R8G8B8A8_UNORM );
			break;

		case TYPE_BLEND2_ADD_ENV:
		case TYPE_BLEND2_MUL_ENV:
		case TYPE_BLEND2_ALPHA_ENV:
		case TYPE_BLEND2_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND2_MIX_ALPHA_ENV:
		case TYPE_BLEND2_MIX_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND2_DST_COLOR_SRC_ALPHA_ENV:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( color4ub_t ) );				// color0 array
			//push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_bind( 3, sizeof( vec2_t ) );					// st1 array
			push_bind( 5, sizeof( vec4_t ) );					// normals
			push_bind( 6, sizeof( color4ub_t ) );				// color1 array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R8G8B8A8_UNORM );
			//push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 3, 3, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 5, 5, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 6, 6, VK_FORMAT_R8G8B8A8_UNORM );
			break;

		case TYPE_BLEND3_ADD:
		case TYPE_BLEND3_MUL:
		case TYPE_BLEND3_ALPHA:
		case TYPE_BLEND3_ONE_MINUS_ALPHA:
		case TYPE_BLEND3_MIX_ALPHA:
		case TYPE_BLEND3_MIX_ONE_MINUS_ALPHA:
		case TYPE_BLEND3_DST_COLOR_SRC_ALPHA:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( color4ub_t ) );				// color0 array
			push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_bind( 3, sizeof( vec2_t ) );					// st1 array
			push_bind( 4, sizeof( vec2_t ) );					// st2 array
			push_bind( 6, sizeof( color4ub_t ) );				// color1 array
			push_bind( 7, sizeof( color4ub_t ) );				// color2 array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R8G8B8A8_UNORM );
			push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 3, 3, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 4, 4, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 6, 6, VK_FORMAT_R8G8B8A8_UNORM );
			push_attr( 7, 7, VK_FORMAT_R8G8B8A8_UNORM );
			break;

		case TYPE_BLEND3_ADD_ENV:
		case TYPE_BLEND3_MUL_ENV:
		case TYPE_BLEND3_ALPHA_ENV:
		case TYPE_BLEND3_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND3_MIX_ALPHA_ENV:
		case TYPE_BLEND3_MIX_ONE_MINUS_ALPHA_ENV:
		case TYPE_BLEND3_DST_COLOR_SRC_ALPHA_ENV:
			push_bind( 0, sizeof( vec4_t ) );					// xyz array
			push_bind( 1, sizeof( color4ub_t ) );				// color0 array
			//push_bind( 2, sizeof( vec2_t ) );					// st0 array
			push_bind( 3, sizeof( vec2_t ) );					// st1 array
			push_bind( 4, sizeof( vec2_t ) );					// st2 array
			push_bind( 5, sizeof( vec4_t ) );					// normals
			push_bind( 6, sizeof( color4ub_t ) );				// color1 array
			push_bind( 7, sizeof( color4ub_t ) );				// color2 array
			push_attr( 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 1, 1, VK_FORMAT_R8G8B8A8_UNORM );
			//push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 3, 3, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 4, 4, VK_FORMAT_R32G32_SFLOAT );
			push_attr( 5, 5, VK_FORMAT_R32G32B32A32_SFLOAT );
			push_attr( 6, 6, VK_FORMAT_R8G8B8A8_UNORM );
			push_attr( 7, 7, VK_FORMAT_R8G8B8A8_UNORM );
			break;

		default:
			ri.Error( ERR_DROP, "%s: invalid shader type - %i", __func__, def->shader_type );
			break;
	}

	vertex_input_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertex_input_state.pNext = NULL;
	vertex_input_state.flags = 0;
	vertex_input_state.pVertexBindingDescriptions = bindings;
	vertex_input_state.pVertexAttributeDescriptions = attribs;
	vertex_input_state.vertexBindingDescriptionCount = num_binds;
	vertex_input_state.vertexAttributeDescriptionCount = num_attrs;

	//
	// Primitive assembly.
	//
	input_assembly_state.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly_state.pNext = NULL;
	input_assembly_state.flags = 0;
	input_assembly_state.primitiveRestartEnable = VK_FALSE;

	switch ( def->primitives ) {
		case LINE_LIST: input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST; break;
		case POINT_LIST: input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST; break;
		case TRIANGLE_STRIP: input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP; break;
		default: input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; break;
	}

	//
	// Viewport.
	//
	viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_state.pNext = NULL;
	viewport_state.flags = 0;
	viewport_state.viewportCount = 1;
	viewport_state.pViewports = NULL; // dynamic viewport state
	viewport_state.scissorCount = 1;
	viewport_state.pScissors = NULL; // dynamic scissor state

	//
	// Rasterization.
	//
	rasterization_state.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterization_state.pNext = NULL;
	rasterization_state.flags = 0;
	rasterization_state.depthClampEnable = ((def->shadow_phase == SHADOW_FS_QUAD || def->shadow_phase == SHADOW_EDGES) && vk.depthClamp) ? VK_TRUE : VK_FALSE;
	rasterization_state.rasterizerDiscardEnable = VK_FALSE;
	rasterization_state.polygonMode = (state_bits & GLS_POLYMODE_LINE) ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;

	switch ( def->face_culling ) {
		case CT_TWO_SIDED:
			rasterization_state.cullMode = VK_CULL_MODE_NONE;
			break;
		case CT_FRONT_SIDED:
			rasterization_state.cullMode = (def->mirror ? VK_CULL_MODE_FRONT_BIT : VK_CULL_MODE_BACK_BIT);
			break;
		case CT_BACK_SIDED:
			rasterization_state.cullMode = (def->mirror ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_FRONT_BIT);
			break;
		default:
			ri.Error( ERR_DROP, "create_pipeline: invalid face culling mode %i\n", def->face_culling );
			break;
	}

	rasterization_state.frontFace = VK_FRONT_FACE_CLOCKWISE; // Q3 defaults to clockwise vertex order

	 // depth bias state
	if ( def->polygon_offset ) {
		rasterization_state.depthBiasEnable = VK_TRUE;
		rasterization_state.depthBiasClamp = 0.0f;
#ifdef USE_REVERSED_DEPTH
		rasterization_state.depthBiasConstantFactor = -r_offsetUnits->value;
		rasterization_state.depthBiasSlopeFactor = -r_offsetFactor->value;
#else
		rasterization_state.depthBiasConstantFactor = r_offsetUnits->value;
		rasterization_state.depthBiasSlopeFactor = r_offsetFactor->value;
#endif
	} else {
		rasterization_state.depthBiasEnable = VK_FALSE;
		rasterization_state.depthBiasClamp = 0.0f;
		rasterization_state.depthBiasConstantFactor = 0.0f;
		rasterization_state.depthBiasSlopeFactor = 0.0f;
	}

	if ( def->line_width )
		rasterization_state.lineWidth = (float)def->line_width;
	else
		rasterization_state.lineWidth = 1.0f;

	multisample_state.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample_state.pNext = NULL;
	multisample_state.flags = 0;

	// Set sample count based on render pass:
	// - SCREENMAP uses its own sample count
	// - MAIN and MAIN_WITH_POST subpass 0 use vkSamples (MSAA when active)
	// - POST_BLOOM_2D renders to swapchain which is always 1 sample
	// - HUD always uses 1 sample (its render pass is non-MSAA)
	if ( renderPassIndex == RENDER_PASS_SCREENMAP ) {
		multisample_state.rasterizationSamples = vk.screenMapSamples;
	} else if ( renderPassIndex == RENDER_PASS_HUD || renderPassIndex == RENDER_PASS_POST_SCENE_2D ) {
		// HUD and post-bloom 2D render to non-MSAA targets (HUD buffer, swapchain)
		multisample_state.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	} else {
		// MAIN, MAIN_WITH_POST (subpass 0), and default use vkSamples
		multisample_state.rasterizationSamples = vkSamples;
	}

	multisample_state.sampleShadingEnable = VK_FALSE;
	multisample_state.minSampleShading = 1.0f;
	multisample_state.pSampleMask = NULL;
	multisample_state.alphaToCoverageEnable = alphaToCoverage;
	multisample_state.alphaToOneEnable = VK_FALSE;

	Com_Memset( &depth_stencil_state, 0, sizeof( depth_stencil_state ) );

	depth_stencil_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depth_stencil_state.pNext = NULL;
	depth_stencil_state.flags = 0;
	// For subpasses without depth attachment (RENDER_PASS_POST_SCENE_2D), force depth disabled
	if ( renderPassIndex == RENDER_PASS_POST_SCENE_2D ) {
		depth_stencil_state.depthTestEnable = VK_FALSE;
		depth_stencil_state.depthWriteEnable = VK_FALSE;
	} else {
		depth_stencil_state.depthTestEnable = (state_bits & GLS_DEPTHTEST_DISABLE) ? VK_FALSE : VK_TRUE;
		depth_stencil_state.depthWriteEnable = (state_bits & GLS_DEPTHMASK_TRUE) ? VK_TRUE : VK_FALSE;
	}
#ifdef USE_REVERSED_DEPTH
	depth_stencil_state.depthCompareOp = (state_bits & GLS_DEPTHFUNC_EQUAL) ? VK_COMPARE_OP_EQUAL : VK_COMPARE_OP_GREATER_OR_EQUAL;
#else
	depth_stencil_state.depthCompareOp = (state_bits & GLS_DEPTHFUNC_EQUAL) ? VK_COMPARE_OP_EQUAL : VK_COMPARE_OP_LESS_OR_EQUAL;
#endif
	depth_stencil_state.depthBoundsTestEnable = VK_FALSE;
	depth_stencil_state.stencilTestEnable = (def->shadow_phase != SHADOW_DISABLED || def->stencil_mark) ? VK_TRUE : VK_FALSE;

	if (def->stencil_mark) {
		// mark entity pixels with stencil bit 0x80 so shadow finish skips them
		depth_stencil_state.front.failOp = VK_STENCIL_OP_KEEP;
		depth_stencil_state.front.passOp = VK_STENCIL_OP_REPLACE;
		depth_stencil_state.front.depthFailOp = VK_STENCIL_OP_KEEP;
		depth_stencil_state.front.compareOp = VK_COMPARE_OP_ALWAYS;
		depth_stencil_state.front.compareMask = 0xFF;
		depth_stencil_state.front.writeMask = 0x80;
		depth_stencil_state.front.reference = 0x80;

		depth_stencil_state.back = depth_stencil_state.front;

	} else if (def->shadow_phase == SHADOW_EDGES) {
		// z-fail (Carmack's reverse): count fragments behind scene geometry.
		// WRAP keeps the count modular across nested volumes and draw order;
		// writeMask 0x7F preserves the 0x80 entity-mark bit. Needs a closed
		// volume (near + far caps) and depth clamp.
		depth_stencil_state.front.failOp = VK_STENCIL_OP_KEEP;
		depth_stencil_state.front.passOp = VK_STENCIL_OP_KEEP;
		depth_stencil_state.front.depthFailOp = (def->face_culling == CT_FRONT_SIDED) ? VK_STENCIL_OP_DECREMENT_AND_WRAP : VK_STENCIL_OP_INCREMENT_AND_WRAP;
		depth_stencil_state.front.compareOp = VK_COMPARE_OP_EQUAL;
		depth_stencil_state.front.compareMask = 0x80;  // skip entity-marked pixels
		depth_stencil_state.front.writeMask = 0x7F;    // only write shadow count to bits 0-6
		depth_stencil_state.front.reference = 0;        // pass where bit 7 == 0 (not entity)

		depth_stencil_state.back = depth_stencil_state.front;

	} else if (def->shadow_phase == SHADOW_FS_QUAD) {
		depth_stencil_state.front.failOp = VK_STENCIL_OP_KEEP;
		depth_stencil_state.front.passOp = VK_STENCIL_OP_KEEP;
		depth_stencil_state.front.depthFailOp = VK_STENCIL_OP_KEEP;
		depth_stencil_state.front.compareOp = VK_COMPARE_OP_NOT_EQUAL;
		depth_stencil_state.front.compareMask = 0x7F;  // check shadow bits 0-6 only
		depth_stencil_state.front.writeMask = 0x7F;
		depth_stencil_state.front.reference = 0;

		depth_stencil_state.back = depth_stencil_state.front;
	}

	depth_stencil_state.minDepthBounds = 0.0f;
	depth_stencil_state.maxDepthBounds = 1.0f;

	Com_Memset(&attachment_blend_state, 0, sizeof(attachment_blend_state));
	attachment_blend_state.blendEnable = (state_bits & (GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS)) ? VK_TRUE : VK_FALSE;

	if (def->shadow_phase == SHADOW_EDGES || def->shader_type == TYPE_SIGNLE_TEXTURE_DF)
		attachment_blend_state.colorWriteMask = 0;
	else
		attachment_blend_state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	if (attachment_blend_state.blendEnable) {
		switch (state_bits & GLS_SRCBLEND_BITS) {
			case GLS_SRCBLEND_ZERO:
				attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_ZERO;
				break;
			case GLS_SRCBLEND_ONE:
				attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
				break;
			case GLS_SRCBLEND_DST_COLOR:
				attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_DST_COLOR;
				break;
			case GLS_SRCBLEND_ONE_MINUS_DST_COLOR:
				attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
				break;
			case GLS_SRCBLEND_SRC_ALPHA:
				attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
				break;
			case GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA:
				attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
				break;
			case GLS_SRCBLEND_DST_ALPHA:
				attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
				break;
			case GLS_SRCBLEND_ONE_MINUS_DST_ALPHA:
				attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
				break;
			case GLS_SRCBLEND_ALPHA_SATURATE:
				attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
				break;
			default:
				ri.Error( ERR_DROP, "create_pipeline: invalid src blend state bits\n" );
				break;
		}
		switch (state_bits & GLS_DSTBLEND_BITS) {
			case GLS_DSTBLEND_ZERO:
				attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
				break;
			case GLS_DSTBLEND_ONE:
				attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
				break;
			case GLS_DSTBLEND_SRC_COLOR:
				attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_SRC_COLOR;
				break;
			case GLS_DSTBLEND_ONE_MINUS_SRC_COLOR:
				attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
				break;
			case GLS_DSTBLEND_SRC_ALPHA:
				attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
				break;
			case GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA:
				attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
				break;
			case GLS_DSTBLEND_DST_ALPHA:
				attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
				break;
			case GLS_DSTBLEND_ONE_MINUS_DST_ALPHA:
				attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
				break;
			default:
				ri.Error( ERR_DROP, "create_pipeline: invalid dst blend state bits\n" );
				break;
		}

		// HUD render pass: use separate alpha blend to accumulate toward opaque
		// This matches glBlendFuncSeparate(srcFactor, dstFactor, GL_ONE, GL_ONE) from renderergl2
		// RGB blends normally so specular shows through texture alpha
		// Alpha accumulates: src=ONE, dst=ONE adds alphas together, saturating toward 1.0
		if (renderPassIndex == RENDER_PASS_HUD) {
			attachment_blend_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
			attachment_blend_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		} else {
			attachment_blend_state.srcAlphaBlendFactor = attachment_blend_state.srcColorBlendFactor;
			attachment_blend_state.dstAlphaBlendFactor = attachment_blend_state.dstColorBlendFactor;
		}
		attachment_blend_state.colorBlendOp = VK_BLEND_OP_ADD;
		attachment_blend_state.alphaBlendOp = VK_BLEND_OP_ADD;

		if ( def->allow_discard && vkSamples != VK_SAMPLE_COUNT_1_BIT ) {
			// try to reduce pixel fillrate for transparent surfaces, this yields 1..10% fps increase when multisampling in enabled
			if ( attachment_blend_state.srcColorBlendFactor == VK_BLEND_FACTOR_SRC_ALPHA && attachment_blend_state.dstColorBlendFactor == VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA ) {
				frag_spec_data[7].i = 1;
			} else if ( attachment_blend_state.srcColorBlendFactor == VK_BLEND_FACTOR_ONE && attachment_blend_state.dstColorBlendFactor == VK_BLEND_FACTOR_ONE ) {
				frag_spec_data[7].i = 2;
			}
		}
	}

	blend_state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blend_state.pNext = NULL;
	blend_state.flags = 0;
	blend_state.logicOpEnable = VK_FALSE;
	blend_state.logicOp = VK_LOGIC_OP_COPY;
	blend_state.attachmentCount = 1;
	blend_state.pAttachments = &attachment_blend_state;
	blend_state.blendConstants[0] = 0.0f;
	blend_state.blendConstants[1] = 0.0f;
	blend_state.blendConstants[2] = 0.0f;
	blend_state.blendConstants[3] = 0.0f;

	dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamic_state.pNext = NULL;
	dynamic_state.flags = 0;
	dynamic_state.dynamicStateCount = ARRAY_LEN( dynamic_state_array );
	dynamic_state.pDynamicStates = dynamic_state_array;

	create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	create_info.pNext = NULL;
	create_info.flags = 0;
	create_info.stageCount = ARRAY_LEN(shader_stages);
	create_info.pStages = shader_stages;
	create_info.pVertexInputState = &vertex_input_state;
	create_info.pInputAssemblyState = &input_assembly_state;
	create_info.pTessellationState = NULL;
	create_info.pViewportState = &viewport_state;
	create_info.pRasterizationState = &rasterization_state;
	create_info.pMultisampleState = &multisample_state;
	create_info.pDepthStencilState = &depth_stencil_state;
	create_info.pColorBlendState = &blend_state;
	create_info.pDynamicState = &dynamic_state;

	if ( def->shader_type == TYPE_DOT )
		create_info.layout = vk.pipeline_layout_storage;
	else
		create_info.layout = vk.pipeline_layout;

	if ( renderPassIndex == RENDER_PASS_SCREENMAP ) {
		create_info.renderPass = vk.render_pass.screenmap;
		create_info.subpass = 0;
	} else if ( renderPassIndex == RENDER_PASS_HUD ) {
		create_info.renderPass = vk.render_pass.hudBuffer;
		create_info.subpass = 0;
	} else if ( renderPassIndex == RENDER_PASS_POST_BLOOM ) {
		// Post-bloom uses post_bloom (handles MSAA properly)
		create_info.renderPass = vk.render_pass.post_bloom;
		create_info.subpass = 0;
	} else if ( renderPassIndex == RENDER_PASS_MAIN_WITH_POST ) {
		// The scene has its own pass
		if ( vk.render_pass.fov_scene != VK_NULL_HANDLE ) {
			create_info.renderPass = vk.render_pass.fov_scene;
		} else {
			// Fallback to main if the split render passes are not available
			create_info.renderPass = vk.render_pass.main;
		}
		create_info.subpass = 0;
	} else if ( renderPassIndex == RENDER_PASS_POST_SCENE_2D ) {
		// Post-scene 2D: the post pass's second subpass, after the composite or gamma quad
		if ( r_bloom && r_bloom->integer && vk.render_pass.main_with_bloom != VK_NULL_HANDLE ) {
			create_info.renderPass = vk.render_pass.main_with_bloom;
			create_info.subpass = 1;
		} else if ( vk.render_pass.main_with_gamma != VK_NULL_HANDLE ) {
			create_info.renderPass = vk.render_pass.main_with_gamma;
			create_info.subpass = 1;
		} else {
			// Fallback to main if subpass render passes not available
			create_info.renderPass = vk.render_pass.main;
			create_info.subpass = 0;
		}
	} else {
		// RENDER_PASS_MAIN or any other value defaults to main
		create_info.renderPass = vk.render_pass.main;
		create_info.subpass = 0;
		// Sanity check: if not RENDER_PASS_MAIN, sample count must match
		if ( renderPassIndex != RENDER_PASS_MAIN ) {
			ri.Printf( PRINT_WARNING, "create_pipeline: unexpected renderPassIndex %d, defaulting to main\n", renderPassIndex );
		}
	}
	create_info.basePipelineHandle = VK_NULL_HANDLE;
	create_info.basePipelineIndex = -1;

	VK_CHECK( qvkCreateGraphicsPipelines( vk.device, vk.pipelineCache, 1, &create_info, NULL, &pipeline ) );

	SET_OBJECT_NAME( pipeline, va( "pipeline def#%i, pass#%i", def_index, renderPassIndex ), VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );

	vk.pipeline_create_count++;

	return pipeline;
}


static uint32_t vk_alloc_pipeline( const Vk_Pipeline_Def *def ) {
	VK_Pipeline_t *pipeline;
	if ( vk.pipelines_count >= MAX_VK_PIPELINES ) {
		ri.Error( ERR_DROP, "alloc_pipeline: MAX_VK_PIPELINES reached" );
		return 0;
	} else {
		int j;
		pipeline = &vk.pipelines[ vk.pipelines_count ];
		pipeline->def = *def;
		for ( j = 0; j < RENDER_PASS_COUNT; j++ ) {
			pipeline->handle[j] = VK_NULL_HANDLE;
		}
		return vk.pipelines_count++;
	}
}


VkPipeline vk_gen_pipeline( uint32_t index ) {
	if ( index < vk.pipelines_count ) {
		VK_Pipeline_t *pipeline = vk.pipelines + index;
		const renderPass_t pass = vk.renderPassIndex;
		if ( pipeline->handle[ pass ] == VK_NULL_HANDLE ) {
			pipeline->handle[ pass ] = create_pipeline( &pipeline->def, pass, index );
		}
		return pipeline->handle[ pass ];
	} else {
		ri.Error( ERR_FATAL, "%s(%i): NULL pipeline", __func__, index );
		return VK_NULL_HANDLE;
	}
}


uint32_t vk_find_pipeline_ext( uint32_t base, const Vk_Pipeline_Def *def, qboolean use ) {
	const Vk_Pipeline_Def *cur_def;
	uint32_t index;

	for ( index = base; index < vk.pipelines_count; index++ ) {
		cur_def = &vk.pipelines[ index ].def;
		if ( memcmp( cur_def, def, sizeof( *def ) ) == 0 ) {
			goto found;
		}
	}

	index = vk_alloc_pipeline( def );
found:

	if ( use )
		vk_gen_pipeline( index );

	return index;
}


void vk_get_pipeline_def( uint32_t pipeline, Vk_Pipeline_Def *def ) {
	if ( pipeline >= vk.pipelines_count ) {
		Com_Memset( def, 0, sizeof( *def ) );
	} else {
		Com_Memcpy( def, &vk.pipelines[ pipeline ].def, sizeof( *def ) );
	}
}


static void get_viewport_rect(VkRect2D *r)
{
	if ( backEnd.projection2D )
	{
		r->offset.x = 0;
		r->offset.y = 0;
		r->extent.width = vk.renderWidth;
		r->extent.height = vk.renderHeight;

	}
	else
	{
		r->offset.x = backEnd.viewParms.viewportX * vk.renderScaleX;
		r->offset.y = vk.renderHeight - (backEnd.viewParms.viewportY + backEnd.viewParms.viewportHeight) * vk.renderScaleY;
		r->extent.width = (float)backEnd.viewParms.viewportWidth * vk.renderScaleX;
		r->extent.height = (float)backEnd.viewParms.viewportHeight * vk.renderScaleY;
	}
}

static void get_viewport(VkViewport *viewport, Vk_Depth_Range depth_range) {
	VkRect2D r;

	get_viewport_rect( &r );

	viewport->x = (float)r.offset.x;
	viewport->y = (float)r.offset.y;
	viewport->width = (float)r.extent.width;
	viewport->height = (float)r.extent.height;

	switch ( depth_range ) {
		default:
#ifdef USE_REVERSED_DEPTH
		//case DEPTH_RANGE_NORMAL:
			viewport->minDepth = 0.0f;
			viewport->maxDepth = 1.0f;
			break;
		case DEPTH_RANGE_ZERO:
			viewport->minDepth = 1.0f;
			viewport->maxDepth = 1.0f;
			break;
		case DEPTH_RANGE_ONE:
			viewport->minDepth = 0.0f;
			viewport->maxDepth = 0.0f;
			break;
		case DEPTH_RANGE_WEAPON:
			viewport->minDepth = 0.6f;
			viewport->maxDepth = 1.0f;
			break;
#else
		//case DEPTH_RANGE_NORMAL:
			viewport->minDepth = 0.0f;
			viewport->maxDepth = 1.0f;
			break;
		case DEPTH_RANGE_ZERO:
			viewport->minDepth = 0.0f;
			viewport->maxDepth = 0.0f;
			break;
		case DEPTH_RANGE_ONE:
			viewport->minDepth = 1.0f;
			viewport->maxDepth = 1.0f;
			break;
		case DEPTH_RANGE_WEAPON:
			viewport->minDepth = 0.0f;
			viewport->maxDepth = 0.3f;
			break;
#endif
	}
}

static void get_scissor_rect(VkRect2D *r) {

	if ( backEnd.viewParms.portalView != PV_NONE )
	{
		r->offset.x = backEnd.viewParms.scissorX;
		r->offset.y = glConfig.vidHeight - backEnd.viewParms.scissorY - backEnd.viewParms.scissorHeight;
		r->extent.width = backEnd.viewParms.scissorWidth;
		r->extent.height = backEnd.viewParms.scissorHeight;
	}
	else
	{
		get_viewport_rect(r);

		if (r->offset.x < 0)
			r->offset.x = 0;
		if (r->offset.y < 0)
			r->offset.y = 0;

		if (r->offset.x + r->extent.width > glConfig.vidWidth)
			r->extent.width = glConfig.vidWidth - r->offset.x;
		if (r->offset.y + r->extent.height > glConfig.vidHeight)
			r->extent.height = glConfig.vidHeight - r->offset.y;
	}
}


// vk.renderWidth/Height name the bound target; glConfig is only the eye buffer.
static void clamp_clear_rect_to_render_area( VkRect2D *r ) {

	if ( r->offset.x < 0 )
		r->offset.x = 0;
	if ( r->offset.y < 0 )
		r->offset.y = 0;

	// Extents are unsigned, so test the offset before subtracting
	if ( r->offset.x >= vk.renderWidth )
		r->extent.width = 0;
	else if ( r->offset.x + r->extent.width > vk.renderWidth )
		r->extent.width = vk.renderWidth - r->offset.x;

	if ( r->offset.y >= vk.renderHeight )
		r->extent.height = 0;
	else if ( r->offset.y + r->extent.height > vk.renderHeight )
		r->extent.height = vk.renderHeight - r->offset.y;
}


void vk_clear_color( const vec4_t color ) {

	VkClearAttachment attachment;
	VkClearRect clear_rect;

	if ( !vk.active )
		return;

	// Don't issue commands if we're not recording (e.g., during RE_Shutdown transition)
	if ( !vk.recordingCommands )
		return;

	// Must be inside a render pass to clear attachments
	if ( !vk.inRenderPass )
		return;

	attachment.colorAttachment = 0;
	attachment.clearValue.color.float32[0] = color[0];
	attachment.clearValue.color.float32[1] = color[1];
	attachment.clearValue.color.float32[2] = color[2];
	attachment.clearValue.color.float32[3] = color[3];
	attachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

	get_scissor_rect( &clear_rect.rect );
	clamp_clear_rect_to_render_area( &clear_rect.rect );
	clear_rect.baseArrayLayer = 0;
	// In multiview render passes, layerCount must be 1: the view mask
	// automatically broadcasts the clear to all active views (both eyes)
	clear_rect.layerCount = 1;

	qvkCmdClearAttachments( vk.cmd->command_buffer, 1, &attachment, 1, &clear_rect );
}


void vk_clear_depth( qboolean clear_stencil ) {

	VkClearAttachment attachment;
	VkClearRect clear_rect[1];

	if ( !vk.active )
		return;

	// Must be inside a render pass to clear attachments
	if ( !vk.inRenderPass )
		return;

	if ( vk_world.dirty_depth_attachment == 0 )
		return;

	attachment.colorAttachment = 0;
#ifdef USE_REVERSED_DEPTH
	attachment.clearValue.depthStencil.depth = 0.0f;
#else
	attachment.clearValue.depthStencil.depth = 1.0f;
#endif
	attachment.clearValue.depthStencil.stencil = 0;
	if ( clear_stencil && glConfig.stencilBits > 0 ) {
		attachment.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
	} else {
		attachment.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	}

	get_scissor_rect( &clear_rect[0].rect );
	clamp_clear_rect_to_render_area( &clear_rect[0].rect );
	clear_rect[0].baseArrayLayer = 0;
	// In multiview render passes, layerCount must be 1: the view mask
	// automatically broadcasts the clear to all active views (both eyes)
	clear_rect[0].layerCount = 1;

	qvkCmdClearAttachments( vk.cmd->command_buffer, 1, &attachment, 1, clear_rect );
}


// Inverse of a rigid/orthonormal affine GL matrix (rotation|reflection + translation).
// Valid for view matrices incl. s_flipMatrix axis permutations.
static void Matrix4x4_OrthonormalInvert( const float in[16], float out[16] )
{
	out[0] = in[0];  out[4] = in[1];  out[8]  = in[2];
	out[1] = in[4];  out[5] = in[5];  out[9]  = in[6];
	out[2] = in[8];  out[6] = in[9];  out[10] = in[10];
	out[3] = 0.0f;   out[7] = 0.0f;   out[11] = 0.0f;
	out[12] = -( in[12]*out[0] + in[13]*out[4] + in[14]*out[8]  );
	out[13] = -( in[12]*out[1] + in[13]*out[5] + in[14]*out[9]  );
	out[14] = -( in[12]*out[2] + in[13]*out[6] + in[14]*out[10] );
	out[15] = 1.0f;
}


float vk_view_eyeproj[2][16];

void vk_set_view_eyeproj( void )
{
	int e;

	if ( tr.vrParms.valid && !backEnd.projection2D &&
		!( backEnd.isDrawingHUD || backEnd.refdef.isHUD ) &&
		!( vr.virtual_screen || vr.weapon_zoomed ) ) {
		// Stereo (normal or portal): eyeProj = E'_e then P_e, where
		// E'_e = inverse(world monoView) * world eyeView_e. Exact because,
		// for any entity's local-to-world transform L, L * E'_e equals what
		// L folded against world.eyeViewMatrix[e] would produce (both share
		// the same L against world.modelMatrix / world.eyeViewMatrix[e]).
		float invMono[16], eprime[16];
		const float *proj[2];

		if ( backEnd.viewParms.portalView != PV_NONE ) {
			proj[0] = tr.vrParms.mirrorProjectionEye[0];
			proj[1] = tr.vrParms.mirrorProjectionEye[1];
		} else {
			proj[0] = tr.vrParms.projectionEye[0];
			proj[1] = tr.vrParms.projectionEye[1];
		}

		Matrix4x4_OrthonormalInvert( backEnd.viewParms.world.modelMatrix, invMono );
		for ( e = 0; e < 2; e++ ) {
			myGlMultMatrix( invMono, backEnd.viewParms.world.eyeViewMatrix[e], eprime );
			myGlMultMatrix( eprime, proj[e], vk_view_eyeproj[e] );
		}
		return;
	}

	// All cyclopean/mono flavors: both slots get the same projection the old
	// code multiplied per draw. Guards mirror vk_update_mvp's ladder exactly.
	{
		float proj[16];

		if ( tr.vrParms.valid && ( backEnd.isDrawingHUD || backEnd.refdef.isHUD ) ) {
			Com_Memcpy( proj, tr.vrParms.monoVRProjection, sizeof( proj ) );
		} else if ( tr.vrParms.valid && backEnd.viewParms.portalView != PV_NONE &&
				( vr.virtual_screen || vr.weapon_zoomed ) ) {
			// Portal cyclopean: no refdef-FOV override. The scope projection already matches the buffer.
			Com_Memcpy( proj, backEnd.viewParms.projectionMatrix, sizeof( proj ) );
			if ( !vr.weapon_zoomed ) {
				proj[5] *= (float)glConfig.vidHeight / (float)glConfig.vidWidth;
			}
			proj[8] = 0.0f;
			proj[9] = 0.0f;
		} else if ( tr.vrParms.valid && ( vr.virtual_screen || vr.weapon_zoomed ) ) {
			Com_Memcpy( proj, tr.vrParms.projection, sizeof( proj ) );
			if ( backEnd.refdef.rdflags & RDF_NOWORLDMODEL ) {
				// UI model scenes: refdef FOV scaled by the 4:3 crop factor
				float cropHeight = (float)( glConfig.vidWidth * 3 ) / 4.0f;
				float cropFactor = (float)glConfig.vidHeight / cropHeight;
				proj[0] = ( 1.0f / tan( DEG2RAD( backEnd.viewParms.fovX ) * 0.5f ) ) / cropFactor;
				proj[5] = ( -1.0f / tan( DEG2RAD( backEnd.viewParms.fovY ) * 0.5f ) ) / cropFactor;
			} else if ( !vr.weapon_zoomed ) {
				// Virtual screen: 4:3 crop. The scope projection already matches the buffer.
				proj[5] *= (float)glConfig.vidHeight / (float)glConfig.vidWidth;
			}
			proj[8] = 0.0f;
			proj[9] = 0.0f;
		} else {
			Com_Memcpy( proj, backEnd.viewParms.projectionMatrix, sizeof( proj ) );
		}

		Com_Memcpy( vk_view_eyeproj[0], proj, sizeof( proj ) );
		Com_Memcpy( vk_view_eyeproj[1], proj, sizeof( proj ) );
	}
}


/*
==================
vk_hud_eye_matrix

Clip x/y back through the eye projection to a head-space direction, into the eye's
frame by the display's cant, and projected again. A clip-space translation fuses only
the center: with cant the eyes see the plane through frames turned apart, so corners double.
==================
*/
static void vk_hud_eye_matrix( int eye, float depth, float xShift, float *out )
{
	const float *P = tr.vrParms.projectionEye[eye];
	const vrQuaternionf_t *q = &vr.eyeLocalRotation[eye];
	float toHead[16], toEye[16], tmp[16];
	float R[3][3];
	int c;

	// Clip x/y back to a head-space direction at unit distance
	Com_Memset( toHead, 0, sizeof( toHead ) );
	toHead[0] = 1.0f / P[0];
	toHead[5] = 1.0f / P[5];
	toHead[13] = P[9] / P[5];
	toHead[14] = -1.0f;
	toHead[15] = 1.0f;

	// Transpose of the eye's rotation in the head: head-space direction into the eye's frame
	R[0][0] = 1.0f - 2.0f * ( q->y * q->y + q->z * q->z );
	R[0][1] = 2.0f * ( q->x * q->y - q->z * q->w );
	R[0][2] = 2.0f * ( q->x * q->z + q->y * q->w );
	R[1][0] = 2.0f * ( q->x * q->y + q->z * q->w );
	R[1][1] = 1.0f - 2.0f * ( q->x * q->x + q->z * q->z );
	R[1][2] = 2.0f * ( q->y * q->z - q->x * q->w );
	R[2][0] = 2.0f * ( q->x * q->z - q->y * q->w );
	R[2][1] = 2.0f * ( q->y * q->z + q->x * q->w );
	R[2][2] = 1.0f - 2.0f * ( q->x * q->x + q->y * q->y );
	Com_Memset( toEye, 0, sizeof( toEye ) );
	for ( c = 0; c < 3; c++ ) {
		toEye[c * 4 + 0] = R[c][0];
		toEye[c * 4 + 1] = R[c][1];
		toEye[c * 4 + 2] = R[c][2];
	}
	toEye[15] = 1.0f;

	myGlMultMatrix( toHead, toEye, tmp );  // tmp = toEye * toHead
	myGlMultMatrix( tmp, P, out );         // out = P * toEye * toHead

	for ( c = 0; c < 4; c++ ) {
		out[c * 4 + 2] = depth * out[c * 4 + 3];
		out[c * 4 + 0] += xShift * out[c * 4 + 3];
	}
}


void vk_update_mvp( const float *m ) {
	float push_constants[16]; // mono modelview

	// Don't issue commands if we're not recording (e.g., during RE_Shutdown transition)
	if ( !vk.recordingCommands ) {
		return;
	}

	if ( backEnd.projection2D ) {
		// 2D ortho: common scale/translate in the push; per-eye asymmetry and
		// HUD parallax become clip-space translations in eyeProj (view slot).
		int hudStatus = vr_currentHudDrawStatus ? vr_currentHudDrawStatus->integer : -1;
		qboolean isHudMode1 = ( backEnd.isDrawingHUD && hudStatus == 1 );
		// vr.virtual_screen can be stale when we are between levels showing loading screens
		qboolean isVirtualScreen = VR_Gameplay_ShouldRenderInVirtualScreen();

		float hudScale = 1.0f;
		if ( backEnd.isDrawingHUD && hudStatus == 2 && !isVirtualScreen ) {
			hudScale = ( vr_hudScale ? vr_hudScale->value : 1.0f ) * ( 2.0f / 3.0f );
		}

		float mvp0 = 2.0f * hudScale / vk.renderWidth;
		float mvp5 = 2.0f * hudScale / vk.renderHeight;

		// Per-eye overlay placement through each eye's projection and cant (vk_hud_eye_matrix)
		qboolean perEye = ( tr.vrParms.valid && !isVirtualScreen && !isHudMode1 && !vr.weapon_zoomed );
		float asymmetryOffsetY = perEye ? tr.vrParms.projectionEye[0][9] : 0.0f;

		float depthOffset = 0.0f;
		if ( backEnd.isDrawingHUD && hudStatus == 2 && !vr.first_person_following && !vr.weapon_zoomed ) {
			float hudDepth = vr_currentHudDepth ? vr_currentHudDepth->value : 3.0f;
			float heightFraction = 0.05f / ( hudDepth + 1.0f );
			depthOffset = heightFraction * (float)vk.renderHeight * mvp0;
		}

		float yOffset = 0.0f;
		if ( backEnd.isDrawingHUD && hudStatus == 2 && !isVirtualScreen ) {
			yOffset = -asymmetryOffsetY * 0.5f;
			float userOffset = vr_hudYOffset ? vr_hudYOffset->value : 0.0f;
			yOffset += -userOffset * mvp5 * 0.5f;
		}

		// Common part -> push
		Com_Memset( push_constants, 0, sizeof( push_constants ) );
		push_constants[0]  = mvp0;
		push_constants[5]  = mvp5;
		push_constants[12] = -hudScale;
		push_constants[13] = -hudScale + yOffset;
#ifdef USE_REVERSED_DEPTH
		push_constants[14] = 1.0f;
#else
		push_constants[10] = 1.0f;
#endif
		push_constants[15] = 1.0f;

		// Per-eye part -> view slot: the plane through each eye, or the convergence shift alone
		Com_Memset( vk_view_eyeproj, 0, sizeof( vk_view_eyeproj ) );
		for ( int e = 0; e < 2; e++ ) {
			const float xShift = e ? -depthOffset : depthOffset;

			if ( perEye ) {
				vk_hud_eye_matrix( e, push_constants[14], xShift, vk_view_eyeproj[e] );
			} else {
				vk_view_eyeproj[e][0] = vk_view_eyeproj[e][5] =
				vk_view_eyeproj[e][10] = vk_view_eyeproj[e][15] = 1.0f;
				vk_view_eyeproj[e][12] = xShift;
			}
		}
		VK_PushEyeProj();
	} else if ( m ) {
		// Explicit modelview (shadows, flares): eyeProj already set per view.
		Com_Memcpy( push_constants, m, sizeof( push_constants ) );
	} else {
		Com_Memcpy( push_constants, vk_world.modelview_transform, sizeof( push_constants ) );
	}

	qvkCmdPushConstants( vk.cmd->command_buffer, vk.pipeline_layout,
		VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof( push_constants ), push_constants );
	vk.stats.push_size += sizeof( push_constants );
}


static VkBuffer shade_bufs[8];
static int bind_base;
static int bind_count;

static void vk_bind_index_attr( int index )
{
	if ( bind_base == -1 ) {
		bind_base = index;
		bind_count = 1;
	} else {
		bind_count = index - bind_base + 1;
	}
}


static void vk_bind_attr( int index, unsigned int item_size, const void *src ) {
	const uint32_t offset = PAD( vk.cmd->vertex_buffer_offset, 32 );
	const uint32_t size = tess.numVertexes * item_size;

	if ( offset + size > vk.geometry_buffer_size ) {
		// schedule geometry buffer resize
		vk.geometry_buffer_size_new = log2pad( offset + size, 1 );
	} else {
		vk.cmd->buf_offset[ index ] = offset;
		Com_Memcpy( vk.cmd->vertex_buffer_ptr + offset, src, size );
		vk.cmd->vertex_buffer_offset = (VkDeviceSize)offset + size;
	}

	vk_bind_index_attr( index );
}


uint32_t vk_tess_index( uint32_t numIndexes, const void *src ) {
	const uint32_t offset = vk.cmd->vertex_buffer_offset;
	const uint32_t size = numIndexes * sizeof( tess.indexes[0] );

	if ( offset + size > vk.geometry_buffer_size ) {
		// schedule geometry buffer resize
		vk.geometry_buffer_size_new = log2pad( offset + size, 1 );
		return ~0U;
	} else {
		Com_Memcpy( vk.cmd->vertex_buffer_ptr + offset, src, size );
		vk.cmd->vertex_buffer_offset = (VkDeviceSize)offset + size;
		return offset;
	}
}


void vk_bind_index_buffer( VkBuffer buffer, uint32_t offset )
{
	// Don't issue commands if we're not recording (e.g., during RE_Shutdown transition)
	if ( !vk.recordingCommands ) {
		return;
	}

	if ( vk.cmd->curr_index_buffer != buffer || vk.cmd->curr_index_offset != offset )
		qvkCmdBindIndexBuffer( vk.cmd->command_buffer, buffer, offset, VK_INDEX_TYPE_UINT32 );

	vk.cmd->curr_index_buffer = buffer;
	vk.cmd->curr_index_offset = offset;
}


#ifdef USE_VBO
void vk_draw_indexed( uint32_t indexCount, uint32_t firstIndex )
{
	// Must be inside a render pass to draw
	if ( !vk.inRenderPass ) {
		return;
	}
	qvkCmdDrawIndexed( vk.cmd->command_buffer, indexCount, 1, firstIndex, 0, 0 );
}
#endif


void vk_bind_index( void )
{
#ifdef USE_VBO
	if ( tess.vboIndex ) {
		vk.cmd->num_indexes = 0;
		//qvkCmdBindIndexBuffer( vk.cmd->command_buffer, vk.vbo.index_buffer, tess.shader->iboOffset, VK_INDEX_TYPE_UINT32 );
		return;
	}
#endif

	vk_bind_index_ext( tess.numIndexes, tess.indexes );
}


void vk_bind_index_ext( const int numIndexes, const uint32_t *indexes )
{
	uint32_t offset	= vk_tess_index( numIndexes, indexes );
	if ( offset != ~0U ) {
		vk_bind_index_buffer( vk.cmd->vertex_buffer, offset );
		vk.cmd->num_indexes = numIndexes;
	} else {
		// overflowed
		vk.cmd->num_indexes = 0;
	}
}


void vk_bind_geometry( uint32_t flags )
{
	//unsigned int size;
	bind_base = -1;
	bind_count = 0;

	// Don't issue commands if we're not recording (e.g., during RE_Shutdown transition)
	if ( !vk.recordingCommands ) {
		return;
	}

	if ( ( flags & ( TESS_XYZ | TESS_RGBA0 | TESS_ST0 | TESS_ST1 | TESS_ST2 | TESS_NNN | TESS_RGBA1 | TESS_RGBA2 ) ) == 0 )
		return;

#ifdef USE_VBO
	if ( tess.vboIndex ) {

		shade_bufs[0] = shade_bufs[1] = shade_bufs[2] = shade_bufs[3] = shade_bufs[4] = shade_bufs[5] = shade_bufs[6] = shade_bufs[7] = vk.vbo.vertex_buffer;

		if ( flags & TESS_XYZ ) {  // 0
			vk.cmd->vbo_offset[0] = tess.shader->vboOffset + 0;
			vk_bind_index_attr( 0 );
		}

		if ( flags & TESS_RGBA0 ) { // 1
			vk.cmd->vbo_offset[1] = tess.shader->stages[ tess.vboStage ]->rgb_offset[0];
			vk_bind_index_attr( 1 );
		}

		if ( flags & TESS_ST0 ) {  // 2
			vk.cmd->vbo_offset[2] = tess.shader->stages[ tess.vboStage ]->tex_offset[0];
			vk_bind_index_attr( 2 );
		}

		if ( flags & TESS_ST1 ) {  // 3
			vk.cmd->vbo_offset[3] = tess.shader->stages[ tess.vboStage ]->tex_offset[1];
			vk_bind_index_attr( 3 );
		}

		if ( flags & TESS_ST2 ) {  // 4
			vk.cmd->vbo_offset[4] = tess.shader->stages[ tess.vboStage ]->tex_offset[2];
			vk_bind_index_attr( 4 );
		}

		if ( flags & TESS_NNN ) { // 5
			vk.cmd->vbo_offset[5] = tess.shader->normalOffset;
			vk_bind_index_attr( 5 );
		}

		if ( flags & TESS_RGBA1 ) { // 6
			vk.cmd->vbo_offset[6] = tess.shader->stages[ tess.vboStage ]->rgb_offset[1];
			vk_bind_index_attr( 6 );
		}

		if ( flags & TESS_RGBA2 ) { // 7
			vk.cmd->vbo_offset[7] = tess.shader->stages[ tess.vboStage ]->rgb_offset[2];
			vk_bind_index_attr( 7 );
		}

		qvkCmdBindVertexBuffers( vk.cmd->command_buffer, bind_base, bind_count, shade_bufs, vk.cmd->vbo_offset + bind_base );

	} else
#endif // USE_VBO
	{
		shade_bufs[0] = shade_bufs[1] = shade_bufs[2] = shade_bufs[3] = shade_bufs[4] = shade_bufs[5] = shade_bufs[6] = shade_bufs[7] = vk.cmd->vertex_buffer;

		if ( flags & TESS_XYZ ) {
			vk_bind_attr(0, sizeof(tess.xyz[0]), &tess.xyz[0]);
		}

		if ( flags & TESS_RGBA0 ) {
			vk_bind_attr(1, sizeof( color4ub_t ), tess.svars.colors[0][0].rgba);
		}

		if ( flags & TESS_ST0 ) {
			vk_bind_attr(2, sizeof( vec2_t ), tess.svars.texcoordPtr[0]);
		}

		if ( flags & TESS_ST1 ) {
			vk_bind_attr(3, sizeof( vec2_t ), tess.svars.texcoordPtr[1]);
		}

		if ( flags & TESS_ST2 ) {
			vk_bind_attr(4, sizeof( vec2_t ), tess.svars.texcoordPtr[2]);
		}

		if ( flags & TESS_NNN ) {
			vk_bind_attr(5, sizeof(tess.normal[0]), tess.normal);
		}

		if ( flags & TESS_RGBA1 ) {
			vk_bind_attr(6, sizeof( color4ub_t ), tess.svars.colors[1][0].rgba);
		}

		if ( flags & TESS_RGBA2 ) {
			vk_bind_attr(7, sizeof( color4ub_t ), tess.svars.colors[2][0].rgba);
		}

		qvkCmdBindVertexBuffers( vk.cmd->command_buffer, bind_base, bind_count, shade_bufs, vk.cmd->buf_offset + bind_base );
	}
}


void vk_bind_lighting( int stage, int bundle )
{
	bind_base = -1;
	bind_count = 0;

	// Don't issue commands if we're not recording (e.g., during RE_Shutdown transition)
	if ( !vk.recordingCommands ) {
		return;
	}

#ifdef USE_VBO
	if ( tess.vboIndex ) {

		shade_bufs[0] = shade_bufs[1] = shade_bufs[2] = vk.vbo.vertex_buffer;

		vk.cmd->vbo_offset[0] = tess.shader->vboOffset + 0;
		vk.cmd->vbo_offset[1] = tess.shader->stages[ stage ]->tex_offset[ bundle ];
		vk.cmd->vbo_offset[2] = tess.shader->normalOffset;

		qvkCmdBindVertexBuffers( vk.cmd->command_buffer, 0, 3, shade_bufs, vk.cmd->vbo_offset + 0 );

	}
	else
#endif // USE_VBO
	{
		shade_bufs[0] = shade_bufs[1] = shade_bufs[2] = vk.cmd->vertex_buffer;

		vk_bind_attr( 0, sizeof( tess.xyz[0] ), &tess.xyz[0] );
		vk_bind_attr( 1, sizeof( vec2_t ), tess.svars.texcoordPtr[ bundle ] );
		vk_bind_attr( 2, sizeof( tess.normal[0] ), tess.normal );

		qvkCmdBindVertexBuffers( vk.cmd->command_buffer, bind_base, bind_count, shade_bufs, vk.cmd->buf_offset + bind_base );
	}
}


void vk_reset_descriptor( int index )
{
	vk.cmd->descriptor_set.current[ index ] = VK_NULL_HANDLE;
}


void vk_update_descriptor( int index, VkDescriptorSet descriptor )
{
	if ( vk.cmd->descriptor_set.current[ index ] != descriptor ) {
		vk.cmd->descriptor_set.start = ( index < vk.cmd->descriptor_set.start ) ? index : vk.cmd->descriptor_set.start;
		vk.cmd->descriptor_set.end = ( index > vk.cmd->descriptor_set.end ) ? index : vk.cmd->descriptor_set.end;
	}
	vk.cmd->descriptor_set.current[ index ] = descriptor;
}


void vk_update_descriptor_offset( int index, uint32_t offset )
{
	vk.cmd->descriptor_set.offset[ index ] = offset;
}


void vk_bind_descriptor_sets( void )
{
	uint32_t offsets[2], offset_count;
	uint32_t start, end, count, i;

	start = vk.cmd->descriptor_set.start;
	if ( start == ~0U )
		return;

	// set 0 (uniform + eyeProj) is statically used by every standard pipeline
	// since the matrix split; if its tracking was wiped by a render-pass
	// transition, fold it back into this bind so no draw runs without it.
	if ( start != VK_DESC_UNIFORM && vk.cmd->descriptor_set.current[ VK_DESC_UNIFORM ] == VK_NULL_HANDLE ) {
		vk.cmd->descriptor_set.current[ VK_DESC_UNIFORM ] = vk.cmd->uniform_descriptor;
		start = VK_DESC_UNIFORM;
	}

	end = vk.cmd->descriptor_set.end;

	offset_count = 0;
	if ( start == VK_DESC_UNIFORM ) { // uniform + eyeproj dynamic offsets, binding order
		offsets[ offset_count++ ] = vk.cmd->descriptor_set.offset[ start ];
		offsets[ offset_count++ ] = vk.cmd->eyeproj_offset;
	}

	// Ensure we always bind at least up to VK_DESC_TEXTURE1 (set 2) when starting from set 0,
	// since multi-texture shaders expect texture1 to be bound even if only texture0 is explicitly set.
	// This prevents validation errors when a pipeline statically uses set 2 but we only updated sets 0-1.
	if ( start == VK_DESC_UNIFORM && end < VK_DESC_TEXTURE1 ) {
		end = VK_DESC_TEXTURE1;
	}

	count = end - start + 1;

	// fill NULL descriptor gaps (including end, since we bind from start to end inclusive)
	for ( i = start + 1; i <= end; i++ ) {
		if ( vk.cmd->descriptor_set.current[i] == VK_NULL_HANDLE ) {
			vk.cmd->descriptor_set.current[i] = tr.whiteImage->descriptor;
		}
	}

	// Always use multiview layout: mono modelview passed via 64-byte push
	// constants, per-eye projection via the ViewTransform UBO (set 0, binding 1)
	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
		vk.pipeline_layout, start, count, vk.cmd->descriptor_set.current + start, offset_count, offsets );

	vk.cmd->descriptor_set.end = 0;
	vk.cmd->descriptor_set.start = ~0U;
}


void vk_bind_pipeline( uint32_t pipeline ) {
	VkPipeline vkpipe;

	// Don't issue commands if we're not recording (e.g., during RE_Shutdown transition)
	if ( !vk.recordingCommands ) {
		return;
	}

	vkpipe = vk_gen_pipeline( pipeline );

	if ( vkpipe != vk.cmd->last_pipeline ) {
		qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vkpipe );
		vk.cmd->last_pipeline = vkpipe;
	}

	vk_world.dirty_depth_attachment |= ( vk.pipelines[ pipeline ].def.state_bits & GLS_DEPTHMASK_TRUE );
}

static void vk_update_depth_range( Vk_Depth_Range depth_range )
{
	if ( vk.cmd->depth_range != depth_range ) {
		VkRect2D scissor_rect;
		VkViewport viewport;

		vk.cmd->depth_range = depth_range;

		get_scissor_rect( &scissor_rect );

		if ( memcmp( &vk.cmd->scissor_rect, &scissor_rect, sizeof( scissor_rect ) ) != 0 ) {
			qvkCmdSetScissor( vk.cmd->command_buffer, 0, 1, &scissor_rect );
			vk.cmd->scissor_rect = scissor_rect;
		}

		get_viewport( &viewport, depth_range );

		qvkCmdSetViewport( vk.cmd->command_buffer, 0, 1, &viewport );
	}
}


void vk_draw_geometry( Vk_Depth_Range depth_range, qboolean indexed ) {

	// Don't issue commands if we're not recording (e.g., during RE_Shutdown transition)
	if ( !vk.recordingCommands ) {
		return;
	}

	// Must be inside a render pass to draw
	if ( !vk.inRenderPass ) {
		return;
	}

	if ( vk.geometry_buffer_size_new ) {
		// geometry buffer overflow happened this frame
		return;
	}

	vk_bind_descriptor_sets();

	// configure pipeline's dynamic state
	vk_update_depth_range( depth_range );

	// issue draw call(s)
#ifdef USE_VBO
	if ( tess.vboIndex )
		VBO_RenderIBOItems();
	else
#endif
	if ( indexed ) {
		qvkCmdDrawIndexed( vk.cmd->command_buffer, vk.cmd->num_indexes, 1, 0, 0, 0 );
	} else {
		qvkCmdDraw( vk.cmd->command_buffer, tess.numVertexes, 1, 0, 0 );
	}
}


void vk_draw_flare_probe( uint32_t storage_offset, const float *push, qboolean depthTested )
{
	// Must be inside a render pass to draw
	if ( !vk.inRenderPass ) {
		return;
	}

	if ( vk.geometry_buffer_size_new ) {
		// geometry buffer overflow happened this frame
		return;
	}

	vk_bind_pipeline( depthTested ? vk.dot_pipeline : vk.dot_total_pipeline );

	// the probe's own push block: vk_update_mvp's 64-byte push does not fit this layout's range
	qvkCmdPushConstants( vk.cmd->command_buffer, vk.pipeline_layout_storage,
		VK_SHADER_STAGE_VERTEX_BIT, 0, FLARE_PROBE_PUSH_FLOATS * sizeof( float ), push );

	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout_storage, VK_DESC_STORAGE, 1, &vk.storage.descriptor, 1, &storage_offset );

	// configure pipeline's dynamic state
	vk_update_depth_range( DEPTH_RANGE_NORMAL );

	qvkCmdDraw( vk.cmd->command_buffer, tess.numVertexes, 1, 0, 0 );

	// vk.storage.descriptor was just bound at set 0 via vk.pipeline_layout_storage, which is
	// NOT compatible-for-set-0 with vk.pipeline_layout (2 UNIFORM_BUFFER_DYNAMIC bindings vs.
	// 1 STORAGE_BUFFER_DYNAMIC binding). Re-dirty set 0 so the next main-layout draw rebinds it, as VK_PushEyeProj does.
	vk_reset_descriptor( VK_DESC_UNIFORM );
	vk_update_descriptor( VK_DESC_UNIFORM, vk.cmd->uniform_descriptor );
}


static void vk_begin_render_pass( VkRenderPass renderPass, VkFramebuffer frameBuffer, qboolean clearValues, uint32_t width, uint32_t height )
{
	VkRenderPassBeginInfo render_pass_begin_info;
	VkClearValue clear_values[3];

	// Begin render pass.

	render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	render_pass_begin_info.pNext = NULL;
	render_pass_begin_info.renderPass = renderPass;
	render_pass_begin_info.framebuffer = frameBuffer;
	render_pass_begin_info.renderArea.offset.x = 0;
	render_pass_begin_info.renderArea.offset.y = 0;
	render_pass_begin_info.renderArea.extent.width = width;
	render_pass_begin_info.renderArea.extent.height = height;

	if ( clearValues ) {
		// attachments layout:
		// [0] - resolve/color/presentation
		// [1] - depth/stencil
		// [2] - multisampled color, optional
		Com_Memset( clear_values, 0, sizeof( clear_values ) );
#ifndef USE_REVERSED_DEPTH
		clear_values[1].depthStencil.depth = 1.0;
#endif
		render_pass_begin_info.clearValueCount = vk.msaaActive ? 3 : 2;
		render_pass_begin_info.pClearValues = clear_values;

		vk_world.dirty_depth_attachment = 0;
	} else {
		render_pass_begin_info.clearValueCount = 0;
		render_pass_begin_info.pClearValues = NULL;
	}

	qvkCmdBeginRenderPass( vk.cmd->command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE );
	vk.inRenderPass = qtrue;

	vk.cmd->last_pipeline = VK_NULL_HANDLE;
	vk.cmd->depth_range = DEPTH_RANGE_COUNT;
}


void vk_begin_main_render_pass( void )
{
	VkRenderPassBeginInfo render_pass_begin_info;
	VkClearValue clear_values[3];  // [0] = resolve/color, [1] = depth, [2] = MSAA color (if active)
	VkRenderPass renderPass;
	VkFramebuffer frameBuffer;

	// Check if we're in a valid rendering state
	if ( vk.frame_count == 0 || !vk.recordingCommands || !vk.cmd ) {
		return;
	}

	// End current render pass if active
	if ( vk.inRenderPass ) {
		// If we're in the post-bloom 2D subpass, end it properly
		if ( VK_IN_POST_SCENE_2D() ) {
			vk_end_post_scene_subpass();
		} else {
			// Need to advance through remaining subpasses before ending
			// Use vk_finish_subpass_post if we haven't done the post processing yet,
			// then vk_end_post_scene_subpass to finish cleanly
			if ( vk.renderPassIndex == RENDER_PASS_MAIN_WITH_POST ) {
				if ( !vk.subpassPostDone ) {
					vk_finish_subpass_post();  // Advances to post-bloom/gamma 2D subpass
				}
				vk_end_post_scene_subpass();  // Ends the render pass properly
			} else if ( vk.renderPassIndex == RENDER_PASS_POST_SCENE_2D ) {
				// Already transitioned to post-bloom 2D (vk_finish_subpass_post changed renderPassIndex)
				vk_end_post_scene_subpass();
			} else {
				qvkCmdEndRenderPass( vk.cmd->command_buffer );
				vk.inRenderPass = qfalse;
			}
		}
	}

	// Validate XR resources are initialized
	if ( !vk.xr.colorInfo || vk.xr.colorIndex >= vk.xr.colorInfo->imageCount ) {
		ri.Printf( PRINT_WARNING, "vk_begin_main_render_pass: XR resources not ready\n" );
		return;
	}

	// FBO mode: the scene draws alone; vk_finish_subpass_post opens the post pass
	if ( vk.fboActive ) {
		VkFramebuffer subpassFb = vk.framebuffers.fov_scene[vk.xr.colorIndex];
		VkRenderPass subpassRp = vk.render_pass.fov_scene;

		// Use subpass optimization: resources must be available in FBO mode
		if ( subpassFb != VK_NULL_HANDLE && subpassRp != VK_NULL_HANDLE ) {
			frameBuffer = subpassFb;
			renderPass = subpassRp;
			vk.renderPassIndex = RENDER_PASS_MAIN_WITH_POST;
		} else {
			// Subpass resources not ready: this is a fatal initialization error
			// (legacy vk.framebuffers.main fallback removed)
			ri.Error( ERR_FATAL, "vk_begin_main_render_pass: subpass resources not initialized (fb=%p, rp=%p)",
				(void*)subpassFb, (void*)subpassRp );
			return;
		}
	}
	else {
		// Direct mode: render directly to XR swapchain (no post-processing)
		frameBuffer = vk.xr.framebuffers[vk.xr.colorIndex];
		renderPass = vk.render_pass.main;
	}

	// Validate render pass exists
	if ( renderPass == VK_NULL_HANDLE ) {
		ri.Printf( PRINT_WARNING, "vk_begin_main_render_pass: render pass not ready (fbo=%d)\n", vk.fboActive );
		return;
	}

	// Validate framebuffer exists
	if ( frameBuffer == VK_NULL_HANDLE ) {
		ri.Printf( PRINT_WARNING, "vk_begin_main_render_pass: framebuffer not ready (fbo=%d)\n", vk.fboActive );
		return;
	}

	// Set render pass index (already set for subpass path above, set for others here)
	if ( vk.renderPassIndex != RENDER_PASS_MAIN_WITH_POST ) {
		vk.renderPassIndex = RENDER_PASS_MAIN;
	}

	// Rewrite this image's density map, if anything moved, while no render pass is open
	vk_update_authored_fdm( vk.xr.colorIndex );

	// Use XR dimensions
	// Note: glConfig.vidWidth/Height = vk.xr.width/height in VR
	vk.renderWidth = glConfig.vidWidth;
	vk.renderHeight = glConfig.vidHeight;

	vk.renderScaleX = vk.renderScaleY = 1.0f;

	// XR render pass: 2 attachments normally, 3 with MSAA (resolve target, depth, MSAA color)
	render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	render_pass_begin_info.pNext = NULL;
	render_pass_begin_info.renderPass = renderPass;
	render_pass_begin_info.framebuffer = frameBuffer;
	render_pass_begin_info.renderArea.offset.x = 0;
	render_pass_begin_info.renderArea.offset.y = 0;
	render_pass_begin_info.renderArea.extent.width = vk.renderWidth;
	render_pass_begin_info.renderArea.extent.height = vk.renderHeight;

	{
		// Clear values: [0] = color/resolve (black), [1] = depth (0.0 for reversed depth)
		// With MSAA: [2] = MSAA color (black)
		Com_Memset( clear_values, 0, sizeof( clear_values ) );
#ifndef USE_REVERSED_DEPTH
		clear_values[1].depthStencil.depth = 1.0f;
#endif
		if ( vk.renderPassIndex == RENDER_PASS_MAIN_WITH_POST ) {
			// Scene pass: [scene, depth] or [msaa, scene, depth]
			render_pass_begin_info.clearValueCount = vk.msaaActive ? 3 : 2;
		} else if ( vk.msaaActive ) {
			// MSAA mode, FBO or direct: 3 attachments (resolve, depth, MSAA color)
			render_pass_begin_info.clearValueCount = 3;
		} else {
			// Non-MSAA mode: 2 attachments (color, depth)
			render_pass_begin_info.clearValueCount = 2;
		}
		render_pass_begin_info.pClearValues = clear_values;
		vk_world.dirty_depth_attachment = 0;
	}

	qvkCmdBeginRenderPass( vk.cmd->command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE );
	vk.inRenderPass = qtrue;

	// Note: mono modelview is passed via push constants (64 bytes); per-eye
	// projection lives in the ViewTransform UBO (set 0, binding 1)

	vk.cmd->last_pipeline = VK_NULL_HANDLE;
	vk.cmd->depth_range = DEPTH_RANGE_COUNT;

	// Conservative reset of binding tracking across the render-pass transition;
	// vk_bind_descriptor_sets self-heals set 0 (see there) so the next draw is
	// never left without the uniform/eyeProj descriptor.
	Com_Memset( vk.cmd->descriptor_set.current, 0, sizeof( vk.cmd->descriptor_set.current ) );
	vk.cmd->descriptor_set.start = ~0U;
	vk.cmd->descriptor_set.end = 0;
}


void vk_begin_blur_render_pass( uint32_t index )
{
	VkFramebuffer frameBuffer = vk.framebuffers.blur[ index ];

	//vk.renderPassIndex = RENDER_PASS_BLOOM_EXTRACT; // doesn't matter, we will use dedicated pipelines

	vk.renderWidth = gls.captureWidth / ( 2 << ( index / 2 ) );
	vk.renderHeight = gls.captureHeight / ( 2 << ( index / 2 ) );

	//vk.renderScaleX = (float)vk.renderWidth / (float)glConfig.vidWidth;
	//vk.renderScaleY = (float)vk.renderHeight / (float)glConfig.vidHeight;
	vk.renderScaleX = vk.renderScaleY = 1.0f;

	vk_begin_render_pass( vk.render_pass.blur[ index ], frameBuffer, qfalse, vk.renderWidth, vk.renderHeight );
}


void vk_end_render_pass( void )
{
	if ( !vk.inRenderPass ) {
		return; // Not in a render pass, nothing to end
	}

	// Direct mode ends its scene pass here
	if ( vk.renderPassIndex == RENDER_PASS_MAIN ) {
		vk_draw_foveation_debug();
	}

	// For subpass-based render passes, need to advance to final subpass before ending
	if ( vk.renderPassIndex == RENDER_PASS_MAIN_WITH_POST ) {
		if ( VK_IN_POST_SCENE_2D() ) {
			vk_end_post_scene_subpass();
		} else {
			if ( !vk.subpassPostDone ) {
				vk_finish_subpass_post();  // Advances to post-bloom/gamma 2D subpass
			}
			vk_end_post_scene_subpass();  // Ends the render pass properly
		}
	} else if ( vk.renderPassIndex == RENDER_PASS_POST_SCENE_2D ) {
		// Already in post-bloom 2D subpass (vk_finish_subpass_post changed renderPassIndex)
		vk_end_post_scene_subpass();
	} else {
		qvkCmdEndRenderPass( vk.cmd->command_buffer );
		vk.inRenderPass = qfalse;
	}
}


/*
==================
vk_reset_command_buffer_caches

Bound state does not carry across command buffers; vertex buffers need no entry,
vk_bind_geometry rebinds them every time.
==================
*/
static void vk_reset_command_buffer_caches( void )
{
	vk.cmd->last_pipeline = VK_NULL_HANDLE;
	vk.cmd->depth_range = DEPTH_RANGE_COUNT;
	vk.cmd->curr_index_buffer = VK_NULL_HANDLE;
	vk.cmd->curr_index_offset = 0;
	Com_Memset( vk.cmd->descriptor_set.current, 0, sizeof( vk.cmd->descriptor_set.current ) );
	vk.cmd->descriptor_set.start = ~0U;
	vk.cmd->descriptor_set.end = 0;
	Com_Memset( &vk.cmd->scissor_rect, 0, sizeof( vk.cmd->scissor_rect ) );
}


/*
==================
vk_leave_hud_command_buffer

Back to the frame's command buffer; the pass open there is still open.
==================
*/
static void vk_leave_hud_command_buffer( void )
{
	if ( !vk.inHudCommandBuffer ) {
		return;
	}
	if ( vk.inRenderPass ) {
		qvkCmdEndRenderPass( vk.cmd->command_buffer );
	}

	vk.cmd->command_buffer = vk.hudSaved.commandBuffer;
	vk.inHudCommandBuffer = qfalse;
	vk.inRenderPass = vk.hudSaved.inRenderPass;
	vk.renderPassIndex = vk.hudSaved.renderPassIndex;
	vk.renderWidth = vk.hudSaved.renderWidth;
	vk.renderHeight = vk.hudSaved.renderHeight;
	vk.renderScaleX = vk.hudSaved.renderScaleX;
	vk.renderScaleY = vk.hudSaved.renderScaleY;

	vk_reset_command_buffer_caches();
}


/*
 * vk_begin_hud_render_pass - Begin rendering to the HUD buffer
 *
 * Records into the HUD's own command buffer, submitted ahead of the frame's, so the
 * frame's pass stays open and the sprite it draws samples this frame's HUD.
 */
void vk_begin_hud_render_pass( qboolean clear )
{
	VkRenderPassBeginInfo rpBI;
	VkViewport viewport;
	VkRect2D scissor;
	VkClearValue clearValues[2];

	// Check if we're in a valid rendering state
	if ( vk.frame_count == 0 || !vk.recordingCommands ) {
		return;
	}

	if ( !vk.cmd || vk.cmd->command_buffer == VK_NULL_HANDLE ) {
		return;
	}

	if ( vk.xr.hudFramebuffer == VK_NULL_HANDLE || vk.render_pass.hudBuffer == VK_NULL_HANDLE ||
		vk.render_pass.hudBufferClear == VK_NULL_HANDLE ) {
		return;
	}

	if ( !vk.inHudCommandBuffer ) {
		// Park the frame's state and record into the HUD command buffer, begun on the frame's first bracket
		if ( !vk.cmd->hud_begun ) {
			VkCommandBufferBeginInfo beginInfo;

			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			beginInfo.pNext = NULL;
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
			beginInfo.pInheritanceInfo = NULL;
			VK_CHECK( qvkBeginCommandBuffer( vk.cmd->hud_command_buffer, &beginInfo ) );
			vk.cmd->hud_begun = qtrue;
		}

		vk.hudSaved.commandBuffer = vk.cmd->command_buffer;
		vk.hudSaved.inRenderPass = vk.inRenderPass;
		vk.hudSaved.renderPassIndex = vk.renderPassIndex;
		vk.hudSaved.renderWidth = vk.renderWidth;
		vk.hudSaved.renderHeight = vk.renderHeight;
		vk.hudSaved.renderScaleX = vk.renderScaleX;
		vk.hudSaved.renderScaleY = vk.renderScaleY;

		vk.cmd->command_buffer = vk.cmd->hud_command_buffer;
		vk.inHudCommandBuffer = qtrue;
		vk.inRenderPass = qfalse;
		vk_reset_command_buffer_caches();
	} else if ( vk.inRenderPass ) {
		// a bracket left open: close its pass before opening the next
		qvkCmdEndRenderPass( vk.cmd->command_buffer );
		vk.inRenderPass = qfalse;
	}

	// Set up clear values
	// Color: transparent black when this pass clears; ignored when it loads
	clearValues[0].color.float32[0] = 0.0f;
	clearValues[0].color.float32[1] = 0.0f;
	clearValues[0].color.float32[2] = 0.0f;
	clearValues[0].color.float32[3] = 0.0f;
	// Depth: cleared each frame (LOAD_OP_CLEAR)
	// USE_REVERSED_DEPTH: clear to 0.0 (farthest), depth test is GREATER_OR_EQUAL
	clearValues[1].depthStencil.depth = 0.0f;
	clearValues[1].depthStencil.stencil = 0;

	// The frame's first HUD pass clears the color; later ones (console notify lines) load it.
	Com_Memset( &rpBI, 0, sizeof( rpBI ) );
	rpBI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	rpBI.renderPass = clear ? vk.render_pass.hudBufferClear : vk.render_pass.hudBuffer;
	rpBI.framebuffer = vk.xr.hudFramebuffer;
	rpBI.renderArea.offset.x = 0;
	rpBI.renderArea.offset.y = 0;
	rpBI.renderArea.extent.width = HUD_BUFFER_WIDTH;
	rpBI.renderArea.extent.height = HUD_BUFFER_HEIGHT;
	rpBI.clearValueCount = 2;  // Color + depth (depth uses LOAD_OP_CLEAR)
	rpBI.pClearValues = clearValues;

	qvkCmdBeginRenderPass( vk.cmd->command_buffer, &rpBI, VK_SUBPASS_CONTENTS_INLINE );

	// Update state
	vk.renderPassIndex = RENDER_PASS_HUD;
	vk.inRenderPass = qtrue;
	vk.renderWidth = HUD_BUFFER_WIDTH;
	vk.renderHeight = HUD_BUFFER_HEIGHT;
	vk.renderScaleX = 1.0f;
	vk.renderScaleY = 1.0f;

	// Set viewport (Y-flipped for Vulkan coordinate space)
	viewport.x = 0.0f;
	viewport.y = (float)HUD_BUFFER_HEIGHT;  // Start at bottom
	viewport.width = (float)HUD_BUFFER_WIDTH;
	viewport.height = -(float)HUD_BUFFER_HEIGHT;  // Negative height for Y-flip
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	qvkCmdSetViewport( vk.cmd->command_buffer, 0, 1, &viewport );

	// Set scissor BEFORE any drawing commands (including vkCmdClearAttachments)
	// vkCmdClearAttachments uses current scissor state, so it must be set first
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent.width = HUD_BUFFER_WIDTH;
	scissor.extent.height = HUD_BUFFER_HEIGHT;
	qvkCmdSetScissor( vk.cmd->command_buffer, 0, 1, &scissor );

	vk.cmd->last_pipeline = VK_NULL_HANDLE;

	// Conservative reset of binding tracking across the render-pass transition;
	// vk_bind_descriptor_sets self-heals set 0 (see there) so the next draw is
	// never left without the uniform/eyeProj descriptor.
	Com_Memset( vk.cmd->descriptor_set.current, 0, sizeof( vk.cmd->descriptor_set.current ) );
	vk.cmd->descriptor_set.start = ~0U;
	vk.cmd->descriptor_set.end = 0;
}


/*
 * vk_end_hud_render_pass - End rendering to the HUD buffer
 *
 * Returns recording to the frame's command buffer, whose pass is still open.
 */
void vk_end_hud_render_pass( void )
{
	if ( vk.renderPassIndex != RENDER_PASS_HUD ) {
		return;
	}

	if ( !vk.cmd || vk.cmd->command_buffer == VK_NULL_HANDLE ) {
		vk.renderPassIndex = RENDER_PASS_MAIN;  // Reset state
		return;
	}

	qvkCmdEndRenderPass( vk.cmd->command_buffer );
	vk.inRenderPass = qfalse;

	if ( vk.inHudCommandBuffer ) {
		vk_leave_hud_command_buffer();
	} else {
		vk.renderPassIndex = RENDER_PASS_MAIN;
	}
}


#ifndef UINT64_MAX
#define UINT64_MAX 0xFFFFFFFFFFFFFFFFULL
#endif

static void vk_resize_geometry_buffer( void )
{
	int i;

	vk_end_render_pass();

	VK_CHECK( qvkEndCommandBuffer( vk.cmd->command_buffer ) );

	qvkResetCommandBuffer( vk.cmd->command_buffer, 0 );

	// the frame is dropped, its HUD command buffer with it
	if ( vk.cmd->hud_begun ) {
		VK_CHECK( qvkEndCommandBuffer( vk.cmd->hud_command_buffer ) );
		qvkResetCommandBuffer( vk.cmd->hud_command_buffer, 0 );
		vk.cmd->hud_begun = qfalse;
	}

	// the buffer is back in the initial state; without this, the next
	// vk_begin_frame would route it into vk_finish_frame and end/submit a
	// never-begun command buffer
	vk.recordingCommands = qfalse;

	vk_wait_idle();

	vk_release_geometry_buffers();

	vk_create_geometry_buffers( vk.geometry_buffer_size_new );
	vk.geometry_buffer_size_new = 0;

	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		vk_update_uniform_descriptor( vk.tess[ i ].uniform_descriptor, vk.tess[ i ].vertex_buffer );
		// fresh buffers: every cached eyeProj slot is gone
		vk.tess[ i ].eyeproj_cache_valid = qfalse;
	}
}

/*
==============================================================================

XR FRAME FUNCTIONS

These functions handle OpenXR frame lifecycle, separate from desktop swapchain.
XR swapchain images are owned by OpenXR, not Vulkan.

==============================================================================
*/

void vk_begin_frame( uint32_t colorIndex, uint32_t depthIndex )
{
	VkCommandBufferBeginInfo begin_info;

	// Descriptor sets are dead during the map-load window (between
	// vk_release_resources() and vk_init_descriptors()); refuse to start a
	// frame that would bind freed or dangling descriptors.
	if ( !vk.descriptorsReady ) {
		ri.Printf( PRINT_DEVELOPER, "vk_begin_frame: skipped, descriptor sets not initialized\n" );
		// Guard: abandon a frame left open across the pool reset rather than
		// let vk_end_frame submit dead-set binds.
		if ( vk.recordingCommands || vk.inRenderPass ) {
			vk_discard_frame();
		}
		vk.frame_count = 0;
		return;
	}

	// If a frame is already in progress, we need to properly finish it first
	// This can happen during loading when VR_Renderer_SubmitLoadingFrame starts new frames
	// but the renderer doesn't always render to them (e.g., during reinit when uivm is NULL)
	if ( vk.frame_count > 0 || vk.recordingCommands ) {
		vk_finish_frame();
	}

	// Always start fresh: increment frame count
	vk.frame_count++;

#ifdef USE_UPLOAD_QUEUE
	vk_flush_staging_buffer( qtrue );
#endif

	// Rotate to next command buffer
	vk.cmd_index = (vk.cmd_index + 1) % NUM_COMMAND_BUFFERS;
	vk.cmd = &vk.tess[ vk.cmd_index ];
	vk.cmd->hud_begun = qfalse;
	vk.inHudCommandBuffer = qfalse;

	// Wait for this command buffer's previous work to complete
	if ( vk.cmd->waitForFence ) {
		VkResult res = qvkWaitForFences( vk.device, 1, &vk.cmd->rendering_finished_fence, VK_FALSE, 1e10 );
		if ( res != VK_SUCCESS ) {
			if ( res == VK_ERROR_DEVICE_LOST ) {
				ri.Printf( PRINT_WARNING, "vk_begin_frame: vkWaitForFences returned %s\n", vk_result_string( res ) );
			} else {
				ri.Error( ERR_FATAL, "vk_begin_frame: vkWaitForFences returned %s", vk_result_string( res ) );
			}
		}
		VK_CHECK( qvkResetFences( vk.device, 1, &vk.cmd->rendering_finished_fence ) );
		vk.cmd->waitForFence = qfalse;
	}

	// Validate XR resources are initialized BEFORE accessing arrays
	if ( !vk.xr.initialized || !vk.xr.colorInfo ) {
		ri.Printf( PRINT_WARNING, "vk_begin_frame: XR resources not initialized (init=%d, colorInfo=%p)\n",
			vk.xr.initialized, (void*)vk.xr.colorInfo );
		vk.frame_count = 0;  // Reset so vk_end_frame won't try to submit
		return;
	}

	// Validate indices are in bounds
	if ( colorIndex >= vk.xr.colorInfo->imageCount ) {
		ri.Printf( PRINT_WARNING, "vk_begin_frame: colorIndex %u >= imageCount %u\n",
			colorIndex, vk.xr.colorInfo->imageCount );
		vk.frame_count = 0;
		return;
	}

	// Store current XR swapchain indices (after validation)
	vk.xr.colorIndex = colorIndex;
	vk.xr.depthIndex = depthIndex;

	// Validate framebuffer exists based on rendering mode
	// When FBO is active: use subpass framebuffers (main_with_bloom or main_with_gamma)
	// When FBO is NOT active: use xr->framebuffers[] (direct to XR swapchain)
	if ( vk.fboActive ) {
		VkFramebuffer subpassFb = r_bloom->integer ?
			vk.framebuffers.main_with_bloom[colorIndex] :
			vk.framebuffers.main_with_gamma[colorIndex];
		if ( subpassFb == VK_NULL_HANDLE ) {
			ri.Printf( PRINT_WARNING, "vk_begin_frame: subpass framebuffer is NULL (FBO mode, colorIndex=%d)\n", colorIndex );
			vk.frame_count = 0;
			return;
		}
	} else {
		// Direct mode: validate XR swapchain framebuffer
		if ( vk.xr.framebuffers[colorIndex] == VK_NULL_HANDLE ) {
			ri.Printf( PRINT_WARNING, "vk_begin_frame: XR framebuffer[%d] is NULL (direct mode)\n", colorIndex );
			vk.frame_count = 0;
			return;
		}
	}

	// Begin command buffer
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.pNext = NULL;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	begin_info.pInheritanceInfo = NULL;

	VK_CHECK( qvkBeginCommandBuffer( vk.cmd->command_buffer, &begin_info ) );
	vk.recordingCommands = qtrue;

	// Batch FBO and XR layout transitions into a single pipeline barrier
	{
		VkImageMemoryBarrier barriers[5];
		uint32_t barrierCount = 0;

		if ( vk.color_image != VK_NULL_HANDLE ) {
			barriers[barrierCount].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barriers[barrierCount].pNext = NULL;
			barriers[barrierCount].srcAccessMask = VK_ACCESS_NONE;
			barriers[barrierCount].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			barriers[barrierCount].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			barriers[barrierCount].newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			barriers[barrierCount].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barriers[barrierCount].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barriers[barrierCount].image = vk.color_image;
			barriers[barrierCount].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			barriers[barrierCount].subresourceRange.baseMipLevel = 0;
			barriers[barrierCount].subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
			barriers[barrierCount].subresourceRange.baseArrayLayer = 0;
			barriers[barrierCount].subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
			barrierCount++;
		}

		if ( vk.depth_image != VK_NULL_HANDLE ) {
			barriers[barrierCount].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barriers[barrierCount].pNext = NULL;
			barriers[barrierCount].srcAccessMask = VK_ACCESS_NONE;
			barriers[barrierCount].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			barriers[barrierCount].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			barriers[barrierCount].newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			barriers[barrierCount].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barriers[barrierCount].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barriers[barrierCount].image = vk.depth_image;
			barriers[barrierCount].subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
			barriers[barrierCount].subresourceRange.baseMipLevel = 0;
			barriers[barrierCount].subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
			barriers[barrierCount].subresourceRange.baseArrayLayer = 0;
			barriers[barrierCount].subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
			barrierCount++;
		}

		if ( vk.msaaActive && vk.msaa_image != VK_NULL_HANDLE ) {
			barriers[barrierCount].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barriers[barrierCount].pNext = NULL;
			barriers[barrierCount].srcAccessMask = VK_ACCESS_NONE;
			barriers[barrierCount].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			barriers[barrierCount].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			barriers[barrierCount].newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			barriers[barrierCount].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barriers[barrierCount].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barriers[barrierCount].image = vk.msaa_image;
			barriers[barrierCount].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			barriers[barrierCount].subresourceRange.baseMipLevel = 0;
			barriers[barrierCount].subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
			barriers[barrierCount].subresourceRange.baseArrayLayer = 0;
			barriers[barrierCount].subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
			barrierCount++;
		}

		// Direct mode: XR swapchain images
		if ( !vk.fboActive && vk.xr.colorInfo && vk.xr.depthInfo ) {
			barriers[barrierCount].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barriers[barrierCount].pNext = NULL;
			barriers[barrierCount].srcAccessMask = VK_ACCESS_NONE;
			barriers[barrierCount].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			barriers[barrierCount].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			barriers[barrierCount].newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			barriers[barrierCount].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barriers[barrierCount].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barriers[barrierCount].image = vk.xr.colorInfo->images[colorIndex];
			barriers[barrierCount].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			barriers[barrierCount].subresourceRange.baseMipLevel = 0;
			barriers[barrierCount].subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
			barriers[barrierCount].subresourceRange.baseArrayLayer = 0;
			barriers[barrierCount].subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
			barrierCount++;

			barriers[barrierCount].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barriers[barrierCount].pNext = NULL;
			barriers[barrierCount].srcAccessMask = VK_ACCESS_NONE;
			barriers[barrierCount].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			barriers[barrierCount].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			barriers[barrierCount].newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			barriers[barrierCount].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barriers[barrierCount].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barriers[barrierCount].image = vk.xr.depthInfo->images[depthIndex];
			barriers[barrierCount].subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
			barriers[barrierCount].subresourceRange.baseMipLevel = 0;
			barriers[barrierCount].subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
			barriers[barrierCount].subresourceRange.baseArrayLayer = 0;
			barriers[barrierCount].subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
			barrierCount++;
		}

		if ( barrierCount > 0 ) {
			qvkCmdPipelineBarrier( vk.cmd->command_buffer,
				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
				VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
				0, 0, NULL, 0, NULL, barrierCount, barriers );
		}
	}

	// Track stats
	if ( vk.cmd->vertex_buffer_offset > vk.stats.vertex_buffer_max ) {
		vk.stats.vertex_buffer_max = vk.cmd->vertex_buffer_offset;
	}
	if ( vk.stats.push_size > vk.stats.push_size_max ) {
		vk.stats.push_size_max = vk.stats.push_size;
	}

	vk.cmd->last_pipeline = VK_NULL_HANDLE;
	backEnd.screenMapDone = qfalse;
	vk.subpassPostDone = qfalse;  // Reset subpass post flag for new frame

	// XR always uses the main multiview render pass: no screenmap in VR
	vk_begin_main_render_pass();  // Clear framebuffer at start of frame

	// Reset dynamic buffers for new frame
	vk.cmd->uniform_read_offset = 0;
	vk.cmd->vertex_buffer_offset = 0;
	Com_Memset( vk.cmd->buf_offset, 0, sizeof( vk.cmd->buf_offset ) );
	Com_Memset( vk.cmd->vbo_offset, 0, sizeof( vk.cmd->vbo_offset ) );
	vk.cmd->curr_index_buffer = VK_NULL_HANDLE;
	vk.cmd->curr_index_offset = 0;
	vk.cmd->num_indexes = 0;

	Com_Memset( &vk.cmd->descriptor_set, 0, sizeof( vk.cmd->descriptor_set ) );
	vk.cmd->descriptor_set.start = ~0U;

	Com_Memset( &vk.cmd->scissor_rect, 0, sizeof( vk.cmd->scissor_rect ) );

	// the ring restarted at offset 0: last frame's cached eyeProj slot is gone
	vk.cmd->eyeproj_cache_valid = qfalse;

	// prime set 0 binding 1 so every dynamic-offset bind this frame has a
	// valid eyeproj_offset even before the first RB_BeginDrawingView() (e.g. 2D/menu draws)
	VK_PushEyeProj();

	vk.stats.push_size = 0;
}


void vk_end_frame( void )
{
	VkSubmitInfo submit_info;
	VkCommandBuffer buffers[2];
	uint32_t bufferCount = 0;

	if ( vk.frame_count == 0 && !vk.recordingCommands ) {
		return;
	}

	vk.frame_count = 0;

	// a HUD bracket left open closes here; everything below is on the frame's command buffer
	vk_leave_hud_command_buffer();

	// Handle geometry buffer resize if needed
	if ( vk.geometry_buffer_size_new ) {
		vk_resize_geometry_buffer();
		return;
	}

	if ( vk.xr.initialized ) {
		// Check if we're using subpass optimization
		if ( vk.renderPassIndex == RENDER_PASS_MAIN_WITH_POST && vk.inRenderPass ) {
			// Subpass path: need to finish subpasses and end render pass
			if ( VK_IN_POST_SCENE_2D() ) {
				// Already in post-bloom 2D subpass, just end it
				vk_end_post_scene_subpass();
			} else if ( vk.subpassPostDone ) {
				// Subpass post done but not in post-bloom 2D (shouldn't happen, but handle it)
				vk_end_render_pass();
			} else {
				// Need to transition through subpasses first
				vk.cmd->last_pipeline = VK_NULL_HANDLE;
				vk_finish_subpass_post();
				// vk_finish_subpass_post transitions to post-bloom 2D subpass but doesn't end it

				// Fallback deferred-corona site for frames without an RC_SCENE_COMPLETE
				// command; doneFlares makes this a no-op once the tr_backend hook ran.
				RB_RenderDeferredFlares();
				// Fallback replay of the in-world HUD sprite over the corona; hudDeferred
				// makes this a no-op once the tr_backend hook already replayed it.
				RB_DrawDeferredHud();

				// Now end the post-bloom 2D subpass
				vk_end_post_scene_subpass();
			}
		}
		else if ( vk.renderPassIndex == RENDER_PASS_POST_SCENE_2D && vk.inRenderPass ) {
			// Already transitioned to post-bloom 2D subpass (vk_finish_subpass_post changed renderPassIndex)
			vk_end_post_scene_subpass();
		}
		else if ( vk.subpassPostDone ) {
			// Subpass post was already done earlier (e.g., before HUD/overlay rendering)
			// Skip: swapchain already has final output
			vk_end_render_pass();
		}
		else {
			// Direct-mode fallback for frames without an RC_SCENE_COMPLETE
			// command; doneFlares/hudDeferred no-op these once the tr_backend
			// hook ran, and both refuse to draw once 2D projection is active.
			if ( vk.renderPassIndex == RENDER_PASS_MAIN && vk.inRenderPass ) {
				RB_RenderDeferredFlares();
				RB_DrawDeferredHud();
			}
			// Fallback: end current render pass (non-subpass path)
			// This can happen if subpass resources failed to create or for r_fbo 0
			vk_end_render_pass();
		}
	}

	// Only proceed if we're actually recording commands
	if ( !vk.recordingCommands ) {
		vk.renderPassIndex = RENDER_PASS_MAIN;
		return;
	}

	VK_CHECK( qvkEndCommandBuffer( vk.cmd->command_buffer ) );
	vk.recordingCommands = qfalse;

	// HUD command buffer first: the frame's samples what it drew, ordered by the HUD pass's external dependencies.
	if ( vk.cmd->hud_begun ) {
		VK_CHECK( qvkEndCommandBuffer( vk.cmd->hud_command_buffer ) );
		buffers[bufferCount++] = vk.cmd->hud_command_buffer;
		vk.cmd->hud_begun = qfalse;
	}
	buffers[bufferCount++] = vk.cmd->command_buffer;

	// Submit command buffers with fence
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.pNext = NULL;
	submit_info.waitSemaphoreCount = 0;
	submit_info.pWaitSemaphores = NULL;
	submit_info.pWaitDstStageMask = NULL;
	submit_info.commandBufferCount = bufferCount;
	submit_info.pCommandBuffers = buffers;
	submit_info.signalSemaphoreCount = 0;
	submit_info.pSignalSemaphores = NULL;

	VK_CHECK( qvkQueueSubmit( vk.queue, 1, &submit_info, vk.cmd->rendering_finished_fence ) );
	vk.cmd->waitForFence = qtrue;

	backEnd.pc.msec = ri.Milliseconds() - backEnd.pc.msec;
	vk.renderPassIndex = RENDER_PASS_MAIN;
}


/*
Shared by vk_finish_frame / vk_discard_frame: end an interrupted render
pass and report whether a command buffer is open: the finish path submits
it, the discard path drops it.
*/
static qboolean vk_end_interrupted_pass( void )
{
	// Safety check: if cmd is null or command buffer is invalid, we can't do anything
	if ( !vk.cmd || vk.cmd->command_buffer == VK_NULL_HANDLE ) {
		vk.inRenderPass = qfalse;
		vk.recordingCommands = qfalse;
		vk.frame_count = 0;
		vk.renderPassIndex = RENDER_PASS_MAIN;
		return qfalse;
	}

	// If we're in a render pass, end it first
	if ( vk.inRenderPass && vk.recordingCommands ) {
		// Only the post pass needs advancing, and only when the composite ran without the 2D subpass being entered.
		if ( vk.renderPassIndex == RENDER_PASS_MAIN_WITH_POST ) {
			if ( vk.subpassPostDone && !VK_IN_POST_SCENE_2D() ) {
				qvkCmdNextSubpass( vk.cmd->command_buffer, VK_SUBPASS_CONTENTS_INLINE );  // to 1 (post-scene 2D)
			}
		} else if ( vk.renderPassIndex == RENDER_PASS_POST_SCENE_2D ) {
			// Already in post-bloom 2D subpass (vk_finish_subpass_post changed renderPassIndex)
			// Already in final subpass, safe to end
		}
		// All other render pass types (RENDER_PASS_MAIN, HUD, etc.) just end directly
		qvkCmdEndRenderPass( vk.cmd->command_buffer );
		vk.inRenderPass = qfalse;
		vk.subpassPostDone = qfalse;
	}
	return vk.recordingCommands;
}


/*
==============================================================================

vk_finish_frame - Force-finish an interrupted frame

Called when we need to cleanly end an in-progress frame, e.g., during shutdown
or when starting a new frame before the previous one was properly ended.
This ensures render passes are ended and command buffers are properly submitted.

==============================================================================
*/
void vk_finish_frame( void )
{
	VkSubmitInfo submit_info;
	VkCommandBuffer buffers[2];
	uint32_t bufferCount = 0;

	// a HUD bracket cut short closes first; the frame's pass is dealt with below
	vk_leave_hud_command_buffer();

	if ( vk_end_interrupted_pass() ) {
		VK_CHECK( qvkEndCommandBuffer( vk.cmd->command_buffer ) );
		vk.recordingCommands = qfalse;

		if ( vk.cmd && vk.cmd->hud_begun ) {
			VK_CHECK( qvkEndCommandBuffer( vk.cmd->hud_command_buffer ) );
			buffers[bufferCount++] = vk.cmd->hud_command_buffer;
			vk.cmd->hud_begun = qfalse;
		}
		buffers[bufferCount++] = vk.cmd->command_buffer;

		// Submit with fence so we can wait for completion
		submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit_info.pNext = NULL;
		submit_info.waitSemaphoreCount = 0;
		submit_info.pWaitSemaphores = NULL;
		submit_info.pWaitDstStageMask = NULL;
		submit_info.commandBufferCount = bufferCount;
		submit_info.pCommandBuffers = buffers;
		submit_info.signalSemaphoreCount = 0;
		submit_info.pSignalSemaphores = NULL;

		VK_CHECK( qvkQueueSubmit( vk.queue, 1, &submit_info, vk.cmd->rendering_finished_fence ) );
		vk.cmd->waitForFence = qtrue;
	}

	// Reset frame tracking
	vk.frame_count = 0;
	vk.renderPassIndex = RENDER_PASS_MAIN;
}


/*
==============================================================================

vk_discard_frame - Abandon an interrupted frame without submitting it

The frame is incomplete and its output is irrelevant at teardown; submitting
it would hand the queue draws referencing resources RE_Shutdown is about to
destroy, with attachments possibly left mid-pass (VUID-vkCmdDraw-None-09600).

Uses the shared vk_end_interrupted_pass() helper for the subpass-advance
logic (required by the subpass optimization path: the recorded
EndRenderPass is validated at record time even though the buffer is never
submitted) but skips the queue submit.

==============================================================================
*/
void vk_discard_frame( void )
{
	vk_leave_hud_command_buffer();

	if ( vk_end_interrupted_pass() ) {
		VK_CHECK( qvkEndCommandBuffer( vk.cmd->command_buffer ) );
		vk.recordingCommands = qfalse;
	}
	// The HUD command buffer is never submitted; its next begin resets it
	if ( vk.cmd && vk.cmd->hud_begun ) {
		VK_CHECK( qvkEndCommandBuffer( vk.cmd->hud_command_buffer ) );
		vk.cmd->hud_begun = qfalse;
	}

	// Do not touch vk.cmd->waitForFence: it may still track this slot's
	// previous real submission, which vk_begin_frame must still wait on.
	vk.frame_count = 0;
	vk.renderPassIndex = RENDER_PASS_MAIN;
}


static qboolean is_bgr( VkFormat format ) {
	switch ( format ) {
		case VK_FORMAT_B8G8R8A8_UNORM:
		case VK_FORMAT_B8G8R8A8_SNORM:
		case VK_FORMAT_B8G8R8A8_UINT:
		case VK_FORMAT_B8G8R8A8_SINT:
		case VK_FORMAT_B8G8R8A8_SRGB:
		case VK_FORMAT_B4G4R4A4_UNORM_PACK16:
			return qtrue;
		default:
			return qfalse;
	}
}


void vk_read_pixels( byte *buffer, uint32_t width, uint32_t height )
{
	VkCommandBuffer command_buffer;
	VkDeviceMemory memory;
	VkMemoryRequirements memory_requirements;
	VkMemoryPropertyFlags memory_reqs;
	VkMemoryPropertyFlags memory_flags;
	VkMemoryAllocateInfo alloc_info;
	VkImageSubresource subresource;
	VkSubresourceLayout layout;
	VkImageCreateInfo desc;
	VkImage srcImage;
	VkImageLayout srcImageLayout;
	VkImage dstImage;
	byte *buffer_ptr;
	byte *data;
	uint32_t pixel_width;
	uint32_t i, n;
	qboolean invalidate_ptr;

	VK_CHECK( qvkWaitForFences( vk.device, 1, &vk.cmd->rendering_finished_fence, VK_FALSE, 1e12 ) );

	if ( vk.capture.image ) {
		// dedicated capture buffer
		srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		srcImage = vk.capture.image;
	} else if ( vk.color_image ) {
		// Legacy FBO color image (direct mode or non-subpass FBO mode)
		srcImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		srcImage = vk.color_image;
	} else if ( vk.xr.initialized && vk.xr.colorInfo && vk.xr.colorIndex < vk.xr.colorInfo->imageCount &&
		( vk.xr.colorInfo->usage & XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT ) ) {
		// Subpass FBO mode: this frame's image is not submitted yet, so this reads the previous
		// complete frame -- fine for a screenshot. Left eye only; foveated swapchains are not transferable.
		srcImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		srcImage = vk.xr.colorInfo->images[vk.xr.colorIndex];
	} else {
		ri.Printf( PRINT_WARNING, "vk_read_pixels: no image to capture\n" );
		Com_Memset( buffer, 0, width * height * 4 );
		return;
	}

	Com_Memset( &desc, 0, sizeof( desc ) );

	// Create image in host visible memory to serve as a destination for framebuffer pixels.
	desc.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.imageType = VK_IMAGE_TYPE_2D;
	desc.format = vk.capture_format;
	desc.extent.width = width;
	desc.extent.height = height;
	desc.extent.depth = 1;
	desc.mipLevels = 1;
	desc.arrayLayers = 1;
	desc.samples = VK_SAMPLE_COUNT_1_BIT;
	desc.tiling = VK_IMAGE_TILING_LINEAR;
	desc.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	desc.queueFamilyIndexCount = 0;
	desc.pQueueFamilyIndices = NULL;
	desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VK_CHECK( qvkCreateImage( vk.device, &desc, NULL, &dstImage ) );

	qvkGetImageMemoryRequirements( vk.device, dstImage, &memory_requirements );

	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.pNext = NULL;
	alloc_info.allocationSize = memory_requirements.size;

	// host_cached bit is desirable for fast reads
	memory_reqs = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
	alloc_info.memoryTypeIndex = find_memory_type2( memory_requirements.memoryTypeBits, memory_reqs, &memory_flags );
	if ( alloc_info.memoryTypeIndex == ~0 ) {
		// try less explicit flags, without host_coherent
		memory_reqs = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
		alloc_info.memoryTypeIndex = find_memory_type2( memory_requirements.memoryTypeBits, memory_reqs, &memory_flags );
		if ( alloc_info.memoryTypeIndex == ~0U ) {
			// slowest case
			memory_reqs = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
			alloc_info.memoryTypeIndex = find_memory_type2( memory_requirements.memoryTypeBits, memory_reqs, &memory_flags );
			if ( alloc_info.memoryTypeIndex == ~0U ) {
				ri.Error( ERR_FATAL, "%s(): failed to find matching memory type for image capture", __func__ );
			}
		}
	}

	if ( memory_flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT ) {
		invalidate_ptr = qfalse;
	} else {
		 // according to specification - must be performed if host_coherent is not set
		invalidate_ptr = qtrue;
	}

	VK_CHECK(qvkAllocateMemory(vk.device, &alloc_info, NULL, &memory));
	VK_CHECK(qvkBindImageMemory(vk.device, dstImage, memory, 0));

	command_buffer = begin_command_buffer();

	if ( srcImageLayout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL ) {
		record_image_layout_transition( command_buffer, srcImage,
			VK_IMAGE_ASPECT_COLOR_BIT,
			srcImageLayout,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			0, 0);
	}

	record_image_layout_transition( command_buffer, dstImage,
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 0 );

	// end_command_buffer( command_buffer );

	// command_buffer = begin_command_buffer();

	if ( vk.blitEnabled ) {
		VkImageBlit region;

		region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.srcSubresource.mipLevel = 0;
		region.srcSubresource.baseArrayLayer = 0;
		region.srcSubresource.layerCount = 1;
		region.srcOffsets[0].x = 0;
		region.srcOffsets[0].y = 0;
		region.srcOffsets[0].z = 0;
		region.srcOffsets[1].x = width;
		region.srcOffsets[1].y = height;
		region.srcOffsets[1].z = 1;
		region.dstSubresource = region.srcSubresource;
		region.dstOffsets[0] = region.srcOffsets[0];
		region.dstOffsets[1] = region.srcOffsets[1];

		qvkCmdBlitImage( command_buffer, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region, VK_FILTER_NEAREST );

	} else {
		VkImageCopy region;

		region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.srcSubresource.mipLevel = 0;
		region.srcSubresource.baseArrayLayer = 0;
		region.srcSubresource.layerCount = 1;
		region.srcOffset.x = 0;
		region.srcOffset.y = 0;
		region.srcOffset.z = 0;
		region.dstSubresource = region.srcSubresource;
		region.dstOffset = region.srcOffset;
		region.extent.width = width;
		region.extent.height = height;
		region.extent.depth = 1;

		qvkCmdCopyImage( command_buffer, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region );
	}

	end_command_buffer( command_buffer, __func__ );

	// Copy data from destination image to memory buffer.
	subresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subresource.mipLevel = 0;
	subresource.arrayLayer = 0;

	qvkGetImageSubresourceLayout( vk.device, dstImage, &subresource, &layout );

	VK_CHECK( qvkMapMemory( vk.device, memory, 0, VK_WHOLE_SIZE, 0, (void**)&data ) );

	if ( invalidate_ptr )
	{
		VkMappedMemoryRange range;
		range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		range.pNext = NULL;
		range.memory = memory;
		range.size = VK_WHOLE_SIZE;
		range.offset = 0;
		qvkInvalidateMappedMemoryRanges( vk.device, 1, &range );
	}

	data += layout.offset;

	switch ( vk.capture_format ) {
		case VK_FORMAT_B4G4R4A4_UNORM_PACK16: pixel_width = 2; break;
		case VK_FORMAT_R16G16B16A16_UNORM: pixel_width = 8; break;
		default: pixel_width = 4; break;
	}

	buffer_ptr = buffer + width * (height - 1) * 3;
	for ( i = 0; i < height; i++ ) {
		switch ( pixel_width ) {
			case 2: {
				uint16_t *src = (uint16_t*)data;
				for ( n = 0; n < width; n++ ) {
					buffer_ptr[n*3+0] = ((src[n]>>12)&0xF)<<4;
					buffer_ptr[n*3+1] = ((src[n]>>8)&0xF)<<4;
					buffer_ptr[n*3+2] = ((src[n]>>4)&0xF)<<4;
				}
			} break;

			case 4: {
				for ( n = 0; n < width; n++ ) {
					Com_Memcpy( &buffer_ptr[n*3], &data[n*4], 3 );
					//buffer_ptr[n*3+0] = data[n*4+0];
					//buffer_ptr[n*3+1] = data[n*4+1];
					//buffer_ptr[n*3+2] = data[n*4+2];
				}
			} break;

			case 8: {
				const uint16_t *src = (uint16_t*)data;
				for ( n = 0; n < width; n++ ) {
					buffer_ptr[n*3+0] = src[n*4+0]>>8;
					buffer_ptr[n*3+1] = src[n*4+1]>>8;
					buffer_ptr[n*3+2] = src[n*4+2]>>8;
				}
			} break;
		}
		buffer_ptr -= width * 3;
		data += layout.rowPitch;
	}

	if ( is_bgr( vk.capture_format ) ) {
		buffer_ptr = buffer;
		for ( i = 0; i < width * height; i++ ) {
			byte tmp = buffer_ptr[0];
			buffer_ptr[0] = buffer_ptr[2];
			buffer_ptr[2] = tmp;
			buffer_ptr += 3;
		}
	}

	qvkDestroyImage( vk.device, dstImage, NULL );
	qvkFreeMemory( vk.device, memory, NULL );

	// restore previous layout
	if ( srcImageLayout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL ) {
		command_buffer = begin_command_buffer();

		record_image_layout_transition( command_buffer, srcImage,
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			srcImageLayout, 0, 0 );

		end_command_buffer( command_buffer, "restore layout" );
	}
}


// Legacy vk_bloom() function removed: bloom is now handled by subpass optimization
// in vk_finish_subpass_post() which uses tile-local memory for bandwidth savings


/*
 * vk_run_bloom_blur_chain - Blur the bright pass into vk.bloom_image, horizontal then vertical, at 1/2 down to 1/16
 *
 * vk_begin/end_render_pass dispatch on vk.renderPassIndex and vk.inRenderPass, which
 * still describe the caller's already-ended pass, so save them across the chain.
 */
static void vk_run_bloom_blur_chain( void )
{
	const renderPass_t savedPassIndex = vk.renderPassIndex;
	const qboolean savedInRenderPass = vk.inRenderPass;
	const uint32_t width = vk.xr.width;
	const uint32_t height = vk.xr.height;
	uint32_t i;

	vk.renderPassIndex = RENDER_PASS_MAIN;
	vk.inRenderPass = qfalse;

	for ( i = 0; i < VK_NUM_BLOOM_PASSES; i++ ) {
		uint32_t blur_width = width / ( 2 << i );
		uint32_t blur_height = height / ( 2 << i );
		// The first pass samples the stored scene and extracts itself
		const VkDescriptorSet *source = ( i == 0 )
			? &vk.transient.scene_descriptor
			: &vk.bloom_image_descriptor[i*2];

		// Horizontal blur
		vk_begin_render_pass( vk.render_pass.blur[i*2], vk.framebuffers.blur[i*2],
			qfalse, blur_width, blur_height );
		qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
			vk.blur_pipeline[i*2] );
		qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
			vk.pipeline_layout_post_process, 0, 1, source, 0, NULL );
		qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
		vk_end_render_pass();

		// Vertical blur
		vk_begin_render_pass( vk.render_pass.blur[i*2+1], vk.framebuffers.blur[i*2+1],
			qfalse, blur_width, blur_height );
		qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
			vk.blur_pipeline[i*2+1] );
		qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
			vk.pipeline_layout_post_process, 0, 1, &vk.bloom_image_descriptor[i*2+1],
			0, NULL );
		qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
		vk_end_render_pass();
	}

	vk.renderPassIndex = savedPassIndex;
	vk.inRenderPass = savedInRenderPass;
}


/*
 * vk_begin_fov_post_pass - End the scene pass, blur, begin the post pass
 *
 * No clear values: every attachment of the post pass is LOAD or DONT_CARE. The blur
 * chain runs here, so the composite gets this frame's blur and needs no reprojection.
 */
static void vk_begin_fov_post_pass( void )
{
	VkRenderPassBeginInfo beginInfo;
	const qboolean useBloom = ( r_bloom && r_bloom->integer );

	qvkCmdEndRenderPass( vk.cmd->command_buffer );

	if ( useBloom && vk.blur_pipeline[0] != VK_NULL_HANDLE ) {
		VkMemoryBarrier barrier;

		vk_run_bloom_blur_chain();

		// The blur passes declare only a color-output external dependency, and the post
		// pass's own is by-region, which does not cover sampling blur at 1/2 to 1/16.
		Com_Memset( &barrier, 0, sizeof( barrier ) );
		barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
		barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		qvkCmdPipelineBarrier( vk.cmd->command_buffer,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			0, 1, &barrier, 0, NULL, 0, NULL );
	}

	Com_Memset( &beginInfo, 0, sizeof( beginInfo ) );
	beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	beginInfo.renderPass = useBloom ? vk.render_pass.main_with_bloom : vk.render_pass.main_with_gamma;
	beginInfo.framebuffer = useBloom ? vk.framebuffers.main_with_bloom[vk.xr.colorIndex] : vk.framebuffers.main_with_gamma[vk.xr.colorIndex];
	beginInfo.renderArea.offset.x = 0;
	beginInfo.renderArea.offset.y = 0;
	beginInfo.renderArea.extent.width = vk.renderWidth;
	beginInfo.renderArea.extent.height = vk.renderHeight;
	beginInfo.clearValueCount = 0;
	beginInfo.pClearValues = NULL;

	qvkCmdBeginRenderPass( vk.cmd->command_buffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE );
}

/*
 * vk_finish_subpass_post - End the scene pass, run the post pass's first subpass
 *
 * Leaves the post pass open in its 2D subpass; vk_end_post_scene_subpass() completes it.
 */
void vk_finish_subpass_post( void )
{
	qboolean useBloom;

	if ( !vk.inRenderPass ) {
		ri.Printf( PRINT_WARNING, "vk_finish_subpass_post: not in render pass\n" );
		return;
	}

	// Safety check for valid command buffer (during map loads/shutdowns vk.cmd may be NULL)
	if ( !vk.cmd || vk.cmd->command_buffer == VK_NULL_HANDLE ) {
		ri.Printf( PRINT_WARNING, "vk_finish_subpass_post: no valid command buffer\n" );
		vk.inRenderPass = qfalse;
		return;
	}

	// The scene subpass ends here, at the 3D to 2D boundary, so this is the tint's last chance
	vk_draw_foveation_debug();

	useBloom = ( r_bloom && r_bloom->integer );

	if ( useBloom && vk.final_composite_subpass_pipeline != VK_NULL_HANDLE ) {
		// End the scene pass, blur it, and open the post pass on the composite
		vk_begin_fov_post_pass();

		qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
			vk.final_composite_subpass_pipeline );

		// Stored scene (set 0) and the combined bloom blur textures (set 1)
		{
			VkDescriptorSet descriptorSets[2];
			descriptorSets[0] = vk.transient.scene_descriptor;
			descriptorSets[1] = vk.bloom_blur_combined_descriptor;

			qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
				vk.pipeline_layout_fov_composite, 0, 2, descriptorSets, 0, NULL );
		}

		qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );

		// Transition to the post-scene 2D subpass
		// Don't end the render pass: 2D content will render in this subpass
		qvkCmdNextSubpass( vk.cmd->command_buffer, VK_SUBPASS_CONTENTS_INLINE );

		ri.Printf( PRINT_DEVELOPER, "vk_finish_subpass_post: transitioned to the post-scene 2D subpass\n" );

		vk.renderPassIndex = RENDER_PASS_POST_SCENE_2D;  // Use special pass for 2D pipelines in subpass 3
		vk.subpassPostDone = qtrue;  // Mark bloom/gamma subpasses as done
		backEnd.doneBloom = qtrue;
		vk.cmd->last_pipeline = VK_NULL_HANDLE;  // Force pipeline rebind for subpass 3
		vk.cmd->depth_range = DEPTH_RANGE_COUNT;  // Force viewport/scissor reset

		// Reset descriptor set tracking for 2D rendering
		// The composite pass bound descriptors with pipeline_layout_fov_composite,
		// but 2D rendering uses pipeline_layout: must clear stale bindings
		Com_Memset( vk.cmd->descriptor_set.current, 0, sizeof( vk.cmd->descriptor_set.current ) );
		vk.cmd->descriptor_set.start = ~0U;
		vk.cmd->descriptor_set.end = 0;

		// Reset scissor rect tracking to force recalculation for 2D rendering
		Com_Memset( &vk.cmd->scissor_rect, 0, sizeof( vk.cmd->scissor_rect ) );

		// Reset vertex buffer offset tracking for fresh 2D geometry
		// The composite pass doesn't bind vertex buffers, but 3D scene rendering in subpass 0
		// left stale offsets that could cause vertex data to be read from wrong locations
		Com_Memset( vk.cmd->buf_offset, 0, sizeof( vk.cmd->buf_offset ) );

		// Explicitly set viewport and scissor for the full render area
		// This ensures we have valid dynamic state for 2D rendering in subpass 3
		{
			VkViewport viewport;
			VkRect2D scissor;
			viewport.x = 0;
			viewport.y = 0;
			viewport.width = (float)vk.renderWidth;
			viewport.height = (float)vk.renderHeight;
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;
			qvkCmdSetViewport( vk.cmd->command_buffer, 0, 1, &viewport );

			scissor.offset.x = 0;
			scissor.offset.y = 0;
			scissor.extent.width = vk.renderWidth;
			scissor.extent.height = vk.renderHeight;
			qvkCmdSetScissor( vk.cmd->command_buffer, 0, 1, &scissor );
		}

		// NOTE: Render pass is NOT ended here.
		// 2D rendering happens in subpass 3, then vk_end_post_scene_subpass() finishes.
	}
	else if ( vk.gamma_subpass_pipeline != VK_NULL_HANDLE ) {
		// === No bloom: gamma alone in the post pass's subpass 0 ===

		vk_begin_fov_post_pass();

		qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
			vk.gamma_subpass_pipeline );
		qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
			vk.pipeline_layout_fov_gamma, 0, 1, &vk.transient.scene_descriptor, 0, NULL );
		qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );

		// Transition to subpass 2 (post-gamma 2D)
		// Don't end the render pass: 2D content will render in this subpass
		qvkCmdNextSubpass( vk.cmd->command_buffer, VK_SUBPASS_CONTENTS_INLINE );

		vk.renderPassIndex = RENDER_PASS_POST_SCENE_2D;  // Use special pass for 2D pipelines in subpass 2
		vk.subpassPostDone = qtrue;  // Mark gamma subpass as done
		vk.cmd->last_pipeline = VK_NULL_HANDLE;  // Force pipeline rebind for subpass 2
		vk.cmd->depth_range = DEPTH_RANGE_COUNT;  // Force viewport/scissor reset

		// Reset descriptor set tracking for 2D rendering
		// The gamma pass bound descriptors with pipeline_layout_fov_gamma,
		// but 2D rendering uses pipeline_layout: must clear stale bindings
		Com_Memset( vk.cmd->descriptor_set.current, 0, sizeof( vk.cmd->descriptor_set.current ) );
		vk.cmd->descriptor_set.start = ~0U;
		vk.cmd->descriptor_set.end = 0;

		// Reset scissor rect tracking to force recalculation for 2D rendering
		Com_Memset( &vk.cmd->scissor_rect, 0, sizeof( vk.cmd->scissor_rect ) );

		// Reset vertex buffer offset tracking for fresh 2D geometry
		Com_Memset( vk.cmd->buf_offset, 0, sizeof( vk.cmd->buf_offset ) );

		// Explicitly set viewport and scissor for the full render area
		{
			VkViewport viewport;
			VkRect2D scissor;
			viewport.x = 0;
			viewport.y = 0;
			viewport.width = (float)vk.renderWidth;
			viewport.height = (float)vk.renderHeight;
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;
			qvkCmdSetViewport( vk.cmd->command_buffer, 0, 1, &viewport );

			scissor.offset.x = 0;
			scissor.offset.y = 0;
			scissor.extent.width = vk.renderWidth;
			scissor.extent.height = vk.renderHeight;
			qvkCmdSetScissor( vk.cmd->command_buffer, 0, 1, &scissor );
		}
	}
	else {
		// Subpass pipelines not available, fall back to ending render pass normally
		ri.Printf( PRINT_WARNING, "vk_finish_subpass_post: subpass pipelines not ready\n" );
		qvkCmdEndRenderPass( vk.cmd->command_buffer );
		vk.inRenderPass = qfalse;
		vk.renderPassIndex = RENDER_PASS_MAIN;  // Reset for subsequent rendering
		// Note: don't set subpassPostDone: the subpass wasn't actually done
	}
}


/*
 * vk_end_post_scene_subpass - End the post-scene 2D subpass and finish the pass
 *
 * Called after all post-scene 2D rendering; the blur chain ran when the scene pass ended.
 */
void vk_end_post_scene_subpass( void )
{

	if ( !vk.inRenderPass ) {
		ri.Printf( PRINT_WARNING, "vk_end_post_scene_subpass: not in render pass\n" );
		return;
	}

	if ( !VK_IN_POST_SCENE_2D() ) {
		// Not in the post-bloom 2D subpass, but we're in some render pass
		// End it anyway to maintain state consistency
		ri.Printf( PRINT_WARNING, "vk_end_post_scene_subpass: not in post-bloom 2D subpass, ending render pass anyway\n" );
		if ( vk.cmd && vk.cmd->command_buffer != VK_NULL_HANDLE ) {
			qvkCmdEndRenderPass( vk.cmd->command_buffer );
		}
		vk.inRenderPass = qfalse;
		vk.renderPassIndex = RENDER_PASS_MAIN;
		return;
	}

	// Safety check for valid command buffer (during map loads/shutdowns vk.cmd may be NULL)
	if ( !vk.cmd || vk.cmd->command_buffer == VK_NULL_HANDLE ) {
		ri.Printf( PRINT_WARNING, "vk_end_post_scene_subpass: no valid command buffer\n" );
		vk.inRenderPass = qfalse;
		return;
	}

	// End the render pass
	qvkCmdEndRenderPass( vk.cmd->command_buffer );
	vk.inRenderPass = qfalse;
	vk.renderPassIndex = RENDER_PASS_MAIN;  // Reset for subsequent rendering
}


// ============================================================================
// XR Swapchain Resource Management (Phase 2)
// These functions create VkImageViews and VkFramebuffers for XR swapchain images.
// Full implementation requires render passes (Phase 3).
// ============================================================================

#ifdef USE_VULKAN
#include "../vrvk/vr_vk_types.h"

/*
================================================================================
Authored fragment density map

The runtime's map stops at its High level and sits on the lens rather than following
the eyes, so we write our own. R8G8_UNORM texels are the fraction of a fragment to
shade per pixel. The tiler reads one texel per bin and holds it across the bin, and
scales a bin by at most four per axis, so the map carries three levels at the
resolution of bins. One map per swapchain image, rewritten in the frame that renders
into it.
================================================================================
*/

// Granularity fallback; both runtimes hand out their own maps at this size
#define VK_FDM_TEXEL_SIZE 32

void vk_destroy_authored_fdm( void )
{
	uint32_t i;

	for ( i = 0; i < MAX_SWAPCHAIN_IMAGES; i++ ) {
		if ( vk.xr.fdmStagingMapped[i] != NULL ) {
			qvkUnmapMemory( vk.device, vk.xr.fdmStagingMemory[i] );
			vk.xr.fdmStagingMapped[i] = NULL;
		}
		if ( vk.xr.fdmStaging[i] != VK_NULL_HANDLE ) {
			qvkDestroyBuffer( vk.device, vk.xr.fdmStaging[i], NULL );
			vk.xr.fdmStaging[i] = VK_NULL_HANDLE;
		}
		if ( vk.xr.fdmStagingMemory[i] != VK_NULL_HANDLE ) {
			qvkFreeMemory( vk.device, vk.xr.fdmStagingMemory[i], NULL );
			vk.xr.fdmStagingMemory[i] = VK_NULL_HANDLE;
		}
		if ( vk.xr.fdmImage[i] != VK_NULL_HANDLE ) {
			qvkDestroyImage( vk.device, vk.xr.fdmImage[i], NULL );
			vk.xr.fdmImage[i] = VK_NULL_HANDLE;
		}
		if ( vk.xr.fdmMemory[i] != VK_NULL_HANDLE ) {
			qvkFreeMemory( vk.device, vk.xr.fdmMemory[i], NULL );
			vk.xr.fdmMemory[i] = VK_NULL_HANDLE;
		}
		vk.xr.fdmUploaded[i] = qfalse;
		vk.xr.fdmAppliedLevel[i] = -1;
	}
	vk.xr.fdmAuthored = qfalse;
}

/*
==================
vk_create_authored_fdm

Fails clean so the caller can fall back to the runtime's map.
==================
*/
static qboolean vk_create_authored_fdm( uint32_t imageCount, uint32_t layers, uint32_t fbWidth, uint32_t fbHeight )
{
	VkImageCreateInfo imageInfo;
	VkBufferCreateInfo bufferInfo;
	VkMemoryRequirements memReqs;
	VkMemoryAllocateInfo allocInfo;
	// Finest granularity the device reads: a coarser grid measured no faster, and this gives smoother density steps
	const uint32_t texelW = vk.xr.fdmTexelWidth ? vk.xr.fdmTexelWidth : VK_FDM_TEXEL_SIZE;
	const uint32_t texelH = vk.xr.fdmTexelHeight ? vk.xr.fdmTexelHeight : VK_FDM_TEXEL_SIZE;
	uint32_t mapWidth, mapHeight, memoryType, i;
	VkDeviceSize bufferSize;

	if ( imageCount == 0 || imageCount > MAX_SWAPCHAIN_IMAGES || layers == 0 || fbWidth == 0 || fbHeight == 0 ) {
		return qfalse;
	}

	mapWidth = ( fbWidth + texelW - 1 ) / texelW;
	mapHeight = ( fbHeight + texelH - 1 ) / texelH;
	bufferSize = (VkDeviceSize)mapWidth * mapHeight * layers * 2;  // two bytes a texel

	Com_Memset( &imageInfo, 0, sizeof( imageInfo ) );
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.format = VK_FDM_FORMAT;
	imageInfo.extent.width = mapWidth;
	imageInfo.extent.height = mapHeight;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = layers;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.usage = VK_IMAGE_USAGE_FRAGMENT_DENSITY_MAP_BIT_EXT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	Com_Memset( &bufferInfo, 0, sizeof( bufferInfo ) );
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = bufferSize;
	bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	Com_Memset( &allocInfo, 0, sizeof( allocInfo ) );
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;

	for ( i = 0; i < imageCount; i++ ) {
		void *mapped = NULL;

		if ( qvkCreateImage( vk.device, &imageInfo, NULL, &vk.xr.fdmImage[i] ) != VK_SUCCESS ) {
			vk_destroy_authored_fdm();
			return qfalse;
		}
		qvkGetImageMemoryRequirements( vk.device, vk.xr.fdmImage[i], &memReqs );
		memoryType = find_memory_type( memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
		allocInfo.allocationSize = memReqs.size;
		allocInfo.memoryTypeIndex = memoryType;
		if ( qvkAllocateMemory( vk.device, &allocInfo, NULL, &vk.xr.fdmMemory[i] ) != VK_SUCCESS ||
			qvkBindImageMemory( vk.device, vk.xr.fdmImage[i], vk.xr.fdmMemory[i], 0 ) != VK_SUCCESS ) {
			vk_destroy_authored_fdm();
			return qfalse;
		}
		SET_OBJECT_NAME( vk.xr.fdmImage[i], va( "authored fragment density map %u", i ),
			VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );

		if ( qvkCreateBuffer( vk.device, &bufferInfo, NULL, &vk.xr.fdmStaging[i] ) != VK_SUCCESS ) {
			vk_destroy_authored_fdm();
			return qfalse;
		}
		qvkGetBufferMemoryRequirements( vk.device, vk.xr.fdmStaging[i], &memReqs );
		memoryType = find_memory_type( memReqs.memoryTypeBits,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
		allocInfo.allocationSize = memReqs.size;
		allocInfo.memoryTypeIndex = memoryType;
		if ( qvkAllocateMemory( vk.device, &allocInfo, NULL, &vk.xr.fdmStagingMemory[i] ) != VK_SUCCESS ||
			qvkBindBufferMemory( vk.device, vk.xr.fdmStaging[i], vk.xr.fdmStagingMemory[i], 0 ) != VK_SUCCESS ||
			qvkMapMemory( vk.device, vk.xr.fdmStagingMemory[i], 0, VK_WHOLE_SIZE, 0, &mapped ) != VK_SUCCESS ) {
			vk_destroy_authored_fdm();
			return qfalse;
		}
		vk.xr.fdmStagingMapped[i] = mapped;
		vk.xr.fdmUploaded[i] = qfalse;
		vk.xr.fdmAppliedLevel[i] = -1;
	}

	vk.xr.fdmAuthored = qtrue;
	vk.xr.fdmLayers = layers;
	vk.xr.foveationWidth = mapWidth;
	vk.xr.foveationHeight = mapHeight;
	vk.xr.fdmTexelWidth = texelW;
	vk.xr.fdmTexelHeight = texelH;
	return qtrue;
}

/*
==================
vk_foveation_level_angles

Falloff per strength, as eccentricity from the gaze: full resolution out to sharp, a
quarter of the pixels at sharp falling to an eighth by coarse, a sixteenth beyond. Degrees
hold the shape steady on any headset, off its own reported field of view. Fixed keeps a
wide sharp region because the eyes rove while the head stays put; the eye-tracked island
rides the fovea and can be tighter.

A wide sharp core with an early drop to a sixteenth puts the resolution where a player
tracking a target is looking, which is what this game wants from the trade.
==================
*/
static void vk_foveation_level_angles( int level, qboolean eyeTracked, float *sharpDeg, float *coarseDeg )
{
	if ( eyeTracked ) {
		switch ( level ) {
			case VR_FOVEATION_STRENGTH_LOW:    *sharpDeg = 20.0f; *coarseDeg = 33.0f; break;
			case VR_FOVEATION_STRENGTH_MEDIUM: *sharpDeg = 16.0f; *coarseDeg = 27.0f; break;
			default:                           *sharpDeg = 12.0f; *coarseDeg = 21.0f; break;
		}
		return;
	}

	// About 42, 30 and 24 percent of an unfoveated frame's fragments on a Quest 3 buffer. The map
	// asks for 30, 22 and 18, and one density sample a bin spends the difference
	switch ( level ) {
		case VR_FOVEATION_STRENGTH_LOW:    *sharpDeg = 30.0f; *coarseDeg = 41.0f; break;
		case VR_FOVEATION_STRENGTH_MEDIUM: *sharpDeg = 25.0f; *coarseDeg = 35.0f; break;
		default:                           *sharpDeg = 22.0f; *coarseDeg = 28.0f; break;
	}
}

/*
==================
vk_fdm_gaze_texel

The gaze in map texels. The map only ever changes when this does, so the upload test
compares these rather than the centers that produce them.
==================
*/
static void vk_fdm_gaze_texel( const float center[2], uint32_t width, uint32_t height,
	uint32_t *tx, uint32_t *ty )
{
	// Centers arrive with y already running down the image, so neither axis flips; fixed sits on the optical axis
	float cx = ( center[0] + 1.0f ) * 0.5f;
	float cy = ( center[1] + 1.0f ) * 0.5f;

	if ( cx < 0.0f ) cx = 0.0f; else if ( cx > 1.0f ) cx = 1.0f;
	if ( cy < 0.0f ) cy = 0.0f; else if ( cy > 1.0f ) cy = 1.0f;

	*tx = (uint32_t)( cx * (float)width );
	*ty = (uint32_t)( cy * (float)height );
	if ( *tx >= width ) *tx = width - 1;
	if ( *ty >= height ) *ty = height - 1;
}

/*
==================
vk_write_fdm_texels

A map per eye, each drawn in that eye's own frustum, since the angle a texel subtends
depends on where in the frustum it sits.

A texel and the gaze are both directions, (tan x, tan y, 1), and the eccentricity is the
angle between them. Comparing the cosine squared against the two thresholds keeps that to
a few multiplies a texel.
==================
*/
static void vk_write_fdm_texels( byte *dst, uint32_t width, uint32_t height, uint32_t layers,
	int level, qboolean eyeTracked, const float center[2][2], const float fovTan[2][4] )
{
	const float invFbWidth = ( vk.xr.width > 0 ) ? 1.0f / (float)vk.xr.width : 0.0f;
	const float invFbHeight = ( vk.xr.height > 0 ) ? 1.0f / (float)vk.xr.height : 0.0f;
	const float texelW = (float)vk.xr.fdmTexelWidth;
	const float texelH = (float)vk.xr.fdmTexelHeight;
	float sharpDeg, coarseDeg, midDeg, cosSharp, cosMid, cosCoarse;
	uint32_t layer, y, x;

	vk_foveation_level_angles( level, eyeTracked, &sharpDeg, &coarseDeg );
	// The outer half of the quarter-density band goes to a fragment twice its area
	midDeg = 0.5f * ( sharpDeg + coarseDeg );
	cosSharp = cosf( (float)DEG2RAD( sharpDeg ) );
	cosMid = cosf( (float)DEG2RAD( midDeg ) );
	cosCoarse = cosf( (float)DEG2RAD( coarseDeg ) );

	for ( layer = 0; layer < layers; layer++ ) {
		const int eye = ( layer < 2 ) ? (int)layer : 0;
		const float tanL = fovTan[eye][0], tanR = fovTan[eye][1];
		const float tanU = fovTan[eye][2], tanD = fovTan[eye][3];
		const float spanX = tanR - tanL, spanY = tanU - tanD;
		// The gaze in the same tangent space. Fixed foveation names the optical axis, which lands on zero
		const float gx = tanL + ( center[eye][0] + 1.0f ) * 0.5f * spanX;
		const float gy = tanU - ( center[eye][1] + 1.0f ) * 0.5f * spanY;
		const float gazeLen2 = gx * gx + gy * gy + 1.0f;
		const float sharpK = cosSharp * cosSharp * gazeLen2;
		const float midK = cosMid * cosMid * gazeLen2;
		const float coarseK = cosCoarse * cosCoarse * gazeLen2;

		for ( y = 0; y < height; y++ ) {
			// A bin past the buffer's edge is sampled at its own center, so the falloff carries on to meet it
			const float ty = tanU - ( ( (float)y + 0.5f ) * texelH * invFbHeight ) * spanY;

			for ( x = 0; x < width; x++ ) {
				const float tx = tanL + ( ( (float)x + 0.5f ) * texelW * invFbWidth ) * spanX;
				const float dot = tx * gx + ty * gy + 1.0f;
				const float cosNum = dot * dot;
				const float texelLen2 = tx * tx + ty * ty + 1.0f;
				byte value;

				// The device reads one texel a bin and scales it by at most four an axis, rounding
				// one axis up inside whatever area the two channels leave spare. 255, 127 and 63
				// sit far enough inside their areas to land square; 64 leaves room for the round-up
				if ( cosNum > sharpK * texelLen2 ) {
					value = 255;  // 1x1
				} else if ( cosNum > midK * texelLen2 ) {
					value = 127;  // 2x2
				} else if ( cosNum > coarseK * texelLen2 ) {
					value = 64;   // 2x4
				} else {
					value = 63;   // 4x4
				}
				dst[0] = value;   // x density
				dst[1] = value;   // y density
				dst += 2;
			}
		}
	}
}

/*
==================
vk_update_authored_fdm

Recorded before the scene render pass opens, and only when the map changed.
==================
*/
void vk_update_authored_fdm( uint32_t index )
{
	VkImageMemoryBarrier barrier;
	VkBufferImageCopy region;
	int level;
	qboolean eyeTracked;
	qboolean changed;

	if ( !vk.xr.fdmAuthored || index >= MAX_SWAPCHAIN_IMAGES || vk.xr.fdmImage[index] == VK_NULL_HANDLE ) {
		return;
	}
	if ( !vk.cmd || vk.cmd->command_buffer == VK_NULL_HANDLE ) {
		return;
	}

	level = vk.xr.fdmLevel;
	eyeTracked = vk.xr.fdmEyeTracked;
	if ( level <= 0 ) {
		// Off still needs a map, since the render pass always carries one: all ones
		level = 0;
	}

	changed = ( !vk.xr.fdmUploaded[index] ||
		vk.xr.fdmAppliedLevel[index] != level ||
		vk.xr.fdmAppliedEyeTracked[index] != eyeTracked );
	if ( !changed && level > 0 ) {
		// Same level and mode, so only a gaze that has moved a whole texel can change a byte.
		// Level 0 is all ones and reads no center at all.
		const uint32_t eyes = ( vk.xr.fdmLayers < 2 ) ? 1 : 2;
		uint32_t eye;

		for ( eye = 0; eye < eyes; eye++ ) {
			uint32_t ox, oy;

			vk_fdm_gaze_texel( vk.xr.fdmCenter[eye], vk.xr.foveationWidth, vk.xr.foveationHeight, &ox, &oy );
			if ( vk.xr.fdmAppliedOffset[index][eye][0] != ox || vk.xr.fdmAppliedOffset[index][eye][1] != oy ) {
				changed = qtrue;
				break;
			}
		}
	}
	if ( !changed ) {
		return;
	}

	if ( level == 0 ) {
		Com_Memset( vk.xr.fdmStagingMapped[index], 0xFF,
			(size_t)vk.xr.foveationWidth * vk.xr.foveationHeight * vk.xr.fdmLayers * 2 );
	} else {
		vk_write_fdm_texels( (byte*)vk.xr.fdmStagingMapped[index],
			vk.xr.foveationWidth, vk.xr.foveationHeight, vk.xr.fdmLayers,
			level, eyeTracked, (const float (*)[2])vk.xr.fdmCenter,
			(const float (*)[4])vk.xr.fdmFovTan );
	}

	Com_Memset( &barrier, 0, sizeof( barrier ) );
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = vk.xr.fdmImage[index];
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.layerCount = vk.xr.fdmLayers;

	// Whatever it held is not worth keeping, so come from UNDEFINED
	barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.srcAccessMask = 0;
	barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	qvkCmdPipelineBarrier( vk.cmd->command_buffer,
		VK_PIPELINE_STAGE_FRAGMENT_DENSITY_PROCESS_BIT_EXT, VK_PIPELINE_STAGE_TRANSFER_BIT,
		0, 0, NULL, 0, NULL, 1, &barrier );

	Com_Memset( &region, 0, sizeof( region ) );
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.layerCount = vk.xr.fdmLayers;
	region.imageExtent.width = vk.xr.foveationWidth;
	region.imageExtent.height = vk.xr.foveationHeight;
	region.imageExtent.depth = 1;
	qvkCmdCopyBufferToImage( vk.cmd->command_buffer, vk.xr.fdmStaging[index], vk.xr.fdmImage[index],
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region );

	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT;
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_FRAGMENT_DENSITY_MAP_READ_BIT_EXT;
	qvkCmdPipelineBarrier( vk.cmd->command_buffer,
		VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_DENSITY_PROCESS_BIT_EXT,
		0, 0, NULL, 0, NULL, 1, &barrier );

	vk.xr.fdmUploaded[index] = qtrue;
	vk.xr.fdmAppliedLevel[index] = level;
	vk.xr.fdmAppliedEyeTracked[index] = eyeTracked;
	{
		uint32_t eye;
		for ( eye = 0; eye < 2; eye++ ) {
			vk_fdm_gaze_texel( vk.xr.fdmCenter[eye], vk.xr.foveationWidth, vk.xr.foveationHeight,
				&vk.xr.fdmAppliedOffset[index][eye][0], &vk.xr.fdmAppliedOffset[index][eye][1] );
		}
	}
}

/*
==================
vk_set_foveation

Only recorded; the map is rewritten when the frame that needs it opens.
==================
*/
void vk_set_foveation( int level, qboolean eyeTracked, const float centers[2][2], const float fovTan[2][4] )
{
	vk.xr.fdmLevel = level;
	vk.xr.fdmEyeTracked = eyeTracked;
	if ( centers != NULL ) {
		Com_Memcpy( vk.xr.fdmCenter, centers, sizeof( vk.xr.fdmCenter ) );
	}
	if ( fovTan != NULL ) {
		Com_Memcpy( vk.xr.fdmFovTan, fovTan, sizeof( vk.xr.fdmFovTan ) );
	}
}

/*
==================
vk_foveation_block_at

Fragment edge in pixels the map asks for at a normalized device position. The device
rounds density to a fragment area no larger than 1/density, so the largest power of two
that fits. Reads the staging copy, which holds what the frame's map holds; the runtime's
own map cannot be read here and counts as its coarsest.
==================
*/
int vk_foveation_block_at( int eye, float ndcX, float ndcY )
{
	const byte *map;
	float fx, fy, density;
	uint32_t tx, ty, layer;
	int block, area;

	if ( !vk.xr.foveationActive || vk.xr.fdmLevel <= 0 ) {
		return 1;
	}
	if ( !vk.xr.fdmAuthored ) {
		return 8;
	}
	if ( vk.xr.colorIndex >= MAX_SWAPCHAIN_IMAGES || vk.xr.fdmStagingMapped[vk.xr.colorIndex] == NULL ||
		vk.xr.fdmTexelWidth == 0 || vk.xr.fdmTexelHeight == 0 ||
		vk.xr.foveationWidth == 0 || vk.xr.foveationHeight == 0 ) {
		return 8;
	}

	// Map texel under the position; the map runs the same way as the image
	fx = ( ndcX * 0.5f + 0.5f ) * (float)vk.renderWidth / (float)vk.xr.fdmTexelWidth;
	fy = ( ndcY * 0.5f + 0.5f ) * (float)vk.renderHeight / (float)vk.xr.fdmTexelHeight;
	if ( fx < 0.0f ) fx = 0.0f;
	if ( fy < 0.0f ) fy = 0.0f;
	tx = (uint32_t)fx;
	ty = (uint32_t)fy;
	if ( tx >= vk.xr.foveationWidth ) tx = vk.xr.foveationWidth - 1;
	if ( ty >= vk.xr.foveationHeight ) ty = vk.xr.foveationHeight - 1;
	layer = ( eye > 0 && vk.xr.fdmLayers > 1 ) ? 1 : 0;

	map = (const byte*)vk.xr.fdmStagingMapped[vk.xr.colorIndex];
	density = (float)map[ ( ( (size_t)layer * vk.xr.foveationHeight + ty ) * vk.xr.foveationWidth + tx ) * 2 ] / 255.0f;
	if ( density >= 0.999f ) {
		return 1;
	}
	if ( density < 1.0f / 16.0f ) {
		density = 1.0f / 16.0f;
	}

	area = (int)( 1.0f / density );
	block = 1;
	while ( block * 2 <= area && block < 16 ) {
		block *= 2;
	}
	return block;
}

/*
==================
vk_log_tile_size

The tiler applies a density map one bin at a time -- one sample per bin, held across
the whole bin -- so the bin is the map's real resolution.
Print it once per framebuffer set; it is the number that says how much of the falloff
survives, and whether the bin grid is a strip grid or something square.
==================
*/
static void vk_log_tile_size( VkFramebuffer framebuffer, const char *pass )
{
	VkTilePropertiesQCOM props;
	uint32_t count = 1;

	if ( qvkGetFramebufferTilePropertiesQCOM == NULL || framebuffer == VK_NULL_HANDLE ) {
		return;
	}

	Com_Memset( &props, 0, sizeof( props ) );
	props.sType = VK_STRUCTURE_TYPE_TILE_PROPERTIES_QCOM;
	if ( qvkGetFramebufferTilePropertiesQCOM( vk.device, framebuffer, &count, &props ) < VK_SUCCESS || count == 0 ) {
		ri.Printf( PRINT_WARNING, "Tile size for the %s pass: query failed\n", pass );
		return;
	}

	vk.xr.tileWidth = props.tileSize.width;
	vk.xr.tileHeight = props.tileSize.height;

	// Bins per eye buffer, and how many map texels fall inside one bin, since that is what gets thrown away
	ri.Printf( PRINT_ALL, "Tile size for the %s pass: %ux%u (%ux%u bins over %ux%u, %ux%u map texels a bin)\n",
		pass, props.tileSize.width, props.tileSize.height,
		props.tileSize.width ? ( vk.xr.width + props.tileSize.width - 1 ) / props.tileSize.width : 0,
		props.tileSize.height ? ( vk.xr.height + props.tileSize.height - 1 ) / props.tileSize.height : 0,
		vk.xr.width, vk.xr.height,
		vk.xr.fdmTexelWidth ? props.tileSize.width / vk.xr.fdmTexelWidth : 0,
		vk.xr.fdmTexelHeight ? props.tileSize.height / vk.xr.fdmTexelHeight : 0 );
}


/*
 * vk_create_xr_image_views - Create VkImageViews for XR swapchain images
 *
 * Creates multiview array views for the color and depth swapchains,
 * and per-eye views for desktop mirror blitting.
 *
 * This function is called after the VR layer creates XR swapchains
 * and the renderer creates render passes.
 */
qboolean vk_create_xr_image_views( void )
{
	VkXrResources *xr = &vk.xr;

	if ( !xr->colorInfo || !xr->depthInfo ) {
		ri.Printf( PRINT_WARNING, "vk_create_xr_image_views: XR swapchain info not set\n" );
		return qfalse;
	}

	ri.Printf( PRINT_ALL, "Creating XR image views...\n" );

	// Store XR resolution
	xr->width = xr->colorInfo->width;
	xr->height = xr->colorInfo->height;

	// Create color views (multiview array)
	for ( uint32_t i = 0; i < xr->colorInfo->imageCount && i < MAX_SWAPCHAIN_IMAGES; i++ ) {
		VkImageViewCreateInfo viewInfo = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.pNext = NULL,
			.flags = 0,
			.image = xr->colorInfo->images[i],
			.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY,
			.format = xr->colorInfo->format,
			.components = {
				.r = VK_COMPONENT_SWIZZLE_IDENTITY,
				.g = VK_COMPONENT_SWIZZLE_IDENTITY,
				.b = VK_COMPONENT_SWIZZLE_IDENTITY,
				.a = VK_COMPONENT_SWIZZLE_IDENTITY,
			},
			.subresourceRange = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = xr->colorInfo->arraySize,
			},
		};

		VK_CHECK( qvkCreateImageView( vk.device, &viewInfo, NULL, &xr->colorViews[i] ) );

		// Create UNORM view for gamma pass (no automatic sRGB conversion)
		// The gamma shader outputs sRGB-encoded values directly, so we need to
		// bypass Vulkan's automatic linear-to-sRGB conversion on write
		{
			VkImageViewCreateInfo gammaViewInfo = viewInfo;
			gammaViewInfo.format = vk_get_unorm_format( xr->colorInfo->format );
			VK_CHECK( qvkCreateImageView( vk.device, &gammaViewInfo, NULL, &xr->gammaViews[i] ) );
		}
	}

	// Create depth views (multiview array)
	for ( uint32_t i = 0; i < xr->depthInfo->imageCount && i < MAX_SWAPCHAIN_IMAGES; i++ ) {
		VkImageViewCreateInfo viewInfo = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.pNext = NULL,
			.flags = 0,
			.image = xr->depthInfo->images[i],
			.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY,
			.format = xr->depthInfo->format,
			.components = {
				.r = VK_COMPONENT_SWIZZLE_IDENTITY,
				.g = VK_COMPONENT_SWIZZLE_IDENTITY,
				.b = VK_COMPONENT_SWIZZLE_IDENTITY,
				.a = VK_COMPONENT_SWIZZLE_IDENTITY,
			},
			.subresourceRange = {
				.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = xr->depthInfo->arraySize,
			},
		};

		VK_CHECK( qvkCreateImageView( vk.device, &viewInfo, NULL, &xr->depthViews[i] ) );
	}

	// Density map views. VK_REMAINING_ARRAY_LAYERS covers both ours (a layer per eye) and the runtime's
	if ( xr->foveationActive ) {
		for ( uint32_t i = 0; i < xr->colorInfo->imageCount && i < MAX_SWAPCHAIN_IMAGES; i++ ) {
			VkImageViewCreateInfo viewInfo = {
				.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
				.pNext = NULL,
				.flags = 0,
				.image = xr->fdmAuthored ? xr->fdmImage[i] : xr->colorInfo->foveationImages[i],
				.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY,
				.format = VK_FDM_FORMAT,
				.components = {
					.r = VK_COMPONENT_SWIZZLE_IDENTITY,
					.g = VK_COMPONENT_SWIZZLE_IDENTITY,
					.b = VK_COMPONENT_SWIZZLE_IDENTITY,
					.a = VK_COMPONENT_SWIZZLE_IDENTITY,
				},
				.subresourceRange = {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.baseMipLevel = 0,
					.levelCount = 1,
					.baseArrayLayer = 0,
					.layerCount = VK_REMAINING_ARRAY_LAYERS,
				},
			};

			VK_CHECK( qvkCreateImageView( vk.device, &viewInfo, NULL, &xr->foveationViews[i] ) );
			SET_OBJECT_NAME( xr->foveationViews[i], va( "XR fragment density map view %d", i ),
				VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
		}
	}

	ri.Printf( PRINT_ALL, "XR image views created: color=%u, depth=%u, density map=%u\n",
		xr->colorInfo->imageCount, xr->depthInfo->imageCount,
		xr->foveationActive ? xr->colorInfo->imageCount : 0 );

	return qtrue;
}

/*
 * vk_destroy_xr_image_views - Destroy VkImageViews for XR swapchain images
 */
void vk_destroy_xr_image_views( void )
{
	VkXrResources *xr = &vk.xr;

	// Destroy color views
	for ( uint32_t i = 0; i < MAX_SWAPCHAIN_IMAGES; i++ ) {
		if ( xr->colorViews[i] != VK_NULL_HANDLE ) {
			qvkDestroyImageView( vk.device, xr->colorViews[i], NULL );
			xr->colorViews[i] = VK_NULL_HANDLE;
		}
		if ( xr->gammaViews[i] != VK_NULL_HANDLE ) {
			qvkDestroyImageView( vk.device, xr->gammaViews[i], NULL );
			xr->gammaViews[i] = VK_NULL_HANDLE;
		}
	}

	// Destroy depth views
	for ( uint32_t i = 0; i < MAX_SWAPCHAIN_IMAGES; i++ ) {
		if ( xr->depthViews[i] != VK_NULL_HANDLE ) {
			qvkDestroyImageView( vk.device, xr->depthViews[i], NULL );
			xr->depthViews[i] = VK_NULL_HANDLE;
		}
	}

	for ( uint32_t i = 0; i < MAX_SWAPCHAIN_IMAGES; i++ ) {
		if ( xr->foveationViews[i] != VK_NULL_HANDLE ) {
			qvkDestroyImageView( vk.device, xr->foveationViews[i], NULL );
			xr->foveationViews[i] = VK_NULL_HANDLE;
		}
	}
}

/*
 * vk_create_xr_framebuffers - Create VkFramebuffers for XR swapchain images
 *
 * When r_fbo is active (subpass optimization):
 *   - Main rendering + post-processing in combined subpass pass
 *   - Uses vk.framebuffers.main_with_bloom[] or main_with_gamma[]
 *   - Output goes directly to XR swapchain in final subpass
 *
 * When r_fbo is NOT active (direct mode):
 *   - Main rendering goes directly to XR swapchain via xr->framebuffers[]
 *   - xr->framebuffers[] use vk.render_pass.main (recreated with XR format)
 */
qboolean vk_create_xr_framebuffers( void )
{
	VkXrResources *xr = &vk.xr;
	VkFramebufferCreateInfo fbInfo;
	VkImageView attachments[4];  // color, depth, MSAA color, density map
	uint32_t i;

	// FBO mode: main XR framebuffers not needed (gamma framebuffers are used instead)
	// Only direct mode needs these framebuffers for main rendering to XR swapchain
	if ( !vk.fboActive ) {
		// Validate prerequisites for direct mode
		if ( !vk.multiviewSupported ) {
			ri.Printf( PRINT_WARNING, "vk_create_xr_framebuffers: multiview not supported\n" );
			return qfalse;
		}

		if ( vk.render_pass.main == VK_NULL_HANDLE ) {
			ri.Printf( PRINT_WARNING, "vk_create_xr_framebuffers: main render pass not created (ptr=%p, multiview=%d)\n",
				(void*)vk.render_pass.main, vk.multiviewSupported );
			return qfalse;
		}

		if ( xr->colorInfo == NULL || xr->depthInfo == NULL ) {
			ri.Printf( PRINT_WARNING, "vk_create_xr_framebuffers: swapchain info not set\n" );
			return qfalse;
		}

		// Direct mode: with MSAA the swapchain is the resolve target, color and depth are transient.
		ri.Printf( PRINT_ALL, "Creating XR framebuffers (direct mode%s)\n", vk.msaaActive ? ", MSAA" : "" );

		// Create XR swapchain framebuffers
		for ( i = 0; i < xr->colorInfo->imageCount && i < MAX_SWAPCHAIN_IMAGES; i++ ) {
			// Color and depth image views should already be created
			if ( xr->colorViews[i] == VK_NULL_HANDLE || xr->depthViews[i] == VK_NULL_HANDLE ) {
				ri.Printf( PRINT_WARNING, "vk_create_xr_framebuffers: image view %d not created\n", i );
				return qfalse;
			}

			VkImageView swapchainView;
			uint32_t attachmentCount;

			// Use UNORM views to bypass automatic sRGB conversion (shader handles gamma)
			if ( xr->gammaViews[i] != VK_NULL_HANDLE ) {
				swapchainView = xr->gammaViews[i];  // UNORM view: no auto sRGB conversion
			} else {
				swapchainView = xr->colorViews[i];  // Fallback to sRGB view
			}

			Com_Memset( &fbInfo, 0, sizeof( fbInfo ) );
			fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			fbInfo.renderPass = vk.render_pass.main;
			fbInfo.pAttachments = attachments;
			fbInfo.width = xr->width;
			fbInfo.height = xr->height;
			// Multiview render pass: layers must be 1 (view mask handles stereo)
			fbInfo.layers = 1;

			attachments[0] = swapchainView;
			if ( vk.msaaActive ) {
				// [resolve target, transient MSAA depth, transient MSAA color]
				if ( vk.transient.depth_view == VK_NULL_HANDLE || vk.transient.msaa_view == VK_NULL_HANDLE ) {
					ri.Printf( PRINT_WARNING, "vk_create_xr_framebuffers: transient MSAA images not created\n" );
					return qfalse;
				}
				attachments[1] = vk.transient.depth_view;
				attachments[2] = vk.transient.msaa_view;
				attachmentCount = 3;
			} else {
				attachments[1] = xr->depthViews[i];  // 2-layer depth array
				attachmentCount = 2;
			}
			if ( xr->foveationActive ) {
				attachments[attachmentCount++] = xr->foveationViews[i];  // matches the render pass's last attachment
			}
			fbInfo.attachmentCount = attachmentCount;

			VK_CHECK( qvkCreateFramebuffer( vk.device, &fbInfo, NULL, &xr->framebuffers[i] ) );
			SET_OBJECT_NAME( xr->framebuffers[i], va( "XR framebuffer %d (direct)", i ),
				VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
			if ( i == 0 ) {
				vk_log_tile_size( xr->framebuffers[i], "direct scene" );
			}
		}

		ri.Printf( PRINT_ALL, "...XR swapchain framebuffers created (%d, direct mode)\n",
			xr->colorInfo->imageCount );
	} else {
		ri.Printf( PRINT_ALL, "Skipping XR main framebuffers (FBO mode - using gamma framebuffers)\n" );
	}

	return qtrue;
}

/*
 * vk_destroy_xr_framebuffers - Destroy VkFramebuffers for XR swapchain images
 */
void vk_destroy_xr_framebuffers( void )
{
	VkXrResources *xr = &vk.xr;

	// Destroy multiview framebuffers
	for ( uint32_t i = 0; i < MAX_SWAPCHAIN_IMAGES; i++ ) {
		if ( xr->framebuffers[i] != VK_NULL_HANDLE ) {
			qvkDestroyFramebuffer( vk.device, xr->framebuffers[i], NULL );
			xr->framebuffers[i] = VK_NULL_HANDLE;
		}
	}
}

/*
 * vk_create_hud_buffer - Create HUD buffer resources (1280x960 color + depth)
 *
 * Creates the VkImage, VkImageView, VkFramebuffer, VkDescriptorSet, and
 * tr.hudImage for the HUD buffer used in HUD mode 1 (in-world sprite rendering).
 * Called from R_InitImages() before CreateExternalShaders() sets up tr.hudShader.
 * Includes depth buffer for proper 3D model rendering (e.g., character heads with ponytails).
 */
qboolean vk_create_hud_buffer( void )
{
	VkXrResources *xr = &vk.xr;
	VkImageCreateInfo imageCI;
	VkMemoryRequirements colorMemReqs, depthMemReqs;
	VkMemoryAllocateInfo allocInfo;
	VkImageViewCreateInfo viewCI;
	VkFramebufferCreateInfo fbCI;
	VkImageView attachments[2];  // Color + depth
	uint32_t memoryType;

	// Ensure HUD render pass exists (created by vk_create_render_passes during vk_initialize)
	if ( vk.render_pass.hudBuffer == VK_NULL_HANDLE ) {
		ri.Printf( PRINT_WARNING, "vk_create_hud_buffer: HUD render pass not created yet\n" );
		return qfalse;
	}

	// Clean up any existing HUD buffer resources first (happens on map reload with REF_KEEP_CONTEXT)
	vk_destroy_hud_buffer();

	ri.Printf( PRINT_ALL, "Creating HUD buffer (%dx%d, color_format=0x%x, depth_format=0x%x)...\n",
		HUD_BUFFER_WIDTH, HUD_BUFFER_HEIGHT, vk.color_format, vk.depth_format );

	// 1. Create HUD color image (1280x960, RGBA8, sampled + color attachment + transfer dest for clears)
	Com_Memset( &imageCI, 0, sizeof( imageCI ) );
	imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCI.imageType = VK_IMAGE_TYPE_2D;
	imageCI.format = vk.color_format;
	imageCI.extent.width = HUD_BUFFER_WIDTH;
	imageCI.extent.height = HUD_BUFFER_HEIGHT;
	imageCI.extent.depth = 1;
	imageCI.mipLevels = 1;
	imageCI.arrayLayers = 1;
	imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageCI.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	imageCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VK_CHECK( qvkCreateImage( vk.device, &imageCI, NULL, &xr->hudImage ) );
	SET_OBJECT_NAME( xr->hudImage, "HUD color image", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );

	// 2. Allocate memory for color image
	qvkGetImageMemoryRequirements( vk.device, xr->hudImage, &colorMemReqs );

	memoryType = find_memory_type(
		colorMemReqs.memoryTypeBits,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );

	Com_Memset( &allocInfo, 0, sizeof( allocInfo ) );
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = colorMemReqs.size;
	allocInfo.memoryTypeIndex = memoryType;

	VK_CHECK( qvkAllocateMemory( vk.device, &allocInfo, NULL, &xr->hudMemory ) );
	VK_CHECK( qvkBindImageMemory( vk.device, xr->hudImage, xr->hudMemory, 0 ) );

	// 3. Create color image views (2D_ARRAY for framebuffer, 2D for shader sampling)
	Com_Memset( &viewCI, 0, sizeof( viewCI ) );
	viewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewCI.image = xr->hudImage;
	viewCI.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
	viewCI.format = vk.color_format;
	viewCI.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewCI.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewCI.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewCI.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	viewCI.subresourceRange.baseMipLevel = 0;
	viewCI.subresourceRange.levelCount = 1;
	viewCI.subresourceRange.baseArrayLayer = 0;
	viewCI.subresourceRange.layerCount = 1;

	VK_CHECK( qvkCreateImageView( vk.device, &viewCI, NULL, &xr->hudView ) );
	SET_OBJECT_NAME( xr->hudView, "HUD color image view (2D_ARRAY for framebuffer)", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );

	// Create 2D view for shader sampling
	viewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
	VK_CHECK( qvkCreateImageView( vk.device, &viewCI, NULL, &xr->hudSamplerView ) );
	SET_OBJECT_NAME( xr->hudSamplerView, "HUD color image view (2D for sampling)", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );

	// 4. Create HUD depth image for proper 3D model rendering
	Com_Memset( &imageCI, 0, sizeof( imageCI ) );
	imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCI.imageType = VK_IMAGE_TYPE_2D;
	imageCI.format = vk.depth_format;
	imageCI.extent.width = HUD_BUFFER_WIDTH;
	imageCI.extent.height = HUD_BUFFER_HEIGHT;
	imageCI.extent.depth = 1;
	imageCI.mipLevels = 1;
	imageCI.arrayLayers = 1;
	imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageCI.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	imageCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VK_CHECK( qvkCreateImage( vk.device, &imageCI, NULL, &xr->hudDepthImage ) );
	SET_OBJECT_NAME( xr->hudDepthImage, "HUD depth image", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );

	// 5. Allocate memory for depth image
	qvkGetImageMemoryRequirements( vk.device, xr->hudDepthImage, &depthMemReqs );

	memoryType = find_memory_type(
		depthMemReqs.memoryTypeBits,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );

	Com_Memset( &allocInfo, 0, sizeof( allocInfo ) );
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = depthMemReqs.size;
	allocInfo.memoryTypeIndex = memoryType;

	VK_CHECK( qvkAllocateMemory( vk.device, &allocInfo, NULL, &xr->hudDepthMemory ) );
	VK_CHECK( qvkBindImageMemory( vk.device, xr->hudDepthImage, xr->hudDepthMemory, 0 ) );

	// 6. Create depth image view (2D_ARRAY for multiview render pass compatibility)
	Com_Memset( &viewCI, 0, sizeof( viewCI ) );
	viewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewCI.image = xr->hudDepthImage;
	viewCI.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
	viewCI.format = vk.depth_format;
	viewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	viewCI.subresourceRange.baseMipLevel = 0;
	viewCI.subresourceRange.levelCount = 1;
	viewCI.subresourceRange.baseArrayLayer = 0;
	viewCI.subresourceRange.layerCount = 1;

	VK_CHECK( qvkCreateImageView( vk.device, &viewCI, NULL, &xr->hudDepthView ) );
	SET_OBJECT_NAME( xr->hudDepthView, "HUD depth image view", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );

	// 7. Create framebuffer with color + depth attachments
	attachments[0] = xr->hudView;
	attachments[1] = xr->hudDepthView;

	Com_Memset( &fbCI, 0, sizeof( fbCI ) );
	fbCI.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	fbCI.renderPass = vk.render_pass.hudBuffer;
	fbCI.attachmentCount = 2;  // Color + depth
	fbCI.pAttachments = attachments;
	fbCI.width = HUD_BUFFER_WIDTH;
	fbCI.height = HUD_BUFFER_HEIGHT;
	fbCI.layers = 1;

	VK_CHECK( qvkCreateFramebuffer( vk.device, &fbCI, NULL, &xr->hudFramebuffer ) );
	SET_OBJECT_NAME( xr->hudFramebuffer, "HUD framebuffer", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );

	// 7. Create descriptor set for sampling HUD texture
	{
		VkDescriptorSetAllocateInfo descAllocInfo;
		VkDescriptorImageInfo imageInfo;
		VkWriteDescriptorSet descriptorWrite;
		Vk_Sampler_Def samplerDef;

		Com_Memset( &descAllocInfo, 0, sizeof( descAllocInfo ) );
		descAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		descAllocInfo.descriptorPool = vk.descriptor_pool;
		descAllocInfo.descriptorSetCount = 1;
		descAllocInfo.pSetLayouts = &vk.set_layout_sampler;

		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &descAllocInfo, &xr->hudDescriptor ) );

		// Set up sampler for HUD texture: linear filtering, clamp to edge, no mipmaps
		Com_Memset( &samplerDef, 0, sizeof( samplerDef ) );
		samplerDef.gl_mag_filter = GL_LINEAR;
		samplerDef.gl_min_filter = GL_LINEAR;
		samplerDef.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerDef.max_lod_1_0 = qtrue;  // No mipmaps
		samplerDef.noAnisotropy = qtrue;

		// Update descriptor to point to HUD color image (use 2D view for shader sampling)
		Com_Memset( &imageInfo, 0, sizeof( imageInfo ) );
		imageInfo.sampler = vk_find_sampler( &samplerDef );
		imageInfo.imageView = xr->hudSamplerView;  // Use 2D view, not 2D_ARRAY
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		Com_Memset( &descriptorWrite, 0, sizeof( descriptorWrite ) );
		descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite.dstSet = xr->hudDescriptor;
		descriptorWrite.dstBinding = 0;
		descriptorWrite.dstArrayElement = 0;
		descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrite.descriptorCount = 1;
		descriptorWrite.pImageInfo = &imageInfo;

		ri.Printf( PRINT_ALL, "HUD descriptor update: descriptor=%p view=%p sampler=%p\n",
			(void*)(uintptr_t)xr->hudDescriptor, (void*)(uintptr_t)xr->hudSamplerView,
			(void*)(uintptr_t)imageInfo.sampler );

		qvkUpdateDescriptorSets( vk.device, 1, &descriptorWrite, 0, NULL );
		ri.Printf( PRINT_ALL, "HUD descriptor updated successfully\n" );
	}

	// 8. Initialize HUD color and depth images
	// Color: clear to transparent black and transition to SHADER_READ_ONLY_OPTIMAL
	// Depth: clear to 0.0 (USE_REVERSED_DEPTH); render pass uses UNDEFINED initialLayout with LOAD_OP_CLEAR
	{
		VkCommandBuffer cmdBuf;
		VkCommandBufferAllocateInfo cmdAllocInfo;
		VkCommandBufferBeginInfo beginInfo;
		VkSubmitInfo submitInfo;
		VkClearColorValue clearColor;
		VkClearDepthStencilValue clearDepth;
		VkImageSubresourceRange range;

		Com_Memset( &cmdAllocInfo, 0, sizeof( cmdAllocInfo ) );
		cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cmdAllocInfo.commandPool = vk.command_pool;
		cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cmdAllocInfo.commandBufferCount = 1;

		VK_CHECK( qvkAllocateCommandBuffers( vk.device, &cmdAllocInfo, &cmdBuf ) );

		Com_Memset( &beginInfo, 0, sizeof( beginInfo ) );
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		VK_CHECK( qvkBeginCommandBuffer( cmdBuf, &beginInfo ) );

		// Initialize color image: transition to TRANSFER_DST and clear
		record_image_layout_transition( cmdBuf, xr->hudImage, VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 0 );

		clearColor.float32[0] = 0.0f;
		clearColor.float32[1] = 0.0f;
		clearColor.float32[2] = 0.0f;
		clearColor.float32[3] = 0.0f;

		range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		range.baseMipLevel = 0;
		range.levelCount = 1;
		range.baseArrayLayer = 0;
		range.layerCount = 1;

		qvkCmdClearColorImage( cmdBuf, xr->hudImage,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &range );

		// Transition color to SHADER_READ_ONLY_OPTIMAL for sampling
		record_image_layout_transition( cmdBuf, xr->hudImage, VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0 );

		// Initialize depth image: transition to TRANSFER_DST and clear to 0.0 (reversed depth)
		record_image_layout_transition( cmdBuf, xr->hudDepthImage, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 0 );

		clearDepth.depth = 0.0f;  // USE_REVERSED_DEPTH: 0.0 = farthest
		clearDepth.stencil = 0;

		range.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;

		qvkCmdClearDepthStencilImage( cmdBuf, xr->hudDepthImage,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearDepth, 1, &range );

		// Transition depth to DEPTH_STENCIL_ATTACHMENT_OPTIMAL
		// The render pass requires initialLayout=DEPTH_STENCIL_ATTACHMENT_OPTIMAL because
		// the HUD pass may run multiple times per frame (cgame + console notify), and
		// subsequent passes need the image in the correct layout
		record_image_layout_transition( cmdBuf, xr->hudDepthImage, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 0, 0 );

		VK_CHECK( qvkEndCommandBuffer( cmdBuf ) );

		Com_Memset( &submitInfo, 0, sizeof( submitInfo ) );
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &cmdBuf;

		ri.Printf( PRINT_ALL, "Submitting HUD init command buffer...\n" );
		VK_CHECK( qvkQueueSubmit( vk.queue, 1, &submitInfo, VK_NULL_HANDLE ) );
		ri.Printf( PRINT_ALL, "Waiting for HUD init to complete...\n" );
		VK_CHECK( qvkQueueWaitIdle( vk.queue ) );
		ri.Printf( PRINT_ALL, "HUD init complete\n" );

		qvkFreeCommandBuffers( vk.device, vk.command_pool, 1, &cmdBuf );
	}

	// 9. Create tr.hudImage wrapper for shader system
	// Pass the 2D sampler view since that's what shaders will sample
	tr.hudImage = R_CreateHUDImage( xr->hudImage, xr->hudSamplerView, xr->hudDescriptor,
	                                HUD_BUFFER_WIDTH, HUD_BUFFER_HEIGHT );
	ri.Printf( PRINT_ALL, "Created tr.hudImage wrapper: %p (descriptor=%p)\n",
		(void*)tr.hudImage, (void*)(uintptr_t)tr.hudImage->descriptor );

	ri.Printf( PRINT_ALL, "...HUD buffer created\n" );
	return qtrue;
}

/*
 * vk_destroy_hud_buffer - Destroy HUD buffer resources
 */
static void vk_destroy_hud_buffer( void )
{
	VkXrResources *xr = &vk.xr;

	if ( xr->hudFramebuffer != VK_NULL_HANDLE ) {
		qvkDestroyFramebuffer( vk.device, xr->hudFramebuffer, NULL );
		xr->hudFramebuffer = VK_NULL_HANDLE;
	}
	if ( xr->hudView != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, xr->hudView, NULL );
		xr->hudView = VK_NULL_HANDLE;
	}
	if ( xr->hudSamplerView != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, xr->hudSamplerView, NULL );
		xr->hudSamplerView = VK_NULL_HANDLE;
	}
	if ( xr->hudImage != VK_NULL_HANDLE ) {
		qvkDestroyImage( vk.device, xr->hudImage, NULL );
		xr->hudImage = VK_NULL_HANDLE;
	}
	if ( xr->hudMemory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, xr->hudMemory, NULL );
		xr->hudMemory = VK_NULL_HANDLE;
	}
	// Depth buffer resources
	if ( xr->hudDepthView != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, xr->hudDepthView, NULL );
		xr->hudDepthView = VK_NULL_HANDLE;
	}
	if ( xr->hudDepthImage != VK_NULL_HANDLE ) {
		qvkDestroyImage( vk.device, xr->hudDepthImage, NULL );
		xr->hudDepthImage = VK_NULL_HANDLE;
	}
	if ( xr->hudDepthMemory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, xr->hudDepthMemory, NULL );
		xr->hudDepthMemory = VK_NULL_HANDLE;
	}
	// Descriptor is freed when pool is reset
	xr->hudDescriptor = VK_NULL_HANDLE;
}


/*
 * vk_destroy_gamma_framebuffers - Destroy gamma framebuffers (XR swapchain outputs)
 */
static void vk_destroy_gamma_framebuffers( void )
{
	uint32_t i;
	for ( i = 0; i < MAX_SWAPCHAIN_IMAGES; i++ ) {
		if ( vk.framebuffers.gamma[i] != VK_NULL_HANDLE ) {
			qvkDestroyFramebuffer( vk.device, vk.framebuffers.gamma[i], NULL );
			vk.framebuffers.gamma[i] = VK_NULL_HANDLE;
		}
	}
}

/*
 * vk_destroy_subpass_framebuffers - Destroy subpass optimization framebuffers
 */
static void vk_destroy_subpass_framebuffers( void )
{
	uint32_t i;
	for ( i = 0; i < MAX_SWAPCHAIN_IMAGES; i++ ) {
		if ( vk.framebuffers.fov_scene[i] != VK_NULL_HANDLE ) {
			qvkDestroyFramebuffer( vk.device, vk.framebuffers.fov_scene[i], NULL );
			vk.framebuffers.fov_scene[i] = VK_NULL_HANDLE;
		}
		if ( vk.framebuffers.main_with_bloom[i] != VK_NULL_HANDLE ) {
			qvkDestroyFramebuffer( vk.device, vk.framebuffers.main_with_bloom[i], NULL );
			vk.framebuffers.main_with_bloom[i] = VK_NULL_HANDLE;
		}
		if ( vk.framebuffers.main_with_gamma[i] != VK_NULL_HANDLE ) {
			qvkDestroyFramebuffer( vk.device, vk.framebuffers.main_with_gamma[i], NULL );
			vk.framebuffers.main_with_gamma[i] = VK_NULL_HANDLE;
		}
	}
}

/*
 * vk_create_subpass_framebuffers - Create the scene and post pass framebuffers
 *
 * fov_scene: [(msaa,) scene, depth] plus the density map; the post passes take [swapchain] and sample the scene
 */
static qboolean vk_create_subpass_framebuffers( void )
{
	VkXrResources *xr = &vk.xr;
	VkFramebufferCreateInfo fbCI;
	VkImageView attachments[6];  // Max 5 for MSAA bloom path, plus the density map when foveated
	uint32_t i, attachmentCount;
	qboolean useBloom;
	VkRenderPass renderPass;

	if ( !vk.multiviewSupported ) {
		ri.Printf( PRINT_WARNING, "vk_create_subpass_framebuffers: multiview not supported\n" );
		return qfalse;
	}

	useBloom = ( r_bloom && r_bloom->integer );

	if ( useBloom ) {
		if ( vk.render_pass.main_with_bloom == VK_NULL_HANDLE ) {
			ri.Printf( PRINT_WARNING, "vk_create_subpass_framebuffers: main_with_bloom render pass not created\n" );
			return qfalse;
		}
		renderPass = vk.render_pass.main_with_bloom;
	} else {
		if ( vk.render_pass.main_with_gamma == VK_NULL_HANDLE ) {
			ri.Printf( PRINT_WARNING, "vk_create_subpass_framebuffers: main_with_gamma render pass not created\n" );
			return qfalse;
		}
		renderPass = vk.render_pass.main_with_gamma;
	}

	if ( xr->colorInfo == NULL ) {
		ri.Printf( PRINT_WARNING, "vk_create_subpass_framebuffers: XR swapchain not ready\n" );
		return qfalse;
	}

	ri.Printf( PRINT_ALL, "Creating subpass framebuffers (%s, %s)...\n",
		useBloom ? "bloom" : "gamma-only",
		vk.msaaActive ? "MSAA" : "non-MSAA" );

	for ( i = 0; i < xr->colorInfo->imageCount && i < MAX_SWAPCHAIN_IMAGES; i++ ) {
		// Get swapchain view (UNORM for gamma)
		VkImageView swapchainView = xr->gammaViews[i] != VK_NULL_HANDLE ?
			xr->gammaViews[i] : xr->colorViews[i];

		if ( swapchainView == VK_NULL_HANDLE ) {
			ri.Printf( PRINT_WARNING, "vk_create_subpass_framebuffers: swapchain view %d not ready\n", i );
			continue;
		}

		{
			// Scene pass [(msaa,) scene, depth, density map], post pass [swapchain]
			VkImageView sceneView = vk.msaaActive ? vk.transient.resolve_view : vk.transient.scene_view;
			VkImageView sceneAttachments[4];
			uint32_t sceneCount = 0;

			if ( vk.msaaActive ) {
				sceneAttachments[sceneCount++] = vk.transient.msaa_view;
			}
			sceneAttachments[sceneCount++] = sceneView;
			sceneAttachments[sceneCount++] = vk.transient.depth_view;
			if ( xr->foveationActive ) {
				sceneAttachments[sceneCount++] = xr->foveationViews[i];
			}

			Com_Memset( &fbCI, 0, sizeof( fbCI ) );
			fbCI.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			fbCI.renderPass = vk.render_pass.fov_scene;
			fbCI.attachmentCount = sceneCount;
			fbCI.pAttachments = sceneAttachments;
			fbCI.width = xr->width;
			fbCI.height = xr->height;
			fbCI.layers = 1;
			VK_CHECK( qvkCreateFramebuffer( vk.device, &fbCI, NULL, &vk.framebuffers.fov_scene[i] ) );
			SET_OBJECT_NAME( vk.framebuffers.fov_scene[i], va( "foveated scene framebuffer %d", i ), VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
			if ( i == 0 ) {
				vk_log_tile_size( vk.framebuffers.fov_scene[i], "foveated scene" );
			}

			// Post pass: the scene is sampled, not attached, and the first blur pass does the extract
			attachmentCount = 0;
			attachments[attachmentCount++] = swapchainView;
		}

		Com_Memset( &fbCI, 0, sizeof( fbCI ) );
		fbCI.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		fbCI.renderPass = renderPass;
		fbCI.attachmentCount = attachmentCount;
		fbCI.pAttachments = attachments;
		fbCI.width = xr->width;
		fbCI.height = xr->height;
		fbCI.layers = 1;  // Multiview handles stereo

		if ( useBloom ) {
			VK_CHECK( qvkCreateFramebuffer( vk.device, &fbCI, NULL, &vk.framebuffers.main_with_bloom[i] ) );
			SET_OBJECT_NAME( vk.framebuffers.main_with_bloom[i],
				va( "subpass framebuffer %d (bloom)", i ), VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
		} else {
			VK_CHECK( qvkCreateFramebuffer( vk.device, &fbCI, NULL, &vk.framebuffers.main_with_gamma[i] ) );
			SET_OBJECT_NAME( vk.framebuffers.main_with_gamma[i],
				va( "subpass framebuffer %d (gamma)", i ), VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT );
		}
	}

	ri.Printf( PRINT_ALL, "...subpass framebuffers created (%d)\n", xr->colorInfo->imageCount );
	return qtrue;
}


// Legacy vk_create_xr_fbo_descriptors() removed: subpass mode uses input attachments
// for scene color instead of vk.color_descriptor

/*
 * vk_create_xr_bloom_descriptors - Create descriptors for bloom chain only
 *
 * Used by subpass optimization path which doesn't need vk.color_descriptor
 * (scene color is read via input attachment, not sampler)
 */
static qboolean vk_create_xr_bloom_descriptors( void )
{
	VkDescriptorSetAllocateInfo allocInfo;
	VkDescriptorImageInfo imageInfo;
	VkWriteDescriptorSet writeSet;
	Vk_Sampler_Def samplerDef;
	uint32_t i;

	if ( !vk.multiviewSupported ) {
		return qtrue;
	}

	ri.Printf( PRINT_ALL, "Creating bloom chain descriptors for subpass mode...\n" );

	// Set up sampler for post-processing: linear filtering, clamp to edge, no mipmaps
	Com_Memset( &samplerDef, 0, sizeof( samplerDef ) );
	samplerDef.gl_mag_filter = GL_LINEAR;
	samplerDef.gl_min_filter = GL_LINEAR;
	samplerDef.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerDef.max_lod_1_0 = qtrue;  // No mipmaps
	samplerDef.noAnisotropy = qtrue;

	// Set up common allocation info
	Com_Memset( &allocInfo, 0, sizeof( allocInfo ) );
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = vk.descriptor_pool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &vk.set_layout_sampler;

	Com_Memset( &imageInfo, 0, sizeof( imageInfo ) );
	imageInfo.sampler = vk_find_sampler( &samplerDef );
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	Com_Memset( &writeSet, 0, sizeof( writeSet ) );
	writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeSet.dstBinding = 0;
	writeSet.descriptorCount = 1;
	writeSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writeSet.pImageInfo = &imageInfo;

	// Create descriptors for bloom chain
	for ( i = 0; i < ARRAY_LEN( vk.bloom_image_descriptor ); i++ ) {
		if ( vk.bloom_image_view[i] == VK_NULL_HANDLE ) {
			continue;
		}

		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &allocInfo, &vk.bloom_image_descriptor[i] ) );

		imageInfo.imageView = vk.bloom_image_view[i];
		writeSet.dstSet = vk.bloom_image_descriptor[i];

		qvkUpdateDescriptorSets( vk.device, 1, &writeSet, 0, NULL );
	}

	// Allocate combined 4-sampler descriptor for composite subpass
	{
		VkDescriptorSetAllocateInfo combAllocInfo;
		Com_Memset( &combAllocInfo, 0, sizeof( combAllocInfo ) );
		combAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		combAllocInfo.descriptorPool = vk.descriptor_pool;
		combAllocInfo.descriptorSetCount = 1;
		combAllocInfo.pSetLayouts = &vk.set_layout_4samplers;
		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &combAllocInfo, &vk.bloom_blur_combined_descriptor ) );
	}

	vk_update_bloom_blur_combined_descriptor();

	ri.Printf( PRINT_ALL, "...bloom chain descriptors created\n" );
	return qtrue;
}

/*
 * vk_reallocate_xr_fbo_descriptors - Reallocate FBO descriptors after pool reset
 *
 * Called from vk_release_resources() after resetting the descriptor pool.
 * The images, views, and samplers are still valid; only descriptor sets need reallocation.
 */
static qboolean vk_reallocate_xr_fbo_descriptors( void )
{
	VkDescriptorSetAllocateInfo allocInfo;
	VkDescriptorImageInfo imageInfo;
	VkWriteDescriptorSet writeSet;
	Vk_Sampler_Def samplerDef;
	uint32_t i;

	// Set up common allocation info
	Com_Memset( &allocInfo, 0, sizeof( allocInfo ) );
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = vk.descriptor_pool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &vk.set_layout_sampler;

	Com_Memset( &imageInfo, 0, sizeof( imageInfo ) );
	Com_Memset( &writeSet, 0, sizeof( writeSet ) );
	writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeSet.dstBinding = 0;
	writeSet.descriptorCount = 1;
	writeSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writeSet.pImageInfo = &imageInfo;

	// Reallocate FBO descriptors (gate matches the allocation gate in vk_init_descriptors)
	if ( vk.fboActive ) {
		// Set up sampler for post-processing
		Com_Memset( &samplerDef, 0, sizeof( samplerDef ) );
		samplerDef.gl_mag_filter = GL_LINEAR;
		samplerDef.gl_min_filter = GL_LINEAR;
		samplerDef.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerDef.max_lod_1_0 = qtrue;
		samplerDef.noAnisotropy = qtrue;

		// Reallocate bloom descriptors if bloom is active
		// Blur passes need these descriptors for bloom processing
		if ( r_bloom->integer ) {
			imageInfo.sampler = vk_find_sampler( &samplerDef );
			imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			for ( i = 0; i < ARRAY_LEN( vk.bloom_image_descriptor ); i++ ) {
				if ( vk.bloom_image_view[i] == VK_NULL_HANDLE ) {
					vk.bloom_image_descriptor[i] = VK_NULL_HANDLE;
					continue;
				}

				VK_CHECK( qvkAllocateDescriptorSets( vk.device, &allocInfo, &vk.bloom_image_descriptor[i] ) );

				imageInfo.imageView = vk.bloom_image_view[i];
				writeSet.dstSet = vk.bloom_image_descriptor[i];

				qvkUpdateDescriptorSets( vk.device, 1, &writeSet, 0, NULL );
			}

			// Reallocate combined blur descriptor
			{
				VkDescriptorSetAllocateInfo combAllocInfo;
				Com_Memset( &combAllocInfo, 0, sizeof( combAllocInfo ) );
				combAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
				combAllocInfo.descriptorPool = vk.descriptor_pool;
				combAllocInfo.descriptorSetCount = 1;
				combAllocInfo.pSetLayouts = &vk.set_layout_4samplers;
				VK_CHECK( qvkAllocateDescriptorSets( vk.device, &combAllocInfo, &vk.bloom_blur_combined_descriptor ) );
			}

			vk_update_bloom_blur_combined_descriptor();
		}
	}

	// The image and view survive the pool reset; only the descriptor set needs rebuilding.
	{
		VkImageView sceneView = vk.msaaActive ? vk.transient.resolve_view : vk.transient.scene_view;

		if ( sceneView != VK_NULL_HANDLE ) {
			VkDescriptorSetAllocateInfo sceneAllocInfo;
			VkDescriptorImageInfo sceneImageInfo;
			VkWriteDescriptorSet sceneWriteDesc;
			Vk_Sampler_Def samplerDef;

			Com_Memset( &samplerDef, 0, sizeof( samplerDef ) );
			samplerDef.gl_mag_filter = GL_LINEAR;
			samplerDef.gl_min_filter = GL_LINEAR;
			samplerDef.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			samplerDef.max_lod_1_0 = qtrue;
			samplerDef.noAnisotropy = qtrue;

			Com_Memset( &sceneAllocInfo, 0, sizeof( sceneAllocInfo ) );
			sceneAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			sceneAllocInfo.descriptorPool = vk.descriptor_pool;
			sceneAllocInfo.descriptorSetCount = 1;
			sceneAllocInfo.pSetLayouts = &vk.set_layout_sampler;
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &sceneAllocInfo, &vk.transient.scene_descriptor ) );

			sceneImageInfo.sampler = vk_find_sampler( &samplerDef );
			sceneImageInfo.imageView = sceneView;
			sceneImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			Com_Memset( &sceneWriteDesc, 0, sizeof( sceneWriteDesc ) );
			sceneWriteDesc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			sceneWriteDesc.dstSet = vk.transient.scene_descriptor;
			sceneWriteDesc.dstBinding = 0;
			sceneWriteDesc.descriptorCount = 1;
			sceneWriteDesc.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			sceneWriteDesc.pImageInfo = &sceneImageInfo;

			qvkUpdateDescriptorSets( vk.device, 1, &sceneWriteDesc, 0, NULL );

			SET_OBJECT_NAME( (uint64_t)vk.transient.scene_descriptor, "scene sampler descriptor", VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT );
		}
	}

	return qtrue;
}

// Static swapchain info storage: populated from VR layer pull
static VR_VK_SwapchainInfo s_colorSwapchainInfo;
static VR_VK_SwapchainInfo s_depthSwapchainInfo;

/*
 * vk_recreate_xr_render_pass - Recreate main render pass with correct XR formats
 *
 * The main render pass is initially created with vk.color_format (desktop format).
 * This function recreates it with the actual XR swapchain formats.
 *
 * When r_fbo 0 (direct mode): main render pass uses XR swapchain UNORM format
 * When r_fbo 1 (FBO mode): main render pass uses vk.color_format for FBO
 */
static qboolean vk_recreate_xr_render_pass( VkFormat colorFormat, VkFormat depthFormat )
{
	VkAttachmentDescription attachments[4];  // [0] = resolve/color, [1] = depth, [2] = MSAA color (if active), [3] = density map (if foveated)
	VkAttachmentReference colorRef, depthRef, colorResolveRef;
	VkSubpassDescription subpass;
	VkSubpassDependency deps[2];
	VkRenderPassCreateInfo desc;
	VkRenderPassMultiviewCreateInfo multiviewInfo;
	VkRenderPassFragmentDensityMapCreateInfoEXT fdmInfo;
	uint32_t viewMask = 0b11;
	uint32_t correlationMask = 0b11;
	uint32_t attachmentCount;
	VkFormat mainColorFormat;

	if ( !vk.multiviewSupported ) {
		ri.Printf( PRINT_WARNING, "vk_recreate_xr_render_pass: multiview not supported\n" );
		return qfalse;
	}

	// Determine color format for main render pass based on FBO mode
	// r_fbo 0 (direct): use XR swapchain UNORM format (renders directly to XR swapchain)
	// r_fbo 1 (FBO): use vk.color_format (renders to FBO, post-processed to XR swapchain)
	if ( vk.fboActive ) {
		mainColorFormat = vk.color_format;
		ri.Printf( PRINT_ALL, "FBO mode: main render pass uses FBO format 0x%x\n", mainColorFormat );
	} else {
		mainColorFormat = vk_get_unorm_format( colorFormat );
		ri.Printf( PRINT_ALL, "Direct mode: main render pass uses XR swapchain UNORM format 0x%x\n", mainColorFormat );
	}

	// Destroy old render pass if it exists
	if ( vk.render_pass.main != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.main, NULL );
		vk.render_pass.main = VK_NULL_HANDLE;
	}

	// Multiview info
	Com_Memset( &multiviewInfo, 0, sizeof( multiviewInfo ) );
	multiviewInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO;
	multiviewInfo.subpassCount = 1;
	multiviewInfo.pViewMasks = &viewMask;
	multiviewInfo.correlationMaskCount = 1;
	multiviewInfo.pCorrelationMasks = &correlationMask;

	Com_Memset( attachments, 0, sizeof( attachments ) );

	if ( vk.msaaActive ) {
		// MSAA mode: 3 attachments
		// [0] = resolve target (1x samples): the FBO color image, or in direct mode the swapchain
		// [1] = depth (multisampled)
		// [2] = MSAA color (render target)
		// [1] and [2] are transient: nothing reads the multisampled data after the resolve

		// Attachment 0: Resolve target (non-MSAA)
		attachments[0].format = mainColorFormat;
		attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;  // Will be resolved into
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		// Attachment 1: Depth (multisampled)
		attachments[1].format = vk.depth_format;
		attachments[1].samples = vkSamples;
		attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[1].stencilLoadOp = glConfig.stencilBits ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		// Attachment 2: MSAA color (render target)
		attachments[2].format = vk.color_format;
		attachments[2].samples = vkSamples;
#ifdef USE_BUFFER_CLEAR
		attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
#else
		attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
#endif
		attachments[2].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[2].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachments[2].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		// Subpass renders to MSAA color, resolves to attachment 0
		colorRef.attachment = 2;  // Render to MSAA
		colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		colorResolveRef.attachment = 0;  // Resolve to non-MSAA
		colorResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		depthRef.attachment = 1;
		depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		Com_Memset( &subpass, 0, sizeof( subpass ) );
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorRef;
		subpass.pResolveAttachments = &colorResolveRef;  // Automatic MSAA resolve
		subpass.pDepthStencilAttachment = &depthRef;

		attachmentCount = 3;

		ri.Printf( PRINT_ALL, "Recreating main render pass with MSAA (%d samples)\n", vkSamples );
	} else {
		// Non-MSAA mode: 2 attachments
		// [0] = color
		// [1] = depth
		// Uses mainColorFormat which is:
		// - vk.color_format when FBO is active (r_fbo 1)
		// - XR swapchain UNORM format when direct rendering (r_fbo 0)
		VkFormat mainDepthFormat = vk.fboActive ? vk.depth_format : depthFormat;

		// Color attachment (2-layer array)
		attachments[0].format = mainColorFormat;
		attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
#ifdef USE_BUFFER_CLEAR
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
#else
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
#endif
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		// Depth attachment (2-layer array)
		attachments[1].format = mainDepthFormat;
		attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		// Stored only where a later pass loads these samples; depth ends here
		attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[1].stencilLoadOp = glConfig.stencilBits ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		colorRef.attachment = 0;
		colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		depthRef.attachment = 1;
		depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		Com_Memset( &subpass, 0, sizeof( subpass ) );
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorRef;
		subpass.pDepthStencilAttachment = &depthRef;

		attachmentCount = 2;
	}

	// Include depth stages so an earlier frame's depth work finishes before this pass clears
	deps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	deps[0].dstSubpass = 0;
	deps[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
	                        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	deps[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
	                        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	deps[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT |
	                         VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	deps[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
	                         VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
	                         VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
	                         VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	deps[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	deps[1].srcSubpass = 0;
	deps[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	deps[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
	                        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	deps[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	deps[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
	                         VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	deps[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	deps[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	Com_Memset( &desc, 0, sizeof( desc ) );
	desc.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	desc.pNext = &multiviewInfo;
	desc.attachmentCount = attachmentCount;
	desc.pAttachments = attachments;
	desc.subpassCount = 1;
	desc.pSubpasses = &subpass;
	desc.dependencyCount = 2;
	desc.pDependencies = deps;

	if ( vk.xr.foveationActive ) {
		vk_chain_fdm_attachment( &desc, &fdmInfo, attachments, attachmentCount );
		attachmentCount++;
	}

	VK_CHECK( qvkCreateRenderPass( vk.device, &desc, NULL, &vk.render_pass.main ) );
	SET_OBJECT_NAME( vk.render_pass.main, "render pass - XR main (multiview, recreated)", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );
	ri.Printf( PRINT_ALL, "Recreated main render pass: %p (attachments: %d, foveated: %s)\n", (void*)vk.render_pass.main, attachmentCount,
		vk.xr.foveationActive ? "yes" : "no" );

	// Destroy existing pipelines: they were created with the old render pass format
	// and are no longer compatible. They will be recreated lazily with the new render pass.
	vk_destroy_pipelines( qfalse );
	ri.Printf( PRINT_ALL, "Destroyed pipelines for render pass format change\n" );

	// Recreate gamma render pass with UNORM format (only needed for FBO mode)
	// The gamma shader outputs sRGB-encoded values directly (like Quake3e), so we use
	// UNORM format to bypass Vulkan's automatic linear-to-sRGB conversion on write.
	// OpenXR will read the sRGB data correctly because the underlying image is sRGB format.
	if ( vk.render_pass.gamma != VK_NULL_HANDLE ) {
		qvkDestroyRenderPass( vk.device, vk.render_pass.gamma, NULL );
		vk.render_pass.gamma = VK_NULL_HANDLE;
	}

	if ( vk.fboActive ) {
		VkSubpassDependency gammaDep;
		VkFormat gammaFormat = vk_get_unorm_format( colorFormat );

		// gamma has only color attachment (no depth): outputs to XR swapchain
		// Uses UNORM format because gamma shader outputs sRGB values directly
		attachments[0].format = gammaFormat;
		attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		// Use UNDEFINED since we don't care about previous content (gamma overwrites everything)
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		// Reset colorRef and subpass for gamma pass (no MSAA, single attachment)
		colorRef.attachment = 0;
		colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		Com_Memset( &subpass, 0, sizeof( subpass ) );
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorRef;
		subpass.pDepthStencilAttachment = NULL;
		// No resolve attachments for gamma pass

		// Simpler dependency for gamma pass
		gammaDep.srcSubpass = VK_SUBPASS_EXTERNAL;
		gammaDep.dstSubpass = 0;
		gammaDep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		gammaDep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		gammaDep.srcAccessMask = 0;
		gammaDep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		gammaDep.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		// Gamma is not foveated: only the multiview info stays chained
		desc.pNext = &multiviewInfo;
		desc.attachmentCount = 1;
		desc.dependencyCount = 1;
		desc.pDependencies = &gammaDep;

		VK_CHECK( qvkCreateRenderPass( vk.device, &desc, NULL, &vk.render_pass.gamma ) );
		SET_OBJECT_NAME( vk.render_pass.gamma, "render pass - XR gamma (multiview, recreated)", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT );
		ri.Printf( PRINT_ALL, "Recreated gamma render pass: %p (UNORM format 0x%x)\n",
			(void*)vk.render_pass.gamma, (unsigned int)gammaFormat );
	}

	// Invalidate all pipelines created for RENDER_PASS_MAIN since render pass was recreated
	// They will be lazily recreated with the new render pass on next use
	{
		uint32_t i, invalidated = 0;
		for ( i = 0; i < vk.pipelines_count; i++ ) {
			if ( vk.pipelines[i].handle[RENDER_PASS_MAIN] != VK_NULL_HANDLE ) {
				qvkDestroyPipeline( vk.device, vk.pipelines[i].handle[RENDER_PASS_MAIN], NULL );
				vk.pipelines[i].handle[RENDER_PASS_MAIN] = VK_NULL_HANDLE;
				invalidated++;
			}
		}
		ri.Printf( PRINT_ALL, "Invalidated %u pipelines for RENDER_PASS_MAIN\n", invalidated );
	}

	// Note: Legacy vk.framebuffers.main and vk.framebuffers.post_bloom recreation removed -
	// subpass optimization uses vk.framebuffers.main_with_bloom/main_with_gamma instead,
	// which are created in vk_create_subpass_framebuffers() called from vk_init_xr_resources()

	if ( vk.fboActive ) {
		VkFormat gammaFormat = vk_get_unorm_format( colorFormat );
		ri.Printf( PRINT_ALL, "Recreated XR render passes: FBO mode (color=0x%x, depth=0x%x), gamma outputs to UNORM (0x%x)\n",
			vk.color_format, vk.depth_format, gammaFormat );
	} else {
		VkFormat unormFormat = vk_get_unorm_format( colorFormat );
		ri.Printf( PRINT_ALL, "Recreated XR render passes: Direct mode (color=0x%x UNORM, depth=0x%x)\n",
			unormFormat, depthFormat );
	}

	return qtrue;
}

/*
 * vk_init_xr_resources - Initialize all XR-related Vulkan resources
 *
 * Called after VR layer creates XR swapchains and renderer creates render passes.
 * Pulls swapchain info from VR layer via ri.VR_Vulkan_GetSwapchainInfo().
 */
qboolean vk_init_xr_resources( void )
{
	// If already initialized, destroy existing resources first to allow reinit
	// This handles the RE_Shutdown(0) + R_Init soft restart case where XR resources
	// need to be recreated to match the reset renderer state.
	if ( vk.xr.initialized ) {
		ri.Printf( PRINT_ALL, "vk_init_xr_resources: Reinitializing XR resources\n" );

		// Wait for GPU to finish before destroying resources
		if ( vk.device != VK_NULL_HANDLE ) {
			qvkDeviceWaitIdle( vk.device );
		}

		// Destroy existing XR Vulkan resources (but preserve swapchain info pointers
		// since they point to VR layer data that remains valid across soft shutdown)
		vk_destroy_post_process_pipelines();
		vk_destroy_gamma_framebuffers();
		vk_destroy_xr_framebuffers();
		vk_destroy_xr_image_views();

		vk.xr.initialized = qfalse;
	}

	// Pull swapchain info from VR layer
	const VR_VulkanSwapchainInfo* xrInfo =
		(const VR_VulkanSwapchainInfo*)ri.VR_Vulkan_GetSwapchainInfo();

	if ( !xrInfo || !xrInfo->colorImages || !xrInfo->depthImages ) {
		ri.Printf( PRINT_WARNING, "vk_init_xr_resources: XR swapchains not available\n" );
		return qfalse;
	}

	// Populate static swapchain info from pulled data
	Com_Memset( &s_colorSwapchainInfo, 0, sizeof( s_colorSwapchainInfo ) );
	s_colorSwapchainInfo.format = xrInfo->colorFormat;
	s_colorSwapchainInfo.width = xrInfo->colorWidth;
	s_colorSwapchainInfo.height = xrInfo->colorHeight;
	s_colorSwapchainInfo.arraySize = xrInfo->colorArraySize;
	s_colorSwapchainInfo.imageCount = xrInfo->colorImageCount;
	s_colorSwapchainInfo.images = xrInfo->colorImages;
	s_colorSwapchainInfo.usage = xrInfo->colorUsage;
	s_colorSwapchainInfo.foveationImages = xrInfo->foveationImages;
	s_colorSwapchainInfo.foveationWidth = xrInfo->foveationWidth;
	s_colorSwapchainInfo.foveationHeight = xrInfo->foveationHeight;

	Com_Memset( &s_depthSwapchainInfo, 0, sizeof( s_depthSwapchainInfo ) );
	s_depthSwapchainInfo.format = xrInfo->depthFormat;
	s_depthSwapchainInfo.width = xrInfo->depthWidth;
	s_depthSwapchainInfo.height = xrInfo->depthHeight;
	s_depthSwapchainInfo.arraySize = xrInfo->depthArraySize;
	s_depthSwapchainInfo.imageCount = xrInfo->depthImageCount;
	s_depthSwapchainInfo.images = xrInfo->depthImages;

	// Point vk.xr to the static info
	vk.xr.colorInfo = &s_colorSwapchainInfo;
	vk.xr.depthInfo = &s_depthSwapchainInfo;

	// Set XR render dimensions from color swapchain
	vk.xr.width = xrInfo->colorWidth;
	vk.xr.height = xrInfo->colorHeight;

	// Sync glConfig with actual swapchain dimensions
	// This fixes a race condition where ADB override resolution (from QGO/SideQuest)
	// is set AFTER initial glConfig setup but BEFORE swapchain creation
	if ( glConfig.vidWidth != (int)xrInfo->colorWidth || glConfig.vidHeight != (int)xrInfo->colorHeight ) {
		ri.Printf( PRINT_ALL, "vk_init_xr_resources: Updating glConfig from %dx%d to match swapchain %ux%u\n",
			glConfig.vidWidth, glConfig.vidHeight, xrInfo->colorWidth, xrInfo->colorHeight );
		glConfig.vidWidth = xrInfo->colorWidth;
		glConfig.vidHeight = xrInfo->colorHeight;
		glConfig.windowAspect = (float)glConfig.vidWidth / (float)glConfig.vidHeight;
	}

	ri.Printf( PRINT_ALL, "XR swapchain info: color=%ux%u (%u images), depth=%ux%u (%u images)\n",
		xrInfo->colorWidth, xrInfo->colorHeight, xrInfo->colorImageCount,
		xrInfo->depthWidth, xrInfo->depthHeight, xrInfo->depthImageCount );

	// Our own density map wherever the device can read one; the runtime's is the fallback
	vk_destroy_authored_fdm();
	vk.xr.foveationActive = qfalse;
	vk.xr.foveationWidth = 0;
	vk.xr.foveationHeight = 0;
	if ( vk.xr.fdmSupported ) {
		if ( vk_create_authored_fdm( xrInfo->colorImageCount, xrInfo->colorArraySize,
				xrInfo->colorWidth, xrInfo->colorHeight ) ) {
			vk.xr.foveationActive = qtrue;
			ri.Printf( PRINT_ALL, "Foveated rendering: %ux%u density maps written by the renderer (%ux%u px texels)\n",
				vk.xr.foveationWidth, vk.xr.foveationHeight, vk.xr.fdmTexelWidth, vk.xr.fdmTexelHeight );
		} else if ( xrInfo->foveationImages && xrInfo->foveationWidth > 0 && xrInfo->foveationHeight > 0 ) {
			vk.xr.foveationActive = qtrue;
			vk.xr.foveationWidth = xrInfo->foveationWidth;
			vk.xr.foveationHeight = xrInfo->foveationHeight;
			ri.Printf( PRINT_ALL, "Foveated rendering: %ux%u density maps from the runtime attached to the scene render passes\n",
				xrInfo->foveationWidth, xrInfo->foveationHeight );
		} else {
			ri.Printf( PRINT_ALL, "Foveated rendering: no density map could be created\n" );
		}
	} else {
		ri.Printf( PRINT_ALL, "Foveated rendering: the device cannot read a density map\n" );
	}

	// Recreate main render pass with correct XR swapchain formats
	// The initial render pass was created with desktop swapchain format which may differ
	if ( !vk_recreate_xr_render_pass( xrInfo->colorFormat, xrInfo->depthFormat ) ) {
		ri.Printf( PRINT_WARNING, "vk_init_xr_resources: Failed to recreate XR render pass\n" );
		return qfalse;
	}

	// The split render passes predate the density maps; rebuild them and their dependents
	if ( vk.fboActive ) {
		vk_destroy_post_process_pipelines();
		vk_destroy_subpass_framebuffers();
		vk_destroy_subpass_render_passes();
		vk_create_fov_split_render_passes();
	}

	if ( !vk_create_xr_image_views() ) {
		return qfalse;
	}

	// Direct mode MSAA transient images were sized before the swapchain dimensions were known
	if ( !vk.fboActive && vk.msaaActive ) {
		vk_destroy_subpass_transient_images();
		vk_create_subpass_transient_images();
	}

	if ( !vk_create_xr_framebuffers() ) {
		vk_destroy_xr_image_views();
		return qfalse;
	}

	// FBO-mode resources: subpass framebuffers, bloom descriptors
	// These are only needed for r_fbo 1 (post-processing pipeline)
	// Direct mode (r_fbo 0) renders directly to XR swapchain, skipping these
	if ( vk.fboActive ) {
		// Subpass mode: scene renders to transient images, gamma/composite in subpasses
		// Recreate transient images with correct XR dimensions
		// These may have been created with wrong dimensions during initial startup
		// before XR swapchain dimensions were known
		vk_destroy_subpass_transient_images();
		vk_create_subpass_transient_images();

		if ( !vk_create_subpass_framebuffers() ) {
			ri.Printf( PRINT_WARNING, "Subpass framebuffer creation failed\n" );
			vk_destroy_xr_framebuffers();
			vk_destroy_xr_image_views();
			return qfalse;
		}

		// Create bloom image descriptors for blur passes (if bloom enabled)
		if ( r_bloom->integer && !vk_create_xr_bloom_descriptors() ) {
			vk_destroy_subpass_framebuffers();
			vk_destroy_xr_framebuffers();
			vk_destroy_xr_image_views();
			return qfalse;
		}

		// Create XR post-processing pipelines (blur, subpass gamma/composite)
		vk_create_post_process_pipelines();
	}

	// Note: HUD buffer is created earlier in R_InitImages(), not here
	// This ensures tr.hudImage exists before CreateExternalShaders() runs

	// Note: Virtual screen resources (buffer, meshes, pipelines) are NOT created here.
	// Virtual screen (menus, spectator mode) is handled by OpenXR cylinder layer
	// composition in VR_EndFrame (vr_render_loop.c). The cylinder layer uses the
	// color swapchain directly, so no separate virtual screen buffer is needed.

	vk.xr.initialized = qtrue;
	ri.Printf( PRINT_ALL, "XR resources initialized successfully\n" );

	return qtrue;
}

/*
 * vk_shutdown_xr_resources - Shutdown all XR-related Vulkan resources
 */
void vk_shutdown_xr_resources( void )
{
	if ( !vk.xr.initialized ) {
		return;
	}

	// Wait for GPU to finish all operations before destroying resources
	if ( vk.device != VK_NULL_HANDLE ) {
		qvkDeviceWaitIdle( vk.device );
	}

	// Destroy post-processing pipelines first
	vk_destroy_post_process_pipelines();

	// Destroy subpass framebuffers
	vk_destroy_subpass_framebuffers();

	// Destroy gamma framebuffers (XR swapchain outputs)
	vk_destroy_gamma_framebuffers();
	vk_destroy_hud_buffer();
	vk_destroy_xr_framebuffers();
	vk_destroy_xr_image_views();
	vk_destroy_authored_fdm();

	// Clear swapchain info pointers
	vk.xr.colorInfo = NULL;
	vk.xr.depthInfo = NULL;
	vk.xr.foveationActive = qfalse;

	vk.xr.initialized = qfalse;
	ri.Printf( PRINT_ALL, "XR resources shutdown complete\n" );
}

#endif // USE_VULKAN
