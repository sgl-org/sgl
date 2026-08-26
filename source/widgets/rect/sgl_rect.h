/* source/widgets/sgl_rectangle.h
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

#ifndef __SGL_RECTANGLE_H__
#define __SGL_RECTANGLE_H__

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
 * @brief sgl rectangle
 * @obj: sgl general object
 * @desc: rectangle draw description
 */
typedef struct sgl_rect {  
    sgl_obj_t       obj;
    const sgl_pixmap_t *pixmap;
    sgl_color_t     color;
    sgl_color_t     border_color;
    uint8_t         alpha;
    uint8_t         border_alpha;
} sgl_rect_t;

/**
 * @brief  create a rectangle
 * @param  parent: parent object
 * @retval rectangle object
 */
sgl_obj_t* sgl_rect_create(sgl_obj_t* parent);

/**
 * @brief  set rectangle color
 * @param  obj: rectangle object
 * @param  color: rectangle color
 * @retval none
 */
void sgl_rect_set_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief  set rectangle alpha
 * @param  obj: rectangle object
 * @param  alpha: rectangle alpha
 * @retval none
 */
void sgl_rect_set_alpha(sgl_obj_t *obj, uint8_t alpha);

/**
 * @brief  set rectangle main body alpha
 * @param  obj: rectangle object
 * @param  alpha: rectangle main body alpha
 * @retval none
 */
void sgl_rect_set_main_alpha(sgl_obj_t *obj, uint8_t alpha);

/**
 * @brief  set rectangle radius
 * @param  obj: rectangle object
 * @param  radius: rectangle radius
 * @retval none
 */
void sgl_rect_set_radius(sgl_obj_t *obj, uint16_t radius);

/**
 * @brief  set rectangle border width
 * @param  obj: rectangle object
 * @param  width: rectangle border width
 * @retval none
 */
void sgl_rect_set_border_width(sgl_obj_t *obj, uint8_t width);

/**
 * @brief  set rectangle border color
 * @param  obj: rectangle object
 * @param  color: rectangle border color
 * @retval none
 */
void sgl_rect_set_border_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief  set rectangle border alpha
 * @param  obj: rectangle object
 * @param  alpha: rectangle border alpha
 * @retval none
 */
void sgl_rect_set_border_alpha(sgl_obj_t *obj, uint8_t alpha);

/**
 * @brief  set rectangle pixmap
 * @param  obj: rectangle object
 * @param  pixmap: rectangle pixmap
 * @retval none
 */
void sgl_rect_set_pixmap(sgl_obj_t *obj, const sgl_pixmap_t *pixmap);

#ifdef __cplusplus
}
#endif

#endif // !__SGL_RECTANGLE_H__
