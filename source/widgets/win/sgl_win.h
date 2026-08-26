/* source/widgets/sgl_win.h
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

#ifndef __SGL_WIN_H__
#define __SGL_WIN_H__

#include <sgl_core.h>
#include <sgl_draw.h>
#include <sgl_math.h>
#include <sgl_log.h>
#include <sgl_mm.h>
#include <sgl_cfgfix.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief sgl window struct
 * @obj: sgl general object
 */
typedef struct sgl_win {
    sgl_obj_t       obj;
    sgl_draw_rect_t bg;
    const char     *title_text;
    const sgl_font_t *title_font;
    sgl_color_t     title_bg_color;
    sgl_color_t     title_text_color;
    sgl_color_t     close_color;
    uint8_t         title_h;
    uint8_t         title_align;
} sgl_win_t;

/**
 * @brief create a window object
 * @param parent parent of the window
 * @return window object
 */
sgl_obj_t* sgl_win_create(sgl_obj_t* parent);

/**
 * @brief set window color
 * @param obj window object
 * @param color window color
 * @return none
 */
void sgl_win_set_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set window radius
 * @param obj window object
 * @param radius radius
 * @return none
 */
void sgl_win_set_radius(sgl_obj_t *obj, uint8_t radius);

/**
 * @brief set window alpha
 * @param obj window object
 * @param alpha alpha
 * @return none
 */
void sgl_win_set_alpha(sgl_obj_t *obj, uint8_t alpha);

/**
 * @brief set window border width
 * @param obj window object
 * @param width border width
 * @return none
 */
void sgl_win_set_border_width(sgl_obj_t *obj, uint8_t width);

/**
 * @brief set window border color
 * @param obj window object
 * @param color border color
 * @return none
 */
void sgl_win_set_border_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set window pixmap
 * @param obj window object
 * @param pixmap pixmap
 * @return none
 */
void sgl_win_set_pixmap(sgl_obj_t *obj, const sgl_pixmap_t *pixmap);

/**
 * @brief set window title text
 * @param obj window object
 * @param text title text
 * @return none
 */
void sgl_win_set_title_text(sgl_obj_t *obj, const char *text);

/**
 * @brief set window title text color
 * @param obj window object
 * @param color text color
 * @return none
 */
void sgl_win_set_title_text_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set window title font
 * @param obj window object
 * @param font title font
 * @return none
 */
void sgl_win_set_title_font(sgl_obj_t *obj, const sgl_font_t *font);

/**
 * @brief set window title height
 * @param obj window object
 * @param height title height
 * @return none
 */
void sgl_win_set_title_height(sgl_obj_t *obj, uint16_t height);

/**
 * @brief set window title text align
 * @param obj window object
 * @param align text align
 * @return none
 */
void sgl_win_set_title_text_align(sgl_obj_t *obj, uint8_t align);

/**
 * @brief set window title background color
 * @param obj window object
 * @param color title background color
 * @return none
 */
void sgl_win_set_title_bg_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set window close button color
 * @param obj window object
 * @param color close button color
 * @return none
 */
void sgl_win_set_close_btn_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief get window title height
 * @param obj window object
 * @return title height
 */
int16_t sgl_win_title_height(sgl_obj_t *obj);

#ifdef __cplusplus
}
#endif

#endif // !__SGL_WIN_H__
