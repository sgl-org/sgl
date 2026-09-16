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

set(SGL_SOURCE
    ${SGL_SOURCE}
    ${CMAKE_CURRENT_LIST_DIR}/sgl_draw_line.c
    ${CMAKE_CURRENT_LIST_DIR}/sgl_draw_rect.c
    ${CMAKE_CURRENT_LIST_DIR}/sgl_draw_circle.c
    ${CMAKE_CURRENT_LIST_DIR}/sgl_draw_arc.c
    ${CMAKE_CURRENT_LIST_DIR}/sgl_draw_text.c
    ${CMAKE_CURRENT_LIST_DIR}/sgl_draw_ring.c
    ${CMAKE_CURRENT_LIST_DIR}/sgl_draw_icon.c
    ${CMAKE_CURRENT_LIST_DIR}/sgl_draw.c
)
