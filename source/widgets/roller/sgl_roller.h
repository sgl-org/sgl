/* source/widgets/sgl_roller.h
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

#ifndef __SGL_ROLLER_H__
#define __SGL_ROLLER_H__

#include <sgl_core.h>
#include <sgl_draw.h>
#include <sgl_math.h>
#include <sgl_log.h>
#include <sgl_mm.h>
#include <sgl_cfgfix.h>
#include <sgl_misc.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief sgl roller struct
 * @desc: vertical scroll selector with \n-separated options
 */
typedef struct sgl_roller {
    sgl_obj_t       obj;
    const sgl_font_t *font;
    char           *opt_text;          /**< \n-separated option string */
    uint16_t        text_offset;       /**< byte offset of selected item */
    sgl_color_t     bg_color;
    sgl_color_t     text_color;
    sgl_color_t     border_color;
    sgl_color_t     selected_color;
    int16_t         item_selected;     /**< index of selected item */
    uint16_t        item_num;          /**< total number of options */
    sgl_scroll_t    sc;                /**< shared scroll physics state (sgl_misc) */
    uint8_t         visible_rows;      /**< number of visible rows (odd recommended) */
    uint8_t         alpha;
    uint8_t         dynamic_text;      /**< 1 = opt_text was malloc'd */
    uint8_t         infinite;          /**< 1 = infinite roller, 0 = bounded roller */
} sgl_roller_t;

/**
 * @brief create a roller object
 * @param parent parent of the roller
 * @return roller object
 */
sgl_obj_t* sgl_roller_create(sgl_obj_t* parent);

/**
 * @brief set roller options with static text (no copy)
 * @param obj roller object
 * @param text static text
 * @return none
 */
void sgl_roller_set_option_static(sgl_obj_t *obj, const char *text);

/**
 * @brief set roller options with dynamic text (makes a copy)
 * @param obj roller object
 * @param text newline-separated option text
 * @return none
 */
void sgl_roller_set_option_dynamic(sgl_obj_t *obj, const char *text);

/**
 * @brief get roller selected index
 * @param obj roller object
 * @return selected index
 */
int sgl_roller_get_selected_index(sgl_obj_t *obj);

/**
 * @brief get roller selected text
 * @param obj roller object
 * @param buf buffer to store text
 * @param buf_size size of buffer
 * @return true if successful
 */
bool sgl_roller_get_selected_text(sgl_obj_t *obj, char *buf, int buf_size);

/**
 * @brief set infinite mode
 * @param obj roller object
 * @param enable true to enable infinite mode, false to disable
 * @return none
 */
void sgl_roller_set_infinite_mode(sgl_obj_t *obj, bool enable);

/**
 * @brief set roller visible rows
 * @param obj roller object
 * @param rows number of visible rows (should be odd for centered selection)
 */
void sgl_roller_set_visible_rows(sgl_obj_t *obj, uint8_t rows);

/**
 * @brief set roller text color
 * @param obj roller object
 * @param color text color
 */
void sgl_roller_set_text_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set roller selected item color
 * @param obj roller object
 * @param color selected color
 */
void sgl_roller_set_selected_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set roller background color
 * @param obj roller object
 * @param color background color
 */
void sgl_roller_set_bg_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set roller border color
 * @param obj roller object
 * @param color border color
 * @return none
 */
void sgl_roller_set_border_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set roller text font
 * @param obj roller object
 * @param font font to use
 */
void sgl_roller_set_text_font(sgl_obj_t *obj, const sgl_font_t *font);

/**
 * @brief set roller alpha
 * @param obj roller object
 * @param alpha alpha value
 * @return none
 */
void sgl_roller_set_alpha(sgl_obj_t *obj, uint8_t alpha);

/**
 * @brief set roller border radius
 * @param obj roller object
 * @param radius border radius
 * @return none
 */
void sgl_roller_set_radius(sgl_obj_t *obj, int16_t radius);

/**
 * @brief set roller border width
 * @param obj roller object
 * @param width border width
 * @return none
 */
void sgl_roller_set_border_width(sgl_obj_t *obj, uint8_t width);

#ifdef __cplusplus
}
#endif

#endif // !__SGL_ROLLER_H__
