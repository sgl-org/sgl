/* source/draw/sgl_draw_line.c
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
#include <sgl_log.h>
#include <sgl_draw.h>
#include <sgl_math.h>

/**
 * @brief draw a horizontal line with alpha
 * @param surf surface
 * @param area area that contains the line
 * @param y line y position
 * @param x1 line start x position
 * @param x2 line end x position
 * @param color line color
 * @param alpha alpha of color
 * @return none
 */
void sgl_draw_fill_hline(sgl_surf_t *surf, sgl_area_t *area, int16_t y, int16_t x1, int16_t x2, uint8_t width, sgl_color_t color, uint8_t alpha)
{
    sgl_color_t *buf = NULL, *blend = NULL;
    sgl_area_t c_rect = {.x1 = x1, .x2 = x2, .y1 = y - (width - 1) / 2, .y2 = y + width / 2}, clip = SGL_AREA_MAX;

    if (c_rect.x1 > c_rect.x2) {
        sgl_swap(&c_rect.x1, &c_rect.x2);
    }

    sgl_surf_clip_area_return(surf, area, &clip);
    if (!sgl_area_selfclip(&clip, &c_rect)) {
        return;
    }

    buf = sgl_surf_get_buf(surf,  clip.x1 - surf->x1, clip.y1 - surf->y1);
    for (int y = clip.y1; y <= clip.y2; y++) {
        blend = buf;
        for (int x = clip.x1; x <= clip.x2; x++, blend++) {
            *blend = alpha == SGL_ALPHA_MAX ? color : sgl_color_mixer(color, *blend, alpha);
        }
        buf += surf->w;
    }
}

/**
 * @brief draw a vertical line with alpha
 * @param surf surface
 * @param area area that contains the line
 * @param x x coordinate
 * @param y1 y1 coordinate
 * @param y2 y2 coordinate
 * @param color line color
 * @param alpha alpha of color
 * @return none
 */
void sgl_draw_fill_vline(sgl_surf_t *surf, sgl_area_t *area, int16_t x, int16_t y1, int16_t y2, uint8_t width, sgl_color_t color, uint8_t alpha)
{
    sgl_color_t *buf = NULL, *blend = NULL;
    sgl_area_t c_rect = {.x1 = x - (width - 1) / 2, .x2 = x + width / 2, .y1 = y1,.y2 = y2}, clip = SGL_AREA_MAX;

    if (c_rect.y1 > c_rect.y2) {
        sgl_swap(&c_rect.y1, &c_rect.y2);
    }

    sgl_surf_clip_area_return(surf, area, &clip);
    if (!sgl_area_selfclip(&clip, &c_rect)) {
        return;
    }

    buf = sgl_surf_get_buf(surf,  clip.x1 - surf->x1, clip.y1 - surf->y1);
    for (int y = clip.y1; y <= clip.y2; y++) {
        blend = buf;
        for (int x = clip.x1; x <= clip.x2; x++, blend++) {
            *blend = (alpha == SGL_ALPHA_MAX ? color : sgl_color_mixer(color, *blend, alpha));
        }
        buf += surf->w;
    }
}

/**
 * @brief draw a slanted line with alpha
 * @param surf surface
 * @param area area that contains the line
 * @param x1 line start x position
 * @param y1 line start y position
 * @param x2 line end x position
 * @param y2 line end y position
 * @param thickness line width
 * @param color line color
 * @param alpha alpha of color
 * @return none
 * @note This algorithm is SDF algorithm
 */
void sgl_draw_line_fill_slanted(sgl_surf_t *surf, sgl_area_t *area, int16_t x1, int16_t y1, int16_t x2, int16_t y2, int16_t thickness, sgl_color_t color, uint8_t alpha)
{
    const int32_t bax = (int32_t)x2 - x1;
    const int32_t bay = (int32_t)y2 - y1;
    const int32_t b_sqd = bax * bax + bay * bay;
    if (b_sqd == 0) return;
    const uint32_t sqrt_bsqd = sgl_sqrt((uint32_t)b_sqd);
    const int32_t inv_len = (int32_t)((65536LL + sqrt_bsqd / 2) / sqrt_bsqd);
    const int32_t inner_limit = (thickness - 1) << 8;
    const int32_t outer_limit = thickness << 8;
    const int32_t aa_range = outer_limit - inner_limit;
    sgl_area_t clip = SGL_AREA_MAX;
    sgl_area_t c_rect = {
        .x1 = (x1 < x2 ? x1 : x2) - thickness,
        .x2 = (x1 > x2 ? x1 : x2) + thickness,
        .y1 = (y1 < y2 ? y1 : y2) - thickness,
        .y2 = (y1 > y2 ? y1 : y2) + thickness,
    };

    sgl_surf_clip_area_return(surf, area, &clip);
    if (!sgl_area_selfclip(&clip, &c_rect)) return;
    const int32_t stride = surf->w;

    sgl_color_t *row_base = sgl_surf_get_buf(surf, clip.x1 - surf->x1, clip.y1 - surf->y1);
    const int64_t len       = (int64_t)sqrt_bsqd;
    const int64_t band_half = (int64_t)(thickness + 1) * len; /* |cross| threshold, +1 pad */
    const int32_t cap_r     = thickness + 1;                  /* endpoint cap radius, +1 pad */
    const int64_t cap_r2    = (int64_t)cap_r * cap_r;

    for (int y = clip.y1; y <= clip.y2; y++) {
        const int32_t pay = (int32_t)y - y1;
        int32_t row_start = clip.x2 + 1;   /* default: empty row */
        int32_t row_end   = clip.x1 - 1;
        if (bay != 0) {
            const int64_t c0 = -(int64_t)x1 * bay - (int64_t)pay * bax;
            const int64_t e1  = ( band_half - c0) / bay;
            const int64_t e2  = (-band_half - c0) / bay;
            const int64_t xlo = e1 < e2 ? e1 : e2;
            const int64_t xhi = e1 > e2 ? e1 : e2;
            const int32_t bx1 = (int32_t)xlo - 2;
            const int32_t bx2 = (int32_t)xhi + 2;
            if (bx1 < row_start) row_start = bx1;
            if (bx2 > row_end)   row_end   = bx2;
        }
        else {
            const int32_t dy = pay < 0 ? -pay : pay;
            if ((int64_t)dy * len <= band_half) {
                row_start = clip.x1;
                row_end   = clip.x2;
            }
        }
        const int64_t dy1  = (int64_t)y - y1;
        const int64_t rem1 = cap_r2 - dy1 * dy1;
        if (rem1 >= 0) {
            const int32_t dx = (int32_t)sgl_sqrt((uint32_t)rem1) + 1;
            if (x1 - dx < row_start) row_start = x1 - dx;
            if (x1 + dx > row_end)   row_end   = x1 + dx;
        }
        const int64_t dy2  = (int64_t)y - y2;
        const int64_t rem2 = cap_r2 - dy2 * dy2;
        if (rem2 >= 0) {
            const int32_t dx = (int32_t)sgl_sqrt((uint32_t)rem2) + 1;
            if (x2 - dx < row_start) row_start = x2 - dx;
            if (x2 + dx > row_end)   row_end   = x2 + dx;
        }
        if (row_start < clip.x1) row_start = clip.x1;
        if (row_end   > clip.x2) row_end   = clip.x2;
        if (row_start <= row_end) {
            sgl_color_t *blend = row_base + (row_start - clip.x1);
            int32_t dot   = ((int32_t)row_start - x1) * bax + pay * bay;
            int32_t cross = ((int32_t)row_start - x1) * bay - pay * bax;
            for (int x = row_start; x <= row_end; x++, blend++) {
                const int32_t cur_dot   = dot;
                const int32_t cur_cross = cross;
                dot   += bax;
                cross += bay;
                int32_t dist_q8;
                const int32_t abs_cross = (cur_cross >= 0) ? cur_cross : -cur_cross;
                if (cur_dot >= 0 && cur_dot <= b_sqd) {
                    dist_q8 = (int32_t)(((int64_t)abs_cross * inv_len) >> 8);
                }
                else {
                    const int32_t along = (cur_dot < 0) ? -cur_dot : (cur_dot - b_sqd);
                    const uint32_t raw_sq = (uint32_t)((int64_t)along * along + (int64_t)cur_cross * cur_cross);
                    const uint32_t raw_d  = sgl_sqrt(raw_sq);
                    dist_q8 = (int32_t)(((int64_t)raw_d * inv_len) >> 8);
                }
                if (dist_q8 < inner_limit) {
                    *blend = (alpha == SGL_ALPHA_MAX) ? color : sgl_color_mixer(color, *blend, alpha);
                }
                else if (dist_q8 < outer_limit) {
                    const uint32_t c = (uint32_t)((outer_limit - dist_q8) * 255 / aa_range);
                    if (alpha == SGL_ALPHA_MAX) {
                        *blend = sgl_color_mixer(color, *blend, (uint8_t)c);
                    } else {
                        const uint8_t final_a = (uint8_t)((c * alpha) >> 8);
                        *blend = sgl_color_mixer(color, *blend, final_a);
                    }
                }
            }
        }
        row_base += stride;
    }
}

/**
 * @brief Draw a dashed line using Bresenham's algorithm.
 * 
 * @param surf   Pointer to the surface structure.
 * @param area   Pointer to the area structure defining the valid drawing region.
 * @param x1     Start X-coordinate.
 * @param y1     Start Y-coordinate.
 * @param x2     End X-coordinate.
 * @param y2     End Y-coordinate.
 * @param gap    Length of the dash and the gap in pixels. Must be > 0.
 * @param color  Color of the line.
 * @return none
 * @note: This function is Non-anti-aliased!!!!
 */
void sgl_draw_dashed_line_noaa(sgl_surf_t *surf, sgl_area_t *area, int16_t x1, int16_t y1, int16_t x2, int16_t y2, int16_t gap, sgl_color_t color)
{
    sgl_area_t clip = SGL_AREA_MAX;
    sgl_surf_clip_area_return(surf, area, &clip);

    int16_t dx = sgl_abs(x2 - x1);
    int16_t dy = sgl_abs(y2 - y1);
    int16_t sx = (x1 < x2) ? 1 : -1;
    int16_t sy = (y1 < y2) ? 1 : -1;
    int16_t err = dx - dy;
    int16_t e2;
    int16_t dash_len = 0;

    gap = sgl_max(1, gap);
    while (1) {
        if (dash_len < gap) {
            if (x1 >= clip.x1 && x1 <= clip.x2 && y1 >= clip.y1 && y1 <= clip.y2) {
                sgl_color_t *buf = sgl_surf_get_buf(surf, x1 - surf->x1, y1 - surf->y1);
                *buf = color;
            }
        }

        dash_len++;
        if (dash_len >= 2 * gap) {
            dash_len = 0;
        }

        if (x1 == x2 && y1 == y2) {
            break;
        }

        e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x1 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y1 += sy;
        }
    }
}

/**
 * @brief Fast line drawing with customizable width and alpha blending
 * @param surf   Pointer to the target surface
 * @param area   Clipping area for rendering
 * @param x0     Start X-coordinate.
 * @param y0     Start Y-coordinate.
 * @param x1     End X-coordinate.
 * @param y1     End Y-coordinate.
 * @param color  Line color
 * @param width  Line width in pixels
 * @param alpha  Alpha transparency value
 * @note: This function is Non-anti-aliased!!!!
 */
void sgl_draw_line_noaa(sgl_surf_t *surf, sgl_area_t *area, int16_t x1, int16_t y1, int16_t x2, int16_t y2, sgl_color_t color, uint8_t width, uint8_t alpha)
{
    if (unlikely(width == 0 || alpha == 0)) {
        return;
    }

    if (x1 == x2) {
        sgl_draw_fill_vline(surf, area, x1, y1, y2, width, color, alpha);
        return;
    }
    if (y1 == y2) {
        sgl_draw_fill_hline(surf, area, y1, x1, x2, width, color, alpha);
        return;
    }

    sgl_area_t clip = {
        .x1 = surf->x1,
        .y1 = surf->y1,
        .x2 = surf->x2,
        .y2 = surf->y2,
    };

    if (!sgl_area_selfclip(&clip, area)) {
        return;
    }

    const int16_t dx = sgl_abs(x2 - x1);
    const int16_t dy = sgl_abs(y2 - y1);
    const int16_t sx = (x1 < x2) ? 1 : -1;
    const int16_t sy = (y1 < y2) ? 1 : -1;
    int16_t err = dx - dy;

    while (1) {
        if (x1 >= clip.x1 && x1 <= clip.x2 && y1 >= clip.y1 && y1 <= clip.y2) {
            sgl_color_t *buf = sgl_surf_get_buf(surf, x1 - surf->x1, y1 - surf->y1);
            for (int i = 0; i < width; i++) {
                *buf++ = color;
            }
        }

        if (x1 == x2 && y1 == y2) {
            break;
        }

        int16_t e2 = err << 1;
        if (e2 > -dy) {
            err -= dy;
            x1 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y1 += sy;
        }
    }
}

/**
 * @brief draw a line
 * @param surf surface
 * @param area area that contains the line
 * @param desc line description
 * @return none
 */
void sgl_draw_line(sgl_surf_t *surf, sgl_area_t *area, sgl_draw_line_t *desc)
{
    if (desc->x1 == desc->x2) {
        sgl_draw_fill_vline(surf, area, desc->x1, desc->y1, desc->y2, desc->width, desc->color, desc->alpha);
    }
    else if (desc->y1 == desc->y2) {
        sgl_draw_fill_hline(surf, area, desc->y1, desc->x1, desc->x2, desc->width, desc->color, desc->alpha);
    }
    else {
        sgl_draw_line_fill_slanted(surf, area, desc->x1, desc->y1, desc->x2, desc->y2, desc->width, desc->color, desc->alpha);
    }
}
