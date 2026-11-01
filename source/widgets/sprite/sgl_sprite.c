/* source/widgets/sgl_sprite.c
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
#include <sgl_cfgfix.h>
#include <string.h>
#include "sgl_sprite.h"

static void sgl_sprite_construct_cb(sgl_surf_t *surf, sgl_obj_t *obj, sgl_event_t *evt)
{
    if (evt->type != SGL_EVENT_DRAW_MAIN) {
        return;
    }

    sgl_sprite_t *sprite = sgl_container_of(obj, sgl_sprite_t, obj);
    sgl_area_t clip;

    if (!sgl_surf_clip(surf, &obj->area, &clip)) return;

    const uint16_t src_w = sprite->pixmap->width;
    const uint16_t dst_stride = surf->w;
    const uint8_t global_alpha = sprite->alpha;

    const uint16_t *src_line = (const uint16_t *)sprite->pixmap->bitmap.array 
                             + (clip.y1 - obj->coords.y1) * src_w 
                             + (clip.x1 - obj->coords.x1);
    sgl_color_t *dst_line = sgl_surf_get_buf(surf, clip.x1 - surf->x1, clip.y1 - surf->y1);

    const int clip_w = clip.x2 - clip.x1 + 1;
    const int clip_h = clip.y2 - clip.y1 + 1;

    if (global_alpha == SGL_ALPHA_MAX) {
        for (int y = 0; y < clip_h; y++) {
            const uint16_t *src = src_line;
            sgl_color_t *dst = dst_line;

            for (int x = 0; x < clip_w; x++) {
                uint16_t c = *src++;
                uint8_t tex_a = c >> 12;
                uint8_t eff_alpha = (uint16_t)tex_a * 17;

                if (eff_alpha) {
                    sgl_color_t sc = sgl_rgb444_to_color(c);
                    *dst = sgl_color_mixer(sc, *dst, eff_alpha);
                }
                dst++;
            }
            src_line += src_w;
            dst_line += dst_stride;
        }
    }
    else {
        for (int y = 0; y < clip_h; y++) {
            const uint16_t *src = src_line;
            sgl_color_t *dst = dst_line;

            for (int x = 0; x < clip_w; x++) {
                uint16_t c = *src++;
                uint8_t tex_a = c >> 12;
                uint8_t eff_alpha = ((uint16_t)tex_a * global_alpha * 17) >> 8;

                if (eff_alpha) {
                    sgl_color_t sc = sgl_rgb444_to_color(c);
                    *dst = sgl_color_mixer(sc, *dst, eff_alpha);
                }
                dst++;
            }
            src_line += src_w;
            dst_line += dst_stride;
        }
    }
}

/**
 * @brief create a sprite object
 * @param parent parent of the sprite
 * @return sprite object
 */
sgl_obj_t* sgl_sprite_create(sgl_obj_t* parent)
{
    sgl_sprite_t *sprite = sgl_malloc(sizeof(sgl_sprite_t));
    if(sprite == NULL) {
        SGL_LOG_ERROR("sgl_sprite_create: malloc failed");
        return NULL;
    }

    /* set object all member to zero */
    memset(sprite, 0, sizeof(sgl_sprite_t));

    sgl_obj_t *obj = &sprite->obj;
    sgl_obj_init(&sprite->obj, parent);
    obj->construct_fn = sgl_sprite_construct_cb;
    sprite->alpha = SGL_ALPHA_MAX;
    return obj;
}

/**
 * @brief set the alpha of the sprite
 * @param obj sprite object
 * @param alpha alpha of the sprite
 * @return none
 */
void sgl_sprite_set_alpha(sgl_obj_t *obj, uint8_t alpha)
{
    sgl_sprite_t *sprite = sgl_container_of(obj, sgl_sprite_t, obj);
    sprite->alpha = alpha;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the pixmap of the sprite
 * @param obj sprite object
 * @param pixmap pixmap of the sprite
 * @return none
 */
void sgl_sprite_set_pixmap(sgl_obj_t *obj, const sgl_pixmap_t *pixmap)
{
    sgl_sprite_t *sprite = sgl_container_of(obj, sgl_sprite_t, obj);
    sprite->pixmap = pixmap;
    sgl_obj_set_dirty(obj);
}
