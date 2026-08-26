/* source/widgets/sgl_bar.h
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

#ifndef __SGL_BAR_H__
#define __SGL_BAR_H__

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
 * @brief sgl bar struct
 * @obj: sgl general object
 * @param track_color track color
 * @param border_color border color
 * @param pixmap pixmap
 * @param fill_color fill color
 * @param direct bar direction
 * @param value bar value
 * @param alpha alpha
 */
typedef struct sgl_bar {
    sgl_obj_t          obj;
    const sgl_pixmap_t *pixmap;
    sgl_color_t        fill_color;
    sgl_color_t        track_color;
    sgl_color_t        border_color;
    uint8_t            alpha;
    /* 0: horizontal, 1: vertical */
    uint8_t            direct : 1;
    uint8_t            value : 7;
} sgl_bar_t;

/**
 * @brief create a bar object
 * @param parent parent object of the bar
 * @return bar object
 */
sgl_obj_t* sgl_bar_create(sgl_obj_t* parent);

/**
 * @brief set the fill color of the bar
 * @param obj bar object
 * @param color fill color
 * @return none
 */
void sgl_bar_set_fill_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set the track color of the bar
 * @param obj bar object
 * @param color track color
 * @return none
 */
void sgl_bar_set_track_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set the bar direction
 * @param obj bar object
 * @param direct bar direction
 * @return none
 * @note direct : SGL_DIRECT_HORIZONTAL or SGL_DIRECT_VERTICAL
 */
void sgl_bar_set_direct(sgl_obj_t *obj, uint8_t direct);

/**
 * @brief set the bar radius
 * @param obj bar object
 * @param radius bar radius
 * @return none
 */
void sgl_bar_set_radius(sgl_obj_t *obj, uint8_t radius);

/**
 * @brief set the bar value
 * @param obj bar object
 * @param value bar value
 * @return none
 */
void sgl_bar_set_value(sgl_obj_t *obj, uint8_t value);

/**
 * @brief get the bar value
 * @param obj bar object
 * @return bar value
 */
uint8_t sgl_bar_get_value(sgl_obj_t *obj);

/**
 * @brief set the bar border width
 * @param obj bar object
 * @param width border width
 */
void sgl_bar_set_border_width(sgl_obj_t *obj, uint8_t width);

/**
 * @brief set the bar border color
 * @param obj bar object
 * @param color border color
 */
void sgl_bar_set_border_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set the bar pixmap
 * @param obj bar object
 * @param pixmap pixmap
 */
void sgl_bar_set_pixmap(sgl_obj_t *obj, const sgl_pixmap_t *pixmap);

/**
 * @brief set the alpha of the bar
 * @param obj bar object
 * @param alpha alpha
 * @return none
 */
void sgl_bar_set_alpha(sgl_obj_t *obj, uint8_t alpha);

#ifdef __cplusplus
}
#endif

#endif // !__SGL_BAR_H__
