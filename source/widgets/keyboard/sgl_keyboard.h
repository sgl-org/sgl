/* source/widgets/sgl_keyboard.h
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

#ifndef __SGL_KEYBOARD_H__
#define __SGL_KEYBOARD_H__

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
 * @brief sgl keyboard struct
 * @obj: sgl general object
 * @body_desc: pointer to sgl_draw_rect_t descriptor
 */
typedef struct sgl_keyboard {
    sgl_obj_t        obj;
    sgl_draw_rect_t  body_desc;
    sgl_draw_rect_t  btn_desc;
    const sgl_font_t *font;
    char             *edit;
    sgl_color_t      text_color;
    uint32_t         edit_max_len;
    uint8_t          opcode;
    uint8_t          key_margin;
    int8_t           key_index;
    uint8_t          key_mode;
    sgl_area_t       btn_area;
} sgl_keyboard_t;


/**
 * @brief create keyboard object
 * @param parent parent of object
 * @return keyboard object
 */
sgl_obj_t* sgl_keyboard_create(sgl_obj_t* parent);

/**
 * @brief set keyboard color
 * @param obj keyboard object
 * @param color color
 * @return none
 */
void sgl_keyboard_set_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set keyboard alpha
 * @param obj keyboard object
 * @param alpha alpha [0 ~ 255]
 * @return none
 */
void sgl_keyboard_set_alpha(sgl_obj_t *obj, uint8_t alpha);

/**
 * @brief set keyboard main body alpha
 * @param obj keyboard object
 * @param alpha alpha [0 ~ 255]
 * @return none
 */
void sgl_keyboard_set_main_alpha(sgl_obj_t *obj, uint8_t alpha);

/**
 * @brief set keyboard radius
 * @param obj keyboard object
 * @param radius radius [0 ~ 255]
 * @return none
 */
void sgl_keyboard_set_radius(sgl_obj_t *obj, uint8_t radius);

/**
 * @brief set keyboard pixmap
 * @param obj keyboard object
 * @param pixmap pixmap
 * @return none
 */
void sgl_keyboard_set_pixmap(sgl_obj_t *obj, const sgl_pixmap_t *pixmap);

/**
 * @brief set keyboard border color
 * @param obj keyboard object
 * @param color border color
 * @return none
 * 
 */
void sgl_keyboard_set_border_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set keyboard border width
 * @param obj keyboard object
 * @param width border width [0 ~ 255]
 * @return none
 */
void sgl_keyboard_set_border_width(sgl_obj_t *obj, uint8_t width);

/**
 * @brief set keyboard border alpha
 * @param obj keyboard object
 * @param alpha border alpha [0 ~ 255]
 * @return none
 */
void sgl_keyboard_set_border_alpha(sgl_obj_t *obj, uint8_t alpha);

/**
 * @brief set keyboard text color
 * @param obj keyboard object
 * @param color text color
 * @return none
 */
void sgl_keyboard_set_text_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set keyboard text font
 * @param obj keyboard object
 * @param font text font
 * @return none
 */
void sgl_keyboard_set_text_font(sgl_obj_t *obj, const sgl_font_t *font);

/**
 * @brief set keyboard button radius
 * @param obj keyboard object
 * @param radius button radius [0 ~ 255]
 * @return none
 */
void sgl_keyboard_set_btn_radius(sgl_obj_t *obj, uint8_t radius);

/**
 * @brief set keyboard button alpha
 * @param obj keyboard object
 * @param alpha button alpha [0 ~ 255]
 * @return none
 */
void sgl_keyboard_set_btn_alpha(sgl_obj_t *obj, uint8_t alpha);

/**
 * @brief set keyboard button main alpha
 * @param obj keyboard object
 * @param alpha button alpha [0 ~ 255]
 * @return none
 */
void sgl_keyboard_set_btn_main_alpha(sgl_obj_t *obj, uint8_t alpha);

/**
 * @brief set keyboard button color
 * @param obj keyboard object
 * @param color button color
 * @return none
 */
void sgl_keyboard_set_btn_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set keyboard button pixmap
 * @param obj keyboard object
 * @param pixmap button pixmap
 * @return none
 */
void sgl_keyboard_set_btn_pixmap(sgl_obj_t *obj, const sgl_pixmap_t *pixmap);

/**
 * @brief set keyboard button border color
 * @param obj keyboard object
 * @param color button border color
 * @return none
 */
void sgl_keyboard_set_btn_border_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set keyboard button border width
 * @param obj keyboard object
 * @param width button border width [0 ~ 255]
 * @return none
 */
void sgl_keyboard_set_btn_border_width(sgl_obj_t *obj, uint8_t width);

/**
 * @brief set keyboard button border alpha
 * @param obj keyboard object
 * @param alpha button border alpha [0 ~ 255]
 * @return none
 */
void sgl_keyboard_set_btn_border_alpha(sgl_obj_t *obj, uint8_t alpha);

/**
 * @brief get keyboard opcode
 * @param obj keyboard object
 * @return opcode [0 ~ 255]
 */
uint8_t sgl_keyboard_get_opcode(sgl_obj_t *obj);

/**
 * @brief set keyboard text buffer
 * @param obj keyboard object
 * @param buffer edit buffer
 * @param buf_max_len edit buffer max length
 */
void sgl_keyboard_set_textarea(sgl_obj_t *obj, char *buffer, int buf_max_len);

#ifdef __cplusplus
}
#endif

#endif // !__SGL_KEYBOARD_H__
