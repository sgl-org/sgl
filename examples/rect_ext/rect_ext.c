/* examples/rect_ext.c
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

/* reuse the 48x48 RGB565 icon defined in launcher.c */
extern const sgl_pixmap_t app_icon;

/**
 * Extended rectangle widget examples:
 *  1. different radius on each corner
 *  2. asymmetric corners with border
 *  3. pixmap fill with rounded corners
 *  4. semi-transparent fill with asymmetric corners
 */

/**
 * @brief create the extended rectangle examples
 * @param parent parent object, NULL creates the rectangles on the active screen
 * @return none
 */
void sgl_rect_ext_examples(sgl_obj_t *parent)
{
    sgl_obj_t *rect;

    /* example 1: different radius on each corner (top-left, top-right, bottom-left, bottom-right) */
    rect = sgl_rect_ext_create(parent);
    sgl_obj_set_pos(rect, 725, 258);
    sgl_obj_set_size(rect, 68, 48);
    sgl_rect_ext_set_color(rect, sgl_rgb(46, 139, 87));
    sgl_rect_ext_set_radius(rect, 20, 0, 20, 0);

    /* example 2: asymmetric corners with thick border */
    rect = sgl_rect_ext_create(parent);
    sgl_obj_set_pos(rect, 725, 312);
    sgl_obj_set_size(rect, 68, 48);
    sgl_rect_ext_set_color(rect, SGL_COLOR_GRAY);
    sgl_rect_ext_set_radius(rect, 0, 18, 0, 18);
    sgl_rect_ext_set_border_width(rect, 0);
    sgl_rect_ext_set_border_color(rect, SGL_COLOR_BLUE);

    /* example 3: pixmap fill with rounded corners */
    rect = sgl_rect_ext_create(parent);
    sgl_obj_set_pos(rect, 725, 366);
    sgl_obj_set_size(rect, 68, 48);
    sgl_rect_ext_set_pixmap(rect, &app_icon);
    sgl_rect_ext_set_radius(rect, 12, 12, 12, 12);

    /* example 4: semi-transparent fill and border with asymmetric corners */
    rect = sgl_rect_ext_create(parent);
    sgl_obj_set_pos(rect, 725, 420);
    sgl_obj_set_size(rect, 68, 48);
    sgl_rect_ext_set_color(rect, SGL_COLOR_RED_ORANGE);
    sgl_rect_ext_set_main_alpha(rect, 160);            /* fill alpha 160/255 */
    sgl_rect_ext_set_radius(rect, 16, 16, 0, 0);
    sgl_rect_ext_set_border_width(rect, 3);
    sgl_rect_ext_set_border_color(rect, SGL_COLOR_GREEN);
    sgl_rect_ext_set_border_alpha(rect, 200);          /* border alpha 200/255 */
}
