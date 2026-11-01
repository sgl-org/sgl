/* source/widgets/sgl_2dball.h
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

#ifndef __SGL_2DBALL_H__
#define __SGL_2DBALL_H__

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

#define  SGL_2DBALL_COLOR      sgl_rgb(255, 255, 255)
#define  SGL_2DBALL_BG_COLOR   sgl_rgb(0, 0, 0)

/**
 * @brief sgl 2dball struct
 * @obj: sgl general object
 * @desc: pointer to 2dball draw descriptor
 */
typedef struct sgl_2dball {
    sgl_obj_t       obj;
    sgl_color_t     color;
    sgl_color_t     bg_color;
    uint8_t         alpha;
}sgl_2dball_t;

/**
 * @brief create a 2dball object
 * @param parent parent of the 2dball
 * @return 2dball object
 */
sgl_obj_t* sgl_2dball_create(sgl_obj_t* parent);

/**
 * @brief set the color of the 2dball
 * @param obj 2dball object
 * @param color color
 * @return none
 */
void sgl_2dball_set_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set the background color of the 2dball
 * @param obj 2dball object
 * @param color background color
 * @return none
 */
void sgl_2dball_set_bg_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set the alpha of the 2dball
 * @param obj 2dball object
 * @param alpha alpha
 * @return none
 */
void sgl_2dball_set_alpha(sgl_obj_t *obj, uint8_t alpha);

/**
 * @brief set the radius of the 2dball
 * @param obj 2dball object
 * @param radius radius
 * @return none
 */
void sgl_2dball_set_radius(sgl_obj_t *obj, uint16_t radius);

#ifdef __cplusplus
}
#endif

#endif // !__SGL_2DBALL_H__
