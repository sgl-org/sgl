/* source/widgets/sgl_viewlist.c
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
#include "sgl_viewlist.h"

static int32_t sgl_viewlist_max_scroll(sgl_viewlist_t *viewlist)
{
    const int list_h = viewlist->obj.coords.y2 - viewlist->obj.coords.y1 + 1;
    const int content_h = (int)viewlist->item_num * (viewlist->item_height + viewlist->margin_y);
    return sgl_max(0, content_h - list_h);
}

static void sgl_viewlist_layout(sgl_viewlist_t *viewlist)
{
    sgl_obj_t *child = NULL;
    sgl_obj_t *obj = &viewlist->obj;
    int pos_y = -(int16_t)viewlist->sc.offset + viewlist->margin_y + viewlist->obj.border;

    sgl_obj_for_each_child(child, obj) {
        sgl_obj_set_pos_y(child, pos_y);
        pos_y += viewlist->item_height + viewlist->margin_y;
    }
}

static void sgl_viewlist_scroll_commit(sgl_scroll_t *sc)
{
    sgl_viewlist_t *viewlist = sgl_container_of(sc, sgl_viewlist_t, sc);
    sgl_viewlist_layout(viewlist);
    sgl_obj_set_dirty(&viewlist->obj);
}

static void sgl_viewlist_construct_cb(sgl_surf_t *surf, sgl_obj_t* obj, sgl_event_t *evt)
{
    sgl_viewlist_t *viewlist = sgl_container_of(obj, sgl_viewlist_t, obj);
    sgl_draw_rect_t bg_desc = {
        .alpha = viewlist->alpha,
        .color = viewlist->bg_color,
        .border = obj->border,
        .border_alpha = viewlist->alpha,
        .border_color = viewlist->border_color,
        .border_mask = obj->focus,
        .radius = obj->radius,
        .pixmap = viewlist->pixmap,
    };

    switch (evt->type) {
    case SGL_EVENT_DRAW_MAIN:
        sgl_draw_rect(surf, &obj->area, &obj->coords, &bg_desc);
        break;

    case SGL_EVENT_PRESSED:
        sgl_scroll_press(&viewlist->sc, evt->pos.y);
        break;

    case SGL_EVENT_MOVE_UP:
    case SGL_EVENT_MOVE_DOWN: {
        const int32_t max_scroll = sgl_viewlist_max_scroll(viewlist);
        uint8_t r = sgl_scroll_stay(&viewlist->sc,
                                    evt->pos.y, max_scroll);
        if (r) {
            sgl_viewlist_layout(viewlist);
            sgl_obj_set_dirty(obj);
        }
        break;
    }

    case SGL_EVENT_RELEASED: {
        const int32_t max_scroll = sgl_viewlist_max_scroll(viewlist);
        viewlist->sc.range = max_scroll;
        viewlist->sc.commit = sgl_viewlist_scroll_commit;
        if (sgl_scroll_release(&viewlist->sc, max_scroll)) {
            sgl_scroll_anim_start(&viewlist->sc);
        }
        sgl_viewlist_layout(viewlist);
        sgl_obj_set_dirty(obj);
        break;
    }

    case SGL_EVENT_DESTROYED:
        sgl_scroll_anim_stop(&viewlist->sc);
        break;

    default:
        break;
    }
}

/**
 * @brief create a viewlist object
 * @param parent parent of the viewlist
 * @return viewlist object
 */
sgl_obj_t* sgl_viewlist_create(sgl_obj_t* parent)
{
    sgl_viewlist_t *viewlist = sgl_malloc(sizeof(sgl_viewlist_t));
    if(viewlist == NULL) {
        SGL_LOG_ERROR("sgl_viewlist_create: malloc failed");
        return NULL;
    }

    memset(viewlist, 0, sizeof(sgl_viewlist_t));
    sgl_obj_t *obj = &viewlist->obj;
    sgl_obj_init(obj, parent);

    obj->construct_fn = sgl_viewlist_construct_cb;
    sgl_obj_set_border_width(obj, 1);
    sgl_obj_set_clickable(obj);
    sgl_obj_set_movable(obj);

    viewlist->alpha = SGL_THEME_ALPHA;
    viewlist->bg_color = SGL_THEME_COLOR;
    viewlist->border_color = SGL_THEME_BORDER_COLOR;
    viewlist->margin_x = 1;
    viewlist->margin_y = 1;
    viewlist->item_height = 20;
    viewlist->item_num = 0;
    sgl_scroll_reset(&viewlist->sc);

    return obj;
}

/**
 * @brief set the radius of the viewlist
 * @param obj viewlist object
 * @param radius radius of the viewlist
 * @return none
 */
void sgl_viewlist_set_radius(sgl_obj_t *obj, uint8_t radius)
{
    sgl_obj_set_radius(obj, radius);
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the background color of the viewlist
 * @param obj viewlist object
 * @param color background color of the viewlist
 * @return none
 */
void sgl_viewlist_set_bg_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_viewlist_t *viewlist = sgl_container_of(obj, sgl_viewlist_t, obj);
    viewlist->bg_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the alpha of the viewlist
 * @param obj viewlist object
 * @param alpha alpha of the viewlist
 * @return none
 */
void sgl_viewlist_set_alpha(sgl_obj_t *obj, uint8_t alpha)
{
    sgl_viewlist_t *viewlist = sgl_container_of(obj, sgl_viewlist_t, obj);
    viewlist->alpha = alpha;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the border width of the viewlist
 * @param obj viewlist object
 * @param width border width of the viewlist
 * @return none
 */
void sgl_viewlist_set_border_width(sgl_obj_t *obj, uint8_t width)
{
    sgl_obj_set_border_width(obj, width);
}

/**
 * @brief set the border color of the viewlist
 * @param obj viewlist object
 * @param color border color of the viewlist
 * @return none
 */
void sgl_viewlist_set_border_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_viewlist_t *viewlist = sgl_container_of(obj, sgl_viewlist_t, obj);
    viewlist->border_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the pixmap of the viewlist
 * @param obj viewlist object
 * @param pixmap pixmap of the viewlist
 * @return none
 */
void sgl_viewlist_set_pixmap(sgl_obj_t *obj, const sgl_pixmap_t *pixmap)
{
    sgl_viewlist_t *viewlist = sgl_container_of(obj, sgl_viewlist_t, obj);
    viewlist->pixmap = pixmap;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the item height of the viewlist
 * @param obj viewlist object
 * @param height item height of the viewlist
 * @return none
 */
void sgl_viewlist_set_item_height(sgl_obj_t *obj, uint16_t height)
{
    sgl_viewlist_t *viewlist = sgl_container_of(obj, sgl_viewlist_t, obj);
    viewlist->item_height = height;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the item margin of the viewlist
 * @param obj viewlist object
 * @param margin_x item margin x of the viewlist
 * @param margin_y item margin y of the viewlist
 * @return none
 */
void sgl_viewlist_set_item_margin(sgl_obj_t *obj, uint8_t margin_x, uint8_t margin_y)
{
    sgl_viewlist_t *viewlist = sgl_container_of(obj, sgl_viewlist_t, obj);
    viewlist->margin_x = margin_x;
    viewlist->margin_y = margin_y;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief append an object to the viewlist
 * @param list viewlist object
 * @param obj object to be added
 * @return none
 */
void sgl_viewlist_append_obj(sgl_obj_t *list, sgl_obj_t *obj)
{
    SGL_ASSERT(list != NULL && obj != NULL);
    sgl_viewlist_t *viewlist = sgl_container_of(list, sgl_viewlist_t, obj);

    sgl_obj_set_pos(obj, list->border + viewlist->margin_x, list->border + viewlist->margin_y + viewlist->item_num * (viewlist->item_height + viewlist->margin_y));
    sgl_obj_set_width(obj, list->coords.x2 - list->coords.x1 + 1 - list->border * 2 - viewlist->margin_x * 2);
    sgl_obj_set_height(obj, viewlist->item_height);
    viewlist->item_num ++;
}
