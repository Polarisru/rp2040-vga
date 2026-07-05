# cmake version
cmake_minimum_required(VERSION 3.13)

# ── Pico SDK bootstrap ───────────────────────────────────────────────────────
# Priority order:
#   1. PICO_SDK_PATH env/cache variable  ->  use existing local SDK
#   2. Neither set                       ->  fetch SDK from GitHub automatically

set(PICO_SDK_IMPORT_CMAKE "${CMAKE_CURRENT_LIST_DIR}/pico_sdk_import.cmake")

if(DEFINED ENV{PICO_SDK_PATH} OR DEFINED PICO_SDK_PATH)
    if(DEFINED ENV{PICO_SDK_PATH})
        include("$ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake")
    else()
        include("${PICO_SDK_PATH}/external/pico_sdk_import.cmake")
    endif()
else()
    if(NOT EXISTS "${PICO_SDK_IMPORT_CMAKE}")
        message(FATAL_ERROR
            "pico_sdk_import.cmake not found.\n"
            "Either:\n"
            "  a) Set PICO_SDK_PATH to your existing Pico SDK, or\n"
            "  b) Run fetch-sdk.bat to download pico_sdk_import.cmake first.")
    endif()
    set(PICO_SDK_FETCH_FROM_GIT ON CACHE BOOL "Fetch Pico SDK from git" FORCE)
    include("${PICO_SDK_IMPORT_CMAKE}")
endif()

# give the project a name (anything you want)
project(vga-pio C CXX ASM)
set(CMAKE_C_STANDARD   11)
set(CMAKE_CXX_STANDARD 17)

# initialize the sdk
pico_sdk_init()

#######

# name anything you want
#add_executable(vga_pio)

set(sources
  main.c
  gui.c
  uart_rx.c
)

# must match with executable name and source file names
#target_sources(vga_pio PRIVATE sources)
add_executable(vga_pio ${sources})

# must match with pio filename and executable name from above
pico_generate_pio_header(vga_pio ${CMAKE_CURRENT_LIST_DIR}/hsync.pio)
pico_generate_pio_header(vga_pio ${CMAKE_CURRENT_LIST_DIR}/vsync.pio)
pico_generate_pio_header(vga_pio ${CMAKE_CURRENT_LIST_DIR}/rgb.pio)

target_include_directories(vga_pio PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}
  ${CMAKE_CURRENT_LIST_DIR}/generated_fonts
)

# must match with executable name
target_link_libraries(vga_pio PRIVATE 
  pico_stdlib 
  hardware_pio 
  hardware_dma
  hardware_uart
)

# must match with executable name
pico_add_extra_outputs(vga_pio)
