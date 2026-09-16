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
#include <sgl_anim.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file sgl_viewlist.h
 * Virtual list widget (LVGL lv_table style): the list only keeps a small
 * sliding-window cache of item data in RAM, no matter how large the total
 * item count is. Item content is provided by the user through callbacks.
 *
 * For example (music list):
 *
 *   static void music_get_item(sgl_obj_t *list, int32_t index, sgl_viewlist_item_t *item)
 *   {
 *       sgl_snprintf(item->text, sizeof(item->text), "Track %d", (int)index);
 *       sgl_snprintf(item->subtext, sizeof(item->subtext), "03:%02d", (int)(index % 60));
 *   }
 *
 *   static void music_draw_item(sgl_obj_t *list, sgl_surf_t *surf, sgl_area_t *clip,
 *                               sgl_area_t *coords, const sgl_viewlist_item_t *item, bool selected)
 *   {
 *       // custom pretty item drawing: cover, title, artist, duration ...
 *       sgl_draw_string(surf, clip, coords->x1 + 8, coords->y1 + 8, item->text,
 *                       SGL_COLOR_WHITE, 255, &consolas23);
 *   }
 *
 *   static void music_click_item(sgl_obj_t *list, int32_t index, const sgl_viewlist_item_t *item)
 *   {
 *       SGL_LOG_INFO("Play: %s", item->text);
 *   }
 *
 *   void test_viewlist(sgl_obj_t *parent)
 *   {
 *       sgl_obj_t *list = sgl_viewlist_create(parent);
 *       sgl_obj_set_pos(list, 50, 10);
 *       sgl_obj_set_size(list, 240, 300);
 *
 *       sgl_viewlist_set_item_num(list, 1000);        // virtual: only cached items live in RAM
 *       sgl_viewlist_set_item_height(list, 48);
 *       sgl_viewlist_set_item_get_cb(list, music_get_item);
 *       sgl_viewlist_set_item_draw_cb(list, music_draw_item);   // optional: NULL uses the default style
 *       sgl_viewlist_set_item_click_cb(list, music_click_item); // optional
 *   }
 */

/* Cache size = visible_rows * SGL_VIEWLIST_CACHE_MULT.
 * 2 = one screen ahead + one screen buffer (recommended).
 * 1 = tightest memory, more re-fills when scrolling.
 * 3 = fewer re-fills, more RAM. */
#ifndef SGL_VIEWLIST_CACHE_MULT
#define SGL_VIEWLIST_CACHE_MULT    (2)
#endif

/* Absolute floor for cache slots (must be >= visible + a few). */
#ifndef SGL_VIEWLIST_CACHE_MIN
#define SGL_VIEWLIST_CACHE_MIN     (8)
#endif

/* Max length of item text / subtext stored in the cache. */
#ifndef SGL_VIEWLIST_TEXT_MAX_LEN
#define SGL_VIEWLIST_TEXT_MAX_LEN  (64)
#endif

/**
 * @brief one cached list item, filled by the user get callback
 * @text     : primary text (e.g. song title)
 * @subtext  : secondary text (e.g. artist / duration), may be empty
 * @icon     : optional icon glyph string (drawn in the left column), may be NULL
 * @user_data: user context pointer, passed through to the draw/click callbacks
 * @index    : logical item index, set by the widget before the callbacks run
 */
typedef struct sgl_viewlist_item {
    char     text[SGL_VIEWLIST_TEXT_MAX_LEN];
    char     subtext[SGL_VIEWLIST_TEXT_MAX_LEN];
    const char *icon;
    void     *user_data;
    int32_t  index;
} sgl_viewlist_item_t;

/**
 * @brief item data provider: fill @item for the logical item @index.
 *        Called lazily and the result is kept in the sliding-window cache,
 *        so it runs only for items near the visible area.
 */
typedef void (*sgl_viewlist_get_item_cb_t)(sgl_obj_t *list, int32_t index, sgl_viewlist_item_t *item);

/**
 * @brief custom item draw callback. If set, the default item style is replaced
 *        entirely: draw whatever you like inside @coords (clipped by @clip).
 * @param list     viewlist object
 * @param surf     drawing surface
 * @param clip     clip area (item list viewport)
 * @param coords   item rectangle in surface coordinates
 * @param item     cached item data (valid during the callback)
 * @param selected true when the item is the currently selected one
 */
typedef void (*sgl_viewlist_draw_item_cb_t)(sgl_obj_t *list, sgl_surf_t *surf, sgl_area_t *clip,
                                            sgl_area_t *coords, const sgl_viewlist_item_t *item, bool selected);

/**
 * @brief item click callback
 * @param list  viewlist object
 * @param index clicked item index
 * @param item  cached item data (valid during the callback, may be NULL)
 */
typedef void (*sgl_viewlist_click_item_cb_t)(sgl_obj_t *list, int32_t index, const sgl_viewlist_item_t *item);

/**
 * @brief sgl viewlist struct (virtual list)
 */
typedef struct sgl_viewlist {
    sgl_obj_t obj;
    sgl_viewlist_item_t *cache_items;       /**< sliding-window item cache */
    uint16_t cache_capacity;                /**< allocated cache slots */
    int32_t  cache_start_index;             /**< logical index of cache slot 0 (-1 = empty) */
    uint16_t cache_count;                   /**< valid slots in the cache window */
    int32_t  item_num;                      /**< total virtual item count */
    int32_t  item_selected;                 /**< selected item index (-1 = none) */
    uint16_t item_height;
    uint8_t  margin_x;
    uint8_t  margin_y;
    uint8_t  alpha;
    const sgl_font_t   *font;
    const sgl_pixmap_t *pixmap;
    sgl_color_t bg_color;
    sgl_color_t border_color;
    sgl_color_t text_color;
    sgl_color_t subtext_color;
    sgl_color_t selected_color;
    sgl_viewlist_get_item_cb_t   get_cb;
    sgl_viewlist_draw_item_cb_t  draw_cb;
    sgl_viewlist_click_item_cb_t click_cb;
    sgl_scroll_t sc;                        /**< shared scroll physics state (sgl_misc) */
} sgl_viewlist_t;

/**
 * @brief create a viewlist object
 * @param parent parent of the viewlist
 * @return viewlist object
 */
sgl_obj_t* sgl_viewlist_create(sgl_obj_t* parent);

/**
 * @brief set the total (virtual) item count of the viewlist
 * @param obj viewlist object
 * @param num total item count, may be very large: only visible items are cached
 * @return none
 * @note resets the scroll position, selection and the item cache
 */
void sgl_viewlist_set_item_num(sgl_obj_t *obj, int32_t num);

/**
 * @brief get the total item count of the viewlist
 * @param obj viewlist object
 * @return total item count
 */
int32_t sgl_viewlist_get_item_num(sgl_obj_t *obj);

/**
 * @brief set the item data provider callback (mandatory for meaningful content)
 * @param obj viewlist object
 * @param cb get item callback, NULL restores the default "Item %d" text
 * @return none
 */
void sgl_viewlist_set_item_get_cb(sgl_obj_t *obj, sgl_viewlist_get_item_cb_t cb);

/**
 * @brief set a custom item draw callback for a fully user-defined item look
 * @param obj viewlist object
 * @param cb draw item callback, NULL restores the default style
 *        (icon column + primary text + right-aligned subtext)
 * @return none
 */
void sgl_viewlist_set_item_draw_cb(sgl_obj_t *obj, sgl_viewlist_draw_item_cb_t cb);

/**
 * @brief set the item click callback
 * @param obj viewlist object
 * @param cb click item callback, NULL disables click notification
 * @return none
 */
void sgl_viewlist_set_item_click_cb(sgl_obj_t *obj, sgl_viewlist_click_item_cb_t cb);

/**
 * @brief select an item and scroll it into the visible area
 * @param obj viewlist object
 * @param index item index, -1 clears the selection
 * @return none
 */
void sgl_viewlist_set_selected(sgl_obj_t *obj, int32_t index);

/**
 * @brief get the selected item index
 * @param obj viewlist object
 * @return selected item index, -1 when nothing is selected
 */
int32_t sgl_viewlist_get_selected(sgl_obj_t *obj);

/**
 * @brief invalidate the item cache and redraw (call after the data set changed)
 * @param obj viewlist object
 * @return none
 */
void sgl_viewlist_refresh(sgl_obj_t *obj);

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
 * @param margin_y item margin y of the viewlist (vertical gap between items)
 * @return none
 */
void sgl_viewlist_set_item_margin(sgl_obj_t *obj, uint8_t margin_x, uint8_t margin_y);

/**
 * @brief set the text font of the viewlist items
 * @param obj viewlist object
 * @param font font of the items
 * @return none
 */
void sgl_viewlist_set_font(sgl_obj_t *obj, const sgl_font_t *font);

/**
 * @brief set the primary text color of the items
 * @param obj viewlist object
 * @param color text color of the items
 * @return none
 */
void sgl_viewlist_set_text_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set the secondary (subtext) color of the items
 * @param obj viewlist object
 * @param color subtext color of the items
 * @return none
 */
void sgl_viewlist_set_subtext_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set the selected item highlight color
 * @param obj viewlist object
 * @param color selected item background color
 * @return none
 */
void sgl_viewlist_set_selected_color(sgl_obj_t *obj, sgl_color_t color);

#ifdef __cplusplus
}
#endif

#endif // !__SGL_VIEWLIST_H__
