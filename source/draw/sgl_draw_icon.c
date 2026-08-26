/* source/draw/sgl_draw_icon.c
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
 * @brief draw icon with alpha
 * @param surf   surface
 * @param area   area of icon
 * @param coords coords of icon
 * @param alpha  alpha of icon
 * @param icon   icon pixmap
 * @return none
 * @note This function is only for 4bpp icon bitmap
 */
void sgl_draw_icon( sgl_surf_t *surf, sgl_area_t *area, int16_t x, int16_t y, sgl_color_t color, uint8_t alpha, const sgl_icon_pixmap_t *icon)
{
    const uint8_t *dot = icon->bitmap;
    sgl_area_t clip = SGL_AREA_MAX;
    sgl_color_t *buf = NULL;
    int rel_x, rel_y, byte_x, dot_index;
    uint8_t alpha_dot;

    sgl_area_t icon_rect = {
        .x1 = x,
        .x2 = x + icon->width - 1,
        .y1 = y,
        .y2 = y + icon->height - 1,
    };

    if (!sgl_surf_clip(surf, &icon_rect, &clip)) {
        return;
    }

    if (!sgl_area_selfclip(&clip, area)) {
        return;
    }

    for (int y = clip.y1; y <= clip.y2; y++) {
        buf = sgl_surf_get_buf(surf, clip.x1 - surf->x1, y - surf->y1);
        rel_y = y - icon_rect.y1;

        for (int x = clip.x1; x <= clip.x2; x++) {
            rel_x = x - icon_rect.x1;

            byte_x = rel_x >> 1;
            dot_index = byte_x + (rel_y * (icon->width >> 1));
            alpha_dot = (rel_x & 1) ? dot[dot_index] & 0xF : (dot[dot_index] >> 4);
            alpha_dot = alpha_dot | (alpha_dot << 4);
            *buf = (alpha == SGL_ALPHA_MAX ? sgl_color_mixer(color, *buf, alpha_dot) : sgl_color_mixer(sgl_color_mixer(color, *buf, alpha_dot), *buf, alpha));
            buf++;
        }
    }
}
