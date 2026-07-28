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

if(${SGL_HEAP_ALGO} STREQUAL "tlsf")
    set(SGL_SOURCE
        ${SGL_SOURCE}
        ${CMAKE_CURRENT_LIST_DIR}/tlsf/tlsf.c
        ${CMAKE_CURRENT_LIST_DIR}/tlsf/sgl_mm.c
    )
endif()

if(${SGL_HEAP_ALGO} STREQUAL "lwmem")
    set(SGL_SOURCE
        ${SGL_SOURCE}
        ${CMAKE_CURRENT_LIST_DIR}/lwmem/lwmem.c
        ${CMAKE_CURRENT_LIST_DIR}/lwmem/sgl_mm.c
    )
endif()

if(${SGL_HEAP_ALGO} STREQUAL "bump")
    set(SGL_SOURCE
        ${SGL_SOURCE}
        ${CMAKE_CURRENT_LIST_DIR}/bump/sgl_mm.c
    )
endif()

if(${SGL_HEAP_ALGO} STREQUAL "other")
    set(SGL_SOURCE
        ${SGL_SOURCE}
        ${CMAKE_CURRENT_LIST_DIR}/other/sgl_mm.c
    )
endif()

if(${SGL_HEAP_ALGO} STREQUAL "umm_malloc")
    set(SGL_SOURCE
        ${SGL_SOURCE}
        ${CMAKE_CURRENT_LIST_DIR}/umm_malloc/umm_malloc.c
        ${CMAKE_CURRENT_LIST_DIR}/umm_malloc/umm_info.c
        ${CMAKE_CURRENT_LIST_DIR}/umm_malloc/umm_integrity.c
        ${CMAKE_CURRENT_LIST_DIR}/umm_malloc/umm_poison.c
        ${CMAKE_CURRENT_LIST_DIR}/umm_malloc/sgl_mm.c
    )
endif()
