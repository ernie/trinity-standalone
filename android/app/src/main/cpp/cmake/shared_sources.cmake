# Shared source files for qcommon, botlib, server, and client
include_guard(GLOBAL)

# Common sources (used by both client and dedicated server)
set(COMMON_SOURCES
    ${SOURCE_DIR}/qcommon/cm_load.c
    ${SOURCE_DIR}/qcommon/cm_patch.c
    ${SOURCE_DIR}/qcommon/cm_polylib.c
    ${SOURCE_DIR}/qcommon/cm_test.c
    ${SOURCE_DIR}/qcommon/cm_trace.c
    ${SOURCE_DIR}/qcommon/cmd.c
    ${SOURCE_DIR}/qcommon/common.c
    ${SOURCE_DIR}/qcommon/cvar.c
    ${SOURCE_DIR}/qcommon/files.c
    ${SOURCE_DIR}/qcommon/md4.c
    ${SOURCE_DIR}/qcommon/md5.c
    ${SOURCE_DIR}/qcommon/msg.c
    ${SOURCE_DIR}/qcommon/net_chan.c
    ${SOURCE_DIR}/qcommon/net_ip.c
    ${SOURCE_DIR}/qcommon/huffman.c
    ${SOURCE_DIR}/qcommon/q_math.c
    ${SOURCE_DIR}/qcommon/q_shared.c
    ${SOURCE_DIR}/qcommon/unzip.c
    ${SOURCE_DIR}/qcommon/ioapi.c
    ${SOURCE_DIR}/qcommon/puff.c
    ${SOURCE_DIR}/qcommon/vm.c
    ${SOURCE_DIR}/qcommon/vm_interpreted.c
    ${SOURCE_DIR}/qcommon/vm_vr.c
    ${SOURCE_DIR}/qcommon/vm_aarch64.c
)

# Suppress warnings in vendored minizip code
set_source_files_properties(
    ${SOURCE_DIR}/qcommon/unzip.c
    ${SOURCE_DIR}/qcommon/ioapi.c
    PROPERTIES COMPILE_FLAGS -w
)

# Zstandard compression library
set(ZSTD_SOURCES
    ${SOURCE_DIR}/libzstd/zstd.c
)

# System sources
set(SYSTEM_SOURCES
    ${SOURCE_DIR}/sys/con_log.c
    ${SOURCE_DIR}/sys/sys_main.c
    ${SYSTEM_PLATFORM_SOURCES}
    ${CONSOLE_SOURCES}
)

# Server sources (embedded in client for ioq3quest)
set(SERVER_SOURCES
    ${SOURCE_DIR}/server/sv_bot.c
    ${SOURCE_DIR}/server/sv_client.c
    ${SOURCE_DIR}/server/sv_ccmds.c
    ${SOURCE_DIR}/server/sv_game.c
    ${SOURCE_DIR}/server/sv_init.c
    ${SOURCE_DIR}/server/sv_main.c
    ${SOURCE_DIR}/server/sv_net_chan.c
    ${SOURCE_DIR}/server/sv_snapshot.c
    ${SOURCE_DIR}/server/sv_tv.c
    ${SOURCE_DIR}/server/sv_world.c
)

# Bot library
set(BOTLIB_SOURCES
    ${SOURCE_DIR}/botlib/be_aas_bspq3.c
    ${SOURCE_DIR}/botlib/be_aas_cluster.c
    ${SOURCE_DIR}/botlib/be_aas_debug.c
    ${SOURCE_DIR}/botlib/be_aas_entity.c
    ${SOURCE_DIR}/botlib/be_aas_file.c
    ${SOURCE_DIR}/botlib/be_aas_main.c
    ${SOURCE_DIR}/botlib/be_aas_move.c
    ${SOURCE_DIR}/botlib/be_aas_optimize.c
    ${SOURCE_DIR}/botlib/be_aas_reach.c
    ${SOURCE_DIR}/botlib/be_aas_route.c
    ${SOURCE_DIR}/botlib/be_aas_routealt.c
    ${SOURCE_DIR}/botlib/be_aas_sample.c
    ${SOURCE_DIR}/botlib/be_ai_char.c
    ${SOURCE_DIR}/botlib/be_ai_chat.c
    ${SOURCE_DIR}/botlib/be_ai_gen.c
    ${SOURCE_DIR}/botlib/be_ai_goal.c
    ${SOURCE_DIR}/botlib/be_ai_move.c
    ${SOURCE_DIR}/botlib/be_ai_weap.c
    ${SOURCE_DIR}/botlib/be_ai_weight.c
    ${SOURCE_DIR}/botlib/be_ea.c
    ${SOURCE_DIR}/botlib/be_interface.c
    ${SOURCE_DIR}/botlib/l_crc.c
    ${SOURCE_DIR}/botlib/l_libvar.c
    ${SOURCE_DIR}/botlib/l_log.c
    ${SOURCE_DIR}/botlib/l_memory.c
    ${SOURCE_DIR}/botlib/l_precomp.c
    ${SOURCE_DIR}/botlib/l_script.c
    ${SOURCE_DIR}/botlib/l_struct.c
)

# Client sources
set(CLIENT_SOURCES
    ${SOURCE_DIR}/client/cl_cgame.c
    ${SOURCE_DIR}/client/cl_cin.c
    ${SOURCE_DIR}/client/cl_console.c
    ${SOURCE_DIR}/client/cl_input.c
    ${SOURCE_DIR}/client/cl_keyboard.c
    ${SOURCE_DIR}/client/cl_vkb_layout.c
    ${SOURCE_DIR}/client/cl_keys.c
    ${SOURCE_DIR}/client/cl_vr_bind.c
    ${SOURCE_DIR}/client/cl_main.c
    ${SOURCE_DIR}/client/cl_net_chan.c
    ${SOURCE_DIR}/client/cl_parse.c
    ${SOURCE_DIR}/client/cl_scrn.c
    ${SOURCE_DIR}/client/cl_ui.c
    ${SOURCE_DIR}/client/cl_voip.c
    ${SOURCE_DIR}/client/cl_avi.c
    ${SOURCE_DIR}/client/cl_http_curl.c
    ${SOURCE_DIR}/client/cl_trinity.c
    ${SOURCE_DIR}/client/cl_trinity_rconset.c
    ${SOURCE_DIR}/qcommon/autoupdate.c
    ${SOURCE_DIR}/client/cl_tv.c
    ${SOURCE_DIR}/client/snd_altivec.c
    ${SOURCE_DIR}/client/snd_adpcm.c
    ${SOURCE_DIR}/client/snd_dma.c
    ${SOURCE_DIR}/client/snd_mem.c
    ${SOURCE_DIR}/client/snd_mix.c
    ${SOURCE_DIR}/client/snd_wavelet.c
    ${SOURCE_DIR}/client/snd_main.c
    ${SOURCE_DIR}/client/snd_codec.c
    ${SOURCE_DIR}/client/snd_codec_wav.c
    ${SOURCE_DIR}/client/snd_codec_ogg.c
    ${SOURCE_DIR}/client/snd_codec_opus.c
    ${SOURCE_DIR}/client/qal.c
    ${SOURCE_DIR}/client/snd_openal.c
    ${SOURCE_DIR}/sdl/sdl_input.c
    ${CLIENT_PLATFORM_SOURCES}
)

# Include directories for qcommon/client
list(APPEND CLIENT_INCLUDE_DIRS
    ${SOURCE_DIR}/qcommon
    ${SOURCE_DIR}/client
    ${SOURCE_DIR}/server
    ${SOURCE_DIR}/botlib
    ${SOURCE_DIR}/sys
    ${SOURCE_DIR}/libzstd
)
