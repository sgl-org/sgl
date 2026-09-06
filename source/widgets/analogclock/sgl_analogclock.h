/* source/widgets/sgl_analogclock.h
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

#ifndef __SGL_ANALOGCLOCK_H__
#define __SGL_ANALOGCLOCK_H__

#include <sgl_core.h>
#include <sgl_draw.h>
#include <sgl_math.h>
#include <sgl_log.h>
#include <sgl_mm.h>
#include <sgl_cfgfix.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief sgl analogclock widget
 */
typedef struct sgl_analogclock {
    sgl_obj_t       obj;
    sgl_color_t     bg_color;
    sgl_color_t     scale_color;
    sgl_color_t     hour_ptr_color;
    sgl_color_t     min_ptr_color;
    sgl_color_t     sec_ptr_color;
    sgl_color_t     text_color;
    sgl_color_t     border_color;
    uint8_t         scale_width;
    uint8_t         hour_ptr_width;
    uint8_t         min_ptr_width;
    uint8_t         sec_ptr_width;
    uint8_t         hub_r;
    uint8_t         scale_len;

    const sgl_font_t *font;

    uint8_t         alpha;
    uint8_t         hour;           /* 0-23 */
    uint8_t         min;            /* 0-59 */
    uint8_t         sec;            /* 0-59 */
} sgl_analogclock_t;

/**
 * @brief Create an analog clock widget.
 * 
 * Allocates memory for a new analog clock object, initializes its properties 
 * with default theme values, and sets up the drawing callback.
 * 
 * @param parent Pointer to the parent object.
 * @return Pointer to the newly created sgl_obj_t object, or NULL if memory allocation fails.
 */
sgl_obj_t* sgl_analogclock_create(sgl_obj_t* parent);

/**
 * @brief Set the time value for the analog clock.
 * 
 * Updates the hour, minute, and second values of the clock. If the new time 
 * differs from the current time, it updates the clock's internal state and 
 * triggers a redraw of the affected area.
 * 
 * @param obj Pointer to the analog clock object.
 * @param hour The hour value (0-23).
 * @param min The minute value (0-59).
 * @param sec The second value (0-59).
 */
void sgl_analogclock_set_time(sgl_obj_t *obj, uint8_t hour, uint8_t min, uint8_t sec);

/**
 * @brief Set the font for the clock's hour numbers.
 * @param obj Pointer to the analog clock object.
 * @param font Pointer to the font to be used.
 */
void sgl_analogclock_set_font(sgl_obj_t *obj, const sgl_font_t *font);

/**
 * @brief Set the background color of the clock.
 * @param obj Pointer to the analog clock object.
 * @param color The background color to set.
 */
void sgl_analogclock_set_bg_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief Set the border color of the clock.
 * @param obj Pointer to the analog clock object.
 * @param color The border color to set.
 */
void sgl_analogclock_set_border_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief Set the scale (tick marks) color of the clock.
 * @param obj Pointer to the analog clock object.
 * @param color The scale color to set.
 */
void sgl_analogclock_set_scale_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief Set the hour pointer (hand) color of the clock.
 * @param obj Pointer to the analog clock object.
 * @param color The hour pointer color to set.
 */
void sgl_analogclock_set_hour_ptr_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief Set the minute pointer (hand) color of the clock.
 * @param obj Pointer to the analog clock object.
 * @param color The minute pointer color to set.
 */
void sgl_analogclock_set_min_ptr_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief Set the second pointer (hand) color of the clock.
 * @param obj Pointer to the analog clock object.
 * @param color The second pointer color to set.
 */
void sgl_analogclock_set_sec_ptr_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief Set the center hub color of the clock.
 * @param obj Pointer to the analog clock object.
 * @param color The hub color to set.
 */
void sgl_analogclock_set_hub_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief Set the text (hour numbers) color of the clock.
 * @param obj Pointer to the analog clock object.
 * @param color The text color to set.
 */
void sgl_analogclock_set_text_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief Set the border width of the clock.
 * @param obj Pointer to the analog clock object.
 * @param width The border width to set.
 */
void sgl_analogclock_set_border_width(sgl_obj_t *obj, uint8_t width);

/**
 * @brief Set the center hub radius of the clock.
 * @param obj Pointer to the analog clock object.
 * @param radius The hub radius to set.
 */
void sgl_analogclock_set_hub_radius(sgl_obj_t *obj, uint8_t radius);

/**
 * @brief Set the scale (tick marks) width of the clock.
 * @param obj Pointer to the analog clock object.
 * @param width The scale width to set.
 */
void sgl_analogclock_set_scale_width(sgl_obj_t *obj, uint8_t width);

/**
 * @brief Set the hour pointer (hand) width of the clock.
 * @param obj Pointer to the analog clock object.
 * @param width The hour pointer width to set.
 */
void sgl_analogclock_set_hour_ptr_width(sgl_obj_t *obj, uint8_t width);

/**
 * @brief Set the minute pointer (hand) width of the clock.
 * @param obj Pointer to the analog clock object.
 * @param width The minute pointer width to set.
 */
void sgl_analogclock_set_min_ptr_width(sgl_obj_t *obj, uint8_t width);

/**
 * @brief Set the second pointer (hand) width of the clock.
 * @param obj Pointer to the analog clock object.
 * @param width The second pointer width to set.
 */
void sgl_analogclock_set_sec_ptr_width(sgl_obj_t *obj, uint8_t width);

/**
 * @brief Set the overall alpha (transparency) of the clock.
 * @param obj Pointer to the analog clock object.
 * @param alpha The alpha value to set (0-255).
 */
void sgl_analogclock_set_alpha(sgl_obj_t *obj, uint8_t alpha);

#ifdef __cplusplus
}
#endif

#endif // !__SGL_ANALOGCLOCK_H__
