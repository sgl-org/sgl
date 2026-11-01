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
#include <sgl_cfgfix.h>
#include <string.h>
#include "sgl_viewlist.h"

static inline int32_t sgl_viewlist_pitch(const sgl_viewlist_t *viewlist)
{
    return (int32_t)viewlist->item_height + viewlist->margin_y;
}

static inline int32_t sgl_viewlist_list_h(const sgl_viewlist_t *viewlist)
{
    const sgl_obj_t *obj = &viewlist->obj;
    return obj->coords.y2 - obj->coords.y1 + 1 - 2 * obj->border;
}

static int32_t sgl_viewlist_max_scroll(sgl_viewlist_t *viewlist)
{
    const int32_t content_h = viewlist->item_num * sgl_viewlist_pitch(viewlist);
    return sgl_max(0, content_h - sgl_viewlist_list_h(viewlist));
}

static void sgl_viewlist_release_cache(sgl_viewlist_t *viewlist)
{
    if (viewlist->cache_items != NULL) {
        sgl_free(viewlist->cache_items);
        viewlist->cache_items = NULL;
    }
    viewlist->cache_capacity    = 0;
    viewlist->cache_start_index = -1;
    viewlist->cache_count       = 0;
}

static bool sgl_viewlist_ensure_cache(sgl_viewlist_t *viewlist, uint16_t capacity)
{
    if (capacity < SGL_VIEWLIST_CACHE_MIN) {
        capacity = SGL_VIEWLIST_CACHE_MIN;
    }
    if (viewlist->cache_items != NULL && viewlist->cache_capacity == capacity) {
        return true;
    }

    sgl_viewlist_release_cache(viewlist);
    viewlist->cache_items = sgl_malloc(sizeof(sgl_viewlist_item_t) * capacity);
    if (viewlist->cache_items == NULL) {
        SGL_LOG_ERROR("sgl_viewlist: cache alloc failed (%u items)", (unsigned)capacity);
        return false;
    }
    viewlist->cache_capacity    = capacity;
    viewlist->cache_start_index = -1;
    viewlist->cache_count       = 0;
    return true;
}

static inline bool sgl_viewlist_cache_covers(const sgl_viewlist_t *viewlist,
                                             int32_t first, int32_t last /* inclusive */)
{
    if (viewlist->cache_items == NULL || viewlist->cache_count == 0 || viewlist->cache_start_index < 0)
        return false;
    return first >= viewlist->cache_start_index &&
           last  <  viewlist->cache_start_index + (int32_t)viewlist->cache_count;
}

static void sgl_viewlist_fill_window(sgl_viewlist_t *viewlist, int32_t start_index)
{
    if (viewlist->cache_items == NULL || viewlist->cache_capacity == 0) return;
    if (viewlist->item_num <= 0) {
        viewlist->cache_start_index = 0;
        viewlist->cache_count       = 0;
        return;
    }

    if (start_index < 0) start_index = 0;
    if (start_index > viewlist->item_num - 1) start_index = viewlist->item_num - 1;

    uint16_t want = viewlist->cache_capacity;
    if ((int32_t)want > viewlist->item_num - start_index) {
        want = (uint16_t)(viewlist->item_num - start_index);
    }
    if (viewlist->cache_start_index == start_index && viewlist->cache_count == want) {
        return;
    }

    viewlist->cache_start_index = start_index;
    viewlist->cache_count       = want;

    for (uint16_t i = 0; i < want; ++i) {
        sgl_viewlist_item_t *item = &viewlist->cache_items[i];
        item->index       = start_index + i;
        item->text[0]     = '\0';
        item->subtext[0]  = '\0';
        item->icon        = NULL;
        item->user_data   = NULL;

        if (viewlist->get_cb != NULL) {
            viewlist->get_cb(&viewlist->obj, item->index, item);
        } else {
            sgl_snprintf(item->text, sizeof(item->text), "Item %d", (int)item->index);
        }
    }
}

static void sgl_viewlist_ensure_visible(sgl_viewlist_t *viewlist, int32_t first, int32_t last)
{
    if (viewlist->cache_items == NULL || viewlist->cache_capacity == 0 || viewlist->item_num <= 0)
        return;
    if (first < 0) first = 0;
    if (last  > viewlist->item_num - 1) last = viewlist->item_num - 1;
    if (first > last) first = last;

    if (sgl_viewlist_cache_covers(viewlist, first, last)) return;

    const int32_t cap   = (int32_t)viewlist->cache_capacity;
    const int32_t total = viewlist->item_num;
    int32_t new_start;

    if (viewlist->cache_start_index < 0) {
        new_start = first;
    } else if (first >= viewlist->cache_start_index + viewlist->cache_count) {
        new_start = first;
    } else if (last < viewlist->cache_start_index) {
        new_start = last - cap + 1;
    } else {
        int32_t range_mid = (first + last) / 2;
        new_start = range_mid - cap / 2;
    }

    if (new_start < 0)           new_start = 0;
    if (new_start > total - cap) new_start = total - cap;
    if (new_start < 0)           new_start = 0;

    sgl_viewlist_fill_window(viewlist, new_start);
}

static sgl_viewlist_item_t *sgl_viewlist_get_item(sgl_viewlist_t *viewlist, int32_t index)
{
    if (index < 0 || index >= viewlist->item_num) return NULL;
    if (!sgl_viewlist_cache_covers(viewlist, index, index)) {
        sgl_viewlist_ensure_visible(viewlist, index, index);
    }
    if (!sgl_viewlist_cache_covers(viewlist, index, index)) return NULL;
    return &viewlist->cache_items[index - viewlist->cache_start_index];
}

static uint16_t sgl_viewlist_target_capacity(sgl_viewlist_t *viewlist)
{
    const int32_t pitch  = sgl_viewlist_pitch(viewlist);
    const int32_t list_h = sgl_viewlist_list_h(viewlist);
    int32_t visible_rows = (list_h + pitch - 1) / pitch + 1;
    if (visible_rows < 2) visible_rows = 2;
    int32_t cap = visible_rows * SGL_VIEWLIST_CACHE_MULT;
    if (cap < SGL_VIEWLIST_CACHE_MIN) cap = SGL_VIEWLIST_CACHE_MIN;
    if (cap > 0xFFFF)                 cap = 0xFFFF;
    return (uint16_t)cap;
}

static void sgl_viewlist_visible_range(sgl_viewlist_t *viewlist, int32_t *out_first, int32_t *out_last)
{
    const int32_t pitch  = sgl_viewlist_pitch(viewlist);
    const int32_t list_h = sgl_viewlist_list_h(viewlist);

    int32_t first = viewlist->sc.offset > 0 ? viewlist->sc.offset / pitch : 0;
    if (first < 0) first = 0;
    if (first >= viewlist->item_num) first = viewlist->item_num - 1;

    int32_t visible_rows = (list_h + pitch - 1) / pitch + 1;
    int32_t last = first + visible_rows - 1;
    if (last >= viewlist->item_num) last = viewlist->item_num - 1;

    *out_first = first;
    *out_last  = last;
}

/* Scroll the selected item into the list viewport */
static void sgl_viewlist_ensure_selected_visible(sgl_viewlist_t *viewlist)
{
    if (viewlist->item_selected < 0) return;
    const int32_t pitch      = sgl_viewlist_pitch(viewlist);
    const int32_t list_h     = sgl_viewlist_list_h(viewlist);
    const int32_t selected_y = viewlist->item_selected * pitch;
    const int32_t view_top   = viewlist->sc.offset;
    const int32_t max_scroll = sgl_viewlist_max_scroll(viewlist);

    if (selected_y < view_top) {
        viewlist->sc.offset = selected_y;
    }
    else if (selected_y + pitch > view_top + list_h) {
        viewlist->sc.offset = selected_y + pitch - list_h;
    }

    if (viewlist->sc.offset < 0)
        viewlist->sc.offset = 0;
    if (viewlist->sc.offset > max_scroll)
        viewlist->sc.offset = max_scroll;
}

static void sgl_viewlist_scroll_commit(sgl_scroll_t *sc)
{
    sgl_viewlist_t *viewlist = sgl_container_of(sc, sgl_viewlist_t, sc);
    sgl_scroll_bar_wake(sc);
    sgl_obj_set_dirty(&viewlist->obj);
}

static void sgl_viewlist_draw_item_default(sgl_viewlist_t *viewlist, sgl_surf_t *surf,
                                           sgl_area_t *clip, sgl_area_t *coords,
                                           const sgl_viewlist_item_t *item)
{
    const sgl_obj_t *obj = &viewlist->obj;
    const int font_h   = sgl_font_get_height(viewlist->font);
    const int16_t pad  = (int16_t)sgl_max(4, obj->border + 2);
    int16_t text_x     = coords->x1 + pad;

    if (item->icon != NULL) {
        const int16_t icon_y = coords->y1 + (viewlist->item_height - font_h) / 2;
        sgl_draw_string(surf, clip, text_x, icon_y, item->icon,
                        viewlist->subtext_color, viewlist->alpha, viewlist->font);
        text_x += viewlist->item_height; /* square icon column */
    }

    if (item->subtext[0] == '\0') {
        const int16_t text_y = coords->y1 + (viewlist->item_height - font_h) / 2;
        sgl_draw_string(surf, clip, text_x, text_y, item->text,
                        viewlist->text_color, viewlist->alpha, viewlist->font);
    } else {
        const int16_t text_h = font_h * 2 + 2;
        int16_t text_y = coords->y1 + (viewlist->item_height - text_h) / 2;
        if (text_y < coords->y1) text_y = coords->y1;
        sgl_draw_string(surf, clip, text_x, text_y, item->text,
                        viewlist->text_color, viewlist->alpha, viewlist->font);
        sgl_draw_string(surf, clip, text_x, text_y + font_h + 2, item->subtext,
                        viewlist->subtext_color, viewlist->alpha, viewlist->font);
    }
}

static void sgl_viewlist_construct_cb(sgl_surf_t *surf, sgl_obj_t* obj, sgl_event_t *evt)
{
    sgl_viewlist_t *viewlist = sgl_container_of(obj, sgl_viewlist_t, obj);
    const int32_t pitch = sgl_viewlist_pitch(viewlist);

    switch (evt->type) {
    case SGL_EVENT_DRAW_MAIN: {
        sgl_draw_rect_t bg_desc = {
            .alpha        = viewlist->alpha,
            .color        = viewlist->bg_color,
            .border       = obj->border,
            .border_alpha = viewlist->alpha,
            .border_color = viewlist->border_color,
            .radius       = obj->radius,
            .pixmap       = viewlist->pixmap,
        };
        sgl_draw_rect(surf, &obj->area, &obj->coords, &bg_desc);

        const int32_t max_scroll = sgl_viewlist_max_scroll(viewlist);
        sgl_area_t viewport = {
            .x1 = obj->coords.x1,
            .y1 = obj->coords.y1,
            .x2 = obj->coords.x2,
            .y2 = obj->coords.y2,
        };
        const sgl_color_t bar_color = sgl_color_invert(viewlist->bg_color);

        if (viewlist->item_num <= 0) {
            sgl_scroll_draw_bar(surf, obj, &viewlist->sc, max_scroll, &viewport, bar_color);
            break;
        }

        /* ensure the sliding cache window covers what we are about to draw */
        sgl_viewlist_ensure_cache(viewlist, sgl_viewlist_target_capacity(viewlist));

        int32_t first, last;
        sgl_viewlist_visible_range(viewlist, &first, &last);
        sgl_viewlist_ensure_visible(viewlist, first, last);

        if (viewlist->cache_items == NULL || viewlist->cache_count == 0) {
            sgl_scroll_draw_bar(surf, obj, &viewlist->sc, max_scroll, &viewport, bar_color);
            break;
        }

        sgl_area_t list_area = {
            .x1 = obj->area.x1 + obj->border,
            .y1 = obj->area.y1 + obj->border,
            .x2 = obj->area.x2 - obj->border,
            .y2 = obj->area.y2 - obj->border,
        };

        /* draw only the intersection of [first,last] and the cache window */
        int32_t draw_first = sgl_max(first, viewlist->cache_start_index);
        int32_t draw_last  = sgl_min(last, viewlist->cache_start_index + (int32_t)viewlist->cache_count - 1);

        for (int32_t idx = draw_first; idx <= draw_last; ++idx) {
            const int32_t item_y = obj->coords.y1 + obj->border + viewlist->margin_y
                                 + idx * pitch - viewlist->sc.offset;
            if (item_y > obj->coords.y2 - obj->border) break;

            sgl_area_t item_coords = {
                .x1 = obj->coords.x1 + obj->border + viewlist->margin_x,
                .y1 = (int16_t)item_y,
                .x2 = obj->coords.x2 - obj->border - viewlist->margin_x,
                .y2 = (int16_t)(item_y + viewlist->item_height - 1),
            };

            const bool selected = (idx == viewlist->item_selected);
            if (selected) {
                sgl_draw_fill_rect(surf, &list_area, &item_coords, obj->radius,
                                   viewlist->selected_color, viewlist->alpha);
            }

            sgl_viewlist_item_t *item = &viewlist->cache_items[idx - viewlist->cache_start_index];
            if (viewlist->draw_cb != NULL) {
                viewlist->draw_cb(obj, surf, &list_area, &item_coords, item, selected);
            } else {
                sgl_viewlist_draw_item_default(viewlist, surf, &list_area, &item_coords, item);
            }
        }

        sgl_scroll_draw_bar(surf, obj, &viewlist->sc, max_scroll, &viewport, bar_color);
    }
    break;

    case SGL_EVENT_PRESSED:
        sgl_scroll_press(&viewlist->sc, evt->pos.y);
        sgl_scroll_bar_wake(&viewlist->sc);
        break;

    case SGL_EVENT_MOVE_UP:
    case SGL_EVENT_MOVE_DOWN: {
        const int32_t max_scroll = sgl_viewlist_max_scroll(viewlist);
        if (sgl_scroll_stay(&viewlist->sc, evt->pos.y, max_scroll)) {
            sgl_obj_set_dirty(obj);
        }
    }
    break;

    case SGL_EVENT_RELEASED: {
        const int32_t max_scroll = sgl_viewlist_max_scroll(viewlist);
        viewlist->sc.range  = max_scroll;
        viewlist->sc.commit = sgl_viewlist_scroll_commit;
        if (sgl_scroll_release(&viewlist->sc, max_scroll)) {
            sgl_scroll_anim_start(&viewlist->sc);
        }
        sgl_obj_set_dirty(obj);
    }
    break;

    case SGL_EVENT_CLICKED: {
        int32_t clicked_index;
        if (evt->pos.x == SGL_POS_MIN && evt->pos.y == SGL_POS_MIN) {
            clicked_index = viewlist->item_selected;
        } else {
            const int32_t origin_y = obj->coords.y1 + obj->border + viewlist->margin_y;
            if (evt->pos.y < origin_y - viewlist->sc.offset) break;
            clicked_index = (evt->pos.y - origin_y + viewlist->sc.offset) / pitch;
        }
        if (clicked_index < 0 || clicked_index >= viewlist->item_num) break;

        viewlist->item_selected = clicked_index;
        sgl_viewlist_item_t *item = sgl_viewlist_get_item(viewlist, clicked_index);
        if (viewlist->click_cb != NULL) {
            viewlist->click_cb(obj, clicked_index, item);
        }
        sgl_obj_set_dirty(obj);
    }
    break;

    case SGL_EVENT_DESTROYED:
        sgl_scroll_anim_stop(&viewlist->sc);
        sgl_viewlist_release_cache(viewlist);
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
    if (viewlist == NULL) {
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

    viewlist->alpha          = SGL_VIEWLIST_ALPHA;
    viewlist->bg_color       = SGL_VIEWLIST_COLOR;
    viewlist->border_color   = SGL_VIEWLIST_BORDER_COLOR;
    viewlist->text_color     = SGL_VIEWLIST_TEXT_COLOR;
    viewlist->subtext_color  = sgl_color_mixer(SGL_VIEWLIST_TEXT_COLOR, SGL_VIEWLIST_COLOR, 128);
    viewlist->selected_color = sgl_color_mixer(SGL_VIEWLIST_COLOR, SGL_VIEWLIST_BORDER_COLOR, 128);
    viewlist->font           = sgl_get_system_font();
    viewlist->margin_x       = 2;
    viewlist->margin_y       = 2;
    viewlist->item_height    = 40;
    viewlist->item_num       = 0;
    viewlist->item_selected  = -1;
    viewlist->cache_start_index = -1;
    sgl_scroll_reset(&viewlist->sc);

    return obj;
}

/**
 * @brief set the total (virtual) item count of the viewlist
 * @param obj viewlist object
 * @param num total item count, may be very large: only visible items are cached
 * @return none
 */
void sgl_viewlist_set_item_num(sgl_obj_t *obj, int32_t num)
{
    sgl_viewlist_t *viewlist = sgl_container_of(obj, sgl_viewlist_t, obj);
    if (num < 0) num = 0;

    viewlist->item_num          = num;
    viewlist->item_selected     = -1;
    viewlist->cache_start_index = -1;
    viewlist->cache_count       = 0;
    sgl_scroll_anim_stop(&viewlist->sc);
    sgl_scroll_reset(&viewlist->sc);
    sgl_obj_set_dirty(obj);
}

/**
 * @brief get the total item count of the viewlist
 * @param obj viewlist object
 * @return total item count
 */
int32_t sgl_viewlist_get_item_num(sgl_obj_t *obj)
{
    sgl_viewlist_t *viewlist = sgl_container_of(obj, sgl_viewlist_t, obj);
    return viewlist->item_num;
}

/**
 * @brief set the item data provider callback
 * @param obj viewlist object
 * @param cb get item callback, NULL restores the default "Item %d" text
 * @return none
 */
void sgl_viewlist_set_item_get_cb(sgl_obj_t *obj, sgl_viewlist_get_item_cb_t cb)
{
    sgl_viewlist_t *viewlist = sgl_container_of(obj, sgl_viewlist_t, obj);
    viewlist->get_cb = cb;
    viewlist->cache_start_index = -1;
    viewlist->cache_count       = 0;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set a custom item draw callback for a fully user-defined item look
 * @param obj viewlist object
 * @param cb draw item callback, NULL restores the default style
 * @return none
 */
void sgl_viewlist_set_item_draw_cb(sgl_obj_t *obj, sgl_viewlist_draw_item_cb_t cb)
{
    sgl_viewlist_t *viewlist = sgl_container_of(obj, sgl_viewlist_t, obj);
    viewlist->draw_cb = cb;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the item click callback
 * @param obj viewlist object
 * @param cb click item callback, NULL disables click notification
 * @return none
 */
void sgl_viewlist_set_item_click_cb(sgl_obj_t *obj, sgl_viewlist_click_item_cb_t cb)
{
    sgl_viewlist_t *viewlist = sgl_container_of(obj, sgl_viewlist_t, obj);
    viewlist->click_cb = cb;
}

/**
 * @brief select an item and scroll it into the visible area
 * @param obj viewlist object
 * @param index item index, -1 clears the selection
 * @return none
 */
void sgl_viewlist_set_selected(sgl_obj_t *obj, int32_t index)
{
    sgl_viewlist_t *viewlist = sgl_container_of(obj, sgl_viewlist_t, obj);
    if (index < 0 || index >= viewlist->item_num) {
        viewlist->item_selected = -1;
    } else {
        viewlist->item_selected = index;
        sgl_scroll_anim_stop(&viewlist->sc);
        sgl_viewlist_ensure_selected_visible(viewlist);
        sgl_scroll_bar_wake(&viewlist->sc);
    }
    sgl_obj_set_dirty(obj);
}

/**
 * @brief get the selected item index
 * @param obj viewlist object
 * @return selected item index, -1 when nothing is selected
 */
int32_t sgl_viewlist_get_selected(sgl_obj_t *obj)
{
    sgl_viewlist_t *viewlist = sgl_container_of(obj, sgl_viewlist_t, obj);
    return viewlist->item_selected;
}

/**
 * @brief invalidate the item cache and redraw (call after the data set changed)
 * @param obj viewlist object
 * @return none
 */
void sgl_viewlist_refresh(sgl_obj_t *obj)
{
    sgl_viewlist_t *viewlist = sgl_container_of(obj, sgl_viewlist_t, obj);
    viewlist->cache_start_index = -1;
    viewlist->cache_count       = 0;
    sgl_obj_set_dirty(obj);
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
    if (height == 0) height = 1;
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
 * @brief set the text font of the viewlist items
 * @param obj viewlist object
 * @param font font of the items
 * @return none
 */
void sgl_viewlist_set_font(sgl_obj_t *obj, const sgl_font_t *font)
{
    sgl_viewlist_t *viewlist = sgl_container_of(obj, sgl_viewlist_t, obj);
    viewlist->font = font;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the primary text color of the items
 * @param obj viewlist object
 * @param color text color of the items
 * @return none
 */
void sgl_viewlist_set_text_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_viewlist_t *viewlist = sgl_container_of(obj, sgl_viewlist_t, obj);
    viewlist->text_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the secondary (subtext) color of the items
 * @param obj viewlist object
 * @param color subtext color of the items
 * @return none
 */
void sgl_viewlist_set_subtext_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_viewlist_t *viewlist = sgl_container_of(obj, sgl_viewlist_t, obj);
    viewlist->subtext_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the selected item highlight color
 * @param obj viewlist object
 * @param color selected item background color
 * @return none
 */
void sgl_viewlist_set_selected_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_viewlist_t *viewlist = sgl_container_of(obj, sgl_viewlist_t, obj);
    viewlist->selected_color = color;
    sgl_obj_set_dirty(obj);
}
