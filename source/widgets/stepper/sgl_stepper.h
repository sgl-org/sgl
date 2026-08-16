/* source/widgets/stepper/sgl_stepper.h
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

#ifndef __SGL_STEPPER_H__
#define __SGL_STEPPER_H__

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
 * @brief stepper widget structure — left [-] / center value / right [+]
 * @obj:            sgl general object
 * @font:           font for value text
 * @bg_color:       center area background color
 * @btn_color:      button area background color
 * @btn_press_color: button area pressed highlight color
 * @text_color:     value text color
 * @sign_color:     +/- sign normal color
 * @sign_press_color: +/- sign pressed color
 * @sign_dis_color: +/- sign disabled color
 * @border_color:   outer border color
 * @value:          current value (fixed-point, real = value / 10^decimals)
 * @min_value:      minimum value
 * @max_value:      maximum value
 * @step:           increment per step (>0)
 * @decimals:       number of decimal places (0 = integer)
 * @radius:         corner radius
 * @buf:            formatted text buffer
 * @active_side:    current pressed side: -1=left, 0=none, +1=right
 * @wrap:           wrap at boundary (1) or disable button (0)
 */
typedef struct sgl_stepper {
    sgl_obj_t              obj;
    const sgl_font_t       *font;
    sgl_color_t            bg_color;
    sgl_color_t            btn_color;
    sgl_color_t            btn_press_color;
    sgl_color_t            text_color;
    sgl_color_t            sign_color;
    sgl_color_t            sign_press_color;
    sgl_color_t            sign_dis_color;
    sgl_color_t            border_color;
    int32_t                value;
    int32_t                min_value;
    int32_t                max_value;
    int32_t                step;
    char                   buf[16];
    uint16_t               radius;
    uint8_t                decimals;
    int8_t                 active_side;
    uint8_t                wrap        : 1;
    uint8_t                pressed     : 1;
    uint8_t                reserved    : 6;
} sgl_stepper_t;

/**
 * @brief  create a stepper object
 * @param  parent: parent object pointer
 * @return stepper object pointer, NULL on failure
 */
sgl_obj_t* sgl_stepper_create(sgl_obj_t *parent);

/**
 * @brief set stepper value
 * @param  obj: stepper object pointer
 * @param  value: new value (fixed-point)
 * @return none
 */
void sgl_stepper_set_value(sgl_obj_t *obj, int32_t value);

/**
 * @brief get stepper value
 * @param  obj: stepper object pointer
 * @return current value (fixed-point)
 */
int32_t sgl_stepper_get_value(sgl_obj_t *obj);

/**
 * @brief set step increment
 * @param  obj: stepper object pointer
 * @param  step: step value (>0)
 * @return none
 */
void sgl_stepper_set_step(sgl_obj_t *obj, int32_t step);

/**
 * @brief set value range
 * @param  obj: stepper object pointer
 * @param  min_value: minimum value
 * @param  max_value: maximum value
 * @return none
 */
void sgl_stepper_set_range(sgl_obj_t *obj, int32_t min_value, int32_t max_value);

/**
 * @brief set decimal places
 * @param  obj: stepper object pointer
 * @param  decimals: decimal places (0-4)
 * @return none
 */
void sgl_stepper_set_decimals(sgl_obj_t *obj, uint8_t decimals);

/**
 * @brief set wrap mode
 * @param  obj: stepper object pointer
 * @param  wrap: 1=wrap at boundary, 0=disable button
 * @return none
 */
void sgl_stepper_set_wrap(sgl_obj_t *obj, uint8_t wrap);

/**
 * @brief set font for value display
 * @param  obj: stepper object pointer
 * @param  font: font pointer
 * @return none
 */
void sgl_stepper_set_font(sgl_obj_t *obj, const sgl_font_t *font);

/**
 * @brief set background color
 * @param  obj: stepper object pointer
 * @param  color: background color
 * @return none
 */
void sgl_stepper_set_bg_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set button color
 * @param  obj: stepper object pointer
 * @param  color: button color
 * @return none
 */
void sgl_stepper_set_btn_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set text color
 * @param  obj: stepper object pointer
 * @param  color: text color
 * @return none
 */
void sgl_stepper_set_text_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set sign color
 * @param  obj: stepper object pointer
 * @param  color: sign color
 * @return none
 */
void sgl_stepper_set_sign_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set border color
 * @param  obj: stepper object pointer
 * @param  color: border color
 * @return none
 */
void sgl_stepper_set_border_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set corner radius
 * @param  obj: stepper object pointer
 * @param  radius: corner radius
 * @return none
 */
void sgl_stepper_set_radius(sgl_obj_t *obj, uint8_t radius);

#ifdef __cplusplus
}
#endif

#endif /* __SGL_STEPPER_H__ */
