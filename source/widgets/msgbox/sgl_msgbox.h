/* source/widgets/sgl_msgbox.h
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

#ifndef __SGL_MSGBOX_H__
#define __SGL_MSGBOX_H__

#include <sgl_core.h>
#include <sgl_draw.h>
#include <sgl_math.h>
#include <sgl_log.h>
#include <sgl_mm.h>
#include <sgl_cfgfix.h>
#include <string.h>


#define  SGL_MSGBOX_STATUS_NORMAL               (1 << 7)
#define  SGL_MSGBOX_STATUS_LEFT                 (1 << 0)
#define  SGL_MSGBOX_STATUS_RIGHT                (1 << 1)
#define  SGL_MSGBOX_STATUS_EXIT                 (1 << 2)

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief sgl message box struct
 * @obj: sgl general object
 * @body_desc: pointer to sgl_draw_rect_t descriptor
 */
typedef struct sgl_msgbox {
    sgl_obj_t        obj;

    uint8_t          status;
    uint8_t          alpha;
    uint8_t          border_alpha;
    sgl_color_t      color;
    sgl_color_t      border_color;
    const sgl_pixmap_t *pixmap;

    const sgl_font_t *font;
    const char       *title_text;
    const char       *msg_text;
    const char       *lbtn_text;
    const char       *rbtn_text;
    const char       **exit_btn;
    sgl_color_t      title_color;
    sgl_color_t      msg_color;
    sgl_color_t      lbtn_text_color;
    sgl_color_t      rbtn_text_color;
    sgl_color_t      lbtn_color;
    sgl_color_t      rbtn_color;
    uint8_t          msg_line_margin;
    uint8_t          title_height;
    uint8_t          text_y_offset;
    uint8_t          text_x_offset;
}sgl_msgbox_t;


/**
 * @brief create a message box object
 * @param parent parent of the message box
 * @return message box object
 */
sgl_obj_t* sgl_msgbox_create(sgl_obj_t* parent);

/**
 * @brief set message box color
 * @param obj message box object
 * @param color message box color
 * @return none
 */
void sgl_msgbox_set_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set message box alpha
 * @param obj message box object
 * @param alpha message box alpha
 * @return none
 */
void sgl_msgbox_set_alpha(sgl_obj_t *obj, uint8_t alpha);

/**
 * @brief set message box main body alpha
 * @param obj message box object
 * @param alpha message box main body alpha
 * @return none
 */
void sgl_msgbox_set_main_alpha(sgl_obj_t *obj, uint8_t alpha);

/**
 * @brief set message box border alpha
 * @param obj message box object
 * @param alpha message box border alpha
 * @return none
 */
void sgl_msgbox_set_border_alpha(sgl_obj_t *obj, uint8_t alpha);

/**
 * @brief set message box radius
 * @param obj message box object
 * @param radius message box radius
 * @return none
 */
void sgl_msgbox_set_radius(sgl_obj_t *obj, uint8_t radius);

/**
 * @brief set message box border width
 * @param obj message box object
 * @param width message box border width
 * @return none
 */
void sgl_msgbox_set_border_width(sgl_obj_t *obj, uint8_t width);

/**
 * @brief set message box border color
 * @param obj message box object
 * @param color message box border color
 * @return none
 */
void sgl_msgbox_set_border_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set message box pixmap
 * @param obj message box object
 * @param pixmap message box pixmap
 * @return none
 */
void sgl_msgbox_set_pixmap(sgl_obj_t *obj, const sgl_pixmap_t *pixmap);

/**
 * @brief set message box all text font
 * @param obj message box object
 * @param font message box font
 * @return none
 */
void sgl_msgbox_set_font(sgl_obj_t *obj, const sgl_font_t *font);

/**
 * @brief set message box title text
 * @param obj message box object
 * @param text message box title text
 * @return none
 */
void sgl_msgbox_set_title_text(sgl_obj_t *obj, const char *text);

/**
 * @brief set message box title text color
 * @param obj message box object
 * @param color message box title text color
 * @return none
 */
void sgl_msgbox_set_title_text_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set message box message text
 * @param obj message box object
 * @param text message box message text
 * @return none
 */
void sgl_msgbox_set_msg_text(sgl_obj_t *obj, const char *text);

/**
 * @brief set message box message text color
 * @param obj message box object
 * @param color message box message text color
 * @return none
 */
void sgl_msgbox_set_msg_text_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set message box message text line margin
 * @param obj message box object
 * @param margin message box message text line margin
 * @return none
 */
void sgl_msgbox_set_msg_line_margin(sgl_obj_t *obj, uint8_t margin);

/**
 * @brief set message box left button text
 * @param obj message box object
 * @param text message box left button text
 * @return none
 */
void sgl_msgbox_set_left_btn_text(sgl_obj_t *obj, const char *text);

/**
 * @brief set message box left button text color
 * @param obj message box object
 * @param color message box left button text color
 * @return none
 */
void sgl_msgbox_set_left_btn_text_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set message box left button color
 * @param obj message box object
 * @param color message box left button color
 * @return none
 */
void sgl_msgbox_set_left_btn_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set message box right button text
 * @param obj message box object
 * @param text message box right button text
 * @return none
 */
void sgl_msgbox_set_right_btn_text(sgl_obj_t *obj, const char *text);

/**
 * @brief set message box right button text color
 * @param obj message box object
 * @param color message box right button text color
 * @return none
 */
void sgl_msgbox_set_right_btn_text_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set message box right button color
 * @param obj message box object
 * @param color message box right button color
 * @return none
 */
void sgl_msgbox_set_right_btn_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set message box exit answer
 * @param obj message box object
 * @param answer [out] pointer to exit answer
 * @return none
 * @note the answer will point to the answer btn string when the message box is destroyed
 */
void sgl_msgbox_set_exit_answer(sgl_obj_t *obj, const char **answer);

/**
 * @brief get message box current button text
 * @param obj message box object
 * @return current current button text
 */
const char* sgl_msgbox_get_current_btn(sgl_obj_t *obj);

/**
 * @brief set message box title height
 * @param obj message box object
 * @param height title bar height in pixels
 * @return none
 */
void sgl_msgbox_set_title_height(sgl_obj_t *obj, uint8_t height);

/**
 * @brief set message box message text x offset
 * @param obj message box object
 * @param offset x axis offset for message text
 * @return none
 */
void sgl_msgbox_set_msg_x_offset(sgl_obj_t *obj, uint8_t offset);

/**
 * @brief set message box message text y offset
 * @param obj message box object
 * @param offset y axis offset for message text
 * @return none
 */
void sgl_msgbox_set_msg_y_offset(sgl_obj_t *obj, uint8_t offset);

#ifdef __cplusplus
}
#endif

#endif // !__SGL_MSGBOX_H__
