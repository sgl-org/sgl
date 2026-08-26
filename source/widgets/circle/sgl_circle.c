/* source/widgets/sgl_circle.c
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
#include "sgl_circle.h"

/**
 * @brief construct function of the circle object
 * @param surf pointer to the surface
 * @param obj pointer to the object
 * @param evt pointer to the event
 * @return none
 */
static void sgl_circle_construct_cb(sgl_surf_t *surf, sgl_obj_t* obj, sgl_event_t *evt)
{
    sgl_circle_t *circle = sgl_container_of(obj, sgl_circle_t, obj);

    if(evt->type == SGL_EVENT_DRAW_MAIN) {
        const int16_t cx = (obj->coords.x1 + obj->coords.x2) / 2 + circle->x_ofs;
        const int16_t cy = (obj->coords.y1 + obj->coords.y2) / 2 + circle->y_ofs;
        const int16_t radius = sgl_obj_get_width(obj) / 2;

        sgl_draw_circle_t desc = {
            .alpha = circle->alpha,
            .color = circle->color,
            .pixmap = circle->pixmap,
            .border = obj->border,
            .border_color = circle->border_color,
            .radius = radius,
            .cx = cx,
            .cy = cy,
        };

        sgl_draw_circle(surf, &obj->area, &desc);
    }
}

/**
 * @brief create a circle object
 * @param parent parent of the object
 * @return pointer to the object
 */
sgl_obj_t* sgl_circle_create(sgl_obj_t* parent)
{
    sgl_circle_t *circle = sgl_malloc(sizeof(sgl_circle_t));
    if(circle == NULL) {
        SGL_LOG_ERROR("sgl_circle_create: malloc failed");
        return NULL;
    }

    /* set object all member to zero */
    memset(circle, 0, sizeof(sgl_circle_t));

    sgl_obj_t *obj = &circle->obj;
    sgl_obj_init(&circle->obj, parent);
    obj->construct_fn = sgl_circle_construct_cb;
    sgl_obj_set_border_width(obj, SGL_THEME_BORDER_WIDTH);

    circle->alpha = SGL_ALPHA_MAX;
    circle->color = SGL_THEME_COLOR;
    circle->pixmap = NULL;
    obj->border = SGL_THEME_BORDER_WIDTH;
    circle->border_color = SGL_THEME_BORDER_COLOR;

    return obj;
}

/**
 * @brief set the color of the circle
 * @param obj pointer to the object
 * @param color color of the circle
 * @return none
 */
void sgl_circle_set_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_circle_t *circle = sgl_container_of(obj, sgl_circle_t, obj);
    circle->color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the radius of the circle
 * @param obj pointer to the object
 * @param radius radius of the circle
 * @return none
 */
void sgl_circle_set_radius(sgl_obj_t *obj, uint16_t radius)
{
    sgl_obj_circle_zoom(obj, radius);
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the alpha of the circle
 * @param obj pointer to the object
 * @param alpha alpha of the circle
 * @return none
 */
void sgl_circle_set_alpha(sgl_obj_t *obj, uint8_t alpha)
{
    sgl_circle_t *circle = sgl_container_of(obj, sgl_circle_t, obj);
    circle->alpha = alpha;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the pixmap of the circle
 * @param obj pointer to the object
 * @param pixmap pixmap of the circle
 * @return none
 */
void sgl_circle_set_pixmap(sgl_obj_t *obj, const sgl_pixmap_t *pixmap)
{
    sgl_circle_t *circle = sgl_container_of(obj, sgl_circle_t, obj);
    circle->pixmap = pixmap;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the border color of the circle
 * @param obj pointer to the object
 * @param color border color of the circle
 * @return none
 */
void sgl_circle_set_border_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_circle_t *circle = sgl_container_of(obj, sgl_circle_t, obj);
    circle->border_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the border width of the circle
 * @param obj pointer to the object
 * @param width border width of the circle
 * @return none
 */
void sgl_circle_set_border_width(sgl_obj_t *obj, uint8_t width)
{
    sgl_obj_set_border_width(obj, width);
}

/**
 * @brief set the x offset of the center of circle
 * @param obj pointer to the object
 * @param offset x offset of the circle
 * @return none
 */
void sgl_circle_set_x_offset(sgl_obj_t *obj, int8_t offset)
{
    sgl_circle_t *circle = sgl_container_of(obj, sgl_circle_t, obj);
    circle->x_ofs = offset;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the y offset of the center of circle
 * @param obj pointer to the object
 * @param offset y offset of the circle
 * @return none
 */
void sgl_circle_set_y_offset(sgl_obj_t *obj, int8_t offset)
{
    sgl_circle_t *circle = sgl_container_of(obj, sgl_circle_t, obj);
    circle->y_ofs = offset;
    sgl_obj_set_dirty(obj);
}
