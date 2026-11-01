/* source/widgets/tabview/sgl_tabview.h
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

#ifndef __SGL_TABVIEW_H__
#define __SGL_TABVIEW_H__

#include <sgl_core.h>
#include <sgl_draw.h>
#include <sgl_math.h>
#include <sgl_log.h>
#include <sgl_mm.h>
#include <sgl_cfgfix.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define  SGL_TABVIEW_COLOR            sgl_rgb(255, 255, 255)
#define  SGL_TABVIEW_BORDER_COLOR     sgl_rgb(0, 0, 0)
#define  SGL_TABVIEW_BORDER_WIDTH     2
#define  SGL_TABVIEW_ALPHA            255
#define  SGL_TABVIEW_TEXT_COLOR       sgl_rgb(0, 0, 0)
#define  SGL_TABVIEW_RADIUS           0

/**
 * @file sgl_tabview.h
 * A tab container widget: a tab bar on top plus one content page per tab.
 *
 * The tabview draws the tab bar itself (hit-tested against the press
 * position). Each tab owns a page object (a plain container) that is created
 * by sgl_tabview_add_tab(); add the tab content widgets to that page. Only
 * the active page is visible, pages are switched by tapping the tab buttons
 * or programmatically with sgl_tabview_set_active().
 *
 * Set the tabview position and size BEFORE adding tabs: the pages are laid
 * out to fill the content area below the tab bar when they are created (they
 * are also re-laid out on the first draw and when the bar height changes).
 *
 * For example:
 *
 *   void test_tabview(sgl_obj_t *parent)
 *   {
 *       sgl_obj_t *tv = sgl_tabview_create(parent);
 *       sgl_obj_set_pos(tv, 20, 20);
 *       sgl_obj_set_size(tv, 300, 200);
 *
 *       sgl_obj_t *page = sgl_tabview_add_tab(tv, "First");
 *       sgl_obj_t *btn = sgl_button_create(page);
 *       sgl_obj_set_pos(btn, 10, 10);
 *       sgl_obj_set_size(btn, 100, 40);
 *       sgl_button_set_text(btn, "OK");
 *
 *       sgl_tabview_add_tab(tv, "Second");
 *   }
 */

/**
 * @brief maximum number of tabs in a tabview, override it in sgl_config.h
 */
#ifndef SGL_TABVIEW_TAB_MAX
#define SGL_TABVIEW_TAB_MAX   8
#endif

/**
 * @brief sgl tabview struct
 * @obj: sgl general object
 * @bg: whole tabview background draw descriptor (border + content area)
 * @bar: tab bar strip draw descriptor
 * @font: tab text font
 * @texts: tab title texts
 * @pages: page objects, one per tab
 * @tab_color: inactive tab button color
 * @tab_active_color: active tab button color
 * @text_color: inactive tab text color
 * @text_active_color: active tab text (and indicator) color
 * @bar_height: tab bar height in px, 0 = auto (font height + 12)
 * @tab_count: number of added tabs
 * @active: active tab index
 * @pressed: pressed tab index for press feedback, -1 = none
 */
typedef struct sgl_tabview {
    sgl_obj_t        obj;
    sgl_draw_rect_t  bg;
    sgl_draw_rect_t  bar;
    const sgl_font_t *font;
    const char       *texts[SGL_TABVIEW_TAB_MAX];
    sgl_obj_t        *pages[SGL_TABVIEW_TAB_MAX];
    sgl_color_t      tab_color;
    sgl_color_t      tab_active_color;
    sgl_color_t      text_color;
    sgl_color_t      text_active_color;
    uint8_t          bar_height;
    int16_t          tab_count;
    int16_t          active;
    int16_t          pressed;
} sgl_tabview_t;

/**
 * @brief create a tabview object
 * @param parent parent of the tabview
 * @return tabview object
 */
sgl_obj_t* sgl_tabview_create(sgl_obj_t *parent);

/**
 * @brief add a tab to the tabview
 * @param obj tabview object
 * @param text tab title text (the string is not copied)
 * @return page object of the new tab, add the tab content widgets to it,
 *         NULL when the tab count reaches SGL_TABVIEW_TAB_MAX
 */
sgl_obj_t* sgl_tabview_add_tab(sgl_obj_t *obj, const char *text);

/**
 * @brief set the active tab
 * @param obj tabview object
 * @param index tab index, from 0 to tab count - 1
 * @return none
 */
void sgl_tabview_set_active(sgl_obj_t *obj, int16_t index);

/**
 * @brief get the active tab index
 * @param obj tabview object
 * @return active tab index
 */
int16_t sgl_tabview_get_active(sgl_obj_t *obj);

/**
 * @brief get the tab count of the tabview
 * @param obj tabview object
 * @return tab count
 */
int16_t sgl_tabview_get_tab_count(sgl_obj_t *obj);

/**
 * @brief get the page object of a tab
 * @param obj tabview object
 * @param index tab index, from 0 to tab count - 1
 * @return page object, NULL when the index is out of range
 */
sgl_obj_t* sgl_tabview_get_page(sgl_obj_t *obj, int16_t index);

/**
 * @brief set the tab bar height of the tabview
 * @param obj tabview object
 * @param height tab bar height in px, 0 restores auto (font height + 12)
 * @return none
 */
void sgl_tabview_set_bar_height(sgl_obj_t *obj, uint8_t height);

/**
 * @brief set the tab font of the tabview
 * @param obj tabview object
 * @param font tab text font
 * @return none
 */
void sgl_tabview_set_font(sgl_obj_t *obj, const sgl_font_t *font);

/**
 * @brief set the background color of the tabview content area
 * @param obj tabview object
 * @param color background color
 * @return none
 */
void sgl_tabview_set_bg_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set the tab bar color of the tabview
 * @param obj tabview object
 * @param color tab bar color
 * @return none
 */
void sgl_tabview_set_bar_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set the inactive tab button color
 * @param obj tabview object
 * @param color inactive tab button color
 * @return none
 */
void sgl_tabview_set_tab_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set the active tab button color
 * @param obj tabview object
 * @param color active tab button color
 * @return none
 */
void sgl_tabview_set_tab_active_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set the inactive tab text color
 * @param obj tabview object
 * @param color inactive tab text color
 * @return none
 */
void sgl_tabview_set_text_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set the active tab text (and indicator) color
 * @param obj tabview object
 * @param color active tab text color
 * @return none
 */
void sgl_tabview_set_text_active_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set the alpha of the tabview
 * @param obj tabview object
 * @param alpha alpha of the tabview
 * @return none
 */
void sgl_tabview_set_alpha(sgl_obj_t *obj, uint8_t alpha);

/**
 * @brief set the border width of the tabview
 * @param obj tabview object
 * @param width border width
 * @return none
 */
void sgl_tabview_set_border_width(sgl_obj_t *obj, uint8_t width);

/**
 * @brief set the border color of the tabview
 * @param obj tabview object
 * @param color border color
 * @return none
 */
void sgl_tabview_set_border_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set the radius of the tabview
 * @param obj tabview object
 * @param radius radius of the tabview
 * @return none
 */
void sgl_tabview_set_radius(sgl_obj_t *obj, uint8_t radius);

/**
 * @brief set the pixmap of the tabview content area
 * @param obj tabview object
 * @param pixmap pixmap of the tabview content area
 * @return none
 */
void sgl_tabview_set_pixmap(sgl_obj_t *obj, const sgl_pixmap_t *pixmap);

/**
 * @brief set the background color of a tabview page
 * @param page page object returned by sgl_tabview_add_tab()
 * @param color page background color
 * @return none
 */
void sgl_tabview_page_set_bg_color(sgl_obj_t *page, sgl_color_t color);

/**
 * @brief set the alpha of a tabview page
 * @param page page object returned by sgl_tabview_add_tab()
 * @param alpha page alpha
 * @return none
 */
void sgl_tabview_page_set_alpha(sgl_obj_t *page, uint8_t alpha);

/**
 * @brief set the pixmap of a tabview page
 * @param page page object returned by sgl_tabview_add_tab()
 * @param pixmap page background pixmap
 * @return none
 */
void sgl_tabview_page_set_pixmap(sgl_obj_t *page, const sgl_pixmap_t *pixmap);

#ifdef __cplusplus
}
#endif

#endif // !__SGL_TABVIEW_H__
