/* source/widgets/sgl_label_ext.c
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
#include "sgl_label_ext.h"

/**
 * @brief to update text of the label_ext
 * @param label_ext pointer to the label_ext object
 * @return none
 */
static void sgl_label_ext_update_area(sgl_label_ext_t *label_ext, const char *text, sgl_area_t *area)
{
    sgl_pos_t align_pos;

    if (label_ext->font) {
        if (text) {
            align_pos = sgl_get_text_pos(&label_ext->obj.coords, label_ext->font, text, 0, (sgl_align_type_t)label_ext->align);
            area->x1 = align_pos.x;
            area->x2 = area->x1 + sgl_font_get_string_width(text, label_ext->font);
            area->y1 = align_pos.y;
            area->y2 = area->y1 + sgl_font_get_height(label_ext->font);
        }
    }
}

/**
 * @brief construct the label_ext object
 * @param surf pointer to the surface
 * @param obj pointer to the label_ext object
 * @param evt pointer to the event
 * @return none
 */
static void sgl_label_ext_construct_cb(sgl_surf_t *surf, sgl_obj_t* obj, sgl_event_t *evt)
{
    sgl_label_ext_t *label_ext = sgl_container_of(obj, sgl_label_ext_t, obj);
    sgl_pos_t align_pos;

    SGL_ASSERT(label_ext->font != NULL);

    if (evt->type == SGL_EVENT_DRAW_MAIN) {
        if (label_ext->bg_flag) {
            sgl_draw_fill_rect(surf, &obj->area, &obj->coords, obj->radius, label_ext->bg_color, label_ext->alpha);
        }

        align_pos = sgl_get_text_pos(&obj->coords, label_ext->font, label_ext->text, 0, (sgl_align_type_t)label_ext->align);

        const int16_t width = obj->area.x2 - obj->area.x1 + 1;
        const int16_t height = obj->area.y2 - obj->area.y1 + 1;
        const uint32_t buf_size = width * height;

        sgl_color_t *temp_buf = sgl_malloc(buf_size * sizeof(sgl_color_t));
        if (temp_buf == NULL) {
            SGL_LOG_ERROR("sgl_label_ext_construct_cb: malloc rotation temp buffer failed");
            return;
        }

        for (uint32_t i = 0; i < buf_size; i++) {
            temp_buf[i] = label_ext->bg_color;
        }

        sgl_surf_t temp_surf = {
            .x1 = align_pos.x,
            .y1 = align_pos.y,
            .x2 = 0,
            .y2 = 0,
            .buffer = temp_buf,
            .w = width,
            .h = height,
            .dirty = NULL
        };

        sgl_draw_string(&temp_surf, &obj->area, align_pos.x, align_pos.y,
                                            label_ext->text, label_ext->color, label_ext->alpha, label_ext->font);
        sgl_draw_xform_surf(surf, &temp_surf, &obj->parent->area, obj->coords.x1, obj->coords.y1, label_ext->rotation);

        sgl_free(temp_buf);
    } else if (evt->type == SGL_EVENT_DESTROYED) {
        if (label_ext->dynamic) {
            sgl_free((void*)label_ext->text);
        }
    }
}

/**
 * @brief create a label_ext object
 * @param parent parent of the label_ext
 * @return pointer to the label_ext object
 */
sgl_obj_t* sgl_label_ext_create(sgl_obj_t* parent)
{
    sgl_label_ext_t *label_ext = sgl_malloc(sizeof(sgl_label_ext_t));
    if(label_ext == NULL) {
        SGL_LOG_ERROR("sgl_label_ext_create: malloc failed");
        return NULL;
    }

    /* set object all member to zero */
    memset(label_ext, 0, sizeof(sgl_label_ext_t));

    sgl_obj_t *obj = &label_ext->obj;
    sgl_obj_init(&label_ext->obj, parent);
    obj->construct_fn = sgl_label_ext_construct_cb;

    label_ext->alpha = SGL_ALPHA_MAX;
    label_ext->bg_flag = 0;
    label_ext->color = SGL_THEME_TEXT_COLOR;
    label_ext->text = "";
    label_ext->rotation = 0;
    label_ext->font = sgl_get_system_font();

    return obj;
}

/**
 * @brief set the text of the label_ext
 * @param obj pointer to the label_ext object
 * @param text pointer to the text
 * @return none
 */
void sgl_label_ext_set_text(sgl_obj_t *obj, const char *text)
{
    sgl_label_ext_t *label_ext = sgl_container_of(obj, sgl_label_ext_t, obj);
    sgl_area_t area = SGL_AREA_INVALID, new_area = SGL_AREA_INVALID;

    if (label_ext->text && strcmp(text, label_ext->text) == 0) {
        return;
    }

    sgl_label_ext_update_area(label_ext, label_ext->text, &area);
    sgl_label_ext_update_area(label_ext, text, &new_area);
    sgl_area_selfmerge(&area, &new_area);
    label_ext->text = (char*)text;
    sgl_obj_update_area(&area);
}

/**
 * @brief set the text buffer of the label_ext
 * @param obj pointer to the label_ext object
 * @param buf pointer to the text buffer
 * @param buf_size size of the text buffer
 * @return none
 */
void sgl_label_ext_set_text_buffer(sgl_obj_t *obj, char *buf, uint16_t buf_size)
{
    sgl_label_ext_t *label_ext = sgl_container_of(obj, sgl_label_ext_t, obj);
    label_ext->text = buf;
    label_ext->dynamic = 0;
    label_ext->text_capacity = buf_size;
    buf[0] = '\0';
}

/**
 * @brief set the text of the label_ext with format by manual memory
 * @param obj pointer to the label_ext object
 * @param fmt pointer to the text
 * @return none
 * @note the text buffer must be set by sgl_label_ext_set_text_buffer() before calling this function
 */
void sgl_label_ext_set_text_fmt(sgl_obj_t *obj, const char *fmt, ...)
{
    va_list args;
    sgl_area_t area = SGL_AREA_INVALID, new_area = SGL_AREA_INVALID;
    sgl_label_ext_t *label_ext = sgl_container_of(obj, sgl_label_ext_t, obj);

    if (!label_ext->text) {
        return;
    }

    sgl_label_ext_update_area(label_ext, label_ext->text, &area);
    va_start(args, fmt);
    sgl_vsnprintf(label_ext->text, label_ext->text_capacity, fmt, args);
    va_end(args);

    sgl_label_ext_update_area(label_ext, label_ext->text, &new_area);
    sgl_area_selfmerge(&area, &new_area);
    sgl_obj_update_area(&area);
}

/**
 * @brief set the text of the label_ext with format by dynamic memory
 * @param obj pointer to the label_ext object
 * @param text pointer to the text
 * @return none
 */
void sgl_label_ext_set_text_fmt_dynamic(sgl_obj_t* obj, const char *fmt, ...)
{
    sgl_label_ext_t *label_ext = sgl_container_of(obj, sgl_label_ext_t, obj);
    sgl_area_t area = SGL_AREA_INVALID, new_area = SGL_AREA_INVALID;
    va_list args;
    va_list args_copy;
    char *text = label_ext->text;
    int len;
    size_t cap;

    va_start(args, fmt);
    va_copy(args_copy, args);
    len = sgl_vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);
    cap = ((size_t)len + 2) & ~(size_t)1;

    sgl_label_ext_update_area(label_ext, label_ext->text, &area);

    if (label_ext->text_capacity < cap) {
        text = label_ext->dynamic ? sgl_realloc(label_ext->text, cap) : sgl_malloc(cap);
        if (text == NULL) {
            va_end(args);
            SGL_LOG_ERROR("sgl_label_ext_set_text_fmt: alloc failed");
            return;
        }
        text[0] = '\0';
        label_ext->text = text;
        label_ext->dynamic = 1;
        label_ext->text_capacity = cap;
    }

    sgl_vsnprintf(label_ext->text, label_ext->text_capacity, fmt, args);
    va_end(args);

    sgl_label_ext_update_area(label_ext, label_ext->text, &new_area);
    sgl_area_selfmerge(&area, &new_area);
    sgl_obj_update_area(&area);
}

/**
 * @brief update label_ext text area
 * @param obj pointer to the label_ext object
 * @return none
 * @note you can update your label_ext text area when you change the text buffer content
 */
void sgl_label_ext_update_text(sgl_obj_t *obj)
{
    sgl_label_ext_t *label_ext = sgl_container_of(obj, sgl_label_ext_t, obj);
    sgl_area_t area = SGL_AREA_INVALID;
    sgl_label_ext_update_area(label_ext, label_ext->text, &area);
    sgl_obj_update_area(&area);
}

/**
 * @brief get the text of the label_ext
 * @param obj pointer to the label_ext object
 * @return pointer to the text
 */
char* sgl_label_ext_get_text(sgl_obj_t *obj)
{
    sgl_label_ext_t *label_ext = sgl_container_of(obj, sgl_label_ext_t, obj);
    return label_ext->text;
}

/**
 * @brief set label_ext font
 * @param obj pointer to the label_ext object
 * @param font pointer to the font
 * @return none
 */
void sgl_label_ext_set_font(sgl_obj_t *obj, const sgl_font_t *font)
{
    sgl_label_ext_t *label_ext = sgl_container_of(obj, sgl_label_ext_t, obj);
    label_ext->font = font;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set label_ext text color
 * @param obj pointer to the label_ext object
 * @param color color to be set
 * @return none
 */
void sgl_label_ext_set_text_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_label_ext_t *label_ext = sgl_container_of(obj, sgl_label_ext_t, obj);
    label_ext->color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set label_ext background color
 * @param obj pointer to the label_ext object
 * @param color color to be set
 * @return none
 */
void sgl_label_ext_set_bg_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_label_ext_t *label_ext = sgl_container_of(obj, sgl_label_ext_t, obj);
    label_ext->bg_color = color;
    label_ext->bg_flag = 1;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set label_ext radius
 * @param obj pointer to the label_ext object
 * @param radius radius to be set
 * @return none
 */
void sgl_label_ext_set_radius(sgl_obj_t *obj, uint8_t radius)
{
    sgl_obj_set_radius(obj, radius);
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set label_ext text align
 * @param obj pointer to the label_ext object
 * @param align align to be set
 * @return none
 */
void sgl_label_ext_set_text_align(sgl_obj_t *obj, sgl_align_type_t align)
{
    sgl_label_ext_t *label_ext = sgl_container_of(obj, sgl_label_ext_t, obj);
    label_ext->align = align;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set label_ext alpha
 * @param obj pointer to the label_ext object
 * @param alpha alpha to be set
 * @return none
 */
void sgl_label_ext_set_alpha(sgl_obj_t *obj, uint8_t alpha)
{
    sgl_label_ext_t *label_ext = sgl_container_of(obj, sgl_label_ext_t, obj);
    label_ext->alpha = alpha;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set label_ext text rotation
 * @param obj pointer to the label_ext object
 * @param text_rotation text rotation angle (0-360 degree)
 * @return none
 */
void sgl_label_ext_set_text_rotation(sgl_obj_t *obj, int16_t text_rotation)
{
    sgl_label_ext_t *label_ext = sgl_container_of(obj, sgl_label_ext_t, obj);
    label_ext->rotation = text_rotation % 360;
    if (label_ext->rotation < 0) label_ext->rotation += 360;
    sgl_obj_set_dirty(obj);
}
