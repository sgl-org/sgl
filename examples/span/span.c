/* examples/span.c
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
 * Span widget examples (rich inline text, like HTML <span>):
 *  1. mixed fonts / colors / sizes in a single line
 *  2. per-fragment background color (highlight, badge, code style)
 *  3. alignment: left / center / right
 *  4. overflow CLIP  : the tail is simply cut off
 *  5. overflow WRAP  : automatic multi-line layout with line space + indent
 *  6. overflow SCROLL: horizontal marquee, driven by a timer
 *  7. widget level alpha blending
 *
 * All fragments keep only a pointer to their text, so the strings must stay
 * alive while the widget is visible (string literals are fine).
 */

/* marquee offset of example 6, and the owning span / timer */
static int16_t g_span_scroll_offset = 0;

/**
 * @brief marquee timer callback for the scroll mode example
 * @param timer pointer to the timer
 * @param user_data user data, the span object
 * @return none
 */
static void sgl_span_scroll_timer_cb(const sgl_timer_t *timer, void *user_data)
{
    SGL_UNUSED(timer);
    sgl_obj_t *obj = (sgl_obj_t *)user_data;
    sgl_span_t *span = sgl_container_of(obj, sgl_span_t, obj);
    int16_t total_w = sgl_span_get_total_width(obj);
    int16_t view_w = sgl_obj_get_width(obj);
    int16_t max_offset = total_w - view_w;

    if (max_offset <= 0) {
        return;
    }

    /* 2 pixels per tick, ping-pong between both ends */
    g_span_scroll_offset += 2;
    if (g_span_scroll_offset > max_offset) {
        g_span_scroll_offset = 0;
    }

    span->scroll_offset = g_span_scroll_offset;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief create the span examples
 * @param parent parent object, NULL creates the widgets on the active screen
 * @return none
 */
void sgl_span_examples(sgl_obj_t *parent)
{
    sgl_obj_t *span;
    int idx;

    /* example 1: rich text - mixed size / color / weight in one line.
     * This is the typical "price / promotion" look. */
    span = sgl_span_create(parent);
    sgl_obj_set_pos(span, 20, 20);
    sgl_obj_set_size(span, 440, 34);
    sgl_span_add_span(span, "Total ", &consolas23, sgl_rgb(120, 120, 120));
    sgl_span_add_span(span, "$", &consolas14, sgl_rgb(35, 120, 220));
    sgl_span_add_span(span, "128", &consolas32, sgl_rgb(35, 120, 220));
    sgl_span_add_span(span, ".50", &consolas14, sgl_rgb(35, 120, 220));
    sgl_span_add_span(span, "  (tax included)", &consolas14, sgl_rgb(160, 160, 160));

    /* example 2: per-fragment background - inline highlight and badge.
     * sgl_span_set_span_padding() gives the background some breathing room. */
    span = sgl_span_create(parent);
    sgl_obj_set_pos(span, 20, 66);
    sgl_obj_set_size(span, 440, 30);
    sgl_span_add_span(span, "Status: ", &consolas23, sgl_rgb(60, 60, 60));
    idx = sgl_span_add_span_bg(span, " RUNNING ", &consolas23,
                               SGL_COLOR_WHITE, sgl_rgb(38, 160, 90));
    sgl_span_set_span_padding(span, idx, 2, 2);
    sgl_span_add_span(span, "  " , &consolas23, sgl_rgb(60, 60, 60));
    idx = sgl_span_add_span_bg(span, " ERR: 0 ", &consolas23,
                               sgl_rgb(200, 40, 40), sgl_rgb(250, 232, 232));
    sgl_span_set_span_padding(span, idx, 2, 2);

    /* example 3: alignment of the whole fragment list inside the box */
    span = sgl_span_create(parent);
    sgl_obj_set_pos(span, 20, 110);
    sgl_obj_set_size(span, 140, 30);
    sgl_span_add_span(span, "[Left]", &consolas23, sgl_rgb(40, 40, 40));
    sgl_span_set_bg_color(span, sgl_rgb(238, 238, 238));
    sgl_span_set_radius(span, 6);

    span = sgl_span_create(parent);
    sgl_obj_set_pos(span, 170, 110);
    sgl_obj_set_size(span, 140, 30);
    sgl_span_add_span(span, "[Center]", &consolas23, sgl_rgb(40, 40, 40));
    sgl_span_set_align(span, SGL_SPAN_ALIGN_CENTER);
    sgl_span_set_bg_color(span, sgl_rgb(238, 238, 238));
    sgl_span_set_radius(span, 6);

    span = sgl_span_create(parent);
    sgl_obj_set_pos(span, 320, 110);
    sgl_obj_set_size(span, 140, 30);
    sgl_span_add_span(span, "[Right]", &consolas23, sgl_rgb(40, 40, 40));
    sgl_span_set_align(span, SGL_SPAN_ALIGN_RIGHT);
    sgl_span_set_bg_color(span, sgl_rgb(238, 238, 238));
    sgl_span_set_radius(span, 6);

    /* example 4: CLIP - content wider than the box is cut at the border */
    span = sgl_span_create(parent);
    sgl_obj_set_pos(span, 20, 154);
    sgl_obj_set_size(span, 180, 30);
    sgl_span_set_overflow(span, SGL_SPAN_OVERFLOW_CLIP);
    sgl_span_add_span(span, "CLIP: ", &consolas23, sgl_rgb(160, 60, 160));
    sgl_span_add_span(span, "this sentence is far too long for the box",
                      &consolas23, sgl_rgb(80, 80, 80));
    sgl_span_set_bg_color(span, sgl_rgb(248, 240, 250));
    sgl_span_set_radius(span, 4);

    /* example 5: WRAP - automatic line break, 4 px line space, 12 px indent */
    span = sgl_span_create(parent);
    sgl_obj_set_pos(span, 220, 154);
    sgl_obj_set_size(span, 240, 100);
    sgl_span_set_overflow(span, SGL_SPAN_OVERFLOW_WRAP);
    sgl_span_set_line_space(span, 4);
    sgl_span_set_indent(span, 12);
    sgl_span_add_span(span, "WRAP: ", &consolas23, sgl_rgb(200, 120, 0));
    sgl_span_add_span(span, "fragments of different ", &consolas23, sgl_rgb(70, 70, 70));
    sgl_span_add_span(span, "styles ", &consolas14, sgl_rgb(210, 40, 40));
    sgl_span_add_span(span, "flow onto the next line automatically when the "
                            "right edge is reached.", &consolas23, sgl_rgb(70, 70, 70));
    sgl_span_set_bg_color(span, sgl_rgb(252, 248, 235));
    sgl_span_set_radius(span, 4);

    /* example 6: SCROLL - horizontal marquee driven by a 20 ms timer */
    span = sgl_span_create(parent);
    sgl_obj_set_pos(span, 20, 198);
    sgl_obj_set_size(span, 440, 30);
    sgl_span_set_overflow(span, SGL_SPAN_OVERFLOW_SCROLL);
    sgl_span_add_span(span, "SCROLL: ", &consolas23, sgl_rgb(0, 130, 130));
    sgl_span_add_span(span, "long text that keeps moving to the left and wraps "
                            "around when it reaches the end  ", &consolas23,
                      sgl_rgb(70, 70, 70));
    sgl_span_set_bg_color(span, sgl_rgb(232, 246, 246));
    sgl_span_set_radius(span, 4);

    sgl_timer_t *timer = sgl_timer_create();
    if (timer != NULL) {
        /* 20 ms interval, infinite repeat, the span object as user data */
        sgl_timer_setup(timer, sgl_span_scroll_timer_cb, 20, -1, span);
    }

    /* example 7: widget level alpha, plus a faded fragment */
    span = sgl_span_create(parent);
    sgl_obj_set_pos(span, 20, 242);
    sgl_obj_set_size(span, 440, 34);
    sgl_span_add_span(span, "ALPHA: ", &consolas23, sgl_rgb(60, 60, 60));
    idx = sgl_span_add_span(span, "full opacity  ", &consolas23, sgl_rgb(20, 90, 200));
    sgl_span_add_span(span, "half opacity  ", &consolas23, sgl_rgb(20, 90, 200));
    sgl_span_set_span_alpha(span, idx + 1, 128);
    idx = sgl_span_add_span(span, "quarter opacity", &consolas23, sgl_rgb(20, 90, 200));
    sgl_span_set_span_alpha(span, idx, 64);
    sgl_span_set_bg_color(span, sgl_rgb(240, 244, 250));
    sgl_span_set_radius(span, 4);

    /* example 8: dynamic update - clear() then re-add, like a status line */
    span = sgl_span_create(parent);
    sgl_obj_set_pos(span, 20, 284);
    sgl_obj_set_size(span, 440, 28);
    sgl_span_add_span(span, "Rebuilt from scratch: ", &consolas23, sgl_rgb(120, 120, 120));
    sgl_span_clear(span);
    sgl_span_add_span(span, "clear() ", &consolas23, sgl_rgb(170, 0, 120));
    sgl_span_add_span(span, "removes every fragment, then add them again",
                      &consolas14, sgl_rgb(90, 90, 90));
    sgl_span_set_bg_color(span, sgl_rgb(250, 240, 250));
    sgl_span_set_radius(span, 4);
}
