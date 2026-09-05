/* source/widgets/scrollview/sgl_scrollview.c
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
#include "sgl_scrollview.h"

/**
 * @brief compute the scrollable range (content height - viewport height)
 * @param sv scrollview object
 * @return scroll upper limit, >= 0
 * @note when no explicit content height is set, the content bottom is derived
 *       from the children bounds. Children are shifted by the applied offset,
 *       so their natural (content) coordinate is coords.y2 + applied, which
 *       keeps the range stable while scrolling.
 */
static int32_t sgl_scrollview_range(sgl_scrollview_t *sv)
{
    sgl_obj_t *obj = &sv->obj;
    int32_t viewport_h = (int32_t)obj->coords.y2 - (int32_t)obj->coords.y1 + 1;
    int32_t content_h = sv->content_h;

    if (content_h <= 0) {
        sgl_obj_t *child = NULL;
        int32_t bottom = (int32_t)obj->coords.y1;

        sgl_obj_for_each_child(child, obj) {
            int32_t cb = (int32_t)child->coords.y2 + sv->applied;
            if (cb > bottom) {
                bottom = cb;
            }
        }
        content_h = bottom - (int32_t)obj->coords.y1 + 1;
    }

    return sgl_max(0, content_h - viewport_h);
}

/**
 * @brief reflect the current scroll offset onto the children coordinates
 * @param sv scrollview object
 * @note moves the whole child subtree by the incremental offset delta, so the
 *       children keep their relative layout while the content scrolls. Growing
 *       offset scrolls the content up, hence the negated delta.
 */
static void sgl_scrollview_apply(sgl_scrollview_t *sv)
{
    int32_t delta = sv->sc.offset - sv->applied;

    if (delta == 0) {
        return;
    }

    sgl_obj_move_child_pos_y(&sv->obj, (int16_t)(-delta));
    sv->applied = sv->sc.offset;
    sgl_scroll_bar_wake(&sv->sc);
}

/**
 * @brief scroll change commit callback (bound to sgl_scroll_t.commit)
 * @param sc scroll state owned by the scrollview
 * @note invoked by the shared inertia/rebound animation on every offset change
 */
static void sgl_scrollview_scroll_commit(sgl_scroll_t *sc)
{
    sgl_scrollview_t *sv = sgl_container_of(sc, sgl_scrollview_t, sc);

    sgl_scrollview_apply(sv);
    sgl_obj_set_dirty(&sv->obj);
}

static void sgl_scrollview_construct_cb(sgl_surf_t *surf, sgl_obj_t *obj, sgl_event_t *evt)
{
    sgl_scrollview_t *sv = sgl_container_of(obj, sgl_scrollview_t, obj);

    switch (evt->type) {
    case SGL_EVENT_DRAW_MAIN: {
        const int32_t range = sgl_scrollview_range(sv);
        sv->bg.border_mask = obj->focus;
        sgl_draw_rect(surf, &obj->area, &obj->coords, &sv->bg);
        sgl_scroll_draw_bar(surf, obj, &sv->sc, range, &obj->coords, sgl_color_invert(sv->bg.color));
        break;
    }

    case SGL_EVENT_PRESSED:
        sgl_scroll_press(&sv->sc, evt->pos.y);
        break;

    case SGL_EVENT_MOVE_UP:
    case SGL_EVENT_MOVE_DOWN: {
        const int32_t range = sgl_scrollview_range(sv);
        /* a press that lands on a child widget never reaches the scrollview as
         * PRESSED (only the motion bubbles up), so anchor the shared scroll
         * state on the first bubbled move to start tracking this drag */
        if (!sv->sc.touching) {
            sgl_scroll_press(&sv->sc, evt->pos.y);
        }
        if (sgl_scroll_stay(&sv->sc, evt->pos.y, range)) {
            sgl_scrollview_apply(sv);
            sgl_obj_set_dirty(obj);
        }
        break;
    }

    case SGL_EVENT_RELEASED: {
        const int32_t range = sgl_scrollview_range(sv);
        sv->sc.range = range;
        sv->sc.commit = sgl_scrollview_scroll_commit;
        if (sgl_scroll_release(&sv->sc, range)) {
            sgl_scroll_anim_start(&sv->sc);
        }
        sgl_scrollview_apply(sv);
        sgl_obj_set_dirty(obj);
        break;
    }

    case SGL_EVENT_DESTROYED:
        sgl_scroll_anim_stop(&sv->sc);
        break;

    default:
        break;
    }
}

/**
 * @brief create a scrollview object
 * @param parent parent of the scrollview
 * @return scrollview object
 */
sgl_obj_t* sgl_scrollview_create(sgl_obj_t* parent)
{
    sgl_scrollview_t *sv = sgl_malloc(sizeof(sgl_scrollview_t));
    if (sv == NULL) {
        SGL_LOG_ERROR("sgl_scrollview_create: malloc failed");
        return NULL;
    }

    memset(sv, 0, sizeof(sgl_scrollview_t));

    sgl_obj_t *obj = &sv->obj;
    sgl_obj_init(obj, parent);
    obj->construct_fn = sgl_scrollview_construct_cb;
    sgl_obj_set_border_width(obj, 1);
    sgl_obj_set_clickable(obj);
    sgl_obj_set_movable(obj);

    sv->bg.alpha = SGL_THEME_ALPHA;
    sv->bg.color = SGL_THEME_COLOR;
    sv->bg.border = 1;
    sv->bg.border_alpha = SGL_THEME_ALPHA;
    sv->bg.border_color = SGL_THEME_BORDER_COLOR;
    sv->bg.radius = 0;
    sv->bg.pixmap = NULL;
    sv->content_h = 0;
    sv->applied = 0;

    sgl_scroll_reset(&sv->sc);
    sv->sc.commit = sgl_scrollview_scroll_commit;
    sv->sc.bar_alpha = 128;

    return obj;
}

/**
 * @brief set the explicit content (scrollable) height of the scrollview
 * @param obj scrollview object
 * @param height content height in px, 0 restores auto compute from children
 * @return none
 */
void sgl_scrollview_set_content_height(sgl_obj_t *obj, int32_t height)
{
    sgl_scrollview_t *sv = sgl_container_of(obj, sgl_scrollview_t, obj);
    sv->content_h = (height > 0) ? height : 0;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief scroll the content to the given offset programmatically
 * @param obj scrollview object
 * @param offset target scroll offset in px (clamped to [0, range])
 * @return none
 */
void sgl_scrollview_scroll_to(sgl_obj_t *obj, int32_t offset)
{
    sgl_scrollview_t *sv = sgl_container_of(obj, sgl_scrollview_t, obj);
    const int32_t range = sgl_scrollview_range(sv);

    sv->sc.offset = sgl_clamp(offset, 0, range);
    sgl_scrollview_apply(sv);
    sgl_obj_set_dirty(obj);
}

/**
 * @brief get the current scroll offset of the scrollview
 * @param obj scrollview object
 * @return current scroll offset in px
 */
int32_t sgl_scrollview_get_offset(sgl_obj_t *obj)
{
    sgl_scrollview_t *sv = sgl_container_of(obj, sgl_scrollview_t, obj);
    return sv->sc.offset;
}

/**
 * @brief set the background color of the scrollview
 * @param obj scrollview object
 * @param color background color of the scrollview
 * @return none
 */
void sgl_scrollview_set_bg_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_scrollview_t *sv = sgl_container_of(obj, sgl_scrollview_t, obj);
    sv->bg.color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the radius of the scrollview
 * @param obj scrollview object
 * @param radius radius of the scrollview
 * @return none
 */
void sgl_scrollview_set_radius(sgl_obj_t *obj, uint8_t radius)
{
    sgl_scrollview_t *sv = sgl_container_of(obj, sgl_scrollview_t, obj);
    sgl_obj_set_radius(obj, radius);
    sv->bg.radius = obj->radius;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the alpha of the scrollview
 * @param obj scrollview object
 * @param alpha alpha of the scrollview
 * @return none
 */
void sgl_scrollview_set_alpha(sgl_obj_t *obj, uint8_t alpha)
{
    sgl_scrollview_t *sv = sgl_container_of(obj, sgl_scrollview_t, obj);
    sv->bg.alpha = alpha;
    sv->bg.border_alpha = alpha;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the border width of the scrollview
 * @param obj scrollview object
 * @param width border width of the scrollview
 * @return none
 */
void sgl_scrollview_set_border_width(sgl_obj_t *obj, uint8_t width)
{
    sgl_scrollview_t *sv = sgl_container_of(obj, sgl_scrollview_t, obj);
    sv->bg.border = width;
    sgl_obj_set_border_width(obj, width);
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the border color of the scrollview
 * @param obj scrollview object
 * @param color border color of the scrollview
 * @return none
 */
void sgl_scrollview_set_border_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_scrollview_t *sv = sgl_container_of(obj, sgl_scrollview_t, obj);
    sv->bg.border_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the pixmap of the scrollview
 * @param obj scrollview object
 * @param pixmap pixmap of the scrollview
 * @return none
 */
void sgl_scrollview_set_pixmap(sgl_obj_t *obj, const sgl_pixmap_t *pixmap)
{
    sgl_scrollview_t *sv = sgl_container_of(obj, sgl_scrollview_t, obj);
    sv->bg.pixmap = pixmap;
    sgl_obj_set_dirty(obj);
}
