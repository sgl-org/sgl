/* source/widgets/sgl_label_ext.h
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

#ifndef __SGL_LABEL_EXT_H__
#define __SGL_LABEL_EXT_H__

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
 * @brief sgl label_ext object
 * @obj: sgl general object
 * @desc: draw task descriptor
 */
typedef struct sgl_label_ext {
    sgl_obj_t        obj;
    const sgl_font_t *font;
    char             *text;
    uint16_t         text_capacity;
    sgl_color_t      color;
    sgl_color_t      bg_color;
    int16_t          rotation;
    uint8_t          alpha;
    uint8_t          dynamic : 1;
    uint8_t          align: 6;
    uint8_t          bg_flag : 1;
} sgl_label_ext_t;


/**
 * @brief create a label_ext object
 * @param parent parent of the label_ext
 * @return pointer to the label_ext object
 */
sgl_obj_t* sgl_label_ext_create(sgl_obj_t* parent);

/**
 * @brief set the text of the label_ext
 * @param obj pointer to the label_ext object
 * @param text pointer to the text
 * @return none
 */
void sgl_label_ext_set_text(sgl_obj_t *obj, const char *text);

/**
 * @brief set the text buffer of the label_ext
 * @param obj pointer to the label_ext object
 * @param buf pointer to the text buffer
 * @param buf_size size of the text buffer
 * @return none
 */
void sgl_label_ext_set_text_buffer(sgl_obj_t *obj, char *buf, uint16_t buf_size);

/**
 * @brief set the text of the label_ext with format by manual memory
 * @param obj pointer to the label_ext object
 * @param fmt pointer to the text
 * @return none
 * @note the text buffer must be set by sgl_label_ext_set_text_buffer() before calling this function
 */
void sgl_label_ext_set_text_fmt(sgl_obj_t *obj, const char *fmt, ...);

/**
 * @brief set the text of the label_ext with format by dynamic memory
 * @param obj pointer to the label_ext object
 * @param text pointer to the text
 * @return none
 */
void sgl_label_ext_set_text_fmt_dynamic(sgl_obj_t* obj, const char *fmt, ...);

/**
 * @brief update label_ext text area
 * @param obj pointer to the label_ext object
 * @return none
 * @note you can update your label_ext text area when you change the text buffer content
 */
void sgl_label_ext_update_text(sgl_obj_t *obj);

/**
 * @brief get the text of the label_ext
 * @param obj pointer to the label_ext object
 * @return pointer to the text
 */
char* sgl_label_ext_get_text(sgl_obj_t *obj);

/**
 * @brief set label_ext font
 * @param obj pointer to the label_ext object
 * @param font pointer to the font
 * @return none
 */
void sgl_label_ext_set_font(sgl_obj_t *obj, const sgl_font_t *font);

/**
 * @brief set label_ext text color
 * @param obj pointer to the label_ext object
 * @param color color to be set
 * @return none
 */
void sgl_label_ext_set_text_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set label_ext background color
 * @param obj pointer to the label_ext object
 * @param color color to be set
 * @return none
 */
void sgl_label_ext_set_bg_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set label_ext radius
 * @param obj pointer to the label_ext object
 * @param radius radius to be set
 * @return none
 */
void sgl_label_ext_set_radius(sgl_obj_t *obj, uint8_t radius);

/**
 * @brief set label_ext text align
 * @param obj pointer to the label_ext object
 * @param align align to be set
 * @return none
 */
void sgl_label_ext_set_text_align(sgl_obj_t *obj, sgl_align_type_t align);

/**
 * @brief set label_ext alpha
 * @param obj pointer to the label_ext object
 * @param alpha alpha to be set
 * @return none
 */
void sgl_label_ext_set_alpha(sgl_obj_t *obj, uint8_t alpha);

/**
 * @brief set label_ext text offset
 * @param obj pointer to the label_ext object
 * @param offset_x offset_x to be set
 * @param offset_y offset_y to be set
 * @return none
 */
void sgl_label_ext_set_text_offset(sgl_obj_t *obj, int8_t offset_x, int8_t offset_y);

/**
 * @brief set label_ext text rotation
 * @param obj pointer to the label_ext object
 * @param text_rotation text rotation angle (0-360 degree)
 * @return none
 */
void sgl_label_ext_set_text_rotation(sgl_obj_t *obj, int16_t text_rotation);

#ifdef __cplusplus
}
#endif

#endif // !__SGL_LABEL_EXT_H__
