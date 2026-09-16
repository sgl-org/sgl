/* source/draw/sgl_draw.c
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
#include <string.h>

/**
 * @brief calculate a point color by bilinear interpolate (with mask support)
 * @param buffer point to image pixmap start buffer (RGB)
 * @param w      width of buffer
 * @param h      height of buffer
 * @param fx     x coordinate of point (fixed point, SGL_FIXED_SHIFT bits fraction)
 * @param fy     y coordinate of point (fixed point, SGL_FIXED_SHIFT bits fraction)
 * @return point color (RGB: interpolated if mask non-0, transparent/black if mask 0)
 */
sgl_color_t sgl_draw_biln_color(const sgl_color_t *buffer, int16_t w, int16_t h, int32_t fx, int32_t fy)
{
    sgl_color_t ret = {0};

    const int32_t max_x = ((int32_t)w - 1) << SGL_FIXED_SHIFT;
    const int32_t max_y = ((int32_t)h - 1) << SGL_FIXED_SHIFT;
    fx = (fx < 0) ? 0 : (fx > max_x) ? max_x : fx;
    fy = (fy < 0) ? 0 : (fy > max_y) ? max_y : fy;

    const int32_t x0 = fx >> SGL_FIXED_SHIFT;
    const int32_t y0 = fy >> SGL_FIXED_SHIFT;
    const int32_t dx = fx & SGL_FIXED_MASK;
    const int32_t dy = fy & SGL_FIXED_MASK;
    const int32_t dx1 = SGL_FIXED_ONE - dx;
    const int32_t dy1 = SGL_FIXED_ONE - dy;

    const int32_t idx00 = y0 * w + x0;
    const int32_t idx01 = idx00 + 1;
    const int32_t idx10 = idx00 + w;
    const int32_t idx11 = idx10 + 1;

    const sgl_color_t p00 = buffer[idx00];
    const sgl_color_t p01 = buffer[idx01];
    const sgl_color_t p10 = buffer[idx10];
    const sgl_color_t p11 = buffer[idx11];

    const int32_t r_y0 = (p00.ch.red * dx1 + p01.ch.red * dx) >> SGL_FIXED_SHIFT;
    const int32_t r_y1 = (p10.ch.red * dx1 + p11.ch.red * dx) >> SGL_FIXED_SHIFT;
    ret.ch.red = (r_y0 * dy1 + r_y1 * dy) >> SGL_FIXED_SHIFT;

    const int32_t g_y0 = (p00.ch.green * dx1 + p01.ch.green * dx) >> SGL_FIXED_SHIFT;
    const int32_t g_y1 = (p10.ch.green * dx1 + p11.ch.green * dx) >> SGL_FIXED_SHIFT;
    ret.ch.green = (g_y0 * dy1 + g_y1 * dy) >> SGL_FIXED_SHIFT;

    const int32_t b_y0 = (p00.ch.blue * dx1 + p01.ch.blue * dx) >> SGL_FIXED_SHIFT;
    const int32_t b_y1 = (p10.ch.blue * dx1 + p11.ch.blue * dx) >> SGL_FIXED_SHIFT;
    ret.ch.blue = (b_y0 * dy1 + b_y1 * dy) >> SGL_FIXED_SHIFT;

    return ret;
}


/**
 * @brief transform a surface
 * @param dst destination surface
 * @param src source surface
 * @param area area of surface
 * @param x x coordinate of surface
 * @param y y coordinate of surface
 * @param rotation rotation angle
 * @return none
 * @note This function has implemented angle normalization to the range of 0 to 360 degrees.
 */
void sgl_draw_xform_surf(sgl_surf_t *dst, sgl_surf_t *src, sgl_area_t *area, int16_t x, int16_t y, int16_t rotation)
{
    const int32_t sin_val = sgl_sin(rotation);
    const int32_t cos_val = sgl_cos(rotation);

    const int16_t half_w = src->w / 2;
    const int16_t half_h = src->h / 2;

    const int16_t x1r = (cos_val * (-half_w) - sin_val * (-half_h)) / SGL_SIN_FIXED_ONE;
    const int16_t y1r = (sin_val * (-half_w) + cos_val * (-half_h)) / SGL_SIN_FIXED_ONE;

    const int16_t x2r = (cos_val * half_w - sin_val * (-half_h)) / SGL_SIN_FIXED_ONE;
    const int16_t y2r = (sin_val * half_w + cos_val * (-half_h)) / SGL_SIN_FIXED_ONE;

    const int16_t x3r = (cos_val * half_w - sin_val * half_h) / SGL_SIN_FIXED_ONE;
    const int16_t y3r = (sin_val * half_w + cos_val * half_h) / SGL_SIN_FIXED_ONE;

    const int16_t x4r = (cos_val * (-half_w) - sin_val * half_h) / SGL_SIN_FIXED_ONE;
    const int16_t y4r = (sin_val * (-half_w) + cos_val * half_h) / SGL_SIN_FIXED_ONE;

    const int16_t min_x = sgl_min4(x1r, x2r, x3r, x4r);
    const int16_t min_y = sgl_min4(y1r, y2r, y3r, y4r);
    const int16_t max_x = sgl_max4(x1r, x2r, x3r, x4r);
    const int16_t max_y = sgl_max4(y1r, y2r, y3r, y4r);

    const int16_t center_x = x + half_w;
    const int16_t center_y = y + half_h;

    const int16_t buf_width = src->w > 0 ? src->w : 1;
    const int16_t buf_height = src->h > 0 ? src->h : 1;

    if (buf_width <= 0 || buf_height <= 0) {
        return;
    }

    for (int py = min_y; py <= max_y; py++) {
        for (int px = min_x; px <= max_x; px++) {
            const int dst_x = center_x + px;
            const int dst_y = center_y + py;
            if (dst_x < area->x1 || dst_x > area->x2 || dst_y < area->y1 || dst_y > area->y2 ||
                dst_x < dst->x1 || dst_x > dst->x2 || dst_y < dst->y1 || dst_y > dst->y2) {
                continue;
            }

            int32_t rel_x = px;
            int32_t rel_y = py;

            int32_t orig_x_fixed = cos_val * rel_x + sin_val * rel_y;
            int32_t orig_y_fixed = -sin_val * rel_x + cos_val * rel_y;

            int32_t src_x = (orig_x_fixed / SGL_SIN_FIXED_ONE) + half_w;
            int32_t src_y = (orig_y_fixed / SGL_SIN_FIXED_ONE) + half_h;

            int32_t src_x_fp = src_x << SGL_FIXED_SHIFT;
            int32_t src_y_fp = src_y << SGL_FIXED_SHIFT;

            if (src_x >= 0 && src_x < buf_width - 1 && src_y >= 0 && src_y < buf_height - 1) {
                sgl_color_t color = sgl_draw_biln_color(src->buffer, buf_width, buf_height, src_x_fp, src_y_fp);
                int dst_idx = (dst_y - dst->y1) * dst->w + (dst_x - dst->x1);
                dst->buffer[dst_idx] = color;
            }
        }
    }
}
