/* source/widgets/spectrum/sgl_spectrum.h
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

#ifndef __SGL_SPECTRUM_H__
#define __SGL_SPECTRUM_H__

#include <sgl_core.h>
#include <sgl_draw.h>
#include <sgl_math.h>
#include <sgl_log.h>
#include <sgl_mm.h>
#include <sgl_cfgfix.h>
#include <string.h>

#define SGL_SPECTRUM_MODE_HAT_FLAG                 (1 << 2)
#define SGL_SPECTRUM_MODE_BAR                      (1 << 0)
#define SGL_SPECTRUM_MODE_BLOCK                    (1 << 1)
#define SGL_SPECTRUM_MODE_BAR_HAT                  (SGL_SPECTRUM_MODE_HAT_FLAG | SGL_SPECTRUM_MODE_BAR)
#define SGL_SPECTRUM_MODE_BLOCK_HAT                (SGL_SPECTRUM_MODE_HAT_FLAG | SGL_SPECTRUM_MODE_BLOCK)
#define SGL_SPECTRUM_MODE_GRADIENT                 (1 << 3)

/**
 * @brief horizontal gradient: bar colour sweeps bar_color (leftmost bar) to
 *        bar_color_low (rightmost bar). One colour per BAR, so it costs the
 *        same as flat mode - no per-pixel work. Combine with BAR or BLOCK.
 */
#define SGL_SPECTRUM_MODE_HGRADIENT                (1 << 4)

/* maximum number of bars; all buffers are static so no dynamic memory */
#define SGL_SPECTRUM_BAR_MAX                       (64)

/* maximum radius used for the rounded bar caps */
#define SGL_SPECTRUM_ROUND_MAX                     (12)

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief sgl spectrum struct
 * @obj: sgl general object
 * @bar_color: main bar colour (top of the gradient)
 * @bar_color_low: bottom of the gradient, only used when bar_gradient is set
 * @bar_hat_color: colour of the floating peak-hold cap
 * @floor_color: colour of the optional baseline band
 * @bar_num: active bar count, <= SGL_SPECTRUM_BAR_MAX
 * @bar_height: widget inner height in px (cached)
 * @bar_width: uniform bar width in px
 * @bar_gap: gap between bars in px
 * @bar_mode: bitmask of SGL_SPECTRUM_MODE_*
 * @alpha: overall opacity
 * @bar_hat_height: height of the peak-hold cap and of one block segment
 * @round_radius: corner radius of the bar tops
 * @peak_fall: px the peak cap drops per update, 0 = hold forever
 * @floor_height: height of the baseline band, 0 = disabled
 * @cut: per-row corner cut (px) of the rounded cap, cut[0] is the top row
 * @grad_step: Q16.16 per-row gradient factor step, avoids a per-row division
 * @bar_colors: per-bar colour cache, one entry per bar (HGRADIENT mode)
 * @bar_gradient: 1 when the vertical gradient is active
 * @gap_auto: 1 when the gap is chosen automatically for the bar count
 * @align_center: 1 to centre the bar row inside the widget
 */
typedef struct sgl_spectrum {
    sgl_obj_t   obj;
    sgl_color_t bar_color;
    sgl_color_t bar_color_low;
    sgl_color_t bar_hat_color;
    sgl_color_t floor_color;
    uint16_t    bar_num;         /* active bar count, <= BAR_MAX      */
    uint16_t    bar_height;      /* widget inner height in px (cached) */
    uint8_t     bar_width;       /* uniform bar width in px            */
    uint8_t     bar_gap;         /* gap between bars in px             */
    uint8_t     bar_mode;
    uint8_t     alpha;
    uint8_t     bar_hat_height;
    uint8_t     round_radius;    /* rounded bar cap radius             */
    uint8_t     peak_fall;       /* peak cap fall speed, px per update */
    uint8_t     floor_height;    /* baseline band height, 0 = off      */
    uint8_t     cut[SGL_SPECTRUM_ROUND_MAX]; /* cap corner cut per row */
    uint32_t    grad_step;       /* Q16.16 gradient factor per row     */
    sgl_color_t bar_colors[SGL_SPECTRUM_BAR_MAX]; /* HGRADIENT cache    */
    uint8_t     bar_gradient : 1;
    uint8_t     gap_auto : 1;
    uint8_t     align_center : 1;
    int16_t     layout_x;        /* coords.x1 snapshot at layout time */
    uint16_t    bar_value[SGL_SPECTRUM_BAR_MAX];
    uint16_t    bar_peek[SGL_SPECTRUM_BAR_MAX];  /* peak-hold heights   */
    int16_t     bar_x[SGL_SPECTRUM_BAR_MAX];     /* cached left x       */
} sgl_spectrum_t;

/**
 * @brief create a spectrum object
 * @param parent parent of the spectrum
 * @return spectrum object
 */
sgl_obj_t* sgl_spectrum_create(sgl_obj_t* parent);

/**
 * @brief set spectrum bar number
 * @param obj spectrum object
 * @param number bar number
 * @return none
 * @note re-layouts the bars and resets the value/peak buffers; safe to call
 *       again with a different number.
 */
void sgl_spectrum_set_bar_number(sgl_obj_t *obj, uint16_t number);

/**
 * @brief set spectrum bar value
 * @param obj spectrum object
 * @param index bar index
 * @param value bar value, 0..widget height in px (clamped)
 * @return none
 * @note only the changed rows are marked dirty.
 */
void sgl_spectrum_set_bar_value(sgl_obj_t *obj, uint16_t index, uint16_t value);

/**
 * @brief set spectrum bar mode
 * @param obj spectrum object
 * @param mode bar mode
 * @return none
 * @note mode value:
 *       SGL_SPECTRUM_MODE_BAR: bar mode
 *       SGL_SPECTRUM_MODE_BLOCK: block mode
 *       SGL_SPECTRUM_MODE_BAR_HAT: bar mode with hat
 *       SGL_SPECTRUM_MODE_BLOCK_HAT: block mode with hat
 *       SGL_SPECTRUM_MODE_GRADIENT: OR this in for a vertical gradient
 *       SGL_SPECTRUM_MODE_HGRADIENT: OR this in for a left-to-right gradient
 */
void sgl_spectrum_set_bar_mode(sgl_obj_t *obj, uint8_t mode);

/**
 * @brief set spectrum bar color
 * @param obj spectrum object
 * @param color bar color
 * @return none
 */
void sgl_spectrum_set_bar_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set the bottom (gradient) colour of the bars
 * @param obj spectrum object
 * @param color colour at the bottom of a full-height bar
 * @return none
 */
void sgl_spectrum_set_bar_color_low(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set spectrum bar hat color
 * @param obj spectrum object
 * @param color bar hat color
 * @return none
 */
void sgl_spectrum_set_bar_hat_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set spectrum bar hat height
 * @param obj spectrum object
 * @param height bar hat height
 * @return none
 */
void sgl_spectrum_set_bar_hat_height(sgl_obj_t *obj, uint8_t height);

/**
 * @brief set the corner radius of the bar tops
 * @param obj spectrum object
 * @param radius radius in px, clamped to half the bar width
 * @return none
 */
void sgl_spectrum_set_radius(sgl_obj_t *obj, uint8_t radius);

/**
 * @brief enable or disable the vertical gradient
 * @param obj spectrum object
 * @param enable true to interpolate bar_color -> bar_color_low
 * @return none
 */
void sgl_spectrum_set_gradient(sgl_obj_t *obj, bool enable);

/**
 * @brief set the peak-hold fall speed
 * @param obj spectrum object
 * @param fall_px pixels the cap drops per value update, 0 holds forever
 * @return none
 */
void sgl_spectrum_set_peak_fall(sgl_obj_t *obj, uint8_t fall_px);

/**
 * @brief set a floor / baseline band under the bars
 * @param obj spectrum object
 * @param height band height in px, 0 disables it
 * @param color band color
 * @return none
 */
void sgl_spectrum_set_floor(sgl_obj_t *obj, uint8_t height, sgl_color_t color);

/**
 * @brief force a gap between bars instead of the automatic one
 * @param obj spectrum object
 * @param gap gap in px, 0 restores automatic gap selection
 * @return none
 */
void sgl_spectrum_set_bar_gap(sgl_obj_t *obj, uint8_t gap);

/**
 * @brief set spectrum alpha
 * @param obj spectrum object
 * @param alpha alpha value
 * @return none
 */
void sgl_spectrum_set_alpha(sgl_obj_t *obj, uint8_t alpha);

#ifdef __cplusplus
}
#endif

#endif // !__SGL_SPECTRUM_H__
