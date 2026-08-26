/* source/widgets/sgl_numberkbd.h
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

#ifndef __SGL_NUMBERKBD_H__
#define __SGL_NUMBERKBD_H__

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
 * @brief sgl number keyboard struct
 * @obj: sgl general object
 * @body_desc: pointer to sgl_draw_rect_t descriptor
 */
typedef struct sgl_numberkbd {
    sgl_obj_t        obj;
    sgl_draw_rect_t  body_desc;
    sgl_draw_rect_t  btn_desc;
    const sgl_font_t *font;
    sgl_color_t      text_color;
    uint8_t          margin;
    uint8_t          opcode;
}sgl_numberkbd_t;


/**
 * @brief create numberkbd object
 * @param parent parent of object
 * @return numberkbd object
 */
sgl_obj_t* sgl_numberkbd_create(sgl_obj_t* parent);


/**
 * @brief set numberkbd color
 * @param obj numberkbd object
 * @param color color
 * @return none
 */
void sgl_numberkbd_set_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set numberkbd alpha
 * @param obj numberkbd object
 * @param alpha alpha [0 ~ 255]
 * @return none
 */
void sgl_numberkbd_set_alpha(sgl_obj_t *obj, uint8_t alpha);

/**
 * @brief set numberkbd radius
 * @param obj numberkbd object
 * @param radius radius [0 ~ 255]
 * @return none
 */
void sgl_numberkbd_set_radius(sgl_obj_t *obj, uint8_t radius);

/**
 * @brief set numberkbd border width
 * @param obj numberkbd object
 * @param width border width [0 ~ 255]
 * @return none
 */
void sgl_numberkbd_set_border_width(sgl_obj_t *obj, uint8_t width);

/**
 * @brief set numberkbd border color
 * @param obj numberkbd object
 * @param color border color
 * @return none
 */
void sgl_numberkbd_set_border_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set numberkbd text font
 * @param obj numberkbd object
 * @param font font
 * @return none
 */
void sgl_numberkbd_set_text_font(sgl_obj_t *obj, const sgl_font_t *font);

/**
 * @brief set numberkbd text color
 * @param obj numberkbd object
 * @param color text color
 * @return none
 */
void sgl_numberkbd_set_text_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set numberkbd pixmap
 * @param obj numberkbd object
 * @param pixmap pixmap of numberkbd
 * @return none
 */
void sgl_numberkbd_set_pixmap(sgl_obj_t *obj, const sgl_pixmap_t *pixmap);

/**
 * @brief set numberkbd button margin
 * @param obj numberkbd object
 * @param margin margin [0 ~ 255]
 * @return none
 */
void sgl_numberkbd_set_btn_margin(sgl_obj_t *obj, uint8_t margin);

/**
 * @brief set numberkbd button color
 * @param obj numberkbd object
 * @param color button color
 * @return none
 */
void sgl_numberkbd_set_btn_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set numberkbd button border width
 * @param obj numberkbd object
 * @param width button border width [0 ~ 255]
 * @return none
 */
void sgl_numberkbd_set_btn_border_width(sgl_obj_t *obj, uint8_t width);

/**
 * @brief set numberkbd button border color
 * @param obj numberkbd object
 * @param color button border color
 * @return none
 */
void sgl_numberkbd_set_btn_border_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set numberkbd button radius
 * @param obj numberkbd object
 * @param radius button radius [0 ~ 255]
 * @return none
 */
void sgl_numberkbd_set_btn_radius(sgl_obj_t *obj, uint8_t radius);

/**
 * @brief set numberkbd button pixmap
 * @param obj numberkbd object
 * @param pixmap button pixmap
 * @return none
 */
void sgl_numberkbd_set_btn_pixmap(sgl_obj_t *obj, const sgl_pixmap_t *pixmap);

/**
 * @brief get numberkbd opcode
 * @param obj numberkbd object
 * @return opcode [0 ~ 255]
 */
uint8_t sgl_numberkbd_get_opcode(sgl_obj_t *obj);

#ifdef __cplusplus
}
#endif

#endif // !__SGL_NUMBERKBD_H__
