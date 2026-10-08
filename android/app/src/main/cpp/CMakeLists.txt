cmake_minimum_required(VERSION 3.10...3.31)

# This CMakeLists.txt has two modes:
# 1. FULL BUILD: When invoked from command line (run.bat/run.sh) with -DFULL_BUILD=ON
#    Builds the entire engine and game modules
# 2. STUB MODE: When invoked from Gradle (default)
#    Just builds libmain.so and copies pre-built libraries from the CMake build

option(FULL_BUILD "Build full engine (not just the stub)" OFF)

if(FULL_BUILD)
    #############################################################################
    # FULL BUILD MODE - Build complete engine and game modules
    #############################################################################

    # Add cmake module path
    list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

    # Include project identity (name, version, etc.)
    include(identity)

    project(${PROJECT_NAME} LANGUAGES C CXX)

    #############################################################################
    # Build Options
    #############################################################################

    option(BUILD_CLIENT "Build client executable" ON)
    option(BUILD_GAME_LIBRARIES "Build game module libraries (cgame, qagame, ui)" ON)

    # Renderer (Vulkan only)
    set(BUILD_RENDERER_VK ON)

    # Codec options
    option(USE_CODEC_VORBIS "Ogg Vorbis support" ON)
    option(USE_CODEC_OPUS "Ogg Opus support" ON)
    option(USE_VOIP "Voice chat support" ON)

    # Library options
    option(USE_INTERNAL_LIBS "Use internally packaged libraries" ON)
    option(USE_INTERNAL_ZLIB "Use internal copy of zlib" ${USE_INTERNAL_LIBS})
    option(USE_INTERNAL_JPEG "Use internal copy of libjpeg" ${USE_INTERNAL_LIBS})
    option(USE_INTERNAL_OGG "Use internal copy of ogg" ${USE_INTERNAL_LIBS})
    option(USE_INTERNAL_VORBIS "Use internal copy of vorbis" ${USE_INTERNAL_LIBS})
    option(USE_INTERNAL_OPUS "Use internal copy of opus" ${USE_INTERNAL_LIBS})

    # HTTP download support
    option(USE_HTTP "HTTP download support" ON)

    #############################################################################
    # Compiler and Platform Configuration
    #############################################################################

    # Release build by default
    if(NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE Release)
    endif()

    set(CMAKE_C_STANDARD 99)
    set(CMAKE_C_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(ENGINE_BINARY "ioquake3_vulkan")

    # Source directory
    set(SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/code)

    # Platform and compiler settings
    include(compilers/clang)
    include(platforms/android)

    #############################################################################
    # Libraries
    #############################################################################

    include(libraries/all)

    #############################################################################
    # Client Executable
    #############################################################################

    if(BUILD_CLIENT)
        include(utils/set_output_dirs)
        include(shared_sources)
        include(renderer_common)
        include(renderer_vk)
        include(vr)

        # Client definitions
        list(APPEND CLIENT_DEFINITIONS BOTLIB)

        # Re-read every build; force-included so q_shared.h's Q3_VERSION sees it
        add_custom_target(trinity_version
            COMMAND ${CMAKE_COMMAND} -DREPO_ROOT=${CMAKE_SOURCE_DIR}/../../../../..
                -DOUT=${CMAKE_BINARY_DIR}/trinity_version.h -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/write_version.cmake
            BYPRODUCTS ${CMAKE_BINARY_DIR}/trinity_version.h)

        if(USE_VOIP)
            list(APPEND CLIENT_DEFINITIONS USE_VOIP)
        endif()

        # Combine all engine sources (without main.c entry point)
        set(ENGINE_SOURCES
            ${SERVER_SOURCES}
            ${CLIENT_SOURCES}
            ${COMMON_SOURCES}
            ${ZSTD_SOURCES}
            ${BOTLIB_SOURCES}
            ${SYSTEM_SOURCES}
            ${VR_SOURCES}
            ${CLIENT_LIBRARY_SOURCES}
        )

        # Add object library sources (CMake 3.10 compatible - can't link OBJECT libs directly)
        foreach(OBJLIB IN LISTS CLIENT_OBJECT_LIBRARIES)
            list(APPEND ENGINE_SOURCES $<TARGET_OBJECTS:${OBJLIB}>)
        endforeach()

        # Add Vulkan renderer sources
        list(APPEND ENGINE_SOURCES ${RENDERER_VK_BINARY_SOURCES})
        list(APPEND CLIENT_DEFINITIONS USE_VULKAN)
        # The model parser keeps KTX2 images because the transcoder below can read them; only the formats it
        # needs for RGBA output of Meta's UASTC files stay compiled in
        list(APPEND CLIENT_DEFINITIONS VR_MODEL_KTX2=1 BASISD_SUPPORT_KTX2=1 BASISD_SUPPORT_KTX2_ZSTD=1
            BASISD_SUPPORT_UASTC=1 BASISD_SUPPORT_DXT1=0 BASISD_SUPPORT_DXT5A=0 BASISD_SUPPORT_BC7_MODE5=0
            BASISD_SUPPORT_PVRTC1=0 BASISD_SUPPORT_ETC2_EAC_A8=0 BASISD_SUPPORT_ASTC=0 BASISD_SUPPORT_ATC=0
            BASISD_SUPPORT_FXT1=0 BASISD_SUPPORT_PVRTC2=0 BASISD_SUPPORT_ETC2_EAC_RG11=0)

        #############################################################################
        # Engine library
        #############################################################################

        add_library(${ENGINE_BINARY} SHARED ${ENGINE_SOURCES})
        add_dependencies(${ENGINE_BINARY} trinity_version)

        target_include_directories(${ENGINE_BINARY} PRIVATE
            ${SOURCE_DIR}
            ${CLIENT_INCLUDE_DIRS}
            ${RENDERER_INCLUDE_DIRS}
        )

        target_compile_definitions(${ENGINE_BINARY} PRIVATE
            ${COMMON_DEFINITIONS}
            ${CLIENT_DEFINITIONS}
        )

        target_compile_options(${ENGINE_BINARY} PRIVATE
            ${COMMON_COMPILE_OPTIONS}
            ${CLIENT_COMPILE_OPTIONS}
            -include ${CMAKE_BINARY_DIR}/trinity_version.h
        )

        target_link_libraries(${ENGINE_BINARY} PRIVATE
            ${COMMON_LIBRARIES}
            ${CLIENT_LIBRARIES}
            ${RENDERER_LIBRARIES}
            android
            log
            m
            dl
        )

        set_output_dirs(${ENGINE_BINARY})

        # Add shader compilation dependency
        if(TARGET compile_shaders)
            add_dependencies(${ENGINE_BINARY} compile_shaders)
        endif()

        # Copy deploy libraries to output directory
        foreach(LIBRARY IN LISTS CLIENT_DEPLOY_LIBRARIES)
            get_filename_component(LIB_NAME ${LIBRARY} NAME)
            add_custom_command(TARGET ${ENGINE_BINARY} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${LIBRARY}
                    ${CMAKE_BINARY_DIR}/${LIB_NAME}
            )
        endforeach()

        # Create marker file for stub build to detect renderer type
        add_custom_command(TARGET ${ENGINE_BINARY} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_BINARY_DIR}/.vulkan_build
            COMMENT "Creating Vulkan build marker"
        )
    endif()

    #############################################################################
    # Game Modules
    #############################################################################

    if(BUILD_GAME_LIBRARIES)
        include(trinity_modules)
    endif()

    #############################################################################
    # Summary
    #############################################################################

    message(STATUS "")
    message(STATUS "Build Configuration:")
    message(STATUS "  Platform: Android (${ARCH})")
    message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
    message(STATUS "  Client: ${BUILD_CLIENT}")
    message(STATUS "  Renderer: Vulkan")
    message(STATUS "  Game Libraries: ${BUILD_GAME_LIBRARIES}")
    message(STATUS "")

else()
    #############################################################################
    # STUB MODE - Gradle invocation (default)
    # Just build libmain.so and copy pre-built libraries
    #############################################################################

    project(ioq3quest_stub LANGUAGES C)

    set(${CMAKE_C_FLAGS}, "${CMAKE_C_FLAGS}")

    set(ENGINE_BINARY "ioquake3_vulkan")

    # Build directory where full CMake build outputs libraries
    set(FULL_BUILD_DIR "${CMAKE_SOURCE_DIR}/../../../../../build")

    # jniLibs destination
    set(JNILIBS_DIR "${CMAKE_SOURCE_DIR}/../jniLibs/arm64-v8a")

    # Create jniLibs directory if it doesn't exist
    file(MAKE_DIRECTORY ${JNILIBS_DIR})

    # Copy libraries from full build to jniLibs
    add_custom_target(copy_libs
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${FULL_BUILD_DIR}/lib${ENGINE_BINARY}.so" "${JNILIBS_DIR}/lib${ENGINE_BINARY}.so"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${FULL_BUILD_DIR}/libSDL2.so" "${JNILIBS_DIR}/libSDL2.so"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${FULL_BUILD_DIR}/libopenxr_loader.so" "${JNILIBS_DIR}/libopenxr_loader.so"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/code/libs/android/arm64-v8a/libcurl.so" "${JNILIBS_DIR}/libcurl.so"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/code/libs/android/arm64-v8a/libopenal.so" "${JNILIBS_DIR}/libopenal.so"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${FULL_BUILD_DIR}/baseq3/cgameaarch64.so" "${JNILIBS_DIR}/libcgameaarch64_baseq3.so"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${FULL_BUILD_DIR}/baseq3/qagameaarch64.so" "${JNILIBS_DIR}/libqagameaarch64_baseq3.so"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${FULL_BUILD_DIR}/baseq3/uiaarch64.so" "${JNILIBS_DIR}/libuiaarch64_baseq3.so"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${FULL_BUILD_DIR}/missionpack/cgameaarch64.so" "${JNILIBS_DIR}/libcgameaarch64_missionpack.so"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${FULL_BUILD_DIR}/missionpack/qagameaarch64.so" "${JNILIBS_DIR}/libqagameaarch64_missionpack.so"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${FULL_BUILD_DIR}/missionpack/uiaarch64.so" "${JNILIBS_DIR}/libuiaarch64_missionpack.so"
        # Copy validation layer if present (for debug builds)
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/code/libs/android/arm64-v8a/libVkLayer_khronos_validation.so" "${JNILIBS_DIR}/libVkLayer_khronos_validation.so" || ${CMAKE_COMMAND} -E true
        COMMENT "Copying pre-built libraries to jniLibs"
    )

    # Link directories for the pre-built libraries
    link_directories(${JNILIBS_DIR})

    # Build libmain.so - the JNI entry point
    add_library(main SHARED main.c)

    # Ensure libraries are copied before building main
    add_dependencies(main copy_libs)

    target_include_directories(main PRIVATE
        ${CMAKE_SOURCE_DIR}/code/OpenXR-SDK/include
        ${CMAKE_SOURCE_DIR}/code/OpenXR/Include
        ${CMAKE_SOURCE_DIR}/code/SDL2/include
        ${CMAKE_SOURCE_DIR}/code
        ${CMAKE_SOURCE_DIR}/code/vrcommon
        ${CMAKE_SOURCE_DIR}/code/vrvk
    )

    target_compile_definitions(main PRIVATE
        ARCH_STRING=android
        USE_VULKAN
    )

    find_library(log-lib log)

    target_link_libraries(main
        android
        ${ENGINE_BINARY}
        SDL2
        openxr_loader
        ${log-lib}
    )

endif()
