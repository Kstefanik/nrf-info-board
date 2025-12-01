#*****************************************************************************
# @file    CMakeLists.txt
# @author  Karol Stefanik
# @brief   CMake build configuration for nrf-info-board client (Zephyr RTOS)
#
# This file defines the build system for the nrf-info-board client application.
# It sets the minimum CMake version, finds the Zephyr environment, sets up
# the project, and specifies source files and include directories.
# The application is organized into threads (LTE, MQTT, display) and common
# utilities, and uses Zephyr's build infrastructure.
#
# Copyright (c) 2025 Karol Stefanik
# SPDX-License-Identifier: Apache-2.0
#*****************************************************************************

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(info_board_client)

target_include_directories(app PRIVATE src src/modules src/common)

file(GLOB MODULE_SOURCES "src/modules/*.c")
file(GLOB COMMON_SOURCES "src/utils/*.c")

target_sources(app PRIVATE ${MODULE_SOURCES} ${COMMON_SOURCES})