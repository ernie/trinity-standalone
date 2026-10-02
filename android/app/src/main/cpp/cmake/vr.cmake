# VR sources for ioq3quest
# Phase 4: Full Vulkan VR integration with XR_KHR_vulkan_enable2

include_guard(GLOBAL)

message(STATUS "Configuring VR sources")

# Graphics-agnostic VR sources (vrcommon/)
set(VR_COMMON_SOURCES
    ${SOURCE_DIR}/vrcommon/vr_base.c
    ${SOURCE_DIR}/vrcommon/vr_bind.c
    ${SOURCE_DIR}/vrcommon/vr_controller_models.c
    ${SOURCE_DIR}/vrcommon/vr_cvars.c
    ${SOURCE_DIR}/vrcommon/vr_debug.c
    ${SOURCE_DIR}/vrcommon/vr_events.c
    ${SOURCE_DIR}/vrcommon/vr_gameplay.c
    ${SOURCE_DIR}/vrcommon/vr_haptics.c
    ${SOURCE_DIR}/vrcommon/vr_input.c
    ${SOURCE_DIR}/vrcommon/vr_instance.c
    ${SOURCE_DIR}/vrcommon/vr_math.c
    ${SOURCE_DIR}/vrcommon/vr_model.c
    ${SOURCE_DIR}/vrcommon/vr_render_loop.c
    ${SOURCE_DIR}/vrcommon/vr_router.c
    ${SOURCE_DIR}/vrcommon/vr_session.c
    ${SOURCE_DIR}/vrcommon/vr_shared_sync.c
    ${SOURCE_DIR}/vrcommon/vr_spaces.c
    ${SOURCE_DIR}/vrcommon/vr_swapchains.c
    ${SOURCE_DIR}/vrcommon/vr_virtual_screen.c
    ${SOURCE_DIR}/vrcommon/vk_xr_models.c
)

# Vulkan specific VR sources (vrvk/)
# Uses vrcommon/vr_events.c for event handling
set(VR_VK_SOURCES
    ${SOURCE_DIR}/vrvk/vr_vk.c
    ${SOURCE_DIR}/vrvk/vr_vk_debug.c
    ${SOURCE_DIR}/vrvk/vr_vk_session.c
    ${SOURCE_DIR}/vrvk/vr_vk_swapchains.c
    ${SOURCE_DIR}/vrvk/vr_vk_foveation.c
    ${SOURCE_DIR}/vrvk/vr_vk_loading.c
    ${SOURCE_DIR}/vrvk/vr_vk_renderer.c
)

# Combine VR sources
set(VR_SOURCES ${VR_COMMON_SOURCES} ${VR_VK_SOURCES})
list(APPEND CLIENT_INCLUDE_DIRS ${SOURCE_DIR}/vrcommon ${SOURCE_DIR}/vrvk)
list(APPEND RENDERER_INCLUDE_DIRS ${SOURCE_DIR}/vrcommon ${SOURCE_DIR}/vrvk)
