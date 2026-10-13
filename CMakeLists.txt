cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME N10_Lever_Screen_Project)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
    Core/Src/gc9a01.c
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)

# Imported LVGL 8.3.11 and original SquareLine screens/assets.
file(GLOB_RECURSE LVGL_SOURCES CONFIGURE_DEPENDS Middlewares/Third_Party/lvgl/src/*.c)
file(GLOB_RECURSE UI_SOURCES CONFIGURE_DEPENDS Middlewares/Third_Party/squarline_ui/*.c)
set_source_files_properties(${LVGL_SOURCES} ${UI_SOURCES} PROPERTIES COMPILE_OPTIONS "-Os;-g0")
target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${LVGL_SOURCES} ${UI_SOURCES}
    Core/Src/lv_port.c Core/Src/cst816s.c Core/Src/screen_app.c)
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    Middlewares/Third_Party Middlewares/Third_Party/squarline_ui)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE LV_CONF_INCLUDE_SIMPLE)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE Core/Src/tle5014.c)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE Core/Src/rtos_timebase.c)
