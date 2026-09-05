/* source/widgets/scrollview/sgl_scrollview.h
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

#ifndef __SGL_SCROLLVIEW_H__
#define __SGL_SCROLLVIEW_H__

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
 * @file sgl_scrollview.h
 * A vertical scroll container that can hold any child widgets.
 *
 * The scrollview does not impose any layout on its children: create the child
 * widgets with the scrollview as parent and place them freely with
 * sgl_obj_set_pos(). Dragging inside the view scrolls all children up/down
 * using the shared sgl_scroll engine (inertia + rubber-band + fade-out
 * scrollbar), exactly like the other scrollable widgets.
 *
 * The scrollable range is derived from the children bounds automatically. If
 * you animate child positions or want a fixed scrollable extent, declare it
 * explicitly with sgl_scrollview_set_content_height().
 *
 * For example:
 *
 *   void test_scrollview(sgl_obj_t *parent)
 *   {
 *       sgl_obj_t *sv = sgl_scrollview_create(parent);
 *       sgl_obj_set_pos(sv, 20, 20);
 *       sgl_obj_set_size(sv, 200, 240);
 *       sgl_scrollview_set_bg_color(sv, SGL_COLOR_DARK_GRAY);
 *
 *       for (int i = 0; i < 20; i++) {
 *           sgl_obj_t *btn = sgl_button_create(sv);
 *           sgl_obj_set_pos(btn, 4, 4 + i * 40);
 *           sgl_obj_set_size(btn, 180, 34);
 *           sgl_button_set_text(btn, "Item");
 *       }
 *       sgl_scrollview_set_content_height(sv, 4 + 20 * 40);
 *   }
 */

/**
 * @brief sgl scrollview struct
 * @obj: sgl general object (container, holds the child widgets)
 * @bg: background draw descriptor
 * @sc: shared scroll physics state
 * @content_h: explicit content height in px, 0 = auto compute from children
 * @applied: scroll offset already reflected in the children coordinates
 */
typedef struct sgl_scrollview {
    sgl_obj_t       obj;
    sgl_draw_rect_t bg;
    sgl_scroll_t    sc;
    int32_t         content_h;
    int32_t         applied;
} sgl_scrollview_t;

/**
 * @brief create a scrollview object
 * @param parent parent of the scrollview
 * @return scrollview object
 */
sgl_obj_t* sgl_scrollview_create(sgl_obj_t* parent);

/**
 * @brief set the explicit content (scrollable) height of the scrollview
 * @param obj scrollview object
 * @param height content height in px, 0 restores auto compute from children
 * @return none
 * @note the scrollable range is max(0, content height - viewport height)
 */
void sgl_scrollview_set_content_height(sgl_obj_t *obj, int32_t height);

/**
 * @brief scroll the content to the given offset programmatically
 * @param obj scrollview object
 * @param offset target scroll offset in px (clamped to [0, range])
 * @return none
 */
void sgl_scrollview_scroll_to(sgl_obj_t *obj, int32_t offset);

/**
 * @brief get the current scroll offset of the scrollview
 * @param obj scrollview object
 * @return current scroll offset in px
 */
int32_t sgl_scrollview_get_offset(sgl_obj_t *obj);

/**
 * @brief set the background color of the scrollview
 * @param obj scrollview object
 * @param color background color of the scrollview
 * @return none
 */
void sgl_scrollview_set_bg_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set the radius of the scrollview
 * @param obj scrollview object
 * @param radius radius of the scrollview
 * @return none
 */
void sgl_scrollview_set_radius(sgl_obj_t *obj, uint8_t radius);

/**
 * @brief set the alpha of the scrollview
 * @param obj scrollview object
 * @param alpha alpha of the scrollview
 * @return none
 */
void sgl_scrollview_set_alpha(sgl_obj_t *obj, uint8_t alpha);

/**
 * @brief set the border width of the scrollview
 * @param obj scrollview object
 * @param width border width of the scrollview
 * @return none
 */
void sgl_scrollview_set_border_width(sgl_obj_t *obj, uint8_t width);

/**
 * @brief set the border color of the scrollview
 * @param obj scrollview object
 * @param color border color of the scrollview
 * @return none
 */
void sgl_scrollview_set_border_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set the pixmap of the scrollview
 * @param obj scrollview object
 * @param pixmap pixmap of the scrollview
 * @return none
 */
void sgl_scrollview_set_pixmap(sgl_obj_t *obj, const sgl_pixmap_t *pixmap);

#ifdef __cplusplus
}
#endif

#endif // !__SGL_SCROLLVIEW_H__
