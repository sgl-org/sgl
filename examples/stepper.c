/* examples/stepper.c
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
 * Stepper widget examples ([-] value [+] numeric adjuster):
 *  1. basic integer stepper, range 0-100, step 1
 *  2. one-decimal stepper with wrap mode (0.0 - 10.0)
 *  3. custom colored stepper with a larger step
 *
 * Click the left [-] / right [+] buttons to decrease / increase the value.
 * The value is stored as fixed-point: real = value / 10^decimals.
 */

/**
 * @brief create the stepper examples
 * @param parent parent object, NULL creates the steppers on the active screen
 * @return none
 */
void sgl_stepper_examples(sgl_obj_t *parent)
{
    sgl_obj_t *st;

    /* example 1: basic integer stepper (0 - 100, step 1) */
    st = sgl_stepper_create(parent);
    sgl_obj_set_pos(st, 255, 350);
    sgl_obj_set_size(st, 130, 40);
    sgl_stepper_set_font(st, &consolas14);
    sgl_stepper_set_range(st, 0, 100);
    sgl_stepper_set_step(st, 1);
    sgl_stepper_set_value(st, 50);

    /* example 2: one-decimal stepper with wrap (0.0 - 10.0, step 0.5) */
    st = sgl_stepper_create(parent);
    sgl_obj_set_pos(st, 400, 350);
    sgl_obj_set_size(st, 130, 40);
    sgl_stepper_set_font(st, &consolas14);
    sgl_stepper_set_decimals(st, 1);
    sgl_stepper_set_range(st, 0, 100);      /* 0.0 - 10.0 */
    sgl_stepper_set_step(st, 5);            /* 0.5 */
    sgl_stepper_set_wrap(st, 1);            /* wrap around at the boundaries */
    sgl_stepper_set_value(st, 25);          /* 2.5 */

    /* example 3: custom colored stepper with a larger step (0 - 1000, step 50) */
    st = sgl_stepper_create(parent);
    sgl_obj_set_pos(st, 560, 430);
    sgl_obj_set_size(st, 160, 40);
    sgl_stepper_set_font(st, &consolas14);
    sgl_stepper_set_range(st, 0, 1000);
    sgl_stepper_set_step(st, 50);
    sgl_stepper_set_value(st, 300);
    sgl_stepper_set_bg_color(st, sgl_rgb(30, 40, 55));
    sgl_stepper_set_btn_color(st, sgl_rgb(50, 70, 100));
    sgl_stepper_set_text_color(st, SGL_COLOR_YELLOW);
    sgl_stepper_set_sign_color(st, SGL_COLOR_CYAN);
    sgl_stepper_set_border_color(st, SGL_COLOR_BLUE);
    sgl_stepper_set_radius(st, 12);
}
