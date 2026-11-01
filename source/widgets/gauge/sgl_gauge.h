/* source/widgets/sgl_gauge.h
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

#ifndef __SGL_GAUGE_H__
#define __SGL_GAUGE_H__

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

#define  SGL_GAUGE_COLOR        sgl_rgb(255, 255, 255)
#define  SGL_GAUGE_BG_COLOR     sgl_rgb(0, 0, 0)
#define  SGL_GAUGE_ALPHA        255

/**
 * @brief sgl gauge widget, How to use, For example:
 *   sgl_obj_t *gauge = sgl_gauge_create(NULL);
 *   sgl_obj_set_pos(gauge, 0, 0);
 *   sgl_obj_set_size(gauge, 100, 100);
 *   sgl_gauge_set_font(gauge, &consolas14);
 */

/**
 * @brief sgl gauge struct
 * @obj: sgl general object
 */
typedef struct sgl_gauge {
    sgl_obj_t  obj;
    int16_t    scale_start;
    uint16_t   scale_step;
    uint8_t    scale_angle;
    uint8_t    text_interval;
    int16_t    angle_start;
    int16_t    angle_end;
    sgl_color_t bg_color;
    sgl_color_t scale_color;
    sgl_color_t pointer_color;
    sgl_color_t text_color;
    int16_t     scale_warning;
    const sgl_font_t  *font;
    sgl_color_t arc_color;
    int16_t     value;
    uint8_t     alpha;
    uint8_t     scale_width;
    uint8_t     pointer_width;
    uint8_t     arc_width;
    sgl_color_t hub_color;
    uint8_t     hub_r;
    uint8_t     scale_length;
} sgl_gauge_t;

/**
 * @brief create a gauge object
 * @param parent parent of the gauge
 * @return gauge object
 */
sgl_obj_t* sgl_gauge_create(sgl_obj_t* parent);

/**
 * @brief set gauge background color
 * @param obj gauge object
 * @param color gauge background color
 * @return none
 */
void sgl_gauge_set_bg_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set gauge arc color
 * @param obj gauge object
 * @param color gauge arc color
 * @return none
 */
void sgl_gauge_set_arc_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set gauge pointer color
 * @param obj gauge object
 * @param color gauge pointer color
 * @return none
 */
void sgl_gauge_set_pointer_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set gauge scale color
 * @param obj gauge object
 * @param color gauge scale color
 * @return none
 */
void sgl_gauge_set_scale_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set gauge text color
 * @param obj gauge object
 * @param color gauge text color
 * @return none
 */
void sgl_gauge_set_text_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set gauge hub color
 * @param obj gauge object
 * @param color gauge hub color
 * @return none
 */
void sgl_gauge_set_hub_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set gauge arc width
 * @param obj gauge object
 * @param width gauge arc width
 * @return none
 */
void sgl_gauge_set_arc_width(sgl_obj_t *obj, uint8_t width);

/**
 * @brief set gauge pointer width
 * @param obj gauge object
 * @param width gauge pointer width
 * @return none
 */
void sgl_gauge_set_pointer_width(sgl_obj_t *obj, uint8_t width);

/**
 * @brief set gauge scale width
 * @param obj gauge object
 * @param width gauge scale width
 * @return none
 */
void sgl_gauge_set_scale_width(sgl_obj_t *obj, uint8_t width);

/**
 * @brief set gauge scale length
 * @param obj gauge object
 * @param length gauge scale length
 * @return none
 */
void sgl_gauge_set_scale_length(sgl_obj_t *obj, uint8_t length);

/**
 * @brief set gauge scale angle
 * @param obj gauge object
 * @param angle gauge scale angle
 * @return none
 */
void sgl_gauge_set_scale_angle(sgl_obj_t *obj, int16_t angle);

/**
 * @brief set gauge scale warning value
 * @param obj gauge object
 * @param value gauge scale warning value
 * @return none
 */
void sgl_gauge_set_scale_warning_value(sgl_obj_t *obj, int16_t value);

/**
 * @brief set gauge angle range
 * @param obj gauge object
 * @param start gauge angle start
 * @param end gauge angle end
 * @return none
 */
void sgl_gauge_set_angle_range(sgl_obj_t *obj, int16_t start, int16_t end);

/**
 * @brief set gauge value
 * @param obj gauge object
 * @param value gauge value
 * @return none
 */
void sgl_gauge_set_value(sgl_obj_t *obj, int16_t value);

/**
 * @brief get gauge value
 * @param obj gauge object
 * @return gauge value
 */
int16_t sgl_gauge_get_value(sgl_obj_t *obj);

/**
 * @brief set gauge font
 * @param obj gauge object
 * @param font gauge font
 * @return none
 */
void sgl_gauge_set_font(sgl_obj_t *obj, const sgl_font_t *font);

/**
 * @brief set gauge hub radius
 * @param obj gauge object
 * @param radius gauge hub radius
 * @return none
 */
void sgl_gauge_set_hub_radiue(sgl_obj_t *obj, uint8_t radius);

/**
 * @brief set gauge scale start value
 * @param obj gauge object
 * @param value gauge scale start value
 * @return none
 */
void sgl_gauge_set_scale_start_value(sgl_obj_t *obj, int16_t value);

/**
 * @brief set gauge scale step value
 * @param obj gauge object
 * @param value gauge scale step value
 * @return none
 */
void sgl_gauge_set_scale_step_value(sgl_obj_t *obj, uint16_t value);

/**
 * @brief set gauge text interval
 * @param obj gauge object
 * @param interval gauge text interval
 * @return none
 */
void sgl_gauge_set_text_interval(sgl_obj_t *obj, uint8_t interval);

/**
 * @brief set gauge alpha
 * @param obj gauge object
 * @param alpha gauge alpha
 * @return none
 */
void sgl_gauge_set_alpha(sgl_obj_t *obj, uint8_t alpha);

#ifdef __cplusplus
}
#endif

#endif // !__SGL_GAUGE_H__
