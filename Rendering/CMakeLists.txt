set(PROJECT_NAME Rendering)

# Disable adding $(CONFIGURATION)$(EFFECTIVE_PLATFORM_NAME) to library search paths
# https://cmake.org/cmake/help/latest/policy/CMP0142.html
cmake_policy(SET CMP0142 NEW)

################################################################################
# Source groups
################################################################################
set(DLL
    "include/Rendering/pch.h"
    "include/Rendering/Rendering.h"
    "src/pch.cpp"
    "src/Rendering.cpp"
)
if(MSVC)
    list(APPEND DLL "src/dllmain.cpp")
endif()
source_group("DLL" FILES ${DLL})

set(Header_Files
    "include/Rendering/Logging.h"
    "include/Rendering/Renderer.h"
    "include/Rendering/RenderObject.h"
    "include/Rendering/RenderData.h"
    "include/Rendering/RenderObjectManager.h"
    "include/Rendering/TextureRenderObject.h"
    "include/Rendering/tiny_gltf.h"
    "include/Rendering/stb_image.h"
    "include/Rendering/stb_image_write.h"
    "include/Rendering/json.hpp"
)
source_group("Header Files" FILES ${Header_Files})

set(Source_Files
    "src/Renderer.cpp"
    "src/Logging.cpp"
    "src/RenderObject.cpp"
    "src/RenderData.cpp"
    "src/RenderObjectManager.cpp"
    "src/TextureRenderObject.cpp"
)
source_group("Source Files" FILES ${Source_Files})

set(ALL_FILES
    ${DLL}
    ${Header_Files}
    ${Source_Files}
)


################################################################################
# Project 
################################################################################
project(${PROJECT_NAME})

################################################################################
# Target
################################################################################
include(UseQt)
add_library(${PROJECT_NAME} SHARED ${ALL_FILES})
set_target_properties(${PROJECT_NAME} PROPERTIES
    FOLDER "API"
	INTERPROCEDURAL_OPTIMIZATION_RELEASE "TRUE"
)

find_package(Qt6 
    COMPONENTS 
        Core 
        Multimedia 
        MultimediaWidgets 
        OpenGL
    REQUIRED
)
find_package(Spdlog REQUIRED)

################################################################################
# Public: include directories
################################################################################
target_include_directories(${PROJECT_NAME} PUBLIC
	"${CMAKE_CURRENT_SOURCE_DIR}/include"
)

################################################################################
# Private: compile and link settings
################################################################################

# platform dependent libraries to link
if (MSVC)
    set(PLATFORM_LIBRARY_DEPENDENCIES 
        PRIVATE glu32         # opengl rendering
        PRIVATE opengl32      # opengl rendering
    )   
    string(TOUPPER "${PROJECT_NAME}" TARGET_NAME_CAPITALIZED)
    set(PROJECT_COMPILE_DEFINITIONS "${TARGET_NAME_CAPITALIZED}_EXPORTS")    # export DLL macro
    target_precompile_headers(${PROJECT_NAME} PRIVATE
        "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/include/${PROJECT_NAME}/pch.h>"
    )
elseif (APPLE)
    find_package(OpenGL REQUIRED)
    find_package(GLUT REQUIRED)
    target_include_directories(${PROJECT_NAME} PRIVATE ${OPENGL_INCLUDE_DIR} ${GLUT_INCLUDE_DIR})
    set(PLATFORM_LIBRARY_DEPENDENCIES
        PRIVATE ${GLUT_LIBRARIES}
        PRIVATE ${OPENGL_LIBRARIES})
endif()

set(LIBRARY_DEPENDENCIES 
    ${PLATFORM_LIBRARY_DEPENDENCIES}
    PRIVATE Qt::Core
    PRIVATE Qt::Multimedia
    PRIVATE Qt::MultimediaWidgets
    PRIVATE Qt::OpenGL
    PRIVATE NimagnaExtern::Spdlog
)
target_compile_definitions(${PROJECT_NAME} 
    PRIVATE ${NIMAGNA_COMPILE_DEFINITIONS}
    PRIVATE ${PROJECT_COMPILE_DEFINITIONS}
)
target_compile_options(${PROJECT_NAME} PRIVATE ${NIMAGNA_COMPILE_OPTIONS})
target_link_options(${PROJECT_NAME} PRIVATE ${NIMAGNA_LINK_OPTIONS})
target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBRARY_DEPENDENCIES})
