/* examples/scrollview.c
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
 * ScrollView widget example:
 *  A vertical scroll container that holds arbitrary child widgets.
 *   - child rows (label + button) are placed freely inside the view
 *   - dragging the view scrolls the content up/down using the shared
 *     sgl_scroll engine (inertia + rubber-band + fade-out scrollbar)
 *   - children keep their relative layout and are clipped to the viewport
 *   - the scrollable range is derived from the children bounds automatically
 */

#define SV_EX_X    (SGL_SCREEN_WIDTH - 250)
#define SV_EX_Y    (60)
#define SV_EX_W    (230)
#define SV_EX_H    (360)
#define SV_EX_ROWS (12)

static char sv_row_txt[SV_EX_ROWS][24];

/**
 * @brief create the scrollview example
 * @param parent parent object, NULL creates the scrollview on the active screen
 * @return none
 */
void sgl_scrollview_demo(sgl_obj_t *parent)
{
    sgl_obj_t *sv;
    sgl_obj_t *child;
    int i;

    sv = sgl_scrollview_create(parent);
    sgl_obj_set_pos(sv, SV_EX_X, SV_EX_Y);
    sgl_obj_set_size(sv, SV_EX_W, SV_EX_H);
    sgl_scrollview_set_bg_color(sv, SGL_COLOR_DARK_GRAY);
    sgl_scrollview_set_border_color(sv, SGL_COLOR_GRAY);
    sgl_scrollview_set_radius(sv, 0);

    /* heterogeneous child rows (label + button) at free positions; the
     * scrollable range is derived from the children bounds automatically */
    for (i = 0; i < SV_EX_ROWS; i++) {
        int16_t y = (int16_t)(8 + i * 60);

        child = sgl_label_create(sv);
        sgl_obj_set_pos(child, 8, y);
        sgl_obj_set_size(child, SV_EX_W - 15, 24);
        sgl_label_set_font(child, &consolas14);
        sgl_label_set_text_color(child, SGL_COLOR_WHITE);
        sgl_label_set_bg_color(child, SGL_COLOR_NAVY);
        sgl_label_set_radius(child, 12);
        sgl_snprintf(sv_row_txt[i], sizeof(sv_row_txt[i]), "ScrollView row %02d", i);
        sgl_label_set_text(child, sv_row_txt[i]);

        child = sgl_button_create(sv);
        sgl_obj_set_pos(child, 8, (int16_t)(y + 28));
        sgl_obj_set_size(child, SV_EX_W - 15, 24);
        sgl_button_set_font(child, &consolas14);
        sgl_button_set_text(child, "Tap me");
        sgl_button_set_radius(child, 12);
    }

    /* a child placed far below the fold to extend the scrollable content */
    child = sgl_button_create(sv);
    sgl_obj_set_pos(child, 10, 800);
    sgl_obj_set_size(child, 100, 26);
    sgl_button_set_font(child, &consolas14);
    sgl_button_set_text(child, "Tap me");
}
