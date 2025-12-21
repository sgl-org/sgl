#
# MIT License
#
# Copyright(c) 2023-present All contributors of SGL
# Document reference link: docs directory
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
    ${CMAKE_CURRENT_LIST_DIR}/line/sgl_line.c
    ${CMAKE_CURRENT_LIST_DIR}/rectangle/sgl_rectangle.c
    ${CMAKE_CURRENT_LIST_DIR}/circle/sgl_circle.c
    ${CMAKE_CURRENT_LIST_DIR}/ring/sgl_ring.c
    ${CMAKE_CURRENT_LIST_DIR}/arc/sgl_arc.c
    ${CMAKE_CURRENT_LIST_DIR}/button/sgl_button.c
    ${CMAKE_CURRENT_LIST_DIR}/slider/sgl_slider.c
    ${CMAKE_CURRENT_LIST_DIR}/progress/sgl_progress.c
    ${CMAKE_CURRENT_LIST_DIR}/label/sgl_label.c
    ${CMAKE_CURRENT_LIST_DIR}/switch/sgl_switch.c
    ${CMAKE_CURRENT_LIST_DIR}/msgbox/sgl_msgbox.c
    ${CMAKE_CURRENT_LIST_DIR}/textline/sgl_textline.c
    ${CMAKE_CURRENT_LIST_DIR}/textbox/sgl_textbox.c
    ${CMAKE_CURRENT_LIST_DIR}/checkbox/sgl_checkbox.c
    ${CMAKE_CURRENT_LIST_DIR}/icon/sgl_icon.c
    ${CMAKE_CURRENT_LIST_DIR}/numberkbd/sgl_numberkbd.c
    ${CMAKE_CURRENT_LIST_DIR}/keyboard/sgl_keyboard.c
    ${CMAKE_CURRENT_LIST_DIR}/led/sgl_led.c
    ${CMAKE_CURRENT_LIST_DIR}/2dball/sgl_2dball.c
    ${CMAKE_CURRENT_LIST_DIR}/unzip_image/sgl_unzip_image.c
    ${CMAKE_CURRENT_LIST_DIR}/scroll/sgl_scroll.c
    ${CMAKE_CURRENT_LIST_DIR}/dropdown/sgl_dropdown.c
    ${CMAKE_CURRENT_LIST_DIR}/scope/sgl_scope.c
    ${CMAKE_CURRENT_LIST_DIR}/polygon/sgl_polygon.c
)
