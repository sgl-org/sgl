/* source/widgets/sgl_box.h
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

#ifndef __SGL_BOX_H__
#define __SGL_BOX_H__

#include <sgl_core.h>
#include <sgl_draw.h>
#include <sgl_math.h>
#include <sgl_log.h>
#include <sgl_mm.h>
#include <sgl_cfgfix.h>
#include <string.h>

// Define scroll modes
#define SGL_BOX_SCROLL_VERTICAL_ONLY      (0x01)  // Vertical scrolling only
#define SGL_BOX_SCROLL_HORIZONTAL_ONLY    (0x02)  // Horizontal scrolling only
#define SGL_BOX_SCROLL_BOTH               (0x03)  // Both vertical and horizontal scrolling enabled

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief sgl box struct
 * @desc: Container for scrolling child widgets
 */
typedef struct sgl_box {
    sgl_obj_t       obj;
    sgl_draw_rect_t bg;
    sgl_color_t     scroll_color;         // scroll color;
    int16_t         x_offset;             // X-axis offset
    int16_t         y_offset;             // Y-axis offset
    int16_t         elastic_scroll_up;    // Maximum allowable upward elastic scroll offset
    int16_t         elastic_scroll_down;  // Maximum allowable downward elastic scroll offset
    int16_t         elastic_scroll_left;  // Maximum allowable leftward elastic scroll offset
    int16_t         elastic_scroll_right; // Maximum allowable rightward elastic scroll offset
    uint8_t         scroll_mode;          // Scroll mode (vertical/horizontal/both)
    uint8_t         scroll_enable : 1;    // Whether scrolling is enabled
    uint8_t         show_v_scrollbar : 1; // Whether to show the vertical scrollbar
    uint8_t         show_h_scrollbar : 1; // Whether to show the horizontal scrollbar
} sgl_box_t;

/**
 * @brief create a box object
 * @param parent parent of the box
 * @return box object
 */
sgl_obj_t* sgl_box_create(sgl_obj_t* parent);

/**
 * @brief set background color of the box
 * @param obj box object
 * @param color background color to be set
 * @return none
 */
void sgl_box_set_bg_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set radius of the box
 * @param obj box object
 * @param radius radius to be set
 * @return none
 */
void sgl_box_set_radius(sgl_obj_t *obj, uint8_t radius);

/**
 * @brief set border color of the box
 * @param obj box object
 * @param color border color to be set
 * @return none
 */
void sgl_box_set_border_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set border width of the box
 * @param obj box object
 * @param width border width to be set
 * @return none
 */
void sgl_box_set_border_width(sgl_obj_t *obj, uint8_t width);

/**
 * @brief set box pixmap
 * @param obj box object
 * @param pixmap pixmap to be set
 * @return none
 */
void sgl_box_set_pixmap(sgl_obj_t *obj, const sgl_pixmap_t *pixmap);

/**
 * @brief Set the elastic scroll limits for up and down directions
 * @param obj box object
 * @param up_limit maximum pixels allowed when scrolling up (0 for unlimited)
 * @param down_limit maximum pixels allowed when scrolling down (0 for unlimited)
 * @param left_limit maximum pixels allowed when scrolling left (0 for unlimited)
 * @param right_limit maximum pixels allowed when scrolling right (0 for unlimited)
 * @return none
 */
void sgl_box_set_elastic_scroll(sgl_obj_t* obj, int16_t up_limit, int16_t down_limit, int16_t left_limit, int16_t right_limit);

/**
 * @brief set scrollbar color of the box
 * @param obj box object
 * @param color scrollbar color to be set
 * @return none
 */
void sgl_box_set_scrollbar_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set whether to show scrollbars
 * @param obj box object
 * @param show_vertical whether to show vertical scrollbar (1 to show, 0 to hide)
 * @param show_horizontal whether to show horizontal scrollbar (1 to show, 0 to hide)
 * @return none
 */
void sgl_box_set_show_scrollbar(sgl_obj_t *obj, uint8_t show_vertical, uint8_t show_horizontal);

/**
 * @brief set alpha (transparency) of the box
 * @param obj box object
 * @param alpha alpha value to be set (0-255, where 0 is fully transparent and 255 is fully opaque)
 * @return none
 */
void sgl_box_set_alpha(sgl_obj_t *obj, uint8_t alpha);

#ifdef __cplusplus
}
#endif

#endif // !__SGL_BOX_H__
