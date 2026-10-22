/* examples/transit.c
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

#include <stdint.h>

#include <sgl.h>

/*
 * Warp page-transition demo.
 *
 * Two pages are built up front: a "home" page with four buttons and a
 * "detail" page with a single back button.  Every home button jumps to the
 * detail page through one of the four WARP effects (genie out / in and the
 * S-curve out / in variants).  The detail back button plays the matching
 * reverse so you can see the funnel pour back out.
 *
 * The whole animation is driven by sgl_task_handler(): a click only needs to
 * call sgl_screen_load(dest, effect) and the core state machine eases every
 * frame until the destination page is settled.  No extra frame buffer is
 * required -- the library compositor transforms each rendered band on the way
 * to the port's flush_area callback.
 *
 * Requires CONFIG_SGL_ANIM_TRANSITION.
 */

#if (CONFIG_SGL_ANIM_TRANSITION)

static sgl_obj_t         *g_page_home;
static sgl_obj_t         *g_page_detail;
static uint8_t            g_last_trans = SGL_ANIM_TRANS_GENIE_OUT;

/* one full-screen page with a solid background and a centered title */
static sgl_obj_t *transit_page_create(sgl_color_t bg, const char *title)
{
    sgl_obj_t *page = sgl_obj_create(NULL);
    sgl_obj_t *label;

    if (page == NULL) {
        return NULL;
    }

    sgl_page_set_color(page, bg);

    label = sgl_label_create(page);
    sgl_obj_set_size(label, SGL_SCREEN_WIDTH, 44);
    sgl_obj_set_pos(label, 0, 14);
    sgl_label_set_font(label, &consolas32);
    sgl_label_set_text(label, title);
    sgl_label_set_text_color(label, SGL_COLOR_WHITE);
    sgl_label_set_text_align(label, SGL_ALIGN_CENTER);

    return page;
}

/* a caption button wired to cb with the transit type packed into event data */
static sgl_obj_t *transit_button_create(sgl_obj_t *page, int16_t x, int16_t y,
                                        int16_t w, int16_t h, const char *text,
                                        sgl_color_t color,
                                        void (*cb)(sgl_event_t *e), void *data)
{
    sgl_obj_t *btn = sgl_button_create(page);

    sgl_obj_set_pos(btn, x, y);
    sgl_obj_set_size(btn, w, h);
    sgl_button_set_font(btn, &consolas24);
    sgl_button_set_text(btn, text);
    sgl_button_set_color(btn, color);
    sgl_button_set_text_color(btn, SGL_COLOR_WHITE);
    sgl_button_set_radius(btn, 8);
    sgl_obj_set_event_cb(btn, cb, data);

    return btn;
}

/* home buttons: switch to the detail page with the chosen warp effect */
static void transit_go_detail_cb(sgl_event_t *e)
{
    uint8_t type;

    if (e->type != SGL_EVENT_CLICKED) {
        return;
    }

    type = (uint8_t)(intptr_t)e->event_data;
    g_last_trans = type;
    sgl_screen_load(g_page_detail, type);
}

/* detail back button: return home playing the reverse of the last effect */
static void transit_back_home_cb(sgl_event_t *e)
{
    if (e->type != SGL_EVENT_CLICKED) {
        return;
    }
    sgl_screen_load(g_page_home, sgl_anim_trans_reverse(g_last_trans));
}

/**
 * @brief create the warp page-transition demo
 * @param parent unused, the demo always builds its own full-screen pages
 * @return none
 * @note press a button to watch the corresponding genie / S-curve transition
 */
void sgl_transit_warp_examples(sgl_obj_t *parent)
{
    sgl_obj_t *btn;
    sgl_obj_t *label;
    const int16_t bw = 260;
    const int16_t bh = 46;
    const int16_t gap = 16;
    int16_t x, y;

    SGL_UNUSED(parent);

    /* ---- home page: four warp effects ---- */
    g_page_home = transit_page_create(sgl_rgb(28, 54, 96), "Warp Transition");

    x = (SGL_SCREEN_WIDTH - bw) / 2;
    y = 110;
    btn = transit_button_create(g_page_home, x, y, bw, bh, "Genie  Out",
                                sgl_rgb(220, 90, 40), transit_go_detail_cb,
                                (void *)(intptr_t)SGL_ANIM_TRANS_GENIE_OUT);
    y += bh + gap;
    btn = transit_button_create(g_page_home, x, y, bw, bh, "Genie  In",
                                sgl_rgb(40, 160, 90), transit_go_detail_cb,
                                (void *)(intptr_t)SGL_ANIM_TRANS_GENIE_IN);
    y += bh + gap;
    btn = transit_button_create(g_page_home, x, y, bw, bh, "S-curve  Out",
                                sgl_rgb(120, 70, 190), transit_go_detail_cb,
                                (void *)(intptr_t)SGL_ANIM_TRANS_S_OUT);
    y += bh + gap;
    btn = transit_button_create(g_page_home, x, y, bw, bh, "S-curve  In",
                                sgl_rgb(30, 140, 180), transit_go_detail_cb,
                                (void *)(intptr_t)SGL_ANIM_TRANS_S_IN);

    label = sgl_label_create(g_page_home);
    sgl_obj_set_size(label, SGL_SCREEN_WIDTH, 30);
    sgl_obj_set_pos(label, 0, y + gap);
    sgl_label_set_font(label, &consolas23);
    sgl_label_set_text(label, "click a button to suck / pour the page into the dock");
    sgl_label_set_text_color(label, sgl_rgb(200, 210, 230));
    sgl_label_set_text_align(label, SGL_ALIGN_CENTER);

    /* ---- detail page: single back button ---- */
    g_page_detail = transit_page_create(sgl_rgb(20, 24, 34), "Detail");

    label = sgl_label_create(g_page_detail);
    sgl_obj_set_size(label, SGL_SCREEN_WIDTH, 36);
    sgl_obj_set_pos(label, 0, SGL_SCREEN_HEIGHT / 2 - 80);
    sgl_label_set_font(label, &consolas24);
    sgl_label_set_text(label, "You arrived via a warp effect");
    sgl_label_set_text_color(label, sgl_rgb(160, 175, 200));
    sgl_label_set_text_align(label, SGL_ALIGN_CENTER);

    btn = transit_button_create(g_page_detail,
                                (SGL_SCREEN_WIDTH - bw) / 2,
                                SGL_SCREEN_HEIGHT / 2, bw, bh, "Back (reverse)",
                                sgl_rgb(90, 90, 110), transit_back_home_cb, NULL);

    /* show the home page first */
    sgl_screen_load(g_page_home, SGL_ANIM_TRANS_NONE);
}

#endif /* CONFIG_SGL_ANIM_TRANSITION */
