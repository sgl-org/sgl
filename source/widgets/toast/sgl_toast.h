/* source/widgets/toast/sgl_toast.h
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

#ifndef __SGL_TOAST_H__
#define __SGL_TOAST_H__

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
 * @brief toast object, a non-interactive transient notification
 * @obj: base object
 * @text: message text to display, centered in the box
 * @font: text font
 * @bg_color: background rounded rect color
 * @text_color: text color
 * @alpha_max: peak alpha used while the toast is visible [0, 255]
 * @alpha_cur: current alpha driven by the fade out animation
 * @hold_ms: time the toast stays at alpha_max before fading out
 * @fade_ms: fade out time, alpha ramps down slowly during this phase
 * @note the life cycle is appear at once -> hold -> fade out, driven by one
 *       delayed linear animation (delay = hold_ms, duration = fade_ms), so
 *       no extra timer is required
 */
typedef struct sgl_toast {
    sgl_obj_t        obj;
    const char       *text;
    const sgl_font_t *font;
    sgl_color_t      bg_color;
    sgl_color_t      text_color;
    uint8_t          alpha_max;
    uint8_t          alpha_cur;
    uint16_t         hold_ms;
    uint16_t         fade_ms;
} sgl_toast_t;

/**
 * @brief create a toast object
 * @param parent parent object
 * @return toast object
 * @note the object is invisible until sgl_toast_show() is called; it is not
 *       interactive (clickable is disabled)
 */
sgl_obj_t* sgl_toast_create(sgl_obj_t* parent);

/**
 * @brief set toast message text
 * @param obj toast object
 * @param text message text, the pointer must stay valid while shown
 * @return none
 */
void sgl_toast_set_text(sgl_obj_t *obj, const char *text);

/**
 * @brief set toast font
 * @param obj toast object
 * @param font font used to draw the text
 * @return none
 */
void sgl_toast_set_font(sgl_obj_t *obj, const sgl_font_t *font);

/**
 * @brief set toast background color
 * @param obj toast object
 * @param color background color
 * @return none
 */
void sgl_toast_set_bg_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set toast text color
 * @param obj toast object
 * @param color text color
 * @return none
 */
void sgl_toast_set_text_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set toast peak alpha
 * @param obj toast object
 * @param alpha peak alpha of both background and text [0, 255]
 * @return none
 */
void sgl_toast_set_alpha(sgl_obj_t *obj, uint8_t alpha);

/**
 * @brief set toast hold time
 * @param obj toast object
 * @param hold_ms time at full alpha before fading out, ms
 * @return none
 * @note takes effect on the next sgl_toast_show() while a toast is active
 */
void sgl_toast_set_duration(sgl_obj_t *obj, uint16_t hold_ms);

/**
 * @brief set toast fade out time
 * @param obj toast object
 * @param fade_ms time the alpha ramps down from peak to 0, ms
 * @return none
 * @note takes effect on the next sgl_toast_show() while a toast is active
 */
void sgl_toast_set_fade_time(sgl_obj_t *obj, uint16_t fade_ms);

/**
 * @brief show the toast and start its life cycle
 * @param obj toast object
 * @return none
 * @note the toast appears at once, holds, then its alpha ramps down to 0 and
 *       the object destroys itself automatically; calling show() again on an
 *       active toast restarts the life cycle
 */
void sgl_toast_show(sgl_obj_t *obj);

/**
 * @brief check whether the toast life cycle is running
 * @param obj toast object
 * @return true if the toast is holding or fading out
 */
bool sgl_toast_is_showing(sgl_obj_t *obj);

#ifdef __cplusplus
}
#endif

#endif // !__SGL_TOAST_H__
