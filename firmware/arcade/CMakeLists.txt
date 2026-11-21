# Firmware do controle arcade (Raspberry Pi Pico, RP2040).
# Needs PICO_SDK_PATH; see scripts/build-firmware.sh.
cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD pico CACHE STRING "Board")
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(snespi_arcade C CXX ASM)
set(CMAKE_C_STANDARD 11)
pico_sdk_init()

# Version (file VERSION, MAJOR.MINOR.PATCH, one digit each): goes to the USB
# bcdDevice field (1.2.3 -> 0x0123) and to the picotool program info.
file(STRINGS ${CMAKE_CURRENT_LIST_DIR}/VERSION ARCADE_VERSION LIMIT_COUNT 1)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CMAKE_CURRENT_LIST_DIR}/VERSION)
if(NOT ARCADE_VERSION MATCHES "^([0-9])\\.([0-9])\\.([0-9])$")
	message(FATAL_ERROR "VERSION must be MAJOR.MINOR.PATCH with one digit each: '${ARCADE_VERSION}'")
endif()
set(ARCADE_VERSION_BCD 0x0${CMAKE_MATCH_1}${CMAKE_MATCH_2}${CMAKE_MATCH_3})

# 1 ou 2: quantos joysticks o Pico apresenta ao computador
set(ARCADE_PLAYERS 2 CACHE STRING "Players (1 or 2)")

add_executable(snespi-arcade
	src/main.c
	src/pad.c
	src/usb_descriptors.c
)
set_target_properties(snespi-arcade PROPERTIES OUTPUT_NAME snespi-arcade-${ARCADE_PLAYERS}p)
target_include_directories(snespi-arcade PRIVATE src)
target_compile_definitions(snespi-arcade PRIVATE
	ARCADE_PLAYERS=${ARCADE_PLAYERS}
	ARCADE_VERSION_BCD=${ARCADE_VERSION_BCD}
)
pico_set_program_name(snespi-arcade "snes-pi-os Controle Arcade")
pico_set_program_version(snespi-arcade "${ARCADE_VERSION}")
target_compile_options(snespi-arcade PRIVATE -Wall -Wextra)
target_link_libraries(snespi-arcade pico_stdlib pico_unique_id pico_bootrom tinyusb_device)

pico_enable_stdio_usb(snespi-arcade 0)
pico_enable_stdio_uart(snespi-arcade 0)
pico_add_extra_outputs(snespi-arcade)
