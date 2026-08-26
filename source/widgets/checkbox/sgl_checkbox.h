/* source/widgets/sgl_checkbox.h
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

#ifndef __SGL_CHECKBOX_H__
#define __SGL_CHECKBOX_H__

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

typedef struct sgl_checkbox {
    sgl_obj_t          obj;
    const char         *text;
    const sgl_font_t   *font;
    sgl_color_t        text_color;
    uint8_t            alpha;
    bool               status;
    sgl_color_t        check_color;
    sgl_color_t        box_color;
} sgl_checkbox_t;

/**
 * @brief create a checkbox object
 * @param parent parent of the checkbox
 * @return checkbox object
 */
sgl_obj_t* sgl_checkbox_create(sgl_obj_t* parent);

/**
 * @brief set checkbox text color
 * @param obj checkbox object
 * @param color color of text
 * @return none
 */
void sgl_checkbox_set_text_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set checkbox box color
 * @param obj checkbox object
 * @param color color of box
 * @return none
 */
void sgl_checkbox_set_box_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set checkbox check color
 * @param obj checkbox object
 * @param color color of check
 * @return none
 */
void sgl_checkbox_set_check_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set checkbox text and icon alpha
 * @param obj checkbox object
 * @param alpha alpha of text
 * @return none
 */
void sgl_checkbox_set_alpha(sgl_obj_t *obj, uint8_t alpha);

/**
 * @brief set checkbox text
 * @param obj checkbox object
 * @param text text of checkbox
 * @return none
 */
void sgl_checkbox_set_text(sgl_obj_t *obj, const char *text);

/**
 * @brief set checkbox font
 * @param obj checkbox object
 * @param font font of checkbox
 * @return none
 */
void sgl_checkbox_set_font(sgl_obj_t *obj, const sgl_font_t *font);

/**
 * @brief set checkbox radius
 * @param obj checkbox object
 * @param radius radius of checkbox
 * @return none
 */
void sgl_checkbox_set_radius(sgl_obj_t *obj, int16_t radius);

/**
 * @brief set checkbox status
 * @param obj checkbox object
 * @param status status of checkbox
 * @return none
 */
void sgl_checkbox_set_status(sgl_obj_t *obj, bool status);

/**
 * @brief get checkbox status
 * @param obj checkbox object
 * @return status of checkbox
 */
bool sgl_checkbox_get_status(sgl_obj_t *obj);

#ifdef __cplusplus
}
#endif

#endif // !__SGL_CHECKBOX_H__
