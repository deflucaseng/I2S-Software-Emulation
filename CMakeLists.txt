cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion RISCV_ZCB_RPI_2_2_0_3)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico2 CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

# Auto-download pico-extras if not found (must be before project)
set(PICO_EXTRAS_FETCH_FROM_GIT ON)
set(PICO_EXTRAS_FETCH_FROM_GIT_PATH ${CMAKE_BINARY_DIR})

include(FetchContent)
set(FETCHCONTENT_BASE_DIR_SAVE ${FETCHCONTENT_BASE_DIR})
if (PICO_EXTRAS_FETCH_FROM_GIT_PATH)
    get_filename_component(FETCHCONTENT_BASE_DIR "${PICO_EXTRAS_FETCH_FROM_GIT_PATH}" REALPATH BASE_DIR "${CMAKE_SOURCE_DIR}")
endif ()
FetchContent_Declare(
    pico_extras
    GIT_REPOSITORY https://github.com/raspberrypi/pico-extras
    GIT_TAG master
)
if (NOT pico_extras_POPULATED)
    message("Downloading Raspberry Pi Pico Extras")
    FetchContent_Populate(pico_extras)
    set(PICO_EXTRAS_PATH ${pico_extras_SOURCE_DIR})
endif ()
set(FETCHCONTENT_BASE_DIR ${FETCHCONTENT_BASE_DIR_SAVE})

# Set PICO_EXTRAS_PATH for subdirectory inclusion
set(PICO_EXTRAS_PATH "${PICO_EXTRAS_PATH}" CACHE PATH "Path to the PICO EXTRAS" FORCE)

# Add pico-extras to the build (must be before project)
add_subdirectory(${PICO_EXTRAS_PATH} pico_extras)

project(I2S-Software-Emulation C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable
add_executable(I2S-Software-Emulation I2S-Software-Emulation.c)

pico_set_program_name(I2S-Software-Emulation "I2S-Software-Emulation")
pico_set_program_version(I2S-Software-Emulation "0.1")

# Generate PIO header
pico_generate_pio_header(I2S-Software-Emulation ${CMAKE_CURRENT_LIST_DIR}/audio_i2s.pio)

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(I2S-Software-Emulation 0)
pico_enable_stdio_usb(I2S-Software-Emulation 1)

# Add the standard library to the build
target_link_libraries(I2S-Software-Emulation
        pico_stdlib
        hardware_spi
        hardware_pio
        hardware_dma
        hardware_irq
        pico_audio
)

# Add the standard include files to the build
target_include_directories(I2S-Software-Emulation PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

pico_add_extra_outputs(I2S-Software-Emulation)