/* source/widgets/arc_label/sgl_arc_label.h
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

#ifndef __SGL_ARC_LABEL_H__
#define __SGL_ARC_LABEL_H__

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
 * @brief sgl arc label object
 * 
 * @obj: sgl general object
 * @font: font to use for rendering
 * @text: text string to display
 * @text_capacity: max text length (for dynamic allocation)
 * @color: text color
 * @bg_color: background color
 * @transform.offset: text offset within the label
 * @transform.rotation: rotation angle (0-360 degrees)
 * @alpha: transparency (0-255)
 * @dynamic: flag indicating text is dynamically allocated
 * @align: text alignment
 * @bg_flag: flag to enable background drawing
 * @rota: flag indicating rotation is active
 * @angle: current rotation angle
 * @orig_x: original x position (before rotation)
 * @orig_y: original y position (before rotation)
 * @orig_w: original width (before rotation)
 * @orig_h: original height (before rotation)
 */
typedef struct sgl_arc_label {
    sgl_obj_t        obj;
    const sgl_font_t *font;
    char             *text;
    uint16_t         text_capacity;
    sgl_color_t      color;
    sgl_color_t      bg_color;
    union {
        struct {
            int8_t offset_x;
            int8_t offset_y;
        } offset;
        int16_t rotation;
    } transform;
    uint8_t          alpha;
    uint8_t          dynamic : 1;
    uint8_t          align: 5;
    uint8_t          bg_flag : 1;
    uint8_t          rota : 1;
    int16_t          angle;
    int16_t          orig_x;      /* Original x position (before rotation) */
    int16_t          orig_y;      /* Original y position (before rotation) */
    int16_t          orig_w;      /* Original width (before rotation) */
    int16_t          orig_h;      /* Original height (before rotation) */
} sgl_arc_label_t;


/**
 * @brief create an arc label object
 * @param parent parent of the arc label
 * @return pointer to the arc label object
 */
sgl_obj_t* sgl_arc_label_create(sgl_obj_t* parent);

/**
 * @brief set the text of the arc label
 * @param obj pointer to the arc label object
 * @param text pointer to the text
 * @return none
 */
void sgl_arc_label_set_text(sgl_obj_t *obj, const char *text);

/**
 * @brief set the text buffer of the arc label
 * @param obj pointer to the arc label object
 * @param buf pointer to the text buffer
 * @param buf_size size of the text buffer
 * @return none
 */
void sgl_arc_label_set_text_buffer(sgl_obj_t *obj, char *buf, uint16_t buf_size);

/**
 * @brief set the text of the arc label with format by manual memory
 * @param obj pointer to the arc label object
 * @param fmt pointer to the text
 * @return none
 * @note the text buffer must be set by sgl_arc_label_set_text_buffer() before calling this function
 */
void sgl_arc_label_set_text_fmt(sgl_obj_t *obj, const char *fmt, ...);

/**
 * @brief set the text of the arc label with format by dynamic memory
 * @param obj pointer to the arc label object
 * @param text pointer to the text
 * @return none
 */
void sgl_arc_label_set_text_fmt_dynamic(sgl_obj_t* obj, const char *fmt, ...);

/**
 * @brief update arc label text area
 * @param obj pointer to the arc label object
 * @return none
 * @note you can update your arc label text area when you change the text buffer content
 */
void sgl_arc_label_update_text(sgl_obj_t *obj);

/**
 * @brief get the text of the arc label
 * @param obj pointer to the arc label object
 * @return pointer to the text
 */
char* sgl_arc_label_get_text(sgl_obj_t *obj);

/**
 * @brief set arc label font
 * @param obj pointer to the arc label object
 * @param font pointer to the font
 * @return none
 */
void sgl_arc_label_set_font(sgl_obj_t *obj, const sgl_font_t *font);

/**
 * @brief set arc label text color
 * @param obj pointer to the arc label object
 * @param color color to be set
 * @return none
 */
void sgl_arc_label_set_text_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set arc label background color
 * @param obj pointer to the arc label object
 * @param color color to be set
 * @return none
 */
void sgl_arc_label_set_bg_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set arc label radius
 * @param obj pointer to the arc label object
 * @param radius radius to be set
 * @return none
 */
void sgl_arc_label_set_radius(sgl_obj_t *obj, uint8_t radius);

/**
 * @brief set arc label text align
 * @param obj pointer to the arc label object
 * @param align align to be set
 * @return none
 */
void sgl_arc_label_set_text_align(sgl_obj_t *obj, sgl_align_type_t align);

/**
 * @brief set arc label alpha
 * @param obj pointer to the arc label object
 * @param alpha alpha to be set
 * @return none
 */
void sgl_arc_label_set_alpha(sgl_obj_t *obj, uint8_t alpha);

/**
 * @brief set arc label text offset
 * @param obj pointer to the arc label object
 * @param offset_x offset_x to be set
 * @param offset_y offset_y to be set
 * @return none
 */
void sgl_arc_label_set_text_offset(sgl_obj_t *obj, int8_t offset_x, int8_t offset_y);

/**
 * @brief set arc label original position (before rotation)
 * @param obj pointer to the arc label object
 * @param x x coordinate
 * @param y y coordinate
 * @return none
 * @note This sets the position as if rotation angle is 0 degrees.
 *       When rotation is applied, the actual displayed position will be adjusted.
 */
void sgl_arc_label_set_orig_pos(sgl_obj_t *obj, int16_t x, int16_t y);

/**
 * @brief set arc label original size (before rotation)
 * @param obj pointer to the arc label object
 * @param w width
 * @param h height
 * @return none
 * @note This sets the size as if rotation angle is 0 degrees.
 *       When rotation is applied, the actual displayed size will be adjusted.
 */
void sgl_arc_label_set_orig_size(sgl_obj_t *obj, int16_t w, int16_t h);

/**
 * @brief set arc label text rotation angle
 * @param obj pointer to the arc label object
 * @param angle rotation angle (0-360 degrees)
 * @return none
 * 
 * @note The rotation angle affects how the text is displayed:
 *       - 0 degrees: Horizontal display (left-to-right)
 *       - 90 degrees: Vertical display (bottom-to-top, first character at bottom)
 *       - 180 degrees: Horizontal flipped (right-to-left)
 *       - 270 degrees: Vertical display (top-to-bottom, first character at top)
 *       
 *       After setting the angle, the widget's position and size are automatically
 *       recalculated to accommodate the rotated text.
 */
void sgl_arc_label_set_angle(sgl_obj_t *obj, int16_t angle);

/**
 * @brief get arc label current rotation angle
 * @param obj pointer to the arc label object
 * @return current rotation angle (0-360)
 */
int16_t sgl_arc_label_get_angle(sgl_obj_t *obj);

#ifdef __cplusplus
}
#endif

#endif // !__SGL_ARC_LABEL_H__
