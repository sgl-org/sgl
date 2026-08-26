/* source/widgets/sgl_led.c
 *
 * MIT License
 *
 * Copyright(c) 2023-present All contributors of SGL  
 * Document reference link: docs directory
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
#include "sgl_led.h"


static void sgl_led_construct_cb(sgl_surf_t *surf, sgl_obj_t* obj, sgl_event_t *evt)
{
    sgl_led_t *led = sgl_container_of(obj, sgl_led_t, obj);
    int16_t cx = 0, cy = 0;
    sgl_color_t color = led->status ? led->on_color : led->off_color;

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
        const int r2 = sgl_max(sgl_pow2(diameter - 6), 0); 
        const int r2_fix_diff = (SGL_ALPHA_MAX  << SGL_FIXED_SHIFT) / sgl_max(r2_max - r2, 1);
        int ds_alpha = SGL_ALPHA_MIN;
        sgl_color_t *blend, *buf;

        buf = sgl_surf_get_buf(surf, clip.x1 - surf->x1, clip.y1 - surf->y1);
        for (int y = clip.y1; y <= clip.y2; y++) {
            blend = buf;
            dy2 = sgl_pow2(2 * y - cy2);

            for (int x = clip.x1; x <= clip.x2; x++, blend++) {
                dx2 = sgl_pow2(2 * x - cx2) + dy2;
                if (dx2 >= r2_max) {
                    if(x > cx)
                        break;
                    continue;
                }
                else if (dx2 >= r2) {
                    edge_alpha = ((r2_max - dx2) * r2_fix_diff) >> SGL_FIXED_SHIFT;
                    sgl_color_t color_mix = sgl_color_mixer(led->bg_color, *blend, edge_alpha);
                    *blend = sgl_color_mixer(color_mix, *blend, led->alpha);
                }
                else {
                    ds_alpha = dx2 * SGL_ALPHA_NUM / r2;
                    ds_alpha = sgl_pow2(ds_alpha) / SGL_ALPHA_NUM ;
                    *blend = sgl_color_mixer(sgl_color_mixer(led->bg_color, color, ds_alpha), *blend, led->alpha);//SGL_THEME_BG_COLOR
                }
            }
            buf += surf->w;
        }
    }
}

/**
 * @brief create a led object
 * @param parent parent of the led
 * @return led object
 */
sgl_obj_t* sgl_led_create(sgl_obj_t* parent)
{
    sgl_led_t *led = sgl_malloc(sizeof(sgl_led_t));
    if(led == NULL) {
        SGL_LOG_ERROR("sgl_led_create: malloc failed");
        return NULL;
    }

    /* set object all member to zero */
    memset(led, 0, sizeof(sgl_led_t));

    sgl_obj_t *obj = &led->obj;
    sgl_obj_init(&led->obj, parent);
    obj->construct_fn = sgl_led_construct_cb;

    led->alpha = SGL_ALPHA_MAX;
    led->on_color = SGL_THEME_COLOR;
    led->off_color = SGL_THEME_BG_COLOR;
    led->bg_color = SGL_THEME_BG_COLOR;

    return obj;
}

/**
 * @brief set the radius of the led
 * @param obj led object
 * @param radius radius of the led
 * @return none
 */
void sgl_led_set_radius(sgl_obj_t *obj, uint8_t radius)
{
    sgl_obj_circle_zoom(obj, radius);
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the color of the led
 * @param obj led object
 * @param color color of the led
 * @return none
 */
void sgl_led_set_on_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_led_t *led = sgl_container_of(obj, sgl_led_t, obj);
    led->on_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the off color of the led
 * @param obj led object
 * @param color off color of the led
 * @return none
 */
void sgl_led_set_off_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_led_t *led = sgl_container_of(obj, sgl_led_t, obj);
    led->off_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set the background color of the led
 * @param obj led object
 * @param color background color of the led
 * @return none
 */
void sgl_led_set_bg_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_led_t *led = sgl_container_of(obj, sgl_led_t, obj);
    led->bg_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the alpha of the led
 * @param obj led object
 * @param alpha alpha of the led
 * @return none
 */
void sgl_led_set_alpha(sgl_obj_t *obj, uint8_t alpha)
{
    sgl_led_t *led = sgl_container_of(obj, sgl_led_t, obj);
    led->alpha = alpha;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the status of the led
 * @param obj led object
 * @param status status of the led
 * @return none
 */
void sgl_led_set_status(sgl_obj_t *obj, bool status)
{
    sgl_led_t *led = sgl_container_of(obj, sgl_led_t, obj);
    led->status = status;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief get the status of the led
 * @param obj led object
 * @return status of the led
 */
bool sgl_led_get_status(sgl_obj_t *obj)
{
    sgl_led_t *led = sgl_container_of(obj, sgl_led_t, obj);
    return led->status;
}

/**
 * @brief turn on the led
 * @param obj led object
 * @return none
 */
void sgl_led_on(sgl_obj_t *obj)
{
    sgl_led_set_status(obj, true);
}

/**
 * @brief turn off the led
 * @param obj led object
 * @return none
 */
void sgl_led_off(sgl_obj_t *obj)
{
    sgl_led_set_status(obj, false);
}
