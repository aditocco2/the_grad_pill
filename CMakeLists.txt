##################################################################################

# Configure stuff here then compile your program :thumbs_up:

set(main_source_file "main.c")
set(dependencies "hub75.c" "hub75.pio") # c/pio files only

set(include_paths "src" "drivers")

set(enable_stdio_uart 0)
set(enable_stdio_usb 1)

set(libraries 
    #FreeRTOS-Kernel-Heap4
    FreeRTOS-Kernel-Static
    pico_stdlib
    hardware_pio
    )

set(PICO_SDK_PATH C:/Users/pluh/.pico-sdk )
set(FREERTOS_KERNEL_PATH C:/Users/pluh/.FreeRTOS-Kernel)

##################################################################################


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
cmake_minimum_required(VERSION 3.14)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(PICO_BOARD pico2 CACHE STRING "Board type")

include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)
include(${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2350_ARM_NTZ/FreeRTOS_Kernel_import.cmake)

project(main C CXX ASM)

pico_sdk_init()

# Build Dependencies:
# Loop over all combos of directory & file
set(all_c_files "")
set(all_pio_files "")
set(file_base_names ${main_source_file} ${dependencies})
foreach(dir ${include_paths})
    foreach(file ${file_base_names})
        # Add C files to a list so add_executable can reference it below
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${dir}/${file}" AND file MATCHES "\\.c$")
            set(all_c_files ${all_c_files} "${dir}/${file}")
        endif()
        # Generate PIO headers for PIO files
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${dir}/${file}" AND file MATCHES "\\.pio$")
            set(all_pio_files ${all_pio_files} "${dir}/${file}")
        endif()
    endforeach()
endforeach()

message(STATUS "Files to compile: ${all_c_files}")

# Add executable with C files
add_executable(main ${all_c_files})

# Generate all PIO headers
foreach(file ${all_pio_files})
    pico_generate_pio_header(main "${CMAKE_CURRENT_LIST_DIR}/${file}")
endforeach()

pico_enable_stdio_uart(main ${enable_stdio_uart})
pico_enable_stdio_usb(main ${enable_stdio_usb})

target_link_libraries(main
    ${libraries}
    )

target_include_directories(main PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
    ${include_paths}
)

pico_add_extra_outputs(main)