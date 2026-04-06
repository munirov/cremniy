# SPDX-License-Identifier: MIT
# Author: Diego Iastrubni <diegoiast@gmail.com>

cmake_minimum_required(VERSION 3.24)

project(codoterm LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)
if(UNIX AND NOT APPLE)
    find_package(Qt6 COMPONENTS DBus)
endif()

include(FetchContent)


###################################################
# Fetch libvterm
FetchContent_Declare(
  libvterm
  GIT_REPOSITORY https://github.com/neovim/libvterm.git
  GIT_TAG        934bc2fbf21800ac3458a499df8820ca5fb45fd3
)
FetchContent_MakeAvailable(libvterm)

# Manually define the vterm library since it lacks CMakeLists.txt
file(GLOB VTERM_SOURCES
    "${libvterm_SOURCE_DIR}/src/*.c"
)

add_library(vterm STATIC ${VTERM_SOURCES})
target_include_directories(vterm PUBLIC
    "${libvterm_SOURCE_DIR}/include"
    "${libvterm_SOURCE_DIR}/src"
)

###################################################
# Main library

set(KODOTERM_SOURCES
    src/KodoTerm.cpp
    src/KodoTermConfig.cpp
    include/KodoTerm/KodoTerm.hpp
    src/PtyProcess.cpp
    src/PtyProcess.h
)

if(UNIX)
    list(APPEND KODOTERM_SOURCES
        src/PtyProcess_unix.cpp
        src/PtyProcess_unix.h
    )
    # Check for util library (needed for forkpty on some systems)
    #find_library(UTIL_LIB util)
elseif(WIN32)
    list(APPEND KODOTERM_SOURCES
        src/PtyProcess_win.cpp
        src/PtyProcess_win.h
    )
endif()
add_library(KodoTerm ${KODOTERM_SOURCES})
add_library(KodoTerm::KodoTerm ALIAS KodoTerm)
target_include_directories(KodoTerm PRIVATE src)
target_include_directories(KodoTerm PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(KodoTerm PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    vterm
)
