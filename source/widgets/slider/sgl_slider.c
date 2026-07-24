/* source/widgets/sgl_slider.c
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
#include "sgl_slider.h"

static void sgl_slider_get_knob_dirty_area(sgl_obj_t *obj, sgl_slider_t *slider, sgl_area_t *dirty)
{
    int16_t w = obj->coords.x2 - obj->coords.x1 + 1;
    int16_t h = obj->coords.y2 - obj->coords.y1 + 1;
    int16_t knob_r, fill_pos;

    if (slider->direct == SGL_DIRECT_HORIZONTAL) {
        knob_r   = h / 2 + 1;
        int16_t bar_x1 = obj->coords.x1 + knob_r;
        int16_t bar_x2 = obj->coords.x2 - knob_r;
        fill_pos = obj->coords.x1 + w * slider->value / 100 - obj->border;
        fill_pos = sgl_clamp(fill_pos, bar_x1, bar_x2);

        dirty->x1 = fill_pos - knob_r;
        dirty->x2 = fill_pos + knob_r;
    }
    else {
        knob_r   = w / 2 + 1;
        int16_t bar_y1 = obj->coords.y1 + knob_r;
        int16_t bar_y2 = obj->coords.y2 - knob_r;
        fill_pos = obj->coords.y2 - h * slider->value / 100 + obj->border;
        fill_pos = sgl_clamp(fill_pos, bar_y1, bar_y2);

        int16_t cx = sgl_mid(obj->coords.x1, obj->coords.x2);
        dirty->x1 = cx - knob_r;
        dirty->x2 = cx + knob_r;
    }
}

static void sgl_slider_construct_cb(sgl_surf_t *surf, sgl_obj_t* obj, sgl_event_t *evt)
{
    sgl_slider_t *slider = sgl_container_of(obj, sgl_slider_t, obj);
    int16_t w = obj->coords.x2 - obj->coords.x1 + 1;
    int16_t h = obj->coords.y2 - obj->coords.y1 + 1;
    int16_t knob_r, fill_pos, radius;
    sgl_area_t bar;
    int value;
    sgl_area_t old_dirty, dirty;
    sgl_area_t desc_area = obj->area;

    switch (evt->type) {
    case SGL_EVENT_DRAW_MAIN:
        if(slider->direct == SGL_DIRECT_HORIZONTAL) {
            knob_r = h / 2 - 2;
            if (slider->thickness == 0) {
                slider->thickness = knob_r;
            }
            bar.x1 = obj->coords.x1 + knob_r + 1;
            bar.x2 = obj->coords.x2 - knob_r - 1;
            bar.y1 = obj->coords.y1 + (h - slider->thickness) / 2;
            bar.y2 = bar.y1 + slider->thickness - 1;
            fill_pos = obj->coords.x1 + (w) * slider->value / 100 - obj->border;
            fill_pos = sgl_clamp(fill_pos, bar.x1, bar.x2);
            desc_area.x1 = sgl_max(bar.x1, obj->area.x1);
            desc_area.x2 = sgl_min(fill_pos, obj->area.x2);

            radius = sgl_min(slider->thickness / 2, obj->radius);
            sgl_draw_fill_rect(surf, &desc_area, &bar, radius, slider->fill_color, SGL_ALPHA_MAX);
            desc_area.x1 = sgl_max(fill_pos, obj->area.x1);
            desc_area.x2 = sgl_min(bar.x2, obj->area.x2);
            sgl_draw_fill_rect(surf, &desc_area, &bar, radius, slider->track_color, SGL_ALPHA_MAX);
            sgl_draw_fill_circle(surf, &obj->area, fill_pos, sgl_mid(bar.y1, bar.y2), knob_r, slider->knob_color, SGL_ALPHA_MAX);
        }
        else {
            knob_r = w / 2 - 2;
            if (slider->thickness == 0) {
                slider->thickness = knob_r;
            }
            bar.y1 = obj->coords.y1 + knob_r + 1;
            bar.y2 = obj->coords.y2 - knob_r - 1;
            bar.x1 = obj->coords.x1 + (w - slider->thickness) / 2;
            bar.x2 = bar.x1 + slider->thickness - 1;
            fill_pos = obj->coords.y2 - (h) * slider->value / 100 + obj->border;
            fill_pos = sgl_clamp(fill_pos, bar.y1, bar.y2);
            desc_area.y2 = sgl_min(bar.y2, obj->area.y2);
            desc_area.y1 = sgl_max(fill_pos, obj->area.y1);

            radius = sgl_min(slider->thickness / 2, obj->radius);
            sgl_draw_fill_rect(surf, &desc_area, &bar, radius, slider->fill_color, SGL_ALPHA_MAX);
            desc_area.y2 = sgl_min(fill_pos, obj->area.y2);
            desc_area.y1 = sgl_max(bar.y1, obj->area.y1);
            sgl_draw_fill_rect(surf, &desc_area, &bar, radius, slider->track_color, SGL_ALPHA_MAX);
            sgl_draw_fill_circle(surf, &obj->area, sgl_mid(bar.x1, bar.x2), fill_pos, knob_r, slider->knob_color, SGL_ALPHA_MAX);
        }
        break;
    
    case SGL_EVENT_PRESSED:
    case SGL_EVENT_MOVE_DOWN:
    case SGL_EVENT_MOVE_UP:
    case SGL_EVENT_MOVE_LEFT:
    case SGL_EVENT_MOVE_RIGHT:
        sgl_slider_get_knob_dirty_area(obj, slider, &old_dirty);
        if(slider->direct == SGL_DIRECT_HORIZONTAL) {
            slider->value = sgl_clamp((evt->pos.x - obj->coords.x1) * 100 / (obj->coords.x2 - obj->coords.x1), 0, 100);
        }
        else {
            slider->value = sgl_clamp((obj->coords.y2 - evt->pos.y) * 100 / (obj->coords.y2 - obj->coords.y1), 0, 100);
        }

        if(evt->type == SGL_EVENT_PRESSED) {
            sgl_obj_size_zoom(obj, 2);
        }
        sgl_slider_get_knob_dirty_area(obj, slider, &dirty);
        sgl_area_selfmerge(&dirty, &old_dirty);
        dirty.y1 = obj->coords.y1;
        dirty.y2 = obj->coords.y2;
        sgl_obj_update_area(&dirty);
        break;

    case SGL_EVENT_RELEASED:
        dirty.y1 = obj->coords.y1;
        dirty.y2 = obj->coords.y2;
        sgl_obj_size_zoom(obj, -2);
        sgl_slider_get_knob_dirty_area(obj, slider, &dirty);
        sgl_obj_update_area(&dirty);
        break;

    case SGL_EVENT_KEY_LEFT:
    case SGL_EVENT_KEY_RIGHT:
        value = evt->type == SGL_EVENT_KEY_LEFT ? slider->value - 1 : slider->value + 1;
        slider->value = sgl_clamp(value, 0, 100);
        sgl_slider_get_knob_dirty_area(obj, slider, &dirty);
        dirty.y1 = obj->coords.y1 - 2;
        dirty.y2 = obj->coords.y2 + 2;
        sgl_obj_update_area(&dirty);
        break;
    }
}

/**
 * @brief create a slider object
 * @param parent parent object of the slider
 * @return slider object
 */
sgl_obj_t* sgl_slider_create(sgl_obj_t* parent)
{
    sgl_slider_t *slider = sgl_malloc(sizeof(sgl_slider_t));
    if(slider == NULL) {
        SGL_LOG_ERROR("sgl_slider_create: malloc failed");
        return NULL;
    }

    /* set object all member to zero */
    memset(slider, 0, sizeof(sgl_slider_t));

    sgl_obj_t *obj = &slider->obj;
    sgl_obj_init(&slider->obj, parent);
    sgl_obj_set_clickable(obj);
    sgl_obj_set_movable(obj);
    obj->construct_fn = sgl_slider_construct_cb;
    sgl_obj_set_border_width(obj, SGL_THEME_BORDER_WIDTH);
    sgl_obj_set_editable(obj);

    slider->direct = SGL_DIRECT_HORIZONTAL;
    slider->track_color = sgl_color_mixer(SGL_THEME_COLOR, SGL_THEME_BG_COLOR, 128);
    slider->knob_color = SGL_THEME_BG_COLOR;
    slider->fill_color = SGL_THEME_BG_COLOR;
    slider->thickness = 0;
    return obj;
}

/**
 * @brief set the fill color of the slider
 * @param obj slider object
 * @param color fill color
 * @return none
 */
void sgl_slider_set_fill_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_slider_t *slider = sgl_container_of(obj, sgl_slider_t, obj);
    slider->fill_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the track color of the slider
 * @param obj slider object
 * @param color track color
 * @return none
 */
void sgl_slider_set_track_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_slider_t *slider = sgl_container_of(obj, sgl_slider_t, obj);
    slider->track_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the slider knob color
 * @param obj slider object
 * @param color knob color
 */
void sgl_slider_set_knob_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_slider_t *slider = sgl_container_of(obj, sgl_slider_t, obj);
    slider->knob_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the slider direction
 * @param obj slider object
 * @param direct slider direction
 * @return none
 * @note direct : SGL_DIRECT_HORIZONTAL or SGL_DIRECT_VERTICAL
 */
void sgl_slider_set_direct(sgl_obj_t *obj, uint8_t direct)
{
    sgl_slider_t *slider = sgl_container_of(obj, sgl_slider_t, obj);
    slider->direct = direct;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the slider radius
 * @param obj slider object
 * @param radius slider radius
 * @return none
 */
void sgl_slider_set_radius(sgl_obj_t *obj, uint8_t radius)
{
    sgl_obj_set_radius(obj, radius);
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the slider knob thickness
 * @param obj slider object
 * @param thickness knob thickness
 * @return none
 */
void sgl_slider_set_thickness(sgl_obj_t *obj, uint8_t thickness)
{
    sgl_slider_t *slider = sgl_container_of(obj, sgl_slider_t, obj);
    slider->thickness = sgl_max(thickness, 4);
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the slider value
 * @param obj slider object
 * @param value slider value
 * @return none
 */
void sgl_slider_set_value(sgl_obj_t *obj, uint8_t value)
{
    sgl_slider_t *slider = sgl_container_of(obj, sgl_slider_t, obj);
    slider->value = value;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief get the slider value
 * @param obj slider object
 * @return slider value
 */
uint8_t sgl_slider_get_value(sgl_obj_t *obj)
{
    sgl_slider_t *slider = sgl_container_of(obj, sgl_slider_t, obj);
    return slider->value;
}

/**
 * @brief set the slider border width
 * @param obj slider object
 * @param width border width
 */
void sgl_slider_set_border_width(sgl_obj_t *obj, uint8_t width)
{
    sgl_obj_set_border_width(obj, width);
}
