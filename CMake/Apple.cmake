################################################################################
# MacOS specifics
################################################################################
message(STATUS "Configuring ${PROJECT_NAME} for MacOS")


# Disable adding $(CONFIGURATION)$(EFFECTIVE_PLATFORM_NAME) to library search paths
# https://cmake.org/cmake/help/latest/policy/CMP0142.html
cmake_policy(SET CMP0142 NEW)

if (${CMAKE_OSX_ARCHITECTURES} STREQUAL "arm64")
    # with XCode 15.0, the linker crashed with a segmentation fault. Using the old linker solves this.
    # related apple blog post: https://developer.apple.com/forums/thread/731089?page=2
    message(WARNING "macOS only: Explicitly using classic linker. \nSince XCode 15.0, the standard linker crashes with a segmentation fault. Please check again when updating XCode and remove once this option is not needed anymore.")
    add_link_options("-ld_classic")
endif()

################################################################################
# Set target arch type if empty. 
################################################################################
if(NOT CMAKE_PLATFORM_NAME)
    set(CMAKE_PLATFORM_NAME "macos")
endif()

if (${CMAKE_OSX_ARCHITECTURES} MATCHES "^(arm64|x86_64)$")
    message(STATUS "${CMAKE_PLATFORM_NAME} platform with ${CMAKE_OSX_ARCHITECTURES} architecture in use")
else()
    # platform not defined
    message(FATAL_ERROR "Unindentified macos architecture [either arm64 or x86_64]: ${CMAKE_OSX_ARCHITECTURES}")
    message(FATAL_ERROR "Please configure using `sh Scripts/macos/ConfigureProjectForMacOS.sh`")
endif()

################################################################################
# Output path depends on configuration
################################################################################
set(NIMAGNA_BASE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/${CMAKE_PLATFORM_NAME}/${CMAKE_OSX_ARCHITECTURES}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${NIMAGNA_BASE_OUTPUT_DIRECTORY}/$<IF:$<CONFIG:Debug>,Debug,Release>)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${NIMAGNA_BASE_OUTPUT_DIRECTORY}/$<IF:$<CONFIG:Debug>,Debug,Release>)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${NIMAGNA_BASE_OUTPUT_DIRECTORY}/$<IF:$<CONFIG:Debug>,Debug,Release>)

################################################################################
# Helper function to deploy resource file
################################################################################
function(CopyResourceToBundle TargetName ResourceFile)
  message(VERBOSE "Adding bundle copy step for ${ResourceFile} to ${TargetName}")
  add_custom_command(TARGET ${TargetName} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${ResourceFile}
        $<TARGET_BUNDLE_CONTENT_DIR:${TargetName}>/Resources/
  )
  add_custom_command(TARGET ${TargetName} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E create_symlink
      $<TARGET_BUNDLE_CONTENT_DIR:${TargetName}>/Resources/${ResourceFile}
      $<TARGET_BUNDLE_CONTENT_DIR:${TargetName}>/MacOS/${ResourceFile}
      DEPENDS $<TARGET_BUNDLE_CONTENT_DIR:${TargetName}>/Resources/${ResourceFile}
      COMMENT "mklink .app/Contents/Resources/${ResourceFile} -> .app/Contents/MacOS/${ResourceFile}")
endfunction()


