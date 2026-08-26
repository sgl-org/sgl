/* source/widgets/battery/sgl_battery.h
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

#ifndef __SGL_BATTERY_H__
#define __SGL_BATTERY_H__

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
 * @brief battery widget structure — semi-transparent 3D battery indicator
 * @obj:          sgl general object
 * @font:         font for percentage text (NULL = use system font)
 * @border_color: outer border color
 * @fill_color:   battery bar fill color (0 = auto by level)
 * @low_color:    color when level < 20%%
 * @medium_color: color when level 20-50%%
 * @high_color:   color when level >= 50%%
 * @bg_color:     inner body background color
 * @charging_color: charging indicator color
 * @text_color:   percentage text color
 * @level:        battery level (0-100)
 * @alpha:        body transparency (0=transparent, 255=opaque)
 */
typedef struct sgl_battery {
    sgl_obj_t              obj;
    const sgl_font_t       *font;
    sgl_color_t            border_color;
    sgl_color_t            fill_color;
    sgl_color_t            low_color;
    sgl_color_t            medium_color;
    sgl_color_t            high_color;
    sgl_color_t            bg_color;
    sgl_color_t            charging_color;
    sgl_color_t            text_color;
    uint8_t                level;
    uint8_t                alpha;
    uint8_t                charging        : 1;
    uint8_t                show_percentage : 1;
    uint8_t                vertical        : 1;
    uint8_t                reserved        : 5;
} sgl_battery_t;

/**
 * @brief  create a battery object
 * @param  parent: parent object pointer
 * @return battery object pointer, NULL on failure
 */
sgl_obj_t* sgl_battery_create(sgl_obj_t* parent);

/**
 * @brief set battery level
 * @param  obj: battery object pointer
 * @param  level: battery level (0-100)
 * @return none
 */
void sgl_battery_set_level(sgl_obj_t* obj, uint8_t level);

/**
 * @brief set battery border color
 * @param  obj: battery object pointer
 * @param  color: border color to set
 * @return none
 */
void sgl_battery_set_border_color(sgl_obj_t* obj, sgl_color_t color);

/**
 * @brief set battery fill color
 * @param  obj: battery object pointer
 * @param  color: fill color to set
 * @return none
 */
void sgl_battery_set_fill_color(sgl_obj_t* obj, sgl_color_t color);

/**
 * @brief set low battery color (below 20%%)
 * @param  obj: battery object pointer
 * @param  color: low battery color to set
 * @return none
 */
void sgl_battery_set_low_color(sgl_obj_t* obj, sgl_color_t color);

/**
 * @brief set medium battery color (20-50%%)
 * @param  obj: battery object pointer
 * @param  color: medium battery color to set
 * @return none
 */
void sgl_battery_set_medium_color(sgl_obj_t* obj, sgl_color_t color);

/**
 * @brief set high battery color (above 50%%)
 * @param  obj: battery object pointer
 * @param  color: high battery color to set
 * @return none
 */
void sgl_battery_set_high_color(sgl_obj_t* obj, sgl_color_t color);

/**
 * @brief set battery background color
 * @param  obj: battery object pointer
 * @param  color: background color to set
 * @return none
 */
void sgl_battery_set_bg_color(sgl_obj_t* obj, sgl_color_t color);

/**
 * @brief set charging status
 * @param  obj: battery object pointer
 * @param  charging: charging status (1 = charging, 0 = not charging)
 * @return none
 */
void sgl_battery_set_charging(sgl_obj_t* obj, bool charging);

/**
 * @brief set charging indicator color
 * @param  obj: battery object pointer
 * @param  color: charging indicator color to set
 * @return none
 */
void sgl_battery_set_charging_color(sgl_obj_t* obj, sgl_color_t color);

/**
 * @brief show or hide percentage text
 * @param  obj: battery object pointer
 * @param  show: show percentage text (1 = show, 0 = hide)
 * @return none
 */
void sgl_battery_show_percentage(sgl_obj_t* obj, bool show);

/**
 * @brief set text font for percentage display
 * @param  obj: battery object pointer
 * @param  font: font pointer to set
 * @return none
 */
void sgl_battery_set_font(sgl_obj_t* obj, const sgl_font_t *font);

/**
 * @brief set percentage text color
 * @param  obj: battery object pointer
 * @param  color: text color to set
 * @return none
 */
void sgl_battery_set_text_color(sgl_obj_t* obj, sgl_color_t color);

/**
 * @brief set battery transparency
 * @param  obj: battery object pointer
 * @param  alpha: alpha value (0 = transparent, 255 = opaque)
 * @return none
 */
void sgl_battery_set_alpha(sgl_obj_t* obj, uint8_t alpha);

/**
 * @brief set battery orientation
 * @param  obj: battery object pointer
 * @param  vertical: true for vertical, false for horizontal
 * @return none
 */
void sgl_battery_set_vertical(sgl_obj_t* obj, bool vertical);

#ifdef __cplusplus
}
#endif

#endif /* __SGL_BATTERY_H__ */
