/* examples/rect_pixmap.c
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
 * Rect widget with pixmap (image) examples.
 *
 * Images are converted from images/image1.png and images/image2.png with
 * sgl_image_conv_cli (RGB565, no compression):
 *   image_conv images/image1.png images/image2.png -f RGB565 -c none -O c \
 *       -n rect_pixmap_imgs -d images/output
 *
 * The generated file images/output/rect_pixmap_imgs.c defines:
 *   - image1_bitmap[180000] / image2_bitmap[180000]  (raw RGB565 data)
 *   - rect_pixmap_imgs[2]                            (sgl_pixmap_t array)
 */

extern const sgl_pixmap_t rect_pixmap_imgs[2];

/**
 * @brief create the rectangle-with-pixmap examples
 *
 * Two 300x300 rectangles are created, each one filled with one of the
 * converted pictures. One rect keeps its border, the other one hides it,
 * so the pixmap area is fully visible.
 *
 * @param parent parent object, NULL creates the rectangles on the active screen
 * @return none
 */
void sgl_rect_pixmap_examples(sgl_obj_t *parent)
{
    sgl_obj_t *rect;

    /* example 1: rectangle filled with image1 (RGB565, uncompressed) */
    rect = sgl_rect_create(parent);
    sgl_obj_set_pos(rect, 30, 30);
    sgl_obj_set_size(rect, rect_pixmap_imgs[0].width, rect_pixmap_imgs[0].height);
    sgl_rect_set_pixmap(rect, &rect_pixmap_imgs[0]);

    /* example 2: rectangle filled with image2, border removed */
    rect = sgl_rect_create(parent);
    sgl_obj_set_pos(rect, 360, 30);
    sgl_obj_set_size(rect, rect_pixmap_imgs[1].width, rect_pixmap_imgs[1].height);
    sgl_obj_set_border_width(rect, 0);
    sgl_rect_set_pixmap(rect, &rect_pixmap_imgs[1]);
}
