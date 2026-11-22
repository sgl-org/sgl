/* examples/2dball.c
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
 * 2DBall widget examples:
 *  1. default theme style ball
 *  2. red ball with dark background
 *  3. blue ball with custom bg color
 *  4. green ball, semi-transparent
 *  5. different sizes (small / medium / large)
 */

/**
 * @brief create the 2dball examples
 * @param parent parent object, NULL creates the balls on the active screen
 * @return none
 */
void sgl_2dball_examples(sgl_obj_t *parent)
{
    sgl_obj_t *ball;

    /* example 1: default theme style ball */
    ball = sgl_2dball_create(parent);
    sgl_obj_set_pos(ball, 20, 360);
    sgl_obj_set_size(ball, 60, 60);

    /* example 2: red ball with dark background */
    ball = sgl_2dball_create(parent);
    sgl_obj_set_pos(ball, 100, 360);
    sgl_obj_set_size(ball, 60, 60);
    sgl_2dball_set_color(ball, SGL_COLOR_RED);
    sgl_2dball_set_bg_color(ball, SGL_COLOR_DARK_GRAY);

    /* example 3: blue ball with navy background */
    ball = sgl_2dball_create(parent);
    sgl_obj_set_pos(ball, 180, 360);
    sgl_obj_set_size(ball, 60, 60);
    sgl_2dball_set_color(ball, SGL_COLOR_BLUE);
    sgl_2dball_set_bg_color(ball, SGL_COLOR_NAVY);

    /* example 4: green ball, semi-transparent */
    ball = sgl_2dball_create(parent);
    sgl_obj_set_pos(ball, 260, 360);
    sgl_obj_set_size(ball, 60, 60);
    sgl_2dball_set_color(ball, SGL_COLOR_GREEN);
    sgl_2dball_set_bg_color(ball, SGL_COLOR_BLACK);
    sgl_2dball_set_alpha(ball, 128);

    /* example 5: different sizes – small, medium, large */
    ball = sgl_2dball_create(parent);
    sgl_obj_set_pos(ball, 350, 390);
    sgl_obj_set_size(ball, 30, 30);
    sgl_2dball_set_color(ball, SGL_COLOR_CYAN);
    sgl_2dball_set_bg_color(ball, SGL_COLOR_BLACK);

    ball = sgl_2dball_create(parent);
    sgl_obj_set_pos(ball, 395, 375);
    sgl_obj_set_size(ball, 50, 50);
    sgl_2dball_set_color(ball, SGL_COLOR_MAGENTA);
    sgl_2dball_set_bg_color(ball, SGL_COLOR_BLACK);

    ball = sgl_2dball_create(parent);
    sgl_obj_set_pos(ball, 460, 355);
    sgl_obj_set_size(ball, 80, 80);
    sgl_2dball_set_color(ball, SGL_COLOR_YELLOW);
    sgl_2dball_set_bg_color(ball, SGL_COLOR_DARK_GRAY);
}
