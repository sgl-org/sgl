/* source/widgets/sgl_msgbox.c
 *
 * MIT License
 *
 * Copyright(c) 2023-present All contributors of SGL  
 * Document reference: docs directory
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

#include "sgl_msgbox.h"
#include <sgl_theme.h>

static void msgbox_draw_text(sgl_surf_t *surf, sgl_area_t *area, sgl_area_t *coords, const char *text, const sgl_font_t *font, sgl_color_t color, uint8_t alpha, uint8_t y_offset)
{
    sgl_pos_t align_pos;
    align_pos = sgl_get_text_pos(coords, font, text, 0, SGL_ALIGN_CENTER);
    sgl_draw_string(surf, area, align_pos.x, align_pos.y + y_offset, text, color, alpha, font);
}

static void sgl_msgbox_construct_cb(sgl_surf_t *surf, sgl_obj_t* obj, sgl_event_t *evt)
{
    sgl_msgbox_t *msgbox = sgl_container_of(obj, sgl_msgbox_t, obj);
    const sgl_font_t *font = msgbox->font;
    int32_t font_height = sgl_font_get_height(font) + 8;
    int16_t border = obj->border;
    uint8_t title_height = msgbox->title_height ? msgbox->title_height : font_height; // title height
    uint8_t msg_y_offset = msgbox->text_y_offset;  // message text y coords offset

    sgl_draw_rect_t body_desc = {
        .alpha = msgbox->alpha,
        .border = border,
        .border_alpha = msgbox->border_alpha,
        .border_color = msgbox->border_color,
        .border_mask = obj->focus,
        .color = msgbox->color,
        .radius = obj->radius,
        .pixmap = msgbox->pixmap,
    };

    SGL_ASSERT(msgbox->font != NULL);

    sgl_color_t tmp_color;
    sgl_area_t  button_coords = {
        .x1 = obj->coords.x1,
        .x2 = obj->coords.x2,
        .y1 = obj->coords.y2 - font_height,
        .y2 = obj->coords.y2
    };

    sgl_area_t left_coords = {
        .x1 =  obj->coords.x1 + border,
        .x2 = (obj->coords.x1 + obj->coords.x2) / 2 - border / 2 - 1,
        .y1 = obj->coords.y2 - 2 * font_height,
        .y2 = obj->coords.y2 - border,
    };

    sgl_area_t right_coords = {
        .x1 = (obj->coords.x1 + obj->coords.x2) / 2 + border / 2 + 1,
        .x2 = obj->coords.x2 - border,
        .y1 = obj->coords.y2 - 2 * font_height,
        .y2 = obj->coords.y2 - border,
    };

    sgl_area_t title_coords = {
        .x1 = obj->coords.x1 + border + 2,
        .x2 = obj->coords.x2 - border + 2,
        .y1 = obj->coords.y1 + 1,
        .y2 = obj->coords.y1 + title_height + border,
    };

    sgl_area_t text_coords = {
        .x1 = obj->coords.x1 + border + 2 + msgbox->text_x_offset,
        .x2 = obj->coords.x2 - border - 2,
        .y1 = obj->coords.y1 + title_height + border + msg_y_offset,
        .y2 = obj->coords.y2 - (font_height + border),
    };

    if(evt->type == SGL_EVENT_DRAW_MAIN) {
        if(msgbox->status & SGL_MSGBOX_STATUS_EXIT) {
            sgl_obj_set_destroyed(&msgbox->obj);
        }

        sgl_draw_rect(surf, &obj->area, &obj->coords, &body_desc);
   
        msgbox_draw_text(surf, &obj->area, &title_coords, msgbox->title_text, font, msgbox->title_color, msgbox->alpha, 0);

        sgl_draw_fill_hline(surf, &obj->area,
                            obj->coords.y1 + title_height + 4,
                            obj->coords.x1 + border,
                            obj->coords.x2 - obj->border,
                            border,
                            msgbox->border_color,
                            msgbox->alpha
                           );

        sgl_draw_string_mult_line(surf, &text_coords, text_coords.x1, text_coords.y1 + 2, msgbox->msg_text, msgbox->msg_color, msgbox->alpha, font, msgbox->msg_line_margin);

        if(msgbox->status & SGL_MSGBOX_STATUS_LEFT) {
            tmp_color = msgbox->lbtn_color;
            msgbox->lbtn_color = sgl_color_mixer(msgbox->lbtn_text_color, msgbox->color, 128);
        }
        else if(msgbox->status & SGL_MSGBOX_STATUS_RIGHT) {
            tmp_color = msgbox->rbtn_color;
            msgbox->rbtn_color = sgl_color_mixer(msgbox->rbtn_text_color, msgbox->color, 128);
        }

        sgl_draw_fill_rect(surf, &button_coords, &left_coords, obj->radius, msgbox->lbtn_color, msgbox->alpha);
        sgl_draw_fill_rect(surf, &button_coords, &right_coords, obj->radius, msgbox->rbtn_color, msgbox->alpha);
        msgbox_draw_text(surf, &obj->area, &left_coords, msgbox->lbtn_text, font, msgbox->lbtn_text_color, msgbox->alpha, font_height / 2);
        msgbox_draw_text(surf, &obj->area, &right_coords, msgbox->rbtn_text, font, msgbox->rbtn_text_color, msgbox->alpha, font_height / 2);
    
        if(msgbox->status & SGL_MSGBOX_STATUS_LEFT) {
            msgbox->lbtn_color = tmp_color;
        }
        else if(msgbox->status & SGL_MSGBOX_STATUS_RIGHT) {
            msgbox->rbtn_color = tmp_color;
        }
    }
    else if(evt->type == SGL_EVENT_PRESSED) {
        if(evt->pos.y > (obj->coords.y2 - font_height - 2) && evt->pos.x < ((obj->coords.x1 + obj->coords.x2) / 2)) {
            msgbox->status |= SGL_MSGBOX_STATUS_LEFT;
            if (msgbox->exit_btn) {
                *msgbox->exit_btn = msgbox->lbtn_text;
            }
            left_coords.y1 += font_height;
            sgl_obj_update_area(&left_coords);
        }
        else if(evt->pos.y > (obj->coords.y2 - font_height - 2) && evt->pos.x > ((obj->coords.x1 + obj->coords.x2) / 2)) {
            msgbox->status |= SGL_MSGBOX_STATUS_RIGHT;
            if (msgbox->exit_btn) {
                *msgbox->exit_btn = msgbox->rbtn_text;
            }
            right_coords.y1 += font_height;
            sgl_obj_update_area(&right_coords);
        }
        else {
            sgl_obj_clear_dirty(obj);
            return;
        }
    }
    else if(evt->type == SGL_EVENT_RELEASED) {
        if(evt->pos.y > (obj->coords.y2 - font_height - 2) && evt->pos.x < ((obj->coords.x1 + obj->coords.x2) / 2)) {
            msgbox->status |= SGL_MSGBOX_STATUS_EXIT;
            sgl_obj_set_destroyed(obj);
        }
        else if(evt->pos.y > (obj->coords.y2 - font_height - 2) && evt->pos.x > ((obj->coords.x1 + obj->coords.x2) / 2)) {
            msgbox->status |= SGL_MSGBOX_STATUS_EXIT;
            sgl_obj_set_destroyed(obj);
        }
        else if (evt->pos.x == SGL_POS_MIN && evt->pos.y == SGL_POS_MIN) {
            msgbox->status |= SGL_MSGBOX_STATUS_EXIT;
            sgl_key_send_signal(evt, SGL_EVENT_DESTROYED);
            sgl_obj_set_destroyed(obj);
        }
        else {
            sgl_obj_clear_dirty(obj);
        }
    }
    else if (evt->type == SGL_EVENT_KEY_LEFT || evt->type == SGL_EVENT_KEY_RIGHT) {
        if (msgbox->status == SGL_MSGBOX_STATUS_NORMAL) {
            msgbox->status = SGL_MSGBOX_STATUS_LEFT;
        }
        else if (msgbox->status == SGL_MSGBOX_STATUS_LEFT) {
            msgbox->status = SGL_MSGBOX_STATUS_RIGHT;
            if (msgbox->exit_btn) {
                *msgbox->exit_btn = msgbox->rbtn_text;
            }
        }
        else if (msgbox->status == SGL_MSGBOX_STATUS_RIGHT) {
            msgbox->status = SGL_MSGBOX_STATUS_LEFT;
            if (msgbox->exit_btn) {
                *msgbox->exit_btn = msgbox->lbtn_text;
            }
        }
        sgl_obj_update_area(&button_coords);
    }
}

/**
 * @brief create a message box object
 * @param parent parent of the message box
 * @return message box object
 */
sgl_obj_t* sgl_msgbox_create(sgl_obj_t* parent)
{
    sgl_msgbox_t *msgbox = sgl_malloc(sizeof(sgl_msgbox_t));
    if(msgbox == NULL) {
        SGL_LOG_ERROR("sgl_msgbox_create: malloc failed");
        return NULL;
    }

    /* set object all member to zero */
    memset(msgbox, 0, sizeof(sgl_msgbox_t));

    sgl_obj_t *obj = &msgbox->obj;
    sgl_obj_init(&msgbox->obj, parent);
    obj->construct_fn = sgl_msgbox_construct_cb;
    sgl_obj_set_border_width(obj, SGL_THEME_BORDER_WIDTH);
    sgl_obj_set_radius(obj, SGL_THEME_RADIUS);
    sgl_obj_set_editable(obj);

    msgbox->alpha = SGL_THEME_ALPHA;
    msgbox->color = SGL_THEME_COLOR;
    msgbox->border_alpha = SGL_THEME_ALPHA;
    msgbox->border_color = SGL_THEME_BORDER_COLOR;
    msgbox->pixmap = NULL;

    msgbox->font = sgl_get_system_font();
    msgbox->msg_color = SGL_THEME_TEXT_COLOR;
    msgbox->title_text = "Message Box";
    msgbox->msg_line_margin = 1;
    msgbox->msg_text = "NULL";

    msgbox->lbtn_color = sgl_color_mixer(SGL_THEME_COLOR, SGL_THEME_TEXT_COLOR, 200);
    msgbox->rbtn_color = sgl_color_mixer(SGL_THEME_COLOR, SGL_THEME_TEXT_COLOR, 200);
    msgbox->lbtn_text = "YES";
    msgbox->rbtn_text = "NO";
    msgbox->lbtn_text_color = SGL_THEME_TEXT_COLOR;
    msgbox->rbtn_text_color = SGL_THEME_TEXT_COLOR;

    msgbox->status = SGL_MSGBOX_STATUS_NORMAL;
    msgbox->title_height = 0;
    msgbox->text_y_offset = 0;
    msgbox->text_x_offset = 0;
    msgbox->exit_btn = NULL;

    sgl_obj_set_clickable(obj);

    return obj;
}

/**
 * @brief set message box color
 * @param obj message box object
 * @param color message box color
 * @return none
 */
void sgl_msgbox_set_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_msgbox_t *msgbox = sgl_container_of(obj, sgl_msgbox_t, obj);
    msgbox->color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set message box alpha
 * @param obj message box object
 * @param alpha message box alpha
 * @return none
 */
void sgl_msgbox_set_alpha(sgl_obj_t *obj, uint8_t alpha)
{
    sgl_msgbox_t *msgbox = sgl_container_of(obj, sgl_msgbox_t, obj);
    msgbox->alpha = alpha;
    msgbox->border_alpha = alpha;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set message box main body alpha
 * @param obj message box object
 * @param alpha message box main body alpha
 * @return none
 */
void sgl_msgbox_set_main_alpha(sgl_obj_t *obj, uint8_t alpha)
{
    sgl_msgbox_t *msgbox = sgl_container_of(obj, sgl_msgbox_t, obj);
    msgbox->alpha = alpha;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set message box border alpha
 * @param obj message box object
 * @param alpha message box border alpha
 * @return none
 */
void sgl_msgbox_set_border_alpha(sgl_obj_t *obj, uint8_t alpha)
{
    sgl_msgbox_t *msgbox = sgl_container_of(obj, sgl_msgbox_t, obj);
    msgbox->border_alpha = alpha;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set message box radius
 * @param obj message box object
 * @param radius message box radius
 * @return none
 */
void sgl_msgbox_set_radius(sgl_obj_t *obj, uint8_t radius)
{
    sgl_obj_set_radius(obj, radius);
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set message box border width
 * @param obj message box object
 * @param width message box border width
 * @return none
 */
void sgl_msgbox_set_border_width(sgl_obj_t *obj, uint8_t width)
{
    sgl_obj_set_border_width(obj, width);
}

/**
 * @brief set message box border color
 * @param obj message box object
 * @param color message box border color
 * @return none
 */
void sgl_msgbox_set_border_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_msgbox_t *msgbox = sgl_container_of(obj, sgl_msgbox_t, obj);
    msgbox->border_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set message box pixmap
 * @param obj message box object
 * @param pixmap message box pixmap
 * @return none
 */
void sgl_msgbox_set_pixmap(sgl_obj_t *obj, const sgl_pixmap_t *pixmap)
{
    sgl_msgbox_t *msgbox = sgl_container_of(obj, sgl_msgbox_t, obj);
    msgbox->pixmap = pixmap;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set message box all text font
 * @param obj message box object
 * @param font message box font
 * @return none
 */
void sgl_msgbox_set_font(sgl_obj_t *obj, const sgl_font_t *font)
{
    sgl_msgbox_t *msgbox = sgl_container_of(obj, sgl_msgbox_t, obj);
    msgbox->font = font;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set message box title text
 * @param obj message box object
 * @param text message box title text
 * @return none
 */
void sgl_msgbox_set_title_text(sgl_obj_t *obj, const char *text)
{
    sgl_msgbox_t *msgbox = sgl_container_of(obj, sgl_msgbox_t, obj);
    msgbox->title_text = text;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set message box title text color
 * @param obj message box object
 * @param color message box title text color
 * @return none
 */
void sgl_msgbox_set_title_text_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_msgbox_t *msgbox = sgl_container_of(obj, sgl_msgbox_t, obj);
    msgbox->title_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set message box message text
 * @param obj message box object
 * @param text message box message text
 * @return none
 */
void sgl_msgbox_set_msg_text(sgl_obj_t *obj, const char *text)
{
    sgl_msgbox_t *msgbox = sgl_container_of(obj, sgl_msgbox_t, obj);
    msgbox->msg_text = text;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set message box message text color
 * @param obj message box object
 * @param color message box message text color
 * @return none
 */
void sgl_msgbox_set_msg_text_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_msgbox_t *msgbox = sgl_container_of(obj, sgl_msgbox_t, obj);
    msgbox->msg_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set message box message text line margin
 * @param obj message box object
 * @param margin message box message text line margin
 * @return none
 */
void sgl_msgbox_set_msg_line_margin(sgl_obj_t *obj, uint8_t margin)
{
    sgl_msgbox_t *msgbox = sgl_container_of(obj, sgl_msgbox_t, obj);
    msgbox->msg_line_margin = margin;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set message box left button text
 * @param obj message box object
 * @param text message box left button text
 * @return none
 */
void sgl_msgbox_set_left_btn_text(sgl_obj_t *obj, const char *text)
{
    sgl_msgbox_t *msgbox = sgl_container_of(obj, sgl_msgbox_t, obj);
    msgbox->lbtn_text = text;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set message box left button text color
 * @param obj message box object
 * @param color message box left button text color
 * @return none
 */
void sgl_msgbox_set_left_btn_text_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_msgbox_t *msgbox = sgl_container_of(obj, sgl_msgbox_t, obj);
    msgbox->lbtn_text_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set message box left button color
 * @param obj message box object
 * @param color message box left button color
 * @return none
 */
void sgl_msgbox_set_left_btn_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_msgbox_t *msgbox = sgl_container_of(obj, sgl_msgbox_t, obj);
    msgbox->lbtn_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set message box right button text
 * @param obj message box object
 * @param text message box right button text
 * @return none
 */
void sgl_msgbox_set_right_btn_text(sgl_obj_t *obj, const char *text)
{
    sgl_msgbox_t *msgbox = sgl_container_of(obj, sgl_msgbox_t, obj);
    msgbox->rbtn_text = text;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set message box right button text color
 * @param obj message box object
 * @param color message box right button text color
 * @return none
 */
void sgl_msgbox_set_right_btn_text_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_msgbox_t *msgbox = sgl_container_of(obj, sgl_msgbox_t, obj);
    msgbox->rbtn_text_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set message box right button color
 * @param obj message box object
 * @param color message box right button color
 * @return none
 */
void sgl_msgbox_set_right_btn_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_msgbox_t *msgbox = sgl_container_of(obj, sgl_msgbox_t, obj);
    msgbox->rbtn_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set message box exit answer
 * @param obj message box object
 * @param answer [out] pointer to exit answer
 * @return none
 * @note the answer will point to the answer btn string when the message box is destroyed
 */
void sgl_msgbox_set_exit_answer(sgl_obj_t *obj, const char **answer)
{
    sgl_msgbox_t *msgbox = sgl_container_of(obj, sgl_msgbox_t, obj);
    msgbox->exit_btn = answer;
}

/**
 * @brief get message box current button text
 * @param obj message box object
 * @return current current button text
 */
const char* sgl_msgbox_get_current_btn(sgl_obj_t *obj)
{
    sgl_msgbox_t *msgbox = sgl_container_of(obj, sgl_msgbox_t, obj);
    return msgbox->status & SGL_MSGBOX_STATUS_LEFT ? msgbox->lbtn_text : msgbox->rbtn_text;
}

/**
 * @brief set message box title height
 * @param obj message box object
 * @param height title bar height in pixels
 * @return none
 */
void sgl_msgbox_set_title_height(sgl_obj_t *obj, uint8_t height)
{
    sgl_msgbox_t *msgbox = sgl_container_of(obj, sgl_msgbox_t, obj);
    msgbox->title_height = height;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set message box message text x offset
 * @param obj message box object
 * @param offset x axis offset for message text
 * @return none
 */
void sgl_msgbox_set_msg_x_offset(sgl_obj_t *obj, uint8_t offset)
{
    sgl_msgbox_t *msgbox = sgl_container_of(obj, sgl_msgbox_t, obj);
    msgbox->text_x_offset = offset;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set message box message text y offset
 * @param obj message box object
 * @param offset y axis offset for message text
 * @return none
 */
void sgl_msgbox_set_msg_y_offset(sgl_obj_t *obj, uint8_t offset)
{
    sgl_msgbox_t *msgbox = sgl_container_of(obj, sgl_msgbox_t, obj);
    msgbox->text_y_offset = offset;
    sgl_obj_set_dirty(obj);
}
