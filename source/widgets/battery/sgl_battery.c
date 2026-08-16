/* source/widgets/battery/sgl_battery.c
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
#include "sgl_battery.h"

/**
 * @brief  get battery level color
 * @param  b: battery object pointer
 * @return color based on current level (low/medium/high)
 */
static sgl_color_t sgl_battery_level_color(const sgl_battery_t *b)
{
    if (b->level < 20)  return b->low_color;
    if (b->level < 50)  return b->medium_color;
    return b->high_color;
}

/**
 * @brief  battery construct callback
 * @param  surf: surface pointer
 * @param  obj: object pointer
 * @param  evt: event pointer
 * @return none
 * @note   this function is called when the object is created or redraw
 */
static void sgl_battery_construct_cb(sgl_surf_t *surf, sgl_obj_t *obj, sgl_event_t *evt)
{
    if (evt->type != SGL_EVENT_DRAW_MAIN) return;

    sgl_battery_t *b = sgl_container_of(obj, sgl_battery_t, obj);
    const uint8_t  alpha  = b->alpha;
    const uint8_t  border = obj->border;
    const int16_t  r      = sgl_max(obj->radius, 1);

    const int16_t body_x1 = obj->coords.x1 + border;
    const int16_t body_y1 = obj->coords.y1 + border;
    const int16_t body_x2 = obj->coords.x2 - border;
    const int16_t body_y2 = obj->coords.y2 - border;
    const int16_t body_w  = body_x2 - body_x1;
    const int16_t body_h  = body_y2 - body_y1;
    const bool    vert    = b->vertical;

    /* Cap protrudes outside the body: reserve space on cap side */
    const int16_t cap_len = vert ? sgl_max(body_h / 12, 3) : sgl_max(body_w / 12, 3);
    const int16_t cap_thick_w = vert ? body_w / 2 : sgl_max(body_h / 3, 4);
    const int16_t cap_thick_h = vert ? sgl_max(body_w / 3, 4) : body_h / 3;

    /* body rect: shrink on cap side to make room for protruding cap */
    sgl_area_t body;
    if (vert) {
        body = (sgl_area_t){ body_x1, body_y1 + cap_len, body_x2, body_y2 };
    } else {
        body = (sgl_area_t){ body_x1, body_y1, body_x2 - cap_len, body_y2 };
    }
    sgl_draw_fill_rect_border(surf, &obj->area, &body, r,
                              b->border_color, 2, alpha);
    sgl_draw_fill_rect(surf, &obj->area, &body, r, b->bg_color, alpha);

    /* Recalculate body extents from the actual body rect */
    const int16_t bx1 = body.x1, by1 = body.y1;
    const int16_t bx2 = body.x2, by2 = body.y2;
    const int16_t bw  = bx2 - bx1;
    const int16_t bh  = by2 - by1;

    /* 3-D body edge: highlight and shadow adapt to orientation */
    if (vert) {
        /* vertical: highlight left, shadow right */
        sgl_area_t hl = { bx1, by1, bx1 + sgl_max(bw / 8, 1), by2 };
        sgl_draw_fill_rect(surf, &obj->area, &hl, r,
                           SGL_COLOR_WHITE, (uint8_t)(alpha * 80 / 255));
        sgl_area_t sh = { bx2 - sgl_max(bw / 10, 1), by1, bx2, by2 };
        sgl_draw_fill_rect(surf, &obj->area, &sh, 0,
                           SGL_COLOR_BLACK, (uint8_t)(alpha * 90 / 255));
    } else {
        /* horizontal: highlight top, shadow bottom */
        sgl_area_t hl = { bx1, by1, bx2, by1 + sgl_max(bh / 8, 1) };
        sgl_draw_fill_rect(surf, &obj->area, &hl, r,
                           SGL_COLOR_WHITE, (uint8_t)(alpha * 80 / 255));
        sgl_area_t sh = { bx1, by2 - sgl_max(bh / 10, 1), bx2, by2 };
        sgl_draw_fill_rect(surf, &obj->area, &sh, 0,
                           SGL_COLOR_BLACK, (uint8_t)(alpha * 90 / 255));
    }

    /* fill area: inset by pad */
    const int16_t pad = 3;
    int16_t fill_x1 = bx1 + pad;
    int16_t fill_y1 = by1 + pad;
    int16_t fill_x2 = bx2 - pad;
    int16_t fill_y2 = by2 - pad;
    const int16_t fw = fill_x2 - fill_x1;
    const int16_t fh = fill_y2 - fill_y1;

    if (fw > 0 && fh > 0) {
        /* Dark inner base for depth */
        sgl_area_t fill_bg = { fill_x1, fill_y1, fill_x2, fill_y2 };
        sgl_draw_fill_rect(surf, &obj->area, &fill_bg, r - 1,
                           sgl_color_mixer(b->bg_color, SGL_COLOR_BLACK, 40), alpha);

        /* Colored fill */
        sgl_color_t fc = b->fill_color.full ? b->fill_color
                                             : sgl_battery_level_color(b);
        if (vert) {
            /* vertical: fill grows upward from bottom */
            const int16_t level_h = (int16_t)((int32_t)fh * sgl_min(b->level, 100) / 100);
            if (level_h > 0) {
                sgl_area_t fill = { fill_x1, fill_y2 - level_h, fill_x2, fill_y2 };
                sgl_draw_fill_rect(surf, &obj->area, &fill, r - 1, fc, alpha);
                /* 3-D highlight: left strip */
                sgl_area_t hl = { fill_x1, fill_y2 - level_h,
                                  fill_x1 + sgl_max(fw / 5, 2), fill_y2 };
                sgl_draw_fill_rect(surf, &obj->area, &hl, r - 1,
                                   SGL_COLOR_WHITE, (uint8_t)(alpha * 90 / 255));
                /* 3-D shadow: right strip */
                sgl_area_t sh = { fill_x2 - sgl_max(fw / 6, 1), fill_y2 - level_h,
                                  fill_x2, fill_y2 };
                sgl_draw_fill_rect(surf, &obj->area, &sh, 0,
                                   SGL_COLOR_BLACK, (uint8_t)(alpha * 100 / 255));
            }
        } else {
            /* horizontal: fill grows rightward from left */
            const int16_t level_w = (int16_t)((int32_t)fw * sgl_min(b->level, 100) / 100);
            if (level_w > 0) {
                sgl_area_t fill = { fill_x1, fill_y1, fill_x1 + level_w, fill_y2 };
                sgl_draw_fill_rect(surf, &obj->area, &fill, r - 1, fc, alpha);
                /* 3-D highlight: top strip */
                sgl_area_t hl = { fill_x1, fill_y1, fill_x1 + level_w,
                                  fill_y1 + sgl_max(fh / 5, 2) };
                sgl_draw_fill_rect(surf, &obj->area, &hl, r - 1,
                                   SGL_COLOR_WHITE, (uint8_t)(alpha * 90 / 255));
                /* 3-D shadow: bottom strip */
                sgl_area_t sh = { fill_x1, fill_y2 - sgl_max(fh / 6, 1),
                                  fill_x1 + level_w, fill_y2 };
                sgl_draw_fill_rect(surf, &obj->area, &sh, 0,
                                   SGL_COLOR_BLACK, (uint8_t)(alpha * 100 / 255));
            }
        }
    }

    /* Cap (positive terminal) — protrudes outside the body */
    if (vert) {
        /* vertical: cap sticks up above the body top */
        const int16_t cx = bx1 + (bw - cap_thick_w) / 2;
        sgl_area_t cap_bg = { cx - 1, by1 - cap_len, cx + cap_thick_w + 1, by1 };
        sgl_draw_fill_rect(surf, &obj->area, &cap_bg, 0, b->bg_color, alpha);
        sgl_area_t cap = { cx, by1 - cap_len, cx + cap_thick_w, by1 };
        sgl_draw_fill_rect(surf, &obj->area, &cap, 2,
                           sgl_color_mixer(b->bg_color, b->border_color, 180), alpha);
        /* 3-D cap: left highlight */
        sgl_area_t cap_hl = { cx, by1 - cap_len,
                              cx + sgl_max(cap_thick_w / 4, 1), by1 };
        sgl_draw_fill_rect(surf, &obj->area, &cap_hl, 2,
                           SGL_COLOR_WHITE, (uint8_t)(alpha * 70 / 255));
        /* 3-D cap: right shadow */
        sgl_area_t cap_sh = { cx + cap_thick_w - sgl_max(cap_thick_w / 5, 1), by1 - cap_len,
                              cx + cap_thick_w, by1 };
        sgl_draw_fill_rect(surf, &obj->area, &cap_sh, 0,
                           SGL_COLOR_BLACK, (uint8_t)(alpha * 60 / 255));
    } else {
        /* horizontal: cap sticks out to the right of the body */
        const int16_t cy = by1 + (bh - cap_thick_h) / 2;
        sgl_area_t cap_bg = { bx2, cy - 1, bx2 + cap_len, cy + cap_thick_h + 1 };
        sgl_draw_fill_rect(surf, &obj->area, &cap_bg, 0, b->bg_color, alpha);
        sgl_area_t cap = { bx2, cy, bx2 + cap_len, cy + cap_thick_h };
        sgl_draw_fill_rect(surf, &obj->area, &cap, 2,
                           sgl_color_mixer(b->bg_color, b->border_color, 180), alpha);
        /* 3-D cap: top highlight */
        sgl_area_t cap_hl = { bx2, cy, bx2 + cap_len,
                              cy + sgl_max(cap_thick_h / 4, 1) };
        sgl_draw_fill_rect(surf, &obj->area, &cap_hl, 2,
                           SGL_COLOR_WHITE, (uint8_t)(alpha * 70 / 255));
        /* 3-D cap: bottom shadow */
        sgl_area_t cap_sh = { bx2, cy + cap_thick_h - sgl_max(cap_thick_h / 5, 1),
                              bx2 + cap_len, cy + cap_thick_h };
        sgl_draw_fill_rect(surf, &obj->area, &cap_sh, 0,
                           SGL_COLOR_BLACK, (uint8_t)(alpha * 60 / 255));
    }

    /* Charging lightning bolt */
    if (b->charging) {
        sgl_draw_line_t ln = {
            .alpha = SGL_ALPHA_MAX,
            .width = 2,
            .color = b->charging_color,
        };
        int16_t cx = (bx1 + bx2) / 2;
        int16_t cy = (by1 + by2) / 2;
        int16_t hw = bw / 6;
        int16_t hh = bh / 3;
        ln.x1 = cx + hw / 2;  ln.y1 = cy - hh;
        ln.x2 = cx - hw / 4;  ln.y2 = cy - hh / 6;
        sgl_draw_line(surf, &obj->area, &ln);
        ln.x1 = ln.x2;  ln.y1 = ln.y2;
        ln.x2 = cx + hw / 4;  ln.y2 = cy + hh / 6;
        sgl_draw_line(surf, &obj->area, &ln);
        ln.x1 = ln.x2;  ln.y1 = ln.y2;
        ln.x2 = cx - hw / 2;  ln.y2 = cy + hh;
        sgl_draw_line(surf, &obj->area, &ln);
    }

    /* Percentage text */
    if (b->show_percentage) {
        char txt[8];
        sgl_sprintf(txt, "%d%%", b->level);
        const sgl_font_t *f = b->font;
        if (f == NULL) f = sgl_get_system_font();
        if (f != NULL) {
            int16_t tw = sgl_font_get_string_width(txt, f);
            int16_t th = sgl_font_get_height(f);
            int16_t tx = bx1 + (bw - tw) / 2;
            int16_t ty = by1 + (bh - th) / 2;
            sgl_area_t ta = { bx1, by1, bx2, by2 };
            sgl_draw_string(surf, &ta, tx, ty, txt, b->text_color, SGL_ALPHA_MAX, f);
        }
    }
}

/**
 * @brief  create a battery object
 * @param  parent: parent object pointer
 * @return battery object pointer, NULL on failure
 */
sgl_obj_t* sgl_battery_create(sgl_obj_t *parent)
{
    sgl_battery_t *b = sgl_malloc(sizeof(sgl_battery_t));
    if (!b) {
        SGL_LOG_ERROR("sgl_battery_create: malloc failed");
        return NULL;
    }
    memset(b, 0, sizeof(*b));

    sgl_obj_init(&b->obj, parent);
    b->obj.construct_fn = sgl_battery_construct_cb;
    b->obj.border       = 1;
    b->obj.radius       = 4;

    b->level          = 100;
    b->alpha          = 140;
    b->border_color   = sgl_rgb(180, 180, 180);
    b->fill_color     = (sgl_color_t){0};  /* auto */
    b->low_color      = sgl_rgb(231, 76, 60);
    b->medium_color   = sgl_rgb(243, 156, 18);
    b->high_color     = sgl_rgb(46, 204, 113);
    b->bg_color       = sgl_rgb(30, 30, 30);
    b->charging       = 0;
    b->charging_color = sgl_rgb(241, 196, 15);
    b->show_percentage = 0;
    b->font           = NULL;
    b->text_color     = sgl_rgb(255, 255, 255);

    return &b->obj;
}

/**
 * @brief set battery level
 * @param  obj: battery object pointer
 * @param  level: battery level (0-100)
 * @return none
 */
void sgl_battery_set_level(sgl_obj_t *obj, uint8_t level)
{
    sgl_battery_t *b = sgl_container_of(obj, sgl_battery_t, obj);
    b->level = level > 100 ? 100 : level;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set battery border color
 * @param  obj: battery object pointer
 * @param  color: border color to set
 * @return none
 */
void sgl_battery_set_border_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_battery_t *b = sgl_container_of(obj, sgl_battery_t, obj);
    b->border_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set battery fill color
 * @param  obj: battery object pointer
 * @param  color: fill color to set
 * @return none
 */
void sgl_battery_set_fill_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_battery_t *b = sgl_container_of(obj, sgl_battery_t, obj);
    b->fill_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set low battery color (below 20%%)
 * @param  obj: battery object pointer
 * @param  color: low battery color to set
 * @return none
 */
void sgl_battery_set_low_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_battery_t *b = sgl_container_of(obj, sgl_battery_t, obj);
    b->low_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set medium battery color (20-50%%)
 * @param  obj: battery object pointer
 * @param  color: medium battery color to set
 * @return none
 */
void sgl_battery_set_medium_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_battery_t *b = sgl_container_of(obj, sgl_battery_t, obj);
    b->medium_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set high battery color (above 50%%)
 * @param  obj: battery object pointer
 * @param  color: high battery color to set
 * @return none
 */
void sgl_battery_set_high_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_battery_t *b = sgl_container_of(obj, sgl_battery_t, obj);
    b->high_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set battery background color
 * @param  obj: battery object pointer
 * @param  color: background color to set
 * @return none
 */
void sgl_battery_set_bg_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_battery_t *b = sgl_container_of(obj, sgl_battery_t, obj);
    b->bg_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set charging status
 * @param  obj: battery object pointer
 * @param  charging: charging status (1 = charging, 0 = not charging)
 * @return none
 */
void sgl_battery_set_charging(sgl_obj_t *obj, bool charging)
{
    sgl_battery_t *b = sgl_container_of(obj, sgl_battery_t, obj);
    b->charging = charging;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set charging indicator color
 * @param  obj: battery object pointer
 * @param  color: charging indicator color to set
 * @return none
 */
void sgl_battery_set_charging_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_battery_t *b = sgl_container_of(obj, sgl_battery_t, obj);
    b->charging_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief show or hide percentage text
 * @param  obj: battery object pointer
 * @param  show: show percentage text (1 = show, 0 = hide)
 * @return none
 */
void sgl_battery_show_percentage(sgl_obj_t *obj, bool show)
{
    sgl_battery_t *b = sgl_container_of(obj, sgl_battery_t, obj);
    b->show_percentage = show;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set text font for percentage display
 * @param  obj: battery object pointer
 * @param  font: font pointer to set
 * @return none
 */
void sgl_battery_set_font(sgl_obj_t *obj, const sgl_font_t *font)
{
    sgl_battery_t *b = sgl_container_of(obj, sgl_battery_t, obj);
    b->font = font;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set percentage text color
 * @param  obj: battery object pointer
 * @param  color: text color to set
 * @return none
 */
void sgl_battery_set_text_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_battery_t *b = sgl_container_of(obj, sgl_battery_t, obj);
    b->text_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set battery transparency
 * @param  obj: battery object pointer
 * @param  alpha: alpha value (0 = transparent, 255 = opaque)
 * @return none
 */
void sgl_battery_set_alpha(sgl_obj_t *obj, uint8_t alpha)
{
    sgl_battery_t *b = sgl_container_of(obj, sgl_battery_t, obj);
    b->alpha = alpha;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set battery orientation
 * @param  obj: battery object pointer
 * @param  vertical: true for vertical, false for horizontal
 * @return none
 */
void sgl_battery_set_vertical(sgl_obj_t *obj, bool vertical)
{
    sgl_battery_t *b = sgl_container_of(obj, sgl_battery_t, obj);
    b->vertical = vertical;
    sgl_obj_set_dirty(obj);
}
