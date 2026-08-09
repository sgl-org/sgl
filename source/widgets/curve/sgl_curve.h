/* source/widgets/curve/sgl_curve.h
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

#ifndef __SGL_CURVE_H__
#define __SGL_CURVE_H__

#include <sgl_core.h>
#include <sgl_draw.h>
#include <sgl_math.h>
#include <sgl_log.h>
#include <sgl_mm.h>
#include <sgl_cfgfix.h>
#include <string.h>

/* 
 * sgl curve example:
 *
 *     // Quadratic bezier curve (red)
 *     sgl_obj_t *quad_curve = sgl_curve_create(parent);
 *     sgl_obj_set_pos(quad_curve, 20, 100);
 *     sgl_obj_set_size(quad_curve, 260, 100);
 *     sgl_curve_set_color(quad_curve, SGL_COLOR_RED);
 *     sgl_curve_set_thickness(quad_curve, 5);
 *     // control points are normalized [0..255], mapped into the widget rect
 *     sgl_curve_set_quad(quad_curve, 0, 255, 128, 0, 255, 255);
 *
 *     // Cubic bezier curve (green)
 *     sgl_obj_t *cubic_curve = sgl_curve_create(parent);
 *     sgl_obj_set_pos(cubic_curve, 20, 240);
 *     sgl_obj_set_size(cubic_curve, 260, 140);
 *     sgl_curve_set_color(cubic_curve, SGL_COLOR_GREEN);
 *     sgl_curve_set_thickness(cubic_curve, 5);
 *     sgl_curve_set_cubic(cubic_curve, 0, 128, 85, 0, 170, 255, 255, 128);
 */

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum number of control points (cubic bezier needs 4) */
#define SGL_CURVE_MAX_POINTS    4

/* Normalized coordinate range: control points are stored in [0, 255]
 * and mapped onto the widget rectangle at draw time. */
#define SGL_CURVE_NORM_MAX      255

/**
 * @brief sgl curve struct
 * @obj: sgl general object
 * @desc: bezier curve widget, supports quadratic (3 points) and cubic (4 points) curves
 */
typedef struct sgl_curve {
    sgl_obj_t       obj;
    sgl_color_t     color;                              /* curve stroke color */
    uint8_t         norm_pts[SGL_CURVE_MAX_POINTS][2];  /* normalized control points [0..255] */
    uint8_t         point_count;                        /* 3 = quadratic, 4 = cubic */
    uint8_t         thickness;                          /* stroke half-width (radius) */
    uint8_t         alpha;
}sgl_curve_t;

/**
 * @brief create a curve object
 * @param parent parent of the curve
 * @return curve object
 */
sgl_obj_t* sgl_curve_create(sgl_obj_t* parent);

/**
 * @brief set a quadratic bezier curve (3 control points, normalized 0..255)
 * @param obj curve object
 * @param x0,y0 start point
 * @param x1,y1 control point
 * @param x2,y2 end point
 * @return none
 */
void sgl_curve_set_quad(sgl_obj_t *obj, uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2);

/**
 * @brief set a cubic bezier curve (4 control points, normalized 0..255)
 * @param obj curve object
 * @param x0,y0 start point
 * @param x1,y1 first control point
 * @param x2,y2 second control point
 * @param x3,y3 end point
 * @return none
 */
void sgl_curve_set_cubic(sgl_obj_t *obj, uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1,
                         uint8_t x2, uint8_t y2, uint8_t x3, uint8_t y3);

/**
 * @brief set the stroke color of the curve
 * @param obj curve object
 * @param color stroke color
 * @return none
 */
void sgl_curve_set_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set the stroke thickness (half-width/radius) of the curve
 * @param obj curve object
 * @param thickness stroke half-width in pixels, must be > 0
 * @return none
 */
void sgl_curve_set_thickness(sgl_obj_t *obj, uint8_t thickness);

/**
 * @brief set the alpha of the curve
 * @param obj curve object
 * @param alpha alpha of the curve
 * @return none
 */
void sgl_curve_set_alpha(sgl_obj_t *obj, uint8_t alpha);

#ifdef __cplusplus
}
#endif

#endif // !__SGL_CURVE_H__
