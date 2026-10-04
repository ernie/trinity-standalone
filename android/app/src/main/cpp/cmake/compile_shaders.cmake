# Shader compilation for Vulkan renderer
# Compiles GLSL shaders to SPIR-V and generates shader_data.c
# Only rebuilds when shader sources change
#
# ioq3quest: All shaders use multiview (gl_ViewIndex) for stereo VR rendering.
# Non-multiview shaders have been removed.

include_guard(GLOBAL)

if(NOT BUILD_CLIENT OR NOT BUILD_RENDERER_VK)
    return()
endif()

# Find glslangValidator from Vulkan SDK
# On Windows, check VULKAN_SDK environment variable
# On other platforms, it should be in PATH
find_program(GLSLANG_VALIDATOR glslangValidator
    HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/bin"
)

if(NOT GLSLANG_VALIDATOR)
    message(WARNING "glslangValidator not found - shader compilation disabled")
    message(WARNING "Set VULKAN_SDK environment variable or install Vulkan SDK")
    return()
endif()

message(STATUS "Found glslangValidator: ${GLSLANG_VALIDATOR}")

set(SHADER_DIR ${SOURCE_DIR}/renderervk/shaders)
set(SPIRV_DIR ${CMAKE_BINARY_DIR}/spirv)
set(SHADER_DATA_OUTPUT ${SPIRV_DIR}/shader_data.c)
set(SHADER_DATA_FINAL ${SHADER_DIR}/spirv/shader_data.c)

# Build bin2hex utility using host compiler (not Android cross-compiler)
# This is needed because we're cross-compiling for Android but need to run
# bin2hex on the build machine (Windows/Linux/Mac)
#
# We use a separate CMake invocation with NO toolchain file to build bin2hex
set(BIN2HEX_SOURCE ${SHADER_DIR}/bin2hex.c)
set(BIN2HEX_BUILD_DIR ${CMAKE_BINARY_DIR}/bin2hex_host)

# Final location - we'll copy the built executable here for consistent access
# Use CMAKE_HOST_WIN32 (not WIN32) because WIN32 refers to target platform,
# and we're cross-compiling for Android
if(CMAKE_HOST_WIN32)
    set(BIN2HEX_EXECUTABLE ${CMAKE_BINARY_DIR}/bin2hex.exe)
else()
    set(BIN2HEX_EXECUTABLE ${CMAKE_BINARY_DIR}/bin2hex)
endif()

# Configure and build bin2hex for the host platform
file(MAKE_DIRECTORY ${BIN2HEX_BUILD_DIR})

# Create a minimal CMakeLists.txt for bin2hex
file(WRITE ${BIN2HEX_BUILD_DIR}/CMakeLists.txt
"cmake_minimum_required(VERSION 3.10)
project(bin2hex C)
add_executable(bin2hex \"${BIN2HEX_SOURCE}\")
")

# Build bin2hex at configure time using host compiler
# Use default generator (not Ninja with NDK toolchain) to get a native Windows executable
execute_process(
    COMMAND ${CMAKE_COMMAND} .
    WORKING_DIRECTORY ${BIN2HEX_BUILD_DIR}
    RESULT_VARIABLE BIN2HEX_CONFIG_RESULT
    OUTPUT_VARIABLE BIN2HEX_CONFIG_OUTPUT
    ERROR_VARIABLE BIN2HEX_CONFIG_ERROR
)

if(NOT BIN2HEX_CONFIG_RESULT EQUAL 0)
    message(WARNING "Failed to configure bin2hex: ${BIN2HEX_CONFIG_ERROR}")
    message(WARNING "Shader compilation will be skipped")
    return()
endif()

execute_process(
    COMMAND ${CMAKE_COMMAND} --build . --config Release
    WORKING_DIRECTORY ${BIN2HEX_BUILD_DIR}
    RESULT_VARIABLE BIN2HEX_BUILD_RESULT
    OUTPUT_VARIABLE BIN2HEX_BUILD_OUTPUT
    ERROR_VARIABLE BIN2HEX_BUILD_ERROR
)

if(NOT BIN2HEX_BUILD_RESULT EQUAL 0)
    message(WARNING "Failed to build bin2hex: ${BIN2HEX_BUILD_ERROR}")
    message(WARNING "Shader compilation will be skipped")
    return()
endif()

# Find where the executable was actually built (varies by generator)
# and copy it to a consistent location
if(CMAKE_HOST_WIN32)
    # Try Release subdirectory first (Visual Studio), then root (Ninja/Makefiles)
    if(EXISTS "${BIN2HEX_BUILD_DIR}/Release/bin2hex.exe")
        file(COPY "${BIN2HEX_BUILD_DIR}/Release/bin2hex.exe" DESTINATION "${CMAKE_BINARY_DIR}")
    elseif(EXISTS "${BIN2HEX_BUILD_DIR}/bin2hex.exe")
        file(COPY "${BIN2HEX_BUILD_DIR}/bin2hex.exe" DESTINATION "${CMAKE_BINARY_DIR}")
    else()
        message(WARNING "bin2hex.exe not found after build")
        return()
    endif()
else()
    file(COPY "${BIN2HEX_BUILD_DIR}/bin2hex" DESTINATION "${CMAKE_BINARY_DIR}")
endif()

message(STATUS "Built host bin2hex: ${BIN2HEX_EXECUTABLE}")

# Collect all shader source files
file(GLOB SHADER_SOURCES
    ${SHADER_DIR}/*.vert
    ${SHADER_DIR}/*.frag
    ${SHADER_DIR}/*.tmpl
)

# Use a single custom command that compiles ALL shaders sequentially
# This ensures proper ordering and atomic update of shader_data.c
#
# All 3D shaders use multiview with --target-env vulkan1.1 for gl_ViewIndex.
# Post-processing shaders also use multiview for XR r_fbo pipeline.
add_custom_command(
    OUTPUT ${SHADER_DATA_FINAL}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${SPIRV_DIR}
    COMMAND ${CMAKE_COMMAND} -E remove -f ${SHADER_DATA_OUTPUT}

    # ========================================================
    # POST-PROCESSING SHADERS (multiview for XR r_fbo pipeline)
    # These render fullscreen quads to 2-layer multiview targets
    # Requires --target-env vulkan1.1 for gl_ViewIndex
    # ========================================================
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gamma.vert
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} gamma_vert_spv
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gamma.frag
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} gamma_frag_spv
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/blend.frag
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} blend_frag_spv
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/blur.frag
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} blur_frag_spv
    # First blur pass of the foveated split, with the bloom extract folded in
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/blur.frag -DUSE_EXTRACT
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} blur_extract_frag_spv

    # Dot shader (flare visibility probe; dot.vert reads gl_ViewIndex, hence --target-env vulkan1.1)
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/dot.vert
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} dot_vert_spv
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/dot.frag
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} dot_frag_spv

    # ========================================================
    # MULTIVIEW 3D SHADERS (VR stereo rendering)
    # Requires --target-env vulkan1.1 for gl_ViewIndex
    # ========================================================

    # Standalone vertex shaders
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/color.vert
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} color_vert_spv
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/fog.vert
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} fog_vert_spv

    # Standalone fragment shaders
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/color.frag
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} color_frag_spv
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/fog.frag
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} fog_frag_spv

    # Lighting shaders
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/light_vert.tmpl
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vert_light
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/light_vert.tmpl -DUSE_FOG
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vert_light_fog

    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/light_frag.tmpl
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} frag_light
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/light_frag.tmpl -DUSE_FOG
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} frag_light_fog
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/light_frag.tmpl -DUSE_LINE
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} frag_light_line
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/light_frag.tmpl -DUSE_LINE -DUSE_FOG
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} frag_light_line_fog

    # Generic vertex shaders - single texture
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_vert.tmpl
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vert_tx0
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_vert.tmpl -DUSE_FOG
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vert_tx0_fog
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_vert.tmpl -DUSE_ENV
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vert_tx0_env
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_vert.tmpl -DUSE_FOG -DUSE_ENV
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vert_tx0_env_fog

    # Single-texture vertex, identity colors
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_vert.tmpl -DUSE_CLX_IDENT
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vert_tx0_ident1
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_vert.tmpl -DUSE_CLX_IDENT -DUSE_FOG
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vert_tx0_ident1_fog
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_vert.tmpl -DUSE_CLX_IDENT -DUSE_ENV
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vert_tx0_ident1_env
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_vert.tmpl -DUSE_CLX_IDENT -DUSE_FOG -DUSE_ENV
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vert_tx0_ident1_env_fog

    # Single-texture vertex, fixed colors
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_vert.tmpl -DUSE_FIXED_COLOR
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vert_tx0_fixed
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_vert.tmpl -DUSE_FIXED_COLOR -DUSE_FOG
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vert_tx0_fixed_fog
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_vert.tmpl -DUSE_FIXED_COLOR -DUSE_ENV
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vert_tx0_fixed_env
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_vert.tmpl -DUSE_FIXED_COLOR -DUSE_FOG -DUSE_ENV
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vert_tx0_fixed_env_fog

    # Double-texture vertex shaders
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_vert.tmpl -DUSE_TX1
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vert_tx1
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_vert.tmpl -DUSE_TX1 -DUSE_FOG
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vert_tx1_fog
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_vert.tmpl -DUSE_TX1 -DUSE_ENV
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vert_tx1_env
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_vert.tmpl -DUSE_TX1 -DUSE_FOG -DUSE_ENV
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vert_tx1_env_fog

    # Double-texture vertex, identity colors
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_vert.tmpl -DUSE_CLX_IDENT -DUSE_TX1
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vert_tx1_ident1
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_vert.tmpl -DUSE_CLX_IDENT -DUSE_TX1 -DUSE_FOG
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vert_tx1_ident1_fog
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_vert.tmpl -DUSE_CLX_IDENT -DUSE_TX1 -DUSE_ENV
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vert_tx1_ident1_env
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_vert.tmpl -DUSE_CLX_IDENT -DUSE_TX1 -DUSE_FOG -DUSE_ENV
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vert_tx1_ident1_env_fog

    # Double-texture vertex, fixed colors
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_vert.tmpl -DUSE_FIXED_COLOR -DUSE_TX1
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vert_tx1_fixed
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_vert.tmpl -DUSE_FIXED_COLOR -DUSE_TX1 -DUSE_FOG
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vert_tx1_fixed_fog
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_vert.tmpl -DUSE_FIXED_COLOR -DUSE_TX1 -DUSE_ENV
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vert_tx1_fixed_env
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_vert.tmpl -DUSE_FIXED_COLOR -DUSE_TX1 -DUSE_FOG -DUSE_ENV
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vert_tx1_fixed_env_fog

    # Double-texture vertex, non-identical colors
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_vert.tmpl -DUSE_CL1 -DUSE_TX1
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vert_tx1_cl
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_vert.tmpl -DUSE_CL1 -DUSE_TX1 -DUSE_FOG
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vert_tx1_cl_fog
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_vert.tmpl -DUSE_CL1 -DUSE_TX1 -DUSE_ENV
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vert_tx1_cl_env
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_vert.tmpl -DUSE_CL1 -DUSE_TX1 -DUSE_ENV -DUSE_FOG
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vert_tx1_cl_env_fog

    # Triple-texture vertex shaders
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_vert.tmpl -DUSE_TX2
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vert_tx2
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_vert.tmpl -DUSE_TX2 -DUSE_FOG
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vert_tx2_fog
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_vert.tmpl -DUSE_TX2 -DUSE_ENV
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vert_tx2_env
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_vert.tmpl -DUSE_TX2 -DUSE_ENV -DUSE_FOG
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vert_tx2_env_fog

    # Triple-texture vertex, non-identical colors
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_vert.tmpl -DUSE_CL2 -DUSE_TX2
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vert_tx2_cl
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_vert.tmpl -DUSE_CL2 -DUSE_TX2 -DUSE_FOG
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vert_tx2_cl_fog
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_vert.tmpl -DUSE_CL2 -DUSE_TX2 -DUSE_ENV
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vert_tx2_cl_env
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_vert.tmpl -DUSE_CL2 -DUSE_TX2 -DUSE_ENV -DUSE_FOG
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vert_tx2_cl_env_fog

    # Generic fragment shaders - single-texture
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_frag.tmpl -DUSE_ATEST
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} frag_tx0
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_frag.tmpl -DUSE_ATEST -DUSE_FOG
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} frag_tx0_fog

    # Single-texture fragment, identity color
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_frag.tmpl -DUSE_CLX_IDENT -DUSE_ATEST
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} frag_tx0_ident1
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_frag.tmpl -DUSE_CLX_IDENT -DUSE_ATEST -DUSE_FOG
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} frag_tx0_ident1_fog

    # Single-texture fragment, fixed color
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_frag.tmpl -DUSE_FIXED_COLOR -DUSE_ATEST
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} frag_tx0_fixed
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_frag.tmpl -DUSE_FIXED_COLOR -DUSE_ATEST -DUSE_FOG
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} frag_tx0_fixed_fog

    # Single-texture fragment, entity color
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_frag.tmpl -DUSE_ENT_COLOR -DUSE_ATEST
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} frag_tx0_ent
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_frag.tmpl -DUSE_ENT_COLOR -DUSE_ATEST -DUSE_FOG
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} frag_tx0_ent_fog

    # Single-texture fragment, depth-fragment
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_frag.tmpl -DUSE_CLX_IDENT -DUSE_ATEST -DUSE_DF
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} frag_tx0_df

    # Double-texture fragment
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_frag.tmpl -DUSE_TX1
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} frag_tx1
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_frag.tmpl -DUSE_TX1 -DUSE_FOG
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} frag_tx1_fog

    # Double-texture fragment, identity colors
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_frag.tmpl -DUSE_CLX_IDENT -DUSE_TX1
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} frag_tx1_ident1
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_frag.tmpl -DUSE_CLX_IDENT -DUSE_TX1 -DUSE_FOG
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} frag_tx1_ident1_fog

    # Double-texture fragment, fixed colors
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_frag.tmpl -DUSE_FIXED_COLOR -DUSE_TX1
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} frag_tx1_fixed
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_frag.tmpl -DUSE_FIXED_COLOR -DUSE_TX1 -DUSE_FOG
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} frag_tx1_fixed_fog

    # Double-texture fragment, non-identical colors
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_frag.tmpl -DUSE_CL1 -DUSE_TX1
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} frag_tx1_cl
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_frag.tmpl -DUSE_CL1 -DUSE_TX1 -DUSE_FOG
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} frag_tx1_cl_fog

    # Triple-texture fragment
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_frag.tmpl -DUSE_TX2
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} frag_tx2
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_frag.tmpl -DUSE_TX2 -DUSE_FOG
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} frag_tx2_fog

    # Triple-texture fragment, non-identical colors
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_frag.tmpl -DUSE_CL2 -DUSE_TX2
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} frag_tx2_cl
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gen_frag.tmpl -DUSE_CL2 -DUSE_TX2 -DUSE_FOG
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} frag_tx2_cl_fog

    # ========================================================
    # POST PASS SHADERS
    # Requires --target-env vulkan1.1 for multiview gl_ViewIndex
    # ========================================================
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/final_composite_fov.frag
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} final_composite_fov_frag_spv
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/gamma_fov.frag
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} gamma_fov_frag_spv

    # r_foveationDebug: reads gl_FragSizeEXT, so it belongs to the foveated scene pass
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/foveationdebug.frag
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} foveationdebug_frag_spv

    # Virtual screen: drawn after the frame into the projection layer, never foveated
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/vscreen.vert
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vscreen_vert_spv
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/vscreen.frag
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vscreen_frag_spv
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/vscreen_reflect.frag
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vscreen_reflect_frag_spv
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/floor_grid.frag
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} floor_grid_frag_spv
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/vscreen_capture.vert
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vscreen_capture_vert_spv
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/vscreen_capture.frag
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vscreen_capture_frag_spv
    # Controllers and pointer rays beside the virtual screen
    COMMAND ${GLSLANG_VALIDATOR} -S vert -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/vscreen_model.vert
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vscreen_model_vert_spv
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/vscreen_model.frag
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vscreen_model_frag_spv

    # The stored scene into a plain sRGB swapchain
    COMMAND ${GLSLANG_VALIDATOR} -S frag -V --target-env vulkan1.1 -o ${SPIRV_DIR}/temp.spv ${SHADER_DIR}/vscreen_present.frag
    COMMAND ${BIN2HEX_EXECUTABLE} ${SPIRV_DIR}/temp.spv +${SHADER_DATA_OUTPUT} vscreen_present_frag_spv

    # Cleanup temp file and copy to source tree atomically
    COMMAND ${CMAKE_COMMAND} -E remove -f ${SPIRV_DIR}/temp.spv
    COMMAND ${CMAKE_COMMAND} -E copy ${SHADER_DATA_OUTPUT} ${SHADER_DATA_FINAL}

    DEPENDS ${SHADER_SOURCES}
    COMMENT "Compiling SPIR-V shaders into ${SHADER_DATA_FINAL}"
    VERBATIM
)

# Create target for shader compilation
add_custom_target(compile_shaders
    DEPENDS ${SHADER_DATA_FINAL}
    COMMENT "All shaders compiled"
)
