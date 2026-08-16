/* source/widgets/sgl_dropdown.c
 *
 * MIT License
 *
 * Copyright(c) 2023-present All contributors of SGL  
 * Document reference link: https://sgl-docs.readthedocs.io
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
#include <sgl_theme.h>
#include <sgl_misc.h>
#include "sgl_dropdown.h"

#define  SGL_DROPDOWN_OPTION_PAD    (3)
#define  SGL_DROPDOWN_OPTION_SPACE  (3)

static int32_t sgl_dropdown_max_scroll(sgl_dropdown_t *dropdown, int item_height)
{
    const int list_h = item_height * sgl_min((int)dropdown->item_num, (int)dropdown->max_visible_item);
    const int content_h = (int)dropdown->item_num * item_height;
    return sgl_max(0, content_h - list_h);
}

static void sgl_dropdown_scroll_commit(sgl_scroll_t *sc)
{
    sgl_dropdown_t *dropdown = sgl_container_of(sc, sgl_dropdown_t, sc);
    sgl_scroll_bar_wake(sc);
    sgl_obj_set_dirty(&dropdown->obj);
}

static void update_item_count(sgl_dropdown_t *dropdown)
{
    dropdown->item_num = sgl_string_option_get_count(dropdown->opt_text);
    if (dropdown->item_selected >= 0 && dropdown->opt_text) {
        dropdown->text_offset = (uint16_t)sgl_string_option_get_offset(dropdown->opt_text, dropdown->item_selected);
    }
}

static void free_dynamic_text(sgl_dropdown_t *dropdown)
{
    if (dropdown->dynamic_text && dropdown->opt_text) {
        sgl_free(dropdown->opt_text);
        dropdown->opt_text = NULL;
        dropdown->dynamic_text = 0;
    }
}

static void remove_item_at(sgl_dropdown_t *dropdown, int offset)
{
    int total_len = (int)strlen(dropdown->opt_text);
    int item_len = sgl_string_option_get_text_len(dropdown->opt_text, offset);
    int entry_len = item_len + (dropdown->opt_text[offset + item_len] == '\n' ? 1 : 0);
    int remaining = total_len - (offset + entry_len);
    if (remaining > 0) {
        memmove(dropdown->opt_text + offset, dropdown->opt_text + offset + entry_len, remaining);
    }
    dropdown->opt_text[offset + remaining] = '\0';
}

static void adjust_selection_after_delete(sgl_dropdown_t *dropdown, int deleted_idx)
{
    if (deleted_idx < dropdown->item_selected) {
        dropdown->item_selected--;
    } else if (deleted_idx == dropdown->item_selected) {
        if (dropdown->item_selected >= dropdown->item_num - 1) {
            dropdown->item_selected = dropdown->item_num > 1 ? dropdown->item_num - 2 : -1;
        }
    }
}

static void sgl_dropdown_clamp_pos(sgl_dropdown_t *dropdown, int list_h, int item_height)
{
    const int max_scroll = sgl_max(0, dropdown->item_num * item_height - list_h);

    if (dropdown->sc.offset < 0) {
        dropdown->sc.offset = 0;
    }
    else if (dropdown->sc.offset > max_scroll) {
        dropdown->sc.offset = max_scroll;
    }
}

static void sgl_dropdown_ensure_visible(sgl_dropdown_t *dropdown, int list_h, int item_height)
{
    const int selected_y = dropdown->item_selected * item_height;
    const int view_top = (int)dropdown->sc.offset;
    const int view_bottom = view_top + list_h;

    if (selected_y < view_top) {
        dropdown->sc.offset = selected_y;
    }
    else if (selected_y + item_height > view_bottom) {
        dropdown->sc.offset = selected_y + item_height - list_h;
    }
    sgl_dropdown_clamp_pos(dropdown, list_h, item_height);
}

static void sgl_dropdown_construct_cb(sgl_surf_t *surf, sgl_obj_t *obj, sgl_event_t *evt)
{
    sgl_dropdown_t *dropdown = sgl_container_of(obj, sgl_dropdown_t, obj);
    sgl_draw_rect_t bg_desc = {
        .alpha = dropdown->alpha,
        .color = dropdown->bg_color,
        .border = obj->border,
        .border_alpha = dropdown->alpha,
        .border_color = dropdown->border_color,
        .border_mask = obj->focus,
        .radius = obj->radius,
        .pixmap = NULL,
    };

    sgl_area_t bg_coords = obj->coords;
    const int item_height = sgl_font_get_height(dropdown->font) + 2 * SGL_DROPDOWN_OPTION_SPACE;
    const int item_pad = sgl_max(obj->radius, obj->border + SGL_DROPDOWN_OPTION_PAD);

    if (dropdown->option_h == 0) {
        dropdown->option_h = sgl_obj_get_height(obj);
    }
    const int visible_items = sgl_min((int)dropdown->item_num, (int)dropdown->max_visible_item);
    const int list_h = item_height * visible_items;

    switch (evt->type) {
    case SGL_EVENT_DRAW_MAIN: {
        char text_buf[128];
        bg_coords.y2 = bg_coords.y1 + dropdown->option_h - 1;
        sgl_draw_rect(surf, &obj->area, &bg_coords, &bg_desc);
        const int text_pos_y = (dropdown->option_h - sgl_font_get_height(dropdown->font) + 1) / 2;

        /* Draw the dropdown arrow icon: downward chevron sized to the header height */
        {
            const int icon_h = dropdown->option_h / 3;
            const int icon_w = 2 * (icon_h - 1); /* half-width == height: 45-degree sides, 90-degree apex */
            const int icon_y_off = dropdown->is_open ? 2 : 0;
            const int16_t ix = obj->coords.x2 - icon_w - obj->radius - obj->border;
            const int16_t iy = obj->coords.y1 + (dropdown->option_h - icon_h + 1) / 2 + icon_y_off;
            const uint8_t lw = (uint8_t)sgl_max(2, icon_h / 8);
            sgl_draw_line_noaa(surf, &obj->area, ix, iy,
                               ix + icon_w / 2, iy + icon_h - 1,
                               dropdown->text_color, lw, dropdown->alpha);
            sgl_draw_line_noaa(surf, &obj->area, ix + icon_w / 2, iy + icon_h - 1,
                               ix + icon_w, iy,
                               dropdown->text_color, lw, dropdown->alpha);
        }

        /* Draw the selected item text on the closed dropdown header */
        if (dropdown->opt_text && dropdown->item_selected >= 0 && dropdown->item_selected < dropdown->item_num) {
            const char *sel_text = dropdown->opt_text + dropdown->text_offset;
            int len = sgl_string_option_get_text_len(dropdown->opt_text, dropdown->text_offset);
            if (len >= (int)sizeof(text_buf)) len = (int)sizeof(text_buf) - 1;
            memcpy(text_buf, sel_text, len);
            text_buf[len] = '\0';
            sgl_draw_string(surf, &obj->area, obj->coords.x1 + item_pad,
                            obj->coords.y1 + text_pos_y, text_buf,
                            dropdown->text_color, dropdown->alpha, dropdown->font);
        }

        if (dropdown->is_open) {
            const int16_t text_pos_x1 = obj->coords.x1 + item_pad;
            const int16_t text_pos_x2 = obj->coords.x2 - item_pad;
            const int16_t hline_h = item_height - SGL_DROPDOWN_OPTION_SPACE;

            bg_coords.y1 = obj->coords.y1 + dropdown->option_h;
            bg_coords.y2 = bg_coords.y1 + visible_items * item_height - 1;
            sgl_draw_rect(surf, &obj->area, &bg_coords, &bg_desc);

            sgl_area_t bg_area = {
                .x1 = obj->area.x1, .x2 = obj->area.x2,
                .y1 = sgl_max(bg_coords.y1, obj->coords.y1),
                .y2 = sgl_min(bg_coords.y2, obj->coords.y2),
            };

            int16_t draw_text_y = bg_coords.y1 + SGL_DROPDOWN_OPTION_SPACE - (int16_t)dropdown->sc.offset;
            sgl_draw_fill_hline(surf, &bg_area, draw_text_y - SGL_DROPDOWN_OPTION_SPACE,
                                text_pos_x1, text_pos_x2, 1, dropdown->text_color, dropdown->alpha);

            /* Iterate through all items from the beginning */
            sgl_area_t select = {
                .x1 = obj->coords.x1 + obj->border,
                .x2 = obj->coords.x2 - obj->border,
            };

            int item_idx = 0;
            int offset = 0;

            while (dropdown->opt_text && dropdown->opt_text[offset] != '\0') {
                if (draw_text_y >= bg_area.y2) break;

                const char *item_text = dropdown->opt_text + offset;
                int len = sgl_string_option_get_text_len(dropdown->opt_text, offset);

                int copy_len = len < (int)sizeof(text_buf) - 1 ? len : (int)sizeof(text_buf) - 1;
                memcpy(text_buf, item_text, copy_len);
                text_buf[copy_len] = '\0';

                if (item_idx == dropdown->item_selected) {
                    select.y1 = draw_text_y - SGL_DROPDOWN_OPTION_SPACE + 1;
                    select.y2 = select.y1 + item_height;

                    if (select.y1 >= (bg_coords.y1 + obj->radius) && select.y2 <= (bg_coords.y2 - obj->radius)) {
                        sgl_draw_fill_rect(surf, &bg_area, &select, 0, dropdown->item_selected_color, dropdown->alpha);
                    } else if (select.y1 <= (bg_coords.y1 + obj->radius)) {
                        sgl_area_t sel = select;
                        sel.y1 = bg_coords.y1 + obj->border;
                        sel.y2 = select.y1 + item_height + obj->radius + 1;
                        sgl_draw_fill_rect(surf, &select, &sel, obj->radius, dropdown->item_selected_color, dropdown->alpha);
                    } else if (select.y2 >= (bg_coords.y2 - obj->radius)) {
                        sgl_area_t sel = select;
                        sel.y1 = select.y1 - item_height - obj->radius - 1;
                        sel.y2 = bg_coords.y2 - obj->border;
                        sgl_draw_fill_rect(surf, &select, &sel, obj->radius, dropdown->item_selected_color, dropdown->alpha);
                    }
                }

                sgl_draw_string(surf, &bg_area, text_pos_x1, draw_text_y, text_buf,
                                dropdown->text_color, dropdown->alpha, dropdown->font);
                sgl_draw_fill_hline(surf, &bg_area, draw_text_y + hline_h,
                                    text_pos_x1, text_pos_x2, 1, dropdown->text_color, dropdown->alpha);

                draw_text_y += item_height;
                item_idx++;
                offset += len;
                if (dropdown->opt_text[offset] == '\n') offset++;
            }

            sgl_scroll_draw_bar(surf, obj, &dropdown->sc, sgl_dropdown_max_scroll(dropdown, item_height), &bg_coords);
        }
    } break;

    case SGL_EVENT_PRESSED:
        if (dropdown->is_open) {
            sgl_scroll_press(&dropdown->sc, evt->pos.y);
            sgl_scroll_bar_wake(&dropdown->sc);
        }
        break;

    case SGL_EVENT_MOVE_UP:
    case SGL_EVENT_MOVE_DOWN:
        if (dropdown->is_open) {
            const int32_t max_scroll = sgl_dropdown_max_scroll(dropdown, item_height);
            uint8_t r = sgl_scroll_stay(&dropdown->sc,
                                        evt->pos.y, max_scroll);
            if (r) {
                bg_coords.y1 += dropdown->option_h;
                sgl_obj_update_area(&bg_coords);
            }
        }
        break;

    case SGL_EVENT_CLICKED:
        if (dropdown->is_open) {
            const int32_t click_offset = dropdown->sc.offset;
            dropdown->is_open = false;
            sgl_scroll_anim_stop(&dropdown->sc);
            sgl_scroll_reset(&dropdown->sc);
            obj->coords.y2 = obj->coords.y1 + dropdown->option_h - 1;
            if (evt->pos.y > obj->coords.y2) {
                int new_sel = (int)((evt->pos.y - obj->coords.y2 + click_offset) / item_height);
                if (new_sel >= 0 && new_sel < dropdown->item_num) {
                    dropdown->item_selected = (int16_t)new_sel;
                    dropdown->text_offset = (uint16_t)sgl_string_option_get_offset(dropdown->opt_text, dropdown->item_selected);
                }
            }
        } else {
            dropdown->is_open = true;
            obj->coords.y2 = obj->coords.y1 + dropdown->option_h
                           + visible_items * item_height - 1;
        }
        sgl_obj_set_dirty(obj);
        break;

    case SGL_EVENT_RELEASED:
        if (dropdown->is_open) {
            const int32_t max_scroll = sgl_dropdown_max_scroll(dropdown, item_height);
            dropdown->sc.range = max_scroll;
            dropdown->sc.commit = sgl_dropdown_scroll_commit;
            if (sgl_scroll_release(&dropdown->sc, max_scroll)) {
                sgl_scroll_anim_start(&dropdown->sc);
            }
            bg_coords.y1 += dropdown->option_h;
            sgl_obj_update_area(&bg_coords);
        }
        break;

    case SGL_EVENT_DESTROYED:
        sgl_scroll_anim_stop(&dropdown->sc);
        free_dynamic_text(dropdown);
        break;

    case SGL_EVENT_KEY_DOWN:
    case SGL_EVENT_KEY_UP:
    case SGL_EVENT_KEY_LEFT:
    case SGL_EVENT_KEY_RIGHT: {
        if (dropdown->item_num == 0 || (!dropdown->is_open)) break;
        if ((evt->type == SGL_EVENT_KEY_DOWN || evt->type == SGL_EVENT_KEY_RIGHT)
            && (dropdown->item_selected < dropdown->item_num - 1)) {
            dropdown->item_selected++;
        }
        else if ((evt->type == SGL_EVENT_KEY_UP || evt->type == SGL_EVENT_KEY_LEFT)
            && (dropdown->item_selected > 0)) {
            dropdown->item_selected--;
        }

        /* Update text_offset for the new selection */
        dropdown->text_offset = (uint16_t)sgl_string_option_get_offset(dropdown->opt_text, dropdown->item_selected);

        if (dropdown->is_open) {
            sgl_dropdown_ensure_visible(dropdown, list_h, dropdown->option_h);
        }
        bg_coords.y1 += dropdown->option_h;
        sgl_obj_update_area(&bg_coords);
    } break;

    default:
        break;
    }
}

/**
 * @brief create a dropdown object
 * @param parent parent of the dropdown
 * @return pointer to the dropdown object
 */
sgl_obj_t* sgl_dropdown_create(sgl_obj_t* parent)
{
    sgl_dropdown_t *dropdown = sgl_malloc(sizeof(sgl_dropdown_t));
    if(dropdown == NULL) {
        SGL_LOG_ERROR("sgl_dropdown_create: malloc failed");
        return NULL;
    }

    /* set object all member to zero */
    memset(dropdown, 0, sizeof(sgl_dropdown_t));

    sgl_obj_t *obj = &dropdown->obj;
    sgl_obj_init(&dropdown->obj, parent);
    obj->construct_fn = sgl_dropdown_construct_cb;
    sgl_obj_set_border_width(obj, 1);
    sgl_obj_set_clickable(obj);
    sgl_obj_set_editable(obj);
    sgl_obj_set_movable(obj);
    sgl_obj_set_keypress_click(obj);

    dropdown->alpha = SGL_THEME_ALPHA;
    dropdown->bg_color = SGL_THEME_COLOR;
    dropdown->border_color = SGL_THEME_BORDER_COLOR;
    dropdown->item_selected_color = sgl_color_mixer(SGL_THEME_COLOR, SGL_THEME_BG_COLOR, 128);
    dropdown->text_color = SGL_THEME_TEXT_COLOR;
    dropdown->item_num = 0;
    dropdown->is_open = false;
    dropdown->font = sgl_get_system_font();
    dropdown->opt_text = NULL;
    dropdown->text_offset = 0;
    dropdown->dynamic_text = 0;
    dropdown->item_selected = -1;
    dropdown->max_visible_item = 5;
    sgl_scroll_reset(&dropdown->sc);
    return obj;
}

/**
 * @brief set dropdown object's max visible rows
 * @param obj dropdown object
 * @param rows max visible rows
 */
void sgl_dropdown_set_visible_rows(sgl_obj_t *obj, int16_t rows)
{
    sgl_dropdown_t *dropdown = sgl_container_of(obj, sgl_dropdown_t, obj);
    dropdown->max_visible_item = rows;
}

/**
 * @brief set dropdown object's background color
 * @param obj dropdown object
 * @param color background color
 */
void sgl_dropdown_set_bg_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_dropdown_t *dropdown = sgl_container_of(obj, sgl_dropdown_t, obj);
    dropdown->bg_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set dropdown object's border width
 * @param obj dropdown object
 * @param width border width
 */
void sgl_dropdown_set_border_width(sgl_obj_t *obj, uint8_t width)
{
    sgl_obj_set_border_width(obj, width);
}

/**
 * @brief set dropdown object's border color
 * @param obj dropdown object
 * @param color border color
 */
void sgl_dropdown_set_border_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_dropdown_t *dropdown = sgl_container_of(obj, sgl_dropdown_t, obj);
    dropdown->border_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the selected color of dropdown
 * @param obj dropdown object
 * @param color selected option color
 */
void sgl_dropdown_set_selected_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_dropdown_t *dropdown = sgl_container_of(obj, sgl_dropdown_t, obj);
    dropdown->item_selected_color = color;
}

/**
 * @brief set dropdown object's radius
 * @param obj dropdown object
 * @param radius radius
 */
void sgl_dropdown_set_radius(sgl_obj_t *obj, uint8_t radius)
{
    sgl_obj_set_radius(obj, radius);
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set dropdown object's alpha
 * @param obj dropdown object
 * @param alpha alpha
 */
void sgl_dropdown_set_alpha(sgl_obj_t *obj, uint8_t alpha)
{
    sgl_dropdown_t *dropdown = sgl_container_of(obj, sgl_dropdown_t, obj);
    dropdown->alpha = alpha;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set dropdown object's text color
 * @param obj dropdown object
 * @param color text color
 */
void sgl_dropdown_set_text_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_dropdown_t *dropdown = sgl_container_of(obj, sgl_dropdown_t, obj);
    dropdown->text_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set dropdown object's text font
 * @param obj dropdown object
 * @param font text font
 * @return none
 * @note font must be initialized
 */
void sgl_dropdown_set_text_font(sgl_obj_t *obj, const sgl_font_t* font)
{
    sgl_dropdown_t *dropdown = sgl_container_of(obj, sgl_dropdown_t, obj);
    dropdown->font = font;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief get dropdown object's selected index
 * @param obj dropdown object
 * @return selected index
 */
int sgl_dropdown_get_selected_index(sgl_obj_t *obj)
{
    sgl_dropdown_t *dropdown = sgl_container_of(obj, sgl_dropdown_t, obj);
    return dropdown->item_selected;
}

/**
 * @brief get dropdown object's selected text
 * @param obj dropdown object
 * @param buf buffer to store the selected text
 * @param buf_size size of the buffer
 * @return true if successful, false otherwise
 */
bool sgl_dropdown_get_selected_text(sgl_obj_t *obj, char *buf, int buf_size)
{
    sgl_dropdown_t *dropdown = sgl_container_of(obj, sgl_dropdown_t, obj);
    if (!dropdown->opt_text || dropdown->item_selected < 0 || dropdown->item_selected >= dropdown->item_num) {
        return false;
    }
    int len = sgl_string_option_get_text_len(dropdown->opt_text, dropdown->text_offset);
    if (len >= buf_size) len = buf_size - 1;
    memcpy(buf, dropdown->opt_text + dropdown->text_offset, len);
    buf[len] = '\0';
    return true;
}

/**
 * @brief set options with static text (no copy, caller must ensure text lifetime)
 * @param obj dropdown object
 * @param text newline-separated option text
 */
void sgl_dropdown_set_option_static(sgl_obj_t *obj, const char *text)
{
    sgl_dropdown_t *dropdown = sgl_container_of(obj, sgl_dropdown_t, obj);

    /* Free old dynamic text if any */
    free_dynamic_text(dropdown);

    dropdown->opt_text = (char *)text;
    dropdown->dynamic_text = 0;
    dropdown->item_selected = 0;
    update_item_count(dropdown);
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set options with dynamic text (makes a copy)
 * @param obj dropdown object
 * @param text newline-separated option text
 */
void sgl_dropdown_set_option_dynamic(sgl_obj_t *obj, const char *text)
{
    sgl_dropdown_t *dropdown = sgl_container_of(obj, sgl_dropdown_t, obj);

    /* Free old dynamic text if any */
    free_dynamic_text(dropdown);

    int len = (int)strlen(text);
    char *buf = sgl_malloc(len + 1);
    if (!buf) {
        SGL_LOG_ERROR("sgl_dropdown_set_option_dynamic: malloc failed");
        return;
    }
    memcpy(buf, text, len + 1);
    dropdown->opt_text = buf;
    dropdown->dynamic_text = 1;
    dropdown->item_selected = 0;
    update_item_count(dropdown);
    sgl_obj_set_dirty(obj);
}

/**
 * @brief add an option to the dropdown (only works for dynamic options)
 * @param obj pointer to the dropdown object
 * @param text pointer to the text to add
 * @return none
 */
void sgl_dropdown_add_option(sgl_obj_t *obj, const char *text)
{
    sgl_dropdown_t *dropdown = sgl_container_of(obj, sgl_dropdown_t, obj);

    if (!text) return;

    int add_len = (int)strlen(text);
    int old_len = dropdown->opt_text ? (int)strlen(dropdown->opt_text) : 0;
    int need_sep = old_len > 0;
    /* Need: old text + optional '\n' + new text + '\0' */
    int new_len = old_len + need_sep + add_len;

    char *new_text = sgl_malloc(new_len + 1);
    if (!new_text) {
        SGL_LOG_ERROR("sgl_dropdown_add_option: malloc failed");
        return;
    }

    if (dropdown->opt_text && old_len > 0) {
        memcpy(new_text, dropdown->opt_text, old_len);
    }
    if (need_sep) {
        new_text[old_len] = '\n';
    }
    memcpy(new_text + old_len + need_sep, text, add_len);
    new_text[new_len] = '\0';

    /* Free old dynamic text */
    free_dynamic_text(dropdown);

    dropdown->opt_text = new_text;
    dropdown->dynamic_text = 1;

    if (dropdown->item_selected == -1) {
        dropdown->item_selected = 0;
    }
    update_item_count(dropdown);
    sgl_obj_set_dirty(obj);
}

/**
 * @brief delete an option from the dropdown by text match (only for dynamic)
 * @param obj pointer to the dropdown object
 * @param text pointer to the text to match
 * @return none
 */
void sgl_dropdown_delete_option_by_text(sgl_obj_t *obj, const char *text)
{
    sgl_dropdown_t *dropdown = sgl_container_of(obj, sgl_dropdown_t, obj);
    if (!dropdown->opt_text || !text || !dropdown->dynamic_text) return;

    int text_len = (int)strlen(text);
    int offset = 0;
    int item_idx = 0;

    while (dropdown->opt_text[offset] != '\0') {
        int item_len = sgl_string_option_get_text_len(dropdown->opt_text, offset);

        if (item_len == text_len && memcmp(dropdown->opt_text + offset, text, item_len) == 0) {
            remove_item_at(dropdown, offset);
            adjust_selection_after_delete(dropdown, item_idx);
            update_item_count(dropdown);
            sgl_obj_set_dirty(obj);
            return;
        }

        offset += item_len;
        if (dropdown->opt_text[offset] == '\n') offset++;
        item_idx++;
    }
}

/**
 * @brief delete an option from the dropdown by index (only for dynamic)
 * @param obj pointer to the dropdown object
 * @param index index of the option to delete
 * @return none
 */
void sgl_dropdown_delete_option_by_index(sgl_obj_t *obj, int index)
{
    sgl_dropdown_t *dropdown = sgl_container_of(obj, sgl_dropdown_t, obj);
    if (!dropdown->opt_text || index < 0 || index >= dropdown->item_num || !dropdown->dynamic_text) return;

    int offset = sgl_string_option_get_offset(dropdown->opt_text, index);
    if (offset < 0) return;

    remove_item_at(dropdown, offset);
    adjust_selection_after_delete(dropdown, index);
    update_item_count(dropdown);
    sgl_obj_set_dirty(obj);
}
