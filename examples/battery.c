/* examples/battery.c
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
 * Battery widget examples:
 *  1. horizontal battery with charging indicator and percentage
 *  2. vertical battery, low level (10%) with red fill color
 *  3. custom colors for different battery levels
 *  4. semi-transparent battery with percentage text hidden
 *
 * All batteries show 3D depth with highlight/shadow effects.
 */

/**
 * @brief create the battery examples
 * @param parent parent object, NULL creates the batteries on the active screen
 * @return none
 */
void sgl_battery_examples(sgl_obj_t *parent)
{
    sgl_obj_t *bat;

    /* example 1: horizontal battery, fully charged (100%), charging indicator */
    bat = sgl_battery_create(parent);
    sgl_obj_set_pos(bat, 50, 60);
    sgl_obj_set_size(bat, 180, 50);
    sgl_battery_set_level(bat, 100);
    sgl_battery_set_charging(bat, true);
    sgl_battery_set_vertical(bat, false);
    sgl_battery_show_percentage(bat, true);
    sgl_battery_set_text_color(bat, SGL_COLOR_GREEN);
    sgl_battery_set_fill_color(bat, SGL_COLOR_GREEN);
    sgl_battery_set_border_color(bat, SGL_COLOR_WHITE);

    /* example 2: vertical battery, low level (10%) — auto-red fill */
    bat = sgl_battery_create(parent);
    sgl_obj_set_pos(bat, 250, 50);
    sgl_obj_set_size(bat, 50, 180);
    sgl_battery_set_level(bat, 10);
    sgl_battery_set_charging(bat, true);
    sgl_battery_set_vertical(bat, true);
    sgl_battery_show_percentage(bat, true);
    sgl_battery_set_font(bat, &consolas14);
    sgl_battery_set_text_color(bat, SGL_COLOR_RED);

    /* example 3: horizontal battery with custom level colors */
    bat = sgl_battery_create(parent);
    sgl_obj_set_pos(bat, 320, 120);
    sgl_obj_set_size(bat, 200, 60);
    sgl_battery_set_level(bat, 35);  /* medium range → default yellow */
    sgl_battery_set_charging(bat, false);
    sgl_battery_set_vertical(bat, false);
    sgl_battery_show_percentage(bat, true);
    sgl_battery_set_low_color(bat, SGL_COLOR_RED_ORANGE);     /* < 20% */
    sgl_battery_set_medium_color(bat, SGL_COLOR_BLUE);        /* 20-50% */
    sgl_battery_set_high_color(bat, SGL_COLOR_MAGENTA);       /* >= 50% */
    sgl_battery_set_bg_color(bat, sgl_rgb(40, 40, 50));       /* dark background */

    /* example 4: horizontal battery, 75%, transparent with rounded corners */
    bat = sgl_battery_create(parent);
    sgl_obj_set_pos(bat, 540, 150);
    sgl_obj_set_size(bat, 160, 45);
    sgl_battery_set_level(bat, 75);
    sgl_battery_set_charging(bat, false);
    sgl_battery_set_vertical(bat, false);
    sgl_battery_show_percentage(bat, false);           /* hide percentage text */
    sgl_battery_set_alpha(bat, 120);                   /* semi-transparent */
    sgl_obj_set_border_width(bat, 3);                  /* thick border */
    sgl_battery_set_border_color(bat, SGL_COLOR_CYAN);
}
