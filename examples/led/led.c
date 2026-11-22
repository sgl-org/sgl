/* examples/led.c
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
 * LED widget examples (round status indicator with a 3D glossy look):
 *  1. green LED turned on  (sgl_led_on)
 *  2. red LED turned off   (sgl_led_off, distinct dark off color)
 *  3. cyan LED on, semi-transparent with a custom background rim
 *
 * A LED is a passive indicator: it is always drawn as a circle (radius =
 * width / 2) and it does not react to clicks. Drive it from code with
 * sgl_led_set_status() / sgl_led_on() / sgl_led_off().
 * The on state shows on_color, the off state shows off_color.
 */

/**
 * @brief create the led examples
 * @param parent parent object, NULL creates the leds on the active screen
 * @return none
 */
void sgl_led_examples(sgl_obj_t *parent)
{
    sgl_obj_t *led;

    /* example 1: green LED, on */
    led = sgl_led_create(parent);
    sgl_obj_set_pos(led, 215, 254);
    sgl_obj_set_size(led, 30, 30);
    sgl_led_set_on_color(led, SGL_COLOR_GREEN);
    sgl_led_set_off_color(led, sgl_rgb(30, 60, 30));   /* dim green when off */
    sgl_led_on(led);

    /* example 2: red LED, off (dark red off color keeps it visibly "red") */
    led = sgl_led_create(parent);
    sgl_obj_set_pos(led, 215, 288);
    sgl_obj_set_size(led, 30, 30);
    sgl_led_set_on_color(led, SGL_COLOR_RED);
    sgl_led_set_off_color(led, sgl_rgb(70, 25, 25));
    sgl_led_off(led);

    /* example 3: cyan LED, on, semi-transparent with a custom rim color */
    led = sgl_led_create(parent);
    sgl_obj_set_pos(led, 215, 322);
    sgl_obj_set_size(led, 30, 30);
    sgl_led_set_on_color(led, SGL_COLOR_CYAN);
    sgl_led_set_off_color(led, sgl_rgb(20, 40, 50));
    sgl_led_set_bg_color(led, sgl_rgb(15, 15, 20));
    sgl_led_set_alpha(led, 140);
    sgl_led_set_status(led, true);
}
