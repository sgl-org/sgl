/* examples/toast.c
 *
 * MIT License
 *
 * Copyright(c) 2023-present All contributors of SGL
 * Document reference link: https://sgl-docs.readthedocs.io
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <sgl.h>

/**
 * Toast widget examples (non-interactive transient notifications):
 *  1. dark classic toast, default 2 s hold / 500 ms fade out, self destroys
 *  2. green success toast with a quick 100 ms fade out and a longer hold
 *  3. wide translucent info toast, holds 12 s so the slow fade out phase is
 *     easy to observe; all three are stacked near the bottom of the screen
 *
 * Every toast appears at once, holds, fades its alpha down to 0 and then
 * removes itself; call sgl_toast_show() again on a live object to restart.
 *
 * Screen is 800x480.
 */

/**
 * @brief create the toast examples
 * @param parent parent object, NULL creates the toasts on the active screen
 * @return none
 */
void sgl_toast_examples(sgl_obj_t *parent)
{
    sgl_obj_t *toast;

    /* example 1: classic dark toast, all default timings */
    toast = sgl_toast_create(parent);
    sgl_obj_set_pos(toast, 280, 360);
    sgl_obj_set_size(toast, 240, 44);
    sgl_toast_set_font(toast, &consolas14);
    sgl_toast_set_text(toast, "Settings saved");
    sgl_toast_set_bg_color(toast, sgl_rgb(40, 40, 40));
    sgl_toast_set_text_color(toast, SGL_COLOR_WHITE);
    sgl_toast_show(toast);

    /* example 2: green success toast, quick fade out, 6 s hold */
    toast = sgl_toast_create(parent);
    sgl_obj_set_pos(toast, 280, 410);
    sgl_obj_set_size(toast, 240, 44);
    sgl_toast_set_font(toast, &consolas14);
    sgl_toast_set_text(toast, "Upload OK");
    sgl_toast_set_bg_color(toast, sgl_rgb(28, 96, 48));
    sgl_toast_set_text_color(toast, SGL_COLOR_WHITE);
    sgl_toast_set_fade_time(toast, 100);
    sgl_toast_set_duration(toast, 6000);
    sgl_toast_show(toast);

    /* example 3: wide translucent info toast, long hold for observation */
    toast = sgl_toast_create(parent);
    sgl_obj_set_pos(toast, 160, 300);
    sgl_obj_set_size(toast, 480, 44);
    sgl_toast_set_font(toast, &consolas14);
    sgl_toast_set_text(toast, "Battery low - connect charger soon");
    sgl_toast_set_bg_color(toast, sgl_rgb(20, 20, 20));
    sgl_toast_set_text_color(toast, SGL_COLOR_YELLOW);
    sgl_toast_set_alpha(toast, 180);
    sgl_toast_set_duration(toast, 12000);
    sgl_toast_show(toast);
}
