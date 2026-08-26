/* source/widgets/sgl_textline.h
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

#ifndef __SGL_TEXTLINE_H__
#define __SGL_TEXTLINE_H__

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
 * @brief sgl textline struct
 * @desc: text description
 */
typedef struct sgl_textline {
    sgl_obj_t        obj;
    const char       *text;
    const sgl_font_t *font;
    sgl_color_t      color;
    sgl_color_t      bg_color;
    uint8_t          alpha;
    uint8_t          line_margin;
    uint8_t          edge_margin : 7;
    uint8_t          bg_flag : 1;
} sgl_textline_t;

/**
 * @brief create a textline object
 * @param parent parent of the textline
 * @return textline object
 */
sgl_obj_t* sgl_textline_create(sgl_obj_t* parent);

/**
 * @brief set textline text
 * @param obj textline object
 * @param text text
 * @return none
 */
void sgl_textline_set_text(sgl_obj_t *obj, const char *text);

/**
 * @brief set textline font
 * @param obj textline object
 * @param font font
 * @return none
 */
void sgl_textline_set_text_font(sgl_obj_t *obj, const sgl_font_t *font);

/**
 * @brief set textline text color
 * @param obj textline object
 * @param color color of text
 * @return none
 */
void sgl_textline_set_text_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set textline background color
 * @param obj textline object
 * @param color color of background
 * @return none
 */
void sgl_textline_set_bg_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set textline background transparent
 * @param obj textline object
 * @return none
 */
void sgl_textline_set_bg_transparent(sgl_obj_t *obj);

/**
 * @brief set textline radius
 * @param obj textline object
 * @param radius radius of textline
 * @return none
 */
void sgl_textline_set_radius(sgl_obj_t *obj, uint8_t radius);

/**
 * @brief set textline alpha
 * @param obj textline object
 * @param alpha alpha of textline
 * @return none
 */
void sgl_textline_set_alpha(sgl_obj_t *obj, uint8_t alpha);

/**
 * @brief set textline edge margin
 * @param obj textline object
 * @param margin margin of textline
 * @return none
 */
void sgl_textline_set_edge_margin(sgl_obj_t *obj, uint8_t margin);

/**
 * @brief set textline text line margin
 * @param obj textline object
 * @param margin margin of textline
 * @return none
 */
void sgl_textline_set_line_margin(sgl_obj_t *obj, uint8_t margin);

#ifdef __cplusplus
}
#endif

#endif // !__SGL_TEXTLINE_H__
