/* examples/label.c
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
 * Label widget examples:
 *  1. plain text label
 *  2. background color and rounded corners
 *  3. text alignment (left / center / right)
 *  4. formatted text written into a user provided buffer
 *  5. long text scrolling (marquee) mode
 *  6. translucent label
 */

/* user buffer of example 4, avoids dynamic allocation on update */
static char g_label_fmt_buf[24] = {0};

/**
 * @brief create the label examples
 * @param parent parent object, NULL creates the labels on the active screen
 * @return none
 */
void sgl_label_examples(sgl_obj_t *parent)
{
    sgl_obj_t *label;

    /* example 1: plain text label, transparent background */
    label = sgl_label_create(parent);
    sgl_obj_set_pos(label, 340, 40);
    sgl_obj_set_size(label, 200, 28);
    sgl_label_set_font(label, &consolas23);
    sgl_label_set_text(label, "Plain label");
    sgl_label_set_text_color(label, SGL_COLOR_WHITE);
    sgl_label_set_text_align(label, SGL_ALIGN_LEFT_MID);

    /* example 2: background color and rounded corners */
    label = sgl_label_create(parent);
    sgl_obj_set_pos(label, 340, 80);
    sgl_obj_set_size(label, 200, 28);
    sgl_label_set_font(label, &consolas23);
    sgl_label_set_text(label, "Styled label");
    sgl_label_set_text_color(label, SGL_COLOR_WHITE);
    sgl_label_set_text_align(label, SGL_ALIGN_CENTER);
    sgl_label_set_bg_color(label, sgl_rgb(21, 94, 160));
    sgl_label_set_radius(label, 6);

    /* example 3: the same box width, three alignments */
    label = sgl_label_create(parent);
    sgl_obj_set_pos(label, 340, 120);
    sgl_obj_set_size(label, 85, 28);
    sgl_label_set_font(label, &consolas23);
    sgl_label_set_text(label, "Left");
    sgl_label_set_text_color(label, SGL_COLOR_YELLOW);
    sgl_label_set_text_align(label, SGL_ALIGN_LEFT_MID);
    sgl_label_set_bg_color(label, SGL_COLOR_GRAY);
    sgl_label_set_radius(label, 4);

    label = sgl_label_create(parent);
    sgl_obj_set_pos(label, 430, 120);
    sgl_obj_set_size(label, 85, 28);
    sgl_label_set_font(label, &consolas23);
    sgl_label_set_text(label, "Center");
    sgl_label_set_text_color(label, SGL_COLOR_YELLOW);
    sgl_label_set_text_align(label, SGL_ALIGN_CENTER);
    sgl_label_set_bg_color(label, SGL_COLOR_GRAY);
    sgl_label_set_radius(label, 4);

    label = sgl_label_create(parent);
    sgl_obj_set_pos(label, 520, 120);
    sgl_obj_set_size(label, 85, 28);
    sgl_label_set_font(label, &consolas23);
    sgl_label_set_text(label, "Right");
    sgl_label_set_text_color(label, SGL_COLOR_YELLOW);
    sgl_label_set_text_align(label, SGL_ALIGN_RIGHT_MID);
    sgl_label_set_bg_color(label, SGL_COLOR_GRAY);
    sgl_label_set_radius(label, 4);

    /* example 4: formatted text into a user buffer, later updates
     * just call sgl_label_set_text_fmt() again, no allocation */
    label = sgl_label_create(parent);
    sgl_obj_set_pos(label, 340, 160);
    sgl_obj_set_size(label, 200, 28);
    sgl_label_set_font(label, &consolas23);
    sgl_label_set_text_color(label, SGL_COLOR_GREEN);
    sgl_label_set_text_align(label, SGL_ALIGN_LEFT_MID);
    sgl_label_set_text_buffer(label, g_label_fmt_buf, sizeof(g_label_fmt_buf));
    sgl_label_set_text_fmt(label, "value = %d", 1234);

    /* example 5: text wider than the box scrolls automatically */
    label = sgl_label_create(parent);
    sgl_obj_set_pos(label, 340, 200);
    sgl_obj_set_size(label, 200, 28);
    sgl_label_set_font(label, &consolas23);
    sgl_label_set_text(label, "This long text scrolls around in marquee mode");
    sgl_label_set_text_color(label, SGL_COLOR_CYAN);
    sgl_label_set_text_align(label, SGL_ALIGN_LEFT_MID);
    sgl_label_set_bg_color(label, sgl_rgb(32, 32, 32));
    sgl_label_set_radius(label, 4);
#if CONFIG_SGL_ANIMATION
    sgl_label_set_long_mode(label, 40, true);   /* 40 pixel / s */
#endif

    /* example 6: translucent label on top of other content */
    label = sgl_label_create(parent);
    sgl_obj_set_pos(label, 340, 240);
    sgl_obj_set_size(label, 200, 28);
    sgl_label_set_font(label, &consolas23);
    sgl_label_set_text(label, "Alpha 128");
    sgl_label_set_text_color(label, SGL_COLOR_WHITE);
    sgl_label_set_text_align(label, SGL_ALIGN_CENTER);
    sgl_label_set_bg_color(label, SGL_COLOR_RED_ORANGE);
    sgl_label_set_radius(label, 6);
    sgl_label_set_alpha(label, 128);
}
