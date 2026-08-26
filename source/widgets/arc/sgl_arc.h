/* source/widgets/sgl_arc.h
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

#include <sgl_core.h>
#include <sgl_draw.h>
#include <sgl_math.h>
#include <sgl_log.h>
#include <sgl_mm.h>
#include <sgl_cfgfix.h>
#include <string.h>

#ifndef __SGL_ARC_H__
#define __SGL_ARC_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sgl_arc {
    sgl_obj_t       obj;
    sgl_draw_arc_t  desc;
    int16_t         width;
}sgl_arc_t;

/**
 * @brief create an arc object
 * @param parent parent object
 * @return arc object
 */
sgl_obj_t* sgl_arc_create(sgl_obj_t* parent);

/**
 * @brief set arc object color
 * @param obj arc object
 * @param color arc color
 * @return none
 */
void sgl_arc_set_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set arc object background color
 * @param obj arc object
 * @param color arc background color
 * @return none
 */
void sgl_arc_set_bg_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set arc object alpha
 * @param obj arc object
 * @param alpha arc alpha
 * @return none
 */
void sgl_arc_set_alpha(sgl_obj_t *obj, uint8_t alpha);

/**
 * @brief set arc object radius
 * @param obj arc object
 * @param radius_in arc radius_in
 * @param radius_out arc radius_out
 * @return none
 */
void sgl_arc_set_radius(sgl_obj_t *obj, int16_t radius_in, int16_t radius_out);

/**
 * @brief set arc object mode
 * @param obj arc object
 * @param mode arc mode
 * @return none
 */
void sgl_arc_set_mode(sgl_obj_t *obj, uint8_t mode);

/**
 * @brief set arc object start angle
 * @param obj arc object
 * @param angle arc start angle
 * @return none
 * @note angle should be in range [0, 360]
 */
void sgl_arc_set_start_angle(sgl_obj_t *obj, int16_t angle);

/**
 * @brief set arc object end angle
 * @param obj arc object
 * @param angle arc end angle
 * @return none
 * @note angle should be in range [0, 360]
 */
void sgl_arc_set_end_angle(sgl_obj_t *obj, int16_t angle);

/**
 * @brief get included angle between start and end angle
 * @param obj arc object
 * @return normalized included angle in range [0, 360]
 */
int16_t sgl_arc_get_included_angle(sgl_obj_t *obj);

#ifdef __cplusplus
}
#endif

#endif // !__SGL_ARC_H__
