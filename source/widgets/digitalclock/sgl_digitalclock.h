/* source/widgets/digitalclock/sgl_digitalclock.h
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

#ifndef __SGL_DIGITALCLOCK_H__
#define __SGL_DIGITALCLOCK_H__

#include <sgl_core.h>
#include <sgl_draw.h>
#include <sgl_anim.h>
#include <sgl_log.h>
#include <sgl_mm.h>
#include <sgl_cfgfix.h>

#ifdef __cplusplus
extern "C" {
#endif

/* radius value that restores the size-dependent default */
#define SGL_DIGITALCLOCK_RADIUS_AUTO       (-1)

/**
 * @brief HH:MM:SS split-flap clock widget
 */
typedef struct sgl_digitalclock {
    sgl_obj_t       obj;
    const sgl_font_t *font;
    sgl_color_t     card_color;
    sgl_color_t     text_color;
    sgl_color_t     flip_color;
    sgl_color_t     flip_text_color;
    uint8_t         flip_contrast;
    int16_t         radius;
    uint8_t         digits[6];
    uint8_t         old_digits[6];
    uint8_t         alpha;
    uint16_t        progress;
    uint16_t        flip_ms;
} sgl_digitalclock_t;

/**
 * @brief Create a split-flap digital clock. The initial value is 00:00:00.
 * @param parent Parent object, NULL uses the active screen.
 * @return Clock object, or NULL when memory allocation fails.
 */
sgl_obj_t* sgl_digitalclock_create(sgl_obj_t *parent);

/**
 * @brief Set the displayed time and flip every changed digit.
 * @param obj Clock object.
 * @param hour Hour, 0-23.
 * @param min Minute, 0-59.
 * @param sec Second, 0-59.
 * @note A pending flip is completed before a newly requested flip starts.
 */
void sgl_digitalclock_set_time(sgl_obj_t *obj, uint8_t hour, uint8_t min, uint8_t sec);

/**
 * @brief Set the digit font.
 * @param obj Clock object.
 * @param font Font pointer, NULL restores the system font.
 */
void sgl_digitalclock_set_font(sgl_obj_t *obj, const sgl_font_t *font);

/**
 * @brief Set the duration of a complete two-stage flip.
 * @param obj Clock object.
 * @param duration_ms Duration in milliseconds; 0 disables animation.
 */
void sgl_digitalclock_set_flip_time(sgl_obj_t *obj, uint16_t duration_ms);

/**
 * @brief Set the background color of the digit cards.
 * @param obj Clock object.
 * @param color Card fill color.
 */
void sgl_digitalclock_set_card_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief Set the color of the static digit text on settled cards.
 * @param obj Clock object.
 * @param color Digit text color.
 */
void sgl_digitalclock_set_text_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief Set the color shown by the moving leaf while a digit flips.
 * @param obj Clock object.
 * @param color Highlight color; it is mixed back to the card color as the leaf settles.
 */
void sgl_digitalclock_set_flip_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief Set the digit color shown by the moving leaf while a digit flips.
 * @param obj Clock object.
 * @param color Digit color on the moving leaf.
 */
void sgl_digitalclock_set_flip_text_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief Set how strongly the moving leaf uses the flip colors.
 * @param obj Clock object.
 * @param contrast 0 keeps the settled colors, 255 uses the flip colors at full strength.
 */
void sgl_digitalclock_set_flip_contrast(sgl_obj_t *obj, uint8_t contrast);

/**
 * @brief Set the overall opacity applied to all card, text and colon drawing.
 * @param obj Clock object.
 * @param alpha Opacity value 0 (transparent) to 255 (fully opaque).
 */
void sgl_digitalclock_set_alpha(sgl_obj_t *obj, uint8_t alpha);

/**
 * @brief Set the corner radius of the cards.
 * @param obj Clock object.
 * @param radius Corner radius in pixels; SGL_DIGITALCLOCK_RADIUS_AUTO restores
 *               the size-dependent default, values are clamped to the card size.
 */
void sgl_digitalclock_set_radius(sgl_obj_t *obj, int16_t radius);

#ifdef __cplusplus
}
#endif

#endif /* __SGL_DIGITALCLOCK_H__ */
