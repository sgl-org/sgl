/* source/widgets/sgl_circle.h
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

#ifndef __SGL_CIRCLE_H__
#define __SGL_CIRCLE_H__

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

#define  SGL_CIRCLE_COLOR            sgl_rgb(255, 255, 255)
#define  SGL_CIRCLE_BORDER_COLOR     sgl_rgb(0, 0, 0)
#define  SGL_CIRCLE_BORDER_WIDTH     2

/**
 * @brief draw a circle
 * @obj:  sgl general object
 * @alpha:  alpha of the circle
 * @x_ofs:  x offset of the circle
 * @y_ofs:  y offset of the circle
 * @color:  color of the circle
 * @border_color:  border color of the circle
 * @pixmap:  pixmap of the circle
 */
typedef struct sgl_circle {
    sgl_obj_t     obj;
    uint8_t       alpha;
    int8_t        x_ofs;
    int8_t        y_ofs;
    sgl_color_t   color;
    sgl_color_t   border_color;
    const sgl_pixmap_t *pixmap;
}sgl_circle_t;


/**
 * @brief create a circle object
 * @param parent parent of the object
 * @return pointer to the object
 */
sgl_obj_t* sgl_circle_create(sgl_obj_t* parent);

/**
 * @brief set the color of the circle
 * @param obj pointer to the object
 * @param color color of the circle
 * @return none
 */
void sgl_circle_set_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set the radius of the circle
 * @param obj pointer to the object
 * @param radius radius of the circle
 * @return none
 */
void sgl_circle_set_radius(sgl_obj_t *obj, uint16_t radius);

/**
 * @brief set the alpha of the circle
 * @param obj pointer to the object
 * @param alpha alpha of the circle
 * @return none
 */
void sgl_circle_set_alpha(sgl_obj_t *obj, uint8_t alpha);

/**
 * @brief set the pixmap of the circle
 * @param obj pointer to the object
 * @param pixmap pixmap of the circle
 * @return none
 */
void sgl_circle_set_pixmap(sgl_obj_t *obj, const sgl_pixmap_t *pixmap);

/**
 * @brief set the border color of the circle
 * @param obj pointer to the object
 * @param color border color of the circle
 * @return none
 */
void sgl_circle_set_border_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set the border width of the circle
 * @param obj pointer to the object
 * @param width border width of the circle
 * @return none
 */
void sgl_circle_set_border_width(sgl_obj_t *obj, uint8_t width);

/**
 * @brief set the x offset of the center of circle
 * @param obj pointer to the object
 * @param offset x offset of the circle
 * @return none
 */
void sgl_circle_set_x_offset(sgl_obj_t *obj, int8_t offset);

/**
 * @brief set the y offset of the center of circle
 * @param obj pointer to the object
 * @param offset y offset of the circle
 * @return none
 */
void sgl_circle_set_y_offset(sgl_obj_t *obj, int8_t offset);

#ifdef __cplusplus
}
#endif

#endif // !__SGL_CIRCLE_H__
