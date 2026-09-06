/* examples/switch.c
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
 * Switch widget examples:
 *  1. basic green switch (on by default)
 *  2. custom colored switch with white knob
 *  3. semi-transparent switch with larger knob margin
 *
 * Click/tap on the switch to toggle between on / off states.
 */

/**
 * @brief create the switch examples
 * @param parent parent object, NULL creates the switches on the active screen
 * @return none
 */
void sgl_switch_examples(sgl_obj_t *parent)
{
    sgl_obj_t *sw;

    /* example 1: basic green switch, status on (default theme color) */
    sw = sgl_switch_create(parent);
    sgl_obj_set_pos(sw, 710, 65);
    sgl_obj_set_size(sw, 80, 36);
    sgl_switch_set_status(sw, true);           /* start on */
    sgl_switch_set_radius(sw, 18);             /* fully rounded pill shape */

    /* example 2: custom colored switch (blue on, dark bg) */
    sw = sgl_switch_create(parent);
    sgl_obj_set_pos(sw, 710, 115);
    sgl_obj_set_size(sw, 80, 36);
    sgl_switch_set_status(sw, true);
    sgl_switch_set_color(sw, SGL_COLOR_BLUE);
    sgl_switch_set_bg_color(sw, sgl_rgb(30, 30, 40));
    sgl_switch_set_knob_color(sw, SGL_COLOR_WHITE);
    sgl_switch_set_radius(sw, 18);
    sgl_switch_set_border_width(sw, 2);
    sgl_switch_set_border_color(sw, SGL_COLOR_CYAN);

    /* example 3: semi-transparent switch with larger knob margin (off) */
    sw = sgl_switch_create(parent);
    sgl_obj_set_pos(sw, 710, 165);
    sgl_obj_set_size(sw, 80, 36);
    sgl_switch_set_status(sw, false);          /* start off */
    sgl_switch_set_knob_margin(sw, 3);         /* more padding around knob */

    sw = sgl_switch_create(parent);
    sgl_obj_set_pos(sw, 710, 215);
    sgl_obj_set_size(sw, 80, 36);
    sgl_switch_set_radius(sw, 18);
    sgl_switch_set_status(sw, false);          /* start off */
    sgl_switch_set_knob_margin(sw, -8);         /* more padding around knob */
}
