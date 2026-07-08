# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico2_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(Clocodile C CXX ASM)

pico_sdk_init()

add_executable(Clocodile
	main.c
	lcd.c
	matrix.c
	ntp.c
	cli.c

	graphics/font1/zero.c
	graphics/font1/one.c
	graphics/font1/two.c
	graphics/font1/three.c
	graphics/font1/four.c
	graphics/font1/five.c
	graphics/font1/six.c
	graphics/font1/seven.c
	graphics/font1/eight.c
	graphics/font1/nine.c

	graphics/font3/zero.c
	graphics/font3/one.c
	graphics/font3/two.c
	graphics/font3/three.c
	graphics/font3/four.c
	graphics/font3/five.c
	graphics/font3/six.c
	graphics/font3/seven.c
	graphics/font3/eight.c
	graphics/font3/nine.c

	graphics/font4/zero.c
	graphics/font4/one.c
	graphics/font4/two.c
	graphics/font4/three.c
	graphics/font4/four.c
	graphics/font4/five.c
	graphics/font4/six.c
	graphics/font4/seven.c
	graphics/font4/eight.c
	graphics/font4/nine.c

	graphics/font5/zero.c
	graphics/font5/one.c
	graphics/font5/two.c
	graphics/font5/three.c
	graphics/font5/four.c
	graphics/font5/five.c
	graphics/font5/six.c
	graphics/font5/seven.c
	graphics/font5/eight.c
	graphics/font5/nine.c

	graphics/font6/zero.c
	graphics/font6/one.c
	graphics/font6/two.c
	graphics/font6/three.c
	graphics/font6/four.c
	graphics/font6/five.c
	graphics/font6/six.c
	graphics/font6/seven.c
	graphics/font6/eight.c
	graphics/font6/nine.c

	graphics/font7/zero.c
	graphics/font7/one.c
	graphics/font7/two.c
	graphics/font7/three.c
	graphics/font7/four.c
	graphics/font7/five.c
	graphics/font7/six.c
	graphics/font7/seven.c
	graphics/font7/eight.c
	graphics/font7/nine.c

	graphics/font8/zero.c
	graphics/font8/one.c
	graphics/font8/two.c
	graphics/font8/three.c
	graphics/font8/four.c
	graphics/font8/five.c
	graphics/font8/six.c
	graphics/font8/seven.c
	graphics/font8/eight.c
	graphics/font8/nine.c

	graphics/font9/zero.c
	graphics/font9/one.c
	graphics/font9/two.c
	graphics/font9/three.c
	graphics/font9/four.c
	graphics/font9/five.c
	graphics/font9/six.c
	graphics/font9/seven.c
	graphics/font9/eight.c
	graphics/font9/nine.c

	graphics/font10/zero.c
	graphics/font10/one.c
	graphics/font10/two.c
	graphics/font10/three.c
	graphics/font10/four.c
	graphics/font10/five.c
	graphics/font10/six.c
	graphics/font10/seven.c
	graphics/font10/eight.c
	graphics/font10/nine.c

	graphics/font11/zero.c
	graphics/font11/one.c
	graphics/font11/two.c
	graphics/font11/three.c
	graphics/font11/four.c
	graphics/font11/five.c
	graphics/font11/six.c
	graphics/font11/seven.c
	graphics/font11/eight.c
	graphics/font11/nine.c

	graphics/font12/zero.c
	graphics/font12/one.c
	graphics/font12/two.c
	graphics/font12/three.c
	graphics/font12/four.c
	graphics/font12/five.c
	graphics/font12/six.c
	graphics/font12/seven.c
	graphics/font12/eight.c
	graphics/font12/nine.c

	graphics/font13/zero.c
	graphics/font13/one.c
	graphics/font13/two.c
	graphics/font13/three.c
	graphics/font13/four.c
	graphics/font13/five.c
	graphics/font13/six.c
	graphics/font13/seven.c
	graphics/font13/eight.c
	graphics/font13/nine.c

	graphics/font14/zero.c
	graphics/font14/one.c
	graphics/font14/two.c
	graphics/font14/three.c
	graphics/font14/four.c
	graphics/font14/five.c
	graphics/font14/six.c
	graphics/font14/seven.c
	graphics/font14/eight.c
	graphics/font14/nine.c
)

pico_set_program_name(Clocodile "Clocodile")
pico_set_program_version(Clocodile "1.0")

pico_enable_stdio_uart(Clocodile 0)
pico_enable_stdio_usb(Clocodile 1)

pico_generate_pio_header(Clocodile ${CMAKE_CURRENT_LIST_DIR}/lcd.pio)

target_link_libraries(Clocodile
	pico_cyw43_arch_lwip_poll
	pico_stdlib
	pico_malloc
	pico_time
	pico_rand
	pico_platform
	pico_multicore
	pico_aon_timer
	hardware_pio
	hardware_dma
	hardware_flash
)

target_include_directories(Clocodile PRIVATE
	${CMAKE_CURRENT_LIST_DIR}
)

pico_add_extra_outputs(Clocodile)
