/* source/widget/qrcode/sgl_qrcode.c
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
#include <string.h>
#include "sgl_qrcode.h"

static void qrcode_get_pixmap_size(sgl_qrcode_t *qrcode, sgl_area_t *mod)
{
    uint32_t allow, mod_size;
    uint32_t total = sgl_pow2(qrcode->qrcode.size);

    switch(qrcode->qrcode.ecc) {
        case 0: allow = total * 7  / 100; break;
        case 1: allow = total * 15 / 100; break;
        case 2: allow = total * 25 / 100; break;
        case 3: allow = total * 30 / 100; break;
        default:allow = 0; break;
    }

    /* safe zone, not fill the ecc all area */
    allow = allow * 80 / 100;
    mod_size = sgl_sqrt(allow);
    mod->y1 = qrcode->scale * ((qrcode->qrcode.size - mod_size + 1) / 2 + qrcode->zone);
    mod->x1 = mod->y1 + 1;
    mod->x2 = mod->x1 + mod_size * qrcode->scale - 1;
    mod->y2 = mod->y1 + mod_size * qrcode->scale - 1;
}

static void sgl_qrcode_construct_cb(sgl_surf_t *surf, sgl_obj_t* obj, sgl_event_t *evt)
{
    sgl_qrcode_t *qrcode = sgl_container_of(obj, sgl_qrcode_t, obj);
    int16_t radius = sgl_min(obj->radius, qrcode->scale);
    sgl_draw_rect_t desc = {
        .alpha = qrcode->alpha,
        .color = qrcode->bg_color,
        .border = 0,
        .pixmap = NULL,
        .radius = radius,
    };

    sgl_area_t coords = {
        .x1 = obj->coords.x1,
        .y1 = obj->coords.y1,
    };

    if(evt->type == SGL_EVENT_DRAW_MAIN) {
        
        /* draw qrcode background */
        sgl_draw_rect(surf, &obj->area, &obj->coords, &desc);
        desc.color = qrcode->cell_color;
        desc.radius = sgl_min(qrcode->scale / 2, qrcode->cell_radius);

        for (uint8_t y = 0; y < qrcode->qrcode.size; y ++) {
            coords.x1 = obj->coords.x1 + qrcode->zone;
            coords.y1 += qrcode->scale;
            coords.y2 = coords.y1 + qrcode->scale - 1;

            for (uint8_t x = 0; x < qrcode->qrcode.size; x ++) {
                coords.x1 += qrcode->scale;
                coords.x2 = coords.x1 + qrcode->scale - 1;
                if (qrcode_getModule(&qrcode->qrcode, x, y)) {
                    sgl_draw_fill_rect(surf, &obj->area, &coords, desc.radius, desc.color, desc.alpha);
                }
            }
        }

        if (qrcode->pixmap) {
            qrcode_get_pixmap_size(qrcode, &coords);
            coords.x1 += obj->coords.x1;
            coords.y1 += obj->coords.y1;
            coords.x2 += obj->coords.x1;
            coords.y2 += obj->coords.y1;
            sgl_draw_fill_rect_pixmap(surf, &obj->area, &coords, radius, qrcode->pixmap, qrcode->alpha);
        }
    }
    else if (evt->type == SGL_EVENT_DESTROYED) {
        sgl_free(qrcode->data);
    }
}

/**
 * @brief create a qrcode object
 * @param parent parent of the qrcode
 * @return qrcode object
 */
sgl_obj_t* sgl_qrcode_create(sgl_obj_t* parent)
{
    sgl_qrcode_t *qrcode = sgl_malloc(sizeof(sgl_qrcode_t));
    if(qrcode == NULL) {
        SGL_LOG_ERROR("sgl_qrcode_create: malloc failed");
        return NULL;
    }

    /* set object all member to zero */
    memset(qrcode, 0, sizeof(sgl_qrcode_t));

    sgl_obj_t *obj = &qrcode->obj;
    sgl_obj_init(&qrcode->obj, parent);
    sgl_obj_set_radius(obj, 0);
    sgl_obj_set_border_width(obj, 0);
    obj->construct_fn = sgl_qrcode_construct_cb;

    qrcode->alpha = SGL_ALPHA_MAX;
    qrcode->bg_color = SGL_COLOR_WHITE;
    qrcode->cell_color = SGL_COLOR_BLACK;
    qrcode->zone = SGL_QRCODE_ZONE_DEFAULT;
    qrcode->scale = SGL_QRCODE_SCALE_DEFAULT;
    qrcode->qrcode.version = CONFIG_SGL_QRCODE_QR_VERSION_MAX;
    qrcode->qrcode.ecc = 0;

    qrcode->data = sgl_malloc(qrcode_getBufferSize(qrcode->qrcode.version));
    if (qrcode->data == NULL) {
        SGL_LOG_ERROR("sgl_qrcode_set_version: malloc failed");
        return NULL;
    }

    return obj;
}

/**
 * @brief set qrcode version
 * @param obj qrcode object
 * @param version qrcode version
 * @return none
 */
void sgl_qrcode_set_version(sgl_obj_t *obj, uint8_t version)
{
    sgl_qrcode_t *qrcode = sgl_container_of(obj, sgl_qrcode_t, obj);
    if (version == qrcode->qrcode.version) {
        return;
    }

    sgl_free(qrcode->data);
    qrcode->qrcode.version = version;
    qrcode->data = sgl_malloc(qrcode_getBufferSize(version));
    if (qrcode->data == NULL) {
        SGL_LOG_ERROR("sgl_qrcode_set_version: malloc failed");
        return;
    }
}

/**
 * @brief set qrcode ecc level
 * @param obj qrcode object
 * @param ecc qrcode ecc level
 * @return none
 * @note the ecc level must be in range [0, 3]
 */
void sgl_qrcode_set_ecc(sgl_obj_t *obj, uint8_t ecc)
{
    sgl_qrcode_t *qrcode = sgl_container_of(obj, sgl_qrcode_t, obj);
    qrcode->qrcode.ecc = ecc;
}

/**
 * @brief set qrcode text
 * @param obj qrcode object
 * @param text qrcode text
 * @return none
 */
void sgl_qrcode_set_text(sgl_obj_t *obj, const char *text)
{
    sgl_qrcode_t *qrcode = sgl_container_of(obj, sgl_qrcode_t, obj);
    qrcode_initText(&qrcode->qrcode, qrcode->data, qrcode->qrcode.version, qrcode->qrcode.ecc, text);
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set qrcode alpha
 * @param obj qrcode object
 * @param alpha qrcode alpha
 * @return none
 */
void sgl_qrcode_set_alpha(sgl_obj_t *obj, uint8_t alpha)
{
    sgl_qrcode_t *qrcode = sgl_container_of(obj, sgl_qrcode_t, obj);
    qrcode->alpha = alpha;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set qrcode background color
 * @param obj qrcode object
 * @param color qrcode background color
 * @return none
 */
void sgl_qrcode_set_bg_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_qrcode_t *qrcode = sgl_container_of(obj, sgl_qrcode_t, obj);
    qrcode->bg_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set qrcode cell color
 * @param obj qrcode object
 * @param color qrcode cell color
 * @return none
 */
void sgl_qrcode_set_cell_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_qrcode_t *qrcode = sgl_container_of(obj, sgl_qrcode_t, obj);
    qrcode->cell_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set qrcode cell radius
 * @param obj qrcode object
 * @param radius qrcode cell radius
 * @return none
 */
void sgl_qrcode_set_cell_radius(sgl_obj_t *obj, uint8_t radius)
{
    sgl_qrcode_t *qrcode = sgl_container_of(obj, sgl_qrcode_t, obj);
    qrcode->cell_radius = radius;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set qrcode zone
 * @param obj qrcode object
 * @param zone qrcode zone
 * @return none
 */
void sgl_qrcode_set_zone(sgl_obj_t *obj, uint8_t zone)
{
    sgl_qrcode_t *qrcode = sgl_container_of(obj, sgl_qrcode_t, obj);
    qrcode->zone = zone;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set qrcode scale
 * @param obj qrcode object
 * @param scale qrcode scale
 * @return none
 */
void sgl_qrcode_set_scale(sgl_obj_t *obj, uint8_t scale)
{
    sgl_qrcode_t *qrcode = sgl_container_of(obj, sgl_qrcode_t, obj);
    qrcode->scale = scale;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set qrcode logo
 * @param obj qrcode object
 * @param pixmap qrcode logo
 * @return none
 */
void sgl_qrcode_set_logo(sgl_obj_t *obj, const sgl_pixmap_t *pixmap)
{
    sgl_qrcode_t *qrcode = sgl_container_of(obj, sgl_qrcode_t, obj);
    qrcode->pixmap = pixmap;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set qrcode logo radius
 * @param obj qrcode object
 * @param radius qrcode logo radius
 * @return none
 */
void sgl_qrcode_set_logo_radius(sgl_obj_t *obj, int16_t radius)
{
    sgl_obj_set_radius(obj, radius);
    sgl_obj_set_dirty(obj);
}
