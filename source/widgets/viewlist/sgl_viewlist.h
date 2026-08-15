/* source/widgets/sgl_viewlist.h
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

#ifndef __SGL_VIEWLIST_H__
#define __SGL_VIEWLIST_H__

#include <sgl_core.h>
#include <sgl_draw.h>
#include <sgl_math.h>
#include <sgl_log.h>
#include <sgl_mm.h>
#include <sgl_cfgfix.h>
#include <sgl_misc.h>
#include <sgl_anim.h>
#include <string.h>

/**
 * @file sgl_viewlist.h
 * You can use this file to create a viewlist object
 * For example:
 * 
 *   void viewlist_click_cb(sgl_event_t *event)
 *   {
 *       sgl_obj_t *label = (sgl_obj_t *)event->event_data;
 *       if (event->type == SGL_EVENT_CLICKED) {
 *           SGL_LOG_INFO("Clicked: %s", sgl_label_get_text(label));
 *       }
 *   }
 *
 *   void test_viewlist(sgl_obj_t *parent)
 *   {
 *       sgl_obj_t *list = sgl_viewlist_create(parent);
 *       sgl_obj_set_size(list, 200, 300);
 *       sgl_obj_set_pos(list, 50, 10);
 *
 *       for (int i = 0; i < 20; i++) {
 *           sgl_obj_t *label = sgl_label_create(list);
 *           sgl_label_set_radius(label, 5);
 *           sgl_label_set_bg_color(label, SGL_COLOR_WHEAT);
 *           sgl_label_set_font(label, &consolas24);
 *           sgl_label_set_text_fmt(label, "List Item %d", i);
 *           sgl_obj_set_event_cb(label, viewlist_click_cb, label);
 *
 *           sgl_viewlist_set_item_height(list, 50);
 *           sgl_viewlist_append_obj(list, label);
 *       }
 *   }
 */

/**
 * @brief sgl viewlist struct
 * @obj: sgl general object
 */
typedef struct sgl_viewlist {
    sgl_obj_t obj;
    const sgl_pixmap_t *pixmap;
    sgl_color_t bg_color;
    sgl_color_t border_color;
    uint16_t item_height;
    uint16_t item_num;
    uint8_t margin_x;
    uint8_t margin_y;
    uint8_t alpha;
    sgl_scroll_t  sc;
} sgl_viewlist_t;

/**
 * @brief create a viewlist object
 * @param parent parent of the viewlist
 * @return viewlist object
 */
sgl_obj_t* sgl_viewlist_create(sgl_obj_t* parent);

/**
 * @brief set the radius of the viewlist
 * @param obj viewlist object
 * @param radius radius of the viewlist
 * @return none
 */
void sgl_viewlist_set_radius(sgl_obj_t *obj, uint8_t radius);

/**
 * @brief set the background color of the viewlist
 * @param obj viewlist object
 * @param color background color of the viewlist
 * @return none
 */
void sgl_viewlist_set_bg_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set the alpha of the viewlist
 * @param obj viewlist object
 * @param alpha alpha of the viewlist
 * @return none
 */
void sgl_viewlist_set_alpha(sgl_obj_t *obj, uint8_t alpha);

/**
 * @brief set the border width of the viewlist
 * @param obj viewlist object
 * @param width border width of the viewlist
 * @return none
 */
void sgl_viewlist_set_border_width(sgl_obj_t *obj, uint8_t width);

/**
 * @brief set the border color of the viewlist
 * @param obj viewlist object
 * @param color border color of the viewlist
 * @return none
 */
void sgl_viewlist_set_border_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set the pixmap of the viewlist
 * @param obj viewlist object
 * @param pixmap pixmap of the viewlist
 * @return none
 */
void sgl_viewlist_set_pixmap(sgl_obj_t *obj, const sgl_pixmap_t *pixmap);

/**
 * @brief set the item height of the viewlist
 * @param obj viewlist object
 * @param height item height of the viewlist
 * @return none
 */
void sgl_viewlist_set_item_height(sgl_obj_t *obj, uint16_t height);

/**
 * @brief set the item margin of the viewlist
 * @param obj viewlist object
 * @param margin_x item margin x of the viewlist
 * @param margin_y item margin y of the viewlist
 * @return none
 */
void sgl_viewlist_set_item_margin(sgl_obj_t *obj, uint8_t margin_x, uint8_t margin_y);

/**
 * @brief append an object to the viewlist
 * @param list viewlist object
 * @param obj object to be added
 * @return none
 */
void sgl_viewlist_append_obj(sgl_obj_t *list, sgl_obj_t *obj);

#endif // !__SGL_VIEWLIST_H__
