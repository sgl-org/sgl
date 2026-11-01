/* source/widgets/winslate/sgl_winslate.h
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

#ifndef __SGL_WINSLATE_H__
#define __SGL_WINSLATE_H__

#include <sgl_core.h>
#include <sgl_draw.h>
#include <sgl_math.h>
#include <sgl_log.h>
#include <sgl_mm.h>
#include <sgl_cfgfix.h>

#ifdef __cplusplus
extern "C" {
#endif

#define  SGL_WINSLATE_BODY_COLOR        sgl_rgb(0xF2, 0xF2, 0xF2)
#define  SGL_WINSLATE_BORDER_COLOR      sgl_rgb(0xCC, 0xCC, 0xCC)
#define  SGL_WINSLATE_TITLE_TEXT_COLOR  sgl_rgb(0x1A, 0x1A, 0x1A)

/* caption button ids, used for the pressed / hover feedback */
#define  SGL_WINSLATE_BTN_NONE          (0)
#define  SGL_WINSLATE_BTN_MAXIMIZE      (1)
#define  SGL_WINSLATE_BTN_CLOSE         (2)

/**
 * @brief sgl Slate (flat "Modern UI") window widget
 * @obj: sgl general object
 * @chrome: window chrome description (caption / body / border colours, title height, border width, alpha)
 * @title_text: caption bar text
 * @title_font: caption bar font
 * @title_text_color: caption text colour
 * @btn_w: caption button width
 * @btn_h: caption button height
 * @btn_gap: horizontal gap between the two caption buttons
 * @maximized: 1 when the window currently fills its parent
 * @pressed_btn: id of the caption button being pressed (SGL_WINSLATE_BTN_*)
 * @dragging: 1 while the caption bar is grabbed and the window is being moved
 * @grab_dx: window_left - press_x, kept constant during a drag
 * @grab_dy: window_top - press_y, kept constant during a drag
 * @res_x: saved relative x, restored when un-maximizing
 * @res_y: saved relative y, restored when un-maximizing
 * @res_w: saved width, restored when un-maximizing
 * @res_h: saved height, restored when un-maximizing
 * @note the window exposes three caption buttons at the right of the title
 *       bar: minimize (display only, never reacts to events), maximize /
 *       restore and close (always drawn on its red background).  Clicking
 *       close destroys the window; clicking maximize toggles between the
 *       saved geometry and a full parent-sized window (the glyph switches
 *       between a square and the restore double-square accordingly).
 *       Pressing an empty spot of the caption bar grabs the window: while
 *       dragged it follows the pointer freely (disabled when maximized).
 */
typedef struct sgl_winslate {
    sgl_obj_t               obj;
    sgl_draw_slate_window_t chrome;
    const char             *title_text;
    const sgl_font_t       *title_font;
    sgl_color_t             title_text_color;
    int16_t                 btn_w;
    int16_t                 btn_h;
    int16_t                 btn_gap;
    uint8_t                 maximized;
    uint8_t                 pressed_btn;
    uint8_t                 dragging;
    int16_t                 grab_dx;
    int16_t                 grab_dy;
    int16_t                 res_x;
    int16_t                 res_y;
    int16_t                 res_w;
    int16_t                 res_h;
} sgl_winslate_t;

/**
 * @brief create a Slate window object
 * @param parent parent of the window, NULL creates it on the active screen
 * @return window object
 */
sgl_obj_t* sgl_winslate_create(sgl_obj_t* parent);

/**
 * @brief set the window title text
 * @param obj window object
 * @param text title text
 * @return none
 */
void sgl_winslate_set_title_text(sgl_obj_t *obj, const char *text);

/**
 * @brief set the window title font
 * @param obj window object
 * @param font title font
 * @return none
 */
void sgl_winslate_set_title_font(sgl_obj_t *obj, const sgl_font_t *font);

/**
 * @brief set the window title text color
 * @param obj window object
 * @param color text color
 * @return none
 */
void sgl_winslate_set_title_text_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set the window caption bar color
 * @param obj window object
 * @param color caption bar color
 * @return none
 */
void sgl_winslate_set_title_bg_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set the window client body color
 * @param obj window object
 * @param color body color
 * @return none
 */
void sgl_winslate_set_body_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set the window outline color
 * @param obj window object
 * @param color outline color
 * @return none
 */
void sgl_winslate_set_border_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set the window caption bar height
 * @param obj window object
 * @param height caption bar height
 * @return none
 */
void sgl_winslate_set_title_height(sgl_obj_t *obj, int16_t height);

/**
 * @brief set the window alpha
 * @param obj window object
 * @param alpha alpha
 * @return none
 */
void sgl_winslate_set_alpha(sgl_obj_t *obj, uint8_t alpha);

/**
 * @brief get the window caption bar height
 * @param obj window object
 * @return caption bar height
 */
int16_t sgl_winslate_title_height(sgl_obj_t *obj);

/**
 * @brief query whether the window is currently maximized
 * @param obj window object
 * @return 1 when maximized, 0 otherwise
 */
uint8_t sgl_winslate_is_maximized(sgl_obj_t *obj);

#ifdef __cplusplus
}
#endif

#endif // !__SGL_WINSLATE_H__
