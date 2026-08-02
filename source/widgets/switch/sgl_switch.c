/* source/widgets/sgl_switch.c
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
#include "sgl_switch.h"

static void sgl_switch_construct_cb(sgl_surf_t *surf, sgl_obj_t *obj, sgl_event_t *evt)
{
    sgl_switch_t *p_switch = sgl_container_of(obj, sgl_switch_t, obj);
    int16_t margin = p_switch->knob_margin;
    int16_t border = obj->border;
    int16_t obj_h = obj->coords.y2 - obj->coords.y1;
    int16_t knob_size, bg_inset, knob_offset;

    if (margin >= 0) {
        knob_offset = margin + border;
        knob_size   = obj_h - 2 * knob_offset;
        bg_inset    = 0;
    } else {
        knob_offset = border;
        knob_size   = obj_h - 2 * knob_offset;
        bg_inset    = -margin;
    }

    if (knob_size < 0) knob_size = 0;

    sgl_area_t bg_rect = obj->coords;
    if (bg_inset > 0) {
        bg_rect.x1 += bg_inset;
        bg_rect.x2 -= bg_inset;
        bg_rect.y1 += bg_inset;
        bg_rect.y2 -= bg_inset;
    }

    int16_t bg_draw_h = bg_rect.y2 - bg_rect.y1;
    int16_t bg_r = sgl_min(obj->radius, bg_draw_h / 2 - 1);
    int16_t knob_r;

    if (margin >= 0) {
        knob_r = bg_r - knob_offset / 2 - 2 * border + 1;
    } else {
        knob_r = sgl_min(obj->radius - border + 1, knob_size / 2);
    }

    if (knob_r < 0) knob_r = 2;

    sgl_area_t knob_rect = {0};
    knob_rect.y1 = obj->coords.y1 + knob_offset;
    knob_rect.y2 = knob_rect.y1 + knob_size;

    sgl_draw_rect_t bg_desc = {
        .alpha        = p_switch->alpha,
        .border_alpha = p_switch->alpha,
        .border       = border,
        .color        = p_switch->bg_color,
        .border_color = p_switch->border_color,
        .border_mask  = obj->focus,
        .pixmap       = p_switch->pixmap,
        .radius       = bg_r,
    };

    if (evt->type == SGL_EVENT_DRAW_MAIN) {
        if (p_switch->status) {
            bg_desc.color = p_switch->color;
            knob_rect.x2 = obj->coords.x2 - knob_offset;
            knob_rect.x1 = knob_rect.x2 - knob_size;
        } else {
            bg_desc.color = p_switch->bg_color;
            knob_rect.x1 = obj->coords.x1 + knob_offset;
            knob_rect.x2 = knob_rect.x1 + knob_size;
        }

        sgl_draw_rect(surf, &obj->area, &bg_rect, &bg_desc);
        sgl_draw_fill_rect(surf, &obj->area, &knob_rect, knob_r, p_switch->knob_color, p_switch->alpha);
    }
    else if (evt->type == SGL_EVENT_PRESSED) {
        p_switch->status = !p_switch->status;
        sgl_obj_set_dirty(obj);
    }
}

/**
 * @brief create a switch object
 * @param parent parent of the switch
 * @return switch object
 */
sgl_obj_t* sgl_switch_create(sgl_obj_t* parent)
{
    sgl_switch_t *p_switch = sgl_malloc(sizeof(sgl_switch_t));
    if(p_switch == NULL) {
        SGL_LOG_ERROR("sgl_switch_create: malloc failed");
        return NULL;
    }

    /* set object all member to zero */
    memset(p_switch, 0, sizeof(sgl_switch_t));

    sgl_obj_t *obj = &p_switch->obj;
    sgl_obj_init(&p_switch->obj, parent);
    obj->construct_fn = sgl_switch_construct_cb;
    sgl_obj_set_border_width(obj, SGL_THEME_BORDER_WIDTH);
    sgl_obj_set_radius(obj, SGL_THEME_RADIUS);

    p_switch->alpha = SGL_THEME_ALPHA;
    p_switch->color = SGL_THEME_COLOR;
    p_switch->border_color = SGL_THEME_BORDER_COLOR;

    p_switch->status = false;
    p_switch->bg_color = SGL_THEME_BG_COLOR;
    p_switch->knob_color = sgl_color_mixer(SGL_THEME_COLOR, SGL_THEME_BG_COLOR, 128);
    p_switch->knob_margin = 1;

    sgl_obj_set_clickable(obj);
    return obj;
}

/**
 * @brief set switch color
 * @param obj switch object
 * @param color switch color
 * @return none
 */
void sgl_switch_set_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_switch_t *switch_obj = sgl_container_of(obj, sgl_switch_t, obj);
    switch_obj->color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set switch background color
 * @param obj switch object
 * @param color switch background color
 * @return none
 */
void sgl_switch_set_bg_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_switch_t *switch_obj = sgl_container_of(obj, sgl_switch_t, obj);
    switch_obj->bg_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set the knob color of the switch object
 * @param obj  Pointer to the switch object
 * @param color  The color of the knob
 * @return none
 */
void sgl_switch_set_knob_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_switch_t *switch_obj = sgl_container_of(obj, sgl_switch_t, obj);
    switch_obj->knob_color = color;
    sgl_obj_set_dirty(obj);
}


/**
 * @brief Set the pixmap of the switch object
 * @param obj  Pointer to the switch object
 * @param pixmap  Pointer to the pixmap
 * @return none
 */
void sgl_switch_set_pixmap(sgl_obj_t *obj, const sgl_pixmap_t *pixmap)
{
    sgl_switch_t *switch_obj = sgl_container_of(obj, sgl_switch_t, obj);
    switch_obj->pixmap = pixmap;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set switch alpha
 * @param obj switch object
 * @param alpha switch alpha
 * @return none
 */
void sgl_switch_set_alpha(sgl_obj_t *obj, uint8_t alpha)
{
    sgl_switch_t *switch_obj = sgl_container_of(obj, sgl_switch_t, obj);
    switch_obj->alpha = alpha;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set switch radius
 * @param obj switch object
 * @param radius switch radius
 * @return none
 */
void sgl_switch_set_radius(sgl_obj_t *obj, uint16_t radius)
{
    sgl_obj_set_radius(obj, radius);
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set switch border color
 * @param obj switch object
 * @param color switch border color
 * @return none
 */
void sgl_switch_set_border_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_switch_t *switch_obj = sgl_container_of(obj, sgl_switch_t, obj);
    switch_obj->border_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set switch border width
 * @param obj switch object
 * @param width switch border width
 * @return none
 */
void sgl_switch_set_border_width(sgl_obj_t *obj, int16_t width)
{
    sgl_obj_set_border_width(obj, width);
}

/**
 * @brief set status of switch
 * @param obj switch object
 * @param status switch status
 * @return none 
 */
void sgl_switch_set_status(sgl_obj_t *obj, bool status)
{
    sgl_switch_t *switch_obj = sgl_container_of(obj, sgl_switch_t, obj);
    switch_obj->status = status;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief get status of switch
 * @param obj switch object
 * @return switch status
 */
bool sgl_switch_get_status(sgl_obj_t *obj)
{
    sgl_switch_t *switch_obj = sgl_container_of(obj, sgl_switch_t, obj);
    return switch_obj->status;
}

/**
 * @brief set knob margin of switch
 * @param obj switch object
 * @param margin knob margin
 * @return none
 */
void sgl_switch_set_knob_margin(sgl_obj_t *obj, int16_t margin)
{
    sgl_switch_t *switch_obj = sgl_container_of(obj, sgl_switch_t, obj);
    switch_obj->knob_margin = margin;
    sgl_obj_set_dirty(obj);
}
