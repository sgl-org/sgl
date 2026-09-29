/* source/widgets/spectrum/sgl_spectrum.c
 *
 * MIT License
 *
 * Copyright(c) 2023-present All contributors of SGL
 * Document reference link: docs directory
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
#include <sgl_log.h>
#include <sgl_mm.h>
#include <sgl_theme.h>
#include <sgl_cfgfix.h>
#include <string.h>
#include "sgl_spectrum.h"

static inline void sgl_spectrum_span_set(sgl_color_t *d, int n, sgl_color_t c)
{
    for (; n > 0; n--, d++) {
        *d = c;
    }
}

static inline void sgl_spectrum_span_cut(sgl_color_t *d, int n, int cut, sgl_color_t c)
{
    int mid = n - cut * 2;

    if (mid <= 0) {
        return;
    }
    d += cut;
    sgl_spectrum_span_set(d, mid, c);
}

static void sgl_spectrum_layout(sgl_spectrum_t *spectrum)
{
    sgl_obj_t *obj = &spectrum->obj;
    int16_t w = sgl_obj_get_width(obj);
    int16_t n = (int16_t)spectrum->bar_num;
    int16_t gap = (int16_t)spectrum->bar_gap;
    int16_t widths[SGL_SPECTRUM_BAR_MAX];
    int16_t x, i;

    spectrum->bar_height = (uint16_t)sgl_obj_get_height(obj);

    if (n <= 0 || w <= 0) {
        spectrum->bar_width = 0;
        spectrum->layout_x  = obj->coords.x1;
        return;
    }

    /* shrink an over-wide gap until every bar keeps at least 1 px */
    while (gap > 1 && (w - (n - 1) * gap) / n < 1) {
        gap--;
    }
    if (gap < 0) {
        gap = 0;
    }

    /* even split with Bresenham remainder distribution */
    sgl_split_len_avg(w, n, gap, widths);

    spectrum->bar_gap    = (uint8_t)gap;
    spectrum->bar_width  = (uint8_t)widths[0];
    spectrum->layout_x   = obj->coords.x1;

    /* cache the left x of every bar */
    x = obj->coords.x1;
    for (i = 0; i < n; i++) {
        spectrum->bar_x[i] = x;
        x += widths[i] + gap;
    }

    /* centre the row: slack = right margin (x already includes the trailing
     * gap), shift every bar by half of it */
    if (spectrum->align_center) {
        int16_t slack = (int16_t)((obj->coords.x2 + 1) - x + gap);
        if (slack > 1) {
            int16_t off = (int16_t)(slack / 2);
            for (i = 0; i < n; i++) {
                spectrum->bar_x[i] += off;
            }
        }
    }
}

static inline void sgl_spectrum_check_layout(sgl_spectrum_t *spectrum)
{
    if (spectrum->bar_height != (uint16_t)sgl_obj_get_height(&spectrum->obj) ||
        spectrum->layout_x != spectrum->obj.coords.x1) {
        sgl_spectrum_layout(spectrum);
    }
}

static void sgl_spectrum_build_round_cut(uint8_t r, uint8_t *cut)
{
    for (int row = 0; row < SGL_SPECTRUM_ROUND_MAX; row++) {
        if (row >= r) {
            cut[row] = 0;
            continue;
        }
        /* dy from the cap top; quarter circle: cut = r - sqrt(r^2 - dy^2) */
        int dy = r - row - 1;
        int dx = (int)sgl_sqrt((uint32_t)(sgl_pow2(r) - sgl_pow2(dy)));
        int c  = r - dx;

        cut[row] = (uint8_t)(c > 0 ? c : 0);
    }
}

static inline uint8_t sgl_spectrum_effective_radius(const sgl_spectrum_t *spectrum)
{
    uint8_t r = spectrum->round_radius;
    uint8_t half = (uint8_t)(spectrum->bar_width / 2);

    return (r > half) ? half : r;
}

static void sgl_spectrum_update_grad(sgl_spectrum_t *spectrum)
{
    if (!spectrum->bar_gradient || spectrum->bar_height <= 1) {
        spectrum->grad_step = 0;
        return;
    }
    spectrum->grad_step = (255u << 16) / (spectrum->bar_height - 1u);
}

static inline sgl_color_t sgl_spectrum_row_color(const sgl_spectrum_t *spectrum, int h, int from_top)
{
    uint32_t f;

    if (h <= 1 || spectrum->grad_step == 0) {
        return spectrum->bar_color;
    }
    /* f is 0 at the bar top (bar_color) and 255 at the bottom (low) */
    f = ((uint32_t)from_top * spectrum->grad_step) >> 16;
    if (f > 255u) {
        f = 255u;
    }
    return sgl_color_mixer(spectrum->bar_color_low, spectrum->bar_color, (uint8_t)f);
}

static inline void sgl_spectrum_draw_row(sgl_surf_t *surf, const sgl_spectrum_t *spectrum,
                                         int16_t x, int16_t y, int16_t width,
                                         uint8_t cut, sgl_color_t color)
{
    int16_t x1 = sgl_max(x, surf->x1);
    int16_t x2 = (int16_t)sgl_min((int32_t)x + width - 1, (int32_t)surf->x2);

    if (x1 > x2) {
        return;
    }

    sgl_color_t *dst = sgl_surf_get_buf(surf, x1 - surf->x1, y - surf->y1);
    int n = x2 - x1 + 1;

    if (cut > 0) {
        /* shift the corner cut when the row is clipped from the left */
        int left_cut = cut - (x1 - x);
        sgl_spectrum_span_cut(dst, n, left_cut < 0 ? 0 : left_cut, color);
    }
    else {
        sgl_spectrum_span_set(dst, n, color);
    }
}

static void sgl_spectrum_draw_bar(sgl_surf_t *surf, const sgl_spectrum_t *spectrum, int i, const sgl_area_t *clip)
{
    int16_t x  = spectrum->bar_x[i];
    int16_t w  = spectrum->bar_width;
    int16_t y2 = spectrum->obj.coords.y2;
    int16_t h  = (int16_t)spectrum->bar_value[i];
    int16_t y1, y1c, y2c;
    uint8_t r, cap;
    sgl_color_t color;

    if (w <= 0 || h <= 0) {
        return;
    }

    r = sgl_spectrum_effective_radius(spectrum);

    y1  = y2 - h + 1;
    y1c = sgl_max(y1, clip->y1);
    y2c = sgl_min(y2, clip->y2);
    if (y1c > y2c) {
        return;
    }

    if (spectrum->bar_mode & SGL_SPECTRUM_MODE_BLOCK) {
        /* segmented LED style: light only the rows inside a block; rows are
         * counted from the bar bottom so the lit segments stack upward */
        int16_t step = (int16_t)spectrum->bar_hat_height + 1;

        for (int16_t y = y1c; y <= y2c; y++) {
            int row = y2 - y;

            if (row % step >= spectrum->bar_hat_height) {
                continue;                       /* gap row */
            }
            color = spectrum->bar_gradient
                  ? sgl_spectrum_row_color(spectrum, h, y - y1)
                  : spectrum->bar_color;
            sgl_spectrum_draw_row(surf, spectrum, x, y, w, 0, color);
        }
        return;
    }

    /* solid bar: only the top `r` rows carry the rounded cap */
    cap = (h > r) ? r : 0;

    for (int16_t y = y1c; y <= y2c; y++) {
        int from_top = y - y1;              /* 0 at the bar top           */
        uint8_t cut = (from_top < cap) ? spectrum->cut[from_top] : 0;

        color = spectrum->bar_gradient
              ? sgl_spectrum_row_color(spectrum, h, from_top)
              : spectrum->bar_color;
        sgl_spectrum_draw_row(surf, spectrum, x, y, w, cut, color);
    }
}

static void sgl_spectrum_draw_peak(sgl_surf_t *surf, const sgl_spectrum_t *spectrum, int i, const sgl_area_t *clip)
{
    int16_t h = (int16_t)spectrum->bar_peek[i];

    if (h <= 0 || !(spectrum->bar_mode & SGL_SPECTRUM_MODE_HAT_FLAG)) {
        return;
    }

    int16_t x     = spectrum->bar_x[i];
    int16_t w     = spectrum->bar_width;
    int16_t y2    = spectrum->obj.coords.y2;
    uint8_t cap_h = spectrum->bar_hat_height ? spectrum->bar_hat_height : 3;
    int16_t yb    = sgl_min((int16_t)(y2 - h - 1), clip->y2);   /* 1 px gap  */
    int16_t y1    = sgl_max((int16_t)(yb - cap_h + 1), clip->y1);

    for (int16_t y = y1; y <= yb; y++) {
        sgl_spectrum_draw_row(surf, spectrum, x, y, w, 0, spectrum->bar_hat_color);
    }
}

static void sgl_spectrum_construct_cb(sgl_surf_t *surf, sgl_obj_t *obj, sgl_event_t *evt)
{
    sgl_spectrum_t *spectrum = sgl_container_of(obj, sgl_spectrum_t, obj);
    sgl_area_t clip;

    if (evt->type != SGL_EVENT_DRAW_MAIN) {
        return;
    }

    sgl_spectrum_check_layout(spectrum);

    /* clip against the surface slice, then against the object area */
    if (!sgl_area_clip((sgl_area_t *)surf, &obj->coords, &clip)) {
        return;
    }
    sgl_area_selfclip(&clip, &obj->area);

    /* optional floor / baseline band behind the bars */
    if (spectrum->floor_height > 0) {
        sgl_area_t floor = {
            .x1 = obj->coords.x1,
            .y1 = (int16_t)(obj->coords.y2 - spectrum->floor_height + 1),
            .x2 = obj->coords.x2,
            .y2 = obj->coords.y2,
        };
        sgl_draw_fill_rect(surf, &obj->area, &floor, 0,
                           spectrum->floor_color, SGL_ALPHA_MAX);
    }

    for (int i = 0; i < spectrum->bar_num; i++) {
        int16_t x = spectrum->bar_x[i];
        int16_t w = spectrum->bar_width;

        /* skip columns outside the dirty area */
        if (w <= 0 || x + w - 1 < clip.x1 || x > clip.x2) {
            continue;
        }
        sgl_spectrum_draw_peak(surf, spectrum, i, &clip);
        sgl_spectrum_draw_bar(surf, spectrum, i, &clip);
    }
}

/**
 * @brief create a spectrum object
 * @param parent parent of the spectrum
 * @return spectrum object
 */
sgl_obj_t* sgl_spectrum_create(sgl_obj_t* parent)
{
    sgl_spectrum_t *spectrum = sgl_malloc(sizeof(sgl_spectrum_t));
    if (spectrum == NULL) {
        SGL_LOG_ERROR("sgl_spectrum_create: malloc failed");
        return NULL;
    }

    /* set object all member to zero */
    memset(spectrum, 0, sizeof(sgl_spectrum_t));

    sgl_obj_t *obj = &spectrum->obj;
    sgl_obj_init(&spectrum->obj, parent);
    obj->construct_fn = sgl_spectrum_construct_cb;

    spectrum->alpha           = SGL_THEME_ALPHA;
    spectrum->bar_color       = SGL_THEME_BG_COLOR;
    spectrum->bar_color_low   = SGL_THEME_BG_COLOR;
    spectrum->bar_hat_color   = sgl_color_mixer(SGL_THEME_BG_COLOR, SGL_THEME_COLOR, 128);
    spectrum->floor_color     = SGL_THEME_BG_COLOR;
    spectrum->bar_mode        = SGL_SPECTRUM_MODE_BLOCK;
    spectrum->bar_gap         = 4;
    spectrum->bar_hat_height  = 3;
    spectrum->peak_fall       = 1;
    spectrum->gap_auto        = 1;
    spectrum->align_center    = 1;

    return obj;
}

/**
 * @brief set spectrum bar number
 * @param obj spectrum object
 * @param number bar number
 * @return none
 * @note re-layouts the bars and resets the value/peak buffers; safe to call
 *       again with a different number.
 */
void sgl_spectrum_set_bar_number(sgl_obj_t *obj, uint16_t number)
{
    sgl_spectrum_t *spectrum = sgl_container_of(obj, sgl_spectrum_t, obj);

    /* clamp to the fixed buffer size */
    if (number > SGL_SPECTRUM_BAR_MAX) {
        number = SGL_SPECTRUM_BAR_MAX;
    }

    spectrum->bar_num = number;
    memset(spectrum->bar_value, 0, sizeof(spectrum->bar_value));
    memset(spectrum->bar_peek, 0, sizeof(spectrum->bar_peek));

    sgl_spectrum_layout(spectrum);
    sgl_spectrum_update_grad(spectrum);
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set spectrum bar value
 * @param obj spectrum object
 * @param index bar index
 * @param value bar value, 0..widget height in px (clamped)
 * @return none
 * @note only the rows that actually changed are marked dirty, so a value
 *       update costs O(delta) instead of O(widget height).
 */
void sgl_spectrum_set_bar_value(sgl_obj_t *obj, uint16_t index, uint16_t value)
{
    sgl_spectrum_t *spectrum = sgl_container_of(obj, sgl_spectrum_t, obj);
    int16_t old, new, top, peek_old, peek_new;
    sgl_area_t area;

    if (index >= spectrum->bar_num) {
        return;
    }

    /* clamp: value is a pixel height, cannot exceed the widget inner height */
    if (value > spectrum->bar_height) {
        value = spectrum->bar_height;
    }

    old      = (int16_t)spectrum->bar_value[index];
    new      = (int16_t)value;
    peek_old = (int16_t)spectrum->bar_peek[index];
    peek_new = peek_old;

    /* peak hold: rise instantly, fall by peak_fall px per update */
    if (new >= peek_old) {
        peek_new = new;
    }
    else if (spectrum->peak_fall > 0) {
        peek_new = peek_old - (int16_t)spectrum->peak_fall;
        if (peek_new < new) {
            peek_new = new;
        }
    }
    spectrum->bar_peek[index] = (uint16_t)(peek_new > 0 ? peek_new : 0);

    if (new == old && peek_new == peek_old) {
        return;
    }

    /* dirty region = union of the old and new bar rects + peak cap.  The
     * cap rides `cap_h` px ABOVE the peak line, so the top margin must
     * cover the higher of the two cap positions or old cap pixels linger. */
    uint8_t cap_h = spectrum->bar_hat_height ? spectrum->bar_hat_height : 3;
    top     = obj->coords.y2 - spectrum->bar_height;
    area.x1 = spectrum->bar_x[index];
    area.x2 = area.x1 + spectrum->bar_width - 1;
    area.y1 = sgl_max(top, obj->coords.y2 - (sgl_max(sgl_max(old, new),
                                                     sgl_max(peek_old, peek_new)) + (int16_t)cap_h));
    area.y2 = obj->coords.y2;

    spectrum->bar_value[index] = value;

    sgl_update_area(&area);
}

/**
 * @brief set spectrum bar mode
 * @param obj spectrum object
 * @param mode bar mode
 * @return none
 * @note mode value:
 *       SGL_SPECTRUM_MODE_BAR: bar mode
 *       SGL_SPECTRUM_MODE_BLOCK: block mode
 *       SGL_SPECTRUM_MODE_BAR_HAT: bar mode with hat
 *       SGL_SPECTRUM_MODE_BLOCK_HAT: block mode with hat
 */
void sgl_spectrum_set_bar_mode(sgl_obj_t *obj, uint8_t mode)
{
    sgl_spectrum_t *spectrum = sgl_container_of(obj, sgl_spectrum_t, obj);

    spectrum->bar_mode = mode;
    spectrum->bar_gradient = !!(mode & SGL_SPECTRUM_MODE_GRADIENT);
    sgl_spectrum_update_grad(spectrum);
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set spectrum bar color
 * @param obj spectrum object
 * @param color bar color
 * @return none
 */
void sgl_spectrum_set_bar_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_spectrum_t *spectrum = sgl_container_of(obj, sgl_spectrum_t, obj);
    spectrum->bar_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the bottom (gradient) colour of the bars
 * @param obj spectrum object
 * @param color colour at the bottom of a full-height bar
 * @return none
 * @note setting this enables the gradient unless SGL_SPECTRUM_MODE_GRADIENT
 *       was explicitly cleared.
 */
void sgl_spectrum_set_bar_color_low(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_spectrum_t *spectrum = sgl_container_of(obj, sgl_spectrum_t, obj);
    spectrum->bar_color_low = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set spectrum bar hat color
 * @param obj spectrum object
 * @param color bar hat color
 * @return none
 */
void sgl_spectrum_set_bar_hat_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_spectrum_t *spectrum = sgl_container_of(obj, sgl_spectrum_t, obj);
    spectrum->bar_hat_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set spectrum bar hat height
 * @param obj spectrum object
 * @param height bar hat height
 * @return none
 */
void sgl_spectrum_set_bar_hat_height(sgl_obj_t *obj, uint8_t height)
{
    sgl_spectrum_t *spectrum = sgl_container_of(obj, sgl_spectrum_t, obj);
    spectrum->bar_hat_height = height;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the corner radius of the bar tops
 * @param obj spectrum object
 * @param radius radius in px, clamped to half the bar width
 * @return none
 */
void sgl_spectrum_set_radius(sgl_obj_t *obj, uint8_t radius)
{
    sgl_spectrum_t *spectrum = sgl_container_of(obj, sgl_spectrum_t, obj);

    if (radius > SGL_SPECTRUM_ROUND_MAX) {
        radius = SGL_SPECTRUM_ROUND_MAX;
    }
    spectrum->round_radius = radius;
    sgl_spectrum_build_round_cut(radius, spectrum->cut);
    sgl_obj_set_dirty(obj);
}

/**
 * @brief enable or disable the vertical gradient
 * @param obj spectrum object
 * @param enable true to interpolate bar_color -> bar_color_low
 * @return none
 */
void sgl_spectrum_set_gradient(sgl_obj_t *obj, bool enable)
{
    sgl_spectrum_t *spectrum = sgl_container_of(obj, sgl_spectrum_t, obj);
    spectrum->bar_gradient = enable ? 1 : 0;
    sgl_spectrum_update_grad(spectrum);
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the peak-hold fall speed
 * @param obj spectrum object
 * @param fall_px pixels the cap drops per value update, 0 holds forever
 * @return none
 */
void sgl_spectrum_set_peak_fall(sgl_obj_t *obj, uint8_t fall_px)
{
    sgl_spectrum_t *spectrum = sgl_container_of(obj, sgl_spectrum_t, obj);
    spectrum->peak_fall = fall_px;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set a floor / baseline band under the bars
 * @param obj spectrum object
 * @param height band height in px, 0 disables it
 * @param color band color
 * @return none
 */
void sgl_spectrum_set_floor(sgl_obj_t *obj, uint8_t height, sgl_color_t color)
{
    sgl_spectrum_t *spectrum = sgl_container_of(obj, sgl_spectrum_t, obj);
    spectrum->floor_height = height;
    spectrum->floor_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief force a gap between bars instead of the automatic one
 * @param obj spectrum object
 * @param gap gap in px, 0 restores automatic gap selection
 * @return none
 */
void sgl_spectrum_set_bar_gap(sgl_obj_t *obj, uint8_t gap)
{
    sgl_spectrum_t *spectrum = sgl_container_of(obj, sgl_spectrum_t, obj);
    spectrum->gap_auto = 1;                 /* kept for API compatibility */
    spectrum->bar_gap = gap ? gap : 4;
    sgl_spectrum_layout(spectrum);
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set spectrum alpha
 * @param obj spectrum object
 * @param alpha alpha value
 * @return none
 */
void sgl_spectrum_set_alpha(sgl_obj_t *obj, uint8_t alpha)
{
    sgl_spectrum_t *spectrum = sgl_container_of(obj, sgl_spectrum_t, obj);
    spectrum->alpha = alpha;
    sgl_obj_set_dirty(obj);
}
