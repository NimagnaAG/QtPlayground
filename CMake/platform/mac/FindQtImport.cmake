
if(DEFINED Qt6_DIR)
    return()
endif()

set(QT_VERSION "6.8.0")
file(REAL_PATH "~/Qt/${QT_VERSION}/macos" QT_BASE_DIR EXPAND_TILDE)
	
if (NOT EXISTS "${QT_BASE_DIR}")
    message(FATAL_ERROR "Qt6 not found in ${QT_BASE_DIR}!")
    message(FATAL_ERROR "Use installer from https://www.qt.io/download-qt-installer-oss to install Qt version ${QT_VERSION} into the default folder.")
    exit()
endif()

set(QT_INCLUDE_DIR "${QT_BASE_DIR}/include")
set(QT_LIB_DIR "${QT_BASE_DIR}/lib")
set(QT_CMAKE_DIR "${QT_LIB_DIR}/cmake/Qt6")

###################################################################################
# Check existence
###################################################################################

find_path(QT_DIR 
    NAMES bin/qmake
    PATHS ${QT_BASE_DIR}
)

# setting cmake prefix path allows cmake to find Qt stuff : https://stackoverflow.com/questions/15639781/how-to-find-the-qt5-cmake-module-on-windows
set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH};${QT_BASE_DIR}")

message(STATUS "Using Qt ${QT_VERSION} for MacOS at ${QT_BASE_DIR}")
message(VERBOSE "> base: ${QT_BASE_DIR}")
message(VERBOSE "> include dir: ${QT_INCLUDE_DIR}")
message(VERBOSE "> lib dir: ${QT_LIB_DIR}")
message(VERBOSE "> cmake dir: ${QT_CMAKE_DIR}")
