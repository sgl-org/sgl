/* source/widgets/sgl_ring.c
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
#include "sgl_ring.h"


static void sgl_ring_construct_cb(sgl_surf_t *surf, sgl_obj_t* obj, sgl_event_t *evt)
{
    sgl_ring_t *ring = sgl_container_of(obj, sgl_ring_t, obj);
    int16_t cx, cy, radius_in, radius_out;

    if(evt->type == SGL_EVENT_DRAW_MAIN) {
        cx = (obj->coords.x2 + obj->coords.x1) / 2;
        cy = (obj->coords.y2 + obj->coords.y1) / 2;
        radius_out = sgl_obj_get_width(obj) / 2;
        radius_in = radius_out - ring->width;

        sgl_draw_fill_ring(surf, &obj->area, cx, cy, radius_in, radius_out, ring->color, ring->alpha);
    }
}

/**
 * @brief Create a ring object
 * @param parent The parent object of the ring
 * @return The ring object
 */
sgl_obj_t* sgl_ring_create(sgl_obj_t* parent)
{
    sgl_ring_t *ring = sgl_malloc(sizeof(sgl_ring_t));
    if(ring == NULL) {
        SGL_LOG_ERROR("sgl_ring_create: malloc failed");
        return NULL;
    }

    /* set object all member to zero */
    memset(ring, 0, sizeof(sgl_ring_t));

    sgl_obj_t *obj = &ring->obj;
    sgl_obj_init(&ring->obj, parent);
    obj->construct_fn = sgl_ring_construct_cb;

    ring->alpha = SGL_THEME_ALPHA;
    ring->color = SGL_THEME_COLOR;
    ring->width = 2;

    return obj;
}

/**
 * @brief Set the ring color
 * @param obj The ring object
 * @param color The ring color
 * @return none
 */
void sgl_ring_set_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_ring_t *ring = sgl_container_of(obj, sgl_ring_t, obj);
    ring->color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set the ring alpha
 * @param obj The ring object
 * @param alpha The ring alpha
 * @return none
 * @note The alpha value range is 0~255
 */
void sgl_ring_set_alpha(sgl_obj_t *obj, uint8_t alpha)
{
    sgl_ring_t *ring = sgl_container_of(obj, sgl_ring_t, obj);
    ring->alpha = alpha;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set the ring radius
 * @param obj The ring object
 * @param radius_in The ring inner radius
 * @param radius_out The ring outer radius
 * @return none
 */
void sgl_ring_set_radius(sgl_obj_t *obj, uint16_t radius_in, uint16_t radius_out)
{
    sgl_ring_t *ring = sgl_container_of(obj, sgl_ring_t, obj);
    sgl_obj_circle_zoom(obj, radius_out);
    ring->width = radius_out - radius_in;
    sgl_obj_set_dirty(obj);
}
