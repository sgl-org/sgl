/* examples/rect.c
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
 * Rectangle widget examples:
 *  1. solid color rectangle
 *  2. rounded corners
 *  3. border with different width and color
 *  4. semi-transparent fill and border
 *  5. multiple borders on the same position for visual effect
 */

/**
 * @brief create the rectangle examples
 * @param parent parent object, NULL creates the rectangles on the active screen
 * @return none
 */
void sgl_rect_examples(sgl_obj_t *parent)
{
    sgl_obj_t *rect;

    /* example 1: solid red rectangle */
    rect = sgl_rect_create(parent);
    sgl_obj_set_pos(rect, 280, 35);
    sgl_obj_set_size(rect, 100, 60);
    sgl_rect_set_color(rect, SGL_COLOR_RED);

    /* example 2: rounded corners with green */
    rect = sgl_rect_create(parent);
    sgl_obj_set_pos(rect, 395, 35);
    sgl_obj_set_size(rect, 100, 60);
    sgl_rect_set_color(rect, sgl_rgb(46, 139, 87));
    sgl_rect_set_radius(rect, 15);

    /* example 3: thick blue border */
    rect = sgl_rect_create(parent);
    sgl_obj_set_pos(rect, 280, 115);
    sgl_obj_set_size(rect, 100, 60);
    sgl_rect_set_color(rect, SGL_COLOR_GRAY);
    sgl_rect_set_border_width(rect, 6);
    sgl_rect_set_border_color(rect, SGL_COLOR_BLUE);

    /* example 4: semi-transparent fill and transparent border */
    rect = sgl_rect_create(parent);
    sgl_obj_set_pos(rect, 280, 195);
    sgl_obj_set_size(rect, 100, 60);
    sgl_rect_set_color(rect, sgl_rgb(255, 255, 0));
    sgl_rect_set_main_alpha(rect, 160);            /* fill alpha 160/255 */
    sgl_rect_set_border_width(rect, 4);
    sgl_rect_set_border_color(rect, SGL_COLOR_GREEN);
    sgl_rect_set_border_alpha(rect, 200);          /* border alpha 200/255 */

    /* example 5: two stacked rectangles – bottom one shows layered effect */
    rect = sgl_rect_create(parent);
    sgl_obj_set_pos(rect, 395, 195);
    sgl_obj_set_size(rect, 100, 60);
    sgl_rect_set_color(rect, SGL_COLOR_CYAN);
    sgl_rect_set_main_alpha(rect, 128);
    sgl_rect_set_radius(rect, 10);

    rect = sgl_rect_create(parent);
    sgl_obj_set_pos(rect, 395, 195);
    sgl_obj_set_size(rect, 100, 60);
    sgl_rect_set_color(rect, SGL_COLOR_WHITE);
    sgl_rect_set_main_alpha(rect, 64);
    sgl_rect_set_radius(rect, 10);
}
