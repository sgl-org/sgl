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

# ---------------------------------------------------------------------------
# Example registry for sgl/examples.
#
# Every demo in this directory is listed exactly once in SGL_EXAMPLE_SOURCE, so
# a consumer (the simulator build in the repository root, a downstream project)
# never spells out example paths itself and the list cannot silently drift away
# from the folder contents.  Adding a new example means adding one line here.
#
#   include(<sgl>/examples/build.cmake)
#
#   # a) build every example
#   add_executable(app ${SGL_EXAMPLE_SOURCE} ${SGL_EXAMPLE_EXTRA_SOURCE})
#
#   # b) build a subset, selected by short name (no directory, no ".c")
#   sgl_examples_select(app_examples button label slider)
#   add_executable(app ${app_examples})
#
# sgl_examples_select() also pulls in the extra translation units an example
# needs (SGL_EXAMPLE_EXTRA_SOURCE) and validates every name, so a typo is a
# configure-time error instead of a silently missing widget demo.
#
# Not listed on purpose: polygon.c is a standalone unit test (it carries its own
# main() and needs <check.h>), not a demo -- see demo/Makefile for its purpose.
# ---------------------------------------------------------------------------

set(SGL_EXAMPLE_DIR ${CMAKE_CURRENT_LIST_DIR})
get_filename_component(SGL_EXAMPLE_SGL_DIR ${SGL_EXAMPLE_DIR} DIRECTORY)
get_filename_component(SGL_EXAMPLE_REPO_DIR ${SGL_EXAMPLE_SGL_DIR} DIRECTORY)

set(SGL_EXAMPLE_SOURCE
    # ---- basic widgets ----------------------------------------------------
    ${SGL_EXAMPLE_DIR}/2dball.c             # sgl_2dball_examples
    ${SGL_EXAMPLE_DIR}/analogclock.c        # sgl_analogclock_examples
    ${SGL_EXAMPLE_DIR}/digitalclock.c       # sgl_digitalclock_examples
    ${SGL_EXAMPLE_DIR}/arc.c                # sgl_arc_examples
    ${SGL_EXAMPLE_DIR}/bar.c                # sgl_bar_examples
    ${SGL_EXAMPLE_DIR}/barcode.c            # sgl_barcode_examples
    ${SGL_EXAMPLE_DIR}/battery.c            # sgl_battery_examples
    ${SGL_EXAMPLE_DIR}/button.c             # sgl_button_examples
    ${SGL_EXAMPLE_DIR}/calendar.c           # sgl_calendar_examples
    ${SGL_EXAMPLE_DIR}/checkbox.c           # sgl_checkbox_examples
    ${SGL_EXAMPLE_DIR}/coverflow.c          # sgl_coverflow_examples
    ${SGL_EXAMPLE_DIR}/dropdown.c           # sgl_dropdown_examples
    ${SGL_EXAMPLE_DIR}/img_ext.c            # sgl_img_ext_examples
    ${SGL_EXAMPLE_DIR}/label.c              # sgl_label_examples
    ${SGL_EXAMPLE_DIR}/label_ext.c          # sgl_label_ext_examples
    ${SGL_EXAMPLE_DIR}/launcher.c           # sgl_launcher_examples
    ${SGL_EXAMPLE_DIR}/led.c                # sgl_led_examples
    ${SGL_EXAMPLE_DIR}/menu.c               # sgl_menu_examples
    ${SGL_EXAMPLE_DIR}/msgbox.c             # sgl_msgbox_examples
    ${SGL_EXAMPLE_DIR}/physical_key.c       # sgl_physical_key_handler
    ${SGL_EXAMPLE_DIR}/progress.c           # sgl_progress_examples
    ${SGL_EXAMPLE_DIR}/rect.c               # sgl_rect_examples
    ${SGL_EXAMPLE_DIR}/rect_ext.c           # sgl_rect_ext_examples
    ${SGL_EXAMPLE_DIR}/roller.c             # sgl_roller_examples
    ${SGL_EXAMPLE_DIR}/scope.c              # sgl_scope_examples
    ${SGL_EXAMPLE_DIR}/scrollview.c         # sgl_scrollview_demo
    ${SGL_EXAMPLE_DIR}/slider.c             # sgl_slider_examples
    ${SGL_EXAMPLE_DIR}/span.c               # sgl_span_examples
    ${SGL_EXAMPLE_DIR}/spinner.c            # sgl_spinner_examples
    ${SGL_EXAMPLE_DIR}/spectrum.c           # sgl_spectrum_examples
    ${SGL_EXAMPLE_DIR}/stepper.c            # sgl_stepper_examples
    ${SGL_EXAMPLE_DIR}/switch.c             # sgl_switch_examples
    ${SGL_EXAMPLE_DIR}/tabview.c            # sgl_tabview_examples
    ${SGL_EXAMPLE_DIR}/toast.c              # sgl_toast_examples
    ${SGL_EXAMPLE_DIR}/textedit.c           # sgl_textedit_examples
    ${SGL_EXAMPLE_DIR}/textlist.c           # sgl_textlist_examples
    ${SGL_EXAMPLE_DIR}/viewlist.c           # sgl_viewlist_examples

    # ---- composite widgets ------------------------------------------------
    ${SGL_EXAMPLE_DIR}/filebrowser.c        # sgl_filebrowser_examples
    ${SGL_EXAMPLE_DIR}/filedialog.c         # sgl_filedialog_examples
    ${SGL_EXAMPLE_DIR}/qrcode.c             # sgl_qrcode_examples
    ${SGL_EXAMPLE_DIR}/win.c                # sgl_win_examples
    ${SGL_EXAMPLE_DIR}/charge.c             # sgl_charge_examples

    # ---- examples with external assets (see SGL_EXAMPLE_EXTRA_SOURCE) ------
    ${SGL_EXAMPLE_DIR}/avi.c                # sgl_avi_examples
    ${SGL_EXAMPLE_DIR}/music_player.c       # sgl_music_player_create
    ${SGL_EXAMPLE_DIR}/rect_pixmap.c        # sgl_rect_pixmap_examples
)

# Extra translation units the examples above link against.  Keeping them here
# means a consumer that enables one of these examples can never forget them:
#
#   rect_pixmap.c  -> rect_pixmap_imgs[]   images/output/rect_pixmap_imgs.c
#   music_player.c -> music_img_image[]    images/output/music_img.c
#                     sgl_icon_player32    sgl/demos/music/sgl_icon_player32.c
#
# avi.c plays ${SGL_AVI_DEMO_BASE_DIR}/movie.avi at run time and gif_test.c has
# its GIF embedded, so neither needs an extra translation unit.
set(SGL_EXAMPLE_ASSET_DIR ${SGL_EXAMPLE_REPO_DIR}/images/output)

set(SGL_EXAMPLE_EXTRA_rect_pixmap
    ${SGL_EXAMPLE_ASSET_DIR}/rect_pixmap_imgs.c
)

set(SGL_EXAMPLE_EXTRA_music_player
    ${SGL_EXAMPLE_ASSET_DIR}/music_img.c
    ${SGL_EXAMPLE_SGL_DIR}/demos/music/sgl_icon_player32.c
)

set(SGL_EXAMPLE_EXTRA_SOURCE
    ${SGL_EXAMPLE_EXTRA_rect_pixmap}
    ${SGL_EXAMPLE_EXTRA_music_player}
)

# sgl_examples_select(<out_var> [<name>...])
#
#   <out_var> receives the example list named by the caller, or the complete
#   list when no name is given.  Each example brings its own extra sources
#   (SGL_EXAMPLE_EXTRA_<name>) along.  An unknown name, or an example whose
#   asset file is missing, stops the configure step with a clear message.
function(sgl_examples_select out_var)
    if(ARGC EQUAL 1)
        set(${out_var} ${SGL_EXAMPLE_SOURCE} ${SGL_EXAMPLE_EXTRA_SOURCE} PARENT_SCOPE)
        return()
    endif()

    set(_selected "")
    foreach(_name IN LISTS ARGN)
        set(_file ${SGL_EXAMPLE_DIR}/${_name}.c)
        if(NOT EXISTS ${_file})
            message(FATAL_ERROR
                "sgl_examples_select: no example named '${_name}' "
                "(expected ${_file}) -- see sgl/examples/build.cmake")
        endif()

        list(APPEND _selected ${_file})

        foreach(_extra IN LISTS SGL_EXAMPLE_EXTRA_${_name})
            if(NOT EXISTS ${_extra})
                message(FATAL_ERROR
                    "sgl_examples_select: '${_name}' needs the missing asset "
                    "${_extra} -- see sgl/examples/build.cmake")
            endif()
            list(APPEND _selected ${_extra})
        endforeach()
    endforeach()

    set(${out_var} ${_selected} PARENT_SCOPE)
endfunction()
