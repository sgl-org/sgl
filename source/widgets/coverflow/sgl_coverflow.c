/* source/widgets/coverflow/sgl_coverflow.c
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

#include <string.h>
#include <sgl_draw.h>
#include <sgl_math.h>
#include "sgl_coverflow.h"

#define COVERFLOW_FOCUS_SCALE        (1000)  /* focused card scale, permille (pixmap size, no upscale) */
#define COVERFLOW_SIDE_SCALE         (750)   /* cards one slot away and beyond, permille */
#define COVERFLOW_CARD_GAP           (8)     /* gap between two adjacent cards, px */
#define COVERFLOW_DEFAULT_ANIM_MS    (260)   /* snap animation duration, ms */
#define COVERFLOW_DEFAULT_RADIUS     (24)    /* focused card corner radius, px */
#define COVERFLOW_PRESS_ZOOM         (2)     /* pressed card growth per side, px */

/**
 * @brief draw an ARGB8565 pixmap scaled into a rounded rectangle
 * @param surf surface to draw to
 * @param area clip area (usually the widget area)
 * @param coords rounded rectangle in screen coordinates
 * @param radius corner radius, px
 * @param alpha extra opacity applied on top of the per-pixel alpha
 * @param pixmap source image
 * @return none
 */
static void coverflow_draw_argb8565(sgl_surf_t *surf, sgl_area_t *area, sgl_area_t *coords,
                                    int16_t radius, uint8_t alpha,
                                    const sgl_pixmap_t *pixmap)
{
    if (alpha == SGL_ALPHA_MIN || pixmap == NULL || pixmap->bitmap.array == NULL ||
        pixmap->width == 0 || pixmap->height == 0) {
        return;
    }

    sgl_area_t image = *coords;
    const int32_t dst_w = image.x2 - image.x1 + 1;
    const int32_t dst_h = image.y2 - image.y1 + 1;
    if (dst_w <= 0 || dst_h <= 0) {
        return;
    }

    sgl_area_t card_clip;
    sgl_area_t clip;
    if (!sgl_area_clip(area, &image, &card_clip) ||
        !sgl_surf_clip(surf, &card_clip, &clip)) {
        return;
    }

    const int32_t max_radius = sgl_min(dst_w, dst_h) / 2;
    if (radius > max_radius) {
        radius = max_radius;
    }
    const int32_t radius_sq = radius * radius;
    const int32_t src_w = (int32_t)pixmap->width;
    const int32_t src_h = (int32_t)pixmap->height;
    const int32_t step_x = (src_w << SGL_FIXED_SHIFT) / dst_w;
    const int32_t step_y = (src_h << SGL_FIXED_SHIFT) / dst_h;
    const size_t src_stride = (size_t)pixmap->width * 3;
    /* shrinking must be sampled bilinearly, otherwise whole source pixels are
     * dropped and the scaled bitmap looks aliased */
    const bool bilinear = step_x > (1 << SGL_FIXED_SHIFT) || step_y > (1 << SGL_FIXED_SHIFT);
    sgl_color_t *dst_row = sgl_surf_get_buf(surf, clip.x1 - surf->x1, clip.y1 - surf->y1);

    for (int32_t y = clip.y1; y <= clip.y2; y++, dst_row += surf->w) {
        int32_t span_x1 = image.x1;
        int32_t span_x2 = image.x2;
        int32_t dy;
        if (radius > 0 && y < image.y1 + radius) {
            dy = radius - (y - image.y1);
        }
        else if (radius > 0 && y > image.y2 - radius) {
            dy = radius - (image.y2 - y);
        }
        else {
            dy = 0;
        }
        if (dy > 0) {
            const int32_t dx = sgl_sqrt((uint32_t)(radius_sq - dy * dy));
            const int32_t inset = radius - dx;
            span_x1 += inset;
            span_x2 -= inset;
        }

        const int32_t x1 = sgl_max(span_x1, clip.x1);
        const int32_t x2 = sgl_min(span_x2, clip.x2);
        if (x1 > x2) {
            continue;
        }

        sgl_color_t *dst = dst_row + (x1 - clip.x1);

        if (bilinear) {
            const int32_t y_src_fp = (y - image.y1) * step_y;
            const int32_t sy0 = y_src_fp >> SGL_FIXED_SHIFT;
            const int32_t sy1 = sgl_min(sy0 + 1, src_h - 1);
            const uint32_t fy = (uint32_t)((y_src_fp >> (SGL_FIXED_SHIFT - 8)) & 0xFF);
            const uint8_t *row0 = pixmap->bitmap.array + (size_t)sy0 * src_stride;
            const uint8_t *row1 = pixmap->bitmap.array + (size_t)sy1 * src_stride;

            for (int32_t x = x1; x <= x2; x++, dst++) {
                const int32_t x_src_fp = (x - image.x1) * step_x;
                const int32_t sx0 = x_src_fp >> SGL_FIXED_SHIFT;
                const int32_t sx1 = sgl_min(sx0 + 1, src_w - 1);
                const uint32_t fx = (uint32_t)((x_src_fp >> (SGL_FIXED_SHIFT - 8)) & 0xFF);
                const uint8_t *p00 = row0 + (size_t)sx0 * 3;
                const uint8_t *p01 = row0 + (size_t)sx1 * 3;
                const uint8_t *p10 = row1 + (size_t)sx0 * 3;
                const uint8_t *p11 = row1 + (size_t)sx1 * 3;
                const uint32_t w00 = (256 - fx) * (256 - fy);
                const uint32_t w01 = fx * (256 - fy);
                const uint32_t w10 = (256 - fx) * fy;
                const uint32_t w11 = fx * fy;
                uint32_t pixel_alpha = ((uint32_t)p00[2] * w00 + (uint32_t)p01[2] * w01 +
                                        (uint32_t)p10[2] * w10 + (uint32_t)p11[2] * w11) >> 16;

                if (pixel_alpha != SGL_ALPHA_MIN) {
                    if (alpha != SGL_ALPHA_MAX) {
                        pixel_alpha = (pixel_alpha * alpha + SGL_ALPHA_MAX / 2) / SGL_ALPHA_MAX;
                    }
                    if (pixel_alpha != SGL_ALPHA_MIN) {
                        const sgl_color_t c00 = sgl_rgb565_to_color((uint16_t)((uint16_t)p00[0] | ((uint16_t)p00[1] << 8)));
                        const sgl_color_t c01 = sgl_rgb565_to_color((uint16_t)((uint16_t)p01[0] | ((uint16_t)p01[1] << 8)));
                        const sgl_color_t c10 = sgl_rgb565_to_color((uint16_t)((uint16_t)p10[0] | ((uint16_t)p10[1] << 8)));
                        const sgl_color_t c11 = sgl_rgb565_to_color((uint16_t)((uint16_t)p11[0] | ((uint16_t)p11[1] << 8)));
                        const sgl_color_t color = sgl_draw_biln_color_mix(c00, c01, c10, c11, w00, w01, w10, w11);
                        *dst = (pixel_alpha == SGL_ALPHA_MAX) ? color :
                               sgl_color_mixer(color, *dst, (uint8_t)pixel_alpha);
                    }
                }
            }
        }
        else {
            const int32_t src_y = ((y - image.y1) * step_y) >> SGL_FIXED_SHIFT;
            const uint8_t *src_pixel = pixmap->bitmap.array + (size_t)src_y * src_stride;
            int32_t src_x_fp = (x1 - image.x1) * step_x;
            int32_t src_x = src_x_fp >> SGL_FIXED_SHIFT;
            src_pixel += (size_t)src_x * 3;

            for (int32_t x = x1; x <= x2; x++, dst++) {
                uint8_t pixel_alpha = src_pixel[2];
                if (pixel_alpha != SGL_ALPHA_MIN) {
                    if (alpha != SGL_ALPHA_MAX) {
                        pixel_alpha = (uint8_t)(((uint16_t)pixel_alpha * alpha + SGL_ALPHA_MAX / 2) / SGL_ALPHA_MAX);
                    }
                    if (pixel_alpha != SGL_ALPHA_MIN) {
                        const uint16_t rgb565 = (uint16_t)src_pixel[0] | ((uint16_t)src_pixel[1] << 8);
                        const sgl_color_t color = sgl_rgb565_to_color(rgb565);
                        *dst = (pixel_alpha == SGL_ALPHA_MAX) ? color : sgl_color_mixer(color, *dst, pixel_alpha);
                    }
                }

                src_x_fp += step_x;
                const int32_t next_src_x = src_x_fp >> SGL_FIXED_SHIFT;
                src_pixel += (size_t)(next_src_x - src_x) * 3;
                src_x = next_src_x;
            }
        }
    }
}

/**
 * @brief convert a widget-local area into screen coordinates
 * @param coverflow coverflow object
 * @param local area relative to the widget origin
 * @param out screen area
 * @return none
 */
static inline void coverflow_to_screen(sgl_coverflow_t *coverflow, const sgl_area_t *local, sgl_area_t *out)
{
    out->x1 = coverflow->obj.coords.x1 + local->x1;
    out->y1 = coverflow->obj.coords.y1 + local->y1;
    out->x2 = coverflow->obj.coords.x1 + local->x2;
    out->y2 = coverflow->obj.coords.y1 + local->y2;
}

/**
 * @brief get the screen area of a card
 * @param coverflow coverflow object
 * @param index card index
 * @param out card area in screen coordinates
 * @return none
 */
static void coverflow_card_area(sgl_coverflow_t *coverflow, int16_t index, sgl_area_t *out)
{
    coverflow_to_screen(coverflow, &coverflow->items[index].coords, out);
}

/**
 * @brief base (unscaled) card width: the pixmap width
 * @param coverflow coverflow object
 * @param index card index
 * @return card width in pixels
 * @note  the pixmap is mandatory (checked at creation), so it is never NULL
 */
static int16_t coverflow_base_width(sgl_coverflow_t *coverflow, int16_t index)
{
    return (int16_t)coverflow->cards[index].pixmap->width;
}

/**
 * @brief base (unscaled) card height: the pixmap height
 * @param coverflow coverflow object
 * @param index card index
 * @return card height in pixels
 */
static int16_t coverflow_base_height(sgl_coverflow_t *coverflow, int16_t index)
{
    return (int16_t)coverflow->cards[index].pixmap->height;
}

/**
 * @brief resting distance between adjacent card centers
 * @param coverflow coverflow object
 * @return pitch in pixels
 * @note  half of (focused card width + base card width) plus the gap: at rest
 *        the focused card is at FOCUS_SCALE and its neighbour at 100%, so the
 *        visible edge gap is exactly COVERFLOW_CARD_GAP and the cards never
 *        overlap
 */
static int16_t coverflow_calc_pitch(sgl_coverflow_t *coverflow)
{
    int32_t max_w = 1;

    for (int16_t i = 0; i < coverflow->card_num; i++) {
        const int16_t w = coverflow_base_width(coverflow, i);
        if (w > max_w) {
            max_w = w;
        }
    }

    return (int16_t)(max_w * (COVERFLOW_FOCUS_SCALE + COVERFLOW_SIDE_SCALE) / 2000 +
                     COVERFLOW_CARD_GAP);
}

/**
 * @brief draw one card (body pixmap)
 * @param coverflow coverflow object
 * @param surf surface to draw to
 * @param index card index
 * @return none
 */
static void coverflow_draw_card(sgl_coverflow_t *coverflow, sgl_surf_t *surf, int16_t index)
{
    sgl_obj_t *obj = &coverflow->obj;
    const sgl_coverflow_card_t *card = &coverflow->cards[index];
    const sgl_coverflow_item_t *item = &coverflow->items[index];
    sgl_area_t rect;

    coverflow_card_area(coverflow, index, &rect);

    if (card->pixmap->format == SGL_PIXMAP_FMT_ARGB8565) {
        coverflow_draw_argb8565(surf, &obj->area, &rect, item->radius,
                                SGL_ALPHA_MAX, card->pixmap);
    }
    else {
        sgl_draw_rect_t desc = {
            .color = SGL_COLOR_WHITE,
            .alpha = SGL_ALPHA_MAX,
            .border_alpha = SGL_ALPHA_MAX,
            .border = 0,
            .pixmap = card->pixmap,
            .radius = item->radius,
        };
        sgl_draw_rect(surf, &obj->area, &rect, &desc);
    }
}

/**
 * @brief current scale of a card, permille of its base size
 * @param coverflow coverflow object
 * @param index card index
 * @return scale in permille: COVERFLOW_FOCUS_SCALE at the widget center down
 *         to COVERFLOW_SIDE_SCALE one slot away
 */
static int32_t coverflow_scale_at(const sgl_coverflow_t *coverflow, int16_t index)
{
    int32_t dist = sgl_abs((int32_t)index * coverflow->spacing - coverflow->scroll);
    if (dist > coverflow->spacing) {
        dist = coverflow->spacing;
    }

    return COVERFLOW_SIDE_SCALE + (COVERFLOW_FOCUS_SCALE - COVERFLOW_SIDE_SCALE) *
                                  (coverflow->spacing - dist) / coverflow->spacing;
}

/**
 * @brief center distance that keeps COVERFLOW_CARD_GAP between two neighbours
 * @param coverflow coverflow object
 * @param left left card index
 * @param right right card index
 * @return distance between the two card centers, px
 * @note  half of both current widths plus the gap, so the visible edge gap is
 *        the same whether the pair is focused, half way or both unscaled
 */
static int32_t coverflow_pair_dist(const sgl_coverflow_t *coverflow, int16_t left, int16_t right)
{
    const int32_t lw = coverflow_base_width((sgl_coverflow_t *)coverflow, left) *
                       coverflow_scale_at(coverflow, left) / 1000;
    const int32_t rw = coverflow_base_width((sgl_coverflow_t *)coverflow, right) *
                       coverflow_scale_at(coverflow, right) / 1000;

    return lw / 2 + rw / 2 + COVERFLOW_CARD_GAP;
}

/**
 * @brief place one card at the given horizontal offset from the widget center
 * @param coverflow coverflow object
 * @param index card index
 * @param off card center offset, px
 * @param top card currently on the top layer
 * @param old_top top layer of the previous layout pass
 * @return none
 * @note pushes only the region the card occupied or now occupies
 */
static void coverflow_place_card(sgl_coverflow_t *coverflow, int16_t index, int32_t off,
                                 int16_t top, int16_t old_top)
{
    sgl_obj_t *obj = &coverflow->obj;
    const int16_t center_x = sgl_obj_get_width(obj) / 2;
    const int16_t center_y = sgl_obj_get_height(obj) / 2;
    const int32_t scale = coverflow_scale_at(coverflow, index);
    int16_t w = coverflow_base_width(coverflow, index) * scale / 1000;
    int16_t h = coverflow_base_height(coverflow, index) * scale / 1000;
    int16_t radius = coverflow->radius * scale / 1000;

    if (w < 1) {
        w = 1;
    }
    if (h < 1) {
        h = 1;
    }
    const int16_t max_radius = sgl_min(w, h) / 2;
    if (radius > max_radius) {
        radius = max_radius;
    }
    if (radius < 0) {
        radius = 0;
    }

    sgl_coverflow_item_t *item = &coverflow->items[index];
    const sgl_area_t old_rect = item->coords;
    const int16_t old_radius = item->radius;

    item->coords.x1 = center_x + (int16_t)off - w / 2;
    item->coords.y1 = center_y - h / 2;
    item->coords.x2 = item->coords.x1 + w - 1;
    item->coords.y2 = item->coords.y1 + h - 1;

    /* press feedback: grow the touched card a little, like the core does
     * for flexible objects */
    if (index == coverflow->press_card) {
        item->coords.x1 -= COVERFLOW_PRESS_ZOOM;
        item->coords.y1 -= COVERFLOW_PRESS_ZOOM;
        item->coords.x2 += COVERFLOW_PRESS_ZOOM;
        item->coords.y2 += COVERFLOW_PRESS_ZOOM;
    }

    item->radius = radius;

    const bool moved = old_rect.x1 != item->coords.x1 || old_rect.y1 != item->coords.y1 ||
                       old_rect.x2 != item->coords.x2 || old_rect.y2 != item->coords.y2 ||
                       old_radius != radius;
    /* nothing moved and the draw order stays the same */
    if (!moved && top == old_top) {
        return;
    }

    /* dirty region: where the card was plus where it is now */
    sgl_area_t dirty;
    sgl_area_t old_abs;
    coverflow_card_area(coverflow, index, &dirty);
    coverflow_to_screen(coverflow, &old_rect, &old_abs);
    sgl_area_selfmerge(&dirty, &old_abs);

    sgl_update_area(&dirty);
}

/**
 * @brief place and scale every card from the current camera position
 * @param coverflow coverflow object
 * @return none
 * @note  card geometry is stored in widget-local coordinates. Each card uses
 *        its pixmap width / height as base size: the centered card keeps that
 *        size (no upscale, so the bitmap stays sharp) and the others shrink
 *        down to COVERFLOW_SIDE_SCALE one slot away. Card centers are
 *        accumulated with the per-pair distance, so every neighbouring pair
 *        keeps exactly COVERFLOW_CARD_GAP, focused or not. Only the cards
 *        that actually moved are pushed through sgl_update_area()
 */
static void coverflow_layout(sgl_coverflow_t *coverflow)
{
    const int16_t old_top = coverflow->top;

    /* the pitch follows the card sizes: when it changes keep the camera on
     * the same relative position */
    const int16_t pitch = coverflow_calc_pitch(coverflow);
    if (pitch != coverflow->spacing) {
        if (coverflow->spacing > 0) {
            coverflow->scroll = coverflow->scroll * pitch / coverflow->spacing;
        }
        coverflow->spacing = pitch;
    }

    int16_t top = 0;
    int32_t top_dist = INT32_MAX;

    /* the focused card is the nearest one and is drawn last, on top of its
     * neighbours */
    for (int16_t i = 0; i < coverflow->card_num; i++) {
        const int32_t dist = sgl_abs(i * coverflow->spacing - coverflow->scroll);
        if (dist < top_dist) {
            top_dist = dist;
            top = i;
        }
    }

    /* camera: an anchor card plus the fractional part (1/256 slot) between it
     * and the next card; the anchor is clamped to an existing pair so the
     * extrapolation still works during the rubber band overscroll */
    int32_t anchor = 0;
    int32_t t_fp = 0;
    if (coverflow->card_num > 1) {
        const int32_t last = coverflow->card_num - 1;
        if (coverflow->scroll <= 0) {
            anchor = 0;
            t_fp = coverflow->scroll * 256 / coverflow->spacing;
        }
        else if (coverflow->scroll >= last * coverflow->spacing) {
            anchor = last - 1;
            t_fp = (coverflow->scroll - anchor * coverflow->spacing) * 256 / coverflow->spacing;
        }
        else {
            anchor = coverflow->scroll / coverflow->spacing;
            t_fp = (coverflow->scroll - anchor * coverflow->spacing) * 256 / coverflow->spacing;
        }
    }

    int32_t anchor_off = 0;
    if (coverflow->card_num > 1) {
        anchor_off = -(t_fp * coverflow_pair_dist(coverflow, (int16_t)anchor,
                                                  (int16_t)(anchor + 1))) / 256;
    }

    /* walk right from the anchor, then left, accumulating the pair distance */
    int32_t off = anchor_off;
    for (int16_t i = (int16_t)anchor; i < coverflow->card_num; i++) {
        coverflow_place_card(coverflow, i, off, top, old_top);
        if (i + 1 < coverflow->card_num) {
            off += coverflow_pair_dist(coverflow, i, (int16_t)(i + 1));
        }
    }

    off = anchor_off;
    for (int16_t i = (int16_t)(anchor - 1); i >= 0; i--) {
        off -= coverflow_pair_dist(coverflow, i, (int16_t)(i + 1));
        coverflow_place_card(coverflow, i, off, top, old_top);
    }

    coverflow->top = top;
}

/**
 * @brief animation frame callback, value is the new camera position
 * @note  the animation carries the widget object as its private data
 */
static void coverflow_anim_path_cb(sgl_anim_t *anim, int32_t value)
{
    sgl_coverflow_t *coverflow = sgl_container_of((sgl_obj_t *)sgl_anim_get_data(anim),
                                                 sgl_coverflow_t, obj);
    coverflow->scroll = value;
    coverflow_layout(coverflow);
}

/**
 * @brief cancel the running snap animation, if any
 * @param coverflow coverflow object
 * @return none
 */
static void coverflow_anim_cancel(sgl_coverflow_t *coverflow)
{
    sgl_anim_t *anim = sgl_anim_get_by_obj(&coverflow->obj);
    if (anim != NULL) {
        sgl_anim_delete(anim);
    }
}

/**
 * @brief scroll the menu with an ease-out scale animation
 * @param coverflow coverflow object
 * @param target camera position in pixels, clamped to the card range
 * @return none
 */
static void coverflow_scroll_to(sgl_coverflow_t *coverflow, int32_t target)
{
    target = sgl_clamp(target, 0, (coverflow->card_num - 1) * coverflow->spacing);
    coverflow->index = target / coverflow->spacing;

    coverflow_anim_cancel(coverflow);
    if (target == coverflow->scroll) {
        /* already at the target camera position, but the widget geometry may
         * have changed since the last layout (the caller resized / moved it
         * after creation): re-run the layout so the cards stay centered */
        coverflow_layout(coverflow);
        return;
    }

    sgl_anim_setup_obj_action(&coverflow->obj, (int16_t)coverflow->scroll, (int16_t)target,
                              (uint16_t)coverflow->anim_ms, coverflow_anim_path_cb,
                              SGL_ANIM_PATH_EASE_OUT, NULL);
}

/**
 * @brief scroll engine commit callback, mirrors sc->offset into the camera
 * @param sc scroll state owned by this widget
 * @return none
 * @note called on every inertia/rebound frame; once the glide settles
 *       (coasting cleared) the menu snaps to the nearest card
 */
static void coverflow_scroll_commit(sgl_scroll_t *sc)
{
    sgl_coverflow_t *coverflow = sgl_container_of(sc, sgl_coverflow_t, sc);
    coverflow->scroll = sc->offset;
    coverflow_layout(coverflow);

    if (!sc->coasting) {
        coverflow_scroll_to(coverflow, ((coverflow->scroll + coverflow->spacing / 2) / coverflow->spacing) * coverflow->spacing);
    }
}

/**
 * @brief find the card under the given screen position
 * @param coverflow coverflow object
 * @param pos event position in screen coordinates
 * @return card index, -1 if the position hits no card
 * @note  cards overlap near the focused one, so among all hit cards the
 *        one whose center is closest to the position wins
 */
static int16_t coverflow_hit_card(sgl_coverflow_t *coverflow, sgl_event_pos_t pos)
{
    sgl_obj_t *obj = &coverflow->obj;
    int16_t hit = -1;
    int32_t hit_dist = INT32_MAX;

    for (int16_t i = 0; i < coverflow->card_num; i++) {
        const sgl_area_t *rect = &coverflow->items[i].coords;
        const int16_t x1 = obj->coords.x1 + rect->x1;
        const int16_t x2 = obj->coords.x1 + rect->x2;
        const int16_t y1 = obj->coords.y1 + rect->y1;
        const int16_t y2 = obj->coords.y1 + rect->y2;

        if (pos.x < x1 || pos.x > x2 || pos.y < y1 || pos.y > y2) {
            continue;
        }

        const int32_t dist = sgl_abs(pos.x - (x1 + x2) / 2);
        if (dist < hit_dist) {
            hit_dist = dist;
            hit = i;
        }
    }

    return hit;
}

/**
 * @brief forward an event to the card activation callback
 * @param coverflow coverflow object
 * @param index target card index
 * @param evt event to forward
 * @return none
 */
static void coverflow_emit_card_event(sgl_coverflow_t *coverflow, int16_t index, sgl_event_t *evt)
{
    if (coverflow->card_event_cb == NULL) {
        return;
    }

    sgl_event_t ce = *evt;
    ce.obj = &coverflow->obj;
    ce.event_data = coverflow->card_event_data;
    coverflow->card_event_cb(&ce, index);
}

/**
 * @brief coverflow construct callback; it draws every card and handles the
 *        touch / key scrolling, and releases its resources on destroy
 * @note  tapping a side card scrolls it to the center; tapping the
 *        centered card fires the card activation callback
 */
static void sgl_coverflow_construct_cb(sgl_surf_t *surf, sgl_obj_t *obj, sgl_event_t *evt)
{
    sgl_coverflow_t *coverflow = sgl_container_of(obj, sgl_coverflow_t, obj);
    const int32_t range = (coverflow->card_num - 1) * coverflow->spacing;

    switch (evt->type) {
    case SGL_EVENT_DRAW_MAIN:
        /* side cards first, the focused card on top */
        for (int16_t i = 0; i < coverflow->card_num; i++) {
            if (i != coverflow->top) {
                coverflow_draw_card(coverflow, surf, i);
            }
        }
        if (coverflow->top >= 0 && coverflow->top < coverflow->card_num) {
            coverflow_draw_card(coverflow, surf, coverflow->top);
        }
        break;

    case SGL_EVENT_PRESSED:
        /* grab the menu: stop the snap animation and any coasting inertia,
         * sync the engine with the current camera, then start tracking */
        coverflow_anim_cancel(coverflow);
        sgl_scroll_anim_stop(&coverflow->sc);
        coverflow->sc.offset = coverflow->scroll;
        sgl_scroll_press(&coverflow->sc, evt->pos.x);

        /* press feedback on the touched card, then notify the user */
        coverflow->press_card = coverflow_hit_card(coverflow, evt->pos);
        if (coverflow->press_card >= 0) {
            coverflow_layout(coverflow);
            coverflow_emit_card_event(coverflow, coverflow->press_card, evt);
        }
        break;

    case SGL_EVENT_MOVE_LEFT:
    case SGL_EVENT_MOVE_RIGHT:
        /* cards follow the finger; the engine samples the flick speed on
         * the way and rubber-bands beyond the first / last card */
        if (sgl_scroll_stay(&coverflow->sc, evt->pos.x, range)) {
            coverflow->scroll = coverflow->sc.offset;
            coverflow_layout(coverflow);
        }
        break;

    case SGL_EVENT_RELEASED:
        /* flick: coast with inertia, snap when the glide settles (commit);
         * dragged release without inertia: snap straight to the nearest
         * card. A plain tap (no drag) is left alone: the CLICKED event,
         * which is queued right before this release, may have started a
         * scroll animation of its own and snapping here would cancel it */
        coverflow->sc.range  = range;
        coverflow->sc.commit = coverflow_scroll_commit;
        if (sgl_scroll_release(&coverflow->sc, range)) {
            sgl_scroll_anim_start(&coverflow->sc);
        }
        else if (coverflow->sc.dragged) {
            coverflow->sc.dragged = 0;
            coverflow_scroll_to(coverflow, ((coverflow->scroll + coverflow->spacing / 2) / coverflow->spacing) * coverflow->spacing);
        }

        /* release feedback on the card the press started on */
        if (coverflow->press_card >= 0) {
            const int16_t card = coverflow->press_card;
            coverflow->press_card = -1;
            coverflow_layout(coverflow);
            coverflow_emit_card_event(coverflow, card, evt);
        }
        break;

    case SGL_EVENT_CLICKED:
    case SGL_EVENT_LONG_CLICKED: {
        /* ENTER (quick tap or hold) activates the centered card, so both
         * click kinds are handled the same way here.
         * a drag ends with no activation, snap back to the nearest card */
        if (coverflow->sc.dragged) {
            coverflow->sc.dragged = 0;
            coverflow_scroll_to(coverflow, ((coverflow->scroll + coverflow->spacing / 2) / coverflow->spacing) * coverflow->spacing);
            break;
        }

        /* key driven click (Enter) activates the centered card */
        int16_t hit = (evt->pos.x == SGL_POS_MIN && evt->pos.y == SGL_POS_MIN) ?
                      coverflow->index : coverflow_hit_card(coverflow, evt->pos);
        if (hit < 0) {
            break;
        }
        SGL_LOG_INFO("coverflow CLICKED: pos %d,%d hit %d index %d", evt->pos.x, evt->pos.y, hit, coverflow->index);

        if (hit != coverflow->index) {
            /* side card tapped: slide it to the center */
            coverflow_scroll_to(coverflow, hit * coverflow->spacing);
        }
        else {
            /* centered card tapped: fire the card activation callback */
            SGL_LOG_INFO("coverflow: center card %d clicked, forward to user cb", hit);
            coverflow_emit_card_event(coverflow, hit, evt);
        }
        break;
    }

    case SGL_EVENT_KEY_LEFT:
    case SGL_EVENT_KEY_UP:
        sgl_scroll_anim_stop(&coverflow->sc);
        coverflow_scroll_to(coverflow, (coverflow->index - 1) * coverflow->spacing);
        break;

    case SGL_EVENT_KEY_RIGHT:
    case SGL_EVENT_KEY_DOWN:
        sgl_scroll_anim_stop(&coverflow->sc);
        coverflow_scroll_to(coverflow, (coverflow->index + 1) * coverflow->spacing);
        break;

    case SGL_EVENT_KEY_ESC:
        sgl_key_send_signal(evt, SGL_EVENT_KEY_CANCEL);
        break;

    case SGL_EVENT_DESTROYED:
        coverflow_anim_cancel(coverflow);
        sgl_scroll_anim_stop(&coverflow->sc);
        if (coverflow->group != NULL) {
            sgl_key_group_delete(coverflow->group);
            coverflow->group = NULL;
        }
        sgl_free(coverflow->cards);
        sgl_free(coverflow->items);
        coverflow->cards = NULL;
        coverflow->items = NULL;
        break;

    default:
        break;
    }
}

/**
 * @brief create a coverflow object
 * @param parent parent object, NULL creates the menu on the active screen
 * @param cards card descriptor array (copied into the widget)
 * @param card_num number of cards
 * @return coverflow object, NULL on failure
 */
sgl_obj_t* sgl_coverflow_create(sgl_obj_t *parent, const sgl_coverflow_card_t *cards, int16_t card_num)
{
    if (cards == NULL || card_num <= 0) {
        SGL_LOG_ERROR("sgl_coverflow_create: invalid cards");
        return NULL;
    }

    /* every card needs a pixmap: its width / height define the card size */
    for (int16_t i = 0; i < card_num; i++) {
        const sgl_pixmap_t *pixmap = cards[i].pixmap;
        if (pixmap == NULL || pixmap->bitmap.array == NULL ||
            pixmap->width == 0 || pixmap->height == 0) {
            SGL_LOG_ERROR("sgl_coverflow_create: card %d has no valid pixmap, "
                          "the pixmap is mandatory", i);
            return NULL;
        }
    }

    sgl_coverflow_t *coverflow = sgl_malloc(sizeof(sgl_coverflow_t));
    if (coverflow == NULL) {
        SGL_LOG_ERROR("sgl_coverflow_create: malloc failed");
        return NULL;
    }
    memset(coverflow, 0, sizeof(sgl_coverflow_t));

    coverflow->cards = sgl_malloc(card_num * sizeof(sgl_coverflow_card_t));
    coverflow->items = sgl_malloc(card_num * sizeof(sgl_coverflow_item_t));
    if (coverflow->cards == NULL || coverflow->items == NULL) {
        SGL_LOG_ERROR("sgl_coverflow_create: malloc failed");
        sgl_free(coverflow->cards);
        sgl_free(coverflow->items);
        sgl_free(coverflow);
        return NULL;
    }
    memcpy(coverflow->cards, cards, card_num * sizeof(sgl_coverflow_card_t));
    memset(coverflow->items, 0, card_num * sizeof(sgl_coverflow_item_t));

    sgl_obj_t *obj = &coverflow->obj;
    sgl_obj_init(obj, parent);
    obj->construct_fn = sgl_coverflow_construct_cb;
    /* the widget itself is the event target: it has no child objects, so it
     * must be clickable for the click detection to reach it; movable enables
     * drag tracking, editable lets ENTER activate the centered card while the
     * group is in the (auto-enabled) key scroll mode */
    sgl_obj_set_clickable(obj);
    sgl_obj_set_movable(obj);
    sgl_obj_set_editable(obj);
    /* default to the parent size, the focused card rests at the center */
    sgl_obj_set_size(obj, sgl_obj_get_width(obj->parent), sgl_obj_get_height(obj->parent));

    coverflow->card_num = card_num;
    coverflow->spacing = coverflow_calc_pitch(coverflow);
    coverflow->anim_ms = COVERFLOW_DEFAULT_ANIM_MS;
    coverflow->radius = COVERFLOW_DEFAULT_RADIUS;
    coverflow->top = -1;
    coverflow->press_card = -1;
    sgl_scroll_reset(&coverflow->sc);

    /* key group for arrow key scrolling: the widget is the only member and,
     * being a main menu, is focused and switched into the editing state at
     * load time, so LEFT / RIGHT / UP / DOWN scroll one card per press right
     * away and ENTER activates the centered card (no arming press needed) */
    coverflow->group = sgl_key_group_create(1);
    if (coverflow->group == NULL) {
        SGL_LOG_ERROR("sgl_coverflow_create: create key group failed");
    }
    else {
        sgl_key_group_add_obj(coverflow->group, obj);
    }

    /* start with the middle card focused */
    coverflow->index = card_num / 2;
    coverflow->scroll = coverflow->index * coverflow->spacing;
    coverflow_layout(coverflow);

    if (coverflow->group != NULL) {
        /* focus the coverflow and switch the group into the editing state
         * right away: this is a main menu, so the arrow keys must scroll card
         * by card without a first ENTER, and ENTER activates the center card.
         * the group is dedicated to this widget, so forcing editing here is
         * safe and does not affect the focus navigation of other widgets */
        coverflow->group->focused = 0;
        sgl_key_group_load(coverflow->group);
        coverflow->group->editing = 1;
        /* key dispatch only needs the group focus index, not the obj focus
         * flag that makes the core draw a wireframe around the widget; as a
         * full-screen main menu the coverflow must not show that box */
        sgl_obj_clear_focused(obj);
    }

    return obj;
}

/**
 * @brief scroll to the given card with an ease-out animation
 * @param obj coverflow object
 * @param index target card index, clamped to the card range
 * @return none
 */
void sgl_coverflow_set_index(sgl_obj_t *obj, int16_t index)
{
    sgl_coverflow_t *coverflow = sgl_container_of(obj, sgl_coverflow_t, obj);
    coverflow_scroll_to(coverflow, (int32_t)index * coverflow->spacing);
}

/**
 * @brief get the selected card index
 * @param obj coverflow object
 * @return selected card index
 */
int16_t sgl_coverflow_get_index(sgl_obj_t *obj)
{
    sgl_coverflow_t *coverflow = sgl_container_of(obj, sgl_coverflow_t, obj);
    return coverflow->index;
}

/**
 * @brief set the card activation callback
 * @param obj coverflow object
 * @param cb callback fired when the centered card is activated
 * @param data user data passed as evt->event_data
 * @return none
 */
void sgl_coverflow_set_card_event_cb(sgl_obj_t *obj, sgl_coverflow_card_event_cb_t cb, void *data)
{
    sgl_coverflow_t *coverflow = sgl_container_of(obj, sgl_coverflow_t, obj);
    coverflow->card_event_cb = cb;
    coverflow->card_event_data = data;
}

/**
 * @brief set the snap animation duration
 * @param obj coverflow object
 * @param ms duration in milliseconds
 * @return none
 */
void sgl_coverflow_set_anim_time(sgl_obj_t *obj, uint32_t ms)
{
    sgl_coverflow_t *coverflow = sgl_container_of(obj, sgl_coverflow_t, obj);
    coverflow->anim_ms = ms;
}

/**
 * @brief set the focused card corner radius
 * @param obj coverflow object
 * @param radius corner radius, px
 * @return none
 */
void sgl_coverflow_set_radius(sgl_obj_t *obj, int16_t radius)
{
    sgl_coverflow_t *coverflow = sgl_container_of(obj, sgl_coverflow_t, obj);
    coverflow->radius = radius;
    coverflow_layout(coverflow);
}
