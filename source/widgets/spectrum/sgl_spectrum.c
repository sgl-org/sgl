/* source/widgets/sgl_spectrum.c
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

#include <sgl_core.h>
#include <sgl_draw.h>
#include <sgl_math.h>
#include <sgl_log.h>
#include <sgl_mm.h>
#include <sgl_theme.h>
#include <sgl_cfgfix.h>
#include <string.h>
#include "sgl_spectrum.h"

static void sgl_spectrum_construct_cb(sgl_surf_t *surf, sgl_obj_t* obj, sgl_event_t *evt)
{
    sgl_spectrum_t *spectrum = sgl_container_of(obj, sgl_spectrum_t, obj);
    sgl_area_t rect = {
        .x1 = obj->coords.x1,
        .y2 = obj->coords.y2,
    };

    sgl_area_t rect_hat = {
        .x1 = obj->coords.x1,
        .y2 = obj->coords.y2,
    };

    if(evt->type == SGL_EVENT_DRAW_MAIN) {
        if (spectrum->bar_mode & SGL_SPECTRUM_MODE_BAR) {
            for (int i = 0; i < spectrum->bar_num; i++) {
                rect.x2 = rect.x1 + spectrum->bar_width - 1;
                rect.y1 = rect.y2 - spectrum->bar_value[i];
                sgl_draw_fill_rect(surf, &obj->area, &rect, 0, spectrum->bar_color, spectrum->alpha);

                if (spectrum->bar_mode & SGL_SPECTRUM_MODE_HAT_FLAG) {
                    rect_hat.x1 = rect.x1;
                    rect_hat.x2 = rect.x2;
                    rect_hat.y1 = sgl_min(obj->coords.y2 - spectrum->bar_hat[i], rect.y1 - spectrum->bar_hat_height);
                    rect_hat.y2 = rect_hat.y1 + spectrum->bar_hat_height - 1;
                    sgl_draw_fill_rect(surf, &obj->area, &rect_hat, 0, spectrum->bar_hat_color, spectrum->alpha);
                }

                rect.x1 += (spectrum->bar_width + 1);
            }
        }
        else if (spectrum->bar_mode & SGL_SPECTRUM_MODE_BLOCK) {
            int16_t pos = 0;
            for (int i = 0; i < spectrum->bar_num; i++) {
                pos = obj->coords.y2 - spectrum->bar_value[i];
                rect.x2 = rect.x1 + spectrum->bar_width - 1;

                for (rect.y2 = obj->coords.y2; rect.y2 > pos; rect.y2 -= (spectrum->bar_hat_height + 1)) {
                    rect.y1 = rect.y2 - spectrum->bar_hat_height + 1;
                    sgl_draw_fill_rect(surf, &obj->area, &rect, 0, spectrum->bar_color, spectrum->alpha);
                }

                if (spectrum->bar_mode & SGL_SPECTRUM_MODE_HAT_FLAG) {
                    rect_hat.x1 = rect.x1;
                    rect_hat.x2 = rect.x2;
                    rect_hat.y1 = sgl_min(obj->coords.y2 - spectrum->bar_hat[i], rect.y1 - spectrum->bar_hat_height);
                    rect_hat.y2 = rect_hat.y1 + spectrum->bar_hat_height - 1;
                    sgl_draw_fill_rect(surf, &obj->area, &rect_hat, 0, spectrum->bar_hat_color, spectrum->alpha);
                }

                rect.x1 += (spectrum->bar_width + 1);
            }
        }
    }
}

/**
 * @brief create a spectrum object
 * @param parent parent of the spectrum
 * @return spectrum object
 */
sgl_obj_t* sgl_spectrum_create(sgl_obj_t* parent)
{
    sgl_spectrum_t *spectrum = sgl_malloc(sizeof(sgl_spectrum_t));
    if(spectrum == NULL) {
        SGL_LOG_ERROR("sgl_spectrum_create: malloc failed");
        return NULL;
    }

    /* set object all member to zero */
    memset(spectrum, 0, sizeof(sgl_spectrum_t));

    sgl_obj_t *obj = &spectrum->obj;
    sgl_obj_init(&spectrum->obj, parent);
    obj->construct_fn = sgl_spectrum_construct_cb;

    spectrum->alpha = SGL_THEME_ALPHA;
    spectrum->bar_color = SGL_THEME_BG_COLOR;
    spectrum->bar_hat_color = sgl_color_mixer(SGL_THEME_BG_COLOR, SGL_THEME_COLOR, 128);
    spectrum->bar_mode = SGL_SPECTRUM_MODE_BLOCK;
    spectrum->bar_num = 0;
    spectrum->bar_width = 0;
    spectrum->bar_hat_height = 3;

    return obj;
}

/**
 * @brief set spectrum bar number
 * @param obj spectrum object
 * @param number bar number
 * @return none
 */
void sgl_spectrum_set_bar_number(sgl_obj_t *obj, uint16_t number)
{
    sgl_spectrum_t *spectrum = sgl_container_of(obj, sgl_spectrum_t, obj);
    spectrum->bar_value = sgl_malloc(number * sizeof(uint16_t));
    if (spectrum->bar_value == NULL) {
        SGL_LOG_ERROR("sgl_spectrum_set_buffer: malloc failed");
        return;
    }
    memset(spectrum->bar_value, 0, number * sizeof(uint16_t));

    spectrum->bar_num = number;
    spectrum->bar_width = sgl_obj_get_width(obj) / (spectrum->bar_num + 1);
}

/**
 * @brief set spectrum bar value
 * @param obj spectrum object
 * @param index bar index
 * @param value bar value
 * @return none
 */
void sgl_spectrum_set_bar_value(sgl_obj_t *obj, uint16_t index, uint16_t value)
{
    sgl_area_t area;
    int16_t hat_y1, hat_y2;
    sgl_spectrum_t *spectrum = sgl_container_of(obj, sgl_spectrum_t, obj);

    if (index >= spectrum->bar_num || value == spectrum->bar_value[index]) {
        return;
    }

    area.x1 = obj->coords.x1 + (spectrum->bar_width + 1) * index;
    area.x2 = area.x1 + spectrum->bar_width - 1;

    area.y1 = obj->coords.y2 - value;
    area.y2 = obj->coords.y2 - spectrum->bar_value[index];

    if (area.y1 > area.y2) {
        sgl_swap(&area.y1, &area.y2);
    }

    area.y1 -= (spectrum->bar_hat_height + 1);
    spectrum->bar_value[index] = value;

    if (spectrum->bar_mode & SGL_SPECTRUM_MODE_HAT_FLAG) {
        hat_y2 = obj->coords.y2 - spectrum->bar_hat[index];
        hat_y1 = area.y1 - spectrum->bar_hat_height + 1;
        area.y1 = sgl_min(hat_y2, hat_y1);
        sgl_update_area(&area);
        spectrum->bar_hat[index] = sgl_max(value, spectrum->bar_hat[index] - spectrum->bar_hat_height);
    }
    else {
        sgl_update_area(&area);
    }
}

/**
 * @brief set spectrum bar mode
 * @param obj spectrum object
 * @param mode bar mode
 * @return none
 * @note mode value:
 *       SGL_SPECTRUM_MODE_BAR: bar mode
 *       SGL_SPECTRUM_MODE_BLOCK: block mode
 *       SGL_SPECTRUM_MODE_BAR_HAT: bar mode with hat
 *       SGL_SPECTRUM_MODE_BLOCK_HAT: block mode with hat
 */
void sgl_spectrum_set_bar_mode(sgl_obj_t *obj, uint8_t mode)
{
    sgl_spectrum_t *spectrum = sgl_container_of(obj, sgl_spectrum_t, obj);
    spectrum->bar_mode = mode;

    if ((mode & SGL_SPECTRUM_MODE_HAT_FLAG) && spectrum->bar_hat == NULL) {
        spectrum->bar_hat = sgl_malloc(spectrum->bar_num * sizeof(uint16_t));
        if (spectrum->bar_hat == NULL) {
            SGL_LOG_ERROR("sgl_spectrum_set_buffer: malloc failed");
            return;
        }
    }

    sgl_obj_set_dirty(obj);
}

/**
 * @brief set spectrum bar color
 * @param obj spectrum object
 * @param color bar color
 * @return none
 */
void sgl_spectrum_set_bar_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_spectrum_t *spectrum = sgl_container_of(obj, sgl_spectrum_t, obj);
    spectrum->bar_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set spectrum bar hat color
 * @param obj spectrum object
 * @param color bar hat color
 * @return none
 */
void sgl_spectrum_set_bar_hat_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_spectrum_t *spectrum = sgl_container_of(obj, sgl_spectrum_t, obj);
    spectrum->bar_hat_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set spectrum bar hat height
 * @param obj spectrum object
 * @param height bar hat height
 * @return none
 */
void sgl_spectrum_set_bar_hat_height(sgl_obj_t *obj, uint8_t height)
{
    sgl_spectrum_t *spectrum = sgl_container_of(obj, sgl_spectrum_t, obj);
    spectrum->bar_hat_height = height;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set spectrum alpha
 * @param obj spectrum object
 * @param alpha alpha value
 * @return none
 */
void sgl_spectrum_set_alpha(sgl_obj_t *obj, uint8_t alpha)
{
    sgl_spectrum_t *spectrum = sgl_container_of(obj, sgl_spectrum_t, obj);
    spectrum->alpha = alpha;
    sgl_obj_set_dirty(obj);
}
