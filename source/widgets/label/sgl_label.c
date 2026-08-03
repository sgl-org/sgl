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
#include <sgl_anim.h>
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
static void sgl_label_update_area(sgl_label_t *label, int16_t text_length, sgl_area_t *area)
{
    sgl_obj_t *obj = &label->obj;
    int16_t up_len = sgl_max(label->text_length, text_length);
    sgl_pos_t pos;
    sgl_size_t obj_size = {
        .h = sgl_obj_get_height(obj),
        .w = sgl_obj_get_width(obj),
    };

    sgl_size_t text_size = {
        .h = sgl_font_get_height(label->font),
        .w = up_len,
    };

    if (label->font && up_len) {
        pos = sgl_get_align_pos(&obj_size, &text_size, (sgl_align_type_t)label->align);
        area->x1 = obj->area.x1 + pos.x + label->offset_x - 1;
        area->x2 = area->x1 + up_len - 1;
        area->y1 = obj->area.y1 + pos.y - 1;
        area->y2 = area->y1 + text_size.h - 1;
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

        sgl_draw_string(surf, &obj->area, align_pos.x + label->offset_x, align_pos.y,
                                            label->text, label->color, label->alpha, label->font);

    } else if (evt->type == SGL_EVENT_DESTROYED) {
        if (label->dynamic) {
            sgl_free((void*)label->text);
        }
#if CONFIG_SGL_ANIMATION
        if (label->long_mode) {
            sgl_anim_delete(sgl_anim_get_by_obj(obj));
        }
#endif
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
    label->font = sgl_get_system_font();
    label->text = "";
    return obj;
}

/**
 * @brief set the text of the label
 * @param obj pointer to the label object
 * @param text pointer to the text
 * @return none
 * @note you must set the font by sgl_label_set_font() before calling this function
 */
void sgl_label_set_text(sgl_obj_t *obj, const char *text)
{
    int16_t text_length;
    sgl_label_t *label = sgl_container_of(obj, sgl_label_t, obj);
    sgl_area_t area = SGL_AREA_INVALID;

    text_length = sgl_font_get_string_width(text, label->font);
    sgl_label_update_area(label, text_length, &area);
    label->text_length = text_length;
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
 * @note you must set the font by sgl_label_set_font() before calling this function
 */
void sgl_label_set_text_fmt(sgl_obj_t *obj, const char *fmt, ...)
{
    va_list args;
    int16_t text_length;
    sgl_area_t area = SGL_AREA_INVALID;
    sgl_label_t *label = sgl_container_of(obj, sgl_label_t, obj);

    if (!label->text) {
        return;
    }

    va_start(args, fmt);
    sgl_vsnprintf(label->text, label->text_capacity, fmt, args);
    va_end(args);
    text_length = sgl_font_get_string_width(label->text, label->font);
    sgl_label_update_area(label, text_length, &area);
    label->text_length = text_length;
    sgl_obj_update_area(&area);
}

/**
 * @brief set the text of the label with format by dynamic memory
 * @param obj pointer to the label object
 * @param text pointer to the text
 * @return none
 * @note you must set the font by sgl_label_set_font() before calling this function
 */
void sgl_label_set_text_fmt_dynamic(sgl_obj_t* obj, const char *fmt, ...)
{
    sgl_label_t *label = sgl_container_of(obj, sgl_label_t, obj);
    sgl_area_t area = SGL_AREA_INVALID;
    int16_t text_length;
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

    text_length = sgl_font_get_string_width(label->text, label->font);
    sgl_label_update_area(label, text_length, &area);
    label->text_length = text_length;
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
 * @return none
 */
void sgl_label_set_text_offset(sgl_obj_t *obj, int8_t offset_x)
{
    sgl_label_t *label = sgl_container_of(obj, sgl_label_t, obj);
    label->offset_x = offset_x;
    sgl_obj_set_dirty(obj);
}

#if CONFIG_SGL_ANIMATION
/**
 * @brief label animation callback
 * @param anim pointer to the animation object
 * @param value animation value
 * @return none
 */
static void label_anim_cb(sgl_anim_t *anim, int32_t value)
{
    sgl_label_t *label = (sgl_label_t*)anim->data;
    sgl_obj_t *obj = &label->obj;
    label->offset_x = sgl_obj_get_width(obj) - value;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set label long mode
 * @param obj pointer to the label object
 * @param speed pixel per second
 * @param flag flag to be set
 * @return none
 */
void sgl_label_set_long_mode(sgl_obj_t *obj, uint32_t speed, bool flag)
{
    sgl_label_t *label = sgl_container_of(obj, sgl_label_t, obj);
    sgl_anim_t *anim;

	int32_t scroll_dist = sgl_obj_get_width(obj) + label->text_length;
    if (speed == 0) speed = 1;
    uint32_t speed_ms = (uint32_t)scroll_dist * 1000 / speed;

    label->align = SGL_ALIGN_LEFT_MID;
    label->offset_x = sgl_obj_get_width(obj);

    if (flag) {
        label->long_mode = 1;
        anim = sgl_anim_get_by_obj(obj);
        if (!anim) {
            anim = sgl_anim_create();
            sgl_anim_set_data(anim, obj);
            sgl_anim_set_start_value(anim, 0);
            sgl_anim_set_end_value(anim, scroll_dist);
            sgl_anim_set_act_duration(anim, speed_ms);
            sgl_anim_set_path(anim, label_anim_cb, SGL_ANIM_PATH_LINEAR);
            sgl_anim_start(anim, SGL_ANIM_REPEAT_LOOP);
        }
    } else {
        if (label->long_mode) {
            sgl_anim_delete(sgl_anim_get_by_obj(obj));
        }
		label->long_mode = 0;
    }
}
#endif
