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

/* Upper bound for the segment-cell buffer (cells is clamped to this) */
#define SGL_BATTERY_MAX_CELLS 32

/**
 * @brief  get battery level color
 * @param  b: battery object pointer
 * @return color based on current level (low/medium/high)
 * @note   thresholds follow the reference style: <=20 low, <=50 medium, else high
 */
static sgl_color_t sgl_battery_level_color(const sgl_battery_t *b)
{
    if (b->level <= 20) return b->low_color;
    if (b->level <= 50) return b->medium_color;
    return b->high_color;
}

/**
 * @brief  battery construct callback
 * @param  surf: surface pointer
 * @param  obj: object pointer
 * @param  evt: event pointer
 * @return none
 * @note   draws a flat, segment-cell battery: a thin rounded shell outline,
 *         a small positive-terminal cap, evenly spaced cells (lit cells colored
 *         by level, unlit cells faint) and an optional percentage label below.
 */
static void sgl_battery_construct_cb(sgl_surf_t *surf, sgl_obj_t *obj, sgl_event_t *evt)
{
    if (evt->type != SGL_EVENT_DRAW_MAIN) return;

    sgl_battery_t *b = sgl_container_of(obj, sgl_battery_t, obj);
    const uint8_t alpha = b->alpha;
    const uint8_t cells = sgl_min(sgl_max(b->cells, 1), SGL_BATTERY_MAX_CELLS);
    const bool    vert  = b->vertical;

    /* Reserve a bottom band for the percentage label so the battery sits above it */
    const sgl_font_t *f = b->font;
    if (f == NULL) f = sgl_get_system_font();
    int16_t text_h = 0;
    if (b->show_percentage && f != NULL) text_h = sgl_font_get_height(f) + 2;

    const int16_t bx1 = obj->coords.x1;
    const int16_t by1 = obj->coords.y1;
    const int16_t bx2 = obj->coords.x2;
    const int16_t by2 = obj->coords.y2 - text_h;
    const int16_t bw  = bx2 - bx1;
    const int16_t bh  = by2 - by1;
    if (bw <= 0 || bh <= 0) return;

    /* Cap protrudes on the run-axis end (right for horizontal, top for vertical) */
    const int16_t cap_len   = vert ? sgl_max(bh / 28, 2) : sgl_max(bw / 28, 2);
    const int16_t cap_cross = vert ? sgl_max(bw / 3, 4)  : sgl_max(bh / 3, 4);

    /* Shell rect (stroke only, transparent interior), leaving room for the cap */
    sgl_area_t shell;
    if (vert) shell = (sgl_area_t){ bx1, by1 + cap_len, bx2, by2 };
    else      shell = (sgl_area_t){ bx1, by1, bx2 - cap_len, by2 };

    const int16_t sw = shell.x2 - shell.x1;
    const int16_t sh = shell.y2 - shell.y1;
    const int16_t shell_r = sgl_max(sgl_min(sw, sh) / 6, 2);

    /* 1) rounded shell outline only (no interior fill) */
    sgl_draw_fill_rect_border(surf, &obj->area, &shell, shell_r,
                              b->border_color, 2, alpha);

    /* 2) positive terminal cap: a solid rectangle (no border) in the shell's
        border color, flush against the body so they read as one connected shape */
    if (vert) {
        const int16_t cx = shell.x1 + (sw - cap_cross) / 2;
        sgl_area_t cap = { cx, by1, cx + cap_cross, shell.y1 };
        sgl_draw_fill_rich_rect(surf, &obj->area, &cap, 2, 2, 0, 0, b->border_color, alpha);
    } else {
        const int16_t cy = shell.y1 + (sh - cap_cross) / 2;
        sgl_area_t cap = { shell.x2, cy, bx2, cy + cap_cross };
        sgl_draw_fill_rich_rect(surf, &obj->area, &cap, 0, 2, 0, 2, b->border_color, alpha);
    }

    /* 3) segment cells inside the shell (inset by padding) */
    const int16_t pad  = sgl_max(sgl_min(sw, sh) / 8, 2);
    const int16_t ix1  = shell.x1 + pad, iy1 = shell.y1 + pad;
    const int16_t ix2  = shell.x2 - pad, iy2 = shell.y2 - pad;
    const int16_t inner_run   = vert ? (iy2 - iy1) : (ix2 - ix1);
    const int16_t inner_cross = vert ? (ix2 - ix1) : (iy2 - iy1);

    if (inner_run > 0 && inner_cross > 0) {
        /* Pick a gap, then split the run length evenly so every cell keeps an
         * (almost) identical size: sgl_split_len_avg distributes the remainder
         * via Bresenham so adjacent cells differ by at most 1px and the cells
         * exactly fill inner_run. */
        const int16_t gap = sgl_max(inner_run / (cells * 6), 1);
        int16_t sizes[SGL_BATTERY_MAX_CELLS];
        sgl_split_len_avg(inner_run, cells, gap, sizes);

        const int16_t r = sgl_max(sgl_min(sizes[0], inner_cross) / 5, 1);

        const uint8_t lit = (uint8_t)(((uint16_t)b->level * cells + 50) / 100);
        const sgl_color_t fc = b->fill_color.full ? b->fill_color
                                                   : sgl_battery_level_color(b);
        const uint8_t unlit_alpha = (uint8_t)(alpha * 3 / 10);

        int16_t pos = 0;
        for (uint8_t i = 0; i < cells; i++) {
            sgl_area_t c;
            if (vert) {
                /* fill grows upward from the bottom */
                const int16_t y2i = iy2 - pos;
                c = (sgl_area_t){ ix1, y2i - sizes[i], ix2, y2i };
            } else {
                /* fill grows rightward from the left */
                const int16_t x1i = ix1 + pos;
                c = (sgl_area_t){ x1i, iy1, x1i + sizes[i], iy2 };
            }
            pos += sizes[i] + gap;
            if (c.x1 >= c.x2 || c.y1 >= c.y2) continue;
            if (i < lit)
                sgl_draw_fill_rect(surf, &obj->area, &c, r, fc, alpha);
            else
                sgl_draw_fill_rect(surf, &obj->area, &c, r, SGL_COLOR_WHITE, unlit_alpha);
        }
    }

    /* 4) percentage label centered below the battery */
    if (b->show_percentage && f != NULL) {
        char txt[8];
        sgl_sprintf(txt, "%d%%", b->level);
        int16_t tw = sgl_font_get_string_width(txt, f);
        int16_t tx = bx1 + (bw - tw) / 2;
        int16_t ty = by2 + 2;
        sgl_area_t ta = { bx1, by2, bx2, obj->coords.y2 };
        sgl_draw_string(surf, &ta, tx, ty, txt, b->text_color, SGL_ALPHA_MAX, f);
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
    b->alpha          = 200;
    b->cells          = 10;
    b->border_color   = SGL_COLOR_GRAY;
    b->fill_color     = (sgl_color_t){0};           /* auto by level */
    b->low_color      = sgl_rgb(239, 68, 68);      /* #ef4444 */
    b->medium_color   = sgl_rgb(245, 158, 11);     /* #f59e0b */
    b->high_color     = sgl_rgb(34, 197, 94);      /* #22c55e */
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

/**
 * @brief set the number of segment cells
 * @param  obj: battery object pointer
 * @param  cells: number of cells (>= 1)
 * @return none
 */
void sgl_battery_set_cells(sgl_obj_t *obj, uint8_t cells)
{
    sgl_battery_t *b = sgl_container_of(obj, sgl_battery_t, obj);
    b->cells = cells < 1 ? 1 : cells;
    sgl_obj_set_dirty(obj);
}
