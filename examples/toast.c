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
 * Toast widget examples (android style stacked transient notifications):
 *  1. classic dark toast, default 3 s hold / 1 s fade out, self destroys
 *  2. green success toast, shown right below the first one
 *  3. longer info toast with a 6 s hold, sits at the bottom of the stack
 *
 * All toasts are auto-sized to their text, centered at the top edge of the
 * screen and stacked in creation order. When the first toast starts fading
 * out, the ones below slide up into the freed slot; every toast destroys
 * itself once its alpha reached 0.
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

    /* example 1: classic dark toast, all default timings (3 s + 1 s fade) */
    toast = sgl_toast_create(parent);
    sgl_toast_set_font(toast, &consolas14);
    sgl_toast_set_text(toast, "Settings saved");
    sgl_toast_set_duration(toast, 3000);
    sgl_toast_show(toast);

    /* example 2: green success toast, stacked below the first one */
    toast = sgl_toast_create(parent);
    sgl_toast_set_font(toast, &consolas14);
    sgl_toast_set_text(toast, "Upload OK");
    sgl_toast_set_bg_color(toast, sgl_rgb(28, 96, 48));
    sgl_toast_set_text_color(toast, SGL_COLOR_WHITE);
    sgl_toast_set_duration(toast, 5000);
    sgl_toast_show(toast);

    /* example 3: long info toast with a 6 s hold, bottom of the stack */
    toast = sgl_toast_create(parent);
    sgl_toast_set_font(toast, &consolas14);
    sgl_toast_set_text(toast, "Battery low - connect charger soon");
    sgl_toast_set_bg_color(toast, sgl_rgb(20, 20, 20));
    sgl_toast_set_text_color(toast, SGL_COLOR_YELLOW);
    sgl_toast_set_duration(toast, 6000);
    sgl_toast_show(toast);
}
