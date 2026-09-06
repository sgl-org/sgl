/* source/widgets/sgl_textlist.c
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
#include "sgl_textlist.h"

#define  SGL_TEXTLIST_ITEM_PAD    (3)
#define  SGL_TEXTLIST_ITEM_SPACE  (3)

static int32_t sgl_textlist_max_scroll(sgl_textlist_t *textlist, int item_height)
{
    const int list_h = textlist->obj.coords.y2 - textlist->obj.coords.y1 + 1;
    const int content_h = (int)textlist->item_num * item_height;
    return sgl_max(0, content_h - list_h);
}

static void sgl_textlist_scroll_commit(sgl_scroll_t *sc)
{
    sgl_textlist_t *textlist = sgl_container_of(sc, sgl_textlist_t, sc);
    sgl_scroll_bar_wake(sc);
    sgl_obj_set_dirty(&textlist->obj);
}

static void sgl_textlist_ensure_visible(sgl_textlist_t *textlist, int list_h, int item_height)
{
    const int selected_y = textlist->item_selected * item_height;
    const int view_top = (int)textlist->sc.offset;
    const int view_bottom = view_top + list_h;
    const int32_t max_scroll = sgl_textlist_max_scroll(textlist, item_height);

    if (selected_y < view_top) {
        textlist->sc.offset = selected_y;
    }
    else if (selected_y + item_height > view_bottom) {
        textlist->sc.offset = selected_y + item_height - list_h;
    }

    if (textlist->sc.offset < 0)
        textlist->sc.offset = 0;
    if (textlist->sc.offset > max_scroll)
        textlist->sc.offset = max_scroll;
}

static void sgl_textlist_change_item(sgl_textlist_t *textlist, int16_t item_height, int16_t index)
{
    sgl_obj_t *obj = &textlist->obj;
    sgl_area_t select = textlist->obj.coords;
    select.y1 = obj->coords.y1 + textlist->item_selected * item_height - (int16_t)textlist->sc.offset;
    select.y2 = select.y1 + item_height + 1;
    sgl_obj_update_area(&select);

    textlist->item_selected = index;

    /* stop coasting and scroll the new selection into the viewport */
    const int32_t offset_last = textlist->sc.offset;
    sgl_scroll_anim_stop(&textlist->sc);
    sgl_textlist_ensure_visible(textlist, obj->coords.y2 - obj->coords.y1 + 1, item_height);

    if (textlist->sc.offset != offset_last) {
        /* the whole list shifted, full redraw */
        sgl_scroll_bar_wake(&textlist->sc);
        sgl_obj_set_dirty(obj);
        return;
    }

    select.y1 = obj->coords.y1 + index * item_height - (int16_t)textlist->sc.offset;
    select.y2 = select.y1 + item_height;
    sgl_obj_update_area(&select);
}

static void sgl_textlist_construct_cb(sgl_surf_t *surf, sgl_obj_t* obj, sgl_event_t *evt)
{
    sgl_textlist_t *textlist = sgl_container_of(obj, sgl_textlist_t, obj);
    sgl_draw_rect_t bg_desc = {
        .alpha = textlist->alpha,
        .color = textlist->bg_color,
        .border = obj->border,
        .border_alpha = textlist->alpha,
        .border_color = textlist->border_color,
        .radius = obj->radius,
        .pixmap = textlist->pixmap,
    };
    const int item_height = sgl_font_get_height(textlist->font) + 2 * SGL_TEXTLIST_ITEM_SPACE;
    sgl_area_t in_bg = sgl_obj_get_fill_rect(obj);
    sgl_textlist_item_t *item = textlist->head;

    switch (evt->type) {
    case SGL_EVENT_DRAW_MAIN: {
        const int item_pad = sgl_max(obj->radius, obj->border + SGL_TEXTLIST_ITEM_PAD);
        const int16_t text_pos_x1 = obj->coords.x1 + item_pad;
        const int16_t text_pos_x2 = obj->coords.x2 - item_pad;
        int16_t text_pos_y =  obj->coords.y1 + SGL_TEXTLIST_ITEM_SPACE;
        const int16_t hline_h = item_height - SGL_TEXTLIST_ITEM_SPACE;

        int item_idx = 0;
        sgl_area_t select = {
            .x1 = obj->coords.x1 + obj->border,
            .y1 = obj->coords.y1,
            .x2 = obj->coords.x2 - obj->border,
            .y2 = obj->coords.y1 + item_height
        };

        sgl_draw_rect(surf, &obj->area, &obj->coords, &bg_desc);
        text_pos_y -= (int16_t)textlist->sc.offset;
        while (item != NULL) {
            if (text_pos_y >= obj->area.y2) {
                break;
            }

            if (item_idx == textlist->item_selected) {
                select.y1 = text_pos_y - SGL_TEXTLIST_ITEM_SPACE + 1;
                select.y2 = select.y1 + item_height;

                if ((select.y1 > (obj->area.y1 + obj->radius)) && (select.y2 <= (obj->area.y2 - obj->radius))) {
                    sgl_draw_fill_rect(surf, &obj->area, &select, 0, textlist->item_selected_color, textlist->alpha);
                }
                else if (select.y1 <= (obj->area.y1 + obj->radius)) {
                    sgl_area_t select_coords = select;
                    select_coords.y1 = obj->coords.y1 + obj->border;
                    select_coords.y2 = select.y1 + item_height + obj->radius + 1;
                    sgl_draw_fill_rect(surf, &select, &select_coords, obj->radius, textlist->item_selected_color, textlist->alpha);
                }
                else if (select.y2 >= (obj->area.y2 - obj->radius)) {
                    sgl_area_t select_coords = select;
                    select_coords.y1 = select.y1 - item_height - obj->radius - 1;
                    select_coords.y2 = obj->coords.y2 - obj->border;
                    sgl_draw_fill_rect(surf, &select, &select_coords, obj->radius, textlist->item_selected_color, textlist->alpha);
                }
                else {
                    sgl_draw_fill_rect(surf, &obj->area, &select, 0, textlist->item_selected_color, textlist->alpha);
                }
            }

            sgl_draw_string(surf, &in_bg, text_pos_x1, text_pos_y, item->text, textlist->item_text_color, textlist->alpha, textlist->font);

            text_pos_y += item_height;
            item = item->next;
            item_idx++;
        }

        sgl_scroll_draw_bar(surf, obj, &textlist->sc, sgl_textlist_max_scroll(textlist, item_height), &obj->coords, sgl_color_invert(textlist->bg_color));
    }
    break;

    case SGL_EVENT_PRESSED:
        sgl_scroll_press(&textlist->sc, evt->pos.y);
        sgl_scroll_bar_wake(&textlist->sc);
        break;

    case SGL_EVENT_MOVE_UP:
    case SGL_EVENT_MOVE_DOWN: {
        const int32_t max_scroll = sgl_textlist_max_scroll(textlist, item_height);
        uint8_t r = sgl_scroll_stay(&textlist->sc, evt->pos.y, max_scroll);
        if (r) {
            sgl_obj_set_dirty(obj);
        }
    }
    break;

    case SGL_EVENT_RELEASED: {
        const int32_t max_scroll = sgl_textlist_max_scroll(textlist, item_height);
        textlist->sc.range = max_scroll;
        textlist->sc.commit = sgl_textlist_scroll_commit;
        if (sgl_scroll_release(&textlist->sc, max_scroll)) {
            sgl_scroll_anim_start(&textlist->sc);
        }
        sgl_obj_set_dirty(obj);
    }
    break;

    case SGL_EVENT_CLICKED: {
        int16_t click_y = evt->pos.y;
        int16_t local_y = click_y - obj->coords.y1;

        int clicked_index = (local_y + (int16_t)textlist->sc.offset + SGL_TEXTLIST_ITEM_SPACE) / item_height;
        if (clicked_index >= 0 && clicked_index < textlist->item_num) {
            sgl_textlist_change_item(textlist, item_height, clicked_index);
        }
    }
    break;

    case SGL_EVENT_KEY_DOWN:
        if (textlist->item_selected < (textlist->item_num - 1)) {
            sgl_textlist_change_item(textlist, item_height, textlist->item_selected + 1);
        }
        break;

    case SGL_EVENT_KEY_UP:
        if (textlist->item_selected > 0) {
            sgl_textlist_change_item(textlist, item_height, textlist->item_selected - 1);
        }
    break;

    case SGL_EVENT_DESTROYED:
        sgl_scroll_anim_stop(&textlist->sc);
        while (item != NULL) {
            sgl_textlist_item_t *next = item->next;
            sgl_free(item);
            item = next;
        }
    break;

    default: break;
    }
}

/**
 * @brief create a textlist object
 * @param parent parent of the textlist
 * @return textlist object
 */
sgl_obj_t* sgl_textlist_create(sgl_obj_t* parent)
{
    sgl_textlist_t *textlist = sgl_malloc(sizeof(sgl_textlist_t));
    if(textlist == NULL) {
        SGL_LOG_ERROR("sgl_textlist_create: malloc failed");
        return NULL;
    }

    /* set object all member to zero */
    memset(textlist, 0, sizeof(sgl_textlist_t));

    sgl_obj_t *obj = &textlist->obj;
    sgl_obj_init(&textlist->obj, parent);
    obj->construct_fn = sgl_textlist_construct_cb;
    sgl_obj_set_clickable(obj);
    sgl_obj_set_movable(obj);
    sgl_obj_set_editable(obj);
    sgl_obj_set_border_width(obj, 1);

    textlist->font = sgl_get_system_font();
    textlist->alpha = SGL_THEME_ALPHA;
    textlist->bg_color = SGL_THEME_COLOR;
    textlist->item_selected_color = sgl_color_mixer(SGL_THEME_COLOR, SGL_THEME_BORDER_COLOR, 128);
    textlist->border_color = SGL_THEME_BORDER_COLOR;
    textlist->item_selected = -1;
    textlist->item_text_color = SGL_THEME_TEXT_COLOR;
    sgl_scroll_reset(&textlist->sc);

    return obj;
}

/**
 * @brief set the radius of the textlist
 * @param obj textlist object
 * @param radius radius of the textlist
 * @return none
 */
void sgl_textlist_set_radius(sgl_obj_t *obj, uint8_t radius)
{
    if (obj->radius > 0) {
        sgl_obj_size_zoom(obj, radius - obj->radius);
    }
    obj->radius = radius;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the text color of the textlist
 * @param obj textlist object
 * @param color text color
 * @return none
 */
void sgl_textlist_set_text_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_textlist_t *textlist = (sgl_textlist_t *)obj;
    textlist->item_text_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the selected color of the textlist
 * @param obj textlist object
 * @param color selected color
 * @return none
 */
void sgl_textlist_set_selected_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_textlist_t *textlist = (sgl_textlist_t *)obj;
    textlist->item_selected_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the border color of the textlist
 * @param obj textlist object
 * @param color border color
 * @return none
 */
void sgl_textlist_set_border_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_textlist_t *textlist = (sgl_textlist_t *)obj;
    textlist->border_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the background color of the textlist
 * @param obj textlist object
 * @param color background color
 * @return none
 */
void sgl_textlist_set_bg_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_textlist_t *textlist = (sgl_textlist_t *)obj;
    textlist->bg_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the font of the textlist
 * @param obj textlist object
 * @param font font of the textlist
 * @return none
 */
void sgl_textlist_set_text_font(sgl_obj_t *obj, const sgl_font_t *font)
{
    sgl_textlist_t *textlist = (sgl_textlist_t *)obj;
    textlist->font = font;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the pixmap of the textlist
 * @param obj textlist object
 * @param pixmap pixmap of the textlist
 * @return none
 */
void sgl_textlist_set_pixmap(sgl_obj_t *obj, const sgl_pixmap_t *pixmap)
{
    sgl_textlist_t *textlist = (sgl_textlist_t *)obj;
    textlist->pixmap = pixmap;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the alpha of the textlist
 * @param obj textlist object
 * @param alpha alpha of the textlist
 * @return none
 */
void sgl_textlist_set_alpha(sgl_obj_t *obj, uint8_t alpha)
{
    sgl_textlist_t *textlist = (sgl_textlist_t *)obj;
    textlist->alpha = alpha;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the border width of the textlist
 * @param obj textlist object
 * @param width border width of the textlist
 * @return none
 */
void sgl_textlist_set_border_width(sgl_obj_t *obj, uint8_t width)
{
    sgl_obj_set_border_width(obj, width);
}

/**
 * @brief add an item to the textlist
 * @param obj textlist object
 * @param text text of the item
 * @return none
 */
void sgl_textlist_add_item(sgl_obj_t *obj, char *text)
{
    sgl_textlist_t *textlist = sgl_container_of(obj, sgl_textlist_t, obj);
    sgl_textlist_item_t *item = sgl_malloc(sizeof(sgl_textlist_item_t));
    if(item == NULL) {
        SGL_LOG_ERROR("sgl_textlist_add_item: malloc failed");
        return;
    }

    textlist->item_num++;

    if (textlist->head == NULL) {
        textlist->head = item;
        textlist->tail = item;
        item->text = text;
        return;
    }

    textlist->tail->next = item;
    textlist->tail = item;
    item->next = NULL;
    item->text = text;
}

/**
 * @brief get the text that selected item of the textlist
 * @param obj textlist object
 * @return selected item text
 */
char* sgl_textlist_get_selected_text(sgl_obj_t *obj)
{
    sgl_textlist_t *textlist = (sgl_textlist_t *)obj;
    sgl_textlist_item_t *item = textlist->head;
 
    for (int i = 0; i < textlist->item_num && item != NULL; i++, item = item->next) {
        if (i == textlist->item_selected) {
            return item->text;
        }
    }

    return NULL;
}

/**
 * @brief get the index of the selected item of the textlist
 * @param obj textlist object
 * @return selected item index
 */
int16_t sgl_textlist_get_selected_index(sgl_obj_t *obj)
{
    sgl_textlist_t *textlist = (sgl_textlist_t *)obj;
    return textlist->item_selected;
}

/**
 * @brief delete an item by index of the textlist
 * @param obj textlist object
 * @param index index of the item
 * @return none
 */
void sgl_textlist_delete_item_by_index(sgl_obj_t *obj, int16_t index)
{
    sgl_textlist_t *textlist = (sgl_textlist_t *)obj;
    sgl_textlist_item_t *curr = textlist->head;
    sgl_textlist_item_t *prev = NULL;

    if (obj == NULL || index < 0 || index >= textlist->item_num) {
        return;
    }

    for (int i = 0; i < index; i++) {
        prev = curr;
        curr = curr->next;
    }

    if (prev == NULL) {
        textlist->head = curr->next;
    } else {
        prev->next = curr->next;
    }

    sgl_free(curr);
    textlist->item_num --;
}

/**
 * @brief delete an item by text of the textlist
 * @param obj textlist object
 * @param text text of the item
 * @return none
 */
void sgl_textlist_delete_item_by_text(sgl_obj_t *obj, char *text)
{
    sgl_textlist_t *textlist = (sgl_textlist_t *)obj;
    sgl_textlist_item_t *curr = textlist->head;

    for (int i = 0; i < textlist->item_num && curr != NULL; i++, curr = curr->next) {
        if (strcmp(curr->text, text) == 0) {
            sgl_textlist_delete_item_by_index(obj, i);
            break;
        }
    }
}
