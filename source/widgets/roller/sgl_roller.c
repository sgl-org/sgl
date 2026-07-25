/* source/widgets/sgl_roller.c
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

#include <sgl_theme.h>
#include <sgl_misc.h>
#include <string.h>
#include "sgl_roller.h"

/** Item height = font height + 6 (per spec) */
static inline int roller_item_height(const sgl_roller_t *roller)
{
    return sgl_font_get_height(roller->font) + 6;
}

/** Effective draw height: max(widget height, 3 * item_h) */
static inline int roller_draw_height(const sgl_roller_t *roller, int item_h)
{
    int widget_h = sgl_obj_get_height((sgl_obj_t *)&roller->obj);
    return sgl_max(widget_h, 3 * item_h);
}

static int roller_wrap_index(int idx, int item_num)
{
    if (item_num <= 0) return 0;
    idx %= item_num;
    if (idx < 0) idx += item_num;
    return idx;
}

static int roller_text_offset_for_index(const sgl_roller_t *roller, int idx)
{
    if (!roller->opt_text || roller->item_num == 0) return 0;
    return sgl_string_option_get_offset(roller->opt_text,
                                        (uint16_t)roller_wrap_index(idx, roller->item_num));
}

/** Free dynamic text buffer if owned */
static void roller_free_dynamic_text(sgl_roller_t *roller)
{
    if (roller->dynamic_text && roller->opt_text) {
        sgl_free(roller->opt_text);
        roller->opt_text = NULL;
        roller->dynamic_text = 0;
    }
}

/** Recalculate item_num and text_offset from opt_text */
static void roller_update_item_count(sgl_roller_t *roller)
{
    roller->item_num = sgl_string_option_get_count(roller->opt_text);
    if (roller->item_selected >= 0 && roller->opt_text) {
        roller->text_offset = (uint16_t)sgl_string_option_get_offset(roller->opt_text, roller->item_selected);
    }
}

/** Clamp scroll_y so the content stays within bounds */
static void roller_clamp_scroll(sgl_roller_t *roller, int item_h)
{
    if (roller->infinite || roller->item_num == 0) {
        return;
    }

    /* scroll_y = -idx * item_h, so:
     * max scroll: idx=0 → scroll_y = 0
     * min scroll: idx=item_num-1 → scroll_y = -(item_num-1) * item_h */
    const int max_scroll = 0;
    const int min_scroll = -(int)(roller->item_num > 0 ? roller->item_num - 1 : 0) * item_h;

    if (roller->scroll_y > max_scroll) roller->scroll_y = (int16_t)max_scroll;
    if (roller->scroll_y < min_scroll) roller->scroll_y = (int16_t)min_scroll;
}

/** Snap scroll_y to the nearest item */
static void roller_snap(sgl_roller_t *roller, int item_h)
{
    /* scroll_y = -idx * item_h → idx = round(-scroll_y / item_h) */
    int idx;
    if (roller->scroll_y <= 0) {
        idx = (-roller->scroll_y + item_h / 2) / item_h;
    } else {
        idx = (-roller->scroll_y - item_h / 2) / item_h;
    }
    if (roller->infinite) {
        idx = roller_wrap_index(idx, roller->item_num);
    } else {
        if (idx < 0) idx = 0;
        if (idx >= (int)roller->item_num) idx = (int)roller->item_num - 1;
    }

    roller->item_selected = (int16_t)idx;
    roller->text_offset = (uint16_t)roller_text_offset_for_index(roller, idx);
    roller->scroll_y = (int16_t)(-idx * item_h);
}

/** Scroll to make item_selected centered */
static void roller_scroll_to_selected(sgl_roller_t *roller, int item_h)
{
    roller->scroll_y = (int16_t)(-roller->item_selected * item_h);
}

static void sgl_roller_construct_cb(sgl_surf_t *surf, sgl_obj_t *obj, sgl_event_t *evt)
{
    sgl_roller_t *roller = sgl_container_of(obj, sgl_roller_t, obj);
    const int item_h = roller_item_height(roller);

    switch (evt->type) {
    case SGL_EVENT_DRAW_MAIN: {
        char text_buf[128];

        /* Draw background */
        sgl_draw_rect_t bg_desc = {
            .alpha = roller->alpha,
            .color = roller->bg_color,
            .border = obj->border,
            .border_alpha = roller->alpha,
            .border_color = roller->border_color,
            .border_mask = obj->focus,
            .radius = obj->radius,
            .pixmap = NULL,
        };
        sgl_draw_rect(surf, &obj->area, &obj->coords, &bg_desc);

        if (!roller->opt_text || roller->item_num == 0) break;

        /* Compute draw area: use widget height, at least 3 rows */
        const int draw_h = roller_draw_height(roller, item_h);
        const int widget_h = sgl_obj_get_height(obj);
        const int draw_y1 = obj->coords.y1;
        const int draw_y2 = draw_y1 + draw_h - 1;

        /* Selected band: vertically centered in widget area (not draw area) */
        const int band_y1 = draw_y1 + (widget_h - item_h) / 2;
        const int band_y2 = band_y1 + item_h - 1;
        sgl_area_t band_area = {
            .x1 = obj->area.x1, .x2 = obj->area.x2,
            .y1 = sgl_max((int16_t)band_y1, obj->coords.y1),
            .y2 = sgl_min((int16_t)band_y2, obj->coords.y2),
        };
        sgl_draw_fill_rect(surf, &obj->area, &band_area, 0, roller->selected_color, roller->alpha);

        /* Text vertical centering within item */
        const int font_h = sgl_font_get_height(roller->font);
        const int text_y_off = (item_h - font_h) / 2;

        /* Items start so that item 0 aligns with band when scroll_y == 0 */
        if (roller->infinite) {
            int idx_base = 0;
            int16_t item_draw_y = band_y1 + roller->scroll_y;

            if (item_draw_y > draw_y1) {
                int delta = (item_draw_y - draw_y1 + item_h - 1) / item_h;
                idx_base -= delta;
                item_draw_y -= (int16_t)(delta * item_h);
            } else if (item_draw_y + item_h < draw_y1) {
                int delta = (draw_y1 - (item_draw_y + item_h) + item_h - 1) / item_h + 1;
                idx_base += delta;
                item_draw_y += (int16_t)(delta * item_h);
            }

            while (item_draw_y <= draw_y2) {
                int offset = roller_text_offset_for_index(roller, idx_base);
                int len = sgl_string_option_get_text_len(roller->opt_text, offset);
                int copy_len = len < (int)sizeof(text_buf) - 1 ? len : (int)sizeof(text_buf) - 1;
                memcpy(text_buf, roller->opt_text + offset, copy_len);
                text_buf[copy_len] = '\0';

                sgl_draw_string(surf, &obj->area, obj->coords.x1 + obj->radius + 2,
                                item_draw_y + text_y_off, text_buf,
                                roller->text_color, roller->alpha, roller->font);

                idx_base++;
                item_draw_y += item_h;
            }
        } else {
            int item_idx = 0;
            int offset = 0;
            int16_t item_draw_y = band_y1 + roller->scroll_y;

            while (roller->opt_text[offset] != '\0') {
                /* Skip items above visible area */
                if (item_draw_y + item_h < draw_y1) {
                    int len = sgl_string_option_get_text_len(roller->opt_text, offset);
                    offset += len;
                    if (roller->opt_text[offset] == '\n') offset++;
                    item_idx++;
                    item_draw_y += item_h;
                    continue;
                }
                /* Stop if below visible area */
                if (item_draw_y > draw_y2) break;

                int len = sgl_string_option_get_text_len(roller->opt_text, offset);
                int copy_len = len < (int)sizeof(text_buf) - 1 ? len : (int)sizeof(text_buf) - 1;
                memcpy(text_buf, roller->opt_text + offset, copy_len);
                text_buf[copy_len] = '\0';

                sgl_draw_string(surf, &obj->area, obj->coords.x1 + obj->radius + 2,
                                item_draw_y + text_y_off, text_buf,
                                roller->text_color, roller->alpha, roller->font);

                offset += len;
                if (roller->opt_text[offset] == '\n') offset++;
                item_idx++;
                item_draw_y += item_h;
            }
        }
    } break;

    case SGL_EVENT_PRESSED:
        roller->drag_start_y = evt->pos.y;
        roller->drag_start_scroll = roller->scroll_y;
        sgl_obj_set_dirty(obj);
        break;

    case SGL_EVENT_MOVE_UP:
    case SGL_EVENT_MOVE_DOWN:
        roller->scroll_y = roller->drag_start_scroll + (evt->pos.y - roller->drag_start_y);
        roller_clamp_scroll(roller, item_h);
        sgl_obj_set_dirty(obj);
        break;

    case SGL_EVENT_RELEASED:
        roller_snap(roller, item_h);
        sgl_obj_set_dirty(obj);
        break;

    case SGL_EVENT_DESTROYED:
        roller_free_dynamic_text(roller);
        break;

    case SGL_EVENT_KEY_UP:
    case SGL_EVENT_KEY_LEFT:
    case SGL_EVENT_KEY_DOWN:
    case SGL_EVENT_KEY_RIGHT:
        if (roller->item_num == 0) break;
        if (evt->type == SGL_EVENT_KEY_UP || evt->type == SGL_EVENT_KEY_LEFT) {
            if (roller->infinite) {
                roller->item_selected = (int16_t)roller_wrap_index(roller->item_selected - 1, roller->item_num);
            } else if (roller->item_selected > 0) {
                roller->item_selected--;
            }
        }
        else {
            if (roller->infinite) {
                roller->item_selected = (int16_t)roller_wrap_index(roller->item_selected + 1, roller->item_num);
            } else if (roller->item_selected < (int)roller->item_num - 1) {
                roller->item_selected++;
            }
        }

        if (roller->item_selected >= 0 && roller->item_selected < (int)roller->item_num) {
            roller->text_offset = (uint16_t)roller_text_offset_for_index(roller, roller->item_selected);
            roller_scroll_to_selected(roller, item_h);
        }
        sgl_obj_set_dirty(obj);
        break;

    default:
        break;
    }
}

/**
 * @brief create a roller object
 * @param parent parent of the roller
 * @return roller object
 */
sgl_obj_t* sgl_roller_create(sgl_obj_t* parent)
{
    sgl_roller_t *roller = sgl_malloc(sizeof(sgl_roller_t));
    if (roller == NULL) {
        SGL_LOG_ERROR("sgl_roller_create: malloc failed");
        return NULL;
    }

    memset(roller, 0, sizeof(sgl_roller_t));
    sgl_obj_t *obj = &roller->obj;
    sgl_obj_init(&roller->obj, parent);
    obj->construct_fn = sgl_roller_construct_cb;
    sgl_obj_set_clickable(obj);
    sgl_obj_set_editable(obj);
    sgl_obj_set_keypress_mask(obj);
    sgl_obj_set_movable(obj);
    sgl_obj_set_border_width(obj, 1);

    roller->bg_color = SGL_THEME_COLOR;
    roller->border_color = SGL_THEME_BORDER_COLOR;
    roller->text_color = SGL_THEME_TEXT_COLOR;
    roller->selected_color = sgl_color_mixer(SGL_THEME_COLOR, SGL_THEME_BG_COLOR, 128);
    roller->font = sgl_get_system_font();
    roller->alpha = SGL_THEME_ALPHA;
    roller->visible_rows = 3;
    roller->item_selected = -1;
    roller->opt_text = NULL;
    roller->dynamic_text = 0;
    roller->infinite = 0;
    roller->scroll_y = 0;

    /* Set default size based on font */
    const int item_h = roller_item_height(roller);
    sgl_obj_set_size(obj, 100, (int16_t)(roller->visible_rows * item_h));
    return obj;
}

/**
 * @brief set roller options with static text (no copy)
 * @param obj roller object
 * @param text static text
 * @return none
 */
void sgl_roller_set_option_static(sgl_obj_t *obj, const char *text)
{
    sgl_roller_t *roller = sgl_container_of(obj, sgl_roller_t, obj);

    roller_free_dynamic_text(roller);
    roller->opt_text = (char *)text;
    roller->dynamic_text = 0;
    roller->item_selected = 0;
    roller_update_item_count(roller);
    roller_scroll_to_selected(roller, roller_item_height(roller));
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set roller options with dynamic text (makes a copy)
 * @param obj roller object
 * @param text dynamic text
 * @return none
 */
void sgl_roller_set_option_dynamic(sgl_obj_t *obj, const char *text)
{
    sgl_roller_t *roller = sgl_container_of(obj, sgl_roller_t, obj);

    roller_free_dynamic_text(roller);

    int len = (int)strlen(text);
    char *buf = sgl_malloc(len + 1);
    if (!buf) {
        SGL_LOG_ERROR("sgl_roller_set_option_dynamic: malloc failed");
        return;
    }
    memcpy(buf, text, len + 1);
    roller->opt_text = buf;
    roller->dynamic_text = 1;
    roller->item_selected = 0;
    roller_update_item_count(roller);
    roller_scroll_to_selected(roller, roller_item_height(roller));
    sgl_obj_set_dirty(obj);
}

/**
 * @brief get selected item index
 * @param obj roller object
 * @return selected item index
 */
int sgl_roller_get_selected_index(sgl_obj_t *obj)
{
    sgl_roller_t *roller = sgl_container_of(obj, sgl_roller_t, obj);
    return roller->item_selected;
}

/**
 * @brief set infinite mode
 * @param obj roller object
 * @param enable true to enable infinite mode, false to disable
 * @return none
 */
void sgl_roller_set_infinite_mode(sgl_obj_t *obj, bool enable)
{
    sgl_roller_t *roller = sgl_container_of(obj, sgl_roller_t, obj);
    roller->infinite = enable ? 1U : 0U;

    if (!roller->infinite) {
        roller_clamp_scroll(roller, roller_item_height(roller));
        roller_snap(roller, roller_item_height(roller));
    }

    sgl_obj_set_dirty(obj);
}

/**
 * @brief get selected item text
 * @param obj roller object
 * @param buf buffer to store text
 * @param buf_size size of buffer
 * @return true if text was copied, false otherwise
 */

bool sgl_roller_get_selected_text(sgl_obj_t *obj, char *buf, int buf_size)
{
    sgl_roller_t *roller = sgl_container_of(obj, sgl_roller_t, obj);
    if (!roller->opt_text || roller->item_selected < 0 || roller->item_selected >= roller->item_num) {
        return false;
    }
    int len = sgl_string_option_get_text_len(roller->opt_text, roller->text_offset);
    if (len >= buf_size) len = buf_size - 1;
    memcpy(buf, roller->opt_text + roller->text_offset, len);
    buf[len] = '\0';
    return true;
}

/**
 * @brief set roller visible rows
 * @param obj roller object
 * @param rows number of visible rows
 * @return none
 */
void sgl_roller_set_visible_rows(sgl_obj_t *obj, uint8_t rows)
{
    sgl_roller_t *roller = sgl_container_of(obj, sgl_roller_t, obj);
    if (rows < 1) rows = 1;
    roller->visible_rows = rows;
    const int item_h = roller_item_height(roller);
    roller_scroll_to_selected(roller, item_h);
}

/**
 * @brief set roller text color
 * @param obj roller object
 * @param color text color
 * @return none
 */
void sgl_roller_set_text_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_roller_t *roller = sgl_container_of(obj, sgl_roller_t, obj);
    roller->text_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set roller selected item color
 * @param obj roller object
 * @param color selected item color
 * @return none
 */
void sgl_roller_set_selected_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_roller_t *roller = sgl_container_of(obj, sgl_roller_t, obj);
    roller->selected_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set roller background color
 * @param obj roller object
 * @param color background color
 * @return none
 */
void sgl_roller_set_bg_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_roller_t *roller = sgl_container_of(obj, sgl_roller_t, obj);
    roller->bg_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set roller border color
 * @param obj roller object
 * @param color border color
 * @return none
 */
void sgl_roller_set_border_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_roller_t *roller = sgl_container_of(obj, sgl_roller_t, obj);
    roller->border_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set roller text font
 * @param obj roller object
 * @param font text font
 * @return none
 */
void sgl_roller_set_text_font(sgl_obj_t *obj, const sgl_font_t *font)
{
    sgl_roller_t *roller = sgl_container_of(obj, sgl_roller_t, obj);
    roller->font = font;
    const int item_h = roller_item_height(roller);
    roller_scroll_to_selected(roller, item_h);
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set roller alpha
 * @param obj roller object
 * @param alpha alpha value
 * @return none
 */
void sgl_roller_set_alpha(sgl_obj_t *obj, uint8_t alpha)
{
    sgl_roller_t *roller = sgl_container_of(obj, sgl_roller_t, obj);
    roller->alpha = alpha;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set roller border radius
 * @param obj roller object
 * @param radius border radius
 * @return none
 */
void sgl_roller_set_radius(sgl_obj_t *obj, int16_t radius)
{
    sgl_obj_set_radius(obj, radius);
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set roller border width
 * @param obj roller object
 * @param width border width
 * @return none
 */
void sgl_roller_set_border_width(sgl_obj_t *obj, uint8_t width)
{
    sgl_obj_set_border_width(obj, width);
}
