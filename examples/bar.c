/* examples/bar.c
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
 * Bar widget examples:
 *  1. horizontal bar with custom fill / track color
 *  2. rounded horizontal bar with a border
 *  3. vertical bar
 *  4. rounded vertical bar with semi-transparent fill
 *
 * Every bar is clickable and movable: press and drag on it to change its
 * value (0 - 100) interactively.
 */

/**
 * @brief create the bar examples
 * @param parent parent object, NULL creates the bars on the active screen
 * @return none
 */
void sgl_bar_examples(sgl_obj_t *parent)
{
    sgl_obj_t *bar;

    /* example 1: horizontal bar, blue fill on a gray track */
    bar = sgl_bar_create(parent);
    sgl_obj_set_pos(bar, 20, 440);
    sgl_obj_set_size(bar, 200, 20);
    sgl_bar_set_direct(bar, SGL_DIRECT_HORIZONTAL);
    sgl_bar_set_fill_color(bar, SGL_COLOR_BLUE);
    sgl_bar_set_track_color(bar, SGL_COLOR_GRAY);
    sgl_bar_set_value(bar, 65);

    /* example 2: rounded horizontal bar with a border */
    bar = sgl_bar_create(parent);
    sgl_obj_set_pos(bar, 240, 440);
    sgl_obj_set_size(bar, 200, 20);
    sgl_bar_set_direct(bar, SGL_DIRECT_HORIZONTAL);
    sgl_bar_set_radius(bar, 10);
    sgl_bar_set_fill_color(bar, sgl_rgb(46, 139, 87));
    sgl_bar_set_track_color(bar, SGL_COLOR_DARK_GRAY);
    sgl_bar_set_border_width(bar, 2);
    sgl_bar_set_border_color(bar, SGL_COLOR_WHITE);
    sgl_bar_set_value(bar, 35);

    /* example 3: vertical bar, red-orange fill */
    bar = sgl_bar_create(parent);
    sgl_obj_set_pos(bar, 470, 425);
    sgl_obj_set_size(bar, 10, 50);
    sgl_bar_set_direct(bar, SGL_DIRECT_VERTICAL);
    sgl_bar_set_fill_color(bar, SGL_COLOR_RED_ORANGE);
    sgl_bar_set_track_color(bar, SGL_COLOR_GRAY);
    sgl_bar_set_value(bar, 80);

    /* example 4: rounded vertical bar, semi-transparent cyan fill */
    bar = sgl_bar_create(parent);
    sgl_obj_set_pos(bar, 510, 425);
    sgl_obj_set_size(bar, 10, 50);
    sgl_bar_set_direct(bar, SGL_DIRECT_VERTICAL);
    sgl_bar_set_radius(bar, 8);
    sgl_bar_set_fill_color(bar, SGL_COLOR_CYAN);
    sgl_bar_set_track_color(bar, SGL_COLOR_DARK_GRAY);
    sgl_bar_set_alpha(bar, 160);
    sgl_bar_set_value(bar, 50);
}
