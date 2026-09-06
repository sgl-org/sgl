/* source/widgets/sgl_checkbox.c
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

#include <sgl_core.h>
#include <sgl_draw.h>
#include <sgl_math.h>
#include <sgl_log.h>
#include <sgl_mm.h>
#include <sgl_theme.h>
#include <sgl_cfgfix.h>
#include <string.h>
#include "sgl_checkbox.h"

/**
 * @brief checkbox construct callback
 * @param surf surface pointer
 * @param obj checkbox object
 * @param evt event parameter
 * @return none
 */
static void sgl_checkbox_construct_cb(sgl_surf_t *surf, sgl_obj_t* obj, sgl_event_t *evt)
{
    sgl_checkbox_t *checkbox = sgl_container_of(obj, sgl_checkbox_t, obj);
    const int16_t font_h = sgl_font_get_height(checkbox->font);
    const int16_t box_w = font_h;
    sgl_pos_t align_pos = sgl_get_text_pos(&obj->coords, checkbox->font, checkbox->text, 0, SGL_ALIGN_LEFT_MID);
    sgl_area_t icon = {
        .x1 = obj->coords.x1 + 1,
        .y1 = align_pos.y,
        .x2 = obj->coords.x1 + box_w,
        .y2 = align_pos.y + box_w - 1,
    };

    SGL_ASSERT(checkbox->font != NULL);
    if(evt->type == SGL_EVENT_DRAW_MAIN) {
        if(checkbox->status) {
            sgl_draw_fill_rect(surf, &obj->area, &icon, box_w / 4, checkbox->box_color, checkbox->alpha);
            const int16_t base_x = icon.x1;
            const int16_t base_y = icon.y1;

            const int32_t bw = (int32_t)box_w;
            const int16_t off_20 = (int16_t)((bw * 205) >> 10);
            const int16_t off_50 = (int16_t)(bw >> 1);
            const int16_t off_40 = (int16_t)((bw * 410) >> 10);
            const int16_t off_72 = (int16_t)((bw * 737) >> 10);
            const int16_t off_26 = (int16_t)((bw * 266) >> 10);

            int16_t ax1 = base_x + off_20;
            int16_t ay1 = base_y + off_50;
            int16_t ax2 = base_x + off_40;
            int16_t ay2 = base_y + off_72;
            int16_t ax3 = base_x + off_72;
            int16_t ay3 = base_y + off_26;

            uint8_t lw = (uint8_t)(box_w >> 3);
            if (lw < 1) lw = 1;

            sgl_draw_line_fill_slanted(surf, &obj->area, ax1, ay1, ax2, ay2, lw, checkbox->check_color, checkbox->alpha);
            sgl_draw_line_fill_slanted(surf, &obj->area, ax2, ay2, ax3, ay3, lw, checkbox->check_color, checkbox->alpha);
        }
        else {
            sgl_draw_fill_rect_border(surf, &obj->area, &icon, box_w / 4, checkbox->box_color, 2, checkbox->alpha);
        }

        sgl_draw_string(surf, &obj->area, align_pos.x + box_w + 4, align_pos.y, checkbox->text, checkbox->text_color, checkbox->alpha, checkbox->font);
    }
    else if(evt->type == SGL_EVENT_PRESSED) {
        checkbox->status = !checkbox->status;
        sgl_obj_update_area(&icon);
    }
}

/**
 * @brief create a checkbox object
 * @param parent parent of the checkbox
 * @return checkbox object
 */
sgl_obj_t* sgl_checkbox_create(sgl_obj_t* parent)
{
    sgl_checkbox_t *checkbox = sgl_malloc(sizeof(sgl_checkbox_t));
    if(checkbox == NULL) {
        SGL_LOG_ERROR("sgl_checkbox_create: malloc failed");
        return NULL;
    }

    /* set object all member to zero */
    memset(checkbox, 0, sizeof(sgl_checkbox_t));

    sgl_obj_t *obj = &checkbox->obj;
    sgl_obj_init(&checkbox->obj, parent);
    obj->construct_fn = sgl_checkbox_construct_cb;
    obj->needinit = 1;

    checkbox->status = false;
    checkbox->alpha = SGL_ALPHA_MAX;
    checkbox->text_color = SGL_THEME_TEXT_COLOR;
    checkbox->box_color = sgl_rgb(33, 150, 243);
    checkbox->check_color = SGL_COLOR_WHITE;
    obj->coords.y2 = SGL_POS_INVALID;
    obj->coords.x2 = SGL_POS_INVALID;

    sgl_obj_set_clickable(obj);

    checkbox->font = sgl_get_system_font();
    checkbox->text = " ";
    return obj;
}

/**
 * @brief set checkbox text and icon color
 * @param obj checkbox object
 * @param color color of text
 * @return none
 */
void sgl_checkbox_set_text_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_checkbox_t *checkbox = sgl_container_of(obj, sgl_checkbox_t, obj);
    checkbox->text_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set checkbox box color
 * @param obj checkbox object
 * @param color color of box
 * @return none
 */
void sgl_checkbox_set_box_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_checkbox_t *checkbox = sgl_container_of(obj, sgl_checkbox_t, obj);
    checkbox->box_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set checkbox check color
 * @param obj checkbox object
 * @param color color of check
 * @return none
 */
void sgl_checkbox_set_check_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_checkbox_t *checkbox = sgl_container_of(obj, sgl_checkbox_t, obj);
    checkbox->check_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set checkbox text and icon alpha
 * @param obj checkbox object
 * @param alpha alpha of text
 * @return none
 */
void sgl_checkbox_set_alpha(sgl_obj_t *obj, uint8_t alpha)
{
    sgl_checkbox_t *checkbox = sgl_container_of(obj, sgl_checkbox_t, obj);
    checkbox->alpha = alpha;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set checkbox text
 * @param obj checkbox object
 * @param text text of checkbox
 * @return none
 */
void sgl_checkbox_set_text(sgl_obj_t *obj, const char *text)
{
    sgl_checkbox_t *checkbox = sgl_container_of(obj, sgl_checkbox_t, obj);
    checkbox->text = text;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set checkbox font
 * @param obj checkbox object
 * @param font font of checkbox
 * @return none
 */
void sgl_checkbox_set_font(sgl_obj_t *obj, const sgl_font_t *font)
{
    sgl_checkbox_t *checkbox = sgl_container_of(obj, sgl_checkbox_t, obj);
    checkbox->font = font;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set checkbox radius
 * @param obj checkbox object
 * @param radius radius of checkbox
 * @return none
 */
void sgl_checkbox_set_radius(sgl_obj_t *obj, int16_t radius)
{
    sgl_obj_set_radius(obj, radius);
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set checkbox status
 * @param obj checkbox object
 * @param status status of checkbox
 * @return none
 */
void sgl_checkbox_set_status(sgl_obj_t *obj, bool status)
{
    sgl_checkbox_t *checkbox = sgl_container_of(obj, sgl_checkbox_t, obj);
    checkbox->status = status;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief get checkbox status
 * @param obj checkbox object
 * @return status of checkbox
 */
bool sgl_checkbox_get_status(sgl_obj_t *obj)
{
    sgl_checkbox_t *checkbox = sgl_container_of(obj, sgl_checkbox_t, obj);
    return checkbox->status;
}
