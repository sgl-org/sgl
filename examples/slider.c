/* examples/slider.c
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
 * Slider widget examples:
 *  1. horizontal slider (value 0-100, custom colors)
 *  2. vertical slider (value 50, thick knob)
 *  3. thin horizontal slider with red fill on gray track
 *
 * Click/drag on the track to change value; grab the knob and move it.
 */

/**
 * @brief create the slider examples
 * @param parent parent object, NULL creates the sliders on the active screen
 * @return none
 */
void sgl_slider_examples(sgl_obj_t *parent)
{
    sgl_obj_t *sl;

    /* example 1: horizontal slider, blue fill, cyan knob, initial value = 75 */
    sl = sgl_slider_create(parent);
    sgl_obj_set_pos(sl, 20, 60);
    sgl_obj_set_size(sl, 300, 20);
    sgl_slider_set_direct(sl, SGL_DIRECT_HORIZONTAL);
    sgl_slider_set_fill_color(sl, SGL_COLOR_BLUE);
    sgl_slider_set_knob_color(sl, SGL_COLOR_CYAN);
    sgl_slider_set_track_color(sl, sgl_rgb(60, 60, 80));
    sgl_slider_set_value(sl, 75);
    sgl_slider_set_radius(sl, 10);

    /* example 2: vertical slider, green fill, yellow knob, thick knob */
    sl = sgl_slider_create(parent);
    sgl_obj_set_pos(sl, 340, 60);
    sgl_obj_set_size(sl, 20, 180);
    sgl_slider_set_direct(sl, SGL_DIRECT_VERTICAL);
    sgl_slider_set_fill_color(sl, SGL_COLOR_GREEN);
    sgl_slider_set_knob_color(sl, SGL_COLOR_YELLOW);
    sgl_slider_set_track_color(sl, sgl_rgb(60, 60, 80));
    sgl_slider_set_value(sl, 50);
    sgl_slider_set_thickness(sl, 8);

    /* example 3: thin horizontal slider, red fill on gray track */
    sl = sgl_slider_create(parent);
    sgl_obj_set_pos(sl, 370, 60);
    sgl_obj_set_size(sl, 300, 20);
    sgl_slider_set_direct(sl, SGL_DIRECT_HORIZONTAL);
    sgl_slider_set_fill_color(sl, SGL_COLOR_RED_ORANGE);
    sgl_slider_set_knob_color(sl, SGL_COLOR_WHITE);
    sgl_slider_set_track_color(sl, SGL_COLOR_GRAY);
    sgl_slider_set_value(sl, 35);
    sgl_slider_set_thickness(sl, 3);
}
