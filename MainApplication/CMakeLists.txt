set(PROJECT_NAME "MainApplication")

# Disable adding $(CONFIGURATION)$(EFFECTIVE_PLATFORM_NAME) to library search paths
# https://cmake.org/cmake/help/latest/policy/CMP0142.html
cmake_policy(SET CMP0142 NEW)

################################################################################
# Source groups
################################################################################

set(Header_Files
    "include/MainWindow.h"
    "include/OpenGlWidget.h"
    "include/LoggerMainApplication.h"
    "include/pch.h"
)
source_group("Header Files" FILES ${Header_Files})

if (MSVC)
    list(APPEND Resource_Files "MainApplication.rc")
endif()
source_group("Resource Files" FILES ${Resource_Files})

set(Source_Files
    "src/main.cpp"
    "src/MainWindow.cpp"
    "src/LoggerMainApplication.cpp"
    "src/OpenGlWidget.cpp"
    "src/pch.cpp"
)
source_group("Source Files" FILES ${Source_Files})


set(Ui_Files
    "ui/MainWindow.ui"
)
source_group("UI Files" FILES ${Ui_Files})

set(Additional_Files
    "../README.md"
    "../.gitignore"
)
source_group("Additional Files" FILES ${Additional_Files})

set(Additional_Files_Cmake
    "../Cmake/Apple.cmake"
    "../CMake/MSVC.cmake"
)
source_group("Additional Files/CMake" FILES ${Additional_Files_Cmake})


set(ALL_FILES
    ${Header_Files}
    ${Resource_Files}
    ${Source_Files}
    ${Ui_Files}
    ${Additional_Files_Cmake}
)

################################################################################
# Target
################################################################################
include(UseQt)
if (MSVC)
    add_executable(${PROJECT_NAME} WIN32 ${ALL_FILES})
    set_target_properties(${PROJECT_NAME} PROPERTIES 
        FOLDER "Application"
        INTERPROCEDURAL_OPTIMIZATION_RELEASE "TRUE"
    )
    # run with console in debug mode
    set(PROJECT_LINK_OPTIONS $<$<CONFIG:Debug>:/SUBSYSTEM:CONSOLE>)
elseif (APPLE)
    add_executable(${PROJECT_NAME} MACOSX_BUNDLE ${ALL_FILES} )
    set_target_properties(${PROJECT_NAME} PROPERTIES
        OUTPUT_NAME ${PROJECT_NAME}
        FOLDER "Application"
        INTERPROCEDURAL_OPTIMIZATION_RELEASE "TRUE"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_LIST_DIR}/MainApplicationInfo.plist"
    )
endif()

if(MSVC)
    # copy DLLs of dependencies
    CopyDLLsToOutput(${PROJECT_NAME})
    # copy Qt resources
    target_link_libraries(${PROJECT_NAME} INTERFACE Qt_CopyPlugins)
    add_dependencies(
        ${PROJECT_NAME} 
        Qt_CopyPlugins 
    )
endif()

################################################################################
# macOS dependencies
################################################################################

if (APPLE)
    # Copy resource files to bundle and create symlink
    CopyResourceToBundle(${PROJECT_NAME} "Base.rcc")              
endif()

################################################################################
# Dependencies
################################################################################
add_dependencies(${PROJECT_NAME}
    Rendering
)
find_package(Qt6 
    COMPONENTS 
        Core 
        Gui 
        Multimedia 
        MultimediaWidgets 
        Widgets 
        QuickWidgets
        Svg
        Concurrent
        OpenGL
        OpenGLWidgets
    REQUIRED
)
find_package(Spdlog REQUIRED)

################################################################################
# Resources
################################################################################

# The resource file: Resources.qrc
qt_add_binary_resources(${PROJECT_NAME}_Resource_RCC Resources.qrc DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${PROJECT_NAME}.rcc)
set_target_properties(${PROJECT_NAME}_Resource_RCC PROPERTIES FOLDER "Utilities")

add_dependencies(${PROJECT_NAME} 
    ${PROJECT_NAME}_Resource_RCC
)

################################################################################
# Private: include directories, compile and link settings
################################################################################

target_include_directories(${PROJECT_NAME} PRIVATE
	"${CMAKE_CURRENT_SOURCE_DIR}/include"
)
if (MSVC)
    target_precompile_headers(${PROJECT_NAME} PRIVATE
        "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/include/pch.h>"
    )
endif()

# note: no private/public here since this is an exe
set(LIBRARY_DEPENDENCIES 
    ${PLATFORM_LIBRARY_DEPENDENCIES}
    Qt::Core
    Qt::Gui
    Qt::Multimedia
    Qt::MultimediaWidgets
    Qt::Widgets
    Qt::QuickWidgets
    Qt::Svg
    Qt::Concurrent
    Qt::OpenGL
    Qt::OpenGLWidgets
    NimagnaExtern::Spdlog
    Rendering
)

################################################################################
# Compiler and linker settings
################################################################################
target_compile_definitions(${PROJECT_NAME} PRIVATE "${NIMAGNA_COMPILE_DEFINITIONS}")
target_compile_options(${PROJECT_NAME} PRIVATE ${NIMAGNA_COMPILE_OPTIONS})
target_link_options(${PROJECT_NAME} PRIVATE ${NIMAGNA_LINK_OPTIONS}${PROJECT_LINK_OPTIONS})
target_link_libraries(${PROJECT_NAME} PRIVATE "${LIBRARY_DEPENDENCIES}")
