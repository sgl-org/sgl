/* source/widgets/span/sgl_span.h
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

#ifndef __SGL_SPAN_H__
#define __SGL_SPAN_H__

#include <sgl_core.h>
#include <sgl_draw.h>
#include <sgl_math.h>
#include <sgl_log.h>
#include <sgl_mm.h>
#include <sgl_cfgfix.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Maximum number of spans in a span widget
 */
#define SGL_SPAN_MAX_COUNT      8

/**
 * @brief Span text alignment
 */
typedef enum sgl_span_align {
    SGL_SPAN_ALIGN_LEFT = 0,    /* Left aligned */
    SGL_SPAN_ALIGN_CENTER,      /* Center aligned */
    SGL_SPAN_ALIGN_RIGHT,       /* Right aligned */
} sgl_span_align_t;

/**
 * @brief Span overflow mode
 */
typedef enum sgl_span_overflow {
    SGL_SPAN_OVERFLOW_CLIP = 0, /* Clip the text that exceeds the width */
    SGL_SPAN_OVERFLOW_WRAP,     /* Wrap text to next line */
    SGL_SPAN_OVERFLOW_SCROLL,   /* Scroll the text horizontally */
} sgl_span_overflow_t;

/**
 * @brief Single span fragment structure
 * @text: text content of this span
 * @font: font used for this span
 * @color: text color of this span
 * @bg_color: background color of this span (optional)
 * @bg_flag: whether to draw background
 * @alpha: alpha value of this span
 * @padding_left: left padding in pixels
 * @padding_right: right padding in pixels
 */
typedef struct sgl_span_fragment {
    const char       *text;
    const sgl_font_t *font;
    sgl_color_t      color;
    sgl_color_t      bg_color;
    uint8_t          alpha;
    uint8_t          bg_flag : 1;
    uint8_t          padding_left;
    uint8_t          padding_right;
    int16_t          width;        /* cached width of this span */
    int16_t          height;       /* cached height of this span */
} sgl_span_fragment_t;

/**
 * @brief sgl span object
 * @obj: sgl general object
 * @spans: array of span fragments
 * @span_count: number of active spans
 * @align: text alignment
 * @overflow: overflow mode
 * @line_space: space between lines when wrapping
 * @indent: first line indent in pixels
 * @bg_color: background color of the whole widget
 * @bg_flag: whether to draw widget background
 * @alpha: overall alpha
 * @scroll_offset: current scroll offset for scroll mode
 */
typedef struct sgl_span {
    sgl_obj_t           obj;
    sgl_span_fragment_t spans[SGL_SPAN_MAX_COUNT];
    uint8_t             span_count;
    uint8_t             align : 2;
    uint8_t             overflow : 2;
    uint8_t             line_space;
    uint8_t             indent;
    sgl_color_t         bg_color;
    uint8_t             bg_flag : 1;
    uint8_t             alpha;
    int16_t             scroll_offset;
    int16_t             total_width;    /* total width of all spans */
    int16_t             total_height;   /* total height including wrapped lines */
    int16_t             max_line_height; /* maximum line height */
} sgl_span_t;

/**
 * @brief create a span object
 * @param parent parent of the span
 * @return pointer to the span object
 */
sgl_obj_t* sgl_span_create(sgl_obj_t* parent);

/**
 * @brief add a span fragment to the span widget
 * @param obj pointer to the span object
 * @param text text content
 * @param font font for this span (NULL to use default)
 * @param color text color
 * @return span index on success, -1 on failure
 */
int sgl_span_add_span(sgl_obj_t *obj, const char *text, const sgl_font_t *font, sgl_color_t color);

/**
 * @brief add a span fragment with background color
 * @param obj pointer to the span object
 * @param text text content
 * @param font font for this span (NULL to use default)
 * @param color text color
 * @param bg_color background color
 * @return span index on success, -1 on failure
 */
int sgl_span_add_span_bg(sgl_obj_t *obj, const char *text, const sgl_font_t *font, sgl_color_t color, sgl_color_t bg_color);

/**
 * @brief set span fragment padding
 * @param obj pointer to the span object
 * @param index span index
 * @param left left padding in pixels
 * @param right right padding in pixels
 * @return none
 */
void sgl_span_set_span_padding(sgl_obj_t *obj, int index, uint8_t left, uint8_t right);

/**
 * @brief set span fragment alpha
 * @param obj pointer to the span object
 * @param index span index
 * @param alpha alpha value (0-255)
 * @return none
 */
void sgl_span_set_span_alpha(sgl_obj_t *obj, int index, uint8_t alpha);

/**
 * @brief clear all spans
 * @param obj pointer to the span object
 * @return none
 */
void sgl_span_clear(sgl_obj_t *obj);

/**
 * @brief set span widget alignment
 * @param obj pointer to the span object
 * @param align alignment type
 * @return none
 */
void sgl_span_set_align(sgl_obj_t *obj, sgl_span_align_t align);

/**
 * @brief set span widget overflow mode
 * @param obj pointer to the span object
 * @param overflow overflow mode
 * @return none
 */
void sgl_span_set_overflow(sgl_obj_t *obj, sgl_span_overflow_t overflow);

/**
 * @brief set line space for wrapped text
 * @param obj pointer to the span object
 * @param line_space line space in pixels
 * @return none
 */
void sgl_span_set_line_space(sgl_obj_t *obj, uint8_t line_space);

/**
 * @brief set first line indent
 * @param obj pointer to the span object
 * @param indent indent in pixels
 * @return none
 */
void sgl_span_set_indent(sgl_obj_t *obj, uint8_t indent);

/**
 * @brief set span widget background color
 * @param obj pointer to the span object
 * @param color background color
 * @return none
 */
void sgl_span_set_bg_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set span widget background transparent
 * @param obj pointer to the span object
 * @return none
 */
void sgl_span_set_bg_transparent(sgl_obj_t *obj);

/**
 * @brief set span widget alpha
 * @param obj pointer to the span object
 * @param alpha alpha value (0-255)
 * @return none
 */
void sgl_span_set_alpha(sgl_obj_t *obj, uint8_t alpha);

/**
 * @brief set span widget radius
 * @param obj pointer to the span object
 * @param radius corner radius
 * @return none
 */
void sgl_span_set_radius(sgl_obj_t *obj, uint8_t radius);

/**
 * @brief get total width of all spans
 * @param obj pointer to the span object
 * @return total width in pixels
 */
int16_t sgl_span_get_total_width(sgl_obj_t *obj);

/**
 * @brief get total height of the span widget
 * @param obj pointer to the span object
 * @return total height in pixels
 */
int16_t sgl_span_get_total_height(sgl_obj_t *obj);

#ifdef __cplusplus
}
#endif

#endif // !__SGL_SPAN_H__
