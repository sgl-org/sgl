/* source/widgets/sgl_rectangle.c
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
#include "sgl_rect.h"

/**
 * @brief rectangle construct callback
 * @param  surf: surface
 * @param  obj: object
 * @param  evt: event parameter
 * @retval none
 */
static void sgl_rectangle_construct_cb(sgl_surf_t *surf, sgl_obj_t* obj, sgl_event_t *evt)
{
    sgl_rect_t *rect = sgl_container_of(obj, sgl_rect_t, obj);
    sgl_draw_rect_t desc = {
        .color = rect->color,
        .alpha = rect->alpha,
        .border_alpha = rect->border_alpha,
        .border = obj->border,
        .border_color = rect->border_color,
        .border_mask = obj->focus,
        .pixmap = rect->pixmap,
        .radius = obj->radius,
    };

    if(evt->type == SGL_EVENT_DRAW_MAIN) {
        if (rect->alpha != SGL_ALPHA_MIN) {
            sgl_draw_rect(surf, &obj->area, &obj->coords, &desc);
        }
        else {
            sgl_draw_fill_rect_border(surf, &obj->area, &obj->coords, obj->radius, rect->border_color, obj->border, rect->border_alpha);
        }
    }
    else if(evt->type == SGL_EVENT_PRESSED) {
        if(sgl_obj_is_flexible(obj)) {
            sgl_obj_size_zoom(obj, 2);
        }
        sgl_obj_set_dirty(obj);
    }
    else if(evt->type == SGL_EVENT_RELEASED) {
        if(sgl_obj_is_flexible(obj)) {
            sgl_obj_size_zoom(obj, -2);
        }
        sgl_obj_set_dirty(obj);
    }
}

/**
 * @brief  create a rectangle
 * @param  parent: parent object
 * @retval rectangle object
 */
sgl_obj_t* sgl_rect_create(sgl_obj_t* parent)
{
    sgl_rect_t *rect = sgl_malloc(sizeof(sgl_rect_t));
    if(rect == NULL) {
        SGL_LOG_ERROR("sgl_rect_create: malloc failed");
        return NULL;
    }

    /* set object all member to zero */
    memset(rect, 0, sizeof(sgl_rect_t));

    sgl_obj_t *obj = &rect->obj;
    sgl_obj_init(&rect->obj, parent);
    sgl_obj_set_unflexible(obj);
    sgl_obj_set_border_width(obj, SGL_THEME_BORDER_WIDTH);

    obj->construct_fn = sgl_rectangle_construct_cb;

    rect->alpha = SGL_THEME_ALPHA;
    rect->border_alpha = SGL_THEME_ALPHA;
    rect->color = SGL_THEME_COLOR;
    rect->border_color = SGL_THEME_BORDER_COLOR;
    rect->pixmap = NULL;

    return obj;
}

/**
 * @brief  set rectangle color
 * @param  obj: rectangle object
 * @param  color: rectangle color
 * @retval none
 */
void sgl_rect_set_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_rect_t *rect = sgl_container_of(obj, sgl_rect_t, obj);
    rect->color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief  set rectangle alpha
 * @param  obj: rectangle object
 * @param  alpha: rectangle alpha
 * @retval none
 */
void sgl_rect_set_alpha(sgl_obj_t *obj, uint8_t alpha)
{
    sgl_rect_t *rect = sgl_container_of(obj, sgl_rect_t, obj);
    rect->alpha = alpha;
    rect->border_alpha = alpha;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief  set rectangle main body alpha
 * @param  obj: rectangle object
 * @param  alpha: rectangle main body alpha
 * @retval none
 */
void sgl_rect_set_main_alpha(sgl_obj_t *obj, uint8_t alpha)
{
    sgl_rect_t *rect = sgl_container_of(obj, sgl_rect_t, obj);
    rect->alpha = alpha;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief  set rectangle radius
 * @param  obj: rectangle object
 * @param  radius: rectangle radius
 * @retval none
 */
void sgl_rect_set_radius(sgl_obj_t *obj, uint16_t radius)
{
    sgl_obj_set_radius(obj, radius);
    sgl_obj_set_dirty(obj);
}

/**
 * @brief  set rectangle border width
 * @param  obj: rectangle object
 * @param  width: rectangle border width
 * @retval none
 */
void sgl_rect_set_border_width(sgl_obj_t *obj, uint8_t width)
{
    sgl_obj_set_border_width(obj, width);
}

/**
 * @brief  set rectangle border color
 * @param  obj: rectangle object
 * @param  color: rectangle border color
 * @retval none
 */
void sgl_rect_set_border_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_rect_t *rect = sgl_container_of(obj, sgl_rect_t, obj);
    rect->border_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief  set rectangle border alpha
 * @param  obj: rectangle object
 * @param  alpha: rectangle border alpha
 * @retval none
 */
void sgl_rect_set_border_alpha(sgl_obj_t *obj, uint8_t alpha)
{
    sgl_rect_t *rect = sgl_container_of(obj, sgl_rect_t, obj);
    rect->border_alpha = alpha;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief  set rectangle pixmap
 * @param  obj: rectangle object
 * @param  pixmap: rectangle pixmap
 * @retval none
 */
void sgl_rect_set_pixmap(sgl_obj_t *obj, const sgl_pixmap_t *pixmap)
{
    sgl_rect_t *rect = sgl_container_of(obj, sgl_rect_t, obj);
    rect->pixmap = pixmap;
    sgl_obj_set_dirty(obj);
}
