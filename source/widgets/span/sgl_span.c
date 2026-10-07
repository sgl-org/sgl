/* source/widgets/span/sgl_span.c
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
#include "sgl_span.h"

/**
 * @brief calculate span fragment dimensions
 * @param span pointer to span fragment
 * @return none
 */
static void sgl_span_calc_fragment_size(sgl_span_fragment_t *span)
{
    if (span->text == NULL || span->font == NULL) {
        span->width = 0;
        span->height = 0;
        return;
    }
    
    span->width = sgl_font_get_string_width(span->text, span->font) + 
                  span->padding_left + span->padding_right;
    span->height = sgl_font_get_height(span->font);
}

/**
 * @brief calculate total dimensions of the span widget
 * @param span pointer to span object
 * @return none
 */
static void sgl_span_calc_total_size(sgl_span_t *span)
{
    int16_t total_w = 0;
    int16_t max_h = 0;
    
    for (int i = 0; i < span->span_count; i++) {
        sgl_span_calc_fragment_size(&span->spans[i]);
        total_w += span->spans[i].width;
        if (span->spans[i].height > max_h) {
            max_h = span->spans[i].height;
        }
    }
    
    span->total_width = total_w;
    span->max_line_height = max_h;
    span->total_height = max_h;  /* Will be recalculated for wrap mode */
}

/**
 * @brief draw a single span fragment
 * @param surf pointer to surface
 * @param obj pointer to span object
 * @param span pointer to span fragment
 * @param x x position
 * @param y y position
 * @param clip clip area
 * @return none
 */
static void sgl_span_draw_fragment(sgl_surf_t *surf, sgl_obj_t *obj, 
                                    sgl_span_fragment_t *span,
                                    int16_t x, int16_t y, sgl_area_t *clip)
{
    if (span->text == NULL || span->font == NULL || span->text[0] == '\0') {
        return;
    }
    
    int16_t text_x = x + span->padding_left;
    int16_t text_y = y;
    int16_t text_w = span->width - span->padding_left - span->padding_right;
    int16_t text_h = span->height;
    
    /* Draw background if enabled */
    if (span->bg_flag) {
        sgl_area_t bg_area = {
            .x1 = x,
            .y1 = y,
            .x2 = x + span->width - 1,
            .y2 = y + text_h - 1,
        };
        sgl_area_t bg_clip;
        if (sgl_area_clip(clip, &bg_area, &bg_clip)) {
            sgl_draw_fill_rect(surf, &bg_clip, &bg_area, 0, span->bg_color, span->alpha);
        }
    }
    
    /* Draw text */
    sgl_area_t text_area = {
        .x1 = text_x,
        .y1 = text_y,
        .x2 = text_x + text_w - 1,
        .y2 = text_y + text_h - 1,
    };
    sgl_area_t text_clip;
    if (sgl_area_clip(clip, &text_area, &text_clip)) {
        sgl_draw_string(surf, &text_clip, text_x, text_y, 
                       span->text, span->color, span->alpha, span->font);
    }
}

/**
 * @brief draw spans in clip mode (single line, clip overflow)
 * @param surf pointer to surface
 * @param obj pointer to span object
 * @param clip clip area
 * @return none
 */
static void sgl_span_draw_clip(sgl_surf_t *surf, sgl_obj_t *obj, sgl_area_t *clip)
{
    sgl_span_t *span = sgl_container_of(obj, sgl_span_t, obj);
    int16_t obj_w = sgl_obj_get_width(obj);
    int16_t obj_h = sgl_obj_get_height(obj);
    int16_t start_x = obj->coords.x1;
    int16_t start_y = obj->coords.y1;
    
    /* Calculate starting x based on alignment */
    if (span->align == SGL_SPAN_ALIGN_CENTER) {
        start_x += (obj_w - span->total_width) / 2;
    } else if (span->align == SGL_SPAN_ALIGN_RIGHT) {
        start_x += obj_w - span->total_width;
    }
    
    /* Apply indent for first line */
    start_x += span->indent;
    
    /* Draw each span fragment */
    int16_t cur_x = start_x;
    for (int i = 0; i < span->span_count; i++) {
        sgl_span_fragment_t *frag = &span->spans[i];
        if (frag->text == NULL || frag->text[0] == '\0') {
            continue;
        }
        
        /* Check if this fragment is visible */
        if (cur_x + frag->width > obj->coords.x1 && 
            cur_x < obj->coords.x2) {
            sgl_span_draw_fragment(surf, obj, frag, cur_x, start_y, clip);
        }
        
        cur_x += frag->width;
    }
}

/**
 * @brief draw spans in wrap mode (multi-line)
 * @param surf pointer to surface
 * @param obj pointer to span object
 * @param clip clip area
 * @return none
 */
static void sgl_span_draw_wrap(sgl_surf_t *surf, sgl_obj_t *obj, sgl_area_t *clip)
{
    sgl_span_t *span = sgl_container_of(obj, sgl_span_t, obj);
    int16_t obj_w = sgl_obj_get_width(obj);
    int16_t cur_x = obj->coords.x1 + span->indent;
    int16_t cur_y = obj->coords.y1;
    int16_t line_height = span->max_line_height;
    int16_t line_start_x = cur_x;
    int16_t line_width = 0;
    
    /* First pass: calculate line breaks and total height */
    int16_t total_h = line_height;
    int16_t temp_x = cur_x;
    for (int i = 0; i < span->span_count; i++) {
        sgl_span_fragment_t *frag = &span->spans[i];
        if (frag->text == NULL || frag->text[0] == '\0') {
            continue;
        }
        
        if (temp_x + frag->width > obj->coords.x2 && temp_x > line_start_x) {
            /* Need to wrap */
            total_h += line_height + span->line_space;
            temp_x = obj->coords.x1;
        }
        temp_x += frag->width;
    }
    span->total_height = total_h;
    
    /* Second pass: draw */
    for (int i = 0; i < span->span_count; i++) {
        sgl_span_fragment_t *frag = &span->spans[i];
        if (frag->text == NULL || frag->text[0] == '\0') {
            continue;
        }
        
        /* Check if need to wrap */
        if (cur_x + frag->width > obj->coords.x2 && cur_x > line_start_x) {
            /* Move to next line */
            cur_y += line_height + span->line_space;
            cur_x = obj->coords.x1;
            line_start_x = cur_x;
            line_width = 0;
        }
        
        /* Draw the fragment */
        sgl_span_draw_fragment(surf, obj, frag, cur_x, cur_y, clip);
        
        cur_x += frag->width;
        line_width += frag->width;
    }
}

/**
 * @brief draw spans in scroll mode (horizontal scrolling)
 * @param surf pointer to surface
 * @param obj pointer to span object
 * @param clip clip area
 * @return none
 */
static void sgl_span_draw_scroll(sgl_surf_t *surf, sgl_obj_t *obj, sgl_area_t *clip)
{
    sgl_span_t *span = sgl_container_of(obj, sgl_span_t, obj);
    int16_t obj_w = sgl_obj_get_width(obj);
    int16_t start_x = obj->coords.x1 - span->scroll_offset;
    int16_t start_y = obj->coords.y1;
    
    /* Draw each span fragment */
    int16_t cur_x = start_x;
    for (int i = 0; i < span->span_count; i++) {
        sgl_span_fragment_t *frag = &span->spans[i];
        if (frag->text == NULL || frag->text[0] == '\0') {
            continue;
        }
        
        /* Check if this fragment is visible */
        if (cur_x + frag->width > obj->coords.x1 && 
            cur_x < obj->coords.x2) {
            sgl_span_draw_fragment(surf, obj, frag, cur_x, start_y, clip);
        }
        
        cur_x += frag->width;
    }
}

/**
 * @brief construct the span object
 * @param surf pointer to the surface
 * @param obj pointer to the span object
 * @param evt pointer to the event
 * @return none
 */
static void sgl_span_construct_cb(sgl_surf_t *surf, sgl_obj_t* obj, sgl_event_t *evt)
{
    sgl_span_t *span = sgl_container_of(obj, sgl_span_t, obj);
    sgl_area_t clip;
    
    if (evt->type == SGL_EVENT_DRAW_MAIN) {
        /* Recalculate dimensions */
        sgl_span_calc_total_size(span);
        
        /* Draw background if enabled */
        if (span->bg_flag) {
            sgl_draw_fill_rect(surf, &obj->area, &obj->coords, obj->radius, 
                              span->bg_color, span->alpha);
        }
        
        /* Get clip area */
        if (!sgl_area_clip(&obj->area, &obj->coords, &clip)) {
            return;
        }
        
        /* Draw based on overflow mode */
        switch (span->overflow) {
        case SGL_SPAN_OVERFLOW_WRAP:
            sgl_span_draw_wrap(surf, obj, &clip);
            break;
        case SGL_SPAN_OVERFLOW_SCROLL:
            sgl_span_draw_scroll(surf, obj, &clip);
            break;
        case SGL_SPAN_OVERFLOW_CLIP:
        default:
            sgl_span_draw_clip(surf, obj, &clip);
            break;
        }
    }
}

/**
 * @brief create a span object
 * @param parent parent of the span
 * @return pointer to the span object
 */
sgl_obj_t* sgl_span_create(sgl_obj_t* parent)
{
    sgl_span_t *span = sgl_malloc(sizeof(sgl_span_t));
    if (span == NULL) {
        SGL_LOG_ERROR("sgl_span_create: malloc failed");
        return NULL;
    }

    /* set object all member to zero */
    memset(span, 0, sizeof(sgl_span_t));

    sgl_obj_t *obj = &span->obj;
    sgl_obj_init(&span->obj, parent);
    obj->construct_fn = sgl_span_construct_cb;

    span->alpha = SGL_ALPHA_MAX;
    span->bg_flag = 0;
    span->bg_color = SGL_THEME_COLOR;
    span->align = SGL_SPAN_ALIGN_LEFT;
    span->overflow = SGL_SPAN_OVERFLOW_CLIP;
    span->line_space = 2;
    span->indent = 0;
    span->span_count = 0;

    return obj;
}

/**
 * @brief add a span fragment to the span widget
 * @param obj pointer to the span object
 * @param text text content
 * @param font font for this span (NULL to use default)
 * @param color text color
 * @return span index on success, -1 on failure
 */
int sgl_span_add_span(sgl_obj_t *obj, const char *text, const sgl_font_t *font, sgl_color_t color)
{
    sgl_span_t *span = sgl_container_of(obj, sgl_span_t, obj);
    
    if (span->span_count >= SGL_SPAN_MAX_COUNT) {
        SGL_LOG_ERROR("sgl_span_add_span: max span count reached");
        return -1;
    }
    
    if (text == NULL) {
        SGL_LOG_ERROR("sgl_span_add_span: text is NULL");
        return -1;
    }
    
    int idx = span->span_count;
    sgl_span_fragment_t *frag = &span->spans[idx];
    
    frag->text = text;
    frag->font = font ? font : sgl_get_system_font();
    frag->color = color;
    frag->bg_flag = 0;
    frag->alpha = SGL_ALPHA_MAX;
    frag->padding_left = 0;
    frag->padding_right = 0;
    
    sgl_span_calc_fragment_size(frag);
    span->span_count++;
    
    sgl_obj_set_dirty(obj);
    return idx;
}

/**
 * @brief add a span fragment with background color
 * @param obj pointer to the span object
 * @param text text content
 * @param font font for this span (NULL to use default)
 * @param color text color
 * @param bg_color background color
 * @return span index on success, -1 on failure
 */
int sgl_span_add_span_bg(sgl_obj_t *obj, const char *text, const sgl_font_t *font, 
                          sgl_color_t color, sgl_color_t bg_color)
{
    int idx = sgl_span_add_span(obj, text, font, color);
    if (idx >= 0) {
        sgl_span_t *span = sgl_container_of(obj, sgl_span_t, obj);
        span->spans[idx].bg_color = bg_color;
        span->spans[idx].bg_flag = 1;
    }
    return idx;
}

/**
 * @brief set span fragment padding
 * @param obj pointer to the span object
 * @param index span index
 * @param left left padding in pixels
 * @param right right padding in pixels
 * @return none
 */
void sgl_span_set_span_padding(sgl_obj_t *obj, int index, uint8_t left, uint8_t right)
{
    sgl_span_t *span = sgl_container_of(obj, sgl_span_t, obj);
    
    if (index < 0 || index >= span->span_count) {
        return;
    }
    
    span->spans[index].padding_left = left;
    span->spans[index].padding_right = right;
    sgl_span_calc_fragment_size(&span->spans[index]);
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set span fragment alpha
 * @param obj pointer to the span object
 * @param index span index
 * @param alpha alpha value (0-255)
 * @return none
 */
void sgl_span_set_span_alpha(sgl_obj_t *obj, int index, uint8_t alpha)
{
    sgl_span_t *span = sgl_container_of(obj, sgl_span_t, obj);
    
    if (index < 0 || index >= span->span_count) {
        return;
    }
    
    span->spans[index].alpha = alpha;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief clear all spans
 * @param obj pointer to the span object
 * @return none
 */
void sgl_span_clear(sgl_obj_t *obj)
{
    sgl_span_t *span = sgl_container_of(obj, sgl_span_t, obj);
    span->span_count = 0;
    span->total_width = 0;
    span->total_height = 0;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set span widget alignment
 * @param obj pointer to the span object
 * @param align alignment type
 * @return none
 */
void sgl_span_set_align(sgl_obj_t *obj, sgl_span_align_t align)
{
    sgl_span_t *span = sgl_container_of(obj, sgl_span_t, obj);
    span->align = align;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set span widget overflow mode
 * @param obj pointer to the span object
 * @param overflow overflow mode
 * @return none
 */
void sgl_span_set_overflow(sgl_obj_t *obj, sgl_span_overflow_t overflow)
{
    sgl_span_t *span = sgl_container_of(obj, sgl_span_t, obj);
    span->overflow = overflow;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set line space for wrapped text
 * @param obj pointer to the span object
 * @param line_space line space in pixels
 * @return none
 */
void sgl_span_set_line_space(sgl_obj_t *obj, uint8_t line_space)
{
    sgl_span_t *span = sgl_container_of(obj, sgl_span_t, obj);
    span->line_space = line_space;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set first line indent
 * @param obj pointer to the span object
 * @param indent indent in pixels
 * @return none
 */
void sgl_span_set_indent(sgl_obj_t *obj, uint8_t indent)
{
    sgl_span_t *span = sgl_container_of(obj, sgl_span_t, obj);
    span->indent = indent;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set span widget background color
 * @param obj pointer to the span object
 * @param color background color
 * @return none
 */
void sgl_span_set_bg_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_span_t *span = sgl_container_of(obj, sgl_span_t, obj);
    span->bg_color = color;
    span->bg_flag = 1;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set span widget background transparent
 * @param obj pointer to the span object
 * @return none
 */
void sgl_span_set_bg_transparent(sgl_obj_t *obj)
{
    sgl_span_t *span = sgl_container_of(obj, sgl_span_t, obj);
    span->bg_flag = 0;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set span widget alpha
 * @param obj pointer to the span object
 * @param alpha alpha value (0-255)
 * @return none
 */
void sgl_span_set_alpha(sgl_obj_t *obj, uint8_t alpha)
{
    sgl_span_t *span = sgl_container_of(obj, sgl_span_t, obj);
    span->alpha = alpha;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set span widget radius
 * @param obj pointer to the span object
 * @param radius corner radius
 * @return none
 */
void sgl_span_set_radius(sgl_obj_t *obj, uint8_t radius)
{
    sgl_obj_set_radius(obj, radius);
    sgl_obj_set_dirty(obj);
}

/**
 * @brief get total width of all spans
 * @param obj pointer to the span object
 * @return total width in pixels
 */
int16_t sgl_span_get_total_width(sgl_obj_t *obj)
{
    sgl_span_t *span = sgl_container_of(obj, sgl_span_t, obj);
    sgl_span_calc_total_size(span);
    return span->total_width;
}

/**
 * @brief get total height of the span widget
 * @param obj pointer to the span object
 * @return total height in pixels
 */
int16_t sgl_span_get_total_height(sgl_obj_t *obj)
{
    sgl_span_t *span = sgl_container_of(obj, sgl_span_t, obj);
    sgl_span_calc_total_size(span);
    return span->total_height;
}
