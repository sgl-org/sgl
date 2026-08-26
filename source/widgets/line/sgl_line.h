/* source/widgets/sgl_line.h
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

#ifndef __SGL_LINE_H__
#define __SGL_LINE_H__

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
 * @brief sgl line struct
 * @obj: sgl general object
 * @color: line color
 * @x_swap: x coordinate swap
 * @y_swap: y coordinate swap
 * @alpha: alpha
 */
typedef struct sgl_line {
    sgl_obj_t     obj;
    sgl_color_t   color;
    uint16_t      dash_length;   /* solid segment length in pixels when dashed */
    uint16_t      gap_length;    /* gap length in pixels when dashed */
    uint8_t       alpha;
    uint8_t       x_swap : 4;
    uint8_t       y_swap : 4;
    uint8_t       dashed;        /* 0: solid line, non-zero: dashed line */
}sgl_line_t;


/**
 * @brief create a line object
 * @param parent parent of the line
 * @return line object
 */
sgl_obj_t* sgl_line_create(sgl_obj_t* parent);


/**
 * @brief set line color
 * @param obj line object
 * @param color line color
 * @return none
 */
void sgl_line_set_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set line alpha
 * @param obj line object
 * @param alpha line alpha
 * @return none
 */
void sgl_line_set_alpha(sgl_obj_t *obj, uint8_t alpha);

/**
 * @brief set line dashed or solid
 * @param obj line object
 * @param dashed 0: solid line, non-zero: dashed line
 * @return none
 */
void sgl_line_set_dashed(sgl_obj_t *obj, uint8_t dashed);

/**
 * @brief set line dash pattern when dashed
 * @param obj line object
 * @param dash_len length of solid segment in pixels
 * @param gap_len length of space segment in pixels
 * @return none
 */
void sgl_line_set_dash_pattern(sgl_obj_t *obj, uint16_t dash_len, uint16_t gap_len);
 
/**
 * @brief set line start position
 * @param obj line object
 * @param x start x position
 * @param y start y position
 * @return none
 */
void sgl_line_set_pos(sgl_obj_t *obj, int16_t x1, int16_t y1, int16_t x2, int16_t y2);

/**
 * @brief set line width
 * @param obj line object
 * @param width line width
 * @return none
 */
void sgl_line_set_width(sgl_obj_t *obj, uint8_t width);

#ifdef __cplusplus
}
#endif

#endif // !__SGL_LINE_H__
