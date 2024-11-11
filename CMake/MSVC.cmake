################################################################################
# Windows/Visual Studio specifics
################################################################################
message(STATUS "Configuring ${PROJECT_NAME} for Visual Studio")

################################################################################
# Set target arch type if empty. 
################################################################################
if(NOT CMAKE_VS_PLATFORM_NAME)
    set(CMAKE_VS_PLATFORM_NAME "x64")
endif()
message(STATUS "${CMAKE_VS_PLATFORM_NAME} architecture in use")

if(NOT ("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64"))
    message(FATAL_ERROR "${CMAKE_VS_PLATFORM_NAME} arch is not supported!")
endif()

################################################################################	
# Output path depends on configuration
################################################################################
set(NIMAGNA_BASE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/${CMAKE_VS_PLATFORM_NAME}/")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${NIMAGNA_BASE_OUTPUT_DIRECTORY}/$<IF:$<CONFIG:Debug>,Debug,Release>)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${NIMAGNA_BASE_OUTPUT_DIRECTORY}/$<IF:$<CONFIG:Debug>,Debug,Release> )
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${NIMAGNA_BASE_OUTPUT_DIRECTORY}/$<IF:$<CONFIG:Debug>,Debug,Release> )

################################################################################
# Visual Studio specific compiler options
################################################################################

# remove default flags provided with CMake for MSVC
set(CMAKE_CXX_FLAGS "")
set(CMAKE_CXX_FLAGS_DEBUG "")
set(CMAKE_CXX_FLAGS_RELEASE "")

set(NIMAGNA_COMPILE_OPTIONS 
	$<$<CONFIG:Release>:
		/O2			# optimizations: favor speed
		/Oi			# enable intrinsic functions
		/Gy			# enable function level linking
	    /Zi			# program database
	>
    $<$<CONFIG:Debug>:
        /Od			# optimization disabled
        /RTC1		# basic runtime checks
		/ZI			# program database for edit and continue
    >
	/permissive-	# conformance mode
	/MP				# Multiprocessor compilation
	/sdl			# SDL checks
	/W3				# warning level 3
	/WX				# treat warnings as errors
    /EHsc			# enable C++ exceptions
)

################################################################################
# Global compiler definitions 
################################################################################

set(NIMAGNA_COMPILE_DEFINITIONS
	"$<$<CONFIG:Debug>:"
		"_DEBUG"
	">"
	"$<$<CONFIG:Release>:"
		"NDEBUG"
	">"
	"_WINDOWS"
	"_USRDLL"
	"UNICODE"
	"_UNICODE"
	# https://github.com/NimagnaAG/Nimagna-App/issues/1238
	# Visual Studio 17.8 in combination with spdlog/fmt caused compiler warnings
	# See also https://github.com/gabime/spdlog/issues/2912
	# TODO: Change back once the above issue is resolved with an update of spdlog
	"_SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING" 
)

################################################################################
# Global linker options
################################################################################

set(NIMAGNA_LINK_OPTIONS
	$<$<CONFIG:Debug>:
		/INCREMENTAL	# incremental linking
    	/DEBUG			# generate debug information
	>
	$<$<CONFIG:Release>:
		/OPT:REF		# optimize: references
		/OPT:ICF		# optimize: COMDAT folding
		/INCREMENTAL:NO # no incremental linking
		/DEBUG
	>
)

function(CopyDLLsToOutput TargetName)
    # copy over the DLLs to the output folder (only on Windows)
	message(VERBOSE "Adding DLL copy step to ${TargetName}")
	#[[ Debugging: shows the DLLs being copied
    add_custom_command(TARGET ${TargetName} POST_BUILD 
      COMMAND echo 'DLLs: $<TARGET_RUNTIME_DLLS:${TargetName}> $<TARGET_FILE_DIR:${TargetName}>'
      COMMAND_EXPAND_LISTS
    )
	]]
	add_custom_command(TARGET ${TargetName} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:${TargetName}> $<TARGET_FILE_DIR:${TargetName}>
      COMMAND_EXPAND_LISTS
    )
endfunction()
