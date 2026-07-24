/* source/widgets/sgl_battery.h
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF KIND, EXPRESS OR
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


/**
 * @brief battery widget structure
 * @obj: sgl general object
 * @level: battery level (0-100)
 * @border_color: border color
 * @fill_color: fill color
 * @low_color: low battery color (below 20%)
 * @medium_color: medium battery color (20-50%)
 * @high_color: high battery color (above 50%)
 * @charging: charging status
 * @charging_color: charging indicator color
 * @show_percentage: show percentage text
 * @font: font for percentage text
 * @text_color: text color
 */
typedef enum {
    SGL_BATTERY_DIR_HORIZONTAL,
    SGL_BATTERY_DIR_VERTICAL
} sgl_battery_dir_t;

typedef enum {
    SGL_BATTERY_CAP_RIGHT,
    SGL_BATTERY_CAP_LEFT,
    SGL_BATTERY_CAP_TOP
} sgl_battery_cap_pos_t;

typedef struct sgl_battery {
    sgl_obj_t              obj;
    const sgl_font_t       *font;
    sgl_battery_dir_t      direction;
    sgl_battery_cap_pos_t  cap_pos;
    sgl_color_t            border_color;
    sgl_color_t            fill_color;
    sgl_color_t            low_color;
    sgl_color_t            medium_color;
    sgl_color_t            high_color;
    sgl_color_t            bg_color;
    sgl_color_t            charging_color;
    sgl_color_t            text_color;
    uint8_t                level;
    uint8_t                num_cells;
    uint8_t                cap_size;
    uint8_t                charging        : 1;
    uint8_t                show_percentage : 1;
    uint8_t                reserved        : 6;
} sgl_battery_t;


/**
 * @brief create a battery object
 * @param parent parent of object
 * @return pointer to the battery object
 */
sgl_obj_t* sgl_battery_create(sgl_obj_t* parent);

/**
 * @brief set battery level
 * @param obj pointer to the battery object
 * @param level battery level (0-100)
 * @return none
 */
void sgl_battery_set_level(sgl_obj_t* obj, uint8_t level);

/**
 * @brief set border color
 * @param obj pointer to the battery object
 * @param color border color
 * @return none
 */
void sgl_battery_set_border_color(sgl_obj_t* obj, sgl_color_t color);

/**
 * @brief set fill color
 * @param obj pointer to the battery object
 * @param color fill color
 * @return none
 */
void sgl_battery_set_fill_color(sgl_obj_t* obj, sgl_color_t color);

/**
 * @brief set low battery color (below 20%)
 * @param obj pointer to the battery object
 * @param color low battery color
 * @return none
 */
void sgl_battery_set_low_color(sgl_obj_t* obj, sgl_color_t color);

/**
 * @brief set medium battery color (20-50%)
 * @param obj pointer to the battery object
 * @param color medium battery color
 * @return none
 */
void sgl_battery_set_medium_color(sgl_obj_t* obj, sgl_color_t color);

/**
 * @brief set high battery color (above 50%)
 * @param obj pointer to the battery object
 * @param color high battery color
 * @return none
 */
void sgl_battery_set_high_color(sgl_obj_t* obj, sgl_color_t color);

/**
 * @brief set background color
 * @param obj pointer to the battery object
 * @param color background color
 * @return none
 */
void sgl_battery_set_bg_color(sgl_obj_t* obj, sgl_color_t color);

/**
 * @brief set number of cells
 * @param obj pointer to the battery object
 * @param num number of cells (1-10)
 * @return none
 */
void sgl_battery_set_num_cells(sgl_obj_t* obj, uint8_t num);

/**
 * @brief set battery direction
 * @param obj pointer to the battery object
 * @param dir direction (horizontal or vertical)
 * @return none
 */
void sgl_battery_set_direction(sgl_obj_t* obj, sgl_battery_dir_t dir);

/**
 * @brief set battery cap size (in pixels)
 * @param obj pointer to the battery object
 * @param size cap size in pixels (3-20, default is 8)
 * @return none
 */
void sgl_battery_set_cap_size(sgl_obj_t* obj, uint8_t size);

/**
 * @brief set battery cap position
 * @param obj pointer to the battery object
 * @param pos cap position (right, left, top, bottom)
 * @return none
 */
void sgl_battery_set_cap_pos(sgl_obj_t* obj, sgl_battery_cap_pos_t pos);

/**
 * @brief set charging status
 * @param obj pointer to the battery object
 * @param charging charging status (1 = charging, 0 = not charging)
 * @return none
 */
void sgl_battery_set_charging(sgl_obj_t* obj, bool charging);

/**
 * @brief set charging indicator color
 * @param obj pointer to the battery object
 * @param color charging indicator color
 * @return none
 */
void sgl_battery_set_charging_color(sgl_obj_t* obj, sgl_color_t color);

/**
 * @brief show/hide percentage text
 * @param obj pointer to the battery object
 * @param show show percentage text (1 = show, 0 = hide)
 * @return none
 */
void sgl_battery_show_percentage(sgl_obj_t* obj, bool show);

/**
 * @brief set font for percentage text
 * @param obj pointer to the battery object
 * @param font font for percentage text
 * @return none
 */
void sgl_battery_set_font(sgl_obj_t* obj, const sgl_font_t *font);

/**
 * @brief set text color
 * @param obj pointer to the battery object
 * @param color text color
 * @return none
 */
void sgl_battery_set_text_color(sgl_obj_t* obj, sgl_color_t color);

#endif // !__SGL_BATTERY_H__
