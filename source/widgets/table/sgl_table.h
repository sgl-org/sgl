/* source/widgets/table/sgl_table.h
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

#ifndef __SGL_TABLE_H__
#define __SGL_TABLE_H__

#include <sgl_core.h>
#include <sgl_draw.h>
#include <sgl_math.h>
#include <sgl_log.h>
#include <sgl_mm.h>
#include <sgl_theme.h>
#include <sgl_cfgfix.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Upper bound of the column count, it sizes the per column settings only,
 * the row count is unlimited. */
#ifndef SGL_TABLE_COL_MAX
#define SGL_TABLE_COL_MAX               (8)
#endif

/* Default horizontal padding between a cell border and its text, in pixels. */
#ifndef SGL_TABLE_CELL_PAD
#define SGL_TABLE_CELL_PAD              (4)
#endif

/* Default vertical padding of a row, added on top of the font height. */
#ifndef SGL_TABLE_ROW_PAD
#define SGL_TABLE_ROW_PAD               (3)
#endif

/**
 * @brief cell alignment inside its column
 */
typedef enum sgl_table_cell_align {
    SGL_TABLE_ALIGN_LEFT = 0,
    SGL_TABLE_ALIGN_CENTER,
    SGL_TABLE_ALIGN_RIGHT,
    SGL_TABLE_ALIGN_NUM,
} sgl_table_cell_align_t;

/**
 * @brief cell text provider
 * @param table table object
 * @param row row index, the header row included
 * @param col column index
 * @param user_data pointer passed to sgl_table_set_cell_cb()
 * @return cell text, NULL or "" draws an empty cell
 * @note called for the visible cells only, on every redraw, so the returned
 *       string may be a formatted temporary buffer as long as it stays alive
 *       until the function returns
 */
typedef const char* (*sgl_table_cell_cb)(sgl_obj_t *table, uint16_t row, uint16_t col,
                                         void *user_data);

/**
 * @brief sgl table struct
 * @obj: sgl general object
 * @sc: vertical scroll state, drag with inertia plus the scroll bar
 * @font: font of every cell
 * @cells: row major cell texts, used when cell_cb is NULL
 * @cell_cb: cell text provider of a dynamic data source, may be NULL
 * @user_data: opaque pointer handed to cell_cb
 * @col_w: width of each column, 0 means share the leftover width
 * @col_align: sgl_table_cell_align_t of each column
 * @bg_color: background of the body
 * @header_color: background of the header band
 * @line_color: grid line color
 * @border_color: color of the object border
 * @text_color: cell text color
 * @sel_color: background of the selected row
 * @sel_text_color: cell text color of the selected row
 * @row_h: row height in pixels, 0 to derive it from the font
 * @rows: number of rows, the header row included
 * @cols: number of columns, never above SGL_TABLE_COL_MAX
 * @sel_row: selected row, -1 selects none
 * @sel_col: column of the last clicked cell
 * @pad: horizontal cell padding
 * @line_w: grid line width
 * @alpha: alpha of the backgrounds, the texts and the lines
 * @header: 1 when row 0 is a sticky header
 * @zebra: 1 to tint every odd row, keeps wide tables readable
 */
typedef struct sgl_table {
    sgl_obj_t             obj;
    sgl_scroll_t          sc;
    const sgl_font_t      *font;
    const char *const     *cells;
    sgl_table_cell_cb     cell_cb;
    void                  *user_data;
    sgl_color_t           bg_color;
    sgl_color_t           header_color;
    sgl_color_t           line_color;
    sgl_color_t           border_color;
    sgl_color_t           text_color;
    sgl_color_t           sel_color;
    sgl_color_t           sel_text_color;
    uint16_t              col_w[SGL_TABLE_COL_MAX];
    uint16_t              row_h;
    uint16_t              rows;
    int16_t               sel_row;
    int16_t               sel_col;
    uint8_t               cols;
    uint8_t               col_align[SGL_TABLE_COL_MAX];
    uint8_t               alpha;
    uint8_t               pad;
    uint8_t               line_w;
    uint8_t               header : 1;
    uint8_t               zebra : 1;
} sgl_table_t;

/**
 * NOTE: the widget updates its selection before the user event callback runs,
 * so a click handler only has to read where the click landed:
 *
 * void table_click_cb(sgl_event_t *e)
 * {
 *     sgl_obj_t *table = e->obj;
 *
 *     if (e->type == SGL_EVENT_CLICKED) {
 *         SGL_LOG_INFO("row %d col %d: %s",
 *                      sgl_table_get_selected_row(table),
 *                      sgl_table_get_selected_col(table),
 *                      sgl_table_get_cell_text(table, sgl_table_get_selected_row(table),
 *                                              sgl_table_get_selected_col(table)));
 *     }
 * }
 *
 * sgl_obj_set_event_cb(table, table_click_cb, NULL);
 */

/**
 * @brief create a table object
 * @param parent parent of the table
 * @return table object
 */
sgl_obj_t* sgl_table_create(sgl_obj_t* parent);

/**
 * @brief set the number of rows
 * @param obj table object
 * @param rows row count including the header row, 0 clears the table
 * @return none
 * @note the selection is dropped when it falls outside the new row count
 */
void sgl_table_set_row_num(sgl_obj_t *obj, uint16_t rows);

/**
 * @brief set the number of columns
 * @param obj table object
 * @param cols column count, capped at SGL_TABLE_COL_MAX
 * @return none
 */
void sgl_table_set_col_num(sgl_obj_t *obj, uint8_t cols);

/**
 * @brief hand over a static cell text array
 * @param obj table object
 * @param cells row major texts, cells[row * cols + col], kept by pointer
 * @return none
 * @note the table never copies nor owns the strings, so they must outlive the
 *       widget, and the array must hold rows * cols entries because the widget
 *       indexes it without bounds checking; this is the default data source
 *       while no cell callback is set
 */
void sgl_table_set_cells(sgl_obj_t *obj, const char *const *cells);

/**
 * @brief set the cell text provider of a dynamic data source
 * @param obj table object
 * @param cb provider called for every visible cell, NULL restores cells
 * @param user_data opaque pointer handed to cb
 * @return none
 */
void sgl_table_set_cell_cb(sgl_obj_t *obj, sgl_table_cell_cb cb, void *user_data);

/**
 * @brief get the text of a cell
 * @param obj table object
 * @param row row index
 * @param col column index
 * @return cell text, NULL when out of range or empty
 */
const char* sgl_table_get_cell_text(sgl_obj_t *obj, int16_t row, int16_t col);

/**
 * @brief set the width of a column
 * @param obj table object
 * @param col column index
 * @param width width in pixels, 0 to share what the fixed columns leave over
 * @return none
 * @note there is no horizontal scrolling, so keep the fixed widths inside the
 *       object width; the last column absorbs whatever is left
 */
void sgl_table_set_column_width(sgl_obj_t *obj, uint8_t col, uint16_t width);

/**
 * @brief set the alignment of a column
 * @param obj table object
 * @param col column index
 * @param align SGL_TABLE_ALIGN_LEFT, _CENTER or _RIGHT
 * @return none
 * @note right alignment is what makes a numeric column readable
 */
void sgl_table_set_column_align(sgl_obj_t *obj, uint8_t col, uint8_t align);

/**
 * @brief set whether row 0 is a header
 * @param obj table object
 * @param enable true to draw row 0 as a sticky header band
 * @return none
 * @note the header does not scroll and cannot be selected, so a table with a
 *       header shows rows - 1 data rows
 */
void sgl_table_set_header(sgl_obj_t *obj, bool enable);

/**
 * @brief set whether every odd row is tinted
 * @param obj table object
 * @param enable true for zebra striping
 * @return none
 */
void sgl_table_set_zebra(sgl_obj_t *obj, bool enable);

/**
 * @brief set the row height
 * @param obj table object
 * @param height height in pixels, 0 to use the font height plus the padding
 * @return none
 */
void sgl_table_set_row_height(sgl_obj_t *obj, uint16_t height);

/**
 * @brief set the cell text padding
 * @param obj table object
 * @param pad horizontal padding in pixels
 * @return none
 */
void sgl_table_set_cell_padding(sgl_obj_t *obj, uint8_t pad);

/**
 * @brief set the font of every cell
 * @param obj table object
 * @param font font
 * @return none
 */
void sgl_table_set_font(sgl_obj_t *obj, const sgl_font_t *font);

/**
 * @brief set the background color of the body
 * @param obj table object
 * @param color color
 * @return none
 */
void sgl_table_set_bg_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set the background color of the header band
 * @param obj table object
 * @param color color
 * @return none
 */
void sgl_table_set_header_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set the grid line color
 * @param obj table object
 * @param color color
 * @return none
 */
void sgl_table_set_line_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set the grid line width
 * @param obj table object
 * @param width width in pixels, 0 removes the grid
 * @return none
 */
void sgl_table_set_line_width(sgl_obj_t *obj, uint8_t width);

/**
 * @brief set the cell text color
 * @param obj table object
 * @param color color
 * @return none
 */
void sgl_table_set_text_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set the background color of the selected row
 * @param obj table object
 * @param color color
 * @return none
 */
void sgl_table_set_selected_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set the text color of the selected row
 * @param obj table object
 * @param color color
 * @return none
 */
void sgl_table_set_selected_text_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set the alpha of the table
 * @param obj table object
 * @param alpha alpha, SGL_ALPHA_MAX is opaque
 * @return none
 */
void sgl_table_set_alpha(sgl_obj_t *obj, uint8_t alpha);

/**
 * @brief set the border width of the table
 * @param obj table object
 * @param width border width in pixels
 * @return none
 */
void sgl_table_set_border_width(sgl_obj_t *obj, uint8_t width);

/**
 * @brief set the border color of the table
 * @param obj table object
 * @param color border color
 * @return none
 */
void sgl_table_set_border_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief select a row
 * @param obj table object
 * @param row row index, -1 clears the selection
 * @return none
 * @note the table scrolls to make the new selection visible
 */
void sgl_table_set_selected_row(sgl_obj_t *obj, int16_t row);

/**
 * @brief get the selected row
 * @param obj table object
 * @return row index, -1 when nothing is selected
 */
int16_t sgl_table_get_selected_row(sgl_obj_t *obj);

/**
 * @brief get the column of the last clicked cell
 * @param obj table object
 * @return column index, -1 when no cell was clicked yet
 */
int16_t sgl_table_get_selected_col(sgl_obj_t *obj);

/**
 * @brief scroll the table to bring a row to the top of the viewport
 * @param obj table object
 * @param row row index
 * @return none
 */
void sgl_table_scroll_to_row(sgl_obj_t *obj, uint16_t row);

/**
 * @brief get the height of one row
 * @param obj table object
 * @return row height in pixels
 * @note useful to size the object for an exact number of visible rows
 */
uint16_t sgl_table_get_row_height(sgl_obj_t *obj);

#ifdef __cplusplus
}
#endif

#endif /* __SGL_TABLE_H__ */
