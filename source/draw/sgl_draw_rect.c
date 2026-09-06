/* source/draw/sgl_draw_rect.c
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
 * @brief draw a wireframe rectangle with alpha
 * @param surf point to surface
 * @param area area of rectangle that you want to draw
 * @param rect point to rectangle that you want to draw
 * @param radius radius of round
 * @param width width of wireframe
 * @param color color of rectangle
 * @param alpha alpha of rectangle
 * @return none
 */
void sgl_draw_wireframe(sgl_surf_t *surf, sgl_area_t *area, sgl_area_t *rect, int16_t radius, int16_t width, sgl_color_t color, uint8_t alpha)
{
    const int16_t x1 = rect->x1;
    const int16_t x2 = rect->x2;
    const int16_t y1 = rect->y1;
    const int16_t y2 = rect->y2;

    if (radius == 0) {
        const int16_t s_ofs = (width - 1) / 2;
        const int16_t e_ofs = width / 2;
        sgl_draw_fill_hline(surf, area, y1 + s_ofs, x1, x2, width, color, alpha);
        sgl_draw_fill_hline(surf, area, y2 - e_ofs, x1, x2, width, color, alpha);
        sgl_draw_fill_vline(surf, area, x1 + s_ofs, y1, y2, width, color, alpha);
        sgl_draw_fill_vline(surf, area, x2 - e_ofs, y1, y2, width, color, alpha);
    }
    else {
        sgl_draw_fill_rect_border(surf, area, rect, radius, color, width, alpha);
    }
}

/**
 * @brief fill a round rectangle with alpha
 * @param surf point to surface
 * @param area area of rectangle that you want to draw
 * @param rect point to rectangle that you want to draw
 * @param radius radius of round
 * @param color color of rectangle
 * @param alpha alpha of rectangle
 * @return none
 */
void sgl_draw_fill_rect(sgl_surf_t *surf, sgl_area_t *area, sgl_area_t *rect, int16_t radius, sgl_color_t color, uint8_t alpha)
{
    sgl_area_t clip = SGL_AREA_INVALID;
    sgl_color_t *buf = NULL, *blend = NULL;
    uint8_t solid_alpha = (alpha == SGL_ALPHA_MAX);

    sgl_surf_clip_area_return(surf, area, &clip);
    if (!sgl_area_selfclip(&clip, rect)) return;

    int pixel_count = clip.x2 - clip.x1 + 1;
    buf = sgl_surf_get_buf(surf, clip.x1 - surf->x1, clip.y1 - surf->y1);

    if (radius <= 0) {
        for (int y = clip.y1; y <= clip.y2; y++) {
            blend = buf;
            if (solid_alpha) {
                for (int i = 0; i < pixel_count; i++) {
                    blend[i] = color;
                }
            }
            else {
                for (int i = 0; i < pixel_count; i++) {
                    blend[i] = sgl_color_mixer(color, blend[i], alpha);
                }
            }
            buf += surf->w;
        }
        return;
    }

    const int cx1 = rect->x1 + radius;
    const int cx2 = rect->x2 - radius;
    const int cy1 = rect->y1 + radius;
    const int cy2 = rect->y2 - radius;

    const int r2 = sgl_pow2(radius);
    const int r2_max = sgl_pow2(radius + 1);
    const int r2_diff = sgl_max(r2_max - r2, 1);
    const int r2_fix_diff = (SGL_ALPHA_MAX << SGL_FIXED_SHIFT) / r2_diff;

    int cy_tmp, cx_tmp;
    int dy2, real_r2;
    uint8_t edge_alpha;

    for (int y = clip.y1; y <= clip.y2; y++) {
        blend = buf;
        if (y >= cy1 && y <= cy2) {
            pixel_count = clip.x2 - clip.x1 + 1;
            solid_alpha = (alpha == SGL_ALPHA_MAX);

            if (solid_alpha) {
                for (int i = 0; i < pixel_count; i++) {
                    blend[i] = color;
                }
            } else {
                for (int i = 0; i < pixel_count; i++) {
                    blend[i] = sgl_color_mixer(color, blend[i], alpha);
                }
            }
        }
        else {
            cy_tmp = (y < cy1) ? cy1 : cy2;
            dy2 = sgl_pow2(y - cy_tmp);

            for (int x = clip.x1; x <= clip.x2; x++, blend++) {
                if (x >= cx1 && x <= cx2) {
                    *blend = (alpha == SGL_ALPHA_MAX ? color : sgl_color_mixer(color, *blend, alpha));
                }
                else {
                    cx_tmp = (x < cx1) ? cx1 : cx2;
                    real_r2 = sgl_pow2(x - cx_tmp) + dy2;

                    if (real_r2 >= r2_max) {
                        if(x > cx_tmp) break; else continue;
                    }
                    else if (real_r2 >= r2) {
                        edge_alpha = ((r2_max - real_r2) * r2_fix_diff) >> SGL_FIXED_SHIFT;
                        *blend = (alpha == SGL_ALPHA_MAX ? sgl_color_mixer(color, *blend, edge_alpha) : 
                                 sgl_color_mixer(sgl_color_mixer(color, *blend, edge_alpha), *blend, alpha));
                    }
                    else {
                        *blend = (alpha == SGL_ALPHA_MAX) ? color : sgl_color_mixer(color, *blend, alpha);
                    }
                }
            }
        }
        buf += surf->w;
    }
}

/**
 * @brief fill a round rectangle with rich independent corner radiuses with alpha
 * @param surf point to surface
 * @param area area of rectangle that you want to draw
 * @param rect point to rectangle that you want to draw
 * @param tl_radius  Top-Left corner radius
 * @param tr_radius Top-Right corner radius
 * @param bl_radius Bottom-Left corner radius
 * @param br_radius   Bottom-Right corner radius
 * @param color color of rectangle
 * @param alpha alpha of rectangle
 * @return none
 */
void sgl_draw_fill_rich_rect(sgl_surf_t *surf, sgl_area_t *area, sgl_area_t *rect, 
                             int16_t tl_radius, int16_t tr_radius, int16_t bl_radius, int16_t br_radius, 
                             sgl_color_t color, uint8_t alpha)
{
    sgl_area_t clip = SGL_AREA_INVALID;
    sgl_color_t *buf = NULL, *blend = NULL;
    const uint8_t solid_alpha = (alpha == SGL_ALPHA_MAX);

    sgl_surf_clip_area_return(surf, area, &clip);
    if (!sgl_area_selfclip(&clip, rect)) return;

    buf = sgl_surf_get_buf(surf, clip.x1 - surf->x1, clip.y1 - surf->y1);

    const int16_t r_tl = sgl_max(tl_radius, 0);
    const int16_t r_tr = sgl_max(tr_radius, 0);
    const int16_t r_bl = sgl_max(bl_radius, 0);
    const int16_t r_br = sgl_max(br_radius, 0);

    const int pixel_count = clip.x2 - clip.x1 + 1;

    if (r_tl == 0 && r_tr == 0 && r_bl == 0 && r_br == 0) {
        for (int y = clip.y1; y <= clip.y2; y++) {
            blend = buf;
            if (solid_alpha) {
                for (int i = 0; i < pixel_count; i++) blend[i] = color;
            } else {
                for (int i = 0; i < pixel_count; i++) blend[i] = sgl_color_mixer(color, blend[i], alpha);
            }
            buf += surf->w;
        }
        return;
    }

    const int x_mid = (rect->x1 + rect->x2) / 2;
    const int y_mid = (rect->y1 + rect->y2) / 2;

    for (int y = clip.y1; y <= clip.y2; y++) {
        blend = buf;
        for (int x = clip.x1; x <= clip.x2; x++, blend++) {
            int r = 0;
            int cx = 0, cy = 0;

            if (y <= y_mid) {
                if (x <= x_mid) {
                    r = r_tl; cx = rect->x1 + r; cy = rect->y1 + r;
                } else {
                    r = r_tr; cx = rect->x2 - r; cy = rect->y1 + r;
                }
            } else {
                if (x <= x_mid) {
                    r = r_bl; cx = rect->x1 + r; cy = rect->y2 - r;
                } else {
                    r = r_br; cx = rect->x2 - r; cy = rect->y2 - r;
                }
            }

            const bool in_x_straight = (x <= x_mid) ? (x >= cx) : (x <= cx);
            const bool in_y_straight = (y <= y_mid) ? (y >= cy) : (y <= cy);

            if (in_x_straight || in_y_straight || r == 0) {
                *blend = solid_alpha ? color : sgl_color_mixer(color, *blend, alpha);
            }
            else {
                const int r2 = sgl_pow2(r);
                const int r2_max = sgl_pow2(r + 1);
                const int r2_diff = sgl_max(r2_max - r2, 1);
                const int r2_fix_diff = (SGL_ALPHA_MAX << SGL_FIXED_SHIFT) / r2_diff;

                const int dy2 = sgl_pow2(y - cy);
                const int real_r2 = sgl_pow2(x - cx) + dy2;

                if (real_r2 >= r2_max) {
                    continue;
                } else if (real_r2 >= r2) {
                    uint8_t edge_alpha = ((r2_max - real_r2) * r2_fix_diff) >> SGL_FIXED_SHIFT;
                    sgl_color_t edge_c = sgl_color_mixer(color, *blend, edge_alpha);
                    *blend = solid_alpha ? edge_c : sgl_color_mixer(edge_c, *blend, alpha);
                } else {
                    *blend = solid_alpha ? color : sgl_color_mixer(color, *blend, alpha);
                }
            }
        }
        buf += surf->w;
    }
}

/**
 * @brief draw only the border ring of a round rectangle, the interior is left untouched
 * @param surf point to surface
 * @param area area of rectangle that you want to draw
 * @param rect point to rectangle that you want to draw
 * @param radius radius of round
 * @param border_color color of border
 * @param border_width width of border
 * @param border_alpha alpha of border
 * @return none
 */
void sgl_draw_fill_rect_border(sgl_surf_t *surf, sgl_area_t *area, sgl_area_t *rect, int16_t radius, sgl_color_t border_color, uint8_t border_width, uint8_t border_alpha)
{
    sgl_area_t clip = SGL_AREA_INVALID;
    sgl_color_t *buf = NULL, *blend = NULL;
    sgl_surf_clip_area_return(surf, area, &clip);
    if (!sgl_area_selfclip(&clip, rect)) return;
    if (border_width <= 0) return;
    const uint8_t solid_border = (border_alpha == SGL_ALPHA_MAX);
    buf = sgl_surf_get_buf(surf, clip.x1 - surf->x1, clip.y1 - surf->y1);
    const int cx1i = rect->x1 + border_width;
    const int cx2i = rect->x2 - border_width;
    const int cyi1 = rect->y1 + border_width;
    const int cyi2 = rect->y2 - border_width;

    if (radius == 0) {
        for (int y = clip.y1; y <= clip.y2; y++) {
            blend = buf;
            const uint8_t edge_row = (y < cyi1 || y > cyi2);
            if (edge_row) {
                for (int x = clip.x1; x <= clip.x2; x++, blend++) {
                    *blend = solid_border ? border_color : sgl_color_mixer(border_color, *blend, border_alpha);
                }
            }
            else {
                for (int x = clip.x1; x <= clip.x2; x++, blend++) {
                    if (x < cx1i || x > cx2i) {
                        *blend = solid_border ? border_color : sgl_color_mixer(border_color, *blend, border_alpha);
                    }
                }
            }
            buf += surf->w;
        }
        return;
    }

    const int cx1  = rect->x1 + radius;
    const int cx2 = rect->x2 - radius;
    const int cy1 = rect->y1 + radius;
    const int cy2 = rect->y2 - radius;
    const int radius_in = sgl_max(radius - border_width, 0);
    const int out_r2 = sgl_pow2(radius);
    const int out_r2_max = sgl_pow2(radius + 1);
    const int in_r2 = sgl_pow2(radius_in);
    const int in_r2_max = sgl_pow2(radius_in + 1);
    const int out_r2_diff = sgl_max(out_r2_max - out_r2, 1);
    const int out_fix_diff = (SGL_ALPHA_MAX << SGL_FIXED_SHIFT) / out_r2_diff;
    const int in_r2_diff = sgl_max(in_r2_max - in_r2, 1);
    const int in_fix_diff = (SGL_ALPHA_MAX << SGL_FIXED_SHIFT) / in_r2_diff;
    int cy_tmp, cx_tmp;
    int dy2, real_r2;
    uint8_t edge_alpha;
    sgl_color_t edge_c;

    for (int y = clip.y1; y <= clip.y2; y++) {
        blend = buf;
        if (y >= cy1 && y <= cy2) {
            const uint8_t edge_row = (y < cyi1 || y > cyi2);
            for (int x = clip.x1; x <= clip.x2; x++, blend++) {
                if (x < cx1i || x > cx2i || edge_row) {
                    *blend = solid_border ? border_color : sgl_color_mixer(border_color, *blend, border_alpha);
                }
            }
        }
        else {
            cy_tmp = (y < cy1) ? cy1 : cy2;
            dy2 = sgl_pow2(y - cy_tmp);
            const uint8_t edge_row = (y < cyi1 || y > cyi2);
            for (int x = clip.x1; x <= clip.x2; x++, blend++) {
                if (x >= cx1 && x <= cx2) {
                    if (edge_row) {
                        *blend = solid_border ? border_color : sgl_color_mixer(border_color, *blend, border_alpha);
                    }
                }
                else {
                    cx_tmp = (x < cx1) ? cx1 : cx2;
                    real_r2 = sgl_pow2(x - cx_tmp) + dy2;
                    if (real_r2 >= out_r2_max) {
                        if(x > cx_tmp) break; else continue;
                    }
                    else if (real_r2 <= in_r2) {
                        continue;
                    }
                    else if (real_r2 < in_r2_max) {
                        edge_alpha = ((real_r2 - in_r2) * in_fix_diff) >> SGL_FIXED_SHIFT;
                        edge_c = sgl_color_mixer(border_color, *blend, edge_alpha);
                        *blend = (border_alpha == SGL_ALPHA_MAX) ? edge_c : sgl_color_mixer(edge_c, *blend, border_alpha);
                    }
                    else if (real_r2 <= out_r2) {
                        *blend = (border_alpha == SGL_ALPHA_MAX) ? border_color : sgl_color_mixer(border_color, *blend, border_alpha);
                    }
                    else {
                        edge_alpha = ((out_r2_max - real_r2) * out_fix_diff) >> SGL_FIXED_SHIFT;
                        edge_c = sgl_color_mixer(border_color, *blend, edge_alpha);
                        *blend = (border_alpha == SGL_ALPHA_MAX) ? edge_c : sgl_color_mixer(edge_c, *blend, border_alpha);
                    }
                }
            }
        }
        buf += surf->w;
    }
}

/**
 * @brief draw only the border ring of a round rectangle with independent corner radii, the interior is left untouched
 * @param surf point to surface
 * @param area area of rectangle that you want to draw
 * @param rect point to rectangle that you want to draw
 * @param tl_radius radius of top-left corner
 * @param tr_radius radius of top-right corner
 * @param bl_radius radius of bottom-left corner
 * @param br_radius radius of bottom-right corner
 * @param border_color color of border
 * @param border_width width of border
 * @param border_alpha alpha of border
 * @return none
 */
void sgl_draw_fill_rect_border_rich(sgl_surf_t *surf, sgl_area_t *area, sgl_area_t *rect, 
                                    int16_t tl_radius, int16_t tr_radius, int16_t bl_radius, int16_t br_radius, 
                                    sgl_color_t border_color, uint8_t border_width, uint8_t border_alpha)
{
    sgl_area_t clip = SGL_AREA_INVALID;
    sgl_color_t *buf = NULL, *blend = NULL;
    sgl_surf_clip_area_return(surf, area, &clip);
    if (!sgl_area_selfclip(&clip, rect)) return;
    if (border_width <= 0) return;

    const uint8_t solid_border = (border_alpha == SGL_ALPHA_MAX);
    buf = sgl_surf_get_buf(surf, clip.x1 - surf->x1, clip.y1 - surf->y1);

    const int cx1i = rect->x1 + border_width;
    const int cx2i = rect->x2 - border_width;
    const int cyi1 = rect->y1 + border_width;
    const int cyi2 = rect->y2 - border_width;

    const int16_t r_tl = sgl_max(tl_radius, 0);
    const int16_t r_tr = sgl_max(tr_radius, 0);
    const int16_t r_bl = sgl_max(bl_radius, 0);
    const int16_t r_br = sgl_max(br_radius, 0);

    if (r_tl == 0 && r_tr == 0 && r_bl == 0 && r_br == 0) {
        for (int y = clip.y1; y <= clip.y2; y++) {
            blend = buf;
            const uint8_t edge_row = (y < cyi1 || y > cyi2);
            if (edge_row) {
                for (int x = clip.x1; x <= clip.x2; x++, blend++) {
                    *blend = solid_border ? border_color : sgl_color_mixer(border_color, *blend, border_alpha);
                }
            } else {
                for (int x = clip.x1; x <= clip.x2; x++, blend++) {
                    if (x < cx1i || x > cx2i) {
                        *blend = solid_border ? border_color : sgl_color_mixer(border_color, *blend, border_alpha);
                    }
                }
            }
            buf += surf->w;
        }
        return;
    }

    const int x_mid = (rect->x1 + rect->x2) / 2;
    const int y_mid = (rect->y1 + rect->y2) / 2;

    for (int y = clip.y1; y <= clip.y2; y++) {
        blend = buf;
        const uint8_t edge_row = (y < cyi1 || y > cyi2);

        for (int x = clip.x1; x <= clip.x2; x++, blend++) {
            int r = 0;
            int cx = 0, cy = 0;

            if (y <= y_mid) {
                if (x <= x_mid) {
                    r = r_tl;
                    cx = rect->x1 + r;
                    cy = rect->y1 + r;
                } else {
                    r = r_tr;
                    cx = rect->x2 - r;
                    cy = rect->y1 + r;
                }
            } else {
                if (x <= x_mid) {
                    r = r_bl;
                    cx = rect->x1 + r;
                    cy = rect->y2 - r;
                } else {
                    r = r_br;
                    cx = rect->x2 - r;
                    cy = rect->y2 - r;
                }
            }

            const bool in_x_straight = (x <= x_mid) ? (x >= cx) : (x <= cx);
            const bool in_y_straight = (y <= y_mid) ? (y >= cy) : (y <= cy);

            if (in_x_straight || in_y_straight || r == 0) {
                if (x < cx1i || x > cx2i || edge_row) {
                    *blend = solid_border ? border_color : sgl_color_mixer(border_color, *blend, border_alpha);
                }
            } else {
                const int radius_in = sgl_max(r - border_width, 0);
                const int out_r2 = sgl_pow2(r);
                const int out_r2_max = sgl_pow2(r + 1);
                const int in_r2 = sgl_pow2(radius_in);
                const int in_r2_max = sgl_pow2(radius_in + 1);

                const int out_r2_diff = sgl_max(out_r2_max - out_r2, 1);
                const int out_fix_diff = (SGL_ALPHA_MAX << SGL_FIXED_SHIFT) / out_r2_diff;
                const int in_r2_diff = sgl_max(in_r2_max - in_r2, 1);
                const int in_fix_diff = (SGL_ALPHA_MAX << SGL_FIXED_SHIFT) / in_r2_diff;

                const int dy2 = sgl_pow2(y - cy);
                const int real_r2 = sgl_pow2(x - cx) + dy2;

                if (real_r2 >= out_r2_max) {
                    continue;
                } else if (real_r2 <= in_r2) {
                    continue;
                } else if (real_r2 < in_r2_max) {
                    uint8_t edge_alpha = ((real_r2 - in_r2) * in_fix_diff) >> SGL_FIXED_SHIFT;
                    sgl_color_t edge_c = sgl_color_mixer(border_color, *blend, edge_alpha);
                    *blend = solid_border ? edge_c : sgl_color_mixer(edge_c, *blend, border_alpha);
                } else if (real_r2 <= out_r2) {
                    *blend = solid_border ? border_color : sgl_color_mixer(border_color, *blend, border_alpha);
                } else {
                    uint8_t edge_alpha = ((out_r2_max - real_r2) * out_fix_diff) >> SGL_FIXED_SHIFT;
                    sgl_color_t edge_c = sgl_color_mixer(border_color, *blend, edge_alpha);
                    *blend = solid_border ? edge_c : sgl_color_mixer(edge_c, *blend, border_alpha);
                }
            }
        }
        buf += surf->w;
    }
}

#if (!CONFIG_SGL_PIXMAP_BILINEAR_INTERP)
/**
 * @brief fill a round rectangle pixmap with alpha
 * @param surf point to surface
 * @param area area of rectangle that you want to draw
 * @param rect point to rectangle that you want to draw
 * @param radius radius of round
 * @param pixmap pixmap of rectangle
 * @param alpha alpha of rectangle
 * @return none
 */
void sgl_draw_fill_rect_pixmap(sgl_surf_t *surf, sgl_area_t *area, sgl_area_t *rect, int16_t radius, const sgl_pixmap_t *pixmap, uint8_t alpha)
{
    sgl_area_t clip = SGL_AREA_INVALID;
    sgl_color_t *buf = NULL, *blend = NULL, *pbuf = (sgl_color_t *)pixmap->bitmap.array;
    
    sgl_surf_clip_area_return(surf, area, &clip);
    if (!sgl_area_selfclip(&clip, rect)) return;

    const int32_t rect_w = rect->x2 - rect->x1 + 1;
    const int32_t rect_h = rect->y2 - rect->y1 + 1;
    const int32_t scale_x = ((int32_t)pixmap->width << SGL_FIXED_SHIFT) / rect_w;
    const int32_t scale_y = ((int32_t)pixmap->height << SGL_FIXED_SHIFT) / rect_h;
    uint32_t step_x = 0, step_y = 0;

    buf = sgl_surf_get_buf(surf, clip.x1 - surf->x1, clip.y1 - surf->y1);
    if (radius == 0) {
        for (int y = clip.y1; y <= clip.y2; y++) {
            blend = buf;
            step_y = (scale_y * (y - rect->y1)) >> SGL_FIXED_SHIFT;
            for (int x = clip.x1; x <= clip.x2; x++, blend++) {
                step_x = (scale_x * (x - rect->x1)) >> SGL_FIXED_SHIFT;
                pbuf = sgl_pixmap_get_buf(pixmap, step_x, step_y);
                *blend = (alpha == SGL_ALPHA_MAX ? *pbuf : sgl_color_mixer(*pbuf, *blend, alpha));
            }
            buf += surf->w;
        }
        return;
    }

    const int cx1 = rect->x1 + radius;
    const int cx2 = rect->x2 - radius;
    const int cy1 = rect->y1 + radius;
    const int cy2 = rect->y2 - radius;

    const int r2 = sgl_pow2(radius);
    const int r2_max = sgl_pow2(radius + 1);
    const int r2_diff = sgl_max(r2_max - r2, 1);
    const int r2_fix_diff = (SGL_ALPHA_MAX << SGL_FIXED_SHIFT) / r2_diff;
    int dy2, real_r2, cx_tmp, cy_tmp;
    uint8_t edge_alpha;

    for (int y = clip.y1; y <= clip.y2; y++) {
        blend = buf;
        step_y = (scale_y * (y - rect->y1)) >> SGL_FIXED_SHIFT;
        if (y >= cy1 && y <= cy2) {
            for (int x = clip.x1; x <= clip.x2; x++, blend++) {
                step_x = (scale_x * (x - rect->x1)) >> SGL_FIXED_SHIFT;
                pbuf = sgl_pixmap_get_buf(pixmap, step_x, step_y);
                *blend = (alpha == SGL_ALPHA_MAX ? *pbuf : sgl_color_mixer(*pbuf, *blend, alpha));
            }
        }
        else {
            cy_tmp = (y < cy1) ? cy1 : cy2;
            dy2 = sgl_pow2(y - cy_tmp);

            for (int x = clip.x1; x <= clip.x2; x++, blend++) {
                step_x = (scale_x * (x - rect->x1)) >> SGL_FIXED_SHIFT;
                pbuf = sgl_pixmap_get_buf(pixmap, step_x, step_y);

                if (x >= cx1 && x <= cx2) {
                    *blend = (alpha == SGL_ALPHA_MAX ? *pbuf : sgl_color_mixer(*pbuf, *blend, alpha));
                }
                else {
                    cx_tmp = (x < cx1) ? cx1 : cx2;
                    real_r2 = sgl_pow2(x - cx_tmp) + dy2;
                    if (real_r2 >= r2_max) {
                        if(x > cx_tmp) break; else continue;
                    }
                    else if (real_r2 >= r2) {
                        edge_alpha = ((r2_max - real_r2) * r2_fix_diff) >> SGL_FIXED_SHIFT;
                        *blend = (alpha == SGL_ALPHA_MAX ? sgl_color_mixer(*pbuf, *blend, edge_alpha) : sgl_color_mixer(sgl_color_mixer(*pbuf, *blend, edge_alpha), *blend, alpha));
                    }
                    else {
                        *blend = (alpha == SGL_ALPHA_MAX ? *pbuf : sgl_color_mixer(*pbuf, *blend, alpha));
                    }
                }
            }
        }
        buf += surf->w;
    }
}

/**
 * @brief fill a round rectangle pixmap with individual corner radii and alpha
 * @param surf point to surface
 * @param area area of rectangle that you want to draw
 * @param rect point to rectangle that you want to draw
 * @param tl_radius radius of top-left corner
 * @param tr_radius radius of top-right corner
 * @param bl_radius radius of bottom-left corner
 * @param br_radius radius of bottom-right corner
 * @param pixmap pixmap of rectangle
 * @param alpha alpha of rectangle
 * @return none
 */
void sgl_draw_fill_rect_pixmap_rich(sgl_surf_t *surf, sgl_area_t *area, sgl_area_t *rect, 
                                    int16_t tl_radius, int16_t tr_radius, int16_t bl_radius, int16_t br_radius,
                                    const sgl_pixmap_t *pixmap, uint8_t alpha)
{
    sgl_area_t clip = SGL_AREA_INVALID;
    sgl_color_t *buf = NULL, *blend = NULL, *pbuf = NULL;
    
    sgl_surf_clip_area_return(surf, area, &clip);
    if (!sgl_area_selfclip(&clip, rect)) return;

    const int32_t rect_w = rect->x2 - rect->x1 + 1;
    const int32_t rect_h = rect->y2 - rect->y1 + 1;
    const int32_t scale_x = ((int32_t)pixmap->width << SGL_FIXED_SHIFT) / rect_w;
    const int32_t scale_y = ((int32_t)pixmap->height << SGL_FIXED_SHIFT) / rect_h;
    uint32_t step_x = 0, step_y = 0;

    if (tl_radius <= 0 && tr_radius <= 0 && bl_radius <= 0 && br_radius <= 0) {
        buf = sgl_surf_get_buf(surf, clip.x1 - surf->x1, clip.y1 - surf->y1);
        for (int y = clip.y1; y <= clip.y2; y++) {
            blend = buf;
            step_y = (scale_y * (y - rect->y1)) >> SGL_FIXED_SHIFT;
            for (int x = clip.x1; x <= clip.x2; x++, blend++) {
                step_x = (scale_x * (x - rect->x1)) >> SGL_FIXED_SHIFT;
                pbuf = sgl_pixmap_get_buf(pixmap, step_x, step_y);
                *blend = (alpha == SGL_ALPHA_MAX ? *pbuf : sgl_color_mixer(*pbuf, *blend, alpha));
            }
            buf += surf->w;
        }
        return;
    }

    int16_t max_r = sgl_min(rect_w / 2, rect_h / 2);
    tl_radius = sgl_min(sgl_max(0, tl_radius), max_r);
    tr_radius = sgl_min(sgl_max(0, tr_radius), max_r);
    bl_radius = sgl_min(sgl_max(0, bl_radius), max_r);
    br_radius = sgl_min(sgl_max(0, br_radius), max_r);

    const int mid_x = rect->x1 + (rect_w >> 1);
    const int mid_y = rect->y1 + (rect_h >> 1);

    // TL: Top-Left
    const int cx_tl = rect->x1 + tl_radius;
    const int cy_tl = rect->y1 + tl_radius;
    const int r2_tl = sgl_pow2(tl_radius);
    const int r2_max_tl = sgl_pow2(tl_radius + 1);
    const int r2_fix_diff_tl = (SGL_ALPHA_MAX << SGL_FIXED_SHIFT) / sgl_max(r2_max_tl - r2_tl, 1);

    // TR: Top-Right
    const int cx_tr = rect->x2 - tr_radius;
    const int cy_tr = rect->y1 + tr_radius;
    const int r2_tr = sgl_pow2(tr_radius);
    const int r2_max_tr = sgl_pow2(tr_radius + 1);
    const int r2_fix_diff_tr = (SGL_ALPHA_MAX << SGL_FIXED_SHIFT) / sgl_max(r2_max_tr - r2_tr, 1);

    // BL: Bottom-Left
    const int cx_bl = rect->x1 + bl_radius;
    const int cy_bl = rect->y2 - bl_radius;
    const int r2_bl = sgl_pow2(bl_radius);
    const int r2_max_bl = sgl_pow2(bl_radius + 1);
    const int r2_fix_diff_bl = (SGL_ALPHA_MAX << SGL_FIXED_SHIFT) / sgl_max(r2_max_bl - r2_bl, 1);

    // BR: Bottom-Right
    const int cx_br = rect->x2 - br_radius;
    const int cy_br = rect->y2 - br_radius;
    const int r2_br = sgl_pow2(br_radius);
    const int r2_max_br = sgl_pow2(br_radius + 1);
    const int r2_fix_diff_br = (SGL_ALPHA_MAX << SGL_FIXED_SHIFT) / sgl_max(r2_max_br - r2_br, 1);

    buf = sgl_surf_get_buf(surf, clip.x1 - surf->x1, clip.y1 - surf->y1);

    for (int y = clip.y1; y <= clip.y2; y++) {
        blend = buf;
        step_y = (scale_y * (y - rect->y1)) >> SGL_FIXED_SHIFT;

        for (int x = clip.x1; x <= clip.x2; x++, blend++) {
            step_x = (scale_x * (x - rect->x1)) >> SGL_FIXED_SHIFT;
            pbuf = sgl_pixmap_get_buf(pixmap, step_x, step_y);

            int cx, cy, r2, r2_max, r2_fix_diff;
            bool is_corner = false;

            if (y < mid_y) {
                if (x < mid_x && tl_radius > 0 && x < cx_tl && y < cy_tl) {
                    cx = cx_tl; cy = cy_tl; r2 = r2_tl; r2_max = r2_max_tl; r2_fix_diff = r2_fix_diff_tl;
                    is_corner = true;
                } else if (x >= mid_x && tr_radius > 0 && x > cx_tr && y < cy_tr) {
                    cx = cx_tr; cy = cy_tr; r2 = r2_tr; r2_max = r2_max_tr; r2_fix_diff = r2_fix_diff_tr;
                    is_corner = true;
                }
            } else {
                if (x < mid_x && bl_radius > 0 && x < cx_bl && y > cy_bl) {
                    cx = cx_bl; cy = cy_bl; r2 = r2_bl; r2_max = r2_max_bl; r2_fix_diff = r2_fix_diff_bl;
                    is_corner = true;
                } else if (x >= mid_x && br_radius > 0 && x > cx_br && y > cy_br) {
                    cx = cx_br; cy = cy_br; r2 = r2_br; r2_max = r2_max_br; r2_fix_diff = r2_fix_diff_br;
                    is_corner = true;
                }
            }

            if (is_corner) {
                int real_r2 = sgl_pow2(x - cx) + sgl_pow2(y - cy);
                if (real_r2 >= r2_max) {
                    continue; 
                } else if (real_r2 >= r2) {
                    uint8_t edge_alpha = ((r2_max - real_r2) * r2_fix_diff) >> SGL_FIXED_SHIFT;
                    *blend = (alpha == SGL_ALPHA_MAX ? 
                              sgl_color_mixer(*pbuf, *blend, edge_alpha) : 
                              sgl_color_mixer(sgl_color_mixer(*pbuf, *blend, edge_alpha), *blend, alpha));
                } else {
                    *blend = (alpha == SGL_ALPHA_MAX ? *pbuf : sgl_color_mixer(*pbuf, *blend, alpha));
                }
            } else {
                *blend = (alpha == SGL_ALPHA_MAX ? *pbuf : sgl_color_mixer(*pbuf, *blend, alpha));
            }
        }
        buf += surf->w;
    }
}
#else
/**
 * @brief fill a round rectangle pixmap with alpha
 * @param surf point to surface
 * @param area area of rectangle that you want to draw
 * @param rect point to rectangle that you want to draw
 * @param radius radius of round
 * @param pixmap pixmap of rectangle
 * @param alpha alpha of rectangle
 * @return none
 */
void sgl_draw_fill_rect_pixmap(sgl_surf_t *surf, sgl_area_t *area, sgl_area_t *rect, int16_t radius, const sgl_pixmap_t *pixmap, uint8_t alpha)
{
    sgl_area_t clip = SGL_AREA_INVALID;
    sgl_color_t *buf = NULL, *blend = NULL, *pbuf = (sgl_color_t *)pixmap->bitmap.array, ip_color;
    
    sgl_surf_clip_area_return(surf, area, &clip);
    if (!sgl_area_selfclip(&clip, rect)) return;

    int cx_tmp = 0, fx = 0;
    int cy_tmp = 0, fy = 0;
    const int32_t rect_w = rect->x2 - rect->x1 + 1;
    const int32_t rect_h = rect->y2 - rect->y1 + 1;
    const int32_t scale_x = ((int32_t)pixmap->width << SGL_FIXED_SHIFT) / rect_w;
    const int32_t scale_y = ((int32_t)pixmap->height << SGL_FIXED_SHIFT) / rect_h;
    buf = sgl_surf_get_buf(surf, clip.x1 - surf->x1, clip.y1 - surf->y1);
    const bool no_scale = (scale_x == SGL_FIXED_ONE && scale_y == SGL_FIXED_ONE);

    if (radius == 0) {
        for (int y = clip.y1; y <= clip.y2; y++) {
            blend = buf;
            if (no_scale) {
                for (int x = clip.x1; x <= clip.x2; x++, blend++) {
                    pbuf = sgl_pixmap_get_buf(pixmap, x - rect->x1, y - rect->y1);
                    *blend = (alpha == SGL_ALPHA_MAX) ? *pbuf : sgl_color_mixer(*pbuf, *blend, alpha);
                }
            } else {
                fy = (int32_t)(y - rect->y1) * scale_y;
                for (int x = clip.x1; x <= clip.x2; x++, blend++) {
                    fx = (int32_t)(x - rect->x1) * scale_x;
                    ip_color = sgl_draw_biln_color(pbuf, pixmap->width, pixmap->height, fx, fy);
                    *blend = (alpha == SGL_ALPHA_MAX) ? ip_color : sgl_color_mixer(ip_color, *blend, alpha);
                }
            }
            buf += surf->w;
        }
        return;
    }

    const int cx1 = rect->x1 + radius;
    const int cx2 = rect->x2 - radius;
    const int cy1 = rect->y1 + radius;
    const int cy2 = rect->y2 - radius;

    const int r2 = sgl_pow2(radius);
    const int r2_max = sgl_pow2(radius + 1);
    const int r2_diff = sgl_max(r2_max - r2, 1);
    const int r2_fix_diff = (SGL_ALPHA_MAX << SGL_FIXED_SHIFT) / r2_diff;
    int dy2, real_r2;
    uint8_t edge_alpha;

    for (int y = clip.y1; y <= clip.y2; y++) {
        blend = buf;
        if (no_scale) {
            if (y >= cy1 && y <= cy2) {
                for (int x = clip.x1; x <= clip.x2; x++, blend++) {
                    pbuf = sgl_pixmap_get_buf(pixmap, x - rect->x1, y - rect->y1);
                    *blend = (alpha == SGL_ALPHA_MAX) ? *pbuf : sgl_color_mixer(*pbuf, *blend, alpha);
                }
            } else {
                cy_tmp = (y < cy1) ? cy1 : cy2;
                dy2 = sgl_pow2(y - cy_tmp);
                for (int x = clip.x1; x <= clip.x2; x++, blend++) {
                    pbuf = sgl_pixmap_get_buf(pixmap, x - rect->x1, y - rect->y1);
                    if (x >= cx1 && x <= cx2) {
                        *blend = (alpha == SGL_ALPHA_MAX) ? *pbuf : sgl_color_mixer(*pbuf, *blend, alpha);
                    } else {
                        cx_tmp = (x < cx1) ? cx1 : cx2;
                        real_r2 = sgl_pow2(x - cx_tmp) + dy2;
                        if (real_r2 >= r2_max) {
                            if (x > cx_tmp) break; else continue;
                        } else if (real_r2 >= r2) {
                            edge_alpha = ((r2_max - real_r2) * r2_fix_diff) >> SGL_FIXED_SHIFT;
                            *blend = (alpha == SGL_ALPHA_MAX ? sgl_color_mixer(*pbuf, *blend, edge_alpha) :
                                     sgl_color_mixer(sgl_color_mixer(*pbuf, *blend, edge_alpha), *blend, alpha));
                        } else {
                            *blend = (alpha == SGL_ALPHA_MAX) ? *pbuf : sgl_color_mixer(*pbuf, *blend, alpha);
                        }
                    }
                }
            }
        } else {
            fy = (int32_t)(y - rect->y1) * scale_y;
            if (y >= cy1 && y <= cy2) {
                for (int x = clip.x1; x <= clip.x2; x++, blend++) {
                    fx = (int32_t)(x - rect->x1) * scale_x;
                    ip_color = sgl_draw_biln_color(pbuf, pixmap->width, pixmap->height, fx, fy);
                    *blend = (alpha == SGL_ALPHA_MAX) ? ip_color : sgl_color_mixer(ip_color, *blend, alpha);
                }
            } else {
                cy_tmp = (y < cy1) ? cy1 : cy2;
                dy2 = sgl_pow2(y - cy_tmp);
                for (int x = clip.x1; x <= clip.x2; x++, blend++) {
                    fx = (int32_t)(x - rect->x1) * scale_x;
                    ip_color = sgl_draw_biln_color(pbuf, pixmap->width, pixmap->height, fx, fy);
                    if (x >= cx1 && x <= cx2) {
                        *blend = (alpha == SGL_ALPHA_MAX) ? ip_color : sgl_color_mixer(ip_color, *blend, alpha);
                    } else {
                        cx_tmp = (x < cx1) ? cx1 : cx2;
                        real_r2 = sgl_pow2(x - cx_tmp) + dy2;
                        if (real_r2 >= r2_max) {
                            if (x > cx_tmp) break; else continue;
                        } else if (real_r2 >= r2) {
                            edge_alpha = ((r2_max - real_r2) * r2_fix_diff) >> SGL_FIXED_SHIFT;
                            *blend = (alpha == SGL_ALPHA_MAX ? sgl_color_mixer(ip_color, *blend, edge_alpha) : 
                                     sgl_color_mixer(sgl_color_mixer(ip_color, *blend, edge_alpha), *blend, alpha));
                        } else {
                            *blend = (alpha == SGL_ALPHA_MAX) ? ip_color : sgl_color_mixer(ip_color, *blend, alpha);
                        }
                    }
                }
            }
        }
        buf += surf->w;
    }
}

/**
 * @brief fill a round rectangle pixmap with individual corner radii and alpha
 * @param surf point to surface
 * @param area area of rectangle that you want to draw
 * @param rect point to rectangle that you want to draw
 * @param tl_radius radius of top-left corner
 * @param tr_radius radius of top-right corner
 * @param bl_radius radius of bottom-left corner
 * @param br_radius radius of bottom-right corner
 * @param pixmap pixmap of rectangle
 * @param alpha alpha of rectangle
 * @return none
 */
void sgl_draw_fill_rect_pixmap_rich(sgl_surf_t *surf, sgl_area_t *area, sgl_area_t *rect, 
                                    int16_t tl_radius, int16_t tr_radius, int16_t bl_radius, int16_t br_radius,
                                    const sgl_pixmap_t *pixmap, uint8_t alpha)
{
    sgl_area_t clip = SGL_AREA_INVALID;
    sgl_color_t *buf = NULL, *blend = NULL, *pbuf = (sgl_color_t *)pixmap->bitmap.array, ip_color;
    
    sgl_surf_clip_area_return(surf, area, &clip);
    if (!sgl_area_selfclip(&clip, rect)) return;

    int fx = 0, fy = 0;
    const int32_t rect_w = rect->x2 - rect->x1 + 1;
    const int32_t rect_h = rect->y2 - rect->y1 + 1;
    const int32_t scale_x = ((int32_t)pixmap->width << SGL_FIXED_SHIFT) / rect_w;
    const int32_t scale_y = ((int32_t)pixmap->height << SGL_FIXED_SHIFT) / rect_h;
    buf = sgl_surf_get_buf(surf, clip.x1 - surf->x1, clip.y1 - surf->y1);
    const bool no_scale = (scale_x == SGL_FIXED_ONE && scale_y == SGL_FIXED_ONE);

    int16_t max_r = sgl_min(rect_w / 2, rect_h / 2);
    tl_radius = sgl_min(sgl_max((int16_t)0, tl_radius), max_r);
    tr_radius = sgl_min(sgl_max((int16_t)0, tr_radius), max_r);
    bl_radius = sgl_min(sgl_max((int16_t)0, bl_radius), max_r);
    br_radius = sgl_min(sgl_max((int16_t)0, br_radius), max_r);

    if (tl_radius == 0 && tr_radius == 0 && bl_radius == 0 && br_radius == 0) {
        for (int y = clip.y1; y <= clip.y2; y++) {
            blend = buf;
            if (no_scale) {
                for (int x = clip.x1; x <= clip.x2; x++, blend++) {
                    pbuf = sgl_pixmap_get_buf(pixmap, x - rect->x1, y - rect->y1);
                    *blend = (alpha == SGL_ALPHA_MAX) ? *pbuf : sgl_color_mixer(*pbuf, *blend, alpha);
                }
            } else {
                fy = (int32_t)(y - rect->y1) * scale_y;
                for (int x = clip.x1; x <= clip.x2; x++, blend++) {
                    fx = (int32_t)(x - rect->x1) * scale_x;
                    ip_color = sgl_draw_biln_color(pbuf, pixmap->width, pixmap->height, fx, fy);
                    *blend = (alpha == SGL_ALPHA_MAX) ? ip_color : sgl_color_mixer(ip_color, *blend, alpha);
                }
            }
            buf += surf->w;
        }
        return;
    }

    const int cx_tl = rect->x1 + tl_radius;
    const int cy_tl = rect->y1 + tl_radius;
    const int r2_tl = sgl_pow2(tl_radius);
    const int r2_max_tl = sgl_pow2(tl_radius + 1);
    const int r2_fix_diff_tl = (SGL_ALPHA_MAX << SGL_FIXED_SHIFT) / sgl_max(r2_max_tl - r2_tl, 1);

    const int cx_tr = rect->x2 - tr_radius;
    const int cy_tr = rect->y1 + tr_radius;
    const int r2_tr = sgl_pow2(tr_radius);
    const int r2_max_tr = sgl_pow2(tr_radius + 1);
    const int r2_fix_diff_tr = (SGL_ALPHA_MAX << SGL_FIXED_SHIFT) / sgl_max(r2_max_tr - r2_tr, 1);

    const int cx_bl = rect->x1 + bl_radius;
    const int cy_bl = rect->y2 - bl_radius;
    const int r2_bl = sgl_pow2(bl_radius);
    const int r2_max_bl = sgl_pow2(bl_radius + 1);
    const int r2_fix_diff_bl = (SGL_ALPHA_MAX << SGL_FIXED_SHIFT) / sgl_max(r2_max_bl - r2_bl, 1);

    const int cx_br = rect->x2 - br_radius;
    const int cy_br = rect->y2 - br_radius;
    const int r2_br = sgl_pow2(br_radius);
    const int r2_max_br = sgl_pow2(br_radius + 1);
    const int r2_fix_diff_br = (SGL_ALPHA_MAX << SGL_FIXED_SHIFT) / sgl_max(r2_max_br - r2_br, 1);

    const int mid_x = rect->x1 + (rect_w >> 1);
    const int mid_y = rect->y1 + (rect_h >> 1);

    int cx_tmp = 0, cy_tmp = 0;
    int r2 = 0, r2_max = 0, r2_fix_diff = 0;
    int dy2 = 0, real_r2 = 0;
    uint8_t edge_alpha = 0;
    sgl_color_t src_color;

    for (int y = clip.y1; y <= clip.y2; y++) {
        blend = buf;
        fy = no_scale ? 0 : (int32_t)(y - rect->y1) * scale_y;

        for (int x = clip.x1; x <= clip.x2; x++, blend++) {
            if (no_scale) {
                src_color = *sgl_pixmap_get_buf(pixmap, x - rect->x1, y - rect->y1);
            } else {
                fx = (int32_t)(x - rect->x1) * scale_x;
                src_color = sgl_draw_biln_color(pbuf, pixmap->width, pixmap->height, fx, fy);
            }

            bool in_corner = false;
            if (y < mid_y) {
                if (x < mid_x && tl_radius > 0) {
                    if (x < cx_tl && y < cy_tl) {
                        cx_tmp = cx_tl; cy_tmp = cy_tl;
                        r2 = r2_tl; r2_max = r2_max_tl; r2_fix_diff = r2_fix_diff_tl;
                        in_corner = true;
                    }
                } else if (x >= mid_x && tr_radius > 0) {
                    if (x > cx_tr && y < cy_tr) {
                        cx_tmp = cx_tr; cy_tmp = cy_tr;
                        r2 = r2_tr; r2_max = r2_max_tr; r2_fix_diff = r2_fix_diff_tr;
                        in_corner = true;
                    }
                }
            } else {
                if (x < mid_x && bl_radius > 0) {
                    if (x < cx_bl && y > cy_bl) {
                        cx_tmp = cx_bl; cy_tmp = cy_bl;
                        r2 = r2_bl; r2_max = r2_max_bl; r2_fix_diff = r2_fix_diff_bl;
                        in_corner = true;
                    }
                } else if (x >= mid_x && br_radius > 0) {
                    if (x > cx_br && y > cy_br) {
                        cx_tmp = cx_br; cy_tmp = cy_br;
                        r2 = r2_br; r2_max = r2_max_br; r2_fix_diff = r2_fix_diff_br;
                        in_corner = true;
                    }
                }
            }

            if (in_corner) {
                dy2 = sgl_pow2(y - cy_tmp);
                real_r2 = sgl_pow2(x - cx_tmp) + dy2;

                if (real_r2 >= r2_max) {
                    if (x > cx_tmp) break; 
                    else continue;
                } else if (real_r2 >= r2) {
                    edge_alpha = ((r2_max - real_r2) * r2_fix_diff) >> SGL_FIXED_SHIFT;
                    *blend = (alpha == SGL_ALPHA_MAX) ? 
                              sgl_color_mixer(src_color, *blend, edge_alpha) : 
                              sgl_color_mixer(sgl_color_mixer(src_color, *blend, edge_alpha), *blend, alpha);
                } else {
                    *blend = (alpha == SGL_ALPHA_MAX) ? src_color : sgl_color_mixer(src_color, *blend, alpha);
                }
            } else {
                *blend = (alpha == SGL_ALPHA_MAX) ? src_color : sgl_color_mixer(src_color, *blend, alpha);
            }
        }
        buf += surf->w;
    }
}
#endif

/**
 * @brief fill a round rectangle with alpha
 * @param surf point to surface
 * @param area area of rectangle that you want to draw
 * @param rect point to rectangle that you want to draw
 * @param desc rectangle description
 * @return none
 */
void sgl_draw_rect(sgl_surf_t *surf, sgl_area_t *area, sgl_area_t *rect, sgl_draw_rect_t *desc)
{
    sgl_area_t rect_tmp = {
        .x1 = rect->x1 + desc->border,
        .x2 = rect->x2 - desc->border,
        .y1 = rect->y1 + desc->border,
        .y2 = rect->y2 - desc->border
    };

    if (desc->pixmap == NULL) {
        sgl_draw_fill_rect(surf, area, &rect_tmp, desc->radius - desc->border, desc->color, desc->alpha);
    }
    else {
        sgl_draw_fill_rect_pixmap(surf, area, &rect_tmp, desc->radius - desc->border, desc->pixmap, desc->alpha);
    }

    if (desc->border && (!desc->border_mask)) {
        sgl_draw_fill_rect_border(surf, area, rect, desc->radius, desc->border_color, desc->border, desc->border_alpha);
    }
}
