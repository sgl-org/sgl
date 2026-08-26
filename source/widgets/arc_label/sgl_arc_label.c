/* source/widgets/arc_label/sgl_arc_label.c
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
#include "sgl_arc_label.h"

/**
 * @brief to update text area of the arc label
 * @param label pointer to the arc label object
 * @param text pointer to the text
 * @param area pointer to the area to update
 * @return none
 */
static void sgl_arc_label_update_area(sgl_arc_label_t *label, const char *text, sgl_area_t *area)
{
    sgl_pos_t align_pos;

    if (label->font) {
        if (text) {
            align_pos = sgl_get_text_pos(&label->obj.coords, label->font, text, 0, (sgl_align_type_t)label->align);
            area->x1 = align_pos.x + label->transform.offset.offset_x;
            area->x2 = area->x1 + sgl_font_get_string_width(text, label->font);
            area->y1 = align_pos.y;
            area->y2 = area->y1 + sgl_font_get_height(label->font);
        }
    }
}


/**
 * @brief calculate rotated bounding box and update object position/size
 * @param label pointer to the arc label object
 * @return none
 */
static void sgl_arc_label_update_rotation_bounds(sgl_arc_label_t *label)
{
    int16_t angle = label->angle;
    int16_t w = label->orig_w;
    int16_t h = label->orig_h;

    // For 0, 180 degrees: width = w, height = h
    // For 90, 270 degrees: width = h, height = w
    // For other angles: need to calculate rotated bounding box

    if (angle == 0 || angle == 180) {
        // Horizontal orientation - no size swap
        sgl_obj_set_pos(&label->obj, label->orig_x, label->orig_y);
        sgl_obj_set_size(&label->obj, w, h);
    } else if (angle == 90 || angle == 270) {
        // Vertical orientation - swap width and height
        // Calculate new position: center the rotated widget at original position
        int16_t new_x = label->orig_x + (w - h) / 2;
        int16_t new_y = label->orig_y + (h - w) / 2;
        sgl_obj_set_pos(&label->obj, new_x, new_y);
        sgl_obj_set_size(&label->obj, h, w);
    } else {
        // General case: calculate rotated bounding box
        // Bounding box size after rotation: max(|w*cos| + |h*sin|, |w*sin| + |h*cos|)
        int32_t sin_val = sgl_sin(angle);
        int32_t cos_val = sgl_cos(angle);
        
        // |cos| and |sin| as fixed point
        int32_t abs_cos = cos_val < 0 ? -cos_val : cos_val;
        int32_t abs_sin = sin_val < 0 ? -sin_val : sin_val;
        
        // width * |cos| + height * |sin|
        int32_t bound_w = ((int32_t)w * abs_cos + (int32_t)h * abs_sin) / SGL_SIN_FIXED_ONE;
        // width * |sin| + height * |cos|
        int32_t bound_h = ((int32_t)w * abs_sin + (int32_t)h * abs_cos) / SGL_SIN_FIXED_ONE;
        
        // Calculate centered position
        int16_t new_x = label->orig_x + (w - (int16_t)bound_w) / 2;
        int16_t new_y = label->orig_y + (h - (int16_t)bound_h) / 2;
        
        sgl_obj_set_pos(&label->obj, new_x, new_y);
        sgl_obj_set_size(&label->obj, (int16_t)bound_w, (int16_t)bound_h);
    }
}


/**
 * @brief construct the arc label object
 * @param surf pointer to the surface
 * @param obj pointer to the object
 * @param evt pointer to the event
 * @return none
 */
static void sgl_arc_label_construct_cb(sgl_surf_t *surf, sgl_obj_t* obj, sgl_event_t *evt)
{
    sgl_arc_label_t *label = sgl_container_of(obj, sgl_arc_label_t, obj);
    sgl_pos_t align_pos;

    SGL_ASSERT(label->font != NULL);

    if (evt->type == SGL_EVENT_DRAW_MAIN) {
        if (label->bg_flag) {
            sgl_draw_fill_rect(surf, &obj->area, &obj->coords, obj->radius, label->bg_color, label->alpha);
        }

        align_pos = sgl_get_text_pos(&obj->coords, label->font, label->text, 0, (sgl_align_type_t)label->align);

        if (label->rota == 0) {
            // No rotation - direct draw
            sgl_draw_string(surf, &obj->area, align_pos.x + label->transform.offset.offset_x,
                                              align_pos.y + label->transform.offset.offset_y,
                                              label->text, label->color, label->alpha, label->font);
        }
                else {
            // Rotation needed - use text dimensions with margin for font offset
            const int16_t text_width = sgl_font_get_string_width(label->text, label->font);
            const int16_t text_height = sgl_font_get_height(label->font);
            const int16_t margin = text_height * 2;  // 增大边距
            const int16_t buf_w = text_width + margin * 2;
            const int16_t buf_h = text_height + margin * 2;
            const uint32_t buf_size = buf_w * buf_h;

            sgl_color_t *temp_buf = sgl_malloc(buf_size * sizeof(sgl_color_t));
            if (temp_buf == NULL) {
                SGL_LOG_ERROR("sgl_arc_label_construct_cb: malloc rotation temp buffer failed");
                return;
            }

            for (uint32_t i = 0; i < buf_size; i++) {
                temp_buf[i] = label->bg_color;
            }

            // Create temporary surface with local coordinates (0,0)
            sgl_surf_t temp_surf = {
                .x1 = 0,
                .y1 = 0,
                .x2 = buf_w - 1,
                .y2 = buf_h - 1,
                .buffer = temp_buf,
                .w = buf_w,
                .h = buf_h,
                .dirty = NULL
            };

            sgl_area_t temp_area = {
                .x1 = 0,
                .y1 = 0,
                .x2 = buf_w - 1,
                .y2 = buf_h - 1
            };

            SGL_LOG_INFO("text: %s, text_w: %d, text_h: %d, buf: %dx%d", 
                         label->text, text_width, text_height, buf_w, buf_h);
            SGL_LOG_INFO("font: base_line=%d, font_height=%d", 
                         label->font->base_line, label->font->font_height);

            sgl_draw_string(&temp_surf, &temp_area, margin, margin,
                                              label->text, label->color, label->alpha, label->font);

            int16_t center_x = obj->coords.x1 + (obj->coords.x2 - obj->coords.x1) / 2;
            int16_t center_y = obj->coords.y1 + (obj->coords.y2 - obj->coords.y1) / 2;
            
            sgl_draw_xform_surf(surf, &temp_surf, &obj->parent->area, 
                               center_x - buf_w / 2,
                               center_y - buf_h / 2, 
                               label->transform.rotation);

            sgl_free(temp_buf);
        }
    } else if (evt->type == SGL_EVENT_DESTROYED) {
        if (label->dynamic) {
            sgl_free((void*)label->text);
        }
    }
}


/**
 * @brief create an arc label object
 * @param parent parent of the arc label
 * @return pointer to the arc label object
 */
sgl_obj_t* sgl_arc_label_create(sgl_obj_t* parent)
{
    sgl_arc_label_t *label = sgl_malloc(sizeof(sgl_arc_label_t));
    if(label == NULL) {
        SGL_LOG_ERROR("sgl_arc_label_create: malloc failed");
        return NULL;
    }

    /* set object all member to zero */
    memset(label, 0, sizeof(sgl_arc_label_t));

    sgl_obj_t *obj = &label->obj;
    sgl_obj_init(&label->obj, parent);
    obj->construct_fn = sgl_arc_label_construct_cb;

    label->alpha = SGL_ALPHA_MAX;
    label->bg_flag = 0;
    label->color = SGL_THEME_TEXT_COLOR;
    label->text = "";
    label->transform.rotation = 0;
    label->rota = 0;
    label->angle = 0;
    label->font = sgl_get_system_font();
    label->orig_x = 0;
    label->orig_y = 0;
    label->orig_w = 0;
    label->orig_h = 0;

    return obj;
}


/**
 * @brief set the text of the arc label
 * @param obj pointer to the arc label object
 * @param text pointer to the text
 * @return none
 */
void sgl_arc_label_set_text(sgl_obj_t *obj, const char *text)
{
    sgl_arc_label_t *label = sgl_container_of(obj, sgl_arc_label_t, obj);
    sgl_area_t area = SGL_AREA_INVALID, new_area = SGL_AREA_INVALID;

    if ((label != NULL) && (strlen(label->text) != strlen(text))) {
        sgl_arc_label_update_area(label, label->text, &area);
        sgl_arc_label_update_area(label, text, &new_area);
        sgl_area_selfmerge(&area, &new_area);
    }
    else {
        sgl_arc_label_update_area(label, text, &area);
    }
    label->text = (char*)text;
    sgl_obj_update_area(&area);
}


/**
 * @brief set the text buffer of the arc label
 * @param obj pointer to the arc label object
 * @param buf pointer to the text buffer
 * @param buf_size size of the text buffer
 * @return none
 */
void sgl_arc_label_set_text_buffer(sgl_obj_t *obj, char *buf, uint16_t buf_size)
{
    sgl_arc_label_t *label = sgl_container_of(obj, sgl_arc_label_t, obj);
    label->text = buf;
    label->dynamic = 0;
    label->text_capacity = buf_size;
    buf[0] = '\0';
}


/**
 * @brief set the text of the arc label with format by manual memory
 * @param obj pointer to the arc label object
 * @param fmt pointer to the text
 * @return none
 * @note the text buffer must be set by sgl_arc_label_set_text_buffer() before calling this function
 */
void sgl_arc_label_set_text_fmt(sgl_obj_t *obj, const char *fmt, ...)
{
    va_list args;
    sgl_area_t area = SGL_AREA_INVALID, new_area = SGL_AREA_INVALID;
    sgl_arc_label_t *label = sgl_container_of(obj, sgl_arc_label_t, obj);

    if (!label->text) {
        return;
    }

    sgl_arc_label_update_area(label, label->text, &area);
    va_start(args, fmt);
    sgl_vsnprintf(label->text, label->text_capacity, fmt, args);
    va_end(args);

    sgl_arc_label_update_area(label, label->text, &new_area);
    sgl_area_selfmerge(&area, &new_area);
    sgl_obj_update_area(&area);
}


/**
 * @brief set the text of the arc label with format by dynamic memory
 * @param obj pointer to the arc label object
 * @param text pointer to the text
 * @return none
 */
void sgl_arc_label_set_text_fmt_dynamic(sgl_obj_t* obj, const char *fmt, ...)
{
    sgl_arc_label_t *label = sgl_container_of(obj, sgl_arc_label_t, obj);
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
    cap = ((size_t)len + 2) & ~(size_t)1;

    sgl_arc_label_update_area(label, label->text, &area);

    if (label->text_capacity < cap) {
        text = label->dynamic ? sgl_realloc(label->text, cap) : sgl_malloc(cap);
        if (text == NULL) {
            va_end(args);
            SGL_LOG_ERROR("sgl_arc_label_set_text_fmt: alloc failed");
            return;
        }
        text[0] = '\0';
        label->text = text;
        label->dynamic = 1;
        label->text_capacity = cap;
    }

    sgl_vsnprintf(label->text, label->text_capacity, fmt, args);
    va_end(args);

    sgl_arc_label_update_area(label, label->text, &new_area);
    sgl_area_selfmerge(&area, &new_area);
    sgl_obj_update_area(&area);
}


/**
 * @brief update arc label text area
 * @param obj pointer to the arc label object
 * @return none
 * @note you can update your arc label text area when you change the text buffer content
 */
void sgl_arc_label_update_text(sgl_obj_t *obj)
{
    sgl_arc_label_t *label = sgl_container_of(obj, sgl_arc_label_t, obj);
    sgl_area_t area = SGL_AREA_INVALID;
    sgl_arc_label_update_area(label, label->text, &area);
    sgl_obj_update_area(&area);
}


/**
 * @brief get the text of the arc label
 * @param obj pointer to the arc label object
 * @return pointer to the text
 */
char* sgl_arc_label_get_text(sgl_obj_t *obj)
{
    sgl_arc_label_t *label = sgl_container_of(obj, sgl_arc_label_t, obj);
    return label->text;
}


/**
 * @brief set arc label font
 * @param obj pointer to the arc label object
 * @param font pointer to the font
 * @return none
 */
void sgl_arc_label_set_font(sgl_obj_t *obj, const sgl_font_t *font)
{
    sgl_arc_label_t *label = sgl_container_of(obj, sgl_arc_label_t, obj);
    label->font = font;
    sgl_obj_set_dirty(obj);
}


/**
 * @brief set arc label text color
 * @param obj pointer to the arc label object
 * @param color color to be set
 * @return none
 */
void sgl_arc_label_set_text_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_arc_label_t *label = sgl_container_of(obj, sgl_arc_label_t, obj);
    label->color = color;
    sgl_obj_set_dirty(obj);
}


/**
 * @brief set arc label background color
 * @param obj pointer to the arc label object
 * @param color color to be set
 * @return none
 */
void sgl_arc_label_set_bg_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_arc_label_t *label = sgl_container_of(obj, sgl_arc_label_t, obj);
    label->bg_color = color;
    label->bg_flag = 1;
    sgl_obj_set_dirty(obj);
}


/**
 * @brief set arc label radius
 * @param obj pointer to the arc label object
 * @param radius radius to be set
 * @return none
 */
void sgl_arc_label_set_radius(sgl_obj_t *obj, uint8_t radius)
{
    sgl_obj_set_radius(obj, radius);
    sgl_obj_set_dirty(obj);
}


/**
 * @brief set arc label text align
 * @param obj pointer to the arc label object
 * @param align align to be set
 * @return none
 */
void sgl_arc_label_set_text_align(sgl_obj_t *obj, sgl_align_type_t align)
{
    sgl_arc_label_t *label = sgl_container_of(obj, sgl_arc_label_t, obj);
    label->align = align;
    sgl_obj_set_dirty(obj);
}


/**
 * @brief set arc label alpha
 * @param obj pointer to the arc label object
 * @param alpha alpha to be set
 * @return none
 */
void sgl_arc_label_set_alpha(sgl_obj_t *obj, uint8_t alpha)
{
    sgl_arc_label_t *label = sgl_container_of(obj, sgl_arc_label_t, obj);
    label->alpha = alpha;
    sgl_obj_set_dirty(obj);
}


/**
 * @brief set arc label text offset
 * @param obj pointer to the arc label object
 * @param offset_x offset_x to be set
 * @param offset_y offset_y to be set
 * @return none
 */
void sgl_arc_label_set_text_offset(sgl_obj_t *obj, int8_t offset_x, int8_t offset_y)
{
    sgl_arc_label_t *label = sgl_container_of(obj, sgl_arc_label_t, obj);
    label->transform.offset.offset_x = offset_x;
    label->transform.offset.offset_y = offset_y;
    sgl_obj_set_dirty(obj);
}


/**
 * @brief set arc label original position (before rotation)
 * @param obj pointer to the arc label object
 * @param x x coordinate
 * @param y y coordinate
 * @return none
 */
void sgl_arc_label_set_orig_pos(sgl_obj_t *obj, int16_t x, int16_t y)
{
    sgl_arc_label_t *label = sgl_container_of(obj, sgl_arc_label_t, obj);
    label->orig_x = x;
    label->orig_y = y;
    
    // Update position based on current rotation angle
    sgl_arc_label_update_rotation_bounds(label);
    sgl_obj_set_dirty(obj);
}


/**
 * @brief set arc label original size (before rotation)
 * @param obj pointer to the arc label object
 * @param w width
 * @param h height
 * @return none
 */
void sgl_arc_label_set_orig_size(sgl_obj_t *obj, int16_t w, int16_t h)
{
    sgl_arc_label_t *label = sgl_container_of(obj, sgl_arc_label_t, obj);
    label->orig_w = w;
    label->orig_h = h;
    
    // Update size based on current rotation angle
    sgl_arc_label_update_rotation_bounds(label);
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set arc label text rotation angle
 * @param obj pointer to the arc label object
 * @param angle rotation angle (0-360 degrees)
 * @return none
 * 
 * @note The rotation angle affects how the text is displayed:
 *       - 0 degrees: Horizontal display (left-to-right)
 *       - 90 degrees: Vertical display (bottom-to-top, first character at bottom)
 *       - 180 degrees: Horizontal flipped (right-to-left)
 *       - 270 degrees: Vertical display (top-to-bottom, first character at top)
 */
void sgl_arc_label_set_angle(sgl_obj_t *obj, int16_t angle)
{
    sgl_arc_label_t *label = sgl_container_of(obj, sgl_arc_label_t, obj);
    
    SGL_LOG_INFO("Setting angle: %d", angle);
    
    label->angle = angle % 360;
    if (label->angle < 0) {
        label->angle += 360;
    }
    
    label->transform.rotation = label->angle;
    label->rota = label->angle ? 1 : 0;
    
    SGL_LOG_INFO("After set: angle=%d, rotation=%d, rota=%d", 
                 label->angle, label->transform.rotation, label->rota);
    
    sgl_arc_label_update_rotation_bounds(label);
    sgl_obj_set_dirty(obj);
}

/**
 * @brief get arc label current rotation angle
 * @param obj pointer to the arc label object
 * @return current rotation angle (0-360)
 */
int16_t sgl_arc_label_get_angle(sgl_obj_t *obj)
{
    sgl_arc_label_t *label = sgl_container_of(obj, sgl_arc_label_t, obj);
    return label->angle;
}
