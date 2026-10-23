/* source/widgets/spinner/sgl_spinner.h
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
#include <sgl_anim.h>
#include <sgl_draw.h>
#include <sgl_math.h>
#include <sgl_log.h>
#include <sgl_mm.h>
#include <sgl_cfgfix.h>
#include <string.h>

#ifndef __SGL_SPINNER_H__
#define __SGL_SPINNER_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief spinner object, a rotating loading indicator made of dots
 * @obj: base object
 * @color: dot color
 * @radius_in: inner radius of the dot ring path
 * @radius_out: outer radius of the dot ring path
 * @angle: leading dot angle, deg [0, 360)
 * @period: time of one full turn, ms
 * @seg_cnt: number of dots around the ring
 * @alpha: alpha of the head dot (the tail fades out from it)
 */
typedef struct sgl_spinner {
    sgl_obj_t       obj;
    sgl_color_t     color;
    int16_t         radius_in;
    int16_t         radius_out;
    int16_t         angle;
    uint16_t        period;
    uint8_t         seg_cnt;
    uint8_t         alpha;
} sgl_spinner_t;

/**
 * @brief create a spinner object
 * @param parent parent object
 * @return spinner object
 */
sgl_obj_t* sgl_spinner_create(sgl_obj_t* parent);

/**
 * @brief set spinner color
 * @param obj spinner object
 * @param color spinner color
 * @return none
 */
void sgl_spinner_set_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set spinner alpha
 * @param obj spinner object
 * @param alpha spinner alpha (brightness of the head dot)
 * @return none
 */
void sgl_spinner_set_alpha(sgl_obj_t *obj, uint8_t alpha);

/**
 * @brief set spinner ring radius
 * @param obj spinner object
 * @param radius_in inner radius
 * @param radius_out outer radius
 * @return none
 * @note the dot diameter is (radius_out - radius_in) / 2 and the dots run on
 *       the mid radius; when never set, the radius is derived from the object
 *       size on draw
 */
void sgl_spinner_set_radius(sgl_obj_t *obj, int16_t radius_in, int16_t radius_out);

/**
 * @brief set spinner dot count
 * @param obj spinner object
 * @param cnt dot count, clamped to [3, 30]
 * @return none
 */
void sgl_spinner_set_seg_count(sgl_obj_t *obj, uint8_t cnt);

/**
 * @brief set spinner rotation period
 * @param obj spinner object
 * @param period_ms time of one full turn in ms
 * @return none
 * @note restarting the internal animation if the spinner is running
 */
void sgl_spinner_set_period(sgl_obj_t *obj, uint16_t period_ms);

/**
 * @brief set spinner leading angle manually
 * @param obj spinner object
 * @param angle leading dot angle, deg, normalized to [0, 360)
 * @return none
 */
void sgl_spinner_set_angle(sgl_obj_t *obj, int16_t angle);

/**
 * @brief get spinner leading angle
 * @param obj spinner object
 * @return leading angle, deg
 */
int16_t sgl_spinner_get_angle(sgl_obj_t *obj);

/**
 * @brief start the spinner rotation animation
 * @param obj spinner object
 * @return none
 * @note no-op when an animation is already attached to the object
 */
void sgl_spinner_start(sgl_obj_t *obj);

/**
 * @brief stop the spinner rotation animation
 * @param obj spinner object
 * @return none
 */
void sgl_spinner_stop(sgl_obj_t *obj);

/**
 * @brief check whether the spinner is rotating
 * @param obj spinner object
 * @return true if rotating
 */
bool sgl_spinner_is_running(sgl_obj_t *obj);

#ifdef __cplusplus
}
#endif

#endif // !__SGL_SPINNER_H__
