# Project identity - sets project name and version
include_guard(GLOBAL)

set(PROJECT_NAME "trinity-standalone")
set(CLIENT_NAME "ioquake3")

# Game directories
set(BASEGAME "baseq3")
set(MISSIONPACK "missionpack")

# Architecture for Android arm64
set(ARCH "aarch64")
set(ARCH_STRING "aarch64")

# Module names for game libraries
set(CGAME_MODULE "cgame${ARCH}")
set(GAME_MODULE "qagame${ARCH}")
set(UI_MODULE "ui${ARCH}")
