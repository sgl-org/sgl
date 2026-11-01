/* source/widgets/Statusbar/sgl_statusbar.h
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

#ifndef __SGL_STATUSBAR_H__
#define __SGL_STATUSBAR_H__

#include <sgl_core.h>
#include <sgl_draw.h>
#include <sgl_log.h>
#include <sgl_mm.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define  SGL_STATUSBAR_BG_COLOR           sgl_rgb(20, 20, 20)
#define SGL_STATUSBAR_LEFT_MAX            (4)
#define SGL_STATUSBAR_RIGHT_MAX           (8)

typedef struct sgl_statusbar_slot {
    char *slot;
    sgl_color_t color;
    uint8_t alpha;
    uint8_t reserved;
} sgl_statusbar_slot_t;

typedef struct sgl_statusbar {
    sgl_obj_t obj;
    const sgl_font_t *font;
    sgl_statusbar_slot_t slot_left[SGL_STATUSBAR_LEFT_MAX];
    sgl_statusbar_slot_t slot_right[SGL_STATUSBAR_RIGHT_MAX];
    uint8_t left_margin;
    uint8_t right_margin;
    sgl_color_t bg_color;
    uint8_t slot_space;
    uint8_t bg_alpha;
} sgl_statusbar_t;

/**
 * @brief create Statusbar widget
 * @param parent parent object
 * @return Statusbar object
 */
sgl_obj_t* sgl_statusbar_create(sgl_obj_t *parent);


/**
 * @brief Set statusbar font
 * @param obj statusbar object
 * @param font font
 */
void sgl_statusbar_set_font(sgl_obj_t *obj, const sgl_font_t *font);

/**
 * @brief Set statusbar background color
 * @param obj statusbar object
 * @param color background color
 */
void sgl_statusbar_set_bg_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief Set statusbar background alpha
 * @param obj statusbar object
 * @param alpha background alpha
 */
void sgl_statusbar_set_bg_alpha(sgl_obj_t *obj, uint8_t alpha);

/**
 * @brief Set statusbar background radius
 * @param obj statusbar object
 * @param radius background radius
 */
void sgl_statusbar_set_bg_radius(sgl_obj_t *obj, uint8_t radius);

/**
 * @brief Add left slot
 * @param obj statusbar object
 * @param index icon index
 * @param slot it may be a UTF8 code or string
 */
void sgl_statusbar_add_left_slot(sgl_obj_t *obj, uint8_t index, char* slot);

/**
 * @brief Add right slot
 * @param obj statusbar object
 * @param index icon index
 * @param slot it may be a UTF8 code or string
 */
void sgl_statusbar_add_right_slot(sgl_obj_t *obj, uint8_t index, char* slot);

/**
 * @brief Set left slot
 * @param obj statusbar object
 * @param index slot index
 * @param slot it may be a UTF8 code or string
 */
void sgl_statusbar_set_left_slot(sgl_obj_t *obj, uint8_t index, char* slot);

/**
 * @brief Set right slot
 * @param obj statusbar object
 * @param index slot index
 * @param slot it may be a UTF8 code or string
 */
void sgl_statusbar_set_right_slot(sgl_obj_t *obj, uint8_t index, char* slot);

/**
 * @brief Remove left slot
 * @param obj statusbar object
 * @param index slot index
 */
void sgl_statusbar_remove_left_slot(sgl_obj_t *obj, uint8_t index);

/**
 * @brief Remove right slot
 * @param obj statusbar object
 * @param index slot index
 */
void sgl_statusbar_remove_right_slot(sgl_obj_t *obj, uint8_t index);

/**
 * @brief Set left slot alpha
 * @param obj statusbar object
 * @param index slot index
 * @param alpha slot alpha
 */
void sgl_statusbar_set_left_slot_alpha(sgl_obj_t *obj, uint8_t index, uint8_t alpha);

/**
 * @brief Set right icon alpha
 * @param obj statusbar object
 * @param index icon index
 * @param alpha icon alpha
 */
void sgl_statusbar_set_right_slot_alpha(sgl_obj_t *obj, uint8_t index, uint8_t alpha);

/**
 * @brief Set left slot color
 * @param obj statusbar object
 * @param index slot index
 * @param color slot color
 */
void sgl_statusbar_set_left_slot_color(sgl_obj_t *obj, uint8_t index, sgl_color_t color);

/**
 * @brief Set right slot color
 * @param obj statusbar object
 * @param index slot index
 * @param color slot color
 */
void sgl_statusbar_set_right_slot_color(sgl_obj_t *obj, uint8_t index, sgl_color_t color);

/**
 * @brief Set slot space
 * @param obj statusbar object
 * @param space slot space
 */
void sgl_statusbar_set_slot_space(sgl_obj_t *obj, uint8_t space);

/**
 * @brief Set slot margin
 * @param obj statusbar object
 * @param left left slot margin
 * @param right right slot margin
 */
void sgl_statusbar_set_slot_margin(sgl_obj_t *obj, uint8_t left, uint8_t right);

/**
 * @brief Get left slot index
 * @param obj statusbar object
 * @param slot slot
 * @return slot index
 */
int16_t sgl_statusbar_get_left_slot_index(sgl_obj_t *obj, char *slot);

/**
 * @brief Get right slot index
 * @param obj statusbar object
 * @param slot slot
 * @return slot index
 */
int16_t sgl_statusbar_get_right_slot_index(sgl_obj_t *obj, char *slot);

#ifdef __cplusplus
}
#endif

#endif /* __SGL_STATUSBAR_H__ */
