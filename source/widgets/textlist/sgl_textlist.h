/* source/widgets/sgl_textlist.h
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

#ifndef __SGL_TEXTLIST_H__
#define __SGL_TEXTLIST_H__

#include <sgl_core.h>
#include <sgl_draw.h>
#include <sgl_math.h>
#include <sgl_log.h>
#include <sgl_mm.h>
#include <sgl_cfgfix.h>
#include <sgl_misc.h>
#include <sgl_anim.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * NOTE: can use this callback function to handle the click event of the textlist
 * 
 * void textlist_click_cb(sgl_event_t *event)
 * {
 *     sgl_textlist_t *textlist = (sgl_textlist_t *)event->event_data;
 *     if (event->type == SGL_EVENT_CLICKED) {
 *         SGL_LOG_INFO("Clicked %s", sgl_textlist_get_selected_text(textlist));
 *     }
 * }
 * 
 */

typedef struct sgl_textlist_item {
    char      *text;
    struct sgl_textlist_item *next;
} sgl_textlist_item_t;

/**
 * @brief sgl textlist struct
 * @obj: sgl general object
 */
typedef struct sgl_textlist {
    sgl_obj_t       obj;
    sgl_textlist_item_t *head;
    sgl_textlist_item_t *tail;
    const sgl_font_t  *font;
    const sgl_pixmap_t *pixmap;
    sgl_color_t     item_text_color;
    int16_t         item_selected;
    sgl_color_t     item_selected_color;
    uint16_t        item_num;
    sgl_color_t     bg_color;
    sgl_color_t     border_color;
    uint8_t         alpha;
    sgl_scroll_t    sc;
} sgl_textlist_t;

/**
 * @brief create a textlist object
 * @param parent parent of the textlist
 * @return textlist object
 */
sgl_obj_t* sgl_textlist_create(sgl_obj_t* parent);

/**
 * @brief set the radius of the textlist
 * @param obj led object
 * @param radius radius of the textlist
 * @return none
 */
void sgl_textlist_set_radius(sgl_obj_t *obj, uint8_t radius);

/**
 * @brief set the text color of the textlist
 * @param obj textlist object
 * @param color text color
 * @return none
 */
void sgl_textlist_set_text_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set the selected color of the textlist
 * @param obj textlist object
 * @param color selected color
 * @return none
 */
void sgl_textlist_set_selected_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set the border color of the textlist
 * @param obj textlist object
 * @param color border color
 * @return none
 */
void sgl_textlist_set_border_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set the background color of the textlist
 * @param obj textlist object
 * @param color background color
 * @return none
 */
void sgl_textlist_set_bg_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set the font of the textlist
 * @param obj textlist object
 * @param font font of the textlist
 * @return none
 */
void sgl_textlist_set_text_font(sgl_obj_t *obj, const sgl_font_t *font);

/**
 * @brief set the pixmap of the textlist
 * @param obj textlist object
 * @param pixmap pixmap of the textlist
 * @return none
 */
void sgl_textlist_set_pixmap(sgl_obj_t *obj, const sgl_pixmap_t *pixmap);

/**
 * @brief set the alpha of the textlist
 * @param obj textlist object
 * @param alpha alpha of the textlist
 * @return none
 */
void sgl_textlist_set_alpha(sgl_obj_t *obj, uint8_t alpha);

/**
 * @brief set the border width of the textlist
 * @param obj textlist object
 * @param width border width of the textlist
 * @return none
 */
void sgl_textlist_set_border_width(sgl_obj_t *obj, uint8_t width);

/**
 * @brief add an item to the textlist
 * @param obj textlist object
 * @param text text of the item
 * @return none
 */
void sgl_textlist_add_item(sgl_obj_t *obj, char *text);

/**
 * @brief get the text that selected item of the textlist
 * @param obj textlist object
 * @return selected item text
 */
char* sgl_textlist_get_selected_text(sgl_obj_t *obj);

/**
 * @brief get the index of the selected item of the textlist
 * @param obj textlist object
 * @return selected item index
 */
int16_t sgl_textlist_get_selected_index(sgl_obj_t *obj);

/**
 * @brief delete an item by index of the textlist
 * @param obj textlist object
 * @param index index of the item
 * @return none
 */
void sgl_textlist_delete_item_by_index(sgl_obj_t *obj, int16_t index);

/**
 * @brief delete an item by text of the textlist
 * @param obj textlist object
 * @param text text of the item
 * @return none
 */
void sgl_textlist_delete_item_by_text(sgl_obj_t *obj, char *text);

#ifdef __cplusplus
}
#endif

#endif // !__SGL_TEXTLIST_H__
