
if(DEFINED QT_BASE_DIR)
    return()
endif()

###################################################################################
# Qt is expected at a fixed location
###################################################################################

set(QT_VERSION 6.8.0)
set(QT_ROOT_DIR "C:/Qt/${QT_VERSION}/")
set(QT_VARIANT "msvc2022_64")
set(QT_BASE_DIR "${QT_ROOT_DIR}${QT_VARIANT}/")
set(QT_BASE_DIR "${QT_ROOT_DIR}${QT_VARIANT}/" PARENT_SCOPE)
set(QT_INCLUDE_DIR "${QT_BASE_DIR}include/")
set(QT_LIB_DIR "${QT_BASE_DIR}lib/")
set(QT_CMAKE_DIR "${QT_LIB_DIR}cmake/Qt6")

###################################################################################
# Check existence
###################################################################################

find_path(Qt6_DIR 
    NAMES bin/qmake.exe
    PATHS ${QT_BASE_DIR}
)

# setting cmake prefix path allows cmake to find Qt stuff 
# https://stackoverflow.com/questions/15639781/how-to-find-the-qt5-cmake-module-on-windows

set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH};${Qt6_DIR};${QT_BASE_DIR}")

message(STATUS "Using Qt ${QT_VERSION} for Windows at ${QT_BASE_DIR}")
message(VERBOSE "> root: ${QT_ROOT_DIR}")
message(VERBOSE "> base: ${QT_BASE_DIR}")
message(VERBOSE "> include dir: ${QT_INCLUDE_DIR}")
message(VERBOSE "> lib dir: ${QT_LIB_DIR}")
message(VERBOSE "> cmake dir: ${QT_CMAKE_DIR}")
