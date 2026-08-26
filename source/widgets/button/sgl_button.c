/* source/widgets/sgl_button.c
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
#include <sgl_cfgfix.h>
#include <sgl_theme.h>
#include <string.h>
#include "sgl_button.h"

/**
 * @brief  button construct callback
 * @param  surf: surface pointer
 * @param  obj: object pointer
 * @param  evt: event pointer
 * @return none
 * @note   this function is called when the object is created or redraw
 */
static void sgl_button_construct_cb(sgl_surf_t *surf, sgl_obj_t* obj, sgl_event_t *evt)
{
    sgl_button_t *button = sgl_container_of(obj, sgl_button_t, obj);
    sgl_pos_t align_pos;
    sgl_color_t text_color;
    sgl_draw_rect_t rect = {
        .alpha = button->alpha,
        .color = button->color,
        .border = obj->border,
        .border_mask = obj->focus,
        .border_alpha = button->alpha,
        .border_color = button->border_color,
        .pixmap = button->pixmap,
        .radius = obj->radius,
    };

    switch (evt->type) {
    case SGL_EVENT_DRAW_MAIN:
        sgl_draw_rect(surf, &obj->area, &obj->coords, &rect);

        if(button->text) {
            SGL_ASSERT(button->font != NULL);
            align_pos = sgl_get_text_pos(&obj->coords, button->font, button->text, 0, (sgl_align_type_t)button->align);
            text_color = sgl_obj_is_clickable(obj) ? button->text_color : 
                                                     sgl_color_mixer(button->text_color, button->color, SGL_ALPHA_UNCLICKABLE);
            sgl_draw_string(surf, &obj->area, align_pos.x, align_pos.y, button->text, text_color, button->alpha, button->font);
        }
        break;

    case SGL_EVENT_PRESSED:
        if(sgl_obj_is_flexible(obj)) {
            sgl_obj_size_zoom(obj, 2);
        }
        sgl_obj_set_dirty(obj);
        break;

    case SGL_EVENT_RELEASED:
        if(sgl_obj_is_flexible(obj)) {
            sgl_obj_size_zoom(obj, -2);
        }
        sgl_obj_set_dirty(obj);
        break;

    default:
        break;
    }
}

/**
 * @brief  create a button object
 * @param  parent: parent object
 * @return object pointer
 * @note  if parent is NULL, the button object will be created at the top level.
 */
sgl_obj_t* sgl_button_create(sgl_obj_t* parent)
{
    sgl_button_t *button = sgl_malloc(sizeof(sgl_button_t));
    if(button == NULL) {
        SGL_LOG_ERROR("sgl_button_create: malloc failed");
        return NULL;
    }

    /* set object all member to zero */
    memset(button, 0, sizeof(sgl_button_t));

    sgl_obj_t *obj = &button->obj;
    sgl_obj_init(&button->obj, parent);
    sgl_obj_set_clickable(obj);
    sgl_obj_set_flexible(obj);
    sgl_obj_set_border_width(obj, SGL_THEME_BORDER_WIDTH);
    sgl_obj_set_radius(obj, SGL_THEME_RADIUS);

    obj->construct_fn = sgl_button_construct_cb;

    button->alpha = SGL_THEME_ALPHA;
    button->color = SGL_THEME_COLOR;
    button->border_color = SGL_THEME_BORDER_COLOR;
    button->pixmap = NULL;

    button->text = NULL;
    button->text_color = SGL_THEME_TEXT_COLOR;
    button->font = sgl_get_system_font();
    button->align = SGL_ALIGN_CENTER;

    return obj;
}

/**
 * @brief set button color
 * @param obj: object pointer
 * @param color: color
 * @return none
 */
void sgl_button_set_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_button_t *button = sgl_container_of(obj, sgl_button_t, obj);
    button->color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set button alpha
 * @param obj: object pointer
 * @param alpha: alpha
 * @return none
 */
void sgl_button_set_alpha(sgl_obj_t *obj, uint8_t alpha)
{
    sgl_button_t *button = sgl_container_of(obj, sgl_button_t, obj);
    button->alpha = alpha;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set button border width
 * @param obj: object pointer
 * @param width: border width
 * @return none
 */
void sgl_button_set_border_width(sgl_obj_t *obj, uint8_t width)
{
    sgl_obj_set_border_width(obj, width);
}

/**
 * @brief set button border color
 * @param obj: object pointer
 * @param color: border color
 * @return none
 */
void sgl_button_set_border_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_button_t *button = sgl_container_of(obj, sgl_button_t, obj);
    button->border_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set button pixmap
 * @param obj: object pointer
 * @param pixmap: pixmap pointer
 * @return none
 */
void sgl_button_set_pixmap(sgl_obj_t *obj, const sgl_pixmap_t *pixmap)
{
    sgl_button_t *button = sgl_container_of(obj, sgl_button_t, obj);
    button->pixmap = pixmap;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set button radius
 * @param obj: object pointer
 * @param radius: radius
 * @return none
 */
void sgl_button_set_radius(sgl_obj_t *obj, uint8_t radius)
{
    sgl_obj_set_radius(obj, radius);
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set button text
 * @param obj: object pointer
 * @param text: text
 * @return none
 */
void sgl_button_set_text(sgl_obj_t *obj, const char *text)
{
    sgl_button_t *button = sgl_container_of(obj, sgl_button_t, obj);
    button->text = text; 
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set button text color
 * @param obj: object pointer
 * @param color: text color
 * @return none
 */
void sgl_button_set_text_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_button_t *button = sgl_container_of(obj, sgl_button_t, obj);
    button->text_color = color; 
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set button text align
 * @param obj: object pointer
 * @param align: align type
 * @return none
 */
void sgl_button_set_text_align(sgl_obj_t *obj, uint8_t align)
{
    sgl_button_t *button = sgl_container_of(obj, sgl_button_t, obj);
    button->align = align; 
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set button text font
 * @param obj: object pointer
 * @param font: font pointer
 * @return none
 */
void sgl_button_set_font(sgl_obj_t *obj, const sgl_font_t *font)
{
    sgl_button_t *button = sgl_container_of(obj, sgl_button_t, obj);
    button->font = font; 
    sgl_obj_set_dirty(obj);
}
