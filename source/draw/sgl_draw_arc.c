/* source/draw/sgl_draw_arc.c
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
#include <sgl_log.h>
#include <sgl_math.h>

typedef struct sgl_arc_dot {
    int16_t  cx;
    int16_t  cy;
    int16_t  r;
    uint32_t r2;
    uint16_t rmax;
    uint16_t outer;
} sgl_arc_dot_t;

static void arc_dot_sin_cos(int16_t cx, int16_t cy, int16_t radius_in, int16_t radius_out, sgl_arc_dot_t *dot,int sin, int cos)
{
    int len = (radius_out + radius_in) / 2;
    int r = (radius_out - radius_in) / 2;
    if (unlikely(r == 0)) {
        return;
    }

    if (sin < 0) {
        dot->cx = (sin * len - 16348) / 32768;
    }
    else {
        dot->cx = (sin * len + 16348) / 32768;
    }
    if (cos<0) {
        dot->cy = (cos * len - 16348) / 32768;
    }
    else {
        dot->cy = (cos * len + 16348) / 32768;
    }

    dot->cx = cx - dot->cx;
    dot->cy = cy - dot->cy;
    dot->r = r + 1;
    dot->r2 =  sgl_pow2(r);
    dot->rmax = sgl_pow2(r + 1);
    dot->outer = 0;
}


static inline uint8_t arc_get_dot(sgl_arc_dot_t *dot,int ax, int ay)
{
    uint32_t temp;
    uint8_t alpha = SGL_ALPHA_MIN, max = SGL_ALPHA_MIN;
    sgl_arc_dot_t *p = dot;
    int32_t rate = (0xff00) / (dot->rmax - dot->r2);

    for (int k = 0; k < 2; k++, p++) {
        int x = ax > p->cx ? ax - p->cx : p->cx-ax;
        int y = ay > p->cy ? ay - p->cy : p->cy-ay;

        if ((x < p->r) && (y < p->r)) {
            temp = sgl_pow2(x) + sgl_pow2(y);
            if (temp >= p->rmax) {
                alpha = 0;
            }
            else if (temp > p->r2) {
                if(dot->outer==0) {
                    dot->outer = rate;
                }
                alpha = (p->rmax - temp) * dot->outer >> 8;
            }
            else {
                alpha = SGL_ALPHA_MAX;
            }
            max = sgl_max(max, alpha);
        }
    }
    return max;
}


/**
 * @brief draw an arc with alpha
 * @param surf pointer to surface
 * @param area pointer to area
 * @param desc pointer to arc description
 * @return none
 */
void sgl_draw_fill_arc(sgl_surf_t *surf, sgl_area_t *area, sgl_draw_arc_t *desc)
{
    sgl_area_t clip = SGL_AREA_MAX;
    sgl_surf_clip_area_return(surf, area, &clip);

    sgl_area_t c_rect = {
        .x1 = desc->cx - desc->radius_out,
        .x2 = desc->cx + desc->radius_out,
        .y1 = desc->cy - desc->radius_out,
        .y2 = desc->cy + desc->radius_out
    };

    if (!sgl_area_selfclip(&clip, &c_rect)) {
        return;
    }

    const int32_t in_r2 = sgl_pow2(desc->radius_in);
    const int32_t out_r2 = sgl_pow2(desc->radius_out);
    const int32_t in_r2_max = sgl_pow2(desc->radius_in - 1);
    const int32_t out_r2_max = sgl_pow2(desc->radius_out + 1);

    const int32_t rate_in = (in_r2 != in_r2_max) ? ((0xff00) / (in_r2 - in_r2_max)) : 0;
    const int32_t rate_out = (out_r2_max != out_r2) ? ((0xff00) / (out_r2_max - out_r2)) : 0;

    const bool is_full_circle = (desc->start_angle == 0 && desc->end_angle == 360);
    uint8_t flag = 0xff;
    int32_t sx = 0, sy = 0, ex = 0, ey = 0;

    sgl_arc_dot_t arc_dot[2];

    if (!is_full_circle) {
        int16_t arc_span = desc->end_angle - desc->start_angle;
        if (arc_span < 0) arc_span += 360;
        flag = (arc_span > 180) ? 1 : 0;

        sx = sgl_sin(desc->start_angle) >> 7;
        sy = -sgl_cos(desc->start_angle) >> 7;
        ex = sgl_sin(desc->end_angle) >> 7;
        ey = -sgl_cos(desc->end_angle) >> 7;

        if (desc->mode == SGL_ARC_MODE_NORMAL_SMOOTH || desc->mode == SGL_ARC_MODE_RING_SMOOTH) {
            arc_dot_sin_cos(desc->cx, desc->cy, desc->radius_in, desc->radius_out, &arc_dot[0], sgl_sin(desc->start_angle), -sgl_cos(desc->start_angle));
            arc_dot_sin_cos(desc->cx, desc->cy, desc->radius_in, desc->radius_out, &arc_dot[1], sgl_sin(desc->end_angle), -sgl_cos(desc->end_angle));
        }
    }

    sgl_color_t *line_buf = sgl_surf_get_buf(surf, clip.x1 - surf->x1, clip.y1 - surf->y1);
    const int32_t stride = surf->w;
    const sgl_color_t color = desc->color;
    const uint8_t global_alpha = desc->alpha;

    for (int32_t y = clip.y1; y <= clip.y2; y++) {
        const int32_t dy = y - desc->cy;
        const int32_t y2 = dy * dy;

        const int32_t ds_base = -dy * sx;
        const int32_t de_base =  dy * ex;

        sgl_color_t *blend = line_buf;

        int32_t dx = clip.x1 - desc->cx;
        int32_t x2 = dx * dx;
        int32_t dx2_inc = (dx << 1) + 1;

        int32_t ds = dx * sy + ds_base;
        int32_t de = -dx * ey + de_base;

        for (int32_t x = clip.x1; x <= clip.x2; x++, blend++) {
            int32_t real_r2 = x2 + y2;

            if (real_r2 >= out_r2_max) {
                if (dx > 0) {
                    break;
                }
                goto NEXT_X;
            }

            if (real_r2 < in_r2_max) {
                if (dx < 0) {
                    int32_t skip = -dx * 2; 

                    if (skip > 0) {
                        int32_t target_x = x + skip;
                        if (target_x > clip.x2) {
                            break; 
                        }

                        x = target_x - 1;
                        blend += (skip - 1);

                        dx = x - desc->cx;
                        x2 = dx * dx;
                        dx2_inc = (dx << 1) + 1;
                        ds = dx * sy + ds_base;
                        de = -dx * ey + de_base;

                        goto NEXT_X;
                    }
                }
                goto NEXT_X;
            }

            uint8_t edge_alpha;
            if (real_r2 < in_r2) {
                edge_alpha = (uint8_t)((real_r2 - in_r2_max) * rate_in >> 8);
            } else if (real_r2 > out_r2) {
                edge_alpha = (uint8_t)((out_r2_max - real_r2) * rate_out >> 8);
            } else {
                edge_alpha = SGL_ALPHA_MAX;
            }

            sgl_color_t tmp_color = color;

            if (flag != 0xff) {
                bool in_range = flag > 0 ? (ds > 0 || de > 0) : (ds >= 0 && de >= 0);
                if (!in_range) {
                    if (desc->mode == SGL_ARC_MODE_NORMAL) {
                        int32_t sd = sgl_xy_has_component(dx, dy, sx, sy) ? sgl_abs(ds) : 256;
                        int32_t ed = sgl_xy_has_component(dx, dy, ex, ey) ? sgl_abs(de) : 256;
                        int32_t d_min = sgl_min(sd, ed);
                        if (d_min < SGL_ALPHA_MAX) {
                            tmp_color = sgl_color_mixer(color, *blend, sgl_min(255 - d_min, edge_alpha));
                        } else {
                            goto NEXT_X;
                        }
                    } else if (desc->mode == SGL_ARC_MODE_RING) {
                        int32_t sd = sgl_xy_has_component(dx, dy, sx, sy) ? sgl_abs(ds) : 256;
                        int32_t ed = sgl_xy_has_component(dx, dy, ex, ey) ? sgl_abs(de) : 256;
                        int32_t d_min = sgl_min(sd, ed);
                        tmp_color = (d_min < SGL_ALPHA_MAX) ? sgl_color_mixer(color, desc->bg_color, sgl_min(255 - d_min, edge_alpha)) : desc->bg_color;
                    } else if (desc->mode == SGL_ARC_MODE_NORMAL_SMOOTH || desc->mode == SGL_ARC_MODE_RING_SMOOTH) {
                        uint8_t dot_alpha = arc_get_dot(arc_dot, x, y);
                        sgl_color_t bg = (desc->mode == SGL_ARC_MODE_RING_SMOOTH) ? desc->bg_color : *blend;
                        tmp_color = (dot_alpha < SGL_ALPHA_MAX) ? sgl_color_mixer(color, bg, dot_alpha) : color;
                    }
                }
            }

            if (edge_alpha == SGL_ALPHA_MAX && global_alpha == SGL_ALPHA_MAX) {
                *blend = tmp_color;
            } else {
                if (global_alpha == SGL_ALPHA_MAX) {
                    *blend = sgl_color_mixer(tmp_color, *blend, edge_alpha);
                } else {
                    *blend = sgl_color_mixer(sgl_color_mixer(tmp_color, *blend, edge_alpha), *blend, global_alpha);
                }
            }

NEXT_X:
            x2 += dx2_inc;
            dx2_inc += 2;
            dx++;
            ds += sy;
            de -= ey;
        }

        line_buf += stride;
    }
}
