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
project(rp2040-vga C CXX ASM)
set(CMAKE_C_STANDARD   11)
set(CMAKE_CXX_STANDARD 17)

# initialize the sdk
pico_sdk_init()

#######

set(FW_DIR ${CMAKE_CURRENT_SOURCE_DIR}/fw)

set(sources
  ${FW_DIR}/main.c
  ${FW_DIR}/gui.c
  ${FW_DIR}/uart_rx.c
)

# must match with executable name and source file names
#target_sources(vga_pio PRIVATE sources)
add_executable(${PROJECT_NAME} ${sources})

# must match with pio filename and executable name from above
pico_generate_pio_header(${PROJECT_NAME} ${FW_DIR}/hsync.pio)
pico_generate_pio_header(${PROJECT_NAME} ${FW_DIR}/vsync.pio)
pico_generate_pio_header(${PROJECT_NAME} ${FW_DIR}/rgb.pio)

target_include_directories(${PROJECT_NAME} PRIVATE
  ${FW_DIR}
  ${FW_DIR}/generated_fonts
)

# must match with executable name
target_link_libraries(${PROJECT_NAME} PRIVATE 
  pico_stdlib 
  hardware_pio 
  hardware_dma
  hardware_uart
)

# must match with executable name
pico_add_extra_outputs(${PROJECT_NAME})
