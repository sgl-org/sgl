/* source/widgets/sgl_scope.c
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
#include <sgl_log.h>
#include <sgl_mm.h>
#include <sgl_cfgfix.h>
#include <sgl_theme.h>
#include "sgl_scope.h"

/* alpha used for the quarter grid lines */
#define SGL_SCOPE_GRID_ALPHA            (90)

/**
 * @brief derive the ring capacity from the current widget size and reset the
 *        FIFO state whenever it changes (buffer length must match plot width)
 */
static uint16_t scope_capacity(sgl_scope_t *scope)
{
    sgl_obj_t *obj = &scope->obj;
    int16_t w = (obj->coords.x2 - obj->coords.x1 + 1) - 2 * (int16_t)scope->border_width;

    if (w <= 0)
        return 0;

    if (scope->cap != (uint16_t)w) {
        scope->cap = (uint16_t)w;
        for (uint8_t c = 0; c < SGL_SCOPE_MAX_CHANNELS; c++) {
            scope->in[c] = 0;
            scope->out[c] = 0;
            scope->count[c] = 0;
        }
    }
    return scope->cap;
}

/**
 * @brief map a sample value to a pixel row, Q16 fixed point: one multiply
 *        and one shift per sample, no division in the hot loop
 */
static inline int16_t scope_map_y(int16_t v, int16_t v_min, int32_t scale_q16, int16_t y_top, int16_t y_bot)
{
    int32_t y = (int32_t)y_bot - (((int32_t)(v - v_min) * scale_q16) >> 16);

    if (y < y_top)
        y = y_top;
    if (y > y_bot)
        y = y_bot;
    return (int16_t)y;
}

/**
 * @brief draw one channel, the fastest path: buffer length == plot width so
 *        each sample owns exactly one pixel column; pixels are written
 *        straight into the surface buffer with no anti-aliasing and no line
 *        rasterizer. Consecutive samples are joined by a vertical span so
 *        fast signals never leave gaps.
 */
static void scope_draw_channel(sgl_surf_t *surf, sgl_scope_t *scope, uint8_t ch,
                               const sgl_area_t *plot, const sgl_area_t *clip, int32_t scale_q16)
{
    uint16_t cap = scope->cap;
    uint16_t n = scope->count[ch];
    uint16_t out = scope->out[ch];
    uint8_t alpha = scope->alpha;
    int16_t stride = surf->w;
    int16_t y_top = plot->y1;
    int16_t y_bot = plot->y2;
    uint16_t idx;
    int16_t sc;

    if (n == 0)
        return;

    const int16_t *buf_data = scope->data_buffers + (int32_t)ch * cap;
    sgl_color_t color = scope->wave_colors ? scope->wave_colors[ch] : SGL_COLOR_GREEN;

    /* visible column range = plot clipped to the dirty area */
    int16_t x_from = sgl_max(plot->x1, clip->x1);
    int16_t x_to = sgl_min(plot->x2, clip->x2);
    if (x_from > x_to)
        return;

    sc = x_from - plot->x1;
    if (sc >= (int16_t)n)
        return;                    /* waveform has not grown this wide yet */

    /* seed prev_y with the sample just left of the clip window so the
     * vertical join across the clip edge stays exact */
    idx = out + (uint16_t)(sc > 0 ? sc - 1 : 0);
    if (idx >= cap)
        idx -= cap;
    int16_t prev_y = scope_map_y(buf_data[idx], scope->v_min, scale_q16, y_top, y_bot);

    /* anchor the write pointer at the clip top-left once, sgl_led style:
     * p stays alive across the whole sweep and is only ever advanced by
     * additions (p++ for columns, p += stride for rows), no multiply */
    sgl_color_t *p = sgl_surf_get_buf(surf, clip->x1 - surf->x1, clip->y1 - surf->y1) + (x_from - clip->x1);
    int16_t row = clip->y1;                  /* row that p currently sits on */

    for (int16_t x = x_from; x <= x_to; x++, sc++) {
        if (sc >= (int16_t)n)
            break;

        /* sample index in the ring FIFO (out points to the oldest) */
        idx = out + (uint16_t)sc;
        if (idx >= cap)
            idx -= cap;

        int16_t y = scope_map_y(buf_data[idx], scope->v_min, scale_q16, y_top, y_bot);

        /* vertical span between the previous and the current sample,
         * clamped to [clip->y1, clip->y2]. In a partial-slice redraw the
         * span may lie fully outside the clip (above or below): such a
         * column must be skipped WITHOUT touching p/row, otherwise the
         * additive pointer walk desyncs from `row` and later columns
         * write out of the slice bounds (crash on merged dirty areas,
         * e.g. an exit msgbox popping over the scope) */
        int16_t lo = y < prev_y ? y : prev_y;
        int16_t hi = y > prev_y ? y : prev_y;
        if (hi < clip->y1 || lo > clip->y2) {
            /* span fully outside the clip: keep p/row in sync, hop column */
            p++;
            prev_y = y;
            continue;
        }
        if (lo < clip->y1)
            lo = clip->y1;
        if (hi > clip->y2)
            hi = clip->y2;
        /* span and clip now overlap, so the clamp above guarantees lo <= hi */

        /* walk p onto the span top additively */
        while (row < lo) { p += stride; row++; }
        while (row > lo) { p -= stride; row--; }

        if (alpha == 255) {
            for (int16_t yy = lo; yy <= hi; yy++, p += stride)
                *p = color;
        } else {
            for (int16_t yy = lo; yy <= hi; yy++, p += stride)
                *p = sgl_color_mixer(color, *p, alpha);
        }
        /* the fill loop leaves p one row past hi (like led's buf += w) */
        row = hi + 1;
        p++;                                 /* hop into the next column */
        prev_y = y;
    }
}

static void sgl_scope_construct_cb(sgl_surf_t *surf, sgl_obj_t *obj, sgl_event_t *evt)
{
    sgl_scope_t *scope = sgl_container_of(obj, sgl_scope_t, obj);

    if (evt->type != SGL_EVENT_DRAW_MAIN)
        return;

    /* background and border in one pass */
    sgl_draw_rect_t bg;
    memset(&bg, 0, sizeof(bg));
    bg.alpha = 255;
    bg.color = scope->bg_color;
    bg.border = scope->border_width;
    bg.border_alpha = 255;
    bg.border_color = scope->border_color;
    bg.radius = obj->radius;
    sgl_draw_rect(surf, &obj->area, &obj->coords, &bg);

    if (scope->data_buffers == NULL || scope->channel_count == 0)
        return;

    uint16_t cap = scope_capacity(scope);
    if (cap == 0)
        return;

    /* plot area inside the border; width == ring capacity by construction */
    int16_t bw = scope->border_width;
    sgl_area_t plot;
    plot.x1 = obj->coords.x1 + bw;
    plot.y1 = obj->coords.y1 + bw;
    plot.x2 = obj->coords.x2 - bw;
    plot.y2 = obj->coords.y2 - bw;
    int16_t h = plot.y2 - plot.y1 + 1;
    if (h <= 0)
        return;

    /* quarter grid lines, cheap: six thin fill rects per frame */
    sgl_area_t line;
    for (int16_t g = 1; g < 4; g++) {
        line.x1 = plot.x1 + (int16_t)(cap * g / 4);
        line.x2 = line.x1;
        line.y1 = plot.y1;
        line.y2 = plot.y2;
        sgl_draw_fill_rect(surf, &obj->area, &line, 0, scope->grid_color, SGL_SCOPE_GRID_ALPHA);

        line.x1 = plot.x1;
        line.x2 = plot.x2;
        line.y1 = plot.y1 + (int16_t)(h * g / 4);
        line.y2 = line.y1;
        sgl_draw_fill_rect(surf, &obj->area, &line, 0, scope->grid_color, SGL_SCOPE_GRID_ALPHA);
    }

    /* value range -> pixel span, precomputed once per frame */
    int32_t span = (int32_t)scope->v_max - scope->v_min;
    if (span <= 0)
        return;
    int32_t scale_q16 = ((int32_t)(h - 1) << 16) / span;

    /* direct pixel path: must stay inside both the slice owned by this
     * object (obj->area) and the surface bounds, otherwise partial-slice
     * draws would write into neighbour objects or past the buffer */
    sgl_area_t clip;
    if (!sgl_area_clip((sgl_area_t *)surf, &obj->area, &clip))
        return;
    if (!sgl_area_selfclip(&clip, &plot))
        return;

    for (uint8_t ch = 0; ch < scope->channel_count; ch++)
        scope_draw_channel(surf, scope, ch, &plot, &clip, scale_q16);
}

/**
 * @brief create scope object
 */
sgl_obj_t* sgl_scope_create(sgl_obj_t* parent)
{
    sgl_scope_t *scope = sgl_malloc(sizeof(sgl_scope_t));
    if (scope == NULL) {
        SGL_LOG_ERROR("sgl_scope_create: malloc failed");
        return NULL;
    }

    memset(scope, 0, sizeof(sgl_scope_t));

    sgl_obj_t *obj = &scope->obj;
    sgl_obj_init(obj, parent);
    obj->construct_fn = sgl_scope_construct_cb;

    scope->bg_color = SGL_COLOR_BLACK;
    scope->grid_color = SGL_COLOR_GRAY;
    scope->border_color = SGL_THEME_BORDER_COLOR;
    scope->border_width = 1;
    sgl_obj_set_border_width(obj, 1);
    scope->alpha = 255;
    scope->v_min = -32768;
    scope->v_max = 32767;

    return obj;
}

/**
 * @brief bind the sample/color arrays to the scope
 */
void sgl_scope_set_buffers(sgl_obj_t *obj, int16_t *data_buffers, sgl_color_t *wave_colors, uint8_t channel_count)
{
    SGL_ASSERT(obj != NULL);
    sgl_scope_t *scope = sgl_container_of(obj, sgl_scope_t, obj);

    if (channel_count == 0 || channel_count > SGL_SCOPE_MAX_CHANNELS)
        return;

    scope->data_buffers = data_buffers;
    scope->wave_colors = wave_colors;
    scope->channel_count = channel_count;
    scope->cap = 0;                     /* re-derive from the size later */
    for (uint8_t c = 0; c < SGL_SCOPE_MAX_CHANNELS; c++) {
        scope->in[c] = 0;
        scope->out[c] = 0;
        scope->count[c] = 0;
    }
    sgl_obj_set_dirty(obj);
}

/**
 * @brief producer side of the per-channel ring FIFO
 * @note pushes dirty rectangles instead of the whole-widget dirty flag:
 *       while the ring is still filling up only the vertical span of the
 *       newest column changes; once the ring is full every new sample
 *       shifts the whole waveform left, so the plot is split into
 *       SGL_SCOPE_SCROLL_STRIPS equal-width strips and each strip is
 *       marked only as tall as the y extent of the samples it covers
 *       (the border never changes)
 */
void sgl_scope_append_data(sgl_obj_t* obj, uint8_t channel, int16_t value)
{
    SGL_ASSERT(obj != NULL);
    sgl_scope_t *scope = sgl_container_of(obj, sgl_scope_t, obj);

    if (scope->data_buffers == NULL || channel >= scope->channel_count)
        return;

    uint16_t cap = scope_capacity(scope);
    if (cap == 0)
        return;

    int16_t *buf = scope->data_buffers + (int32_t)channel * cap;
    bool scrolled = (scope->count[channel] >= cap);   /* ring already full */
    int16_t dropped = 0;
    if (scrolled) {
        /* ring full: in == out, the slot we are about to overwrite holds
         * the oldest sample that scrolls off the left edge this update */
        dropped = buf[scope->in[channel]];
    }

    buf[scope->in[channel]] = value;
    scope->in[channel]++;
    if (scope->in[channel] >= cap)
        scope->in[channel] = 0;

    if (!scrolled) {
        scope->count[channel]++;
    } else {
        /* ring full: oldest sample got overwritten, display consumes it */
        scope->out[channel]++;
        if (scope->out[channel] >= cap)
            scope->out[channel] = 0;
    }

    /* plot area inside the border, same as the draw pass */
    sgl_area_t area;
    int16_t bw = scope->border_width;
    area.x1 = obj->coords.x1 + bw;
    area.y1 = obj->coords.y1 + bw;
    area.x2 = obj->coords.x2 - bw;
    area.y2 = obj->coords.y2 - bw;

    int16_t h = area.y2 - area.y1 + 1;
    int32_t span = (int32_t)scope->v_max - scope->v_min;

    if (span <= 0 || h <= 0) {
        /* no valid value mapping, be conservative with the full plot */
        sgl_update_area(&area);
        return;
    }

    /* same Q16 mapping as scope_draw_channel, so the rectangles exactly
     * cover what the draw pass renders */
    int32_t scale_q16 = ((int32_t)(h - 1) << 16) / span;

    if (!scrolled) {
        uint16_t n = scope->count[channel];
        int16_t x = area.x1 + (int16_t)n - 1;
        int16_t y = scope_map_y(value, scope->v_min, scale_q16, area.y1, area.y2);
        int16_t prev_y = y;

        if (n >= 2) {
            uint16_t prev_idx = (scope->in[channel] + cap - 2) % cap;
            prev_y = scope_map_y(buf[prev_idx], scope->v_min, scale_q16, area.y1, area.y2);
        }

        area.x1 = area.x2 = x;
        area.y1 = sgl_min(y, prev_y);
        area.y2 = sgl_max(y, prev_y);
        sgl_update_area(&area);
        return;
    }

    /* ring full: the waveform shifted left by one column. Mark
     * SGL_SCOPE_SCROLL_STRIPS equal-width strips, each one as tall as
     * the y extent of the samples it covers. The pixels that change at
     * column c are bounded by the samples at c-2..c of the shifted
     * waveform: the previous frame drew span(c-2, c-1) there, the new
     * frame draws span(c-1, c).
     * The first strip also includes the dropped sample, whose last pixel
     * at the left edge must be erased */
    uint16_t out = scope->out[channel];
    for (int32_t q = 0; q < SGL_SCOPE_SCROLL_STRIPS; q++) {
        int32_t c1 = ((int32_t)cap * q) / SGL_SCOPE_SCROLL_STRIPS;
        int32_t c2 = ((int32_t)cap * (q + 1)) / SGL_SCOPE_SCROLL_STRIPS - 1;
        if (c1 > c2)
            continue;               /* cap < strips: this strip is empty */

        /* one extra sample on each side covers the vertical joins that
         * cross the strip borders */
        int32_t s1 = c1 - 2;
        int32_t s2 = c2 + 1;
        if (s1 < 0)
            s1 = 0;
        if (s2 > (int32_t)cap - 1)
            s2 = cap - 1;

        int16_t ymin = INT16_MAX;
        int16_t ymax = INT16_MIN;
        for (int32_t s = s1; s <= s2; s++) {
            uint16_t idx = (uint16_t)(((uint32_t)out + (uint32_t)s) % cap);
            int16_t y = scope_map_y(buf[idx], scope->v_min, scale_q16, area.y1, area.y2);
            if (y < ymin)
                ymin = y;
            if (y > ymax)
                ymax = y;
        }
        if (q == 0) {
            /* the dropped sample just scrolled off the left edge */
            int16_t y = scope_map_y(dropped, scope->v_min, scale_q16, area.y1, area.y2);
            if (y < ymin)
                ymin = y;
            if (y > ymax)
                ymax = y;
        }

        sgl_area_t strip = area;
        strip.x1 = area.x1 + (int16_t)c1;
        strip.x2 = area.x1 + (int16_t)c2;
        strip.y1 = ymin;
        strip.y2 = ymax;
        sgl_update_area(&strip);
    }
}

/**
 * @brief clear all samples of a channel, or of every channel with 0xFF
 */
void sgl_scope_clear(sgl_obj_t *obj, uint8_t channel)
{
    SGL_ASSERT(obj != NULL);
    sgl_scope_t *scope = sgl_container_of(obj, sgl_scope_t, obj);

    uint8_t first = (channel == 0xFF) ? 0 : channel;
    uint8_t last = (channel == 0xFF) ? (SGL_SCOPE_MAX_CHANNELS - 1) : channel;

    for (uint8_t c = first; c <= last && c < SGL_SCOPE_MAX_CHANNELS; c++) {
        scope->in[c] = 0;
        scope->out[c] = 0;
        scope->count[c] = 0;
    }
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the value range mapped to the widget height
 */
void sgl_scope_set_vrange(sgl_obj_t *obj, int16_t v_min, int16_t v_max)
{
    SGL_ASSERT(obj != NULL);
    sgl_scope_t *scope = sgl_container_of(obj, sgl_scope_t, obj);

    if (v_max <= v_min)
        return;

    scope->v_min = v_min;
    scope->v_max = v_max;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set scope waveform color for a specific channel
 */
void sgl_scope_set_channel_waveform_color(sgl_obj_t* obj, uint8_t channel, sgl_color_t color)
{
    SGL_ASSERT(obj != NULL);
    sgl_scope_t *scope = sgl_container_of(obj, sgl_scope_t, obj);

    if (scope->wave_colors == NULL || channel >= scope->channel_count)
        return;

    scope->wave_colors[channel] = color;
    sgl_obj_set_dirty(obj);
}
