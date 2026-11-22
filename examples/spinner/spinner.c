/* examples/spinner.c
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
 * Spinner widget examples (rotating loading indicators made of dots):
 *  1. default spinner: 12 dots, radius auto-derived from the object box,
 *     one turn per 1.2 s
 *  2. chunky 8-dot red spinner, fast (0.8 s per turn)
 *  3. fine 20-dot cyan spinner, slow (2.4 s per turn)
 *  4. explicit inner/outer radius and a custom alpha (semi transparent head)
 *
 * All spinners are started with sgl_spinner_start(); use sgl_spinner_stop()
 * to freeze them and sgl_spinner_set_angle() to drive them manually.
 *
 * Screen is 800x480, spinners are laid out in one row.
 */

/**
 * @brief create the spinner examples
 * @param parent parent object, NULL creates the spinners on the active screen
 * @return none
 */
void sgl_spinner_examples(sgl_obj_t *parent)
{
    sgl_obj_t *spinner;

    /* example 1: default 12-dot spinner, auto radius, 1.2 s per turn
     * (color set explicitly because the theme color is invisible on the
     * light desktop background) */
    spinner = sgl_spinner_create(parent);
    sgl_obj_set_pos(spinner, 40, 170);
    sgl_obj_set_size(spinner, 140, 140);
    sgl_spinner_set_color(spinner, SGL_COLOR_BLUE);
    sgl_spinner_start(spinner);

    /* example 2: 8 chunky dots, red, fast */
    spinner = sgl_spinner_create(parent);
    sgl_obj_set_pos(spinner, 230, 190);
    sgl_obj_set_size(spinner, 100, 100);
    sgl_spinner_set_seg_count(spinner, 8);
    sgl_spinner_set_color(spinner, SGL_COLOR_RED);
    sgl_spinner_set_period(spinner, 800);
    sgl_spinner_start(spinner);

    /* example 3: 20 small dots, cyan, slow */
    spinner = sgl_spinner_create(parent);
    sgl_obj_set_pos(spinner, 380, 150);
    sgl_obj_set_size(spinner, 180, 180);
    sgl_spinner_set_seg_count(spinner, 15);
    sgl_spinner_set_color(spinner, SGL_COLOR_GREEN);
    sgl_spinner_set_period(spinner, 2400);
    sgl_spinner_start(spinner);

    /* example 4: explicit ring geometry and a translucent head */
    spinner = sgl_spinner_create(parent);
    sgl_obj_set_pos(spinner, 620, 190);
    sgl_obj_set_size(spinner, 100, 100);
    sgl_spinner_set_radius(spinner, 28, 44);
    sgl_spinner_set_color(spinner, SGL_COLOR_YELLOW);
    sgl_spinner_set_alpha(spinner, 200);
    sgl_spinner_start(spinner);
}
