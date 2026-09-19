/* source/draw/sgl_draw_ring.c
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
 * @brief draw a ring on surface with alpha
 * @param surf: pointer of surface
 * @param area: pointer of area
 * @param cx: ring center x
 * @param cy: ring center y
 * @param radius_in: ring inner radius
 * @param radius_out: ring outer radius
 * @param color: ring color
 * @param alpha: ring alpha
 * @return none
 */
void sgl_draw_fill_ring(sgl_surf_t *surf, sgl_area_t *area, int16_t cx, int16_t cy, int16_t radius_in, int16_t radius_out, sgl_color_t color, uint8_t alpha)
{
    sgl_area_t clip = SGL_AREA_MAX;
    sgl_surf_clip_area_return(surf, area, &clip);
    if (unlikely(alpha == SGL_ALPHA_MIN)) return;

    const int cx2 = cx * 2 + 1;
    const int cy2 = cy * 2 + 1;
    const int out_diameter = radius_out << 1;
    const int out_r2_max   = sgl_pow2(out_diameter);
    const int out_r2       = sgl_max(sgl_pow2(out_diameter - 4), 0);
    const int out_diff     = out_r2_max - out_r2;
    const int out_fix      = (SGL_ALPHA_MAX << SGL_FIXED_SHIFT) / (out_diff > 0 ? out_diff : 1);

    const int in_diameter  = radius_in << 1;
    const int in_r2_max    = sgl_pow2(in_diameter);
    const int in_r2        = sgl_max(sgl_pow2(in_diameter - 4), 0);
    const int in_diff      = in_r2_max - in_r2;
    const int in_fix       = (SGL_ALPHA_MAX << SGL_FIXED_SHIFT) / (in_diff > 0 ? in_diff : 1);
    const uint8_t use_alpha = (alpha != SGL_ALPHA_MAX);

    uint8_t edge_alpha;
    sgl_color_t edge_c;
    sgl_color_t *blend, *buf = sgl_surf_get_buf(surf, clip.x1 - surf->x1, clip.y1 - surf->y1);
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
                if (x < cx) {
                    int skip = cx - x;
                    blend += skip;
                    x += skip;
                }
                continue;
            }

            if (dx2 <= in_r2_max) {
                edge_alpha = ((dx2 - in_r2) * in_fix) >> SGL_FIXED_SHIFT;
                edge_c = sgl_color_mixer(color, *blend, edge_alpha);
                *blend = use_alpha ? sgl_color_mixer(edge_c, *blend, alpha) : edge_c;
            }
            else if (dx2 <= out_r2) {
                *blend = use_alpha ? sgl_color_mixer(color, *blend, alpha) : color;
            }
            else {
                edge_alpha = ((out_r2_max - dx2) * out_fix) >> SGL_FIXED_SHIFT;
                edge_c = sgl_color_mixer(color, *blend, edge_alpha);
                *blend = use_alpha ? sgl_color_mixer(edge_c, *blend, alpha) : edge_c;
            }
        }
        buf += surf->w;
    }
}
