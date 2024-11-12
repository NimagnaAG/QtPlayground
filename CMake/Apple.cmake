################################################################################
# MacOS specifics
################################################################################
message(STATUS "Configuring ${PROJECT_NAME} for MacOS")

# Disable adding $(CONFIGURATION)$(EFFECTIVE_PLATFORM_NAME) to library search paths
# https://cmake.org/cmake/help/latest/policy/CMP0142.html
cmake_policy(SET CMP0142 NEW)

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

set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGN_STYLE $<IF:$<CONFIG:Debug>,Automatic,Manual>)

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
      COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_BUNDLE_CONTENT_DIR:${TargetName}>/Resources/
  )
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
