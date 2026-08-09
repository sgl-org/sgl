/* source/draw/sgl_draw_bezier.c
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

/* Lower/upper bounds on the number of straight segments a curve is split into.
 * Too few looks polygonal; too many wastes time without visible benefit. */
#define SGL_BEZIER_MIN_SEGMENTS          4
#define SGL_BEZIER_MAX_SEGMENTS          96
/* Target chord length (in pixels) per segment when choosing the segment count */
#define SGL_BEZIER_PIXELS_PER_SEGMENT    4

/* Mask geometry helpers (inclusive rectangle). sgl_bezier_mask_t is declared in sgl_draw.h */
#define SGL_BEZIER_MASK_W(m)             ((m)->x2 - (m)->x1 + 1)
#define SGL_BEZIER_MASK_H(m)             ((m)->y2 - (m)->y1 + 1)

/**
 * @brief Integer distance between two points (rounded).
 */
static int32_t sgl_bezier_dist(int32_t ax, int32_t ay, int32_t bx, int32_t by)
{
    const int32_t dx = bx - ax;
    const int32_t dy = by - ay;
    return (int32_t)sgl_sqrt((uint32_t)(dx * dx + dy * dy));
}

/**
 * @brief Pick a segment count from the control polygon length.
 */
static int32_t sgl_bezier_segments(int32_t poly_len)
{
    int32_t segments = poly_len / SGL_BEZIER_PIXELS_PER_SEGMENT;
    if (segments < SGL_BEZIER_MIN_SEGMENTS) segments = SGL_BEZIER_MIN_SEGMENTS;
    if (segments > SGL_BEZIER_MAX_SEGMENTS) segments = SGL_BEZIER_MAX_SEGMENTS;
    return segments;
}

/**
 * @brief Stroke one straight segment into the coverage mask (overlap = max).
 *        Endpoints are Q8 fixed-point (subpixel) to keep joints seamless.
 */
static void sgl_bezier_stroke_mask(sgl_bezier_mask_t *mask,
                                   int32_t x1, int32_t y1, int32_t x2, int32_t y2,
                                   int16_t thickness)
{
    const int32_t bax = x2 - x1;
    const int32_t bay = y2 - y1;
    const int64_t b_sqd = (int64_t)bax * bax + (int64_t)bay * bay;
    const int32_t inner_limit = (thickness - 1) << 8;
    const int32_t outer_limit = (thickness + 1) << 8;
    const int32_t aa_range = outer_limit - inner_limit;
    
    const int32_t stride = SGL_BEZIER_MASK_W(mask);

    const int32_t ix1 = x1 >> 8;
    const int32_t ix2 = x2 >> 8;
    const int32_t iy1 = y1 >> 8;
    const int32_t iy2 = y2 >> 8;
    const int32_t min_x = (ix1 < ix2) ? ix1 : ix2;
    const int32_t max_x = (ix1 > ix2) ? ix1 : ix2;
    const int32_t min_y = (iy1 < iy2) ? iy1 : iy2;
    const int32_t max_y = (iy1 > iy2) ? iy1 : iy2;

    int32_t cx1 = min_x - thickness - 2;
    int32_t cx2 = max_x + thickness + 2;
    int32_t cy1 = min_y - thickness - 2;
    int32_t cy2 = max_y + thickness + 2;

    if (cx1 < mask->x1) cx1 = mask->x1;
    if (cy1 < mask->y1) cy1 = mask->y1;
    if (cx2 > mask->x2) cx2 = mask->x2;
    if (cy2 > mask->y2) cy2 = mask->y2;
    if (cx1 > cx2 || cy1 > cy2) return;

    /* Degenerate (zero-length) segment: a single round dot of radius thickness. */
    if (b_sqd == 0) {
        for (int32_t y = cy1; y <= cy2; y++) {
            uint8_t *row = mask->data + (int64_t)(y - mask->y1) * stride + (cx1 - mask->x1);
            for (int32_t x = cx1; x <= cx2; x++, row++) {
                const int32_t dx = (x << 8) + 128 - x1;
                const int32_t dy = (y << 8) + 128 - y1;
                const uint32_t dsq = (uint32_t)((int64_t)dx * dx + (int64_t)dy * dy);
                const int32_t dist_q8 = (int32_t)sgl_sqrt(dsq);
                uint8_t cov = 0;
                if (dist_q8 < inner_limit) cov = 255;
                else if (dist_q8 < outer_limit && aa_range > 0)
                    cov = (uint8_t)((outer_limit - dist_q8) * 255 / aa_range);
                if (cov > *row) *row = cov;
            }
        }
        return;
    }

    const int32_t len_q8 = (int32_t)sgl_sqrt((uint32_t)b_sqd);

    for (int32_t y = cy1; y <= cy2; y++) {
        uint8_t *row = mask->data + (int64_t)(y - mask->y1) * stride + (cx1 - mask->x1);
        const int32_t py = (y << 8) + 128 - y1;

        for (int32_t x = cx1; x <= cx2; x++, row++) {
            const int32_t px = (x << 8) + 128 - x1;
            const int64_t cur_dot   = (int64_t)px * bax + (int64_t)py * bay;
            const int64_t cur_cross = (int64_t)px * bay - (int64_t)py * bax;
            int32_t dist_q8;

            if (cur_dot >= 0 && cur_dot <= b_sqd) {
                const int64_t abs_cross = (cur_cross >= 0) ? cur_cross : -cur_cross;
                dist_q8 = (int32_t)(abs_cross / len_q8);
            }
            else {
                const int32_t ex = (cur_dot < 0) ? x1 : x2;
                const int32_t ey = (cur_dot < 0) ? y1 : y2;
                const int32_t dx = (x << 8) + 128 - ex;
                const int32_t dy = (y << 8) + 128 - ey;
                const uint32_t dsq = (uint32_t)((int64_t)dx * dx + (int64_t)dy * dy);
                dist_q8 = (int32_t)sgl_sqrt(dsq);
            }

            uint8_t cov = 0;
            if (dist_q8 < inner_limit) {
                cov = 255;
            }
            else if (dist_q8 < outer_limit) {
                cov = (uint8_t)((uint32_t)((outer_limit - dist_q8) * 255 / aa_range));
            }

            if (cov > *row) *row = cov;   /* overlap takes the max, never accumulates */
        }
    }
}

/**
 * @brief Blend the whole coverage mask onto the surface once, using curve alpha.
 */
static void sgl_bezier_blend_mask(sgl_surf_t *surf, sgl_area_t *area,
                                const sgl_bezier_mask_t *mask,
                                sgl_color_t color, uint8_t alpha)
{
    sgl_area_t clip = SGL_AREA_MAX;
    sgl_area_t m_rect = {
        .x1 = (int16_t)mask->x1,
        .y1 = (int16_t)mask->y1,
        .x2 = (int16_t)mask->x2,
        .y2 = (int16_t)mask->y2,
    };

    sgl_surf_clip_area_return(surf, area, &clip);
    if (!sgl_area_selfclip(&clip, &m_rect)) return;
    const int32_t surf_stride = surf->w;
    const int32_t mask_stride = SGL_BEZIER_MASK_W(mask);
    sgl_color_t *row_base = sgl_surf_get_buf(surf, clip.x1 - surf->x1, clip.y1 - surf->y1);

    for (int32_t y = clip.y1; y <= clip.y2; y++) {
        sgl_color_t *blend = row_base;
        const uint8_t *mrow = mask->data + (int64_t)(y - mask->y1) * mask_stride + (clip.x1 - mask->x1);

        for (int32_t x = clip.x1; x <= clip.x2; x++, blend++, mrow++) {
            const uint8_t cov = *mrow;
            if (cov == 0) continue;
            const uint8_t final_a = (alpha == SGL_ALPHA_MAX) ? cov : (uint8_t)(((uint32_t)cov * alpha + 127) / 255);
            *blend = sgl_color_mixer(color, *blend, final_a);
        }
        row_base += surf_stride;
    }
}

/**
 * @brief Number of data bytes a mask rectangle needs (1 byte per pixel).
 */
uint32_t sgl_bezier_mask_bytes(const sgl_bezier_mask_t *mask)
{
    const int32_t w = SGL_BEZIER_MASK_W(mask);
    const int32_t h = SGL_BEZIER_MASK_H(mask);
    if (w <= 0 || h <= 0) return 0;
    return (uint32_t)w * (uint32_t)h;
}

/**
 * @brief Draw a quadratic bezier curve (anti-aliased, round-capped, variable width)
 */
void sgl_draw_bezier_quad(sgl_surf_t *surf, sgl_area_t *area,
                          int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                          int16_t x2, int16_t y2,
                          int16_t thickness, sgl_color_t color, uint8_t alpha,
                          sgl_bezier_mask_t *mask)
{
    if (alpha == SGL_ALPHA_MIN || thickness <= 0) return;
    const int32_t poly_len = sgl_bezier_dist(x0, y0, x1, y1) +
                             sgl_bezier_dist(x1, y1, x2, y2);
    const int32_t segments = sgl_bezier_segments(poly_len);
    const int64_t denom = (int64_t)segments * segments;
    const int64_t half  = denom / 2;

    if (mask == NULL || mask->data == NULL) return;
    const int64_t n = (int64_t)SGL_BEZIER_MASK_W(mask) * SGL_BEZIER_MASK_H(mask);

    for (int64_t i = 0; i < n; i++) {
        mask->data[i] = 0;
    }

    int32_t prev_x = (int32_t)x0 << 8;
    int32_t prev_y = (int32_t)y0 << 8;

    for (int32_t i = 1; i <= segments; i++) {
        int32_t cur_x, cur_y;
        if (i == segments) {
            cur_x = (int32_t)x2 << 8;
            cur_y = (int32_t)y2 << 8;
        } else {
            const int64_t t = i;
            const int64_t u = segments - i;
            const int64_t numx = u * u * x0 + 2 * u * t * x1 + t * t * x2;
            const int64_t numy = u * u * y0 + 2 * u * t * y1 + t * t * y2;
            cur_x = (int32_t)((numx * 256 + half) / denom);
            cur_y = (int32_t)((numy * 256 + half) / denom);
        }
        sgl_bezier_stroke_mask(mask, prev_x, prev_y, cur_x, cur_y, thickness);
        prev_x = cur_x;
        prev_y = cur_y;
    }
    sgl_bezier_blend_mask(surf, area, mask, color, alpha);
}

/**
 * @brief Draw a cubic bezier curve (anti-aliased, round-capped, variable width)
 */
void sgl_draw_bezier_cubic(sgl_surf_t *surf, sgl_area_t *area,
                           int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                           int16_t x2, int16_t y2, int16_t x3, int16_t y3,
                           int16_t thickness, sgl_color_t color, uint8_t alpha,
                           sgl_bezier_mask_t *mask)
{
    if (alpha == SGL_ALPHA_MIN || thickness <= 0) return;
    const int32_t poly_len = sgl_bezier_dist(x0, y0, x1, y1) +
                             sgl_bezier_dist(x1, y1, x2, y2) +
                             sgl_bezier_dist(x2, y2, x3, y3);
    const int32_t segments = sgl_bezier_segments(poly_len);
    const int64_t denom = (int64_t)segments * segments * segments;
    const int64_t half  = denom / 2;

    if (mask == NULL || mask->data == NULL) return;

    const int64_t n = (int64_t)SGL_BEZIER_MASK_W(mask) * SGL_BEZIER_MASK_H(mask);
    for (int64_t i = 0; i < n; i++) mask->data[i] = 0;

    int32_t prev_x = (int32_t)x0 << 8;
    int32_t prev_y = (int32_t)y0 << 8;

    for (int32_t i = 1; i <= segments; i++) {
        int32_t cur_x, cur_y;
        if (i == segments) {
            cur_x = (int32_t)x3 << 8;
            cur_y = (int32_t)y3 << 8;
        } else {
            const int64_t t  = i;
            const int64_t u  = segments - i;
            const int64_t u2 = u * u;
            const int64_t t2 = t * t;

            const int64_t numx = u2 * u * x0 + 3 * u2 * t * x1 + 3 * u * t2 * x2 + t2 * t * x3;
            const int64_t numy = u2 * u * y0 + 3 * u2 * t * y1 + 3 * u * t2 * y2 + t2 * t * y3;
            cur_x = (int32_t)((numx * 256 + half) / denom);
            cur_y = (int32_t)((numy * 256 + half) / denom);
        }
        sgl_bezier_stroke_mask(mask, prev_x, prev_y, cur_x, cur_y, thickness);
        prev_x = cur_x;
        prev_y = cur_y;
    }

    sgl_bezier_blend_mask(surf, area, mask, color, alpha);
}
