/* source/widgets/sgl_led.h
 *
 * MIT License
 *
 * Copyright(c) 2023-present All contributors of SGL  
 * Document reference link: docs directory
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

#ifndef __SGL_LED_H__
#define __SGL_LED_H__

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

#define  SGL_LED_COLOR        sgl_rgb(255, 255, 255)
#define  SGL_LED_BG_COLOR     sgl_rgb(0, 0, 0)

/**
 * @brief sgl led struct
 * @obj: sgl general object
 * @desc: pointer to led draw descriptor
 */
typedef struct sgl_led {
    sgl_obj_t       obj;
    sgl_color_t     on_color;
    sgl_color_t     off_color;
    sgl_color_t     bg_color;
    uint8_t         alpha;
    bool            status;
}sgl_led_t;


/**
 * @brief create a led object
 * @param parent parent of the led
 * @return led object
 */
sgl_obj_t* sgl_led_create(sgl_obj_t* parent);

/**
 * @brief set the radius of the led
 * @param obj led object
 * @param radius radius of the led
 * @return none
 */
void sgl_led_set_radius(sgl_obj_t *obj, uint8_t radius);

/**
 * @brief set the color of the led
 * @param obj led object
 * @param color color of the led
 * @return none
 */
void sgl_led_set_on_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set the off color of the led
 * @param obj led object
 * @param color off color of the led
 * @return none
 */
void sgl_led_set_off_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief Set the background color of the led
 * @param obj led object
 * @param color background color of the led
 * @return none
 */
void sgl_led_set_bg_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set the alpha of the led
 * @param obj led object
 * @param alpha alpha of the led
 * @return none
 */
void sgl_led_set_alpha(sgl_obj_t *obj, uint8_t alpha);

/**
 * @brief set the status of the led
 * @param obj led object
 * @param status status of the led
 * @return none
 */
void sgl_led_set_status(sgl_obj_t *obj, bool status);

/**
 * @brief get the status of the led
 * @param obj led object
 * @return status of the led
 */
bool sgl_led_get_status(sgl_obj_t *obj);

/**
 * @brief turn on the led
 * @param obj led object
 * @return none
 */
void sgl_led_on(sgl_obj_t *obj);

/**
 * @brief turn off the led
 * @param obj led object
 * @return none
 */
void sgl_led_off(sgl_obj_t *obj);

#ifdef __cplusplus
}
#endif

#endif // !__SGL_LED_H__
