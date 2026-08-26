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
#include "sgl_rect_ext.h"


/**
 * @brief rectangle construct callback
 * @param  surf: surface
 * @param  obj: object
 * @param  evt: event parameter
 * @retval none
 */
static void sgl_rectangle_ext_construct_cb(sgl_surf_t *surf, sgl_obj_t* obj, sgl_event_t *evt)
{
    sgl_rect_ext_t *rect = sgl_container_of(obj, sgl_rect_ext_t, obj);

    if(evt->type == SGL_EVENT_DRAW_MAIN) {
        sgl_area_t rect_tmp = {
            .x1 = obj->coords.x1 + obj->border,
            .x2 = obj->coords.x2 - obj->border,
            .y1 = obj->coords.y1 + obj->border,
            .y2 = obj->coords.y2 - obj->border
        };

        const int16_t left = rect->tl_radius - obj->border;
        const int16_t right = rect->tr_radius - obj->border;
        const int16_t bottom = rect->bl_radius - obj->border;
        const int16_t top = rect->br_radius - obj->border;

        if (rect->pixmap == NULL) {
            sgl_draw_fill_rich_rect(surf, &obj->area, &rect_tmp, 
                                        left, 
                                        right,
                                        bottom,
                                        top,
                                        rect->color, rect->alpha
                                        );
        }
        else {
            sgl_draw_fill_rect_pixmap_rich(surf, &obj->area, &rect_tmp,
                                            left, 
                                            right,
                                            bottom,
                                            top,
                                            rect->pixmap, rect->alpha
                                        );
        }

        if (obj->border) {
                sgl_draw_fill_rect_border_rich(surf, &obj->area, &obj->coords, 
                                                rect->tl_radius, 
                                                rect->tr_radius,
                                                rect->bl_radius,
                                                rect->br_radius,
                                                rect->border_color, obj->border, rect->border_alpha
                                                );
        }
    }
}

/**
 * @brief  create a rectangle
 * @param  parent: parent object
 * @retval rectangle object
 */
sgl_obj_t* sgl_rect_ext_create(sgl_obj_t* parent)
{
    sgl_rect_ext_t *rect_ext = sgl_malloc(sizeof(sgl_rect_ext_t));
    if(rect_ext == NULL) {
        SGL_LOG_ERROR("sgl_rect_ext_create: malloc failed");
        return NULL;
    }

    /* set object all member to zero */
    memset(rect_ext, 0, sizeof(sgl_rect_ext_t));

    sgl_obj_t *obj = &rect_ext->obj;
    sgl_obj_init(&rect_ext->obj, parent);
    sgl_obj_set_unflexible(obj);
    sgl_obj_set_border_width(obj, SGL_THEME_BORDER_WIDTH);

    obj->construct_fn = sgl_rectangle_ext_construct_cb;

    rect_ext->alpha = SGL_THEME_ALPHA;
    rect_ext->border_alpha = SGL_THEME_ALPHA;
    rect_ext->color = SGL_THEME_COLOR;
    rect_ext->border_color = SGL_THEME_BORDER_COLOR;
    rect_ext->pixmap = NULL;

    return obj;
}

/**
 * @brief  set rectangle color
 * @param  obj: rectangle object
 * @param  color: rectangle color
 * @retval none
 */
void sgl_rect_ext_set_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_rect_ext_t *rect = sgl_container_of(obj, sgl_rect_ext_t, obj);
    rect->color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief  set rectangle alpha
 * @param  obj: rectangle object
 * @param  alpha: rectangle alpha
 * @retval none
 */
void sgl_rect_ext_set_alpha(sgl_obj_t *obj, uint8_t alpha)
{
    sgl_rect_ext_t *rect = sgl_container_of(obj, sgl_rect_ext_t, obj);
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
void sgl_rect_ext_set_main_alpha(sgl_obj_t *obj, uint8_t alpha)
{
    sgl_rect_ext_t *rect = sgl_container_of(obj, sgl_rect_ext_t, obj);
    rect->alpha = alpha;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief  set rectangle radius
 * @param  obj: rectangle object
 * @param  radius: rectangle radius
 * @retval none
 */
void sgl_rect_ext_set_radius(sgl_obj_t *obj, uint8_t top_left, uint8_t top_right, uint8_t bottom_left, uint8_t bottom_right)
{
    int16_t radius = sgl_max4(top_left, top_right, bottom_left, bottom_right);
    sgl_rect_ext_t *rect = sgl_container_of(obj, sgl_rect_ext_t, obj);
    sgl_obj_set_radius(obj, radius);
    rect->tl_radius = top_left;
    rect->tr_radius = top_right;
    rect->bl_radius = bottom_left;
    rect->br_radius = bottom_right;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief  set rectangle border width
 * @param  obj: rectangle object
 * @param  width: rectangle border width
 * @retval none
 */
void sgl_rect_ext_set_border_width(sgl_obj_t *obj, uint8_t width)
{
    sgl_obj_set_border_width(obj, width);
}

/**
 * @brief  set rectangle border color
 * @param  obj: rectangle object
 * @param  color: rectangle border color
 * @retval none
 */
void sgl_rect_ext_set_border_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_rect_ext_t *rect = sgl_container_of(obj, sgl_rect_ext_t, obj);
    rect->border_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief  set rectangle border alpha
 * @param  obj: rectangle object
 * @param  alpha: rectangle border alpha
 * @retval none
 */
void sgl_rect_ext_set_border_alpha(sgl_obj_t *obj, uint8_t alpha)
{
    sgl_rect_ext_t *rect = sgl_container_of(obj, sgl_rect_ext_t, obj);
    rect->border_alpha = alpha;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief  set rectangle pixmap
 * @param  obj: rectangle object
 * @param  pixmap: rectangle pixmap
 * @retval none
 */
void sgl_rect_ext_set_pixmap(sgl_obj_t *obj, const sgl_pixmap_t *pixmap)
{
    sgl_rect_ext_t *rect = sgl_container_of(obj, sgl_rect_ext_t, obj);
    rect->pixmap = pixmap;
    sgl_obj_set_dirty(obj);
}
