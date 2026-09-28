#
# Copyright 2014-2015 Ettus Research LLC
# Copyright 2018 Ettus Research, a National Instruments Company
#
# SPDX-License-Identifier: GPL-3.0-or-later
#

cmake_minimum_required(VERSION 3.5.1)
project(cli)
find_package(UHD 4.1.0 REQUIRED)
find_library(READLINE_LIBRARY NAMES readline REQUIRED)

### Configure Compiler ########################################################
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_INSTALL_PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/..)

set(UHD_BOOST_REQUIRED_COMPONENTS
    program_options
    system
    thread
)
set(BOOST_MIN_VERSION 1.65)
include(UHDBoost)

# need these include and link directories for the build
include_directories(
    ${Boost_INCLUDE_DIRS}
    ${UHD_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
link_directories(${Boost_LIBRARY_DIRS})

file(GLOB CPPS "*.cpp")
file(GLOB HDRS "*.h")
add_executable(${PROJECT_NAME} ${CPPS} ${HDRS})

if(NOT UHD_USE_STATIC_LIBS)
    target_link_libraries(cli
        ${UHD_LIBRARIES}
        ${Boost_LIBRARIES}
        ${READLINE_LIBRARY}
    )
else()
    target_link_libraries(cli
        ${UHD_STATIC_LIB_LINK_FLAG}
        ${UHD_STATIC_LIB_DEPS}
        ${READLINE_LIBRARY}
    )
endif()

install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
