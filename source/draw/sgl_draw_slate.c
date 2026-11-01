/* source/draw/sgl_draw_slate.c
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

#include <sgl_core.h>
#include <sgl_draw.h>
#include <sgl_math.h>

/**
 * @brief draw a Slate style flat window chrome (caption + body + outline)
 * @param surf point to surface
 * @param area area of rectangle that you want to draw
 * @param rect point to rectangle that you want to draw
 * @param desc window chrome description
 * @return none
 */
void sgl_draw_slate_window(sgl_surf_t *surf, sgl_area_t *area, sgl_area_t *rect, const sgl_draw_slate_window_t *desc)
{
    sgl_area_t title = *rect;
    int16_t title_h = desc->title_h;

    if (desc->alpha == SGL_ALPHA_MIN) {
        return;
    }

    sgl_draw_fill_rect(surf, area, rect, 0, desc->body_color, desc->alpha);

    if (title_h > rect->y2 - rect->y1) {
        title_h = (int16_t)(rect->y2 - rect->y1);
    }

    title.y2 = (int16_t)(title.y1 + title_h);
    sgl_draw_fill_rect(surf, area, &title, 0, desc->title_color, desc->alpha);

    if (desc->border_width) {
        sgl_draw_fill_rect_border(surf, area, rect, 0, desc->border_color, desc->border_width, desc->alpha);
    }
}

typedef struct {
    uint8_t     has_bg;
    sgl_color_t bg;
    sgl_color_t glyph;
} slate_btn_pal_t;

static void slate_btn_palette(sgl_slate_glyph_t glyph, sgl_slate_state_t state, slate_btn_pal_t *pal)
{
    int close = (glyph == SGL_SLATE_BTN_CLOSE);

    pal->has_bg = (state != SGL_SLATE_STATE_NORMAL);

    if (state == SGL_SLATE_STATE_NORMAL) {
        pal->bg    = SGL_COLOR_BLACK;
        pal->glyph = sgl_rgb(0x1A, 0x1A, 0x1A);
    }
    else if (close) {
        pal->bg    = (state == SGL_SLATE_STATE_HOVER) ? sgl_rgb(0xE8, 0x11, 0x23)
                                                      : sgl_rgb(0xF1, 0x70, 0x7A);
        pal->glyph = SGL_COLOR_WHITE;
    }
    else {
        pal->bg    = (state == SGL_SLATE_STATE_HOVER) ? sgl_rgb(0xE5, 0xE5, 0xE5)
                                                      : sgl_rgb(0x99, 0x99, 0x99);
        pal->glyph = (state == SGL_SLATE_STATE_HOVER) ? sgl_rgb(0x1A, 0x1A, 0x1A)
                                                      : SGL_COLOR_WHITE;
    }
}

static void slate_btn_glyph(sgl_surf_t *surf, sgl_area_t *area, sgl_area_t *rect, sgl_slate_glyph_t glyph, sgl_color_t color, uint8_t alpha)
{
    int w = rect->x2 - rect->x1 + 1;
    int h = rect->y2 - rect->y1 + 1;
    int cx = rect->x1 + w / 2;
    int cy = rect->y1 + h / 2;
    int gs = sgl_min(w, h) / 3;
    sgl_area_t box;
    int16_t y, r;

    if (gs < 2) {
        gs = 2;
    }

    switch (glyph) {
    case SGL_SLATE_BTN_MIN:
        y = (int16_t)(rect->y2 - sgl_max(h / 4, 2));
        sgl_draw_fill_hline(surf, area, y, (int16_t)(cx - gs), (int16_t)(cx + gs), 1, color, alpha);
        break;
    case SGL_SLATE_BTN_MAX:
        r = (int16_t)gs;
        box.x1 = (int16_t)(cx - r);
        box.y1 = (int16_t)(cy - r);
        box.x2 = (int16_t)(cx + r);
        box.y2 = (int16_t)(cy + r);
        sgl_draw_wireframe(surf, area, &box, 0, 1, color, alpha);
        break;
    case SGL_SLATE_BTN_RESTORE: {
        int s   = gs;
        int off = sgl_max(2, s / 2);
        int fx1 = cx - s, fy1 = cy - s + off, fx2 = cx + s - off, fy2 = cy + s;
        int bx2 = cx + s, by1 = cy - s;
        box.x1 = (int16_t)fx1;
        box.y1 = (int16_t)fy1;
        box.x2 = (int16_t)fx2;
        box.y2 = (int16_t)fy2;
        sgl_draw_wireframe(surf, area, &box, 0, 1, color, alpha);
        sgl_draw_fill_hline(surf, area, (int16_t)by1, (int16_t)(fx1 + off), (int16_t)bx2, 1, color, alpha);
        sgl_draw_fill_vline(surf, area, (int16_t)bx2, (int16_t)by1, (int16_t)(fy2 - off), 1, color, alpha);
        break;
    }
    case SGL_SLATE_BTN_CLOSE:
    default:
        r = (int16_t)(gs * 2 / 3);        /* keep the X a bit smaller than the box */
        if (r < 2) {
            r = 2;
        }
        sgl_draw_cross(surf, area, (int16_t)(cx - r), (int16_t)(cy - r), r, color, 1, alpha);
        break;
    }
}

/**
 * @brief draw a Slate style flat caption button (solid background + glyph)
 * @param surf point to surface
 * @param area area of rectangle that you want to draw
 * @param rect button bounding box
 * @param glyph which glyph to stamp (min / max / restore / close)
 * @param state visual state (normal / hover / pressed)
 * @param alpha alpha of the button
 * @return none
 */
void sgl_draw_slate_caption_btn(sgl_surf_t *surf, sgl_area_t *area, sgl_area_t *rect, sgl_slate_glyph_t glyph, sgl_slate_state_t state, uint8_t alpha)
{
    slate_btn_pal_t pal;

    if (alpha == SGL_ALPHA_MIN) {
        return;
    }

    if ((uint32_t)state >= SGL_SLATE_STATE_MAX) {
        state = SGL_SLATE_STATE_NORMAL;
    }
    if ((uint32_t)glyph >= SGL_SLATE_GLYPH_MAX) {
        glyph = SGL_SLATE_BTN_CLOSE;
    }

    slate_btn_palette(glyph, state, &pal);

    if (pal.has_bg) {
        sgl_draw_fill_rect(surf, area, rect, 0, pal.bg, alpha);
    }
    slate_btn_glyph(surf, area, rect, glyph, pal.glyph, alpha);
}

static void slate_scroll_arrow(sgl_surf_t *surf, sgl_area_t *area, int16_t cx, int16_t cy, int a, int dir, sgl_color_t color, uint8_t alpha)
{
    int i;
    int span = 2 * a;

    if (a < 1) {
        a = 1;
        span = 2;
    }

    for (i = 0; i <= span; i++) {
        if (dir == 0) {
            int half = (i * a) / span;
            sgl_draw_fill_hline(surf, area, (int16_t)(cy - a + i), (int16_t)(cx - half), (int16_t)(cx + half), 1, color, alpha);
        }
        else if (dir == 1) {
            int half = ((span - i) * a) / span;
            sgl_draw_fill_hline(surf, area, (int16_t)(cy - a + i), (int16_t)(cx - half), (int16_t)(cx + half), 1, color, alpha);
        }
        else if (dir == 2) {
            int half = (i * a) / span;
            sgl_draw_fill_vline(surf, area, (int16_t)(cx - a + i), (int16_t)(cy - half), (int16_t)(cy + half), 1, color, alpha);
        }
        else {
            int half = ((span - i) * a) / span;
            sgl_draw_fill_vline(surf, area, (int16_t)(cx - a + i), (int16_t)(cy - half), (int16_t)(cy + half), 1, color, alpha);
        }
    }
}

/**
 * @brief draw a Slate style flat scrollbar (track + thumb + optional arrows)
 * @param surf point to surface
 * @param area area of rectangle that you want to draw
 * @param rect scrollbar bounding box; the cross axis of rect is the bar thickness
 * @param desc scrollbar description
 * @return none
 */
void sgl_draw_slate_scrollbar(sgl_surf_t *surf, sgl_area_t *area, sgl_area_t *rect, const sgl_draw_slate_scrollbar_t *desc)
{
    int x1 = rect->x1, y1 = rect->y1, x2 = rect->x2, y2 = rect->y2;
    int w  = x2 - x1 + 1;
    int h  = y2 - y1 + 1;
    int vertical  = (desc->dir == SGL_SLATE_SCROLLBAR_VERTICAL);
    int thickness = vertical ? w : h;
    int track_len = vertical ? h : w;
    int arrow     = desc->show_arrows ? thickness : 0;
    int run       = track_len - 2 * arrow;
    int32_t total   = desc->total;
    int32_t visible = desc->visible;
    int32_t pos     = desc->pos;
    int thumb_len, usable, thumb_off, pad, minlen;
    sgl_color_t thumb = desc->thumb_color;
    sgl_area_t t;

    if (desc->alpha == SGL_ALPHA_MIN) {
        return;
    }
    if (w <= 0 || h <= 0 || total <= 0 || visible <= 0 || run <= 0) {
        return;
    }

    sgl_draw_fill_rect(surf, area, rect, 0, desc->track_color, desc->alpha);

    if (desc->show_arrows) {
        int a = sgl_max(2, thickness / 4);
        if (vertical) {
            int cs = x1 + w / 2;
            slate_scroll_arrow(surf, area, (int16_t)cs, (int16_t)(y1 + thickness / 2), a, 0, desc->arrow_color, desc->alpha);
            slate_scroll_arrow(surf, area, (int16_t)cs, (int16_t)(y2 - thickness / 2), a, 1, desc->arrow_color, desc->alpha);
        }
        else {
            int cyc = y1 + h / 2;
            slate_scroll_arrow(surf, area, (int16_t)(x1 + thickness / 2), (int16_t)cyc, a, 2, desc->arrow_color, desc->alpha);
            slate_scroll_arrow(surf, area, (int16_t)(x2 - thickness / 2), (int16_t)cyc, a, 3, desc->arrow_color, desc->alpha);
        }
    }

    if (visible >= total) {
        thumb_len = run;
    }
    else {
        minlen = sgl_max(thickness / 2, 10);
        thumb_len = (int)((int32_t)run * visible / total);
        if (thumb_len < minlen) {
            thumb_len = minlen;
        }
        if (thumb_len > run) {
            thumb_len = run;
        }
    }

    if (pos < 0) {
        pos = 0;
    }
    if (total > visible && pos > total - visible) {
        pos = total - visible;
    }
    usable = run - thumb_len;
    thumb_off = (usable > 0 && total > visible) ? (int)((int32_t)usable * pos / (total - visible)) : 0;

    if (desc->state == SGL_SLATE_SCROLLBAR_HOVER) {
        thumb = sgl_color_mixer(SGL_COLOR_BLACK, thumb, 60);
    }
    else if (desc->state == SGL_SLATE_SCROLLBAR_PRESSED) {
        thumb = sgl_color_mixer(SGL_COLOR_BLACK, thumb, 110);
    }

    pad = sgl_max(2, thickness / 5);

    if (vertical) {
        int ty = y1 + arrow + thumb_off;
        t.x1 = (int16_t)(x1 + pad);
        t.x2 = (int16_t)(x2 - pad);
        t.y1 = (int16_t)ty;
        t.y2 = (int16_t)(ty + thumb_len - 1);
    }
    else {
        int tx = x1 + arrow + thumb_off;
        t.x1 = (int16_t)tx;
        t.x2 = (int16_t)(tx + thumb_len - 1);
        t.y1 = (int16_t)(y1 + pad);
        t.y2 = (int16_t)(y2 - pad);
    }
    if (t.x2 < t.x1) {
        t.x2 = t.x1;
    }
    if (t.y2 < t.y1) {
        t.y2 = t.y1;
    }
    sgl_draw_fill_rect(surf, area, &t, 0, thumb, desc->alpha);
}
