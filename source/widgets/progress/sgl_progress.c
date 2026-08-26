/* source/widgets/sgl_progress.c
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
#include "sgl_progress.h"


static void sgl_progress_construct_cb(sgl_surf_t *surf, sgl_obj_t* obj, sgl_event_t *evt)
{
    sgl_progress_t *progress = sgl_container_of(obj, sgl_progress_t, obj);
    sgl_area_t knob = obj->coords;
    knob.x1 = obj->coords.x1 + obj->radius / 2 + obj->border;
    int16_t fill_radius;
    sgl_area_t rect = {
        .x1 = obj->coords.x1 - progress->interval * 2 + obj->border + 1,
        .y1 = obj->coords.y1 + obj->border + 1,
        .x2 = 0,
        .y2 = obj->coords.y2 - obj->border - 1,
    };

    if(evt->type == SGL_EVENT_DRAW_MAIN) {
        knob.x2 = obj->coords.x1 - obj->radius / 2 - 2 + (obj->coords.x2 - obj->coords.x1) * progress->value / 100;
        knob.x2 -= obj->border - 1;
        progress->body.border_mask = obj->focus;
        sgl_draw_rect(surf, &obj->area, &obj->coords, &progress->body);

        fill_radius = sgl_min3(obj->radius, progress->knob_radius, progress->knob_width / 2);
        knob.x1 = sgl_max(obj->area.x1, knob.x1);
        knob.x2 = sgl_min(obj->area.x2, knob.x2);
        knob.y1 = sgl_max(obj->area.y1, knob.y1);
        knob.y2 = sgl_min(obj->area.y2, knob.y2);

        while (rect.x2 <= knob.x2) {
            rect.x2 = rect.x1 + progress->knob_width;
            sgl_draw_fill_rect(surf, &knob, &rect, fill_radius, progress->color, progress->alpha);
            rect.x1 = rect.x2 + progress->interval;
        }
    }
}

/**
 * @brief create a progress object
 * @param parent parent object of the progress
 * @return progress object
 */
sgl_obj_t* sgl_progress_create(sgl_obj_t* parent)
{
    sgl_progress_t *progress = sgl_malloc(sizeof(sgl_progress_t));
    if(progress == NULL) {
        SGL_LOG_ERROR("sgl_progress_create: malloc failed");
        return NULL;
    }

    /* set object all member to zero */
    memset(progress, 0, sizeof(sgl_progress_t));

    sgl_obj_t *obj = &progress->obj;
    sgl_obj_init(&progress->obj, parent);
    obj->construct_fn = sgl_progress_construct_cb;
    sgl_obj_set_border_width(obj, SGL_THEME_BORDER_WIDTH);

    progress->body.alpha = SGL_THEME_ALPHA;
    progress->body.color = SGL_THEME_BG_COLOR;
    progress->body.border = SGL_THEME_BORDER_WIDTH;
    progress->body.border_alpha = SGL_THEME_ALPHA;
    progress->body.border_color = SGL_THEME_BORDER_COLOR;

    progress->color = SGL_THEME_COLOR;
    progress->alpha = SGL_THEME_ALPHA;
    progress->knob_width = 4;
    progress->interval = 4;
    progress->value = 50;
    progress->knob_radius = 0;

    return obj;
}

/**
 * @brief set progress track color
 * @param obj progress object
 * @param color track color
 * @return none
 */
void sgl_progress_set_track_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_progress_t *progress = sgl_container_of(obj, sgl_progress_t, obj);
    progress->body.color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set progress track alpha
 * @param obj progress object
 * @param alpha track alpha
 * @return none
 */
void sgl_progress_set_track_alpha(sgl_obj_t *obj, uint8_t alpha)
{
    sgl_progress_t *progress = sgl_container_of(obj, sgl_progress_t, obj);
    progress->body.alpha = alpha;
    progress->body.border_alpha = alpha;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set progress fill color
 * @param obj progress object
 * @param color fill color
 * @return none
 */
void sgl_progress_set_fill_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_progress_t *progress = sgl_container_of(obj, sgl_progress_t, obj);
    progress->color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set progress fill alpha
 * @param obj progress object
 * @param alpha fill alpha
 * @return none
 */
void sgl_progress_set_fill_alpha(sgl_obj_t *obj, uint8_t alpha)
{
    sgl_progress_t *progress = sgl_container_of(obj, sgl_progress_t, obj);
    progress->alpha = alpha;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set progress radius
 * @param obj progress object
 * @param radius progress radius
 * @return none
 */
void sgl_progress_set_radius(sgl_obj_t *obj, uint8_t radius)
{
    sgl_progress_t *progress = sgl_container_of(obj, sgl_progress_t, obj);
    sgl_obj_set_radius(obj, radius);
    progress->body.radius = obj->radius;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set progress border width
 * @param obj progress object
 * @param width progress border width
 * @return none
 */
void sgl_progress_set_border_width(sgl_obj_t *obj, uint8_t width)
{
    sgl_progress_t *progress = sgl_container_of(obj, sgl_progress_t, obj);
    progress->body.border = width;
    sgl_obj_set_border_width(obj, width);
}

/**
 * @brief set progress border color
 * @param obj progress object
 * @param color progress border color
 * @return none
 */
void sgl_progress_set_border_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_progress_t *progress = sgl_container_of(obj, sgl_progress_t, obj);
    progress->body.border_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set progress pixmap
 * @param obj progress object
 * @param pixmap progress pixmap
 * @return none
 */
void sgl_progress_set_pixmap(sgl_obj_t *obj, const sgl_pixmap_t *pixmap)
{
    sgl_progress_t *progress = sgl_container_of(obj, sgl_progress_t, obj);
    progress->body.pixmap = pixmap;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set progress fill gap size
 * @param obj progress object
 * @param gap progress fill gap size
 * @return none
 */
void sgl_progress_set_fill_gap(sgl_obj_t *obj, uint8_t gap)
{
    sgl_progress_t *progress = sgl_container_of(obj, sgl_progress_t, obj);
    progress->interval = gap;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set progress fill radius
 * @param obj progress object
 * @param radius progress fill radius
 * @return none
 */
void sgl_progress_set_fill_radius(sgl_obj_t *obj, uint8_t radius)
{
    sgl_progress_t *progress = sgl_container_of(obj, sgl_progress_t, obj);
    progress->knob_radius = radius;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set progress fill width
 * @param obj progress object
 * @param width progress fill width
 * @return none
 */
void sgl_progress_set_fill_width(sgl_obj_t *obj, uint8_t width)
{
    sgl_progress_t *progress = sgl_container_of(obj, sgl_progress_t, obj);
    progress->knob_width = width;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set progress value
 * @param obj progress object
 * @param value progress value
 * @return none
 */
void sgl_progress_set_value(sgl_obj_t *obj, uint8_t value)
{
    sgl_progress_t *progress = sgl_container_of(obj, sgl_progress_t, obj);
    progress->value = sgl_min(value, 100);
    sgl_obj_set_dirty(obj);
}

/**
 * @brief get progress value
 * @param obj progress object
 * @return progress value
 */
uint8_t sgl_progress_get_value(sgl_obj_t *obj)
{
    sgl_progress_t *progress = sgl_container_of(obj, sgl_progress_t, obj);
    return progress->value;
}
