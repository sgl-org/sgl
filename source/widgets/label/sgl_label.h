/* source/widgets/sgl_label.h
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

#ifndef __SGL_LABEL_H__
#define __SGL_LABEL_H__

#include <sgl_core.h>
#include <sgl_draw.h>
#include <sgl_math.h>
#include <sgl_log.h>
#include <sgl_mm.h>
#include <sgl_cfgfix.h>
#include <string.h>


/**
 * @brief sgl label object
 * @obj: sgl general object
 * @desc: draw task descriptor
 */
typedef struct sgl_label {
    sgl_obj_t        obj;
    const sgl_font_t *font;
    char             *text;
    sgl_color_t      color;
    sgl_color_t      bg_color;
    int16_t          offset_x;
    int16_t          text_length;
    uint16_t         text_capacity;
    uint8_t          alpha;
    uint8_t          dynamic : 1;
    uint8_t          align: 5;
    uint8_t          bg_flag : 1;
    uint8_t          long_mode : 1;
} sgl_label_t;

/**
 * @brief create a label object
 * @param parent parent of the label
 * @return pointer to the label object
 */
sgl_obj_t* sgl_label_create(sgl_obj_t* parent);

/**
 * @brief set the text of the label
 * @param obj pointer to the label object
 * @param text pointer to the text
 * @return none
 */
void sgl_label_set_text(sgl_obj_t *obj, char *text);

/**
 * @brief set the text buffer of the label
 * @param obj pointer to the label object
 * @param buf pointer to the text buffer
 * @param buf_size size of the text buffer
 * @return none
 */
void sgl_label_set_text_buffer(sgl_obj_t *obj, char *buf, uint16_t buf_size);

/**
 * @brief set the text of the label with format by manual memory
 * @param obj pointer to the label object
 * @param fmt pointer to the text
 * @return none
 * @note the text buffer must be set by sgl_label_set_text_buffer() before calling this function
 */
void sgl_label_set_text_fmt(sgl_obj_t *obj, const char *fmt, ...);

/**
 * @brief set the text of the label with format by dynamic memory
 * @param obj pointer to the label object
 * @param text pointer to the text
 * @return none
 */
void sgl_label_set_text_fmt_dynamic(sgl_obj_t* obj, const char *fmt, ...);

/**
 * @brief update label text area
 * @param obj pointer to the label object
 * @return none
 * @note you can update your label text area when you change the text buffer content
 */
void sgl_label_update_text(sgl_obj_t *obj);

/**
 * @brief get the text of the label
 * @param obj pointer to the label object
 * @return pointer to the text
 */
char* sgl_label_get_text(sgl_obj_t *obj);

/**
 * @brief set label font
 * @param obj pointer to the label object
 * @param font pointer to the font
 * @return none
 */
void sgl_label_set_font(sgl_obj_t *obj, const sgl_font_t *font);

/**
 * @brief set label text color
 * @param obj pointer to the label object
 * @param color color to be set
 * @return none
 */
void sgl_label_set_text_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set label background color
 * @param obj pointer to the label object
 * @param color color to be set
 * @return none
 */
void sgl_label_set_bg_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set label radius
 * @param obj pointer to the label object
 * @param radius radius to be set
 * @return none
 */
void sgl_label_set_radius(sgl_obj_t *obj, uint8_t radius);

/**
 * @brief set label text align
 * @param obj pointer to the label object
 * @param align align to be set
 * @return none
 */
void sgl_label_set_text_align(sgl_obj_t *obj, sgl_align_type_t align);

/**
 * @brief set label alpha
 * @param obj pointer to the label object
 * @param alpha alpha to be set
 * @return none
 */
void sgl_label_set_alpha(sgl_obj_t *obj, uint8_t alpha);

/**
 * @brief set label text offset
 * @param obj pointer to the label object
 * @param offset_x offset_x to be set
 * @return none
 */
void sgl_label_set_text_offset(sgl_obj_t *obj, int8_t offset_x);

#if CONFIG_SGL_ANIMATION
/**
 * @brief set label long mode
 * @param obj pointer to the label object
 * @param flag flag to be set
 * @return none
 */
void sgl_label_set_long_mode(sgl_obj_t *obj, uint32_t speed_ms, bool flag);
#endif

#endif // !__SGL_LABEL_H__
