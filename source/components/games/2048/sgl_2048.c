/* source/components/games/2048/sgl_2048.c
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

#include "sgl_2048.h"

/* runtime layout, filled in by layout_calc() before any widget is created */
static int16_t g_margin_x, g_margin_y;        /* grid origin on screen */
static int16_t g_header_h;                    /* height of the title/score band */
static int16_t g_cell_sz, g_cell_gap;         /* tile edge / spacing */
static int16_t g_cell_r, g_grid_bg_r;         /* corner radii */
static int16_t g_grid_px;                     /* whole grid square edge */

static int g_grid[SGL_2048_GRID_N][SGL_2048_GRID_N];
static int g_prev[SGL_2048_GRID_N][SGL_2048_GRID_N];
static int g_score, g_best, g_over, g_won, g_busy;

/* permanent widgets */
static sgl_obj_t *g_scr_score, *g_scr_best;       /* score value labels */
static sgl_obj_t *g_cell_bg[SGL_2048_GRID_N][SGL_2048_GRID_N];    /* empty cell backgrounds */
static sgl_obj_t *g_tile_r[SGL_2048_GRID_N][SGL_2048_GRID_N];     /* tile rect */
static sgl_obj_t *g_tile_l[SGL_2048_GRID_N][SGL_2048_GRID_N];     /* tile label (child of rect) */
static sgl_obj_t *g_ov_bg, *g_ov_txt;             /* game-over overlay */

/* one slide move: sr/sc = source cell in the previous grid,
 * dr/dc = destination cell, merge = 1 if merging at the destination */
typedef struct { int sr, sc, dr, dc, val, merge; } move_t;
static move_t g_moves[SGL_2048_MAX_MOVES];
static int    g_move_cnt;

/* one merge pop: destination cell of a merge */
typedef struct { int dr, dc, val; } pop_t;
static pop_t     g_pops[SGL_2048_MAX_POPS];
static int       g_pop_cnt;

/* temporary animation widgets, created and destroyed on every move */
static sgl_obj_t *g_amov_r[SGL_2048_MAX_MOVES], *g_amov_l[SGL_2048_MAX_MOVES];  /* slide */
static sgl_obj_t *g_pop_r[SGL_2048_MAX_POPS],  *g_pop_l[SGL_2048_MAX_POPS];   /* pop */

static void ui_update(void);
static void new_game(void);
static void anim_cb(sgl_anim_t *a, int32_t pct);
static void anim_done_cb(sgl_anim_t *a);
static void pop_start(void);
static void pop_cb(sgl_anim_t *a, int32_t pct);
static void pop_done_cb(sgl_anim_t *a);

/**
 * @brief Derive the whole layout from the requested game size.
 *        The grid stays a square centered inside width x height, every
 *        dimension scales with the size (but never above the header).
 * @param width  game area width
 * @param height game area height
 * @return none
 */
static void layout_calc(int16_t width, int16_t height)
{
    int16_t avail_w, avail_h;

    /* top band reserved for the title + score boxes */
    g_header_h = height / 7;
    if (g_header_h < 40) g_header_h = 40;

    /* largest square that fits below the top band */
    avail_w = width  - width / 16;                /* side padding */
    avail_h = height - g_header_h - height / 40;  /* bottom padding */
    g_grid_px = (avail_w < avail_h) ? avail_w : avail_h;
    if (g_grid_px < SGL_2048_GRID_N * 2) g_grid_px = SGL_2048_GRID_N * 2;

    g_cell_gap = g_grid_px / 50;
    if (g_cell_gap < 2) g_cell_gap = 2;
    g_cell_sz  = (g_grid_px - (SGL_2048_GRID_N - 1) * g_cell_gap) / SGL_2048_GRID_N;

    /* exact square after integer rounding, centered in the whole area */
    g_grid_px  = SGL_2048_GRID_N * g_cell_sz + (SGL_2048_GRID_N - 1) * g_cell_gap;
    g_margin_x = (width - g_grid_px) / 2;
    g_margin_y = (height - g_grid_px) / 2;
    if (g_margin_y < g_header_h) g_margin_y = g_header_h;

    g_cell_r    = g_cell_sz / 9;
    g_grid_bg_r = g_cell_sz / 6;
}

/**
 * @brief Get the tile background color for a value.
 * @param v tile value
 * @return background color
 */
static sgl_color_t tile_bg(int v)
{
    switch (v) {
    case    2: return SGL_2048_C_2;    case    4: return SGL_2048_C_4;
    case    8: return SGL_2048_C_8;    case   16: return SGL_2048_C_16;
    case   32: return SGL_2048_C_32;   case   64: return SGL_2048_C_64;
    case  128: return SGL_2048_C_128;  case  256: return SGL_2048_C_256;
    case  512: return SGL_2048_C_512;  case 1024: return SGL_2048_C_1024;
    default:   return SGL_2048_C_2048;
    }
}

/**
 * @brief Get the tile text color for a value.
 * @param v tile value
 * @return text color
 */
static sgl_color_t tile_fg(int v)
{
    return (v <= 4) ? SGL_2048_C_TXT_D : SGL_2048_C_TXT_L;
}

/**
 * @brief Get the tile text font for a value (shrinks as digits grow).
 * @param v tile value
 * @return font
 */
static const sgl_font_t *tile_fn(int v)
{
    if (v < 100)   return &consolas24;
    if (v < 10000) return &consolas23;
    return &consolas14;
}

/**
 * @brief Place a new tile (90% '2', 10% '4') on a random empty cell.
 * @param none
 * @return none
 */
static void grid_add_random(void)
{
    int empty[SGL_2048_GRID_N * SGL_2048_GRID_N][2], n = 0, r, c, i;

    for (r = 0; r < SGL_2048_GRID_N; r++)
        for (c = 0; c < SGL_2048_GRID_N; c++)
            if (g_grid[r][c] == 0) { empty[n][0] = r; empty[n][1] = c; n++; }

    if (n > 0) {
        i = rand() % n;
        g_grid[empty[i][0]][empty[i][1]] = (rand() % 10 < 9) ? 2 : 4;
    }
}

/**
 * @brief Check whether any move is still possible.
 * @param none
 * @return 1 if a move is possible
 */
static int grid_can_move(void)
{
    int r, c;

    for (r = 0; r < SGL_2048_GRID_N; r++)
        for (c = 0; c < SGL_2048_GRID_N; c++) {
            if (!g_grid[r][c]) return 1;
            if (c < SGL_2048_GRID_N - 1 && g_grid[r][c] == g_grid[r][c+1]) return 1;
            if (r < SGL_2048_GRID_N - 1 && g_grid[r][c] == g_grid[r+1][c]) return 1;
        }
    return 0;
}

/**
 * @brief Check whether any tile reached 2048.
 * @param none
 * @return 1 if a tile is >= 2048
 */
static int grid_has_2048(void)
{
    int r, c;

    for (r = 0; r < SGL_2048_GRID_N; r++)
        for (c = 0; c < SGL_2048_GRID_N; c++)
            if (g_grid[r][c] >= 2048) return 1;
    return 0;
}

/**
 * @brief Rotate the grid 90 deg clockwise.
 * @param none
 * @return none
 */
static void rot_cw(void)
{
    int t[SGL_2048_GRID_N][SGL_2048_GRID_N], r, c;

    for (r = 0; r < SGL_2048_GRID_N; r++)
        for (c = 0; c < SGL_2048_GRID_N; c++)
            t[c][SGL_2048_GRID_N-1-r] = g_grid[r][c];
    memcpy(g_grid, t, sizeof t);
}

/**
 * @brief Rotate the grid 90 deg counter-clockwise.
 * @param none
 * @return none
 */
static void rot_ccw(void)
{
    int t[SGL_2048_GRID_N][SGL_2048_GRID_N], r, c;

    for (r = 0; r < SGL_2048_GRID_N; r++)
        for (c = 0; c < SGL_2048_GRID_N; c++)
            t[SGL_2048_GRID_N-1-c][r] = g_grid[r][c];
    memcpy(g_grid, t, sizeof t);
}

/**
 * @brief Record one animated move, mapping the rotated (row, col) used by
 *        slide_row() back to original grid coordinates.
 * @param row      row index in the rotated grid
 * @param anim_dir original move direction (0=left, 1=up, 2=right, 3=down)
 * @param src_col  source column in the rotated grid
 * @param dst_col  destination column in the rotated grid
 * @param val      tile value while sliding
 * @param merge    1 if the tile takes part in a merge at the destination
 * @return none
 */
static void record_move(int row, int anim_dir, int src_col, int dst_col, int val, int merge)
{
    int sr, sc, dr, dc;

    switch (anim_dir) {
    case 0: /* left */
        sr = row; sc = src_col; dr = row; dc = dst_col; break;
    case 1: /* up (rotated 1xCCW before) */
        sr = src_col; sc = SGL_2048_GRID_N-1-row; dr = dst_col; dc = SGL_2048_GRID_N-1-row; break;
    case 2: /* right (rotated 2xCCW before) */
        sr = SGL_2048_GRID_N-1-row; sc = SGL_2048_GRID_N-1-src_col; dr = SGL_2048_GRID_N-1-row; dc = SGL_2048_GRID_N-1-dst_col; break;
    case 3: /* down (rotated 3xCCW before) */
        sr = SGL_2048_GRID_N-1-src_col; sc = row; dr = SGL_2048_GRID_N-1-dst_col; dc = row; break;
    default:
        sr = row; sc = src_col; dr = row; dc = dst_col; break;
    }

    if (g_move_cnt < SGL_2048_MAX_MOVES) {
        g_moves[g_move_cnt].sr    = sr;
        g_moves[g_move_cnt].sc    = sc;
        g_moves[g_move_cnt].dr    = dr;
        g_moves[g_move_cnt].dc    = dc;
        g_moves[g_move_cnt].val   = val;
        g_moves[g_move_cnt].merge = merge;
        g_move_cnt++;
    }
}

/**
 * @brief Slide one (rotated) row to the left and merge equal neighbours.
 *        Every moving tile is recorded in g_moves[] for the slide animation.
 * @param row      row index in the rotated grid
 * @param anim_dir original move direction (0=left, 1=up, 2=right, 3=down)
 * @return score gained by this row
 */
static int slide_row(int row, int anim_dir)
{
    int i, j, k, v, sc = 0;
    int val[SGL_2048_GRID_N], src[SGL_2048_GRID_N], out[SGL_2048_GRID_N];

    /* compact, remember each tile's source column */
    k = 0;
    for (i = 0; i < SGL_2048_GRID_N; i++)
        if (g_grid[row][i]) {
            val[k] = g_grid[row][i];
            src[k] = i;
            k++;
        }

    /* place tiles at dest column j, merge equal neighbours */
    j = 0;
    for (i = 0; i < k; i++) {
        v = val[i];
        if (i + 1 < k && val[i + 1] == v) {
            v *= 2;
            sc += v;
            /* both source tiles slide to the same dest (own value each) */
            record_move(row, anim_dir, src[i],     j, val[i],     1);
            record_move(row, anim_dir, src[i + 1], j, val[i + 1], 1);
            i++;
        } else {
            record_move(row, anim_dir, src[i], j, v, 0);
        }
        out[j++] = v;
    }
    for (; j < SGL_2048_GRID_N; j++) out[j] = 0;

    for (i = 0; i < SGL_2048_GRID_N; i++) g_grid[row][i] = out[i];

    return sc;
}

/**
 * @brief Perform one move in the given direction and start the slide animation.
 *        The grid is rotated so every direction becomes a slide-left.
 * @param dir move direction (0=left, 1=up, 2=right, 3=down)
 * @return none
 */
static void do_move(int dir)
{
    int r, i, v, sc = 0, changed;
    move_t *m;
    sgl_anim_t *a;

    if (g_busy || g_over) return;

    memcpy(g_prev, g_grid, sizeof g_grid);
    g_move_cnt = 0;

    for (i = 0; i < dir; i++) rot_ccw();
    for (r = 0; r < SGL_2048_GRID_N; r++) sc += slide_row(r, dir);
    for (i = 0; i < dir; i++) rot_cw();

    /* nothing actually changed, no animation needed */
    changed = memcmp(g_grid, g_prev, sizeof g_grid) != 0;
    if (!changed) return;

    g_score += sc;
    if (g_score > g_best) g_best = g_score;

    g_busy = 1;

    /* hide all static tiles, they are replaced by temp animation widgets */
    for (r = 0; r < SGL_2048_GRID_N; r++)
        for (i = 0; i < SGL_2048_GRID_N; i++) {
            sgl_obj_set_hidden(g_tile_r[r][i]);
            sgl_obj_set_hidden(g_tile_l[r][i]);
        }

    for (i = 0; i < g_move_cnt && i < SGL_2048_MAX_MOVES; i++) {
        m = &g_moves[i];
        v = m->val;

        g_amov_r[i] = sgl_rect_create(NULL);
        sgl_obj_set_pos(g_amov_r[i], SGL_2048_CELL_X(m->sc), SGL_2048_CELL_Y(m->sr));
        sgl_obj_set_size(g_amov_r[i], SGL_2048_CELL_SZ, SGL_2048_CELL_SZ);
        sgl_rect_set_color(g_amov_r[i], tile_bg(v));
        sgl_obj_set_radius(g_amov_r[i], SGL_2048_CELL_R);
        sgl_obj_set_border_width(g_amov_r[i], 0);

        g_amov_l[i] = sgl_label_create(g_amov_r[i]);
        sgl_obj_set_pos(g_amov_l[i], 0, 0);
        sgl_obj_set_size(g_amov_l[i], SGL_2048_CELL_SZ, SGL_2048_CELL_SZ);
        sgl_label_set_font(g_amov_l[i], tile_fn(v));
        sgl_label_set_text_fmt_dynamic(g_amov_l[i], "%d", v);
        sgl_label_set_text_color(g_amov_l[i], tile_fg(v));
        sgl_label_set_text_align(g_amov_l[i], SGL_ALIGN_CENTER);
    }

    a = sgl_anim_create();
    sgl_anim_set_start_value(a, 0);
    sgl_anim_set_end_value(a, 100);
    sgl_anim_set_act_duration(a, SGL_2048_ANIM_MS);
    sgl_anim_set_path(a, anim_cb, SGL_ANIM_PATH_EASE_OUT);
    sgl_anim_set_finish_cb(a, anim_done_cb);
    sgl_anim_set_auto_free(a);
    sgl_anim_start(a, SGL_ANIM_REPEAT_ONCE);
}

/**
 * @brief Slide animation path callback: interpolate every moving tile
 *        from its source cell to its destination cell.
 * @param a   animation object (unused)
 * @param pct animation progress in [0, 100]
 * @return none
 */
static void anim_cb(sgl_anim_t *a, int32_t pct)
{
    int i, sx, sy, dx, dy;
    move_t *m;

    (void)a;
    for (i = 0; i < g_move_cnt && i < SGL_2048_MAX_MOVES; i++) {
        m = &g_moves[i];
        sx = SGL_2048_CELL_X(m->sc); sy = SGL_2048_CELL_Y(m->sr);
        dx = SGL_2048_CELL_X(m->dc); dy = SGL_2048_CELL_Y(m->dr);

        if (g_amov_r[i])
            sgl_obj_set_pos(g_amov_r[i], sx + (dx - sx) * pct / 100,
                                         sy + (dy - sy) * pct / 100);
    }
}

/**
 * @brief Slide animation finish callback: destroy the temp widgets, spawn a
 *        new tile, update win/lose state and start the merge pop effect.
 * @param a animation object (unused)
 * @return none
 */
static void anim_done_cb(sgl_anim_t *a)
{
    int i, j;
    move_t *m;

    (void)a;
    for (i = 0; i < g_move_cnt && i < SGL_2048_MAX_MOVES; i++) {
        if (g_amov_r[i]) {
            sgl_obj_delete(g_amov_r[i]);
            g_amov_r[i] = NULL;
        }
        g_amov_l[i] = NULL;     /* child of rect, already freed */
    }

    /* collect unique merge destinations for the pop effect */
    g_pop_cnt = 0;
    for (i = 0; i < g_move_cnt && i < SGL_2048_MAX_MOVES; i++) {
        m = &g_moves[i];
        if (!m->merge) continue;
        for (j = 0; j < g_pop_cnt; j++)
            if (g_pops[j].dr == m->dr && g_pops[j].dc == m->dc) break;
        if (j == g_pop_cnt && g_pop_cnt < SGL_2048_MAX_POPS) {
            g_pops[g_pop_cnt].dr  = m->dr;
            g_pops[g_pop_cnt].dc  = m->dc;
            g_pops[g_pop_cnt].val = g_grid[m->dr][m->dc];
            g_pop_cnt++;
        }
    }

    grid_add_random();
    if (grid_has_2048() && !g_won) g_won = 1;
    if (!grid_can_move()) g_over = 1;
    ui_update();

    if (g_pop_cnt > 0)
        pop_start();
    else
        g_busy = 0;
}

/**
 * @brief Start the merge pop effect: merged tiles scale up from 20% to
 *        their full size, animated by temp widgets.
 * @param none
 * @return none
 */
static void pop_start(void)
{
    int i;
    int sz0 = SGL_2048_CELL_SZ / 5;      /* initial size: 20% */
    pop_t *p;

    for (i = 0; i < g_pop_cnt; i++) {
        p = &g_pops[i];

        /* hide the static merged tile, animate a temp widget in its place */
        sgl_obj_set_hidden(g_tile_r[p->dr][p->dc]);
        sgl_obj_set_hidden(g_tile_l[p->dr][p->dc]);

        g_pop_r[i] = sgl_rect_create(NULL);
        sgl_obj_set_pos(g_pop_r[i], SGL_2048_CELL_X(p->dc) + (SGL_2048_CELL_SZ - sz0) / 2,
                                    SGL_2048_CELL_Y(p->dr) + (SGL_2048_CELL_SZ - sz0) / 2);
        sgl_obj_set_size(g_pop_r[i], sz0, sz0);
        sgl_rect_set_color(g_pop_r[i], tile_bg(p->val));
        sgl_obj_set_radius(g_pop_r[i], SGL_2048_CELL_R);
        sgl_obj_set_border_width(g_pop_r[i], 0);

        g_pop_l[i] = sgl_label_create(g_pop_r[i]);
        sgl_obj_set_pos(g_pop_l[i], 0, 0);
        sgl_obj_set_size(g_pop_l[i], sz0, sz0);
        sgl_label_set_font(g_pop_l[i], tile_fn(p->val));
        sgl_label_set_text_fmt_dynamic(g_pop_l[i], "%d", p->val);
        sgl_label_set_text_color(g_pop_l[i], tile_fg(p->val));
        sgl_label_set_text_align(g_pop_l[i], SGL_ALIGN_CENTER);
    }

    sgl_anim_move_to(0, 100, SGL_2048_POP_MS, pop_cb, SGL_ANIM_PATH_EASE_OUT, pop_done_cb);
}

/**
 * @brief Pop animation path callback: scale every popping tile 20% -> 100%
 *        while keeping it centered on its cell.
 * @param a   animation object (unused)
 * @param pct animation progress in [0, 100]
 * @return none
 */
static void pop_cb(sgl_anim_t *a, int32_t pct)
{
    int i, sz;
    pop_t *p;

    (void)a;
    sz = SGL_2048_CELL_SZ * (20 + 80 * pct / 100) / 100;
    for (i = 0; i < g_pop_cnt && i < SGL_2048_MAX_POPS; i++) {
        p = &g_pops[i];
        if (!g_pop_r[i]) continue;
        sgl_obj_set_pos(g_pop_r[i], SGL_2048_CELL_X(p->dc) + (SGL_2048_CELL_SZ - sz) / 2,
                                    SGL_2048_CELL_Y(p->dr) + (SGL_2048_CELL_SZ - sz) / 2);
        sgl_obj_set_size(g_pop_r[i], sz, sz);
        sgl_obj_set_size(g_pop_l[i], sz, sz);
    }
}

/**
 * @brief Pop animation finish callback: destroy the temp widgets and
 *        unlock input.
 * @param a animation object (unused)
 * @return none
 */
static void pop_done_cb(sgl_anim_t *a)
{
    int i;

    (void)a;
    for (i = 0; i < g_pop_cnt && i < SGL_2048_MAX_POPS; i++) {
        if (g_pop_r[i]) {
            sgl_obj_delete(g_pop_r[i]);
            g_pop_r[i] = NULL;
        }
        g_pop_l[i] = NULL;      /* child of rect, already freed */
    }
    g_pop_cnt = 0;
    g_busy = 0;
    ui_update();
}

/**
 * @brief Refresh every widget from the current grid state: score labels,
 *        tile colors/values, and the win/lose overlay.
 * @param none
 * @return none
 */
static void ui_update(void)
{
    int r, c, v;

    if (g_scr_score) sgl_label_set_text_fmt_dynamic(g_scr_score, "%d", g_score);
    if (g_scr_best)  sgl_label_set_text_fmt_dynamic(g_scr_best, "%d", g_best);

    for (r = 0; r < SGL_2048_GRID_N; r++) {
        for (c = 0; c < SGL_2048_GRID_N; c++) {
            v = g_grid[r][c];
            sgl_obj_set_pos(g_tile_r[r][c], SGL_2048_CELL_X(c), SGL_2048_CELL_Y(r));
            if (v == 0) {
                sgl_obj_set_hidden(g_tile_r[r][c]);
                sgl_obj_set_hidden(g_tile_l[r][c]);
            } else {
                sgl_obj_set_visible(g_tile_r[r][c]);
                sgl_rect_set_color(g_tile_r[r][c], tile_bg(v));
                sgl_obj_set_visible(g_tile_l[r][c]);
                sgl_label_set_font(g_tile_l[r][c], tile_fn(v));
                sgl_label_set_text_fmt_dynamic(g_tile_l[r][c], "%d", v);
                sgl_label_set_text_color(g_tile_l[r][c], tile_fg(v));
            }
        }
    }

    if (g_over || g_won) {
        sgl_obj_set_visible(g_ov_bg);
        sgl_obj_set_visible(g_ov_txt);
        sgl_label_set_text(g_ov_txt, g_won ? "You Win!" : "Game Over!");
    } else {
        sgl_obj_set_hidden(g_ov_bg);
        sgl_obj_set_hidden(g_ov_txt);
    }
}

/**
 * @brief Reset the game state and spawn the two initial tiles.
 * @param none
 * @return none
 */
static void new_game(void)
{
    int i;

    /* clean up any leftover animation widgets */
    for (i = 0; i < SGL_2048_MAX_MOVES; i++) {
        if (g_amov_r[i]) { sgl_obj_delete(g_amov_r[i]); g_amov_r[i] = NULL; }
        g_amov_l[i] = NULL;
    }
    for (i = 0; i < SGL_2048_MAX_POPS; i++) {
        if (g_pop_r[i]) { sgl_obj_delete(g_pop_r[i]); g_pop_r[i] = NULL; }
        g_pop_l[i] = NULL;
    }
    g_pop_cnt = 0;

    memset(g_grid, 0, sizeof g_grid);
    g_score = 0; g_over = 0; g_won = 0; g_busy = 0; g_move_cnt = 0;
    grid_add_random();
    grid_add_random();
    ui_update();
}

/**
 * @brief Game event callback: swipe gestures move tiles, a click restarts
 *        after win/lose, a long click asks to exit (launcher msgbox).
 * @param e event
 * @return none
 */
static void event_cb(sgl_event_t *e)
{
    switch (e->type) {
    case SGL_EVENT_MOVE_LEFT:  do_move(0); break;
    case SGL_EVENT_MOVE_UP:    do_move(1); break;
    case SGL_EVENT_MOVE_RIGHT: do_move(2); break;
    case SGL_EVENT_MOVE_DOWN:  do_move(3); break;
    case SGL_EVENT_CLICKED:
        if (g_over || g_won) new_game();
        break;
    default: break;
    }
}

/**
 * @brief Start the 2048 game: build the whole UI under parent, sized to
 *        width x height. All geometry is derived from the size by
 *        layout_calc(), the grid is centered inside the area.
 * @param parent     parent object (usually the active screen)
 * @param width      game area width
 * @param height     game area height
 * @param title_font font for the "2048" title and the overlay text
 * @param score_font font for the SCORE box
 * @param best_font  font for the BEST box
 * @param tile_font  initial tile font (resized per value while playing)
 * @return none
 */
void sgl_game2048_start(sgl_obj_t *parent, int16_t width, int16_t height,
                        sgl_font_t *title_font, sgl_font_t *score_font, sgl_font_t *best_font, sgl_font_t *tile_font)
{
    int r, c, i;
    int16_t pad_y, box_h, box_w, box_gap, best_x, score_x;
    sgl_obj_t *bg, *t, *sb, *st, *bb, *bt, *gb;

    srand(54321);

    layout_calc(width, height);
    pad_y   = g_header_h / 5;
    box_h   = g_header_h / 2;
    box_w   = width / 5;
    box_gap = width / 40;
    best_x  = width - box_w - box_gap;
    score_x = best_x - box_w - box_gap;

    for (i = 0; i < SGL_2048_MAX_MOVES; i++) {
        g_amov_r[i] = NULL;
        g_amov_l[i] = NULL;
    }

    /* background */
    bg = sgl_rect_create(parent);
    sgl_obj_set_pos(bg, 0, 0);
    sgl_obj_set_size(bg, width, height);
    sgl_rect_set_border_width(bg, 0);
    sgl_rect_set_color(bg, SGL_2048_C_BG);

    /* title */
    t = sgl_label_create(parent);
    sgl_obj_set_pos(t, g_margin_x, pad_y);
    sgl_obj_set_size(t, width / 3, box_h);
    sgl_label_set_font(t, title_font);
    sgl_label_set_text(t, "2048");
    sgl_label_set_text_color(t, SGL_2048_C_TXT_D);

    /* score box */
    sb = sgl_rect_create(parent);
    sgl_obj_set_pos(sb, score_x, pad_y);
    sgl_obj_set_size(sb, box_w, box_h);
    sgl_rect_set_color(sb, SGL_2048_C_SBOX);
    sgl_obj_set_radius(sb, 6);
    sgl_obj_set_border_width(sb, 0);

    st = sgl_label_create(sb);
    sgl_obj_set_pos(st, 0, 0);
    sgl_obj_set_size(st, box_w, box_h / 2);
    sgl_label_set_font(st, score_font);
    sgl_label_set_text(st, "SCORE");
    sgl_label_set_text_color(st, SGL_COLOR_WHITE);
    sgl_label_set_text_align(st, SGL_ALIGN_CENTER);

    g_scr_score = sgl_label_create(sb);
    sgl_obj_set_pos(g_scr_score, 0, box_h / 2);
    sgl_obj_set_size(g_scr_score, box_w, box_h / 2);
    sgl_label_set_font(g_scr_score, score_font);
    sgl_label_set_text(g_scr_score, "0");
    sgl_label_set_text_color(g_scr_score, SGL_COLOR_WHITE);
    sgl_label_set_text_align(g_scr_score, SGL_ALIGN_CENTER);

    /* best box */
    bb = sgl_rect_create(parent);
    sgl_obj_set_pos(bb, best_x, pad_y);
    sgl_obj_set_size(bb, box_w, box_h);
    sgl_rect_set_color(bb, SGL_2048_C_SBOX);
    sgl_obj_set_radius(bb, 6);
    sgl_obj_set_border_width(bb, 0);

    bt = sgl_label_create(bb);
    sgl_obj_set_pos(bt, 0, 0);
    sgl_obj_set_size(bt, box_w, box_h / 2);
    sgl_label_set_font(bt, best_font);
    sgl_label_set_text(bt, "BEST");
    sgl_label_set_text_color(bt, SGL_COLOR_WHITE);
    sgl_label_set_text_align(bt, SGL_ALIGN_CENTER);

    g_scr_best = sgl_label_create(bb);
    sgl_obj_set_pos(g_scr_best, 0, box_h / 2);
    sgl_obj_set_size(g_scr_best, box_w, box_h / 2);
    sgl_label_set_font(g_scr_best, best_font);
    sgl_label_set_text(g_scr_best, "0");
    sgl_label_set_text_color(g_scr_best, SGL_COLOR_WHITE);
    sgl_label_set_text_align(g_scr_best, SGL_ALIGN_CENTER);

    /* grid background */
    gb = sgl_rect_create(parent);
    sgl_obj_set_pos(gb, g_margin_x, g_margin_y);
    sgl_obj_set_size(gb, SGL_2048_GRID_PX, SGL_2048_GRID_PX);
    sgl_rect_set_color(gb, SGL_2048_C_BG);
    sgl_obj_set_radius(gb, SGL_2048_GRID_BG_R);
    sgl_obj_set_border_width(gb, 0);

    /* empty cell backgrounds (always visible, under the tiles) */
    for (r = 0; r < SGL_2048_GRID_N; r++) {
        for (c = 0; c < SGL_2048_GRID_N; c++) {
            g_cell_bg[r][c] = sgl_rect_create(parent);
            sgl_obj_set_pos(g_cell_bg[r][c], SGL_2048_CELL_X(c), SGL_2048_CELL_Y(r));
            sgl_obj_set_size(g_cell_bg[r][c], SGL_2048_CELL_SZ, SGL_2048_CELL_SZ);
            sgl_rect_set_color(g_cell_bg[r][c], SGL_2048_C_EMPTY);
            sgl_obj_set_radius(g_cell_bg[r][c], SGL_2048_CELL_R);
            sgl_obj_set_border_width(g_cell_bg[r][c], 0);
        }
    }

    /* tiles */
    for (r = 0; r < SGL_2048_GRID_N; r++) {
        for (c = 0; c < SGL_2048_GRID_N; c++) {
            g_tile_r[r][c] = sgl_rect_create(parent);
            sgl_obj_set_pos(g_tile_r[r][c], SGL_2048_CELL_X(c), SGL_2048_CELL_Y(r));
            sgl_obj_set_size(g_tile_r[r][c], SGL_2048_CELL_SZ, SGL_2048_CELL_SZ);
            sgl_rect_set_color(g_tile_r[r][c], SGL_2048_C_EMPTY);
            sgl_obj_set_radius(g_tile_r[r][c], SGL_2048_CELL_R);
            sgl_obj_set_border_width(g_tile_r[r][c], 0);
            sgl_obj_set_hidden(g_tile_r[r][c]);

            g_tile_l[r][c] = sgl_label_create(g_tile_r[r][c]);
            sgl_obj_set_pos(g_tile_l[r][c], 0, 0);
            sgl_obj_set_size(g_tile_l[r][c], SGL_2048_CELL_SZ, SGL_2048_CELL_SZ);
            sgl_label_set_font(g_tile_l[r][c], tile_font);
            sgl_label_set_text(g_tile_l[r][c], "");
            sgl_label_set_text_color(g_tile_l[r][c], SGL_2048_C_TXT_D);
            sgl_label_set_text_align(g_tile_l[r][c], SGL_ALIGN_CENTER);
            sgl_obj_set_hidden(g_tile_l[r][c]);
        }
    }

    /* game-over overlay (hidden) */
    g_ov_bg = sgl_rect_create(parent);
    sgl_obj_set_pos(g_ov_bg, g_margin_x, g_margin_y);
    sgl_obj_set_size(g_ov_bg, SGL_2048_GRID_PX, SGL_2048_GRID_PX);
    sgl_rect_set_color(g_ov_bg, SGL_COLOR_BLACK);
    sgl_rect_set_main_alpha(g_ov_bg, 160);
    sgl_obj_set_radius(g_ov_bg, SGL_2048_GRID_BG_R);
    sgl_obj_set_border_width(g_ov_bg, 0);
    sgl_obj_set_hidden(g_ov_bg);

    g_ov_txt = sgl_label_create(g_ov_bg);
    sgl_obj_set_pos(g_ov_txt, 0, 0);
    sgl_obj_set_size(g_ov_txt, SGL_2048_GRID_PX, SGL_2048_GRID_PX);
    sgl_label_set_font(g_ov_txt, title_font);
    sgl_label_set_text(g_ov_txt, "Game Over!");
    sgl_label_set_text_color(g_ov_txt, SGL_COLOR_WHITE);
    sgl_label_set_text_align(g_ov_txt, SGL_ALIGN_CENTER);
    sgl_obj_set_hidden(g_ov_txt);

    /* events */
    sgl_obj_set_event_cb(parent, event_cb, NULL);
    sgl_obj_set_event_cb(g_ov_bg, event_cb, NULL);
    new_game();
}

/**
 * @brief Destroy the game: release any leftover temporary animation widgets.
 * @param none
 * @return none
 */
void sgl_game2048_destroy(void)
{
    int i;

    for (i = 0; i < SGL_2048_MAX_MOVES; i++) {
        if (g_amov_r[i]) { sgl_obj_delete(g_amov_r[i]); g_amov_r[i] = NULL; }
        g_amov_l[i] = NULL;
    }

    for (i = 0; i < SGL_2048_MAX_POPS; i++) {
        if (g_pop_r[i]) { sgl_obj_delete(g_pop_r[i]); g_pop_r[i] = NULL; }
        g_pop_l[i] = NULL;
    }
    g_pop_cnt = 0;
}
