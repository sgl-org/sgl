/* source/widgets/Statusbar/sgl_statusbar.c
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
#include "sgl_statusbar.h"

static void statusbar_update_slot(sgl_statusbar_t *bar, uint8_t index, bool left, sgl_area_t *area)
{
    int16_t pos_x, pos_y;
    sgl_obj_t *obj = &bar->obj;
    pos_y = obj->coords.y1 + (sgl_obj_get_height(obj) - bar->font->font_height) / 2;

    if (left) {
        pos_x = obj->coords.x1 + bar->left_margin;
        for (int i = 0; i < index; i++) {
            if (bar->slot_left[i].slot == NULL) {
                continue;
            }
            pos_x += sgl_font_get_string_width(bar->slot_left[i].slot, bar->font);
            pos_x += bar->slot_space;
        }
        area->x1 = pos_x;
        area->x2 = pos_x + sgl_font_get_string_width(bar->slot_left[index].slot, bar->font);
    }
    else {
        pos_x = obj->coords.x2 - bar->right_margin;
        for (int i = 0; i < index; i++) {
            if (bar->slot_right[i].slot == NULL) {
                continue;
            }
            pos_x -= sgl_font_get_string_width(bar->slot_right[i].slot, bar->font);
            pos_x -= bar->slot_space;
        }
        area->x1 = pos_x - sgl_font_get_string_width(bar->slot_right[index].slot, bar->font);
        area->x2 = pos_x;
    }
    area->y1 = pos_y;
    area->y2 = pos_y + bar->font->font_height;
}

static void sgl_statusbar_construct_cb(sgl_surf_t *surf, sgl_obj_t *obj, sgl_event_t *evt)
{
    sgl_statusbar_t *bar = sgl_container_of(obj, sgl_statusbar_t, obj);
    int16_t pos_x, pos_y, char_w;

    switch (evt->type) {
    case SGL_EVENT_DRAW_MAIN:
        sgl_draw_fill_rect(surf, &obj->area, &obj->coords, obj->radius, bar->bg_color, bar->bg_alpha);

        if (unlikely(!bar->font)) {
            return;
        }

        pos_y = obj->coords.y1 + (sgl_obj_get_height(obj) - bar->font->font_height) / 2;
        pos_x = obj->coords.x1 + bar->left_margin;

        for (int i = 0; i < SGL_STATUSBAR_LEFT_MAX; i++) {
            if (bar->slot_left[i].slot == NULL) {
                continue;
            }
            sgl_draw_string(surf, &obj->area, pos_x, pos_y, bar->slot_left[i].slot, bar->slot_left[i].color, bar->slot_left[i].alpha, bar->font);
            pos_x += sgl_font_get_string_width(bar->slot_left[i].slot, bar->font);
            pos_x += bar->slot_space;
        }

        pos_x = obj->coords.x2 - bar->right_margin;
        for (int i = 0; i < SGL_STATUSBAR_RIGHT_MAX; i++) {
            if (bar->slot_right[i].slot == NULL) {
                continue;
            }
            char_w = sgl_font_get_string_width(bar->slot_right[i].slot, bar->font);
            pos_x -= char_w;
            sgl_draw_string(surf, &obj->area, pos_x, pos_y, bar->slot_right[i].slot, bar->slot_right[i].color, bar->slot_right[i].alpha, bar->font);
            pos_x -= bar->slot_space;
        }
    break;
    default:
        break;
    };
}

/**
 * @brief Create a statusbar object
 * @param parent object parent
 * @return statusbar object
 */
sgl_obj_t* sgl_statusbar_create(sgl_obj_t *parent)
{
    sgl_statusbar_t *bar = sgl_malloc(sizeof(sgl_statusbar_t));
    if (bar == NULL) {
        SGL_LOG_ERROR("sgl_statusbar_create: malloc failed");
        return NULL;
    }

    /* set object all member to zero */
    memset(bar, 0, sizeof(sgl_statusbar_t));
    sgl_obj_t *obj = &bar->obj;
    sgl_obj_init(obj, parent);
    obj->construct_fn = sgl_statusbar_construct_cb;

    bar->bg_alpha = SGL_ALPHA_MAX / 2;
    bar->bg_color = SGL_STATUSBAR_BG_COLOR;
    bar->left_margin = 5;
    bar->right_margin = 5;
    bar->slot_space = 4;

    return obj;
}

/**
 * @brief Set statusbar font
 * @param obj statusbar object
 * @param font font
 */
void sgl_statusbar_set_font(sgl_obj_t *obj, const sgl_font_t *font)
{
    sgl_statusbar_t *bar = sgl_container_of(obj, sgl_statusbar_t, obj);
    bar->font = font;
}

/**
 * @brief Set statusbar background color
 * @param obj statusbar object
 * @param color background color
 */
void sgl_statusbar_set_bg_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_statusbar_t *bar = sgl_container_of(obj, sgl_statusbar_t, obj);
    bar->bg_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set statusbar background alpha
 * @param obj statusbar object
 * @param alpha background alpha
 */
void sgl_statusbar_set_bg_alpha(sgl_obj_t *obj, uint8_t alpha)
{
    sgl_statusbar_t *bar = sgl_container_of(obj, sgl_statusbar_t, obj);
    bar->bg_alpha = alpha;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set statusbar background radius
 * @param obj statusbar object
 * @param radius background radius
 */
void sgl_statusbar_set_bg_radius(sgl_obj_t *obj, uint8_t radius)
{
    sgl_obj_set_radius(obj, radius);
    sgl_obj_set_dirty(obj);
}

/**
 * @brief add left slot
 * @param obj statusbar object
 * @param index icon index
 * @param slot it may be a UTF8 code or string
 */
void sgl_statusbar_add_left_slot(sgl_obj_t *obj, uint8_t index, char* slot)
{
    sgl_statusbar_t *bar = sgl_container_of(obj, sgl_statusbar_t, obj);
    if (index < SGL_STATUSBAR_LEFT_MAX) {
        if (bar->slot_left[index].slot != NULL) {
            memmove(bar->slot_left + index + 1, bar->slot_left + index, (SGL_STATUSBAR_LEFT_MAX - index - 1) * sizeof(sgl_statusbar_slot_t));
        }
        bar->slot_left[index].slot = slot;
        bar->slot_left[index].alpha = SGL_ALPHA_MAX;
        bar->slot_left[index].color = SGL_COLOR_WHITE;
        sgl_obj_set_dirty(obj);
    }
}

/**
 * @brief add right slot
 * @param obj statusbar object
 * @param index icon index
 * @param slot it may be a UTF8 code or string
 */
void sgl_statusbar_add_right_slot(sgl_obj_t *obj, uint8_t index, char* slot)
{
    sgl_statusbar_t *bar = sgl_container_of(obj, sgl_statusbar_t, obj);
    if (index < SGL_STATUSBAR_RIGHT_MAX) {
        if (bar->slot_right[index].slot != NULL) {
            memmove(bar->slot_right + index + 1, bar->slot_right + index, (SGL_STATUSBAR_RIGHT_MAX - index - 1) * sizeof(sgl_statusbar_slot_t));
        }
        bar->slot_right[index].slot = slot;
        bar->slot_right[index].alpha = SGL_ALPHA_MAX;
        bar->slot_right[index].color = SGL_COLOR_WHITE;
        sgl_obj_set_dirty(obj);
    }
}

/**
 * @brief Set left slot
 * @param obj statusbar object
 * @param index slot index
 * @param slot it may be a UTF8 code or string
 */
void sgl_statusbar_set_left_slot(sgl_obj_t *obj, uint8_t index, char* slot)
{
    sgl_statusbar_t *bar = sgl_container_of(obj, sgl_statusbar_t, obj);
    sgl_area_t area;

    if (index < SGL_STATUSBAR_LEFT_MAX) {
        bar->slot_left[index].slot = slot;
        statusbar_update_slot(bar, index, true, &area);
        sgl_obj_update_area(&area);
    }
}

/**
 * @brief Set right slot
 * @param obj statusbar object
 * @param index slot index
 * @param slot it may be a UTF8 code or string
 */
void sgl_statusbar_set_right_slot(sgl_obj_t *obj, uint8_t index, char* slot)
{
    sgl_statusbar_t *bar = sgl_container_of(obj, sgl_statusbar_t, obj);
    sgl_area_t area;

    if (index < SGL_STATUSBAR_RIGHT_MAX) {
        bar->slot_right[index].slot = slot;
        statusbar_update_slot(bar, index, false, &area);
        sgl_obj_update_area(&area);
    }
}

/**
 * @brief Remove left slot
 * @param obj statusbar object
 * @param index slot index
 */
void sgl_statusbar_remove_left_slot(sgl_obj_t *obj, uint8_t index)
{
    sgl_statusbar_t *bar = sgl_container_of(obj, sgl_statusbar_t, obj);
    if (index < SGL_STATUSBAR_LEFT_MAX) {
        memmove(bar->slot_left + index, bar->slot_left + index + 1, (SGL_STATUSBAR_LEFT_MAX - index - 1) * sizeof(sgl_statusbar_slot_t));
        sgl_obj_set_dirty(obj);
    }
}

/**
 * @brief Remove right slot
 * @param obj statusbar object
 * @param index slot index
 */
void sgl_statusbar_remove_right_slot(sgl_obj_t *obj, uint8_t index)
{
    sgl_statusbar_t *bar = sgl_container_of(obj, sgl_statusbar_t, obj);
    if (index < SGL_STATUSBAR_RIGHT_MAX) {
        memmove(bar->slot_right + index, bar->slot_right + index + 1, (SGL_STATUSBAR_RIGHT_MAX - index - 1) * sizeof(sgl_statusbar_slot_t));
        sgl_obj_set_dirty(obj);
    }
}

/**
 * @brief Set left slot alpha
 * @param obj statusbar object
 * @param index slot index
 * @param alpha slot alpha
 */
void sgl_statusbar_set_left_slot_alpha(sgl_obj_t *obj, uint8_t index, uint8_t alpha)
{
    sgl_statusbar_t *bar = sgl_container_of(obj, sgl_statusbar_t, obj);
    sgl_area_t area;

    if (index < SGL_STATUSBAR_LEFT_MAX) {
        bar->slot_left[index].alpha = alpha;
        statusbar_update_slot(bar, index, true, &area);
        sgl_obj_update_area(&area);
    }
}

/**
 * @brief Set right icon alpha
 * @param obj statusbar object
 * @param index icon index
 * @param alpha icon alpha
 */
void sgl_statusbar_set_right_slot_alpha(sgl_obj_t *obj, uint8_t index, uint8_t alpha)
{
    sgl_statusbar_t *bar = sgl_container_of(obj, sgl_statusbar_t, obj);
    sgl_area_t area;

    if (index < SGL_STATUSBAR_RIGHT_MAX) {
        bar->slot_right[index].alpha = alpha;
        statusbar_update_slot(bar, index, false, &area);
        sgl_obj_update_area(&area);
    }
}

/**
 * @brief Set left slot color
 * @param obj statusbar object
 * @param index slot index
 * @param color slot color
 */
void sgl_statusbar_set_left_slot_color(sgl_obj_t *obj, uint8_t index, sgl_color_t color)
{
    sgl_statusbar_t *bar = sgl_container_of(obj, sgl_statusbar_t, obj);
    sgl_area_t area;

    if (index < SGL_STATUSBAR_LEFT_MAX) {
        bar->slot_left[index].color = color;
        statusbar_update_slot(bar, index, true, &area);
        sgl_obj_update_area(&area);
    }
}

/**
 * @brief Set right slot color
 * @param obj statusbar object
 * @param index slot index
 * @param color slot color
 */
void sgl_statusbar_set_right_slot_color(sgl_obj_t *obj, uint8_t index, sgl_color_t color)
{
    sgl_statusbar_t *bar = sgl_container_of(obj, sgl_statusbar_t, obj);
    sgl_area_t area;

    if (index < SGL_STATUSBAR_LEFT_MAX) {
        bar->slot_right[index].color = color;
        statusbar_update_slot(bar, index, false, &area);
        sgl_obj_update_area(&area);
    }
}

/**
 * @brief Set slot space
 * @param obj statusbar object
 * @param space slot space
 */
void sgl_statusbar_set_slot_space(sgl_obj_t *obj, uint8_t space)
{
    sgl_statusbar_t *bar = sgl_container_of(obj, sgl_statusbar_t, obj);
    bar->slot_space = space;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set slot margin
 * @param obj statusbar object
 * @param left left slot margin
 * @param right right slot margin
 */
void sgl_statusbar_set_slot_margin(sgl_obj_t *obj, uint8_t left, uint8_t right)
{
    sgl_statusbar_t *bar = sgl_container_of(obj, sgl_statusbar_t, obj);
    bar->left_margin = left;
    bar->right_margin = right;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Get left slot index
 * @param obj statusbar object
 * @param slot slot
 * @return slot index
 */
int16_t sgl_statusbar_get_left_slot_index(sgl_obj_t *obj, char *slot)
{
    sgl_statusbar_t *bar = sgl_container_of(obj, sgl_statusbar_t, obj);
    for (int i = 0; i < SGL_STATUSBAR_LEFT_MAX; i++) {
        if (strcmp(bar->slot_left[i].slot, slot) == 0) {
            return i;
        }
    }
    return -1;
}

/**
 * @brief Get right slot index
 * @param obj statusbar object
 * @param slot slot
 * @return slot index
 */
int16_t sgl_statusbar_get_right_slot_index(sgl_obj_t *obj, char *slot)
{
    sgl_statusbar_t *bar = sgl_container_of(obj, sgl_statusbar_t, obj);
    for (int i = 0; i < SGL_STATUSBAR_RIGHT_MAX; i++) {
        if (strcmp(bar->slot_right[i].slot, slot) == 0) {
            return i;
        }
    }
    return -1;
}
