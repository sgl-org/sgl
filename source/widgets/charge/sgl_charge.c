/* source/widgets/charge/sgl_charge.c
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
#include <sgl_anim.h>
#include <sgl_log.h>
#include <sgl_mm.h>
#include <sgl_cfgfix.h>
#include "sgl_charge.h"

/* one wave cycle in ms, the animation sweeps 0..360 degrees over that period */
#define CHARGE_ANIM_MS         (3000)

/* sine ripple, degrees travelled across the whole cavity. The three ratios
 * below match charge.html. NOTE: the phase must be scaled by 1/256 as well,
 * @cfx arrives in 1/256 px, leaving that factor out is pure aliasing */
#define CHARGE_WAVE_DEG        (465)

/* wave height, 1/36 of the cavity width */
#define CHARGE_WAVE_AMP_DIV    (36)

/* parabolic dip that follows the cylinder wall, 1/20 of the cavity width */
#define CHARGE_CURVE_DIV       (20)

/* depth cues: shell shadow and inner wall chamfer */
#define CHARGE_SHADOW_DIV      (5)
#define CHARGE_WALL_DIV        (12)

/* anti aliasing: coverage samples taken across one pixel column */
#define CHARGE_AA_SAMPLES      (4)

/* gradient stop count, every ramp is the cylindrical one of charge_cyl_stops() */
#define CHARGE_STOP_NUM        (4)

typedef struct charge_stop {
    uint8_t     pos;
    sgl_color_t color;
} charge_stop_t;

typedef struct charge_layout {
    sgl_area_t body;       /* shell, terminal excluded */
    sgl_area_t cap;        /* positive terminal */
    sgl_area_t cav;        /* inner cavity */
    int16_t    body_r;     /* shell corner radius */
    int16_t    cav_r;      /* cavity corner radius */
    int16_t    cross_len;  /* cavity size across the cylinder axis */
    int16_t    along_len;  /* cavity size along the fill axis */
} charge_layout_t;

/**
 * @brief  mix a color towards white or towards black
 * @param  c: base color
 * @param  k: strength in [-255, 255], negative darkens, positive lightens
 * @return mixed color
 */
static sgl_color_t charge_mix(sgl_color_t c, int16_t k)
{
    return (k < 0) ? sgl_color_mixer(SGL_COLOR_BLACK, c, (uint8_t)-k)
                   : sgl_color_mixer(SGL_COLOR_WHITE, c, (uint8_t)k);
}

/**
 * @brief  build the cylindrical ramp shared by the terminal, the cavity and the
 *         liquid
 * @param  stops: out: stop table of CHARGE_STOP_NUM entries
 * @param  c: base color
 * @param  wall: how far both walls sit from the base, negative
 * @param  ridge: how far the highlight ridge sits above the base, positive
 * @param  mid: how far the far side of the tube sits below the base, negative
 * @note  the ridge is at 38% of the cross axis in all three of them, so shell,
 *        cavity and liquid read as one single tube
 */
static void charge_cyl_stops(charge_stop_t *stops, sgl_color_t c,
                             int16_t wall, int16_t ridge, int16_t mid)
{
    stops[0].pos = 0;   stops[0].color = charge_mix(c, wall);
    stops[1].pos = 38;  stops[1].color = charge_mix(c, ridge);
    stops[2].pos = 72;  stops[2].color = charge_mix(c, mid);
    stops[3].pos = 100; stops[3].color = charge_mix(c, wall);
}

/**
 * @brief  sample a gradient stop table
 * @param  stops: stop table, sorted by ascending @pos
 * @param  t: sample position in percent [0, 100]
 * @return interpolated color
 */
static sgl_color_t charge_grad_at(const charge_stop_t *stops, uint8_t t)
{
    uint8_t i;

    if (t <= stops[0].pos) return stops[0].color;
    for (i = 1; i < CHARGE_STOP_NUM; i++) {
        if (t <= stops[i].pos) {
            uint16_t span = (uint16_t)(stops[i].pos - stops[i - 1].pos);
            return sgl_color_mixer(stops[i].color, stops[i - 1].color, (uint8_t)(((uint16_t)(t - stops[i - 1].pos) * 255) / span));
        }
    }
    return stops[CHARGE_STOP_NUM - 1].color;
}

/**
 * @brief  vertical cut of a rounded rectangle at a given column
 * @param  d: distance from the rounded edge, 0 = the edge itself
 * @param  r: corner radius
 * @return how far the column must be pulled in on both ends, in pixels
 * @note  mirrors the circle equation (r-d)^2 + y^2 = r^2
 */
static int16_t charge_corner_inset(int16_t d, int16_t r)
{
    int32_t a, v;
    if (r <= 0 || d >= r) return 0;
    if (d < 0) d = 0;
    a = r - d;
    v = (int32_t)r * r - a * a;
    return (int16_t)(r - sgl_sqrt((uint32_t)v));
}

/**
 * @brief  corner cut of a rounded rectangle for the column at @pos
 * @param  pos: column index inside the rectangle
 * @param  len: rectangle length along the same axis
 * @param  r: corner radius
 * @return how far the column must be pulled in on both ends, in pixels
 */
static int16_t charge_column_inset(int16_t pos, int16_t len, int16_t r)
{
    int16_t a = charge_corner_inset(pos, r);
    int16_t c = charge_corner_inset(len - 1 - pos, r);
    return a > c ? a : c;
}

/**
 * @brief  map a cross axis / fill axis rectangle to screen coordinates
 * @param  vert: true when the charge is vertical
 * @param  base: rectangle the local coordinates are relative to
 * @param  c1: cross axis start, left for vertical and top for horizontal
 * @param  v1: fill axis start
 * @param  c2: cross axis end
 * @param  v2: fill axis end
 * @param  out: resulting screen rectangle
 */
static void charge_map_rect(bool vert, const sgl_area_t *base, int16_t c1, int16_t v1, int16_t c2, int16_t v2, sgl_area_t *out)
{
    if (vert) {
        out->x1 = base->x1 + c1;
        out->x2 = base->x1 + c2;
        out->y1 = base->y1 + v1;
        out->y2 = base->y1 + v2;
    } else {
        out->y1 = base->y1 + c1;
        out->y2 = base->y1 + c2;
        out->x1 = base->x1 + v1;
        out->x2 = base->x1 + v2;
    }
}

/**
 * @brief  derive every rectangle of the widget from the object bounds
 * @param  ch: charge object pointer
 * @param  lay: out: layout
 */
static void charge_layout_calc(const sgl_charge_t *ch, charge_layout_t *lay)
{
    const sgl_obj_t  *obj = &ch->obj;
    const sgl_font_t *f   = ch->font ? ch->font : sgl_get_system_font();
    const bool       vert = ch->vertical;

    int16_t x1 = obj->coords.x1;
    int16_t y1 = obj->coords.y1;
    int16_t w  = obj->coords.x2 - x1 + 1;
    int16_t h  = obj->coords.y2 - y1 + 1;
    int16_t fh = f ? sgl_font_get_height(f) : 0;
    int16_t text_h = ch->show_percentage ? (fh + 4) : 0;
    int16_t cap_h = 0, cap_w = 0, pad, cross, cw, a1, a2;

    if (vert) {
        cap_h = sgl_max(sgl_min(h / 22, h / 6), 2);
    } else {
        cap_w = sgl_max(sgl_min(w / 22, w / 6), 2);
    }

    lay->body.x1 = x1;
    lay->body.x2 = vert ? (x1 + w - 1) : (x1 + w - cap_w - 1);
    lay->body.y1 = y1 + cap_h;
    lay->body.y2 = y1 + h - text_h - 1;

    lay->body_r = lay->cav_r = 0;
    lay->cross_len = lay->along_len = 0;

    if (lay->body.x2 <= lay->body.x1 || lay->body.y2 <= lay->body.y1) return;

    cross = vert ? (lay->body.x2 - lay->body.x1 + 1)
                 : (lay->body.y2 - lay->body.y1 + 1);

    pad = sgl_max(sgl_max(cross / 40, 1), (int16_t)obj->border);
    lay->body_r = sgl_max(sgl_min(cross / 16, cross / 2), 1);
    lay->cav.x1 = lay->body.x1 + pad;
    lay->cav.y1 = lay->body.y1 + pad;
    lay->cav.x2 = lay->body.x2 - pad;
    lay->cav.y2 = lay->body.y2 - pad;
    lay->cav_r  = sgl_max(lay->body_r - pad, 1);

    lay->cross_len = vert ? (lay->cav.x2 - lay->cav.x1 + 1)
                          : (lay->cav.y2 - lay->cav.y1 + 1);
    lay->along_len = vert ? (lay->cav.y2 - lay->cav.y1 + 1)
                          : (lay->cav.x2 - lay->cav.x1 + 1);

    cw = cross * 31 / 100;
    a1 = vert ? (int16_t)(-cap_h) : (int16_t)(lay->body.x2 - lay->body.x1);
    a2 = vert ? 0 : (int16_t)(a1 + cap_w);
    charge_map_rect(vert, &lay->body, (cross - cw) / 2, a1, (cross - cw) / 2 + cw - 1, a2, &lay->cap);
}

/**
 * @brief  fill a run of one cavity column
 * @param  surf: surface pointer
 * @param  ch: charge object pointer
 * @param  lay: layout pointer
 * @param  c: column index along the cross axis
 * @param  v1: fill axis start
 * @param  v2: fill axis end, pass @v1 for a single pixel
 * @param  color: color to fill with
 * @param  alpha: alpha of the fill
 */
static void charge_put(sgl_surf_t *surf, sgl_charge_t *ch, charge_layout_t *lay,
                       int16_t c, int16_t v1, int16_t v2, sgl_color_t color, uint8_t alpha)
{
    sgl_area_t r;

    charge_map_rect(ch->vertical, &lay->cav, c, v1, c, v2, &r);
    sgl_draw_fill_rect(surf, &lay->cav, &r, 0, color, alpha);
}

/**
 * @brief  fill a rectangle with a gradient along the cross axis
 * @param  surf: surface pointer
 * @param  clip: clip area
 * @param  vert: true when the charge is vertical
 * @param  base: rectangle the local coordinates are relative to
 * @param  len: gradient length along the cross axis
 * @param  along: length along the fill axis
 * @param  r: corner radius, every strip is shortened accordingly
 * @param  v1: lowest fill axis row that may be painted
 * @param  v2: highest fill axis row that may be painted, plus one
 * @param  stops: stop table
 * @param  alpha: alpha of the gradient
 * @note  @v1/@v2 are absolute bounds, the corner trim only pulls the strip in
 *        as far as the rounded cavity allows, so a band that starts part way
 *        down the tube is still trimmed by one radius, not by two
 */
static void charge_fill_grad(sgl_surf_t *surf, sgl_area_t *clip, bool vert,
                              const sgl_area_t *base, int16_t len, int16_t along, int16_t r,
                              int16_t v1, int16_t v2,
                              const charge_stop_t *stops, uint8_t alpha)
{
    /* 1..4 px per strip, coarser shows banding */
    int16_t step = sgl_min(sgl_max(len / 64, 1), 4);
    int16_t c;

    for (c = 0; c < len; c += step) {
        sgl_area_t strip;
        int16_t c2  = sgl_min(c + step, len) - 1;
        int16_t cut = charge_column_inset(c, len, r);
        int16_t lo  = sgl_max(v1, cut);
        int16_t hi  = sgl_min(v2, along - cut);
        uint8_t  t  = (uint8_t)(((uint16_t)c * 100) / (len > 1 ? len - 1 : 1));

        if (hi - lo < 1) continue;
        charge_map_rect(vert, base, c, lo, c2, hi, &strip);
        sgl_draw_fill_rect(surf, clip, &strip, 0, charge_grad_at(stops, t), alpha);
    }
}

/**
 * @brief  draw the positive terminal
 * @param  surf: surface pointer
 * @param  ch: charge object pointer
 * @param  lay: layout pointer
 * @note  the terminal gets the same cylindrical ramp as the rest of the widget,
 *        only the end away from the shell is rounded
 */
static void charge_draw_cap(sgl_surf_t *surf, sgl_charge_t *ch, charge_layout_t *lay)
{
    charge_stop_t stops[CHARGE_STOP_NUM];
    bool vert = ch->vertical;
    int16_t len   = vert ? (lay->cap.x2 - lay->cap.x1 + 1)
                         : (lay->cap.y2 - lay->cap.y1 + 1);
    int16_t depth = vert ? (lay->cap.y2 - lay->cap.y1 + 1)
                         : (lay->cap.x2 - lay->cap.x1 + 1);
    int16_t r  = sgl_max(len / 12, 1);

    if (len < 8 || depth < 6) return;

    sgl_draw_fill_rich_rect(surf, &ch->obj.area, &lay->cap, vert ? r : 0, r, 0,
                            vert ? 0 : r, charge_mix(ch->body_color, -150), SGL_ALPHA_MAX);
    charge_cyl_stops(stops, ch->body_color, -150, 104, -55);
    /* the ramp stops one radius short of the rounded end */
    charge_fill_grad(surf, &ch->obj.area, vert, &lay->cap, len, depth, 0, vert ? r : 0, depth - r, stops, SGL_ALPHA_MAX);
}

/**
 * @brief  fill the cavity with the charge colour
 * @param  surf: surface pointer
 * @param  ch: charge object pointer
 * @param  lay: layout pointer
 * @param  v1: first fill axis row that still needs the cavity
 * @param  v2: last fill axis row that still needs the cavity, plus one
 * @note  the same cylindrical ramp as the liquid but much darker, so the empty
 *        part of the cavity already reads as the inside of a tube
 */
static void charge_draw_cavity(sgl_surf_t *surf, sgl_charge_t *ch, charge_layout_t *lay, int16_t v1, int16_t v2)
{
    charge_stop_t stops[CHARGE_STOP_NUM];
    charge_cyl_stops(stops, charge_mix(ch->body_color, -120), -90, 40, -70);
    charge_fill_grad(surf, &ch->obj.area, ch->vertical, &lay->cav, lay->cross_len,
                      lay->along_len, lay->cav_r, v1, v2, stops, SGL_ALPHA_MAX);
}

/**
 * @brief  fill axis rows that the liquid cannot cover this frame
 * @param  ch: charge object pointer
 * @param  lay: layout pointer
 * @param  lo: out: first row that has to be drawn under the liquid
 * @param  hi: out: last row that has to be drawn under the liquid, plus one
 * @note  the cavity ramp and the top shadow sit under the liquid, which is
 *        filled opaque, so only the band the surface can wander into is ever
 *        visible. The parabolic dip reaches its maximum at the centre of the
 *        cross axis and the ripple is bounded by its amplitude, both in 1/256 px
 */
static void charge_empty_band(const sgl_charge_t *ch, charge_layout_t *lay,
                              int16_t *lo, int16_t *hi)
{
    int32_t len   = lay->cross_len;
    int32_t along = lay->along_len;
    int32_t curve = sgl_max(len / CHARGE_CURVE_DIV, 2);
    int32_t amp   = sgl_max(len / CHARGE_WAVE_AMP_DIV, 1);
    int32_t fill  = (along * ch->disp_level * 256) / 100;
    int32_t margin = (curve + amp + 1) * 256;

    if (ch->vertical) {   /* the liquid sits at the far end of the tube */
        *lo = 0;
        *hi = (int16_t)sgl_min(along, (along * 256 - fill + margin + 255) / 256);
    } else {              /* the liquid sits at the near end */
        int32_t start = (fill - margin) / 256;
        *lo = (int16_t)(start < 0 ? 0 : sgl_min(start, along));
        *hi = (int16_t)along;
    }
}

/**
 * @brief  clamped surface position of one column, in 1/256 px
 * @param  ch: charge object pointer
 * @param  lay: layout pointer
 * @param  cfx: cross axis position in 1/256 px
 * @return fill axis coordinate of the surface in 1/256 px, from the cavity top
 */
static int32_t charge_surface_fx(const sgl_charge_t *ch, charge_layout_t *lay, int32_t cfx)
{
    int32_t len   = lay->cross_len;
    int32_t curve = sgl_max(len / CHARGE_CURVE_DIV, 2);
    int32_t amp   = sgl_max(len / CHARGE_WAVE_AMP_DIV, 1);
    int32_t fill  = (lay->along_len * ch->disp_level * 256) / 100;
    int32_t nx, cyl, wav, sv;
    int32_t cut;
    int32_t tick, deg, frac, a, b;

    /* parabolic dip */
    nx  = (cfx * 2 - len * 256) * 1000 / (len * 256);
    cyl = (1000 - nx * nx / 1000) * curve * 256 / 1000;
    /* sine ripple in Q15, angle in 1/256 degree ticks interpolated between
     * the two integer degree entries of sgl_sin() */
    tick = (int32_t)ch->phase * 256 + cfx * CHARGE_WAVE_DEG / len;
    tick %= 92160; /* wraps at 360 deg */
    if (tick < 0) tick += 92160;
    deg  = tick >> 8;
    frac = tick & 0xFF;
    a = sgl_sin((int16_t)deg);
    b = sgl_sin((int16_t)(deg + 1));
    wav = ((a + (((b - a) * frac) >> 8)) * amp) / 128; /* Q15 -> 1/256 px */

    sv = fill - cyl - wav;
    if (sv < 0) sv = 0;

    if (ch->vertical) sv = lay->along_len * 256 - sv; /* vertical grows from the bottom */

    /* clamp to the rounded cavity */
    cut = charge_column_inset((int16_t)(cfx >> 8), len, lay->cav_r);
    sv = sgl_max(sv, cut * 256);
    sv = sgl_min(sv, (lay->along_len - cut) * 256);

    return sv;
}

/**
 * @brief  how much of the boundary pixel one column covers with liquid
 * @param  ch: charge object pointer
 * @param  sv0: surface of the column start, in 1/256 px
 * @param  sv1: surface of the next column start, in 1/256 px
 * @param  row: the pixel the surface runs through
 * @return coverage in [0, 255]
 * @note  box filter of CHARGE_AA_SAMPLES coverages across one column. Average
 *        the coverages, not the sample positions: position averaging pins every
 *        transition to full brightness and breaks the edge into dashes. The
 *        samples are interpolated from the two column heads, both surface terms
 *        are linear over one pixel column to far under 1/256 px.
 */
static uint8_t charge_edge_cov(const sgl_charge_t *ch,
                               int16_t row, int32_t sv0, int32_t sv1)
{
    int32_t acc  = 0;
    int32_t slope = sv1 - sv0;
    int16_t i;

    for (i = 0; i < CHARGE_AA_SAMPLES; i++) {
        int32_t off = 256 / CHARGE_AA_SAMPLES / 2 + i * (256 / CHARGE_AA_SAMPLES);
        int32_t sv  = sv0 + slope * off / 256;
        int32_t d   = ch->vertical ? ((int32_t)row + 1) * 256 - sv
                                   : sv - (int32_t)row * 256;

        if (d < 0) d = 0;
        if (d > 256) d = 256;
        acc += d;
    }

    return (uint8_t)((acc / CHARGE_AA_SAMPLES) * 255 / 256);
}

/**
 * @brief  draw the liquid, one pixel column at a time
 * @param  surf: surface pointer
 * @param  ch: charge object pointer
 * @param  lay: layout pointer
 * @note  the sine surface is evaluated per pixel column in 1/256 px fixed
 *        point and charge_edge_cov() turns it into a pixel coverage. The body
 *        stops one pixel short of the surface, the one pixel meniscus is split
 *        over the boundary pixel and the first covered pixel so the highlight
 *        centre lands on the surface itself.
 */
static void charge_draw_liquid(sgl_surf_t *surf, sgl_charge_t *ch, charge_layout_t *lay)
{
    charge_stop_t stops[CHARGE_STOP_NUM];
    sgl_color_t hl;
    sgl_color_t cc;
    bool vert = ch->vertical;
    int16_t len = lay->cross_len;
    int16_t c;
    int16_t row;
    int16_t cut;
    int16_t v_lo;
    int16_t v_hi;
    int16_t o;
    int32_t sv;
    int32_t sv1;
    int32_t sv_next;
    uint8_t cov;
    uint8_t rest;

    if (len < 4 || ch->disp_level == 0) return;

    hl = charge_mix(ch->fill_color, 200);
    charge_cyl_stops(stops, ch->fill_color, -185, 130, -95);
    /* column heads are shared between neighbours, roll them through */
    sv_next = charge_surface_fx(ch, lay, 0);

    for (c = 0; c < len; c++) {
        sv   = sv_next;
        sv1  = charge_surface_fx(ch, lay, (int32_t)(c + 1) * 256);
        row  = (int16_t)(sv >> 8);
        cut  = charge_column_inset(c, len, lay->cav_r);
        cov  = charge_edge_cov(ch, row, sv, sv1);
        rest = (uint8_t)(SGL_ALPHA_MAX - cov);
        /* the boundary pixel belongs to the meniscus alone, sgl_draw_fill_rect()
         * spans are closed */
        v_lo = vert ? (row + 1) : cut;
        v_hi = vert ? (lay->along_len - cut) : (row - 1);
        cc   = charge_grad_at(stops, (uint8_t)(((uint16_t)c * 100) / (len - 1)));
        sv_next = sv1;

        if (v_hi >= v_lo) {
            charge_put(surf, ch, lay, c, v_lo, v_hi, cc, SGL_ALPHA_MAX);
        }

        if (cov > 0) {
            charge_put(surf, ch, lay, c, row, row, cc, cov);
            charge_put(surf, ch, lay, c, row, row, hl, cov);
        }

        /* meniscus remainder on the liquid side, bounded by the corner trim */
        if (rest > 0) {
            o = vert ? (row + 1) : (row - 1);
            if (o >= cut && o < lay->along_len - cut) {
                charge_put(surf, ch, lay, c, o, o, hl, rest);
            }
        }
    }
}

/**
 * @brief  draw the bubbles rising inside the liquid
 * @param  surf: surface pointer
 * @param  ch: charge object pointer
 * @param  lay: layout pointer
 * @note  a bubble climbs between the wall and the surface of its own column,
 *        inset by its radius, both ends from charge_surface_fx()
 */
static void charge_draw_bubbles(sgl_surf_t *surf, sgl_charge_t *ch, charge_layout_t *lay)
{
    int16_t min_dim = sgl_min(lay->cross_len, lay->along_len);
    sgl_area_t clip;
    uint8_t i;

    if (min_dim < 10 || ch->disp_level == 0) return;

    for (i = 0; i < SGL_CHARGE_BUBBLE_NUM; i++) {
        const sgl_charge_bubble_t *p = &ch->bubbles[i];
        int16_t r = sgl_max((int16_t)(min_dim * (6 + (int16_t)p->z * 5 / 255) / 260), 1);
        int16_t c, t, from, span, pos, cx, cy;
        int32_t sv;

        c  = (int16_t)(r + (int32_t)p->x * sgl_max(lay->cross_len - 2 * r, 1) / 255);
        sv = charge_surface_fx(ch, lay, (int32_t)c * 256);
        /* height derived from the animation phase, no per frame state */
        t  = (int16_t)(((ch->phase / (p->spd ? p->spd : 1)) + p->off) & 0xFF);

        if (ch->vertical) {
            /* flip the travel so the bubble keeps climbing as the phase runs on */
            from = (int16_t)((sv >> 8) + r);
            span = (int16_t)(lay->along_len - 1 - r - from);
            t    = (int16_t)(255 - t);
        } else {
            from = r;
            span = (int16_t)((sv >> 8) - 1 - r - from);
        }

        if (span < 0) continue;

        pos = (int16_t)(from + (int32_t)t * span / 255);
        cx  = ch->vertical ? (lay->cav.x1 + c) : (lay->cav.x1 + pos);
        cy  = ch->vertical ? (lay->cav.y1 + pos) : (lay->cav.y1 + c);

        /* hand sgl_draw_fill_circle() its own bounding box: the area argument is
         * the scan range, so passing the whole cavity makes every bubble walk
         * every cavity pixel. The clip stays inside the cavity as before. */
        clip.x1 = sgl_max(cx - r, lay->cav.x1);
        clip.y1 = sgl_max(cy - r, lay->cav.y1);
        clip.x2 = sgl_min(cx + r, lay->cav.x2);
        clip.y2 = sgl_min(cy + r, lay->cav.y2);
        if (clip.x2 < clip.x1 || clip.y2 < clip.y1) continue;

        sgl_draw_fill_circle(surf, &clip, cx, cy, r, ch->bubble_color, SGL_ALPHA_MAX);
    }
}

/**
 * @brief  darken a run of full length strips under a linear alpha ramp
 * @param  surf: surface pointer
 * @param  ch: charge object pointer
 * @param  lay: layout pointer
 * @param  cross: true to step along the cross axis, false along the fill axis
 * @param  first: index of the first strip
 * @param  dir: +1 walks up from @first, -1 walks down from it
 * @param  n: number of strips
 * @param  k0: alpha of the first strip, the ramp fades to transparent
 * @param  lo: first strip index that still needs painting
 * @param  hi: last strip index that still needs painting, plus one
 * @note  walks a linear alpha ramp one strip at a time, every strip trimmed by
 *        the corner radius so it reaches the rounded edge. Strips outside
 *        [lo, hi] keep the ramp they would have had, they are just skipped
 *        because something opaque covers them anyway
 */
static void charge_fade_strips(sgl_surf_t *surf, sgl_charge_t *ch, charge_layout_t *lay,
                               bool cross, int16_t first, int8_t dir, int16_t n, uint8_t k0,
                               int16_t lo, int16_t hi)
{
    int16_t i;
    if (n < 1) return;
    for (i = 0; i < n; i++) {
        sgl_area_t band;
        int16_t idx = (int16_t)(first + dir * i);
        uint8_t k   = (uint8_t)(k0 - (int32_t)i * k0 / n);
        int16_t cut;

        /* skip before the corner maths, the covered strips are the majority */
        if (idx < lo || idx >= hi) continue;
        cut = charge_column_inset(idx, cross ? lay->cross_len : lay->along_len,
                                  lay->cav_r);
        if (cross) {
            charge_map_rect(ch->vertical, &lay->cav, idx, cut, idx,
                            lay->along_len - 1 - cut, &band);
        } else {
            charge_map_rect(ch->vertical, &lay->cav, cut, idx,
                            lay->cross_len - 1 - cut, idx, &band);
        }
        sgl_draw_fill_rect(surf, &lay->cav, &band, 0, SGL_COLOR_BLACK, k);
    }
}

/**
 * @brief  draw the percentage readout below the shell
 * @param  surf: surface pointer
 * @param  ch: charge object pointer
 * @param  lay: layout pointer
 */
static void charge_draw_readout(sgl_surf_t *surf, sgl_charge_t *ch, charge_layout_t *lay)
{
    const sgl_font_t *f = ch->font ? ch->font : sgl_get_system_font();
    int16_t cx, y;
    char txt[8];

    if (f == NULL || !ch->show_percentage) return;

    sgl_sprintf(txt, "%d%%", ch->disp_level);
    y  = (int16_t)(lay->body.y2 + 1);
    cx = (int16_t)(ch->obj.coords.x1 +
                   (ch->obj.coords.x2 - ch->obj.coords.x1 + 1 -
                    (int16_t)sgl_font_get_string_width(txt, f)) / 2);
    sgl_draw_string(surf, &ch->obj.area, cx, y, txt, SGL_COLOR_WHITE, SGL_ALPHA_MAX, f);
}

/**
 * @brief  draw the whole widget
 * @param  surf: surface pointer
 * @param  ch: charge object pointer
 */
static void charge_draw(sgl_surf_t *surf, sgl_charge_t *ch)
{
    charge_layout_t lay;
    int16_t lo, hi;

    charge_layout_calc(ch, &lay);
    if (lay.cross_len < 4 || lay.along_len < 4) return;

    /* rows the still moving surface can uncover, everything under the liquid
     * body is painted over opaquely and does not need a background at all */
    charge_empty_band(ch, &lay, &lo, &hi);

    charge_draw_cap(surf, ch, &lay);
    sgl_draw_fill_rect(surf, &ch->obj.area, &lay.body, lay.body_r, ch->body_color, SGL_ALPHA_MAX);
    charge_draw_cavity(surf, ch, &lay, lo, hi);
    /* the shadow goes under the liquid, the walls over it */
    charge_fade_strips(surf, ch, &lay, false, 0, 1, lay.along_len / CHARGE_SHADOW_DIV, 217, lo, hi);
    charge_draw_liquid(surf, ch, &lay);
    charge_draw_bubbles(surf, ch, &lay);
    charge_fade_strips(surf, ch, &lay, true, 0, 1, lay.cross_len / CHARGE_WALL_DIV, 140, 0, lay.cross_len);
    charge_fade_strips(surf, ch, &lay, true, lay.cross_len - 1, -1, lay.cross_len / CHARGE_WALL_DIV, 166, 0, lay.cross_len);
    charge_draw_readout(surf, ch, &lay);
}

/**
 * @brief  seed one bubble
 * @param  p: bubble pointer
 * @note  the bubble has no runtime state besides this seed, its height comes
 *        from the animation phase
 */
static void charge_bubble_seed(sgl_charge_bubble_t *p)
{
    p->x   = (int16_t)((sgl_rand() & 0x7FFF) % 256);
    p->off = (int16_t)((sgl_rand() & 0x7FFF) % 256);
    p->z   = (uint8_t)(sgl_rand() & 0x7FFF);
    p->spd = (uint8_t)(1 + (sgl_rand() & 0x7FFF) % 3);
}

/**
 * @brief  animation path callback, stores the wave phase
 * @param  anim: animation pointer, its data is the charge object
 * @param  value: phase in degrees, 0..360
 * @return none
 * @note  the 0..360 sweep loops seamlessly because sgl_sin() wraps at 360
 */
static void charge_anim_cb(sgl_anim_t *anim, int32_t value)
{
    struct sgl_obj *obj = (struct sgl_obj *)anim->data;
    sgl_charge_t *ch;

    if (obj == NULL) return;

    ch = sgl_container_of(obj, sgl_charge_t, obj);
    ch->phase = (uint16_t)(value & 0xFFFF);
    sgl_obj_set_dirty(obj);
}

/**
 * @brief  start the looping wave animation
 * @param  ch: charge object pointer
 * @return none
 */
static void charge_start_anim(sgl_charge_t *ch)
{
    if (sgl_anim_get_by_obj(&ch->obj) != NULL) return;
    sgl_anim_setup_obj_action_loop(&ch->obj, 0, 360, CHARGE_ANIM_MS, charge_anim_cb, SGL_ANIM_PATH_LINEAR, NULL);
}

/**
 * @brief  charge construct callback
 * @param  surf: surface pointer
 * @param  obj: object pointer
 * @param  evt: event pointer
 * @return none
 * @note   this function is called when the object is created or redraw
 */
static void sgl_charge_construct_cb(sgl_surf_t *surf, sgl_obj_t *obj, sgl_event_t *evt)
{
    sgl_charge_t *ch = sgl_container_of(obj, sgl_charge_t, obj);

    if (evt->type == SGL_EVENT_DESTROYED) {
        sgl_anim_delete_by_obj(obj);
        return;
    }
    if (evt->type != SGL_EVENT_DRAW_MAIN) return;

    if (!ch->animated) {
        ch->disp_level = ch->level;
    } else {
        charge_start_anim(ch);
        if (ch->disp_level < ch->level) {
            ch->disp_level++;
        } else if (ch->disp_level > ch->level) {
            ch->disp_level--;
        }
    }
    charge_draw(surf, ch);
}

/**
 * @brief  create a charge object
 * @param  parent: parent object pointer
 * @return charge object pointer, NULL on failure
 */
sgl_obj_t* sgl_charge_create(sgl_obj_t *parent)
{
    sgl_charge_t *ch = sgl_malloc(sizeof(sgl_charge_t));
    uint8_t i;

    if (!ch) {
        SGL_LOG_ERROR("sgl_charge_create: malloc failed");
        return NULL;
    }
    memset(ch, 0, sizeof(*ch));

    sgl_obj_init(&ch->obj, parent);
    ch->obj.construct_fn = sgl_charge_construct_cb;
    ch->obj.border       = 1;
    ch->obj.radius       = 4;
    ch->level            = 60;
    ch->disp_level       = 60;
    ch->body_color       = sgl_rgb(30, 38, 48);
    ch->fill_color       = sgl_rgb(0, 200, 110);
    ch->bubble_color     = sgl_rgb(190, 255, 225);
    ch->show_percentage  = 1;
    ch->animated         = 1;

    for (i = 0; i < SGL_CHARGE_BUBBLE_NUM; i++) {
        charge_bubble_seed(&ch->bubbles[i]);
    }
    return &ch->obj;
}

/**
 * @brief set charge level
 * @param  obj: charge object pointer
 * @param  level: charge level (0-100)
 * @return none
 */
void sgl_charge_set_level(sgl_obj_t *obj, uint8_t level)
{
    sgl_charge_t *ch = sgl_container_of(obj, sgl_charge_t, obj);
    ch->level = level > 100 ? 100 : level;
    if (!ch->animated) ch->disp_level = ch->level;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the liquid fill color
 * @param  obj: charge object pointer
 * @param  color: liquid color to set
 * @return none
 */
void sgl_charge_set_fill_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_charge_t *ch = sgl_container_of(obj, sgl_charge_t, obj);
    ch->fill_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the shell / cavity color
 * @param  obj: charge object pointer
 * @param  color: body color to set
 * @return none
 */
void sgl_charge_set_body_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_charge_t *ch = sgl_container_of(obj, sgl_charge_t, obj);
    ch->body_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the bubble color
 * @param  obj: charge object pointer
 * @param  color: bubble color to set
 * @return none
 */
void sgl_charge_set_bubble_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_charge_t *ch = sgl_container_of(obj, sgl_charge_t, obj);
    ch->bubble_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the font of the percentage readout
 * @param  obj: charge object pointer
 * @param  font: font pointer to set (NULL = use the system font)
 * @return none
 */
void sgl_charge_set_font(sgl_obj_t *obj, const sgl_font_t *font)
{
    sgl_charge_t *ch = sgl_container_of(obj, sgl_charge_t, obj);
    ch->font = font;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief show or hide the percentage readout
 * @param  obj: charge object pointer
 * @param  show: show the percentage (1 = show, 0 = hide)
 * @return none
 */
void sgl_charge_show_percentage(sgl_obj_t *obj, bool show)
{
    sgl_charge_t *ch = sgl_container_of(obj, sgl_charge_t, obj);
    ch->show_percentage = show;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set charge orientation
 * @param  obj: charge object pointer
 * @param  vertical: true for vertical, false for horizontal
 * @return none
 */
void sgl_charge_set_vertical(sgl_obj_t *obj, bool vertical)
{
    sgl_charge_t *ch = sgl_container_of(obj, sgl_charge_t, obj);
    ch->vertical = vertical;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief enable or disable the built-in animation
 * @param  obj: charge object pointer
 * @param  animated: true to animate the sine wave and the bubbles
 * @return none
 * @note  the wave runs on a looping sgl_anim bound to the object; disabling
 *        deletes it
 */
void sgl_charge_set_animated(sgl_obj_t *obj, bool animated)
{
    sgl_charge_t *ch = sgl_container_of(obj, sgl_charge_t, obj);

    if (ch->animated == animated) return;
    ch->animated = animated;
    if (animated) {
        charge_start_anim(ch);
    } else {
        sgl_anim_delete_by_obj(obj);
        ch->disp_level = ch->level;
    }
    sgl_obj_set_dirty(obj);
}
