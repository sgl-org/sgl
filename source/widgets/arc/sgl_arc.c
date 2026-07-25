/* source/widgets/sgl_arc.c
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
#include "sgl_arc.h"


static void sgl_arc_construct_cb(sgl_surf_t *surf, sgl_obj_t* obj, sgl_event_t *evt)
{
    sgl_arc_t *arc = sgl_container_of(obj, sgl_arc_t, obj);
    int16_t tb_angle = 0;

    if(evt->type == SGL_EVENT_DRAW_MAIN) {
        arc->desc.cx = (obj->coords.x2 + obj->coords.x1) / 2;
        arc->desc.cy = (obj->coords.y2 + obj->coords.y1) / 2;

        if(arc->desc.start_angle == 0 && arc->desc.end_angle == 360) {
            sgl_draw_fill_ring(surf, &obj->area, arc->desc.cx, arc->desc.cy, arc->desc.radius_in, arc->desc.radius_out, arc->desc.color, arc->desc.alpha);
        }
        else if (arc->desc.start_angle != arc->desc.end_angle) {
            sgl_draw_fill_arc(surf, &obj->area, &arc->desc);
        }
    }
    else if(evt->type == SGL_EVENT_PRESSED ||
        evt->type == SGL_EVENT_MOVE_DOWN || evt->type == SGL_EVENT_MOVE_UP || evt->type == SGL_EVENT_MOVE_LEFT || evt->type == SGL_EVENT_MOVE_RIGHT
    ) {
        tb_angle = sgl_atan2(evt->pos.x - arc->desc.cx, evt->pos.y - arc->desc.cy);
        tb_angle = 360 - tb_angle;
        if ((tb_angle != arc->desc.end_angle) && tb_angle >= 0 && tb_angle <= 360) {
            arc->desc.end_angle = tb_angle;
        }

        sgl_obj_set_dirty(obj);
    }
    else if(SGL_EVENT_DRAW_INIT) {
        if(arc->desc.radius_out < 0) {
            arc->desc.radius_out = (obj->coords.x2 - obj->coords.x1) / 2;
        }

        if(arc->desc.radius_in < 0) {
            arc->desc.radius_in = arc->desc.radius_out - 2;
        }
    }
}


/**
 * @brief create an arc object
 * @param parent parent object
 * @return arc object
 */
sgl_obj_t* sgl_arc_create(sgl_obj_t* parent)
{
    sgl_arc_t *arc = sgl_malloc(sizeof(sgl_arc_t));
    if(arc == NULL) {
        SGL_LOG_ERROR("sgl_arc_create: malloc failed");
        return NULL;
    }

    /* set object all member to zero */
    memset(arc, 0, sizeof(sgl_arc_t));

    sgl_obj_t *obj = &arc->obj;
    sgl_obj_init(&arc->obj, parent);
    obj->needinit = 1;
    obj->clickable = 1;
    obj->movable = 1;

    arc->desc.alpha = SGL_THEME_ALPHA;
    arc->desc.mode = SGL_ARC_MODE_NORMAL;
    arc->desc.color = SGL_THEME_BG_COLOR;
    arc->desc.bg_color = SGL_THEME_COLOR;
    arc->desc.start_angle = 0;
    arc->desc.end_angle = 360;
    arc->desc.radius_out = -1;
    arc->desc.radius_in = -1;
    arc->desc.cx = -1;
    arc->desc.cy = -1;

    obj->construct_fn = sgl_arc_construct_cb;

    return obj;
}

/**
 * @brief set arc object color
 * @param obj arc object
 * @param color arc color
 * @return none
 */
void sgl_arc_set_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_arc_t *arc = sgl_container_of(obj, sgl_arc_t, obj);
    arc->desc.color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set arc object background color
 * @param obj arc object
 * @param color arc background color
 * @return none
 */
void sgl_arc_set_bg_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_arc_t *arc = sgl_container_of(obj, sgl_arc_t, obj);
    arc->desc.bg_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set arc object alpha
 * @param obj arc object
 * @param alpha arc alpha
 * @return none
 */
void sgl_arc_set_alpha(sgl_obj_t *obj, uint8_t alpha)
{
    sgl_arc_t *arc = sgl_container_of(obj, sgl_arc_t, obj);
    arc->desc.alpha = alpha;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set arc object radius
 * @param obj arc object
 * @param radius_in arc radius_in
 * @param radius_out arc radius_out
 * @return none
 */
void sgl_arc_set_radius(sgl_obj_t *obj, int16_t radius_in, int16_t radius_out)
{
    sgl_arc_t *arc = sgl_container_of(obj, sgl_arc_t, obj);
    if (obj->radius > 0) {
        sgl_obj_size_zoom(obj, radius_out - obj->radius);
    }
    arc->desc.radius_in = radius_in;
    arc->desc.radius_out = obj->radius = radius_out;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set arc object mode
 * @param obj arc object
 * @param mode arc mode
 * @return none
 */
void sgl_arc_set_mode(sgl_obj_t *obj, uint8_t mode)
{
    sgl_arc_t *arc = sgl_container_of(obj, sgl_arc_t, obj);
    arc->desc.mode = mode;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set arc object start angle
 * @param obj arc object
 * @param angle arc start angle
 * @return none
 * @note angle should be in range [0, 360]
 */
void sgl_arc_set_start_angle(sgl_obj_t *obj, int16_t angle)
{
    sgl_arc_t *arc = sgl_container_of(obj, sgl_arc_t, obj);
    arc->desc.start_angle = angle;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set arc object end angle
 * @param obj arc object
 * @param angle arc end angle
 * @return none
 * @note angle should be in range [0, 360]
 */
void sgl_arc_set_end_angle(sgl_obj_t *obj, int16_t angle)
{
    sgl_arc_t *arc = sgl_container_of(obj, sgl_arc_t, obj);
    arc->desc.end_angle = angle;
    sgl_obj_set_dirty(obj);
}

int16_t sgl_arc_get_included_angle(sgl_obj_t *obj)
{
    sgl_arc_t *arc = sgl_container_of(obj, sgl_arc_t, obj);
    int16_t angle = arc->desc.end_angle - arc->desc.start_angle;

    while (angle < 0) {
        angle += 360;
    }
    while (angle > 360) {
        angle -= 360;
    }

    return angle;
}
