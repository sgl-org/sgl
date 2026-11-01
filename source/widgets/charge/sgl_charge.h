/* source/widgets/charge/sgl_charge.h
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

#ifndef __SGL_CHARGE_H__
#define __SGL_CHARGE_H__

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

#define  SGL_CHARGE_BODY_COLOR      sgl_rgb(30, 38, 48)
#define  SGL_CHARGE_FILL_COLOR      sgl_rgb(0, 200, 110)
#define  SGL_CHARGE_BUBBLE_COLOR    sgl_rgb(190, 255, 225)

/* number of bubbles rising inside the liquid */
#define SGL_CHARGE_BUBBLE_NUM       (12)

/**
 * @brief one bubble, position is normalized to the inner cavity
 * @x:   cross axis position, 0..255
 * @off: phase offset in [0, 255], where the bubble sits in the rise cycle
 * @z:   depth in [0, 255], drives the radius and the alpha
 * @spd: rise divisor, the bubble climbs from the wall to the surface every @spd
 *       cycles
 * @note  the height is a pure function of the animation phase, so nothing has
 *        to be accumulated frame by frame
 */
typedef struct sgl_charge_bubble {
    int16_t x;
    int16_t off;
    uint8_t z;
    uint8_t spd;
} sgl_charge_bubble_t;

/**
 * @brief charge widget — rounded shell with an animated liquid
 *
 * Drawing order:
 *   1. the shell, one rounded fill in the body colour that is the frame and the
 *      outline at once, plus the positive terminal
 *   2. the cavity is filled with a dark ramp of the body colour, then the liquid
 *      rises over it with a sine wave surface, a cylindrical highlight and
 *      rising bubbles
 *
 * The object box holds the terminal, the shell and the percentage readout, so
 * the whole widget moves as one object. Everything is drawn opaque.
 *
 * @obj:           sgl general object
 * @font:          percentage font (NULL = use the system font)
 * @fill_color:    liquid colour
 * @body_color:    shell band colour, also the cavity and terminal tint source
 * @bubble_color:  bubble colour
 * @bubbles:       bubbles rising inside the liquid
 * @phase:         wave phase in degrees, driven by sgl_anim
 * @level:         target charge level (0-100)
 * @disp_level:    displayed level, eases toward @level
 */
typedef struct sgl_charge {
    sgl_obj_t                 obj;
    const sgl_font_t          *font;
    sgl_color_t               fill_color;
    sgl_color_t               body_color;
    sgl_color_t               bubble_color;
    sgl_charge_bubble_t       bubbles[SGL_CHARGE_BUBBLE_NUM];
    uint16_t                  phase;
    uint8_t                   level;
    uint8_t                   disp_level;
    uint8_t                   show_percentage : 1;
    uint8_t                   vertical        : 1;
    uint8_t                   animated        : 1;
    uint8_t                   reserved        : 5;
} sgl_charge_t;

/**
 * @brief  create a charge object
 * @param  parent: parent object pointer
 * @return charge object pointer, NULL on failure
 */
sgl_obj_t* sgl_charge_create(sgl_obj_t* parent);

/**
 * @brief set charge level
 * @param  obj: charge object pointer
 * @param  level: charge level (0-100)
 * @return none
 */
void sgl_charge_set_level(sgl_obj_t* obj, uint8_t level);

/**
 * @brief set the liquid fill color
 * @param  obj: charge object pointer
 * @param  color: liquid color to set
 * @return none
 */
void sgl_charge_set_fill_color(sgl_obj_t* obj, sgl_color_t color);

/**
 * @brief set the shell / cavity color
 * @param  obj: charge object pointer
 * @param  color: body color to set
 * @return none
 */
void sgl_charge_set_body_color(sgl_obj_t* obj, sgl_color_t color);

/**
 * @brief set the bubble color
 * @param  obj: charge object pointer
 * @param  color: bubble color to set
 * @return none
 */
void sgl_charge_set_bubble_color(sgl_obj_t* obj, sgl_color_t color);

/**
 * @brief set the font of the percentage readout
 * @param  obj: charge object pointer
 * @param  font: font pointer to set (NULL = use the system font)
 * @return none
 */
void sgl_charge_set_font(sgl_obj_t* obj, const sgl_font_t* font);

/**
 * @brief show or hide the percentage readout
 * @param  obj: charge object pointer
 * @param  show: show the percentage (1 = show, 0 = hide)
 * @return none
 */
void sgl_charge_show_percentage(sgl_obj_t* obj, bool show);

/**
 * @brief set charge orientation
 * @param  obj: charge object pointer
 * @param  vertical: true for vertical, false for horizontal
 * @return none
 */
void sgl_charge_set_vertical(sgl_obj_t* obj, bool vertical);

/**
 * @brief enable or disable the built-in animation
 * @param  obj: charge object pointer
 * @param  animated: true to animate the sine wave and the bubbles
 * @return none
 * @note  the wave runs on a looping sgl_anim bound to the object; disabling
 *        deletes it
 */
void sgl_charge_set_animated(sgl_obj_t* obj, bool animated);

#ifdef __cplusplus
}
#endif

#endif /* __SGL_CHARGE_H__ */
