/* source/widgets/sgl_dropdown.h
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

#ifndef __SGL_DROPDOWN_H__
#define __SGL_DROPDOWN_H__

#include <sgl_core.h>
#include <sgl_draw.h>
#include <sgl_math.h>
#include <sgl_log.h>
#include <sgl_mm.h>
#include <sgl_cfgfix.h>
#include <sgl_misc.h>
#include <sgl_anim.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief sgl dropdown struct
 * @desc: text description
 */
typedef struct sgl_dropdown {
    sgl_obj_t             obj;
    const sgl_font_t      *font;
    char                  *opt_text;
    uint16_t              text_offset;
    sgl_color_t           text_color;
    int16_t               item_selected;
    sgl_color_t           item_selected_color;
    uint16_t              item_num;
    sgl_color_t           bg_color;
    sgl_color_t           border_color;
    uint8_t               option_h;
    uint8_t               alpha;
    uint8_t               is_open : 1;
    uint8_t               dynamic_text : 7;
    uint8_t               max_visible_item;
    sgl_scroll_t          sc;
} sgl_dropdown_t;

/**
 * @brief create a dropdown object
 * @param parent parent of the dropdown
 * @return dropdown object
 */
sgl_obj_t* sgl_dropdown_create(sgl_obj_t* parent);

/**
 * @brief set dropdown object's max visible rows
 * @param obj dropdown object
 * @param rows max visible rows
 */
void sgl_dropdown_set_visible_rows(sgl_obj_t *obj, int16_t rows);

/**
 * @brief set dropdown object's background color
 * @param obj dropdown object
 * @param color background color
 */
void sgl_dropdown_set_bg_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set dropdown object's border width
 * @param obj dropdown object
 * @param width border width
 */
void sgl_dropdown_set_border_width(sgl_obj_t *obj, uint8_t width);

/**
 * @brief set dropdown object's border color
 * @param obj dropdown object
 * @param color border color
 */
void sgl_dropdown_set_border_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set the selected color of dropdown
 * @param obj dropdown object
 * @param color selected option color
 */
void sgl_dropdown_set_selected_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set dropdown object's radius
 * @param obj dropdown object
 * @param radius radius
 */
void sgl_dropdown_set_radius(sgl_obj_t *obj, uint8_t radius);

/**
 * @brief set dropdown object's alpha
 * @param obj dropdown object
 * @param alpha alpha
 */
void sgl_dropdown_set_alpha(sgl_obj_t *obj, uint8_t alpha);

/**
 * @brief set dropdown object's text color
 * @param obj dropdown object
 * @param color text color
 */
void sgl_dropdown_set_text_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set dropdown object's text font
 * @param obj dropdown object
 * @param font text font
 * @return none
 * @note font must be initialized
 */
void sgl_dropdown_set_text_font(sgl_obj_t *obj, const sgl_font_t* font);

/**
 * @brief get dropdown object's selected index
 * @param obj dropdown object
 * @return selected index
 */
int sgl_dropdown_get_selected_index(sgl_obj_t *obj);

/**
 * @brief get dropdown object's selected text
 * @param obj dropdown object
 * @param buf buffer to store the selected text
 * @param buf_size size of the buffer
 * @return true if successful, false otherwise
 */
bool sgl_dropdown_get_selected_text(sgl_obj_t *obj, char *buf, int buf_size);

/**
 * @brief set options with static text (no copy, caller ensures text lifetime)
 * @param obj dropdown object
 * @param text \n-separated option text
 */
void sgl_dropdown_set_option_static(sgl_obj_t *obj, const char *text);

/**
 * @brief set options with dynamic text (makes a copy)
 * @param obj dropdown object
 * @param text \n-separated option text
 */
void sgl_dropdown_set_option_dynamic(sgl_obj_t *obj, const char *text);

/**
 * @brief add an option to the dropdown
 * @param obj pointer to the dropdown object
 * @param text pointer to the text
 * @return none
 */
void sgl_dropdown_add_option(sgl_obj_t *obj, const char *text);

/**
 * @brief delete an option from the dropdown
 * @param obj pointer to the dropdown object
 * @param text pointer to the text
 * @return none
 */
void sgl_dropdown_delete_option_by_text(sgl_obj_t *obj, const char *text);

/**
 * @brief delete an option from the dropdown
 * @param obj pointer to the dropdown object
 * @param index index of the option
 * @return none
 */
void sgl_dropdown_delete_option_by_index(sgl_obj_t *obj, int index);

#ifdef __cplusplus
}
#endif

#endif // !__SGL_DROPDOWN_H__
