/* source/draw/sgl_draw_rect.c
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

/* Pixels per iteration of the span loops: 1 = smallest code, 4..8 = fastest. */
#ifndef SGL_RECT_UNROLL
#define SGL_RECT_UNROLL 4
#endif

/* Paint `src` over `*dst` with opacity `a`.  Redefine it with a SIMD / Helium /
 * DMA friendly blender on targets that have one. */
#ifndef SGL_RECT_MIX
#define SGL_RECT_MIX(dst, src, a) sgl_color_mixer((src), (dst), (a))
#endif

/* Solid store of a span; may be replaced by __stosd() / wide word memset. */
#ifndef SGL_RECT_SPAN_SET
#define SGL_RECT_SPAN_SET(d, n, c) sgl_rect_span_set((d), (n), (c))
#endif

typedef enum { RECT_SOLID = 0, RECT_BLIT, RECT_STEP, RECT_BILN } rect_kind_t;

typedef struct {
    sgl_color_t color;              /* solid source colour */
    const sgl_pixmap_t *px;         /* source pixmap (RECT_SOLID: NULL) */
    rect_kind_t kind;               /* how to get the source colour */
    int32_t sx, sy;                 /* pixmap -> rect step, SGL fixed point */
    int16_t rx1, ry1;               /* rect origin: maps screen -> source */
    uint8_t alpha;                  /* global opacity of the source */
    uint8_t solid;                  /* alpha == SGL_ALPHA_MAX: hoisted branch */
    uint8_t ring;                   /* 1 = border ring only, keep the interior */
    uint8_t bw;                     /* ring: border width */
    int hx1, hx2, hy1, hy2;         /* ring: inner hole rectangle */
} rect_paint_t;

typedef struct {
    int cy;                         /* arc centre (the x centre is per corner) */
    int cx;
    int sol_lim;                    /* real_r2 < sol_lim       -> fully covered */
    int edge_lim;                   /* real_r2 < edge_lim      -> AA outer rim */
    int hole_lim;                   /* real_r2 < hole_lim      -> hole, skip */
    int hole_edge_lim;              /* real_r2 < hole_edge_lim -> AA hole rim */
    int fix_out, fix_in;            /* fixed point slopes of the two AA bands */
    int dx_sol, dx_edge, dx_hole, dx_hole_edge;   /* scanline walk state */
} rect_arc_t;

static inline void sgl_rect_span_set(sgl_color_t *d, int n, sgl_color_t c)
{
    for (; n >= SGL_RECT_UNROLL; n -= SGL_RECT_UNROLL, d += SGL_RECT_UNROLL) {
        int k;
        for (k = 0; k < SGL_RECT_UNROLL; k++) d[k] = c;
    }
    for (; n > 0; n--, d++) *d = c;
}

/* Largest dx with dx*dx + dy2 < lim, i.e. the walk state of `lim` on row dy2. */
static inline int rect_dx_max(int lim, int dy2)
{
    int v = lim - dy2 - 1;
    return v < 0 ? -1 : (int)sgl_sqrt((uint32_t)v);
}

/* Source colour of one pixel: used by the anti-aliased rims, which are a
 * handful of pixels per scanline.  Bulk pixels go through rect_span(). */
static inline sgl_color_t rect_src(int x, int y, const rect_paint_t *p)
{
    switch (p->kind) {
    case RECT_BLIT: return *sgl_pixmap_get_buf(p->px, x - p->rx1, y - p->ry1);
    case RECT_STEP: return *sgl_pixmap_get_buf(p->px,
                            (p->sx * (x - p->rx1)) >> SGL_FIXED_SHIFT,
                            (p->sy * (y - p->ry1)) >> SGL_FIXED_SHIFT);
    case RECT_BILN: return sgl_draw_biln_color((sgl_color_t *)p->px->bitmap.array,
                            p->px->width, p->px->height,
                            p->sx * (x - p->rx1), p->sy * (y - p->ry1));
    default:        return p->color;
    }
}

/* dst = mix(src, dst, cov) then dst = mix(that, dst, alpha): identical result to
 * the two chained sgl_color_mixer() calls of the original code. */
static inline void rect_put(sgl_color_t *dst, sgl_color_t src, uint8_t cov, const rect_paint_t *p)
{
    if (p->solid) {
        *dst = SGL_RECT_MIX(*dst, src, cov);
    }
    else {
        sgl_color_t c = SGL_RECT_MIX(*dst, src, cov);
        *dst = SGL_RECT_MIX(*dst, c, p->alpha);
    }
}

/* Paint `n` pixels of one anti-aliased band, x ascending from `x0`.
 * `band`: 0 = outer rim, 1 = hole rim (ring mode only). */
static void rect_band(sgl_color_t *d, int x0, int y, int n, const rect_paint_t *p,
                      const rect_arc_t *a, int band)
{
    int dy2 = sgl_pow2(y - a->cy);
    int lim = band ? (a->hole_lim - 1) : a->edge_lim;
    int fix = band ? a->fix_in : a->fix_out;

    for (; n > 0; n--, d++, x0++) {
        int real_r2 = sgl_pow2(x0 - a->cx) + dy2;
        int diff = band ? (real_r2 - lim) : (lim - real_r2);
        uint8_t cov = (uint8_t)((diff < 1 ? 1 : diff) * fix >> SGL_FIXED_SHIFT);
        rect_put(d, rect_src(x0, y, p), cov, p);
    }
}

/* Paint `n` fully covered pixels starting at screen x `x0`.  The source
 * selection is hoisted out of the pixel loop, so the inner loops are pure
 * store / copy / blend streams that the compiler can vectorise. */
static void rect_span(sgl_color_t *d, int x0, int y, int n, const rect_paint_t *p)
{
    switch (p->kind) {
    case RECT_BLIT: {                             /* 1:1 pixmap: copy / blend */
        sgl_color_t *s = sgl_pixmap_get_buf(p->px, x0 - p->rx1, y - p->ry1);
        if (p->solid) {
            for (; n >= SGL_RECT_UNROLL; n -= SGL_RECT_UNROLL, d += SGL_RECT_UNROLL, s += SGL_RECT_UNROLL) {
                int k;
                for (k = 0; k < SGL_RECT_UNROLL; k++) d[k] = s[k];
            }
            for (; n > 0; n--, d++, s++) *d = *s;
        }
        else {
            for (; n > 0; n--, d++, s++) *d = SGL_RECT_MIX(*d, *s, p->alpha);
        }
        return;
    }
    case RECT_STEP: {                             /* nearest neighbour resample */
        uint32_t w = p->px->width;
        sgl_color_t *row = (sgl_color_t *)p->px->bitmap.array +
                           (uint32_t)((p->sy * (y - p->ry1)) >> SGL_FIXED_SHIFT) * w;
        int32_t acc = p->sx * (x0 - p->rx1);       /* no multiply per pixel any more */
        for (; n > 0; n--, d++, acc += p->sx) {
            sgl_color_t c = row[acc >> SGL_FIXED_SHIFT];
            *d = p->solid ? c : SGL_RECT_MIX(*d, c, p->alpha);
        }
        return;
    }
    case RECT_BILN: {                             /* bilinear resample */
        sgl_color_t *base = (sgl_color_t *)p->px->bitmap.array;
        uint32_t w = p->px->width, h = p->px->height;
        int32_t fy = p->sy * (y - p->ry1);
        int32_t fx = p->sx * (x0 - p->rx1);
        for (; n > 0; n--, d++, fx += p->sx) {
            sgl_color_t c = sgl_draw_biln_color(base, w, h, fx, fy);
            *d = p->solid ? c : SGL_RECT_MIX(*d, c, p->alpha);
        }
        return;
    }
    default: break;
    }

    if (p->solid) {                               /* opaque solid colour */
        SGL_RECT_SPAN_SET(d, n, p->color);
        return;
    }
    {
        sgl_color_t c = p->color;
        uint8_t a = p->alpha;
        for (; n >= SGL_RECT_UNROLL; n -= SGL_RECT_UNROLL, d += SGL_RECT_UNROLL) {
            int k;
            for (k = 0; k < SGL_RECT_UNROLL; k++) d[k] = SGL_RECT_MIX(d[k], c, a);
        }
        for (; n > 0; n--, d++) *d = SGL_RECT_MIX(*d, c, a);
    }
}

/* Seed one corner arc.  `in_r` < 0 for a filled rectangle, >= 0 for the border
 * ring (inner radius).  `dir` < 0 for the top arcs (|dy| shrinks while the
 * scanline loop runs), > 0 for the bottom ones. */
static void arc_init(rect_arc_t *a, int cx, int cy, int r, int in_r, int dir, int ring)
{
    int r2 = sgl_pow2(r), r2_max = sgl_pow2(r + 1);

    a->cx = cx;
    a->cy = cy;
    a->edge_lim = r2_max;                       /* covered at all: real_r2 < r2_max */
    a->sol_lim = ring ? r2 + 1 : r2;            /* fully covered (ring uses <= r2) */
    a->fix_out = (SGL_ALPHA_MAX << SGL_FIXED_SHIFT) / sgl_max(r2_max - r2, 1);

    if (ring) {
        int i2 = sgl_pow2(in_r), i2_max = sgl_pow2(in_r + 1);
        a->hole_lim = i2 + 1;                   /* hole: real_r2 <= in_r2 */
        a->hole_edge_lim = i2_max;              /* hole rim: real_r2 < in_r2_max */
        a->fix_in = (SGL_ALPHA_MAX << SGL_FIXED_SHIFT) / sgl_max(i2_max - i2, 1);
    }
    else {
        a->hole_lim = a->hole_edge_lim = a->fix_in = 0;
    }

    if (dir < 0) {
        a->dx_edge = a->dx_sol = a->dx_hole = a->dx_hole_edge = -1;
    }
    else {                                      /* first row of the band: |dy| = 1 */
        a->dx_hole = rect_dx_max(a->hole_lim, 1);
        a->dx_hole_edge = rect_dx_max(a->hole_edge_lim, 1);
        a->dx_sol = rect_dx_max(a->sol_lim, 1);
        a->dx_edge = rect_dx_max(a->edge_lim, 1);
    }
}

/* Monotone walk of the four thresholds of one arc on row `y`: a couple of adds
 * and compares per scanline instead of sgl_pow2() per pixel. */
static inline void arc_walk(rect_arc_t *a, int y)
{
    int dy2 = sgl_pow2(y - a->cy);

#define ARC_STEP(dx, lim)                                     \
    do {                                                     \
        while (sgl_pow2((dx) + 1) + dy2 < (lim)) (dx)++;     \
        while ((dx) >= 0 && sgl_pow2(dx) + dy2 >= (lim)) (dx)--; \
    } while (0)

    ARC_STEP(a->dx_edge, a->edge_lim);
    ARC_STEP(a->dx_sol, a->sol_lim);
    if (a->hole_lim) {              /* border ring only: a plain fill has no hole */
        ARC_STEP(a->dx_hole_edge, a->hole_edge_lim);
        ARC_STEP(a->dx_hole, a->hole_lim);
    }
#undef ARC_STEP
}

/* Left corner of a scanline: the arc bites in from the left, x <= cx.
 * `row` points at x = lo, the arc may only touch [lo, hi]. */
static void arc_left(sgl_color_t *row, int lo, int hi, int y, const rect_paint_t *p, const rect_arc_t *a)
{
    int out0 = sgl_max(lo, a->cx - a->dx_edge);
    int out1 = sgl_min(hi, a->cx - a->dx_sol - 1);
    int sol0 = sgl_max(lo, a->cx - a->dx_sol);
    int sol1 = sgl_min(hi, a->cx - a->dx_hole_edge - 1);
    int in0 = sgl_max(lo, a->cx - a->dx_hole_edge);
    int in1 = sgl_min(hi, a->cx - a->dx_hole - 1);

    if (out0 <= out1) rect_band(row + out0 - lo, out0, y, out1 - out0 + 1, p, a, 0);
    if (sol0 <= sol1) rect_span(row + sol0 - lo, sol0, y, sol1 - sol0 + 1, p);
    if (p->ring && in0 <= in1) rect_band(row + in0 - lo, in0, y, in1 - in0 + 1, p, a, 1);
}

/* Right corner of a scanline: the arc bites in from the right, x >= cx. */
static void arc_right(sgl_color_t *row, int lo, int hi, int y, const rect_paint_t *p, const rect_arc_t *a)
{
    int in0 = sgl_max(lo, a->cx + a->dx_hole + 1);
    int in1 = sgl_min(hi, a->cx + a->dx_hole_edge);
    int sol0 = sgl_max(lo, a->cx + a->dx_hole_edge + 1);
    int sol1 = sgl_min(hi, a->cx + a->dx_sol);
    int out0 = sgl_max(lo, a->cx + a->dx_sol + 1);
    int out1 = sgl_min(hi, a->cx + a->dx_edge);

    if (p->ring && in0 <= in1) rect_band(row + in0 - lo, in0, y, in1 - in0 + 1, p, a, 1);
    if (sol0 <= sol1) rect_span(row + sol0 - lo, sol0, y, sol1 - sol0 + 1, p);
    if (out0 <= out1) rect_band(row + out0 - lo, out0, y, out1 - out0 + 1, p, a, 0);
}

/* Rows [y0, y1] share the same two corner arcs (`top` picks TL/TR or BL/BR),
 * which is also what keeps the arc walk monotone. */
static void rect_rows(sgl_surf_t *surf, const sgl_area_t *clip, const sgl_area_t *rect,
                      rect_arc_t *la, rect_arc_t *ra, const rect_paint_t *p, int y0, int y1, int top)
{
    const int mid_x = (rect->x1 + rect->x2) / 2;
    const int stride = (int)surf->w;
    sgl_color_t *row = sgl_surf_get_buf(surf, clip->x1 - surf->x1, y0 - surf->y1);
    int y;

    for (y = y0; y <= y1; y++, row += stride) {
        int lo = clip->x1, hi = clip->x2;
        int l_on = top ? (y < la->cy) : (y > la->cy);
        int r_on = top ? (y < ra->cy) : (y > ra->cy);
        int lz_hi = rect->x1 - 1;               /* left arc zone  [rect->x1, lz_hi] */
        int rz_lo = rect->x2 + 1;               /* right arc zone [rz_lo, rect->x2] */
        int zs, ze;

        if (l_on) { arc_walk(la, y); lz_hi = sgl_min(la->cx - 1, mid_x); }
        if (r_on) { arc_walk(ra, y); rz_lo = sgl_max(ra->cx + 1, mid_x + 1); }

        if (l_on && lo <= lz_hi)
            arc_left(row + sgl_max(lo, rect->x1) - lo, sgl_max(lo, rect->x1), sgl_min(hi, lz_hi), y, p, la);

        /* what is left in between is bounded by straight edges */
        zs = sgl_max(lz_hi + 1, lo);
        ze = sgl_min(rz_lo - 1, hi);
        if (zs <= ze) {
            if (!p->ring || y < p->hy1 || y > p->hy2) {
                rect_span(row + zs - lo, zs, y, ze - zs + 1, p);
            }
            else {
                int a_end = sgl_min(ze, p->hx1 - 1);
                int b_start = sgl_max(sgl_max(zs, p->hx2 + 1), a_end + 1);
                if (zs <= a_end) rect_span(row + zs - lo, zs, y, a_end - zs + 1, p);
                if (b_start <= ze) rect_span(row + b_start - lo, b_start, y, ze - b_start + 1, p);
            }
        }

        if (r_on && rz_lo <= hi)
            arc_right(row + sgl_max(lo, rz_lo) - lo, sgl_max(lo, rz_lo), sgl_min(hi, rect->x2), y, p, ra);
    }
}

/* rect_rows() but for an axis aligned shape: `la`/`ra` are not used. */
static void rect_axis(sgl_surf_t *surf, const sgl_area_t *clip, const rect_paint_t *p)
{
    const int stride = (int)surf->w;
    int lo = clip->x1, n = clip->x2 - clip->x1 + 1, y;
    sgl_color_t *row = sgl_surf_get_buf(surf, clip->x1 - surf->x1, clip->y1 - surf->y1);

    if (!p->ring) {
        for (y = clip->y1; y <= clip->y2; y++, row += stride)
            rect_span(row, lo, y, n, p);
        return;
    }
    for (y = clip->y1; y <= clip->y2; y++, row += stride) {
        if (y < p->hy1 || y > p->hy2) {
            rect_span(row, lo, y, n, p);
        }
        else {
            int w1 = sgl_max(0, sgl_min(clip->x2, p->hx1 - 1) - lo + 1);
            int x2 = sgl_max(sgl_max(clip->x1, p->hx2 + 1), lo + w1);
            if (w1) rect_span(row, lo, y, w1, p);
            if (x2 <= clip->x2) rect_span(row + x2 - lo, x2, y, clip->x2 - x2 + 1, p);
        }
    }
}

/* Common core of all the rectangle routines.  r_* are the four corner radii
 * (they get clamped to half the box, which also fixes the old "radius larger
 * than the box" artefacts). */
static void rect_run(sgl_surf_t *surf, const sgl_area_t *clip, const sgl_area_t *rect,
                     int r_tl, int r_tr, int r_bl, int r_br, const rect_paint_t *p)
{
    rect_arc_t tl, tr, bl, br;
    int w = rect->x2 - rect->x1 + 1, h = rect->y2 - rect->y1 + 1;
    int max_r = sgl_min(w / 2, h / 2);
    int mid_y = (rect->y1 + rect->y2) / 2;
    int ring = p->ring;
    int in_tl = -1, in_tr = -1, in_bl = -1, in_br = -1;

    r_tl = sgl_min(sgl_max(r_tl, 0), max_r);
    r_tr = sgl_min(sgl_max(r_tr, 0), max_r);
    r_bl = sgl_min(sgl_max(r_bl, 0), max_r);
    r_br = sgl_min(sgl_max(r_br, 0), max_r);

    if (ring) {
        in_tl = sgl_max(r_tl - p->bw, 0);
        in_tr = sgl_max(r_tr - p->bw, 0);
        in_bl = sgl_max(r_bl - p->bw, 0);
        in_br = sgl_max(r_br - p->bw, 0);
    }

    arc_init(&tl, rect->x1 + r_tl, rect->y1 + r_tl, r_tl, in_tl, -1, ring);
    arc_init(&tr, rect->x2 - r_tr, rect->y1 + r_tr, r_tr, in_tr, -1, ring);
    arc_init(&bl, rect->x1 + r_bl, rect->y2 - r_bl, r_bl, in_bl, 1, ring);
    arc_init(&br, rect->x2 - r_br, rect->y2 - r_br, r_br, in_br, 1, ring);

    if (clip->y1 <= mid_y)
        rect_rows(surf, clip, rect, &tl, &tr, p, clip->y1, sgl_min(clip->y2, mid_y), 1);
    if (clip->y2 > mid_y)
        rect_rows(surf, clip, rect, &bl, &br, p, sgl_max(clip->y1, mid_y + 1), clip->y2, 0);
}

/**
 * @brief draw a wireframe rectangle with alpha
 * @param surf point to surface
 * @param area area of rectangle that you want to draw
 * @param rect point to rectangle that you want to draw
 * @param radius radius of round
 * @param width width of wireframe
 * @param color color of rectangle
 * @param alpha alpha of rectangle
 * @return none
 */
void sgl_draw_wireframe(sgl_surf_t *surf, sgl_area_t *area, sgl_area_t *rect, int16_t radius, int16_t width, sgl_color_t color, uint8_t alpha)
{
    const int16_t x1 = rect->x1;
    const int16_t x2 = rect->x2;
    const int16_t y1 = rect->y1;
    const int16_t y2 = rect->y2;

    if (radius == 0) {
        const int16_t s_ofs = (width - 1) / 2;
        const int16_t e_ofs = width / 2;
        sgl_draw_fill_hline(surf, area, y1 + s_ofs, x1, x2, width, color, alpha);
        sgl_draw_fill_hline(surf, area, y2 - e_ofs, x1, x2, width, color, alpha);
        sgl_draw_fill_vline(surf, area, x1 + s_ofs, y1, y2, width, color, alpha);
        sgl_draw_fill_vline(surf, area, x2 - e_ofs, y1, y2, width, color, alpha);
    }
    else {
        sgl_draw_fill_rect_border(surf, area, rect, radius, color, width, alpha);
    }
}

/* Every public routine only has to fill in this descriptor. */
static void rect_paint_init(rect_paint_t *p, const sgl_area_t *rect, sgl_color_t color, uint8_t alpha)
{
    p->color = color;
    p->px = NULL;
    p->kind = RECT_SOLID;
    p->sx = p->sy = 0;
    p->rx1 = rect->x1;
    p->ry1 = rect->y1;
    p->alpha = alpha;
    p->solid = (alpha == SGL_ALPHA_MAX);
    p->ring = 0;
    p->bw = 0;
    p->hx1 = p->hy1 = 0;
    p->hx2 = p->hy2 = -1;
}

/**
 * @brief fill a round rectangle with alpha
 * @param surf point to surface
 * @param area area of rectangle that you want to draw
 * @param rect point to rectangle that you want to draw
 * @param radius radius of round
 * @param color color of rectangle
 * @param alpha alpha of rectangle
 * @return none
 */
void sgl_draw_fill_rect(sgl_surf_t *surf, sgl_area_t *area, sgl_area_t *rect, int16_t radius, sgl_color_t color, uint8_t alpha)
{
    sgl_area_t clip = SGL_AREA_INVALID;
    rect_paint_t p;

    sgl_surf_clip_area_return(surf, area, &clip);
    if (!sgl_area_selfclip(&clip, rect)) return;

    rect_paint_init(&p, rect, color, alpha);
    if (radius <= 0) { rect_axis(surf, &clip, &p); return; }
    rect_run(surf, &clip, rect, radius, radius, radius, radius, &p);
}

/**
 * @brief fill a round rectangle with rich independent corner radiuses with alpha
 * @param surf point to surface
 * @param area area of rectangle that you want to draw
 * @param rect point to rectangle that you want to draw
 * @param tl_radius  Top-Left corner radius
 * @param tr_radius Top-Right corner radius
 * @param bl_radius Bottom-Left corner radius
 * @param br_radius   Bottom-Right corner radius
 * @param color color of rectangle
 * @param alpha alpha of rectangle
 * @return none
 */
void sgl_draw_fill_rich_rect(sgl_surf_t *surf, sgl_area_t *area, sgl_area_t *rect,
                             int16_t tl_radius, int16_t tr_radius, int16_t bl_radius, int16_t br_radius,
                             sgl_color_t color, uint8_t alpha)
{
    sgl_area_t clip = SGL_AREA_INVALID;
    rect_paint_t p;

    sgl_surf_clip_area_return(surf, area, &clip);
    if (!sgl_area_selfclip(&clip, rect)) return;

    rect_paint_init(&p, rect, color, alpha);
    if (tl_radius <= 0 && tr_radius <= 0 && bl_radius <= 0 && br_radius <= 0) {
        rect_axis(surf, &clip, &p);
        return;
    }
    rect_run(surf, &clip, rect, tl_radius, tr_radius, bl_radius, br_radius, &p);
}

/**
 * @brief draw only the border ring of a round rectangle, the interior is left untouched
 * @param surf point to surface
 * @param area area of rectangle that you want to draw
 * @param rect point to rectangle that you want to draw
 * @param radius radius of round
 * @param border_color color of border
 * @param border_width width of border
 * @param border_alpha alpha of border
 * @return none
 */
void sgl_draw_fill_rect_border(sgl_surf_t *surf, sgl_area_t *area, sgl_area_t *rect, int16_t radius,
                               sgl_color_t border_color, uint8_t border_width, uint8_t border_alpha)
{
    sgl_area_t clip = SGL_AREA_INVALID;
    rect_paint_t p;

    if (border_width == 0) return;
    sgl_surf_clip_area_return(surf, area, &clip);
    if (!sgl_area_selfclip(&clip, rect)) return;

    rect_paint_init(&p, rect, border_color, border_alpha);
    p.ring = 1;
    p.bw = border_width;
    p.hx1 = rect->x1 + border_width;
    p.hx2 = rect->x2 - border_width;
    p.hy1 = rect->y1 + border_width;
    p.hy2 = rect->y2 - border_width;

    if (radius <= 0) { rect_axis(surf, &clip, &p); return; }
    rect_run(surf, &clip, rect, radius, radius, radius, radius, &p);
}

/**
 * @brief draw only the border ring of a round rectangle with independent corner radii,
 *        the interior is left untouched
 * @param surf point to surface
 * @param area area of rectangle that you want to draw
 * @param rect point to rectangle that you want to draw
 * @param tl_radius radius of top-left corner
 * @param tr_radius radius of top-right corner
 * @param bl_radius radius of bottom-left corner
 * @param br_radius radius of bottom-right corner
 * @param border_color color of border
 * @param border_width width of border
 * @param border_alpha alpha of border
 * @return none
 */
void sgl_draw_fill_rect_border_rich(sgl_surf_t *surf, sgl_area_t *area, sgl_area_t *rect,
                                    int16_t tl_radius, int16_t tr_radius, int16_t bl_radius, int16_t br_radius,
                                    sgl_color_t border_color, uint8_t border_width, uint8_t border_alpha)
{
    sgl_area_t clip = SGL_AREA_INVALID;
    rect_paint_t p;

    if (border_width == 0) return;
    sgl_surf_clip_area_return(surf, area, &clip);
    if (!sgl_area_selfclip(&clip, rect)) return;

    rect_paint_init(&p, rect, border_color, border_alpha);
    p.ring = 1;
    p.bw = border_width;
    p.hx1 = rect->x1 + border_width;
    p.hx2 = rect->x2 - border_width;
    p.hy1 = rect->y1 + border_width;
    p.hy2 = rect->y2 - border_width;

    if (tl_radius <= 0 && tr_radius <= 0 && bl_radius <= 0 && br_radius <= 0) {
        rect_axis(surf, &clip, &p);
        return;
    }
    rect_run(surf, &clip, rect, tl_radius, tr_radius, bl_radius, br_radius, &p);
}

/* Map the pixmap onto the rect and pick the cheapest sampler:
 * 1:1 (copy), nearest neighbour, or bilinear. */
static void rect_paint_pixmap(rect_paint_t *p, const sgl_area_t *rect, const sgl_pixmap_t *px, uint8_t alpha)
{
    int32_t w = rect->x2 - rect->x1 + 1;
    int32_t h = rect->y2 - rect->y1 + 1;

    p->px = px;
    p->rx1 = rect->x1;
    p->ry1 = rect->y1;
    p->alpha = alpha;
    p->solid = (alpha == SGL_ALPHA_MAX);
    p->sx = ((int32_t)px->width << SGL_FIXED_SHIFT) / w;
    p->sy = ((int32_t)px->height << SGL_FIXED_SHIFT) / h;
#if (!CONFIG_SGL_PIXMAP_BILINEAR_INTERP)
    p->kind = (p->sx == SGL_FIXED_ONE && p->sy == SGL_FIXED_ONE) ? RECT_BLIT : RECT_STEP;
#else
    p->kind = (p->sx == SGL_FIXED_ONE && p->sy == SGL_FIXED_ONE) ? RECT_BLIT : RECT_BILN;
#endif
}

#if (!CONFIG_SGL_PIXMAP_BILINEAR_INTERP)
/**
 * @brief fill a round rectangle pixmap with alpha
 * @param surf point to surface
 * @param area area of rectangle that you want to draw
 * @param rect point to rectangle that you want to draw
 * @param radius radius of round
 * @param pixmap pixmap of rectangle
 * @param alpha alpha of rectangle
 * @return none
 */
void sgl_draw_fill_rect_pixmap(sgl_surf_t *surf, sgl_area_t *area, sgl_area_t *rect, int16_t radius, const sgl_pixmap_t *pixmap, uint8_t alpha)
{
    sgl_area_t clip = SGL_AREA_INVALID;
    rect_paint_t p;

    sgl_surf_clip_area_return(surf, area, &clip);
    if (!sgl_area_selfclip(&clip, rect)) return;

    rect_paint_init(&p, rect, (sgl_color_t){0}, alpha);
    rect_paint_pixmap(&p, rect, pixmap, alpha);

    if (radius <= 0) { rect_axis(surf, &clip, &p); return; }
    rect_run(surf, &clip, rect, radius, radius, radius, radius, &p);
}

/**
 * @brief fill a round rectangle pixmap with independent corner radii and alpha
 * @param surf point to surface
 * @param area area of rectangle that you want to draw
 * @param rect point to rectangle that you want to draw
 * @param tl_radius radius of top-left corner
 * @param tr_radius radius of top-right corner
 * @param bl_radius radius of bottom-left corner
 * @param br_radius radius of bottom-right corner
 * @param pixmap pixmap of rectangle
 * @param alpha alpha of rectangle
 * @return none
 */
void sgl_draw_fill_rect_pixmap_rich(sgl_surf_t *surf, sgl_area_t *area, sgl_area_t *rect,
                                    int16_t tl_radius, int16_t tr_radius, int16_t bl_radius, int16_t br_radius,
                                    const sgl_pixmap_t *pixmap, uint8_t alpha)
{
    sgl_area_t clip = SGL_AREA_INVALID;
    rect_paint_t p;

    sgl_surf_clip_area_return(surf, area, &clip);
    if (!sgl_area_selfclip(&clip, rect)) return;

    rect_paint_init(&p, rect, (sgl_color_t){0}, alpha);
    rect_paint_pixmap(&p, rect, pixmap, alpha);

    if (tl_radius <= 0 && tr_radius <= 0 && bl_radius <= 0 && br_radius <= 0) {
        rect_axis(surf, &clip, &p);
        return;
    }
    rect_run(surf, &clip, rect, tl_radius, tr_radius, bl_radius, br_radius, &p);
}
#else
/**
 * @brief fill a round rectangle pixmap with alpha
 * @param surf point to surface
 * @param area area of rectangle that you want to draw
 * @param rect point to rectangle that you want to draw
 * @param radius radius of round
 * @param pixmap pixmap of rectangle
 * @param alpha alpha of rectangle
 * @return none
 */
void sgl_draw_fill_rect_pixmap(sgl_surf_t *surf, sgl_area_t *area, sgl_area_t *rect, int16_t radius, const sgl_pixmap_t *pixmap, uint8_t alpha)
{
    sgl_area_t clip = SGL_AREA_INVALID;
    rect_paint_t p;

    sgl_surf_clip_area_return(surf, area, &clip);
    if (!sgl_area_selfclip(&clip, rect)) return;

    rect_paint_init(&p, rect, (sgl_color_t){0}, alpha);
    rect_paint_pixmap(&p, rect, pixmap, alpha);

    if (radius <= 0) { rect_axis(surf, &clip, &p); return; }
    rect_run(surf, &clip, rect, radius, radius, radius, radius, &p);
}

/**
 * @brief fill a round rectangle pixmap with individual corner radii and alpha
 * @param surf point to surface
 * @param area area of rectangle that you want to draw
 * @param rect point to rectangle that you want to draw
 * @param tl_radius radius of top-left corner
 * @param tr_radius radius of top-right corner
 * @param bl_radius radius of bottom-left corner
 * @param br_radius radius of bottom-right corner
 * @param pixmap pixmap of rectangle
 * @param alpha alpha of rectangle
 * @return none
 */
void sgl_draw_fill_rect_pixmap_rich(sgl_surf_t *surf, sgl_area_t *area, sgl_area_t *rect,
                                    int16_t tl_radius, int16_t tr_radius, int16_t bl_radius, int16_t br_radius,
                                    const sgl_pixmap_t *pixmap, uint8_t alpha)
{
    sgl_area_t clip = SGL_AREA_INVALID;
    rect_paint_t p;

    sgl_surf_clip_area_return(surf, area, &clip);
    if (!sgl_area_selfclip(&clip, rect)) return;

    rect_paint_init(&p, rect, (sgl_color_t){0}, alpha);
    rect_paint_pixmap(&p, rect, pixmap, alpha);

    if (tl_radius <= 0 && tr_radius <= 0 && bl_radius <= 0 && br_radius <= 0) {
        rect_axis(surf, &clip, &p);
        return;
    }
    rect_run(surf, &clip, rect, tl_radius, tr_radius, bl_radius, br_radius, &p);
}
#endif

/**
 * @brief fill a round rectangle with alpha
 * @param surf point to surface
 * @param area area of rectangle that you want to draw
 * @param rect point to rectangle that you want to draw
 * @param desc rectangle description
 * @return none
 */
void sgl_draw_rect(sgl_surf_t *surf, sgl_area_t *area, sgl_area_t *rect, sgl_draw_rect_t *desc)
{
    sgl_area_t rect_tmp = {
        .x1 = rect->x1 + desc->border,
        .x2 = rect->x2 - desc->border,
        .y1 = rect->y1 + desc->border,
        .y2 = rect->y2 - desc->border
    };

    if (desc->pixmap == NULL) {
        sgl_draw_fill_rect(surf, area, &rect_tmp, desc->radius - desc->border, desc->color, desc->alpha);
    }
    else {
        sgl_draw_fill_rect_pixmap(surf, area, &rect_tmp, desc->radius - desc->border, desc->pixmap, desc->alpha);
    }

    if (desc->border && (!desc->border_mask)) {
        sgl_draw_fill_rect_border(surf, area, rect, desc->radius, desc->border_color, desc->border, desc->border_alpha);
    }
}
