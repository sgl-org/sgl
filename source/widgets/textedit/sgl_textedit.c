/* source/widgets/textedit/sgl_textedit.c
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
#include <sgl_theme.h>
#include <sgl_cfgfix.h>
#include <sgl_anim.h>
#include <string.h>
#include "sgl_textedit.h"

/**
 * @brief calculate cursor area in pixels for single-line mode
 * @param textedit textedit object
 * @param obj object
 * @param cursor_area output cursor area
 * @return none
 */
static void textedit_calc_cursor_area_single(sgl_textedit_t *textedit, sgl_obj_t *obj, sgl_area_t *cursor_area)
{
    int16_t text_x = obj->coords.x1 + textedit->bg.radius + 2;
    int16_t text_y = obj->coords.y1 + (obj->coords.y2 - obj->coords.y1 - textedit->font->font_height) / 2;

    /* calculate cursor x position based on text before cursor */
    char saved = textedit->text[textedit->cursor_pos];
    textedit->text[textedit->cursor_pos] = '\0';
    int32_t cursor_w = sgl_font_get_string_width(textedit->text, textedit->font);
    textedit->text[textedit->cursor_pos] = saved;

    cursor_area->x1 = text_x + cursor_w;
    cursor_area->y1 = text_y;
    cursor_area->x2 = cursor_area->x1 + SGL_TEXTEDIT_CURSOR_WIDTH - 1;
    cursor_area->y2 = cursor_area->y1 + textedit->font->font_height - 1;

    /* clamp to right edge */
    if (cursor_area->x1 > obj->coords.x2 - textedit->bg.radius - 2) {
        cursor_area->x1 = obj->coords.x2 - textedit->bg.radius - 2;
        cursor_area->x2 = cursor_area->x1 + SGL_TEXTEDIT_CURSOR_WIDTH - 1;
    }
}

/**
 * @brief calculate cursor area in pixels for multi-line mode
 * @param textedit textedit object
 * @param obj object
 * @param cursor_area output cursor area
 * @return none
 */
static void textedit_calc_cursor_area_multi(sgl_textedit_t *textedit, sgl_obj_t *obj, sgl_area_t *cursor_area)
{
    int16_t body_w = obj->coords.x2 - obj->coords.x1 - 2 * textedit->bg.radius - 4;
    int16_t text_x = obj->coords.x1 + textedit->bg.radius + 2;
    int16_t text_y = obj->coords.y1 + textedit->bg.radius + 2;
    int16_t line_h = textedit->font->font_height + textedit->line_margin;
    int16_t cur_line = 0;
    int32_t line_start = 0;
    int32_t i = 0;

    /* find which line the cursor is on */
    while (i < textedit->cursor_pos && textedit->text[i] != '\0') {
        if (textedit->text[i] == '\n') {
            cur_line++;
            line_start = i + 1;
        }
        i++;
    }

    /* calculate cursor x position within the line */
    char saved = textedit->text[textedit->cursor_pos];
    textedit->text[textedit->cursor_pos] = '\0';
    int32_t cursor_w = sgl_font_get_string_width(textedit->text + line_start, textedit->font);
    textedit->text[textedit->cursor_pos] = saved;

    int16_t cursor_x = text_x + cursor_w;
    int16_t cursor_y = text_y + cur_line * line_h + textedit->y_offset;
    int16_t cursor_h = textedit->font->font_height;

    /* vertical scroll: ensure cursor is visible */
    int16_t view_h = obj->coords.y2 - obj->coords.y1 - 2 * textedit->bg.radius - 4;
    if (cursor_y + cursor_h > text_y + view_h) {
        textedit->y_offset -= (cursor_y + cursor_h) - (text_y + view_h);
        cursor_y = text_y + view_h - cursor_h;
    }
    else if (cursor_y < text_y) {
        textedit->y_offset += text_y - cursor_y;
        cursor_y = text_y;
    }

    /* clamp y_offset */
    if (textedit->y_offset > 0) {
        textedit->y_offset = 0;
    }
    int32_t total_h = sgl_font_get_string_height(body_w, textedit->text, textedit->font, textedit->line_margin);
    if (total_h > view_h) {
        int32_t min_offset = view_h - total_h;
        if (textedit->y_offset < min_offset) {
            textedit->y_offset = min_offset;
        }
    }
    else {
        textedit->y_offset = 0;
    }

    cursor_area->x1 = cursor_x;
    cursor_area->y1 = cursor_y;
    cursor_area->x2 = cursor_area->x1 + SGL_TEXTEDIT_CURSOR_WIDTH - 1;
    cursor_area->y2 = cursor_area->y1 + cursor_h - 1;
}

/**
 * @brief calculate cursor area based on current mode
 * @param textedit textedit object
 * @param obj object
 * @param cursor_area output cursor area
 * @return none
 */
static void textedit_calc_cursor_area(sgl_textedit_t *textedit, sgl_obj_t *obj, sgl_area_t *cursor_area)
{
    if (textedit->mode == SGL_TEXTEDIT_SINGLE_LINE) {
        textedit_calc_cursor_area_single(textedit, obj, cursor_area);
    }
    else {
        textedit_calc_cursor_area_multi(textedit, obj, cursor_area);
    }
}

/**
 * @brief cursor blink animation path callback
 * @param anim animation object
 * @param value animation value (0 or 1)
 * @return none
 */
static void textedit_cursor_blink_cb(sgl_anim_t *anim, int32_t value)
{
    sgl_obj_t *obj = (sgl_obj_t*)anim->data;
    sgl_textedit_t *textedit = sgl_container_of(obj, sgl_textedit_t, obj);
    textedit->cursor_visible = (value != 0) ? 1 : 0;

    /* only update cursor area */
    sgl_area_t cursor_area;
    textedit_calc_cursor_area(textedit, obj, &cursor_area);
    sgl_update_area(&cursor_area);
}

/**
 * @brief cursor blink animation path algorithm (step function)
 * @param elaps elapsed time
 * @param duration total duration
 * @param start start value
 * @param end end value
 * @return current value
 */
static int32_t textedit_cursor_blink_path(uint16_t elaps, uint16_t duration, int32_t start, int32_t end)
{
    SGL_UNUSED(duration);
    SGL_UNUSED(start);
    SGL_UNUSED(end);
    /* toggle between 0 and 1 based on elapsed time */
    return (elaps / (SGL_TEXTEDIT_CURSOR_BLINK_MS / 2)) % 2;
}

/**
 * @brief start cursor blink animation
 * @param obj textedit object
 * @return none
 */
void sgl_textedit_cursor_blink_start(sgl_obj_t *obj)
{
    sgl_textedit_t *textedit = sgl_container_of(obj, sgl_textedit_t, obj);

    if (textedit->cursor_anim != NULL) {
        return; /* already running */
    }

    textedit->cursor_anim = sgl_anim_create();
    if (textedit->cursor_anim == NULL) {
        SGL_LOG_ERROR("textedit cursor anim create failed");
        return;
    }

    sgl_anim_set_data(textedit->cursor_anim, obj);
    sgl_anim_set_start_value(textedit->cursor_anim, 0);
    sgl_anim_set_end_value(textedit->cursor_anim, 1);
    sgl_anim_set_act_duration(textedit->cursor_anim, SGL_TEXTEDIT_CURSOR_BLINK_MS);
    sgl_anim_set_path(textedit->cursor_anim, textedit_cursor_blink_cb, textedit_cursor_blink_path);
    sgl_anim_start(textedit->cursor_anim, SGL_ANIM_REPEAT_LOOP);

    textedit->cursor_visible = 1;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief stop cursor blink animation
 * @param obj textedit object
 * @return none
 */
void sgl_textedit_cursor_blink_stop(sgl_obj_t *obj)
{
    sgl_textedit_t *textedit = sgl_container_of(obj, sgl_textedit_t, obj);

    if (textedit->cursor_anim != NULL) {
        sgl_anim_delete(textedit->cursor_anim);
        textedit->cursor_anim = NULL;
    }

    textedit->cursor_visible = 0;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief textedit constructor function
 * @param surf pointer to surface
 * @param obj pointer to textedit object
 * @param evt pointer to event
 * @return none
 */
static void sgl_textedit_construct_cb(sgl_surf_t *surf, sgl_obj_t* obj, sgl_event_t *evt)
{
    sgl_textedit_t *textedit = sgl_container_of(obj, sgl_textedit_t, obj);
    int16_t body_w = obj->coords.x2 - obj->coords.x1 - 2 * textedit->bg.radius;
    int16_t body_h = obj->coords.y2 - obj->coords.y1 - 2 * textedit->bg.radius;
    sgl_area_t area, clip;

    SGL_ASSERT(textedit->font != NULL);

    switch (evt->type) {
    case SGL_EVENT_DRAW_MAIN:
        /* draw background */
        textedit->bg.border_mask = obj->focus;
        sgl_draw_rect(surf, &obj->area, &obj->coords, &textedit->bg);

        /* calculate text area */
        area.x1 = obj->coords.x1 + textedit->bg.radius + 2;
        area.y1 = obj->coords.y1 + textedit->bg.radius + 2;
        area.x2 = obj->coords.x2 - textedit->bg.radius - 2;
        area.y2 = obj->coords.y2 - textedit->bg.radius - 2;

        if (!sgl_area_selfclip(&area, &obj->area)) {
            break;
        }

        /* draw text */
        if (textedit->text != NULL && textedit->text[0] != '\0') {
            if (textedit->mode == SGL_TEXTEDIT_SINGLE_LINE) {
                sgl_draw_string(surf, &area, area.x1,
                               obj->coords.y1 + (body_h - textedit->font->font_height) / 2 + textedit->bg.radius,
                               textedit->text, textedit->text_color, textedit->bg.alpha, textedit->font);
            }
            else {
                sgl_draw_string_mult_line(surf, &area, area.x1,
                                         area.y1 + textedit->y_offset,
                                         textedit->text, textedit->text_color, textedit->bg.alpha,
                                         textedit->font, textedit->line_margin);
            }
        }

        /* draw cursor */
        if (textedit->cursor_visible) {
            sgl_area_t cursor_area;
            textedit_calc_cursor_area(textedit, obj, &cursor_area);

            /* clip cursor to visible area */
            clip = cursor_area;
            if (sgl_area_selfclip(&clip, &area)) {
                sgl_draw_fill_rect(surf, &area, &clip, 0, textedit->cursor_color, SGL_ALPHA_MAX);
            }
        }

        /* draw scroll bar for multi-line mode when pressed */
        if (textedit->mode == SGL_TEXTEDIT_MULTI_LINE) {
            int16_t view_h = body_h - 4;
            int32_t total_h = sgl_font_get_string_height(body_w - 4, textedit->text,
                                                          textedit->font, textedit->line_margin);
            if (total_h > view_h) {
                int16_t scroll_h = sgl_max(view_h * view_h / total_h, 8);
                int16_t scroll_y = obj->coords.y1 + textedit->bg.radius + 2 +
                                   (-textedit->y_offset) * (view_h - scroll_h) / (total_h - view_h);
                sgl_area_t scroll_area = {
                    .x1 = obj->coords.x2 - SGL_TEXTEDIT_SCROLL_WIDTH - textedit->bg.radius,
                    .y1 = scroll_y,
                    .x2 = obj->coords.x2 - textedit->bg.radius - 1,
                    .y2 = scroll_y + scroll_h - 1,
                };
                sgl_draw_fill_rect(surf, &obj->area, &scroll_area, SGL_TEXTEDIT_SCROLL_WIDTH / 2,
                                  textedit->text_color, 128);
            }
        }
        break;

    case SGL_EVENT_MOVE_UP:
    case SGL_EVENT_MOVE_DOWN:
        if (textedit->mode == SGL_TEXTEDIT_MULTI_LINE) {
            int32_t text_height = sgl_font_get_string_height(body_w - 4, textedit->text,
                                                              textedit->font, textedit->line_margin);
            int16_t view_h = body_h - 4;
            bool can_move = (evt->type == SGL_EVENT_MOVE_UP)
                ? ((text_height + textedit->y_offset) > view_h)
                : (textedit->y_offset < 0);
            if (can_move) {
                textedit->y_offset += evt->distance;
            }
            sgl_obj_set_dirty(obj);
        }
        break;

    case SGL_EVENT_CLICKED:
        sgl_textedit_cursor_blink_start(obj);
        sgl_obj_set_dirty(obj);
        break;

    case SGL_EVENT_DESTROYED:
        sgl_textedit_cursor_blink_stop(obj);
        break;

    default:
        break;
    }
}

/**
 * @brief create a textedit object
 * @param parent parent of the textedit
 * @return pointer to the textedit object
 */
sgl_obj_t* sgl_textedit_create(sgl_obj_t* parent)
{
    sgl_textedit_t *textedit = sgl_malloc(sizeof(sgl_textedit_t));
    if (textedit == NULL) {
        SGL_LOG_ERROR("sgl_textedit_create: malloc failed");
        return NULL;
    }

    /* set object all member to zero */
    memset(textedit, 0, sizeof(sgl_textedit_t));

    sgl_obj_t *obj = &textedit->obj;
    sgl_obj_init(&textedit->obj, parent);
    obj->construct_fn = sgl_textedit_construct_cb;
    sgl_obj_set_border_width(obj, SGL_THEME_BORDER_WIDTH);
    obj->focus = 1;

    sgl_obj_set_clickable(obj);
    sgl_obj_set_movable(obj);

    textedit->bg.alpha = SGL_THEME_ALPHA;
    textedit->bg.color = SGL_THEME_COLOR;
    textedit->bg.border_alpha = SGL_THEME_ALPHA;
    textedit->bg.radius = 0;
    textedit->bg.border = 1;
    textedit->bg.border_color = SGL_THEME_BORDER_COLOR;
    textedit->bg.pixmap = NULL;

    textedit->font = sgl_get_system_font();
    textedit->text_color = SGL_THEME_TEXT_COLOR;
    textedit->cursor_color = SGL_THEME_TEXT_COLOR;
    textedit->text = "";
    textedit->text_max_len = 0;
    textedit->mode = SGL_TEXTEDIT_SINGLE_LINE;
    textedit->line_margin = 2;

    return obj;
}

/**
 * @brief set textedit text buffer
 * @param obj textedit object
 * @param buffer text buffer pointer
 * @param max_len buffer max length (including null terminator)
 * @return none
 */
void sgl_textedit_set_text_buffer(sgl_obj_t *obj, char *buffer, int32_t max_len)
{
    sgl_textedit_t *textedit = sgl_container_of(obj, sgl_textedit_t, obj);
    textedit->text = buffer;
    textedit->text_max_len = max_len;
    if (buffer != NULL) {
        buffer[0] = '\0';
    }
    textedit->cursor_pos = 0;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set textedit text
 * @param obj textedit object
 * @param text text string
 * @return none
 */
void sgl_textedit_set_text(sgl_obj_t *obj, const char *text)
{
    sgl_textedit_t *textedit = sgl_container_of(obj, sgl_textedit_t, obj);
    if (textedit->text != NULL && textedit->text_max_len > 0) {
        strncpy(textedit->text, text, textedit->text_max_len - 1);
        textedit->text[textedit->text_max_len - 1] = '\0';
        textedit->cursor_pos = strlen(textedit->text);
    }
    else {
        textedit->text = (char*)text;
    }
    sgl_obj_set_dirty(obj);
}

/**
 * @brief get textedit text
 * @param obj textedit object
 * @return text string pointer
 */
const char* sgl_textedit_get_text(sgl_obj_t *obj)
{
    sgl_textedit_t *textedit = sgl_container_of(obj, sgl_textedit_t, obj);
    return textedit->text;
}

/**
 * @brief set textedit mode
 * @param obj textedit object
 * @param mode SGL_TEXTEDIT_SINGLE_LINE or SGL_TEXTEDIT_MULTI_LINE
 * @return none
 */
void sgl_textedit_set_mode(sgl_obj_t *obj, uint8_t mode)
{
    sgl_textedit_t *textedit = sgl_container_of(obj, sgl_textedit_t, obj);
    textedit->mode = mode;
    textedit->y_offset = 0;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set textedit text color
 * @param obj textedit object
 * @param color text color
 * @return none
 */
void sgl_textedit_set_text_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_textedit_t *textedit = sgl_container_of(obj, sgl_textedit_t, obj);
    textedit->text_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set textedit text font
 * @param obj textedit object
 * @param font text font
 * @return none
 */
void sgl_textedit_set_text_font(sgl_obj_t *obj, const sgl_font_t *font)
{
    sgl_textedit_t *textedit = sgl_container_of(obj, sgl_textedit_t, obj);
    textedit->font = font;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set textedit background color
 * @param obj textedit object
 * @param color background color
 * @return none
 */
void sgl_textedit_set_bg_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_textedit_t *textedit = sgl_container_of(obj, sgl_textedit_t, obj);
    textedit->bg.color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set textedit cursor color
 * @param obj textedit object
 * @param color cursor color
 * @return none
 */
void sgl_textedit_set_cursor_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_textedit_t *textedit = sgl_container_of(obj, sgl_textedit_t, obj);
    textedit->cursor_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set textedit border color
 * @param obj textedit object
 * @param color border color
 * @return none
 */
void sgl_textedit_set_border_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_textedit_t *textedit = sgl_container_of(obj, sgl_textedit_t, obj);
    textedit->bg.border_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set textedit border width
 * @param obj textedit object
 * @param width border width
 * @return none
 */
void sgl_textedit_set_border_width(sgl_obj_t *obj, uint8_t width)
{
    sgl_textedit_t *textedit = sgl_container_of(obj, sgl_textedit_t, obj);
    textedit->bg.border = width;
    sgl_obj_set_border_width(obj, width);
}

/**
 * @brief set textedit radius
 * @param obj textedit object
 * @param radius radius
 * @return none
 */
void sgl_textedit_set_radius(sgl_obj_t *obj, uint8_t radius)
{
    sgl_textedit_t *textedit = sgl_container_of(obj, sgl_textedit_t, obj);
    sgl_obj_set_radius(obj, radius);
    textedit->bg.radius = obj->radius;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set textedit line margin (for multi-line mode)
 * @param obj textedit object
 * @param margin line margin
 * @return none
 */
void sgl_textedit_set_line_margin(sgl_obj_t *obj, uint8_t margin)
{
    sgl_textedit_t *textedit = sgl_container_of(obj, sgl_textedit_t, obj);
    textedit->line_margin = margin;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief insert a character at cursor position
 * @param obj textedit object
 * @param c character to insert
 * @return none
 */
void sgl_textedit_insert_char(sgl_obj_t *obj, char c)
{
    sgl_textedit_t *textedit = sgl_container_of(obj, sgl_textedit_t, obj);
    if (textedit->text == NULL || textedit->text_max_len <= 0) {
        return;
    }

    int32_t len = strlen(textedit->text);
    if (len >= textedit->text_max_len - 1) {
        SGL_LOG_WARN("textedit buffer is full");
        return;
    }

    /* single line mode: ignore newline */
    if (textedit->mode == SGL_TEXTEDIT_SINGLE_LINE && (c == '\n' || c == '\r')) {
        return;
    }

    /* shift text after cursor to make room */
    memmove(textedit->text + textedit->cursor_pos + 1,
            textedit->text + textedit->cursor_pos,
            len - textedit->cursor_pos + 1);

    textedit->text[textedit->cursor_pos] = c;
    textedit->cursor_pos++;

    /* reset cursor blink to show cursor immediately */
    if (textedit->cursor_anim != NULL) {
        textedit->cursor_visible = 1;
    }

    sgl_obj_set_dirty(obj);
}

/**
 * @brief delete character before cursor (backspace)
 * @param obj textedit object
 * @return none
 */
void sgl_textedit_backspace(sgl_obj_t *obj)
{
    sgl_textedit_t *textedit = sgl_container_of(obj, sgl_textedit_t, obj);
    if (textedit->text == NULL || textedit->cursor_pos <= 0) {
        return;
    }

    int32_t len = strlen(textedit->text);
    memmove(textedit->text + textedit->cursor_pos - 1,
            textedit->text + textedit->cursor_pos,
            len - textedit->cursor_pos + 1);
    textedit->cursor_pos--;

    /* reset cursor blink to show cursor immediately */
    if (textedit->cursor_anim != NULL) {
        textedit->cursor_visible = 1;
    }

    sgl_obj_set_dirty(obj);
}

/**
 * @brief move cursor left
 * @param obj textedit object
 * @return none
 */
void sgl_textedit_cursor_left(sgl_obj_t *obj)
{
    sgl_textedit_t *textedit = sgl_container_of(obj, sgl_textedit_t, obj);
    if (textedit->cursor_pos > 0) {
        textedit->cursor_pos--;
        if (textedit->cursor_anim != NULL) {
            textedit->cursor_visible = 1;
        }
        sgl_obj_set_dirty(obj);
    }
}

/**
 * @brief move cursor right
 * @param obj textedit object
 * @return none
 */
void sgl_textedit_cursor_right(sgl_obj_t *obj)
{
    sgl_textedit_t *textedit = sgl_container_of(obj, sgl_textedit_t, obj);
    if (textedit->text != NULL && textedit->text[textedit->cursor_pos] != '\0') {
        textedit->cursor_pos++;
        if (textedit->cursor_anim != NULL) {
            textedit->cursor_visible = 1;
        }
        sgl_obj_set_dirty(obj);
    }
}

/**
 * @brief move cursor up (multi-line mode)
 * @param obj textedit object
 * @return none
 */
void sgl_textedit_cursor_up(sgl_obj_t *obj)
{
    sgl_textedit_t *textedit = sgl_container_of(obj, sgl_textedit_t, obj);
    if (textedit->mode != SGL_TEXTEDIT_MULTI_LINE || textedit->text == NULL) {
        return;
    }

    /* find start of current line */
    int32_t line_start = textedit->cursor_pos;
    while (line_start > 0 && textedit->text[line_start - 1] != '\n') {
        line_start--;
    }

    if (line_start == 0) {
        return; /* already on first line */
    }

    /* find start of previous line */
    int32_t prev_line_start = line_start - 1;
    while (prev_line_start > 0 && textedit->text[prev_line_start - 1] != '\n') {
        prev_line_start--;
    }

    /* calculate column offset in current line */
    int32_t col = textedit->cursor_pos - line_start;

    /* move to same column in previous line, or end of previous line if shorter */
    int32_t prev_line_len = (line_start - 1) - prev_line_start;
    textedit->cursor_pos = prev_line_start + sgl_min(col, prev_line_len);

    if (textedit->cursor_anim != NULL) {
        textedit->cursor_visible = 1;
    }
    sgl_obj_set_dirty(obj);
}

/**
 * @brief move cursor down (multi-line mode)
 * @param obj textedit object
 * @return none
 */
void sgl_textedit_cursor_down(sgl_obj_t *obj)
{
    sgl_textedit_t *textedit = sgl_container_of(obj, sgl_textedit_t, obj);
    if (textedit->mode != SGL_TEXTEDIT_MULTI_LINE || textedit->text == NULL) {
        return;
    }

    /* find start of current line */
    int32_t line_start = textedit->cursor_pos;
    while (line_start > 0 && textedit->text[line_start - 1] != '\n') {
        line_start--;
    }

    /* find end of current line */
    int32_t line_end = textedit->cursor_pos;
    while (textedit->text[line_end] != '\0' && textedit->text[line_end] != '\n') {
        line_end++;
    }

    if (textedit->text[line_end] == '\0') {
        return; /* already on last line */
    }

    /* calculate column offset in current line */
    int32_t col = textedit->cursor_pos - line_start;

    /* find end of next line */
    int32_t next_line_start = line_end + 1;
    int32_t next_line_end = next_line_start;
    while (textedit->text[next_line_end] != '\0' && textedit->text[next_line_end] != '\n') {
        next_line_end++;
    }

    /* move to same column in next line, or end of next line if shorter */
    int32_t next_line_len = next_line_end - next_line_start;
    textedit->cursor_pos = next_line_start + sgl_min(col, next_line_len);

    if (textedit->cursor_anim != NULL) {
        textedit->cursor_visible = 1;
    }
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set cursor position
 * @param obj textedit object
 * @param pos cursor position in text buffer
 * @return none
 */
void sgl_textedit_set_cursor_pos(sgl_obj_t *obj, int32_t pos)
{
    sgl_textedit_t *textedit = sgl_container_of(obj, sgl_textedit_t, obj);
    if (textedit->text == NULL) {
        textedit->cursor_pos = 0;
        return;
    }
    int32_t len = strlen(textedit->text);
    textedit->cursor_pos = sgl_clamp(pos, 0, len);
    if (textedit->cursor_anim != NULL) {
        textedit->cursor_visible = 1;
    }
    sgl_obj_set_dirty(obj);
}

/**
 * @brief get cursor position
 * @param obj textedit object
 * @return cursor position in text buffer
 */
int32_t sgl_textedit_get_cursor_pos(sgl_obj_t *obj)
{
    sgl_textedit_t *textedit = sgl_container_of(obj, sgl_textedit_t, obj);
    return textedit->cursor_pos;
}
