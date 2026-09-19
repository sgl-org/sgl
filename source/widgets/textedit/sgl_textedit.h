/* source/widgets/textedit/sgl_textedit.h
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

#ifndef __SGL_TEXTEDIT_H__
#define __SGL_TEXTEDIT_H__

#include <sgl_core.h>
#include <sgl_draw.h>
#include <sgl_math.h>
#include <sgl_log.h>
#include <sgl_mm.h>
#include <sgl_cfgfix.h>
#include <sgl_anim.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* textedit mode */
#define SGL_TEXTEDIT_SINGLE_LINE    0
#define SGL_TEXTEDIT_MULTI_LINE     1

/* cursor blink period in ms */
#define SGL_TEXTEDIT_CURSOR_BLINK_MS  500

/* cursor line width */
#define SGL_TEXTEDIT_CURSOR_WIDTH     1

/* scroll bar width */
#define SGL_TEXTEDIT_SCROLL_WIDTH     4

/**
 * @brief sgl textedit struct
 * @obj: sgl general object
 * @bg: background draw descriptor
 * @text: text buffer pointer
 * @font: text font
 * @text_color: text color
 * @cursor_color: cursor color
 * @cursor_pos: cursor position in text buffer (byte index)
 * @cursor_x: cursor x coordinate (cached)
 * @cursor_y: cursor y coordinate (cached)
 * @cursor_h: cursor height (cached)
 * @cursor_visible: cursor visible flag (controlled by blink animation)
 * @cursor_anim: cursor blink animation object (dynamically created/deleted)
 * @y_offset: vertical scroll offset
 * @text_height: total text height in pixels
 * @text_max_len: text buffer max length (including null terminator)
 * @mode: single line or multi line mode
 * @line_margin: line margin for multi-line mode
 * @editable: whether text can be edited
 * @scroll_enable: whether scroll bar is visible
 */
typedef struct sgl_textedit {
    sgl_obj_t        obj;
    sgl_draw_rect_t  bg;
    char             *text;
    const sgl_font_t *font;
    sgl_color_t      text_color;
    sgl_color_t      cursor_color;
    int32_t          cursor_pos;
    int16_t          cursor_x;
    int16_t          cursor_y;
    int16_t          cursor_h;
    uint8_t          cursor_visible;
    sgl_anim_t       *cursor_anim;
    int32_t          y_offset;
    int32_t          text_height;
    int32_t          text_max_len;
    uint8_t          mode;
    uint8_t          line_margin;
    uint8_t          editable;
    uint8_t          scroll_enable;
} sgl_textedit_t;

/**
 * @brief create a textedit object
 * @param parent parent of the textedit
 * @return pointer to the textedit object
 */
sgl_obj_t* sgl_textedit_create(sgl_obj_t* parent);

/**
 * @brief set textedit text buffer
 * @param obj textedit object
 * @param buffer text buffer pointer
 * @param max_len buffer max length (including null terminator)
 * @return none
 */
void sgl_textedit_set_text_buffer(sgl_obj_t *obj, char *buffer, int32_t max_len);

/**
 * @brief set textedit text
 * @param obj textedit object
 * @param text text string
 * @return none
 */
void sgl_textedit_set_text(sgl_obj_t *obj, const char *text);

/**
 * @brief get textedit text
 * @param obj textedit object
 * @return text string pointer
 */
const char* sgl_textedit_get_text(sgl_obj_t *obj);

/**
 * @brief set textedit mode
 * @param obj textedit object
 * @param mode SGL_TEXTEDIT_SINGLE_LINE or SGL_TEXTEDIT_MULTI_LINE
 * @return none
 */
void sgl_textedit_set_mode(sgl_obj_t *obj, uint8_t mode);

/**
 * @brief set textedit text color
 * @param obj textedit object
 * @param color text color
 * @return none
 */
void sgl_textedit_set_text_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set textedit text font
 * @param obj textedit object
 * @param font text font
 * @return none
 */
void sgl_textedit_set_text_font(sgl_obj_t *obj, const sgl_font_t *font);

/**
 * @brief set textedit background color
 * @param obj textedit object
 * @param color background color
 * @return none
 */
void sgl_textedit_set_bg_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set textedit cursor color
 * @param obj textedit object
 * @param color cursor color
 * @return none
 */
void sgl_textedit_set_cursor_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set textedit border color
 * @param obj textedit object
 * @param color border color
 * @return none
 */
void sgl_textedit_set_border_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set textedit border width
 * @param obj textedit object
 * @param width border width
 * @return none
 */
void sgl_textedit_set_border_width(sgl_obj_t *obj, uint8_t width);

/**
 * @brief set textedit radius
 * @param obj textedit object
 * @param radius radius
 * @return none
 */
void sgl_textedit_set_radius(sgl_obj_t *obj, uint8_t radius);

/**
 * @brief set textedit line margin (for multi-line mode)
 * @param obj textedit object
 * @param margin line margin
 * @return none
 */
void sgl_textedit_set_line_margin(sgl_obj_t *obj, uint8_t margin);

/**
 * @brief insert a character at cursor position
 * @param obj textedit object
 * @param c character to insert
 * @return none
 */
void sgl_textedit_insert_char(sgl_obj_t *obj, char c);

/**
 * @brief delete character before cursor (backspace)
 * @param obj textedit object
 * @return none
 */
void sgl_textedit_backspace(sgl_obj_t *obj);

/**
 * @brief move cursor left
 * @param obj textedit object
 * @return none
 */
void sgl_textedit_cursor_left(sgl_obj_t *obj);

/**
 * @brief move cursor right
 * @param obj textedit object
 * @return none
 */
void sgl_textedit_cursor_right(sgl_obj_t *obj);

/**
 * @brief move cursor up (multi-line mode)
 * @param obj textedit object
 * @return none
 */
void sgl_textedit_cursor_up(sgl_obj_t *obj);

/**
 * @brief move cursor down (multi-line mode)
 * @param obj textedit object
 * @return none
 */
void sgl_textedit_cursor_down(sgl_obj_t *obj);

/**
 * @brief set cursor position
 * @param obj textedit object
 * @param pos cursor position in text buffer
 * @return none
 */
void sgl_textedit_set_cursor_pos(sgl_obj_t *obj, int32_t pos);

/**
 * @brief get cursor position
 * @param obj textedit object
 * @return cursor position in text buffer
 */
int32_t sgl_textedit_get_cursor_pos(sgl_obj_t *obj);

/**
 * @brief start cursor blink animation
 * @param obj textedit object
 * @return none
 * @note this function is called internally when textedit gets focus
 */
void sgl_textedit_cursor_blink_start(sgl_obj_t *obj);

/**
 * @brief stop cursor blink animation
 * @param obj textedit object
 * @return none
 * @note this function is called internally when textedit loses focus
 */
void sgl_textedit_cursor_blink_stop(sgl_obj_t *obj);

#ifdef __cplusplus
}
#endif

#endif // !__SGL_TEXTEDIT_H__
