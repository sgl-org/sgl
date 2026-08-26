/* source/draw/sgl_draw_circle.c
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
 * @brief Draw a circle
 * @param surf Surface
 * @param area Area of the circle
 * @param cx X coordinate of the center
 * @param cy Y coordinate of the center
 * @param radius Radius of the circle
 * @param color Color of the circle
 * @param alpha Alpha of the circle
 * @return none
 */
void sgl_draw_fill_circle(sgl_surf_t *surf, sgl_area_t *area, int16_t cx, int16_t cy, int16_t radius, sgl_color_t color, uint8_t alpha)
{
    sgl_area_t clip = SGL_AREA_MAX;
    sgl_surf_clip_area_return(surf, area, &clip);

    uint8_t edge_alpha;
    int cx2 = 2 * cx + 1;
    int cy2 = 2 * cy + 1;
    int dx2 = 0, dy2 = 0;
    const int diameter = radius << 1;
    const int r2_max = sgl_pow2(diameter);
    const int r2 = sgl_max(sgl_pow2(diameter - 3), 0); 
    const int r2_fix_diff = (SGL_ALPHA_MAX  << SGL_FIXED_SHIFT) / sgl_max(r2_max - r2, 1);
    sgl_color_t *blend, *buf = sgl_surf_get_buf(surf, clip.x1 - surf->x1, clip.y1 - surf->y1);

    for (int y = clip.y1; y <= clip.y2; y++) {
        blend = buf;
        dy2 = sgl_pow2(2 * y - cy2);

        for (int x = clip.x1; x <= clip.x2; x++, blend++) {
            dx2 = sgl_pow2(2 * x - cx2) + dy2;

            if (dx2 >= r2_max) {
                if (x > cx)
                    break;
                continue;
            }
            else if (dx2 >= r2) {
                edge_alpha = ((r2_max - dx2) * r2_fix_diff) >> SGL_FIXED_SHIFT;
                *blend = (alpha == SGL_ALPHA_MAX ? sgl_color_mixer(color, *blend, edge_alpha) : sgl_color_mixer(sgl_color_mixer(color, *blend, edge_alpha), *blend, alpha));
            }
            else {
                *blend = (alpha == SGL_ALPHA_MAX) ? color : sgl_color_mixer(color, *blend, alpha);
            }
        }
        buf += surf->w;
    }
}

/**
 * @brief Draw only the border ring of a circle, the interior is left untouched
 * @param surf Surface
 * @param area Area of the circle
 * @param cx X coordinate of the center
 * @param cy Y coordinate of the center
 * @param radius Radius of the circle
 * @param border_color Color of the border
 * @param border_width Width of the border
 * @param alpha Alpha of the border
 * @return none
 */
void sgl_draw_fill_circle_border(sgl_surf_t *surf, sgl_area_t *area, int16_t cx, int16_t cy, int16_t radius, sgl_color_t border_color, int16_t border_width, uint8_t alpha)
{
    sgl_area_t clip = SGL_AREA_MAX;
    sgl_surf_clip_area_return(surf, area, &clip);

    if (border_width <= 0) return;

    const int radius_in = sgl_max(radius - border_width, 0);
    const int cx2 = 2 * cx + 1;
    const int cy2 = 2 * cy + 1;

    const int out_diameter = radius << 1;
    const int out_r2_max   = sgl_pow2(out_diameter);
    const int out_r2       = sgl_max(sgl_pow2(out_diameter - 3), 0);

    const int in_diameter  = radius_in << 1;
    const int in_r2_max    = sgl_pow2(in_diameter);
    const int in_r2        = sgl_max(sgl_pow2(in_diameter - 3), 0);

    const int out_fix = (SGL_ALPHA_MAX << SGL_FIXED_SHIFT) / sgl_max(out_r2_max - out_r2, 1);
    const int in_fix  = (SGL_ALPHA_MAX << SGL_FIXED_SHIFT) / sgl_max(in_r2_max - in_r2, 1);

    uint8_t edge_alpha;
    sgl_color_t edge_c, *blend, *buf = sgl_surf_get_buf(surf, clip.x1 - surf->x1, clip.y1 - surf->y1);
    int dx2, dy2;

    for (int y = clip.y1; y <= clip.y2; y++) {
        blend = buf;
        dy2 = sgl_pow2(2 * y - cy2);

        for (int x = clip.x1; x <= clip.x2; x++, blend++) {
            dx2 = sgl_pow2(2 * x - cx2) + dy2;

            if (dx2 >= out_r2_max) {
                if (x > cx) break;
                continue;
            }

            if (dx2 <= in_r2) {
                continue;
            }
            else if (dx2 < in_r2_max) {
                edge_alpha = ((dx2 - in_r2) * in_fix) >> SGL_FIXED_SHIFT;
                edge_c = sgl_color_mixer(border_color, *blend, edge_alpha);
                *blend = (alpha == SGL_ALPHA_MAX) ? edge_c : sgl_color_mixer(edge_c, *blend, alpha);
            }
            else if (dx2 <= out_r2) {
                *blend = (alpha == SGL_ALPHA_MAX) ? border_color : sgl_color_mixer(border_color, *blend, alpha);
            }
            else {
                edge_alpha = ((out_r2_max - dx2) * out_fix) >> SGL_FIXED_SHIFT;
                edge_c = sgl_color_mixer(border_color, *blend, edge_alpha);
                *blend = (alpha == SGL_ALPHA_MAX) ? edge_c : sgl_color_mixer(edge_c, *blend, alpha);
            }
        }
        buf += surf->w;
    }
}

#if (!CONFIG_SGL_PIXMAP_BILINEAR_INTERP)
/**
 * @brief Draw a circle with pixmap and alpha
 * @param surf Surface
 * @param area Area of the circle
 * @param cx X coordinate of the center
 * @param cy Y coordinate of the center
 * @param radius Radius of the circle
 * @param pixmap Pixmap of image
 * @param alpha Alpha of the circle
 * @return none
 */
void sgl_draw_fill_circle_pixmap(sgl_surf_t *surf, sgl_area_t *area, int16_t cx, int16_t cy, int16_t radius, const sgl_pixmap_t *pixmap, uint8_t alpha)
{
    sgl_area_t clip = SGL_AREA_MAX;
    sgl_surf_clip_area_return(surf, area, &clip);

    uint8_t edge_alpha = 0;
    int s_x = cx - radius, s_y = cy - radius;
    int cx2 = 2 * cx + 1;
    int cy2 = 2 * cy + 1;
    int dx2 = 0, dy2 = 0;
    const int diameter = radius << 1;
    const int r2_max = sgl_pow2(diameter);
    const int r2 = sgl_max(sgl_pow2(diameter - 3), 0); 
    const int r2_fix_diff = (SGL_ALPHA_MAX  << SGL_FIXED_SHIFT) / sgl_max(r2_max - r2, 1);
    sgl_color_t *blend, *pbuf, *buf = sgl_surf_get_buf(surf, clip.x1 - surf->x1, clip.y1 - surf->y1);

    const uint32_t scale_x = (pixmap->width << SGL_FIXED_SHIFT) / (radius * 2);
    const uint32_t scale_y = (pixmap->height << SGL_FIXED_SHIFT) / (radius * 2);
    uint32_t step_x = 0, step_y = 0;

    for (int y = clip.y1; y <= clip.y2; y++) {
        blend = buf;
        dy2 = sgl_pow2(2 * y - cy2);
        step_y = (scale_y * (y - s_y)) >> SGL_FIXED_SHIFT;

        for (int x = clip.x1; x <= clip.x2; x++, blend++) {
            dx2 = sgl_pow2(2 * x - cx2) + dy2;
            step_x = (scale_x * (x - s_x)) >> SGL_FIXED_SHIFT;
            pbuf = sgl_pixmap_get_buf(pixmap, step_x, step_y);

            if (dx2 >= r2_max) {
                if(x > cx)
                    break;
                continue;
            }
            else if (dx2 >= r2) {
                edge_alpha = ((r2_max - dx2) * r2_fix_diff) >> SGL_FIXED_SHIFT;
                *blend = (alpha == SGL_ALPHA_MAX ? sgl_color_mixer(*pbuf, *blend, edge_alpha) : sgl_color_mixer(sgl_color_mixer(*pbuf, *blend, edge_alpha), *blend, alpha));
            }
            else {
                *blend = (alpha == SGL_ALPHA_MAX ? *pbuf : sgl_color_mixer(*pbuf, *blend, alpha));
            }
        }
        buf += surf->w;
    }
}
#else
void sgl_draw_fill_circle_pixmap(sgl_surf_t *surf, sgl_area_t *area, int16_t cx, int16_t cy, int16_t radius, const sgl_pixmap_t *pixmap, uint8_t alpha)
{
    sgl_area_t clip = SGL_AREA_MAX;
    sgl_surf_clip_area_return(surf, area, &clip);

    uint8_t edge_alpha = 0;
    int s_x = cx - radius, s_y = cy - radius;
    int cx2 = 2 * cx + 1;
    int cy2 = 2 * cy + 1;
    int dx2 = 0, dy2 = 0;
    const int diameter = radius << 1;
    const int r2_max = sgl_pow2(diameter);
    const int r2 = sgl_max(sgl_pow2(diameter - 3), 0); 
    const int r2_fix_diff = (SGL_ALPHA_MAX  << SGL_FIXED_SHIFT) / sgl_max(r2_max - r2, 1);
    sgl_color_t ip_color, *blend, *buf = sgl_surf_get_buf(surf, clip.x1 - surf->x1, clip.y1 - surf->y1), *pbuf = (sgl_color_t *)pixmap->bitmap.array;

    const int32_t scale_x = ((int32_t)pixmap->width << SGL_FIXED_SHIFT) / (radius * 2);
    const int32_t scale_y = ((int32_t)pixmap->height << SGL_FIXED_SHIFT) / (radius * 2);
    int fx = 0, fy = 0;

    for (int y = clip.y1; y <= clip.y2; y++) {
        blend = buf;
        dy2 = sgl_pow2(2 * y - cy2);
        fy = (int32_t)(y - s_y) * scale_y;

        for (int x = clip.x1; x <= clip.x2; x++, blend++) {
            dx2 = sgl_pow2(2 * x - cx2) + dy2;

            fx = (int32_t)(x - s_x) * scale_x;
            ip_color = sgl_draw_biln_color(pbuf, pixmap->width, pixmap->height, fx, fy);

            if (dx2 >= r2_max) {
                if(x > cx)
                    break;
                continue;
            }
            else if (dx2 >= r2) {
                edge_alpha = ((r2_max - dx2) * r2_fix_diff) >> SGL_FIXED_SHIFT;
                *blend = (alpha == SGL_ALPHA_MAX ? sgl_color_mixer(ip_color, *blend, edge_alpha) : sgl_color_mixer(sgl_color_mixer(ip_color, *blend, edge_alpha), *blend, alpha));
            }
            else {
                *blend = (alpha == SGL_ALPHA_MAX ? ip_color : sgl_color_mixer(ip_color, *blend, alpha));
            }
        }
        buf += surf->w;
    }
}
#endif // CONFIG_SGL_PIXMAP_BILINEAR_INTERP

/**
 * @brief Draw a circle with alpha and border
 * @param surf Surface
 * @param area Area of the circle
 * @param cx X coordinate of the center
 * @param cy Y coordinate of the center
 * @param radius Radius of the circle
 * @param color Color of the circle
 * @param border_color Color of the border
 * @param border_width Width of the border
 * @param alpha Alpha of the circle
 * @return none
 */
void sgl_draw_fill_circle_with_border(sgl_surf_t *surf, sgl_area_t *area, int16_t cx, int16_t cy, int16_t radius, sgl_color_t color, sgl_color_t border_color, int16_t border_width, uint8_t alpha)
{
    sgl_area_t clip = SGL_AREA_MAX;
    sgl_surf_clip_area_return(surf, area, &clip);

    int cx2 = 2 * cx + 1;
    int cy2 = 2 * cy + 1;
    int radius_in = sgl_max(radius - border_width, 0);

    const int out_diameter = radius << 1;
    const int out_r2_max   = sgl_pow2(out_diameter);
    const int out_r2       = sgl_max(sgl_pow2(out_diameter - 3), 0);

    const int in_diameter  = radius_in << 1;
    const int in_r2_max    = sgl_pow2(in_diameter);
    const int in_r2        = sgl_max(sgl_pow2(in_diameter - 3), 0);

    const int out_fix = (SGL_ALPHA_MAX << SGL_FIXED_SHIFT) / sgl_max(out_r2_max - out_r2, 1);
    const int in_fix  = (SGL_ALPHA_MAX << SGL_FIXED_SHIFT) / sgl_max(in_r2_max - in_r2, 1);

    uint8_t edge_alpha;
    sgl_color_t edge_c, *blend, *buf = sgl_surf_get_buf(surf, clip.x1 - surf->x1, clip.y1 - surf->y1);
    int dx2, dy2;

    for (int y = clip.y1; y <= clip.y2; y++) {
        blend = buf;
        dy2 = sgl_pow2(2 * y - cy2);

        for (int x = clip.x1; x <= clip.x2; x++, blend++) {
            dx2 = sgl_pow2(2 * x - cx2) + dy2;
            if (dx2 >= out_r2_max) {
                if (x > cx) break;
                continue;
            }

            if (dx2 <= in_r2) {
                *blend = (alpha == SGL_ALPHA_MAX) ? color : sgl_color_mixer(color, *blend, alpha);
            }
            else if (dx2 < in_r2_max) {
                edge_alpha = ((dx2 - in_r2) * in_fix) >> SGL_FIXED_SHIFT;
                edge_c = sgl_color_mixer(border_color, color, edge_alpha);
                *blend = (alpha == SGL_ALPHA_MAX) ? edge_c : sgl_color_mixer(edge_c, *blend, alpha);
            }
            else if (dx2 <= out_r2) {
                *blend = (alpha == SGL_ALPHA_MAX) ? border_color : sgl_color_mixer(border_color, *blend, alpha);
            }
            else {
                edge_alpha = ((out_r2_max - dx2) * out_fix) >> SGL_FIXED_SHIFT;
                edge_c = sgl_color_mixer(border_color, *blend, edge_alpha);
                *blend = (alpha == SGL_ALPHA_MAX) ? edge_c : sgl_color_mixer(edge_c, *blend, alpha);
            }
        }
        buf += surf->w;
    }
}


/**
 * @brief draw task, the task contains the draw information and canvas
 * @param surf surface pointer
 * @param area the area of the task
 * @param desc the draw information
 * @return none
 */
void sgl_draw_circle(sgl_surf_t *surf, sgl_area_t *area, sgl_draw_circle_t *desc)
{
    if (unlikely(desc->alpha == SGL_ALPHA_MIN)) {
        return;
    }

    if (desc->pixmap == NULL) {
        sgl_draw_fill_circle(surf, area, desc->cx, desc->cy, desc->radius - desc->border, desc->color, desc->alpha);
    }
    else {
        sgl_draw_fill_circle_pixmap(surf, area, desc->cx, desc->cy, desc->radius - desc->border, desc->pixmap, desc->alpha);
    }

    if (desc->border) {
        sgl_draw_fill_circle_border(surf, area, desc->cx, desc->cy, desc->radius, desc->border_color, desc->border, desc->alpha);
    }
}
