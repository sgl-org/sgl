#
# MIT License
#
# Copyright(c) 2023-present All contributors of SGL
# Document reference link: https://sgl-docs.readthedocs.io
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
#

cmake_minimum_required(VERSION 3.12)
project(sgl VERSION 0.0.1 LANGUAGES C)

include(GNUInstallDirs)
include(CheckIncludeFile)

set(SGL_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)
set(SGL_SOURCE_PATH ${PROJECT_SOURCE_DIR}/source)
set(SGL_HEADER_PATH ${SGL_SOURCE_PATH}/include)
set(SGL_GENERATED_PATH ${PROJECT_BINARY_DIR}/generated)
set(SGL_EXPORT_PATH ${PROJECT_BINARY_DIR}/export)

option(SGL_COLOR16_SWAP "COLOR16_SWAP" OFF)
option(SGL_ANIMATION "ANIMATION" ON)
option(SGL_DEBUG "DEBUG" OFF)
option(SGL_LOG_COLOR "LOG_COLOR" ON)
option(SGL_EXTERNAL_PIXMAP "EXTERNAL_PIXMAP" OFF)
option(SGL_OBJ_USE_NAME "OBJ_USE_NAME" OFF)
option(SGL_BOOT_LOGO "BOOT_LOGO" OFF)
option(SGL_BOOT_ANIMATION "BOOT_ANIMATION" OFF)
option(SGL_FONT_SONG23 "FONT_SONG23" ON)
option(SGL_FONT_CONSOLAS14 "FONT_CONSOLAS14" OFF)
option(SGL_FONT_CONSOLAS23 "FONT_CONSOLAS23" OFF)
option(SGL_FONT_CONSOLAS24 "FONT_CONSOLAS24" OFF)
option(SGL_FONT_CONSOLAS32 "FONT_CONSOLAS32" OFF)
option(SGL_SOFT_TIMER "SOFT_TIMER" OFF)
option(SGL_QRCODE "QRCODE" OFF)


set(SGL_LOG_LEVEL 0)
set(SGL_PANEL_PIXEL_DEPTH "16")
set(SGL_EVENT_QUEUE_SIZE 16)
set(SGL_SYSTICK_MS 10)
set(SGL_DIRTY_AREA_THRESHOLD 64)
set(SGL_HEAP_ALGO lwmem)
set(SGL_HEAP_MEMORY_SIZE 10240)
set(SGL_QRCODE_MAX_VERSION 5)

set(CONFIG_SGL_COLOR16_SWAP ${SGL_COLOR16_SWAP})
set(CONFIG_SGL_ANIMATION ${SGL_ANIMATION})
set(CONFIG_SGL_DEBUG ${SGL_DEBUG})
set(CONFIG_SGL_LOG_COLOR ${SGL_LOG_COLOR})
set(CONFIG_SGL_OBJ_USE_NAME ${SGL_OBJ_USE_NAME})
set(CONFIG_SGL_BOOT_LOGO ${SGL_BOOT_LOGO})
set(CONFIG_SGL_FONT_SONG23 ${SGL_FONT_SONG23})
set(CONFIG_SGL_FONT_CONSOLAS14 ${SGL_FONT_CONSOLAS14})
set(CONFIG_SGL_FONT_CONSOLAS23 ${SGL_FONT_CONSOLAS23})
set(CONFIG_SGL_FONT_CONSOLAS24 ${SGL_FONT_CONSOLAS24})
set(CONFIG_SGL_FONT_CONSOLAS32 ${SGL_FONT_CONSOLAS32})

set(CONFIG_SGL_QRCODE_QR_VERSION_MAX ${SGL_QRCODE_MAX_VERSION})
set(CONFIG_SGL_QRCODE ${SGL_QRCODE})
set(CONFIG_SGL_SOFT_TIMER ${SGL_SOFT_TIMER})

include(${PROJECT_SOURCE_DIR}/build.cmake)

macro(sgl_dependencies target)
    add_dependencies(
        ${target}
        sgl_export
    )
endmacro()

add_custom_target(
    sgl_export
    COMMAND ${CMAKE_COMMAND} -E make_directory
        ${SGL_EXPORT_PATH}
    COMMAND ${CMAKE_COMMAND} -E copy
        ${SGL_GENERATED_PATH}/sgl_config.h
        ${SGL_SOURCE_PATH}/sgl.h
        ${SGL_EXPORT_PATH}
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${SGL_HEADER_PATH}
        ${SGL_EXPORT_PATH}
)

set_property(
    GLOBAL PROPERTY
    "SGL_INCLUDE_DIRS"
    ${SGL_EXPORT_PATH}
)

add_library(sgl_object OBJECT ${SGL_LIBRARY})
sgl_dependencies(sgl_object)
add_library(sgl ALIAS sgl_object)
