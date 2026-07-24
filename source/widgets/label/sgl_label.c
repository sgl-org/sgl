/* source/widgets/sgl_label.c
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
#include <string.h>
#include "sgl_label.h"

/**
 * @brief to update text of the label
 * @param label pointer to the label object
 * @return none
 */
static void sgl_label_update_area(sgl_label_t *label, const char *text, sgl_area_t *area)
{
    sgl_pos_t align_pos;

    if (label->font) {
        if (text) {
            align_pos = sgl_get_text_pos(&label->obj.coords, label->font, text, 0, (sgl_align_type_t)label->align);
            area->x1 = align_pos.x + label->offset_x - 1;
            area->x2 = area->x1 + sgl_font_get_string_width(text, label->font);
            area->y1 = align_pos.y + label->offset_y - 1;
            area->y2 = area->y1 + sgl_font_get_height(label->font);
        }
    }
}

/**
 * @brief construct the label object
 * @param surf pointer to the surface
 * @param obj pointer to the label object
 * @param evt pointer to the event
 * @return none
 */
static void sgl_label_construct_cb(sgl_surf_t *surf, sgl_obj_t* obj, sgl_event_t *evt)
{
    sgl_label_t *label = sgl_container_of(obj, sgl_label_t, obj);
    sgl_pos_t align_pos;

    SGL_ASSERT(label->font != NULL);

    if (evt->type == SGL_EVENT_DRAW_MAIN) {
        if (label->bg_flag) {
            sgl_draw_fill_rect(surf, &obj->area, &obj->coords, obj->radius, label->bg_color, label->alpha);
        }

        align_pos = sgl_get_text_pos(&obj->coords, label->font, label->text, 0, (sgl_align_type_t)label->align);

        sgl_draw_string(surf, &obj->area, align_pos.x + label->offset_x, 
                                            align_pos.y + label->offset_y, 
                                            label->text, label->color, label->alpha, label->font);

    } else if (evt->type == SGL_EVENT_DESTROYED) {
        if (label->dynamic) {
            sgl_free((void*)label->text);
        }
    }
}

/**
 * @brief create a label object
 * @param parent parent of the label
 * @return pointer to the label object
 */
sgl_obj_t* sgl_label_create(sgl_obj_t* parent)
{
    sgl_label_t *label = sgl_malloc(sizeof(sgl_label_t));
    if(label == NULL) {
        SGL_LOG_ERROR("sgl_label_create: malloc failed");
        return NULL;
    }

    /* set object all member to zero */
    memset(label, 0, sizeof(sgl_label_t));

    sgl_obj_t *obj = &label->obj;
    sgl_obj_init(&label->obj, parent);
    obj->construct_fn = sgl_label_construct_cb;

    label->alpha = SGL_ALPHA_MAX;
    label->bg_flag = 0;
    label->color = SGL_THEME_TEXT_COLOR;
    label->text = "";
    label->font = sgl_get_system_font();

    return obj;
}

/**
 * @brief set the text of the label
 * @param obj pointer to the label object
 * @param text pointer to the text
 * @return none
 */
void sgl_label_set_text(sgl_obj_t *obj, const char *text)
{
    sgl_label_t *label = sgl_container_of(obj, sgl_label_t, obj);
    sgl_area_t area = SGL_AREA_INVALID, new_area = SGL_AREA_INVALID;

    if (label->text && strcmp(text, label->text) == 0) {
        return;
    }

    sgl_label_update_area(label, label->text, &area);
    sgl_label_update_area(label, text, &new_area);
    sgl_area_selfmerge(&area, &new_area);
    label->text = (char*)text;
    sgl_obj_update_area(&area);
}

/**
 * @brief set the text buffer of the label
 * @param obj pointer to the label object
 * @param buf pointer to the text buffer
 * @param buf_size size of the text buffer
 * @return none
 */
void sgl_label_set_text_buffer(sgl_obj_t *obj, char *buf, uint16_t buf_size)
{
    sgl_label_t *label = sgl_container_of(obj, sgl_label_t, obj);
    label->text = buf;
    label->dynamic = 0;
    label->text_capacity = buf_size;
    buf[0] = '\0';
}

/**
 * @brief set the text of the label with format by manual memory
 * @param obj pointer to the label object
 * @param fmt pointer to the text
 * @return none
 * @note the text buffer must be set by sgl_label_set_text_buffer() before calling this function
 */
void sgl_label_set_text_fmt(sgl_obj_t *obj, const char *fmt, ...)
{
    va_list args;
    sgl_area_t area = SGL_AREA_INVALID, new_area = SGL_AREA_INVALID;
    sgl_label_t *label = sgl_container_of(obj, sgl_label_t, obj);

    if (!label->text) {
        return;
    }

    sgl_label_update_area(label, label->text, &area);
    va_start(args, fmt);
    sgl_vsnprintf(label->text, label->text_capacity, fmt, args);
    va_end(args);

    sgl_label_update_area(label, label->text, &new_area);
    sgl_area_selfmerge(&area, &new_area);
    sgl_obj_update_area(&area);
}

/**
 * @brief set the text of the label with format by dynamic memory
 * @param obj pointer to the label object
 * @param text pointer to the text
 * @return none
 */
void sgl_label_set_text_fmt_dynamic(sgl_obj_t* obj, const char *fmt, ...)
{
    sgl_label_t *label = sgl_container_of(obj, sgl_label_t, obj);
    sgl_area_t area = SGL_AREA_INVALID, new_area = SGL_AREA_INVALID;
    va_list args;
    va_list args_copy;
    char *text = label->text;
    int len;
    size_t cap;

    va_start(args, fmt);
    va_copy(args_copy, args);
    len = sgl_vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);
    cap = ((size_t)len + 4) & ~(size_t)3;

    sgl_label_update_area(label, label->text, &area);

    if (label->text_capacity < cap) {
        text = label->dynamic ? sgl_realloc(label->text, cap) : sgl_malloc(cap);
        if (text == NULL) {
            va_end(args);
            SGL_LOG_ERROR("sgl_label_set_text_fmt: alloc failed");
            return;
        }
        text[0] = '\0';
        label->text = text;
        label->dynamic = 1;
        label->text_capacity = cap;
    }

    sgl_vsnprintf(label->text, label->text_capacity, fmt, args);
    va_end(args);

    sgl_label_update_area(label, label->text, &new_area);
    sgl_area_selfmerge(&area, &new_area);
    sgl_obj_update_area(&area);
}

/**
 * @brief update label text area
 * @param obj pointer to the label object
 * @return none
 * @note you can update your label text area when you change the text buffer content
 */
void sgl_label_update_text(sgl_obj_t *obj)
{
    sgl_label_t *label = sgl_container_of(obj, sgl_label_t, obj);
    sgl_area_t area = SGL_AREA_INVALID;
    sgl_label_update_area(label, label->text, &area);
    sgl_obj_update_area(&area);
}

/**
 * @brief get the text of the label
 * @param obj pointer to the label object
 * @return pointer to the text
 */
char* sgl_label_get_text(sgl_obj_t *obj)
{
    sgl_label_t *label = sgl_container_of(obj, sgl_label_t, obj);
    return label->text;
}

/**
 * @brief set label font
 * @param obj pointer to the label object
 * @param font pointer to the font
 * @return none
 */
void sgl_label_set_font(sgl_obj_t *obj, const sgl_font_t *font)
{
    sgl_label_t *label = sgl_container_of(obj, sgl_label_t, obj);
    label->font = font;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set label text color
 * @param obj pointer to the label object
 * @param color color to be set
 * @return none
 */
void sgl_label_set_text_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_label_t *label = sgl_container_of(obj, sgl_label_t, obj);
    label->color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set label background color
 * @param obj pointer to the label object
 * @param color color to be set
 * @return none
 */
void sgl_label_set_bg_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_label_t *label = sgl_container_of(obj, sgl_label_t, obj);
    label->bg_color = color;
    label->bg_flag = 1;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set label radius
 * @param obj pointer to the label object
 * @param radius radius to be set
 * @return none
 */
void sgl_label_set_radius(sgl_obj_t *obj, uint8_t radius)
{
    sgl_obj_set_radius(obj, radius);
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set label text align
 * @param obj pointer to the label object
 * @param align align to be set
 * @return none
 */
void sgl_label_set_text_align(sgl_obj_t *obj, sgl_align_type_t align)
{
    sgl_label_t *label = sgl_container_of(obj, sgl_label_t, obj);
    label->align = align;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set label alpha
 * @param obj pointer to the label object
 * @param alpha alpha to be set
 * @return none
 */
void sgl_label_set_alpha(sgl_obj_t *obj, uint8_t alpha)
{
    sgl_label_t *label = sgl_container_of(obj, sgl_label_t, obj);
    label->alpha = alpha;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set label text offset
 * @param obj pointer to the label object
 * @param offset_x offset_x to be set
 * @param offset_y offset_y to be set
 * @return none
 */
void sgl_label_set_text_offset(sgl_obj_t *obj, int8_t offset_x, int8_t offset_y)
{
    sgl_label_t *label = sgl_container_of(obj, sgl_label_t, obj);
    label->offset_x = offset_x;
    label->offset_y = offset_y;
    sgl_obj_set_dirty(obj);
}
