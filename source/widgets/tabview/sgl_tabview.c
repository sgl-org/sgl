/* source/widgets/tabview/sgl_tabview.c
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

#include "sgl_tabview.h"

/**
 * @brief get the effective tab bar height
 * @param tv tabview object
 * @return tab bar height in px
 */
static uint8_t sgl_tabview_get_bar_height(sgl_tabview_t *tv)
{
    return tv->bar_height ? tv->bar_height : (uint8_t)(sgl_font_get_height(tv->font) + 12);
}

/**
 * @brief get the tab bar strip area (inside the tabview border)
 * @param tv tabview object
 * @return tab bar strip area
 */
static sgl_area_t sgl_tabview_bar_coords(sgl_tabview_t *tv)
{
    sgl_obj_t *obj = &tv->obj;
    sgl_area_t area = {
        .x1 = obj->coords.x1 + obj->border,
        .x2 = obj->coords.x2 - obj->border,
        .y1 = obj->coords.y1 + obj->border,
        .y2 = obj->coords.y1 + obj->border + sgl_tabview_get_bar_height(tv) - 1,
    };
    return area;
}

/**
 * @brief get the button area of one tab inside the tab bar
 * @param tv tabview object
 * @param index tab index
 * @return tab button area
 */
static sgl_area_t sgl_tabview_tab_coords(sgl_tabview_t *tv, int16_t index)
{
    sgl_area_t area = sgl_tabview_bar_coords(tv);
    int16_t width = (int16_t)((area.x2 - area.x1 + 1) / tv->tab_count);

    area.x1 = (int16_t)(area.x1 + width * index);
    area.x2 = (index == tv->tab_count - 1) ? sgl_tabview_bar_coords(tv).x2
                                           : (int16_t)(area.x1 + width - 1);
    return area;
}

/**
 * @brief hit test a position against the tab buttons
 * @param tv tabview object
 * @param x position x
 * @param y position y
 * @return tab index, -1 when the position is outside the tab bar
 */
static int16_t sgl_tabview_hit_tab(sgl_tabview_t *tv, int16_t x, int16_t y)
{
    if (tv->tab_count <= 0) {
        return -1;
    }

    sgl_area_t bar = sgl_tabview_bar_coords(tv);
    if (x < bar.x1 || x > bar.x2 || y < bar.y1 || y > bar.y2) {
        return -1;
    }

    int16_t index = (int16_t)((x - bar.x1) / ((bar.x2 - bar.x1 + 1) / tv->tab_count));
    return sgl_min(index, tv->tab_count - 1);
}

/**
 * @brief lay out all pages to fill the content area below the tab bar
 * @param tv tabview object
 */
static void sgl_tabview_layout(sgl_tabview_t *tv)
{
    sgl_obj_t *obj = &tv->obj;
    int16_t body_x = obj->border;
    int16_t body_y = (int16_t)(obj->border + sgl_tabview_get_bar_height(tv));
    int16_t body_w = (int16_t)(obj->coords.x2 - obj->coords.x1 + 1 - 2 * obj->border);
    int16_t body_h = (int16_t)(obj->coords.y2 - obj->coords.y1 + 1 - body_y - obj->border);

    if (body_w <= 0 || body_h <= 0) {
        return;
    }

    for (int16_t i = 0; i < tv->tab_count; i++) {
        sgl_obj_set_pos(tv->pages[i], body_x, body_y);
        sgl_obj_set_size(tv->pages[i], body_w, body_h);
    }
}

/**
 * @brief shared construct callback of all tab pages: pages are plain
 *        containers and draw nothing, the tabview draws the common
 *        background of the content area
 */
static void sgl_tabview_page_construct_cb(sgl_surf_t *surf, sgl_obj_t *obj, sgl_event_t *evt)
{
    SGL_UNUSED(surf);
    SGL_UNUSED(obj);
    SGL_UNUSED(evt);
}

static void sgl_tabview_construct_cb(sgl_surf_t *surf, sgl_obj_t *obj, sgl_event_t *evt)
{
    sgl_tabview_t *tv = sgl_container_of(obj, sgl_tabview_t, obj);

    switch (evt->type) {
    case SGL_EVENT_DRAW_INIT:
        sgl_tabview_layout(tv);
        break;

    case SGL_EVENT_DRAW_MAIN: {
        tv->bg.border_mask = obj->focus;
        sgl_draw_rect(surf, &obj->area, &obj->coords, &tv->bg);

        if (tv->tab_count <= 0) {
            break;
        }

        sgl_area_t bar = sgl_tabview_bar_coords(tv);

        for (int16_t i = 0; i < tv->tab_count; i++) {
            sgl_area_t cell = sgl_tabview_tab_coords(tv, i);
            sgl_color_t color = (i == tv->active) ? tv->tab_active_color : tv->tab_color;
            sgl_color_t text_color = (i == tv->active) ? tv->text_active_color : tv->text_color;

            if (i == tv->pressed) {
                color = sgl_color_mixer(tv->text_color, color, 48);
            }

            /* separator between tab buttons */
            if (i > 0) {
                sgl_draw_fill_vline(surf, &obj->area, cell.x1, cell.y1, cell.y2, 1,
                                    tv->bar.border_color, tv->bar.alpha);
            }

            /* indicator under the active tab button */
            if (i == tv->active) {
                sgl_draw_fill_hline(surf, &obj->area, cell.y2 - 2, cell.x1, cell.x2, 3,
                                    tv->text_active_color, tv->bar.alpha);
            }

            sgl_pos_t pos = sgl_get_text_pos(&cell, tv->font, tv->texts[i], 0, SGL_ALIGN_CENTER);
            sgl_draw_string(surf, &obj->area, pos.x, pos.y, tv->texts[i], text_color,
                            tv->bar.alpha, tv->font);
        }

        /* baseline between the tab bar and the content area */
        sgl_draw_fill_hline(surf, &obj->area, bar.y2, bar.x1, bar.x2, 1,
                            tv->bar.border_color, tv->bar.alpha);
        break;
    }

    case SGL_EVENT_PRESSED: {
        int16_t index = sgl_tabview_hit_tab(tv, evt->pos.x, evt->pos.y);
        if (index < 0) {
            sgl_obj_clear_dirty(obj);
            return;
        }

        tv->pressed = index;
        sgl_area_t cell = sgl_tabview_tab_coords(tv, index);
        sgl_obj_update_area(&cell);
        break;
    }

    case SGL_EVENT_RELEASED: {
        if (tv->pressed < 0) {
            sgl_obj_clear_dirty(obj);
            return;
        }

        int16_t index = sgl_tabview_hit_tab(tv, evt->pos.x, evt->pos.y);
        if (index >= 0 && index == tv->pressed) {
            sgl_tabview_set_active(obj, index);
        }

        tv->pressed = -1;
        sgl_obj_set_dirty(obj);
        break;
    }

    default:
        break;
    }
}

/**
 * @brief create a tabview object
 * @param parent parent of the tabview
 * @return tabview object
 */
sgl_obj_t* sgl_tabview_create(sgl_obj_t *parent)
{
    sgl_tabview_t *tv = sgl_malloc(sizeof(sgl_tabview_t));
    if (tv == NULL) {
        SGL_LOG_ERROR("sgl_tabview_create: malloc failed");
        return NULL;
    }

    memset(tv, 0, sizeof(sgl_tabview_t));

    sgl_obj_t *obj = &tv->obj;
    sgl_obj_init(obj, parent);
    obj->construct_fn = sgl_tabview_construct_cb;
    sgl_obj_set_border_width(obj, SGL_TABVIEW_BORDER_WIDTH);
    sgl_obj_set_radius(obj, SGL_TABVIEW_RADIUS);
    sgl_obj_set_clickable(obj);

    tv->bg.alpha = SGL_TABVIEW_ALPHA;
    tv->bg.color = SGL_TABVIEW_COLOR;
    tv->bg.border = SGL_TABVIEW_BORDER_WIDTH;
    tv->bg.border_alpha = SGL_TABVIEW_ALPHA;
    tv->bg.border_color = SGL_TABVIEW_BORDER_COLOR;
    tv->bg.radius = SGL_TABVIEW_RADIUS;
    tv->bg.pixmap = NULL;

    tv->bar.alpha = SGL_TABVIEW_ALPHA;
    tv->bar.color = sgl_color_mixer(SGL_TABVIEW_TEXT_COLOR, SGL_TABVIEW_COLOR, 24);
    tv->bar.border_color = SGL_TABVIEW_BORDER_COLOR;
    tv->bar.pixmap = NULL;

    tv->font = sgl_get_system_font();
    tv->tab_color = tv->bar.color;
    tv->tab_active_color = SGL_TABVIEW_COLOR;
    tv->text_color = SGL_TABVIEW_TEXT_COLOR;
    tv->text_active_color = SGL_TABVIEW_TEXT_COLOR;

    tv->bar_height = 0;
    tv->tab_count = 0;
    tv->active = 0;
    tv->pressed = -1;

    return obj;
}

/**
 * @brief add a tab to the tabview
 * @param obj tabview object
 * @param text tab title text (the string is not copied)
 * @return page object of the new tab, NULL when the tab count reaches
 *         SGL_TABVIEW_TAB_MAX
 */
sgl_obj_t* sgl_tabview_add_tab(sgl_obj_t *obj, const char *text)
{
    sgl_tabview_t *tv = sgl_container_of(obj, sgl_tabview_t, obj);

    if (tv->tab_count >= SGL_TABVIEW_TAB_MAX) {
        SGL_LOG_ERROR("sgl_tabview_add_tab: tab count reaches SGL_TABVIEW_TAB_MAX");
        return NULL;
    }

    sgl_obj_t *page = sgl_malloc(sizeof(sgl_obj_t));
    if (page == NULL) {
        SGL_LOG_ERROR("sgl_tabview_add_tab: malloc failed");
        return NULL;
    }

    memset(page, 0, sizeof(sgl_obj_t));

    sgl_obj_init(page, obj);
    page->construct_fn = sgl_tabview_page_construct_cb;
    sgl_obj_set_border_width(page, 0);

    tv->texts[tv->tab_count] = text;
    tv->pages[tv->tab_count] = page;
    tv->tab_count++;

    sgl_tabview_layout(tv);

    if (tv->tab_count - 1 != tv->active) {
        sgl_obj_set_hidden(page);
    }

    sgl_obj_set_dirty(obj);
    return page;
}

/**
 * @brief set the active tab
 * @param obj tabview object
 * @param index tab index, from 0 to tab count - 1
 * @return none
 */
void sgl_tabview_set_active(sgl_obj_t *obj, int16_t index)
{
    sgl_tabview_t *tv = sgl_container_of(obj, sgl_tabview_t, obj);

    if (index < 0 || index >= tv->tab_count || index == tv->active) {
        return;
    }

    sgl_obj_set_hidden(tv->pages[tv->active]);
    sgl_obj_set_visible(tv->pages[index]);
    tv->active = index;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief get the active tab index
 * @param obj tabview object
 * @return active tab index
 */
int16_t sgl_tabview_get_active(sgl_obj_t *obj)
{
    sgl_tabview_t *tv = sgl_container_of(obj, sgl_tabview_t, obj);
    return tv->active;
}

/**
 * @brief get the tab count of the tabview
 * @param obj tabview object
 * @return tab count
 */
int16_t sgl_tabview_get_tab_count(sgl_obj_t *obj)
{
    sgl_tabview_t *tv = sgl_container_of(obj, sgl_tabview_t, obj);
    return tv->tab_count;
}

/**
 * @brief get the page object of a tab
 * @param obj tabview object
 * @param index tab index, from 0 to tab count - 1
 * @return page object, NULL when the index is out of range
 */
sgl_obj_t* sgl_tabview_get_page(sgl_obj_t *obj, int16_t index)
{
    sgl_tabview_t *tv = sgl_container_of(obj, sgl_tabview_t, obj);
    return (index >= 0 && index < tv->tab_count) ? tv->pages[index] : NULL;
}

/**
 * @brief set the tab bar height of the tabview
 * @param obj tabview object
 * @param height tab bar height in px, 0 restores auto (font height + 12)
 * @return none
 */
void sgl_tabview_set_bar_height(sgl_obj_t *obj, uint8_t height)
{
    sgl_tabview_t *tv = sgl_container_of(obj, sgl_tabview_t, obj);
    tv->bar_height = height;
    sgl_tabview_layout(tv);
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the tab font of the tabview
 * @param obj tabview object
 * @param font tab text font
 * @return none
 */
void sgl_tabview_set_font(sgl_obj_t *obj, const sgl_font_t *font)
{
    sgl_tabview_t *tv = sgl_container_of(obj, sgl_tabview_t, obj);
    tv->font = font;
    sgl_tabview_layout(tv);
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the background color of the tabview content area
 * @param obj tabview object
 * @param color background color
 * @return none
 */
void sgl_tabview_set_bg_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_tabview_t *tv = sgl_container_of(obj, sgl_tabview_t, obj);
    tv->bg.color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the tab bar color of the tabview
 * @param obj tabview object
 * @param color tab bar color
 * @return none
 */
void sgl_tabview_set_bar_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_tabview_t *tv = sgl_container_of(obj, sgl_tabview_t, obj);
    tv->bar.color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the inactive tab button color
 * @param obj tabview object
 * @param color inactive tab button color
 * @return none
 */
void sgl_tabview_set_tab_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_tabview_t *tv = sgl_container_of(obj, sgl_tabview_t, obj);
    tv->tab_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the active tab button color
 * @param obj tabview object
 * @param color active tab button color
 * @return none
 */
void sgl_tabview_set_tab_active_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_tabview_t *tv = sgl_container_of(obj, sgl_tabview_t, obj);
    tv->tab_active_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the inactive tab text color
 * @param obj tabview object
 * @param color inactive tab text color
 * @return none
 */
void sgl_tabview_set_text_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_tabview_t *tv = sgl_container_of(obj, sgl_tabview_t, obj);
    tv->text_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the active tab text (and indicator) color
 * @param obj tabview object
 * @param color active tab text color
 * @return none
 */
void sgl_tabview_set_text_active_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_tabview_t *tv = sgl_container_of(obj, sgl_tabview_t, obj);
    tv->text_active_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the alpha of the tabview
 * @param obj tabview object
 * @param alpha alpha of the tabview
 * @return none
 */
void sgl_tabview_set_alpha(sgl_obj_t *obj, uint8_t alpha)
{
    sgl_tabview_t *tv = sgl_container_of(obj, sgl_tabview_t, obj);
    tv->bg.alpha = alpha;
    tv->bg.border_alpha = alpha;
    tv->bar.alpha = alpha;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the border width of the tabview
 * @param obj tabview object
 * @param width border width
 * @return none
 */
void sgl_tabview_set_border_width(sgl_obj_t *obj, uint8_t width)
{
    sgl_tabview_t *tv = sgl_container_of(obj, sgl_tabview_t, obj);
    tv->bg.border = width;
    sgl_obj_set_border_width(obj, width);
    sgl_tabview_layout(tv);
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the border color of the tabview
 * @param obj tabview object
 * @param color border color
 * @return none
 */
void sgl_tabview_set_border_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_tabview_t *tv = sgl_container_of(obj, sgl_tabview_t, obj);
    tv->bg.border_color = color;
    tv->bar.border_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the radius of the tabview
 * @param obj tabview object
 * @param radius radius of the tabview
 * @return none
 */
void sgl_tabview_set_radius(sgl_obj_t *obj, uint8_t radius)
{
    sgl_tabview_t *tv = sgl_container_of(obj, sgl_tabview_t, obj);
    sgl_obj_set_radius(obj, radius);
    tv->bg.radius = obj->radius;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the pixmap of the tabview content area
 * @param obj tabview object
 * @param pixmap pixmap of the tabview content area
 * @return none
 */
void sgl_tabview_set_pixmap(sgl_obj_t *obj, const sgl_pixmap_t *pixmap)
{
    sgl_tabview_t *tv = sgl_container_of(obj, sgl_tabview_t, obj);
    tv->bg.pixmap = pixmap;
    sgl_obj_set_dirty(obj);
}
