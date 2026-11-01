/* source/widgets/sgl_icon.c
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
#include <sgl_cfgfix.h>
#include <string.h>
#include "sgl_icon.h"


/**
 * @brief icon construct callback
 * @param surf pointer to surface
 * @param obj pointer to object
 * @param evt pointer to event
 * @return none
 */
static void sgl_icon_construct_cb(sgl_surf_t *surf, sgl_obj_t* obj, sgl_event_t *evt)
{
    sgl_icon_t *icon = sgl_container_of(obj, sgl_icon_t, obj);
    sgl_pos_t icon_pos;

    if(evt->type == SGL_EVENT_DRAW_MAIN) {
        icon_pos = sgl_get_icon_pos(&obj->area, icon->icon, 0, (sgl_align_type_t)icon->align);
        sgl_draw_icon(surf, &obj->area, icon_pos.x, icon_pos.y, icon->color, icon->alpha, icon->icon);
    }
}


/**
 * @brief create icon object
 * @param parent pointer to parent object
 * @return pointer to icon object
 */
sgl_obj_t* sgl_icon_create(sgl_obj_t* parent)
{
    sgl_icon_t *icon = sgl_malloc(sizeof(sgl_icon_t));
    if(icon == NULL) {
        SGL_LOG_ERROR("sgl_icon_create: malloc failed");
        return NULL;
    }

    /* set object all member to zero */
    memset(icon, 0, sizeof(sgl_icon_t));

    sgl_obj_t *obj = &icon->obj;
    sgl_obj_init(&icon->obj, parent);
    obj->construct_fn = sgl_icon_construct_cb;

    icon->alpha = SGL_ICON_ALPHA;
    icon->icon = NULL;
    icon->color = SGL_ICON_TEXT_COLOR;
    icon->align = SGL_ALIGN_CENTER;

    return obj;
}


/**
 * @brief set icon color
 * @param obj pointer to icon object
 * @param color icon color
 * @return none
 */
void sgl_icon_set_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_icon_t *icon = sgl_container_of(obj, sgl_icon_t, obj);
    icon->color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set icon alpha
 * @param obj pointer to icon object
 * @param alpha icon alpha
 * @return none
 */
void sgl_icon_set_alpha(sgl_obj_t *obj, uint8_t alpha)
{
    sgl_icon_t *icon = sgl_container_of(obj, sgl_icon_t, obj);
    icon->alpha = alpha;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set icon pixmap
 * @param obj pointer to icon object
 * @param icon pointer to icon pixmap
 * @return none
 */
void sgl_icon_set_icon(sgl_obj_t *obj, const sgl_icon_pixmap_t *icon)
{
    sgl_icon_t *icon_obj = sgl_container_of(obj, sgl_icon_t, obj);
    icon_obj->icon = icon;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set icon align
 * @param obj pointer to icon object
 * @param align icon align
 * @return none
 */
void sgl_icon_set_align(sgl_obj_t *obj, sgl_align_type_t align)
{
    sgl_icon_t *icon_obj = sgl_container_of(obj, sgl_icon_t, obj);
    icon_obj->align = align;
    sgl_obj_set_dirty(obj);
}
