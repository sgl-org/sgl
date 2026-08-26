/* source/widgets/sgl_line.c
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
#include "sgl_line.h"


static void sgl_line_draw_dashed(sgl_surf_t *surf, sgl_area_t *area, sgl_draw_line_t *desc, sgl_line_t *line)
{
    uint16_t dash_len = line->dash_length;
    uint16_t gap_len = line->gap_length;

    /* If pattern is not configured properly, fallback to solid line */
    if (dash_len == 0 || gap_len == 0) {
        sgl_draw_line(surf, area, desc);
        return;
    }

    int16_t x0 = desc->x1;
    int16_t y0 = desc->y1;
    int16_t x1 = desc->x2;
    int16_t y1 = desc->y2;

    int16_t dx = sgl_abs(x1 - x0);
    int16_t dy = sgl_abs(y1 - y0);
    int16_t sx = (x0 < x1) ? 1 : -1;
    int16_t sy = (y0 < y1) ? 1 : -1;
    int16_t err = dx - dy;
    int16_t e2;

    uint32_t pattern_len = (uint32_t)dash_len + (uint32_t)gap_len;
    uint32_t pattern_pos = 0;

    sgl_area_t clip_area = {
        .x1 = surf->x1,
        .y1 = surf->y1,
        .x2 = surf->x2,
        .y2 = surf->y2
    };

    sgl_area_selfclip(&clip_area, area);

    while (1) {
        if (pattern_pos < dash_len) {
            if (x0 >= clip_area.x1 && x0 <= clip_area.x2 &&
                y0 >= clip_area.y1 && y0 <= clip_area.y2) {

                sgl_color_t *buf = sgl_surf_get_buf(surf, x0 - surf->x1, y0 - surf->y1);
                if (desc->alpha == SGL_ALPHA_MAX) {
                    *buf = desc->color;
                } else {
                    *buf = sgl_color_mixer(desc->color, *buf, desc->alpha);
                }
            }
        }

        if (x0 == x1 && y0 == y1) {
            break;
        }

        pattern_pos++;
        if (pattern_pos >= pattern_len) {
            pattern_pos = 0;
        }

        e2 = err << 1;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

static void sgl_line_construct_cb(sgl_surf_t *surf, sgl_obj_t* obj, sgl_event_t *evt)
{
    sgl_line_t *line = sgl_container_of(obj, sgl_line_t, obj);
    sgl_draw_line_t desc;

    desc.x1 = obj->coords.x1;
    desc.y1 = obj->coords.y1;
    desc.x2 = obj->coords.x2;
    desc.y2 = obj->coords.y2;
    desc.color = line->color;
    desc.alpha = line->alpha;
    desc.width = obj->border;

    if(evt->type == SGL_EVENT_DRAW_MAIN) {
        if (line->x_swap) {
            sgl_swap(&desc.x1, &desc.x2);
        }
        if (line->y_swap) {
            sgl_swap(&desc.y1, &desc.y2);
        }

        if (line->dashed) {
            sgl_line_draw_dashed(surf, &obj->parent->area, &desc, line);
        } else {
            sgl_draw_line(surf, &obj->parent->area, &desc);
        }
    }
}

/**
 * @brief create a line object
 * @param parent parent of the line
 * @return line object
 */
sgl_obj_t* sgl_line_create(sgl_obj_t* parent)
{
    sgl_line_t *line = sgl_malloc(sizeof(sgl_line_t));
    if(line == NULL) {
        SGL_LOG_ERROR("sgl_line_create: malloc failed");
        return NULL;
    }

    /* set object all member to zero */
    memset(line, 0, sizeof(sgl_line_t));

    sgl_obj_t *obj = &line->obj;
    sgl_obj_init(&line->obj, parent);
    obj->construct_fn = sgl_line_construct_cb;

    line->color = SGL_THEME_BG_COLOR;
    line->alpha = SGL_ALPHA_MAX;
    obj->border = 1;

    return obj;
}

/**
 * @brief set line start position
 * @param obj line object
 * @param x start x position
 * @param y start y position
 * @return none
 */
void sgl_line_set_pos(sgl_obj_t *obj, int16_t x1, int16_t y1, int16_t x2, int16_t y2)
{
	SGL_ASSERT(obj != NULL);
    int16_t _x1, _y1, _x2, _y2;
	sgl_line_t *line = sgl_container_of(obj, sgl_line_t, obj);

    _x1 = obj->parent->coords.x1 + x1;
    _x2 = obj->parent->coords.x1 + x2;
    _y1 = obj->parent->coords.y1 + y1;
    _y2 = obj->parent->coords.y1 + y2;

	if (_x1 > _x2) {
		sgl_swap(&_x1, &_x2);
        line->x_swap = 1;
	}
    else {
        line->x_swap = 0;
    }

	if (_y1 > _y2) {
		sgl_swap(&_y1, &_y2);
        line->y_swap = 1;
	}
    else {
        line->y_swap = 0;
    }

	/* default thinckness is 1 */
	obj->coords.x1 = _x1;
	obj->coords.y1 = _y1;
	obj->coords.x2 = _x2;
	obj->coords.y2 = _y2;
	obj->border = 2;

	sgl_obj_set_dirty(obj);
}

/**
 * @brief set line color
 * @param obj line object
 * @param color line color
 * @return none
 */
void sgl_line_set_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_line_t *line = sgl_container_of(obj, sgl_line_t, obj);
    line->color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set line alpha
 * @param obj line object
 * @param alpha line alpha
 * @return none
 */
void sgl_line_set_alpha(sgl_obj_t *obj, uint8_t alpha)
{
    SGL_ASSERT(obj != NULL);
    sgl_line_t *line = sgl_container_of(obj, sgl_line_t, obj);
    line->alpha = alpha;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set line dashed or solid
 * @param obj line object
 * @param dashed 0: solid line, non-zero: dashed line
 * @return none
 */
void sgl_line_set_dashed(sgl_obj_t *obj, uint8_t dashed)
{
    SGL_ASSERT(obj != NULL);
    sgl_line_t *line = sgl_container_of(obj, sgl_line_t, obj);
    line->dashed = dashed;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set line dash pattern when dashed
 * @param obj line object
 * @param dash_len length of solid segment in pixels
 * @param gap_len length of space segment in pixels
 * @return none
 */
void sgl_line_set_dash_pattern(sgl_obj_t *obj, uint16_t dash_len, uint16_t gap_len)
{
    SGL_ASSERT(obj != NULL);
    sgl_line_t *line = sgl_container_of(obj, sgl_line_t, obj);
    line->dash_length = dash_len;
    line->gap_length = gap_len;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set line width
 * @param obj line object
 * @param width line width
 * @return none
 */
void sgl_line_set_width(sgl_obj_t *obj, uint8_t width)
{
	SGL_ASSERT(obj != NULL);
	obj->border = width;
	sgl_obj_set_dirty(obj);
}
