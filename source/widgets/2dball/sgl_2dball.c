/* source/widgets/sgl_2dball.c
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
#include <sgl_theme.h>
#include <sgl_cfgfix.h>
#include <string.h>
#include "sgl_2dball.h"


static void sgl_2dball_construct_cb(sgl_surf_t *surf, sgl_obj_t* obj, sgl_event_t *evt)
{
    sgl_2dball_t *ball = sgl_container_of(obj, sgl_2dball_t, obj);
    int16_t cx = 0, cy = 0;

    if(evt->type == SGL_EVENT_DRAW_MAIN) {
        sgl_area_t clip = SGL_AREA_MAX;
        sgl_surf_clip_area_return(surf, &obj->area, &clip);

        const int16_t radius = sgl_obj_get_width(obj) / 2;
        cx = (obj->coords.x1 + obj->coords.x2) / 2;
        cy = (obj->coords.y1 + obj->coords.y2) / 2;

        uint8_t edge_alpha;
        int cx2 = 2 * cx + 1;
        int cy2 = 2 * cy + 1;
        int dx2 = 0, dy2 = 0;
        const int diameter = radius << 1;
        const int r2_max = sgl_pow2(diameter);
        const int r2 = sgl_max(sgl_pow2(diameter - 3), 0); 
        const int r2_fix_diff = (SGL_ALPHA_MAX  << SGL_FIXED_SHIFT) / sgl_max(r2_max - r2, 1);
        int ds_alpha = SGL_ALPHA_MIN;
        sgl_color_t *blend, *buf = sgl_surf_get_buf(surf, clip.x1 - surf->x1, clip.y1 - surf->y1);

        buf = sgl_surf_get_buf(surf, clip.x1 - surf->x1, clip.y1 - surf->y1);
        for (int y = clip.y1; y <= clip.y2; y++) {
            blend = buf;
            dy2 = sgl_pow2(2 * y - cy2);

            for (int x = clip.x1; x <= clip.x2; x++, blend++) {
                dx2 = sgl_pow2(2 * x - cx2) + dy2;
                ds_alpha = dx2 * SGL_ALPHA_NUM / r2;

                if (dx2 >= r2_max) {
                    if(x > cx)
                        break;
                    continue;
                }
                else if (dx2 >= r2) {
                    edge_alpha = ((r2_max - dx2) * r2_fix_diff) >> SGL_FIXED_SHIFT;
                    sgl_color_t color_mix = sgl_color_mixer(ball->bg_color, *blend, edge_alpha);
                    *blend = sgl_color_mixer(color_mix, *blend, ball->alpha);
                }
                else {
                    *blend = sgl_color_mixer(sgl_color_mixer(ball->bg_color, ball->color, ds_alpha), *blend, ball->alpha);
                }
            }
            buf += surf->w;
        }
    }
}

/**
 * @brief create a 2dball object
 * @param parent parent of the 2dball
 * @return 2dball object
 */
sgl_obj_t* sgl_2dball_create(sgl_obj_t* parent)
{
    sgl_2dball_t *ball = sgl_malloc(sizeof(sgl_2dball_t));
    if(ball == NULL) {
        SGL_LOG_ERROR("sgl_2dball_create: malloc failed");
        return NULL;
    }

    /* set object all member to zero */
    memset(ball, 0, sizeof(sgl_2dball_t));

    sgl_obj_t *obj = &ball->obj;
    sgl_obj_init(&ball->obj, parent);
    obj->construct_fn = sgl_2dball_construct_cb;

    ball->alpha = SGL_ALPHA_MAX;
    ball->color = SGL_THEME_COLOR;
    ball->bg_color = SGL_THEME_BG_COLOR;

    return obj;
}

/**
 * @brief set the color of the 2dball
 * @param obj 2dball object
 * @param color color
 * @return none
 */
void sgl_2dball_set_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_2dball_t *ball = sgl_container_of(obj, sgl_2dball_t, obj);
    ball->color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the background color of the 2dball
 * @param obj 2dball object
 * @param color background color
 * @return none
 */
void sgl_2dball_set_bg_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_2dball_t *ball = sgl_container_of(obj, sgl_2dball_t, obj);
    ball->bg_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the alpha of the 2dball
 * @param obj 2dball object
 * @param alpha alpha
 * @return none
 */
void sgl_2dball_set_alpha(sgl_obj_t *obj, uint8_t alpha)
{
    sgl_2dball_t *ball = sgl_container_of(obj, sgl_2dball_t, obj);
    ball->alpha = alpha;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the radius of the 2dball
 * @param obj 2dball object
 * @param radius radius
 * @return none
 */
void sgl_2dball_set_radius(sgl_obj_t *obj, uint16_t radius)
{
    sgl_obj_circle_zoom(obj, radius);
    sgl_obj_set_dirty(obj);
}
