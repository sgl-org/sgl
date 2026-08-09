/* source/widgets/curve/sgl_curve.c
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

#include <sgl_core.h>
#include <sgl_draw.h>
#include <sgl_math.h>
#include <sgl_log.h>
#include <sgl_mm.h>
#include <sgl_theme.h>
#include <sgl_cfgfix.h>
#include <string.h>
#include "sgl_curve.h"


/* Map a normalized [0..255] value onto [lo..hi] */
static int16_t sgl_curve_map(uint8_t norm, int16_t lo, int16_t hi)
{
    return (int16_t)(lo + ((int32_t)norm * (hi - lo) + (SGL_CURVE_NORM_MAX / 2)) / SGL_CURVE_NORM_MAX);
}

static void sgl_curve_construct_cb(sgl_surf_t *surf, sgl_obj_t* obj, sgl_event_t *evt)
{
    sgl_curve_t *curve = sgl_container_of(obj, sgl_curve_t, obj);

    if(evt->type == SGL_EVENT_DRAW_MAIN) {
        if (curve->point_count < 3 || curve->thickness == 0 || curve->alpha == SGL_ALPHA_MIN) return;

        /* 映射范围向内缩进 pad，保证圆头端帽和 AA 带完全落在控件矩形内，
         * 避免越界绘制被脏区/条带裁剪截断 */
        const int32_t pad = curve->thickness + 3;
        int16_t lo_x = (int16_t)(obj->coords.x1 + pad);
        int16_t hi_x = (int16_t)(obj->coords.x2 - pad);
        int16_t lo_y = (int16_t)(obj->coords.y1 + pad);
        int16_t hi_y = (int16_t)(obj->coords.y2 - pad);
        if (hi_x < lo_x) lo_x = hi_x = (int16_t)((obj->coords.x1 + obj->coords.x2) / 2);
        if (hi_y < lo_y) lo_y = hi_y = (int16_t)((obj->coords.y1 + obj->coords.y2) / 2);

        /* map normalized control points onto the padded widget rectangle */
        int16_t px[SGL_CURVE_MAX_POINTS], py[SGL_CURVE_MAX_POINTS];
        for (int i = 0; i < curve->point_count; i++) {
            px[i] = sgl_curve_map(curve->norm_pts[i][0], lo_x, hi_x);
            py[i] = sgl_curve_map(curve->norm_pts[i][1], lo_y, hi_y);
        }

        /* bounding box of control points padded by thickness + AA margin.
         * The bezier curve always stays inside its control polygon. */
        int32_t min_x = px[0], max_x = px[0], min_y = py[0], max_y = py[0];
        for (int i = 1; i < curve->point_count; i++) {
            if (px[i] < min_x) min_x = px[i];
            if (px[i] > max_x) max_x = px[i];
            if (py[i] < min_y) min_y = py[i];
            if (py[i] > max_y) max_y = py[i];
        }
        sgl_bezier_mask_t mask = {
            .x1 = min_x - pad, .y1 = min_y - pad,
            .x2 = max_x + pad, .y2 = max_y + pad,
        };
        const int32_t bytes = (mask.x2 - mask.x1 + 1) * (mask.y2 - mask.y1 + 1);
        if (bytes <= 0) return;
        mask.data = sgl_malloc((uint32_t)bytes);
        if (mask.data == NULL) {
            SGL_LOG_ERROR("sgl_curve: mask malloc failed (%d bytes)", bytes);
            return;
        }

        if (curve->point_count == 3) {
            sgl_draw_bezier_quad(surf, &obj->area,
                                 px[0], py[0], px[1], py[1], px[2], py[2],
                                 curve->thickness, curve->color, curve->alpha, &mask);
        } else {
            sgl_draw_bezier_cubic(surf, &obj->area,
                                  px[0], py[0], px[1], py[1], px[2], py[2], px[3], py[3],
                                  curve->thickness, curve->color, curve->alpha, &mask);
        }

        sgl_free(mask.data);
    }
}

/**
 * @brief create a curve object
 * @param parent parent of the curve
 * @return curve object
 */
sgl_obj_t* sgl_curve_create(sgl_obj_t* parent)
{
    sgl_curve_t *curve = sgl_malloc(sizeof(sgl_curve_t));
    if(curve == NULL) {
        SGL_LOG_ERROR("sgl_curve_create: malloc failed");
        return NULL;
    }

    /* set object all member to zero */
    memset(curve, 0, sizeof(sgl_curve_t));

    sgl_obj_t *obj = &curve->obj;
    sgl_obj_init(&curve->obj, parent);
    obj->construct_fn = sgl_curve_construct_cb;

    curve->alpha = SGL_ALPHA_MAX;
    curve->color = SGL_THEME_COLOR;
    curve->thickness = 3;
    curve->point_count = 0;

    return obj;
}

/**
 * @brief set a quadratic bezier curve (3 control points, normalized 0..255)
 * @param obj curve object
 * @return none
 */
void sgl_curve_set_quad(sgl_obj_t *obj, uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2)
{
    sgl_curve_t *curve = sgl_container_of(obj, sgl_curve_t, obj);
    curve->norm_pts[0][0] = x0; curve->norm_pts[0][1] = y0;
    curve->norm_pts[1][0] = x1; curve->norm_pts[1][1] = y1;
    curve->norm_pts[2][0] = x2; curve->norm_pts[2][1] = y2;
    curve->point_count = 3;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set a cubic bezier curve (4 control points, normalized 0..255)
 * @param obj curve object
 * @return none
 */
void sgl_curve_set_cubic(sgl_obj_t *obj, uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1,
                         uint8_t x2, uint8_t y2, uint8_t x3, uint8_t y3)
{
    sgl_curve_t *curve = sgl_container_of(obj, sgl_curve_t, obj);
    curve->norm_pts[0][0] = x0; curve->norm_pts[0][1] = y0;
    curve->norm_pts[1][0] = x1; curve->norm_pts[1][1] = y1;
    curve->norm_pts[2][0] = x2; curve->norm_pts[2][1] = y2;
    curve->norm_pts[3][0] = x3; curve->norm_pts[3][1] = y3;
    curve->point_count = 4;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the stroke color of the curve
 * @param obj curve object
 * @param color stroke color
 * @return none
 */
void sgl_curve_set_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_curve_t *curve = sgl_container_of(obj, sgl_curve_t, obj);
    curve->color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the stroke thickness (half-width/radius) of the curve
 * @param obj curve object
 * @param thickness stroke half-width in pixels, must be > 0
 * @return none
 */
void sgl_curve_set_thickness(sgl_obj_t *obj, uint8_t thickness)
{
    sgl_curve_t *curve = sgl_container_of(obj, sgl_curve_t, obj);
    if (thickness == 0) thickness = 1;
    curve->thickness = thickness;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the alpha of the curve
 * @param obj curve object
 * @param alpha alpha of the curve
 * @return none
 */
void sgl_curve_set_alpha(sgl_obj_t *obj, uint8_t alpha)
{
    sgl_curve_t *curve = sgl_container_of(obj, sgl_curve_t, obj);
    curve->alpha = alpha;
    sgl_obj_set_dirty(obj);
}
