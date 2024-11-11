include(FindPackageHandleStandardArgs)

###################################################################################
# Spdlog module
###################################################################################

set(MODULE_NAME "Spdlog")
set(MODULE_TARGET_NAME NimagnaExtern::${MODULE_NAME})

if(TARGET ${MODULE_TARGET_NAME})
	return()
endif()

set(MODULE_VERSION "spdlog_v1.x_93f59d0")
set(MODULE_BASE_DIR "${CMAKE_CURRENT_LIST_DIR}/spdlog/${MODULE_VERSION}")
set(MODULE_INCLUDE_DIR "${MODULE_BASE_DIR}/include")
set(MODULE_HEADER_FILE_CHECK "Spdlog/spdlog.h")

###################################################################################
# Check existence
###################################################################################

find_path(${MODULE_NAME}_INCLUDE_DIR 
    NAMES ${MODULE_HEADER_FILE_CHECK}
    PATHS ${MODULE_INCLUDE_DIR}
)
# expose
mark_as_advanced(${MODULE_NAME}_INCLUDE_DIR ${MODULE_NAME}_FOUND)
# handle arguments of find_package
find_package_handle_standard_args(
    ${MODULE_NAME}
    REQUIRED_VARS
        "${MODULE_NAME}_INCLUDE_DIR"
)

###################################################################################
# Target
###################################################################################

if(${MODULE_NAME}_FOUND)
    # Create imported target
    add_library(${MODULE_TARGET_NAME} INTERFACE IMPORTED)
    set_target_properties(
        ${MODULE_TARGET_NAME} PROPERTIES 
        INTERFACE_INCLUDE_DIRECTORIES ${MODULE_INCLUDE_DIR}
    )
endif()
