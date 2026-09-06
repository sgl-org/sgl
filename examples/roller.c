/* examples/roller.c
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
 * Roller widget examples (vertical scroll selector):
 *  1. basic roller with static options (5 items, visible rows = 3)
 *  2. infinite roller (continuous loop when scrolling past boundaries)
 *  3. custom colored roller with larger visible rows (5 rows)
 *
 * Scroll by dragging the roller vertically; tap on an item to select it.
 * The selected item is highlighted with selected_color inside the visible area.
 */

/**
 * @brief create the roller examples
 * @param parent parent object, NULL creates the rollers on the active screen
 * @return none
 */
void sgl_roller_examples(sgl_obj_t *parent)
{
    sgl_obj_t *rl;
    const char *cities = "Beijing\nShanghai\nGuangzhou\nShenzhen\nChengdu\nHangzhou";
    const char *fruits = "Apple\nBanana\nOrange\nMango\nKiwi\nPeach\nGrape";

    /* example 1: basic roller (5 items, visible rows = 3 by default) */
    rl = sgl_roller_create(parent);
    sgl_obj_set_pos(rl, 500, 80);
    sgl_obj_set_size(rl, 100, 150);
    sgl_roller_set_text_font(rl, &consolas14);
    sgl_roller_set_option_static(rl, cities);
    sgl_roller_set_visible_rows(rl, 3);
    sgl_roller_set_selected_color(rl, sgl_rgb(46, 167, 218));

    /* example 2: infinite roller (continuous loop when scrolling) */
    rl = sgl_roller_create(parent);
    sgl_obj_set_pos(rl, 500, 245);
    sgl_obj_set_size(rl, 100, 150);
    sgl_roller_set_text_font(rl, &consolas14);
    sgl_roller_set_option_dynamic(rl, fruits);
    sgl_roller_set_visible_rows(rl, 5);
    sgl_roller_set_infinite_mode(rl, true);
    sgl_roller_set_bg_color(rl, sgl_rgb(30, 40, 55));
    sgl_roller_set_text_color(rl, SGL_COLOR_YELLOW);
    sgl_roller_set_selected_color(rl, SGL_COLOR_GREEN);

    /* example 3: custom colored roller with large visible rows (5) */
    rl = sgl_roller_create(parent);
    sgl_obj_set_pos(rl, 620, 245);
    sgl_obj_set_size(rl, 100, 180);
    sgl_roller_set_text_font(rl, &consolas14);
    sgl_roller_set_option_static(rl, cities);
    sgl_roller_set_visible_rows(rl, 5);
    sgl_roller_set_radius(rl, 12);
    sgl_roller_set_border_width(rl, 2);
    sgl_roller_set_border_color(rl, SGL_COLOR_CYAN);
    sgl_roller_set_selected_color(rl, SGL_COLOR_MAGENTA);
}
