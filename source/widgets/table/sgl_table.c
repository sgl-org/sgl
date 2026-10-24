/* source/widgets/table/sgl_table.c
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
#include "sgl_table.h"

/* Zebra tint: how much of the line color is mixed into an odd row. */
#define SGL_TABLE_ZEBRA_MIX             (28)

/**
 * @brief text of one cell, from the callback or from the static array
 * @param tbl table
 * @param row row index
 * @param col column index
 * @return cell text, NULL when there is no data source
 */
static const char* table_cell_text(sgl_table_t *tbl, uint16_t row, uint16_t col)
{
    if (tbl->cell_cb != NULL) {
        return tbl->cell_cb(&tbl->obj, row, col, tbl->user_data);
    }

    if (tbl->cells != NULL && tbl->cols > 0) {
        return tbl->cells[(size_t)row * tbl->cols + col];
    }

    return NULL;
}

/**
 * @brief height of one row
 * @param tbl table
 * @return row height in pixels
 */
static uint16_t table_row_height(sgl_table_t *tbl)
{
    if (tbl->row_h) {
        return tbl->row_h;
    }

    return (uint16_t)(sgl_font_get_height(tbl->font) + 2 * SGL_TABLE_ROW_PAD);
}

/**
 * @brief index of the first row that takes part in the scrolling
 * @param tbl table
 * @return 1 with a header, 0 without
 */
static uint16_t table_first_row(sgl_table_t *tbl)
{
    return tbl->header ? 1 : 0;
}

/**
 * @brief height of the scrolling viewport
 * @param tbl table
 * @return viewport height in pixels, the header band excluded
 */
static int16_t table_view_height(sgl_table_t *tbl)
{
    sgl_area_t in_bg = sgl_obj_get_fill_rect(&tbl->obj);

    return (int16_t)(in_bg.y2 - in_bg.y1 + 1 - table_row_height(tbl) * (tbl->header ? 1 : 0));
}

/**
 * @brief largest scroll offset of the table
 * @param tbl table
 * @return scroll range in pixels, 0 when every row already fits
 */
static int32_t table_max_scroll(sgl_table_t *tbl)
{
    int32_t content = (int32_t)(tbl->rows - table_first_row(tbl)) * table_row_height(tbl);

    return sgl_max(0, content - table_view_height(tbl));
}

/**
 * @brief compute the column boundaries of the table
 * @param tbl table
 * @param width inner width the columns are laid out in
 * @param edges receives width + 1 left edge offsets, relative to the inner left
 * @return none
 * @note columns without an explicit width share what the fixed ones leave
 *       over, and the last column absorbs the rounding remainder so the grid
 *       always ends on the inner border
 */
static void table_column_edges(sgl_table_t *tbl, int16_t width, int16_t *edges)
{
    int32_t fixed = 0;
    int32_t auto_w = 0;
    int32_t x = 0;
    uint8_t auto_num = 0;
    uint8_t i;

    for (i = 0; i < tbl->cols; i++) {
        if (tbl->col_w[i]) {
            fixed += tbl->col_w[i];
        }
        else {
            auto_num ++;
        }
    }

    if (auto_num) {
        auto_w = ((int32_t)width - fixed) / auto_num;
        if (auto_w < 1) {
            auto_w = 1;
        }
    }

    for (i = 0; i <= tbl->cols; i++) {
        edges[i] = (int16_t)x;
        if (i < tbl->cols) {
            x += tbl->col_w[i] ? tbl->col_w[i] : (int16_t)auto_w;
        }
    }

    if (x <= width) {
        edges[tbl->cols] = width;
    }
}

/**
 * @brief draw the text of one cell
 * @param surf surface to draw on
 * @param tbl table
 * @param clip clipping area, reused for the cell box
 * @param x1 left edge of the cell
 * @param y1 top edge of the cell
 * @param cell_w width of the cell
 * @param row_h height of the row
 * @param font_h height of the font
 * @param row row index
 * @param col column index
 * @param color text color
 * @param y_min top of the area the text may be painted in
 * @return none
 * @note a text wider than its column is clipped, not ellipsized, because the
 *       cell texts are owned by the application. y_min keeps the first
 *       partially scrolled row from writing into the header band
 */
static void table_draw_cell(sgl_surf_t *surf, sgl_table_t *tbl, sgl_area_t *clip,
                            int16_t x1, int16_t y1, int16_t cell_w, uint16_t row_h,
                            int16_t font_h, uint16_t row, uint16_t col, sgl_color_t color,
                            int16_t y_min)
{
    const char *text = table_cell_text(tbl, row, col);
    int16_t text_w;
    int16_t x;

    if (text == NULL || text[0] == '\0' || cell_w <= 0) {
        return;
    }

    text_w = (int16_t)sgl_font_get_string_width(text, tbl->font);
    switch ((sgl_table_cell_align_t)tbl->col_align[col]) {
    case SGL_TABLE_ALIGN_RIGHT:
        x = x1 + cell_w - tbl->pad - text_w;
        break;

    case SGL_TABLE_ALIGN_CENTER:
        x = x1 + (cell_w - text_w) / 2;
        break;

    default:
        x = x1 + tbl->pad;
        break;
    }

    clip->x1 = x1;
    clip->y1 = (y1 > y_min) ? y1 : y_min;
    clip->x2 = x1 + cell_w - 1;
    clip->y2 = y1 + (int16_t)row_h - 1;
    if (clip->y1 > clip->y2) {
        return;
    }
    sgl_draw_string(surf, clip, x, y1 + ((int16_t)row_h - font_h) / 2, text, color,
                    tbl->alpha, tbl->font);
}

static void sgl_table_scroll_commit(sgl_scroll_t *sc)
{
    sgl_table_t *tbl = sgl_container_of(sc, sgl_table_t, sc);

    sgl_scroll_bar_wake(sc);
    sgl_obj_set_dirty(&tbl->obj);
}

/**
 * @brief scroll the minimum distance that brings a row into the viewport
 * @param tbl table
 * @param row row index
 * @return none
 */
static void table_ensure_visible(sgl_table_t *tbl, int16_t row)
{
    int32_t max_scroll = table_max_scroll(tbl);
    int32_t top = (int32_t)(row - table_first_row(tbl)) * table_row_height(tbl);
    int32_t bottom = top + table_row_height(tbl);
    int16_t view_h = table_view_height(tbl);

    if (top < tbl->sc.offset) {
        tbl->sc.offset = top;
    }
    else if (bottom > tbl->sc.offset + view_h) {
        tbl->sc.offset = bottom - view_h;
    }

    tbl->sc.offset = sgl_max(0, sgl_min(tbl->sc.offset, max_scroll));
}

static void sgl_table_construct_cb(sgl_surf_t *surf, sgl_obj_t *obj, sgl_event_t *evt)
{
    sgl_table_t *tbl = sgl_container_of(obj, sgl_table_t, obj);
    sgl_draw_rect_t bg_desc = {
        .alpha = tbl->alpha,
        .color = tbl->bg_color,
        .border = obj->border,
        .border_alpha = tbl->alpha,
        .border_color = tbl->border_color,
        .radius = obj->radius,
        .pixmap = NULL,
    };
    int16_t edges[SGL_TABLE_COL_MAX + 1];
    uint16_t row;
    uint16_t col;
    uint8_t i;
    int16_t in_w, view_y1, view_y2, font_h, x, y;
    int16_t header_h;
    int16_t start_y;
    uint16_t start_row;
    int32_t max_scroll;
    sgl_area_t in_bg;
    sgl_area_t box;

    if (unlikely(evt->type != SGL_EVENT_DRAW_MAIN)) {
        switch (evt->type) {
        case SGL_EVENT_PRESSED:
            sgl_scroll_press(&tbl->sc, evt->pos.y);
            sgl_scroll_bar_wake(&tbl->sc);
            break;

        case SGL_EVENT_MOVE_UP:
        case SGL_EVENT_MOVE_DOWN:
            if (sgl_scroll_stay(&tbl->sc, evt->pos.y, table_max_scroll(tbl))) {
                sgl_obj_set_dirty(obj);
            }
            break;

        case SGL_EVENT_RELEASED:
            max_scroll = table_max_scroll(tbl);
            tbl->sc.range = max_scroll;
            tbl->sc.commit = sgl_table_scroll_commit;
            if (sgl_scroll_release(&tbl->sc, max_scroll)) {
                sgl_scroll_anim_start(&tbl->sc);
            }
            sgl_obj_set_dirty(obj);
            break;

        case SGL_EVENT_CLICKED: {
            uint16_t height;

            /* a click that ends a drag is a scroll, not a cell pick */
            if (tbl->sc.dragged) {
                break;
            }
            in_bg = sgl_obj_get_fill_rect(obj);
            height = table_row_height(tbl);
            y = (int16_t)(evt->pos.y - (int16_t)(in_bg.y1 + (tbl->header ? height : 0)));
            if (y < 0 || (int32_t)y + tbl->sc.offset < 0 || tbl->rows <= table_first_row(tbl)) {
                break;      /* the header band and the rubber band gap are not selectable */
            }
            row = (uint16_t)(table_first_row(tbl) + (y + tbl->sc.offset) / height);
            if (row >= tbl->rows) {
                break;
            }

            table_column_edges(tbl, (int16_t)(in_bg.x2 - in_bg.x1 + 1), edges);
            x = (int16_t)(evt->pos.x - in_bg.x1);
            col = tbl->cols - 1;
            for (i = 0; i < tbl->cols; i++) {
                if (x < edges[i + 1]) {
                    col = i;
                    break;
                }
            }

            tbl->sel_row = (int16_t)row;
            tbl->sel_col = (int16_t)col;
            table_ensure_visible(tbl, (int16_t)row);
            sgl_obj_set_dirty(obj);
            break;
        }

        case SGL_EVENT_KEY_DOWN:
            if (tbl->sel_row < (int16_t)(tbl->rows - 1)) {
                tbl->sel_row ++;
            }
            else if (tbl->sel_row < 0) {
                tbl->sel_row = table_first_row(tbl);
            }
            table_ensure_visible(tbl, tbl->sel_row);
            sgl_obj_set_dirty(obj);
            break;

        case SGL_EVENT_KEY_UP:
            if (tbl->sel_row > (int16_t)table_first_row(tbl)) {
                tbl->sel_row --;
            }
            table_ensure_visible(tbl, tbl->sel_row);
            sgl_obj_set_dirty(obj);
            break;

        case SGL_EVENT_DESTROYED:
            sgl_scroll_anim_stop(&tbl->sc);
            break;

        default:
            break;
        }

        return;
    }

    sgl_draw_rect(surf, &obj->area, &obj->coords, &bg_desc);

    if (tbl->cols == 0 || tbl->rows == 0) {
        return;
    }

    in_bg = sgl_obj_get_fill_rect(obj);
    in_w = (int16_t)(in_bg.x2 - in_bg.x1 + 1);
    font_h = sgl_font_get_height(tbl->font);
    header_h = (int16_t)(tbl->header ? table_row_height(tbl) : 0);
    view_y1 = (int16_t)(in_bg.y1 + header_h);
    view_y2 = in_bg.y2;
    max_scroll = table_max_scroll(tbl);
    table_column_edges(tbl, in_w, edges);

    if (tbl->sc.offset < 0) {
        start_row = table_first_row(tbl);
        start_y = (int16_t)(view_y1 - tbl->sc.offset);
    }
    else {
        start_row = (uint16_t)(table_first_row(tbl) + tbl->sc.offset / table_row_height(tbl));
        start_y = (int16_t)(view_y1 - (int16_t)(tbl->sc.offset % table_row_height(tbl)));
    }

    row = start_row;
    y = start_y;
    while (row < tbl->rows && y < view_y2) {
        box.x1 = in_bg.x1;
        box.x2 = in_bg.x2;
        box.y1 = y;
        box.y2 = (int16_t)(y + table_row_height(tbl) - 1);
        if ((int16_t)row == tbl->sel_row) {
            sgl_draw_fill_rect(surf, &obj->area, &box, 0, tbl->sel_color, tbl->alpha);
        }
        else if (tbl->zebra && (((row - table_first_row(tbl)) & 1) != 0)) {
            sgl_draw_fill_rect(surf, &obj->area, &box, 0,
                               sgl_color_mixer(tbl->line_color, tbl->bg_color, SGL_TABLE_ZEBRA_MIX),
                               tbl->alpha);
        }
        y = (int16_t)(y + table_row_height(tbl));
        row ++;
    }

    if (header_h) {
        box.x1 = in_bg.x1;
        box.x2 = in_bg.x2;
        box.y1 = in_bg.y1;
        box.y2 = (int16_t)(in_bg.y1 + header_h - 1);
        sgl_draw_fill_rect(surf, &obj->area, &box, 0, tbl->header_color, tbl->alpha);
    }

    if (tbl->line_w) {
        for (i = 1; i < tbl->cols; i++) {
            box.x1 = (int16_t)(in_bg.x1 + edges[i] - (tbl->line_w / 2));
            box.x2 = (int16_t)(box.x1 + tbl->line_w - 1);
            box.y1 = in_bg.y1;
            box.y2 = in_bg.y2;
            sgl_draw_fill_rect(surf, &obj->area, &box, 0, tbl->line_color, tbl->alpha);
        }

        y = start_y;
        for (row = table_first_row(tbl); row <= tbl->rows && y < view_y2; row++) {
            if (y >= view_y1) {
                box.x1 = in_bg.x1;
                box.x2 = in_bg.x2;
                box.y1 = y;
                box.y2 = (int16_t)(y + tbl->line_w - 1);
                sgl_draw_fill_rect(surf, &obj->area, &box, 0, tbl->line_color, tbl->alpha);
            }
            y = (int16_t)(y + table_row_height(tbl));
        }
    }

    row = start_row;
    y = start_y;
    while (row < tbl->rows && y < view_y2) {
        for (col = 0; col < tbl->cols; col++) {
            table_draw_cell(surf, tbl, &box, (int16_t)(in_bg.x1 + edges[col]), y,
                            (int16_t)(edges[col + 1] - edges[col]), table_row_height(tbl),
                            font_h, row, col,
                            ((int16_t)row == tbl->sel_row) ? tbl->sel_text_color : tbl->text_color,
                            view_y1);
        }
        y = (int16_t)(y + table_row_height(tbl));
        row ++;
    }

    if (header_h) {
        for (col = 0; col < tbl->cols; col++) {
            table_draw_cell(surf, tbl, &box, (int16_t)(in_bg.x1 + edges[col]), in_bg.y1,
                            (int16_t)(edges[col + 1] - edges[col]), (uint16_t)header_h,
                            font_h, 0, col, tbl->text_color, in_bg.y1);
        }
    }

    box.x1 = in_bg.x1;
    box.x2 = in_bg.x2;
    box.y1 = view_y1;
    box.y2 = view_y2;
    sgl_scroll_draw_bar(surf, obj, &tbl->sc, max_scroll, &box,
                        sgl_color_invert(tbl->bg_color));
}

/**
 * @brief create a table object
 * @param parent parent of the table
 * @return pointer to the table object
 */
sgl_obj_t* sgl_table_create(sgl_obj_t* parent)
{
    sgl_table_t *tbl = sgl_malloc(sizeof(sgl_table_t));
    sgl_obj_t *obj;

    if (tbl == NULL) {
        SGL_LOG_ERROR("sgl_table_create: malloc failed");
        return NULL;
    }

    /* set object all member to zero */
    memset(tbl, 0, sizeof(sgl_table_t));

    obj = &tbl->obj;
    sgl_obj_init(obj, parent);
    obj->construct_fn = sgl_table_construct_cb;
    sgl_obj_set_clickable(obj);
    sgl_obj_set_movable(obj);
    sgl_obj_set_editable(obj);
    sgl_obj_set_border_width(obj, 1);

    tbl->font = sgl_get_system_font();
    tbl->alpha = SGL_THEME_ALPHA;
    tbl->bg_color = SGL_THEME_COLOR;
    tbl->header_color = sgl_color_mixer(SGL_THEME_BORDER_COLOR, SGL_THEME_COLOR, 48);
    tbl->line_color = sgl_color_mixer(SGL_THEME_BORDER_COLOR, SGL_THEME_COLOR, 128);
    tbl->border_color = SGL_THEME_BORDER_COLOR;
    tbl->text_color = SGL_THEME_TEXT_COLOR;
    tbl->sel_color = sgl_color_mixer(SGL_THEME_COLOR, SGL_THEME_BORDER_COLOR, 128);
    tbl->sel_text_color = sgl_color_invert(SGL_THEME_TEXT_COLOR);
    tbl->pad = SGL_TABLE_CELL_PAD;
    tbl->line_w = 1;
    tbl->sel_row = -1;
    tbl->sel_col = -1;
    sgl_scroll_reset(&tbl->sc);

    return obj;
}

/**
 * @brief set the number of rows
 * @param obj table object
 * @param rows row count including the header row
 * @return none
 */
void sgl_table_set_row_num(sgl_obj_t *obj, uint16_t rows)
{
    sgl_table_t *tbl = sgl_container_of(obj, sgl_table_t, obj);

    tbl->rows = rows;
    if (tbl->sel_row >= (int16_t)rows) {
        tbl->sel_row = -1;
        tbl->sel_col = -1;
    }
    if (tbl->sc.offset > table_max_scroll(tbl)) {
        tbl->sc.offset = table_max_scroll(tbl);
    }
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the number of columns
 * @param obj table object
 * @param cols column count, capped at SGL_TABLE_COL_MAX
 * @return none
 */
void sgl_table_set_col_num(sgl_obj_t *obj, uint8_t cols)
{
    sgl_table_t *tbl = sgl_container_of(obj, sgl_table_t, obj);

    if (cols > SGL_TABLE_COL_MAX) {
        SGL_LOG_ERROR("sgl_table: %d columns exceeds SGL_TABLE_COL_MAX", cols);
        cols = SGL_TABLE_COL_MAX;
    }

    tbl->cols = cols;
    if (tbl->sel_col >= (int16_t)cols) {
        tbl->sel_col = -1;
    }
    sgl_obj_set_dirty(obj);
}

/**
 * @brief hand over a static cell text array
 * @param obj table object
 * @param cells row major texts, cells[row * cols + col]
 * @return none
 */
void sgl_table_set_cells(sgl_obj_t *obj, const char *const *cells)
{
    sgl_table_t *tbl = sgl_container_of(obj, sgl_table_t, obj);

    tbl->cells = cells;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the cell text provider
 * @param obj table object
 * @param cb provider, NULL falls back to the cell array
 * @param user_data opaque pointer handed to cb
 * @return none
 */
void sgl_table_set_cell_cb(sgl_obj_t *obj, sgl_table_cell_cb cb, void *user_data)
{
    sgl_table_t *tbl = sgl_container_of(obj, sgl_table_t, obj);

    tbl->cell_cb = cb;
    tbl->user_data = user_data;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief get the text of a cell
 * @param obj table object
 * @param row row index
 * @param col column index
 * @return cell text, NULL when out of range or when there is no data source
 */
const char* sgl_table_get_cell_text(sgl_obj_t *obj, int16_t row, int16_t col)
{
    sgl_table_t *tbl = sgl_container_of(obj, sgl_table_t, obj);

    if (row < 0 || col < 0 || row >= (int16_t)tbl->rows || col >= (int16_t)tbl->cols) {
        return NULL;
    }

    return table_cell_text(tbl, (uint16_t)row, (uint16_t)col);
}

/**
 * @brief set the width of a column
 * @param obj table object
 * @param col column index
 * @param width width in pixels, 0 to share the leftover width
 * @return none
 */
void sgl_table_set_column_width(sgl_obj_t *obj, uint8_t col, uint16_t width)
{
    sgl_table_t *tbl = sgl_container_of(obj, sgl_table_t, obj);

    if (col >= SGL_TABLE_COL_MAX) {
        return;
    }

    tbl->col_w[col] = width;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the alignment of a column
 * @param obj table object
 * @param col column index
 * @param align SGL_TABLE_ALIGN_LEFT, _CENTER or _RIGHT
 * @return none
 */
void sgl_table_set_column_align(sgl_obj_t *obj, uint8_t col, uint8_t align)
{
    sgl_table_t *tbl = sgl_container_of(obj, sgl_table_t, obj);

    if (col >= SGL_TABLE_COL_MAX || align >= SGL_TABLE_ALIGN_NUM) {
        return;
    }

    tbl->col_align[col] = align;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set whether row 0 is a sticky header
 * @param obj table object
 * @param enable true to enable the header
 * @return none
 */
void sgl_table_set_header(sgl_obj_t *obj, bool enable)
{
    sgl_table_t *tbl = sgl_container_of(obj, sgl_table_t, obj);

    tbl->header = enable ? 1 : 0;
    if (tbl->header && tbl->sel_row == 0) {
        tbl->sel_row = -1;      /* the header cannot be selected */
    }
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set whether every odd row is tinted
 * @param obj table object
 * @param enable true for zebra striping
 * @return none
 */
void sgl_table_set_zebra(sgl_obj_t *obj, bool enable)
{
    sgl_table_t *tbl = sgl_container_of(obj, sgl_table_t, obj);

    tbl->zebra = enable ? 1 : 0;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the row height
 * @param obj table object
 * @param height height in pixels, 0 to derive it from the font
 * @return none
 */
void sgl_table_set_row_height(sgl_obj_t *obj, uint16_t height)
{
    sgl_table_t *tbl = sgl_container_of(obj, sgl_table_t, obj);

    tbl->row_h = height;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the cell text padding
 * @param obj table object
 * @param pad horizontal padding in pixels
 * @return none
 */
void sgl_table_set_cell_padding(sgl_obj_t *obj, uint8_t pad)
{
    sgl_table_t *tbl = sgl_container_of(obj, sgl_table_t, obj);

    tbl->pad = pad;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the font of every cell
 * @param obj table object
 * @param font font
 * @return none
 */
void sgl_table_set_font(sgl_obj_t *obj, const sgl_font_t *font)
{
    sgl_table_t *tbl = sgl_container_of(obj, sgl_table_t, obj);

    tbl->font = font;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the background color of the body
 * @param obj table object
 * @param color color
 * @return none
 */
void sgl_table_set_bg_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_table_t *tbl = sgl_container_of(obj, sgl_table_t, obj);

    tbl->bg_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the background color of the header band
 * @param obj table object
 * @param color color
 * @return none
 */
void sgl_table_set_header_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_table_t *tbl = sgl_container_of(obj, sgl_table_t, obj);

    tbl->header_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the grid line color
 * @param obj table object
 * @param color color
 * @return none
 */
void sgl_table_set_line_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_table_t *tbl = sgl_container_of(obj, sgl_table_t, obj);

    tbl->line_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the grid line width
 * @param obj table object
 * @param width width in pixels, 0 removes the grid
 * @return none
 */
void sgl_table_set_line_width(sgl_obj_t *obj, uint8_t width)
{
    sgl_table_t *tbl = sgl_container_of(obj, sgl_table_t, obj);

    tbl->line_w = width;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the cell text color
 * @param obj table object
 * @param color color
 * @return none
 */
void sgl_table_set_text_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_table_t *tbl = sgl_container_of(obj, sgl_table_t, obj);

    tbl->text_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the background color of the selected row
 * @param obj table object
 * @param color color
 * @return none
 */
void sgl_table_set_selected_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_table_t *tbl = sgl_container_of(obj, sgl_table_t, obj);

    tbl->sel_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the text color of the selected row
 * @param obj table object
 * @param color color
 * @return none
 */
void sgl_table_set_selected_text_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_table_t *tbl = sgl_container_of(obj, sgl_table_t, obj);

    tbl->sel_text_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the alpha of the table
 * @param obj table object
 * @param alpha alpha, SGL_ALPHA_MAX is opaque
 * @return none
 */
void sgl_table_set_alpha(sgl_obj_t *obj, uint8_t alpha)
{
    sgl_table_t *tbl = sgl_container_of(obj, sgl_table_t, obj);

    tbl->alpha = alpha;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the border width of the table
 * @param obj table object
 * @param width border width in pixels
 * @return none
 */
void sgl_table_set_border_width(sgl_obj_t *obj, uint8_t width)
{
    sgl_obj_set_border_width(obj, width);
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the border color of the table
 * @param obj table object
 * @param color border color
 * @return none
 */
void sgl_table_set_border_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_table_t *tbl = sgl_container_of(obj, sgl_table_t, obj);

    tbl->border_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief select a row
 * @param obj table object
 * @param row row index, -1 clears the selection
 * @return none
 */
void sgl_table_set_selected_row(sgl_obj_t *obj, int16_t row)
{
    sgl_table_t *tbl = sgl_container_of(obj, sgl_table_t, obj);

    if (row >= (int16_t)tbl->rows || (row >= 0 && row < (int16_t)table_first_row(tbl))) {
        return;
    }

    tbl->sel_row = row;
    if (tbl->sel_col < 0 && row >= 0) {
        tbl->sel_col = 0;
    }
    if (row >= 0) {
        table_ensure_visible(tbl, row);
    }
    sgl_obj_set_dirty(obj);
}

/**
 * @brief get the selected row
 * @param obj table object
 * @return row index, -1 when nothing is selected
 */
int16_t sgl_table_get_selected_row(sgl_obj_t *obj)
{
    sgl_table_t *tbl = sgl_container_of(obj, sgl_table_t, obj);

    return tbl->sel_row;
}

/**
 * @brief get the column of the last clicked cell
 * @param obj table object
 * @return column index, -1 when no cell was clicked yet
 */
int16_t sgl_table_get_selected_col(sgl_obj_t *obj)
{
    sgl_table_t *tbl = sgl_container_of(obj, sgl_table_t, obj);

    return tbl->sel_col;
}

/**
 * @brief scroll a row to the top of the viewport
 * @param obj table object
 * @param row row index
 * @return none
 */
void sgl_table_scroll_to_row(sgl_obj_t *obj, uint16_t row)
{
    sgl_table_t *tbl = sgl_container_of(obj, sgl_table_t, obj);
    int32_t offset = (int32_t)(row - table_first_row(tbl)) * table_row_height(tbl);

    if (offset < 0) {
        offset = 0;
    }

    tbl->sc.offset = sgl_min(offset, table_max_scroll(tbl));
    sgl_scroll_bar_wake(&tbl->sc);
    sgl_obj_set_dirty(obj);
}

/**
 * @brief get the height of one row
 * @param obj table object
 * @return row height in pixels
 */
uint16_t sgl_table_get_row_height(sgl_obj_t *obj)
{
    sgl_table_t *tbl = sgl_container_of(obj, sgl_table_t, obj);

    return table_row_height(tbl);
}
