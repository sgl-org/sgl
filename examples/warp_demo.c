/* examples/warp_demo.c
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
 * Warp transition showcase.
 *
 * Two content-rich pages (a coloured checker grid + a big letter) are swapped
 * through the four WARP effects (genie out / in and the S-curve out / in
 * variants).  The dense grid lines make the funnel squeeze and the bilinear
 * horizontal resampling easy to judge, which a plain text page hides.
 *
 * It runs hands-free: a repeating timer keeps flipping to the other page using
 * the next effect in the list, so you can just watch the loop.  The four
 * buttons at the bottom let you fire a specific effect on demand instead.
 *
 * Requires CONFIG_SGL_ANIM_TRANSITION.
 */

#if (CONFIG_SGL_ANIM_TRANSITION)

#define WARP_COLS       10
#define WARP_ROWS       5
#define WARP_AUTO_MS    1600u   /* comfortably longer than the 320 ms warp */

static sgl_obj_t   *g_page[2];      /* 0 = page A, 1 = page B */
static int          g_cur;          /* index of the page currently shown */
static sgl_timer_t *g_auto_timer;

static const uint8_t g_fx_list[] = {
    SGL_ANIM_TRANS_GENIE_OUT,
    SGL_ANIM_TRANS_GENIE_IN,
    SGL_ANIM_TRANS_S_OUT,
    SGL_ANIM_TRANS_S_IN,
};

/* flip to the other page playing the given warp effect */
static void warp_go(uint8_t type)
{
    g_cur ^= 1;
    sgl_screen_load(g_page[g_cur], type);
}

/* auto-cycle callback: play the next effect in the list */
static void warp_auto_cb(const sgl_timer_t *timer, void *user_data)
{
    static uint8_t idx;

    SGL_UNUSED(timer);
    SGL_UNUSED(user_data);

    warp_go(g_fx_list[idx]);
    idx = (uint8_t)((idx + 1u) % (sizeof(g_fx_list) / sizeof(g_fx_list[0])));
}

/* button handler: the wanted effect id is packed into event_data */
static void warp_btn_cb(sgl_event_t *e)
{
    if (e->type != SGL_EVENT_CLICKED) {
        return;
    }
    warp_go((uint8_t)(intptr_t)e->event_data);
}

/* a small caption button wired to warp_btn_cb with the effect id as payload */
static void warp_button_create(sgl_obj_t *page, int16_t x, int16_t y,
                               int16_t w, int16_t h, const char *text,
                               sgl_color_t color, uint8_t fx)
{
    sgl_obj_t *btn = sgl_button_create(page);

    sgl_obj_set_pos(btn, x, y);
    sgl_obj_set_size(btn, w, h);
    sgl_button_set_font(btn, &consolas23);
    sgl_button_set_text(btn, text);
    sgl_button_set_color(btn, color);
    sgl_button_set_text_color(btn, SGL_COLOR_WHITE);
    sgl_button_set_radius(btn, 8);
    sgl_obj_set_event_cb(btn, warp_btn_cb, (void *)(intptr_t)fx);
}

/**
 * @brief build one checker-grid page with a big letter and the effect buttons
 * @param letter  the large centred glyph ("A" / "B")
 * @param c0,c1   the two alternating checker colours
 * @param accent  colour used for the corner cells and the button strip
 * @return the page object, or NULL on allocation failure
 */
static sgl_obj_t *warp_page_create(const char *letter, sgl_color_t c0,
                                   sgl_color_t c1, sgl_color_t accent)
{
    sgl_obj_t *page = sgl_obj_create(NULL);
    sgl_obj_t *label;
    const int16_t gx = 40;
    const int16_t gy = 66;
    const int16_t gw = SGL_SCREEN_WIDTH - 80;
    const int16_t gh = 300;
    const int16_t cw = gw / WARP_COLS;
    const int16_t ch = gh / WARP_ROWS;
    int16_t bx, bw, by;

    if (page == NULL) {
        return NULL;
    }
    sgl_page_set_color(page, sgl_rgb(14, 18, 28));

    /* dense checker grid: the many straight lines expose the funnel warp */
    for (int r = 0; r < WARP_ROWS; r++) {
        for (int c = 0; c < WARP_COLS; c++) {
            sgl_obj_t *cell = sgl_rect_create(page);
            sgl_color_t col = ((r + c) & 1) ? c0 : c1;

            if ((r == 0 && c == 0) || (r == WARP_ROWS - 1 && c == WARP_COLS - 1)) {
                col = accent;
            }
            sgl_obj_set_pos(cell, (int16_t)(gx + c * cw + 2), (int16_t)(gy + r * ch + 2));
            sgl_obj_set_size(cell, (int16_t)(cw - 4), (int16_t)(ch - 4));
            sgl_rect_set_radius(cell, 4);
            sgl_rect_set_color(cell, col);
        }
    }

    /* big letter overlaid on the grid centre */
    label = sgl_label_create(page);
    sgl_obj_set_size(label, SGL_SCREEN_WIDTH, 120);
    sgl_obj_set_pos(label, 0, gy + gh / 2 - 60);
    sgl_label_set_font(label, &consolas32);
    sgl_label_set_text(label, letter);
    sgl_label_set_text_color(label, SGL_COLOR_WHITE);
    sgl_label_set_text_align(label, SGL_ALIGN_CENTER);

    /* title */
    label = sgl_label_create(page);
    sgl_obj_set_size(label, SGL_SCREEN_WIDTH, 40);
    sgl_obj_set_pos(label, 0, 14);
    sgl_label_set_font(label, &consolas32);
    sgl_label_set_text(label, "Warp Showcase");
    sgl_label_set_text_color(label, sgl_rgb(210, 220, 240));
    sgl_label_set_text_align(label, SGL_ALIGN_CENTER);

    /* bottom button strip: fire a specific effect on demand */
    by = gy + gh + 12;
    bw = 178;
    bx = (SGL_SCREEN_WIDTH - (bw * 4 + 12 * 3)) / 2;
    warp_button_create(page, bx, by, bw, 44, "Genie Out",
                       sgl_rgb(220, 90, 40), SGL_ANIM_TRANS_GENIE_OUT);
    warp_button_create(page, (int16_t)(bx + (bw + 12)), by, bw, 44, "Genie In",
                       sgl_rgb(40, 160, 90), SGL_ANIM_TRANS_GENIE_IN);
    warp_button_create(page, (int16_t)(bx + 2 * (bw + 12)), by, bw, 44, "S-curve Out",
                       sgl_rgb(120, 70, 190), SGL_ANIM_TRANS_S_OUT);
    warp_button_create(page, (int16_t)(bx + 3 * (bw + 12)), by, bw, 44, "S-curve In",
                       sgl_rgb(30, 140, 180), SGL_ANIM_TRANS_S_IN);

    return page;
}

/**
 * @brief create the warp transition showcase and start the auto-cycle
 * @param parent unused, the demo builds its own two full-screen pages
 * @return none
 * @note the timer keeps flipping pages through the four warp effects; click a
 *       button to play a chosen effect immediately.
 */
void sgl_warp_demo(sgl_obj_t *parent)
{
    SGL_UNUSED(parent);

    g_page[0] = warp_page_create("A", sgl_rgb(34, 66, 128), sgl_rgb(48, 92, 168),
                                 sgl_rgb(255, 150, 40));
    g_page[1] = warp_page_create("B", sgl_rgb(22, 96, 74), sgl_rgb(32, 128, 96),
                                 sgl_rgb(236, 72, 132));
    if (g_page[0] == NULL || g_page[1] == NULL) {
        return;
    }

    g_cur = 0;
    sgl_screen_load(g_page[0], SGL_ANIM_TRANS_NONE);

    g_auto_timer = sgl_timer_create();
    if (g_auto_timer != NULL) {
        sgl_timer_setup(g_auto_timer, warp_auto_cb, WARP_AUTO_MS, -1, NULL);
    }
}

#endif /* CONFIG_SGL_ANIM_TRANSITION */
