# tested with 3.24 only (if earlier versions work, too, adapt to "3.16....3.24" or similar)
cmake_minimum_required(VERSION 3.24 FATAL_ERROR)

# MacOS: Minimum target platform. Must be set before any project invocation
set(CMAKE_OSX_DEPLOYMENT_TARGET 12.3 CACHE STRING "Minimal MacOS target platform")
set(MACOSX_BUNDLE_GUI_IDENTIFIER "com.nimagna.QtPlayground")

#[=============================================================================[
    Project
]=============================================================================]

get_filename_component(ProjectId ${CMAKE_CURRENT_SOURCE_DIR} NAME)
string(REPLACE " " "_" ProjectId ${ProjectId})
project(
    "QtPlayground - ${ProjectId}" 
    LANGUAGES CXX)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# extend search path for include
list(APPEND CMAKE_MODULE_PATH 
    # to let include find modules in the CMake folder
    "${CMAKE_SOURCE_DIR}/CMake" 
)

# Globals with global compiler settings
include("Globals")

#[=============================================================================[
    Platform specific settings
]=============================================================================]

if(MSVC)
    include("MSVC")
    list(APPEND CMAKE_MODULE_PATH 
        "${CMAKE_SOURCE_DIR}/CMake/platform/win" 
    )
endif()
if(APPLE)
    include("Apple")
    list(APPEND CMAKE_MODULE_PATH 
        "${CMAKE_SOURCE_DIR}/CMake/platform/mac" 
    )
endif()

#[=============================================================================[
    Libraries/DLLs
]=============================================================================]
add_subdirectory(Rendering)

#[=============================================================================[
    Applications
]=============================================================================]
add_subdirectory(MainApplication)
