# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)
set(PICO_SDK_PATH "/Users/gx1000/code/pico-sdk")

set(PICO_BOARD pico CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

if (PICO_SDK_VERSION_STRING VERSION_LESS "1.4.0")
  message(FATAL_ERROR "Raspberry Pi Pico SDK version 1.4.0 (or later) required. Your version is ${PICO_SDK_VERSION_STRING}")
endif()

project(usb_mouse C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(usb_mouse usb_mouse.c )
pico_generate_pio_header(usb_mouse ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio)

pico_set_program_name(usb_mouse "usb_mouse")
pico_set_program_version(usb_mouse "0.1")

# Disable stdio over UART and USB since we're using USB for HID
pico_enable_stdio_uart(usb_mouse 0)
pico_enable_stdio_usb(usb_mouse 0)

# Add the standard library to the build
target_link_libraries(usb_mouse
        pico_stdlib
        tinyusb_device
        tinyusb_board
        hardware_pio)

# Add the standard include files to the build
target_include_directories(usb_mouse PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}
  ${CMAKE_CURRENT_LIST_DIR}/.. # for our common lwipopts or any other standard includes, if required
)

# USB configuration
pico_enable_stdio_usb(usb_mouse 0)

# TinyUSB device configuration
target_compile_definitions(usb_mouse PRIVATE
    PICO_TINYUSB_IMPL=1
    CFG_TUSB_MCU=OPT_MCU_RP2040
    CFG_TUD_CDC=0          # Disable CDC (Serial)
    CFG_TUD_MSC=0          # Disable MSC (Mass Storage)
    CFG_TUD_HID=1          # Enable HID
    CFG_TUD_HID_EPIN_BUFSIZE=64
    CFG_TUD_HID_EPOUT_BUFSIZE=64
)

pico_add_extra_outputs(usb_mouse)

# Add custom flash target to copy UF2 file to Pico
add_custom_target(flash
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_BINARY_DIR}/usb_mouse.uf2
        /Volumes/RPI-RP2/
    DEPENDS usb_mouse
    COMMENT "Copying usb_mouse.uf2 to Pico (/Volumes/RPI-RP2/)"
    VERBATIM
)