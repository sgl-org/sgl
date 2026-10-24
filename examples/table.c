/* examples/table.c
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

#include <sgl.h>

/**
 * Table widget examples:
 *  1. static cell array, sticky header, zebra rows, columns share the width
 *  2. fixed column widths, right aligned numbers, dark palette, no header
 *  3. callback data source, 24 rows that scroll with a drag and can be picked
 *
 * The demo is laid out for a 800x480 panel: two tables side by side on the
 * top half, one wide table below it and a status line at the very bottom.
 */

/* Rows of example 3, the sticky header included. */
#define TABLE_DEMO_ROW_NUM          (25)

/* Columns of example 3. */
#define TABLE_DEMO_COL_NUM          (5)

/* Rows a row major text array holds for the given column count. */
#define TABLE_ROW_NUM(cells, cols)  (uint16_t)(sizeof(cells) / sizeof((cells)[0]) / (cols))

/* Cell texts of example 1, row major: header first, then the data rows. */
static const char *const table_cells_basic[] = {
    "Task",       "Owner",      "State",
    "Bring-up",   "Lee",        "Done",
    "Clock tree", "Morales",    "Done",
    "SRAM ECC",   "Nakamura",   "Running",
    "Boot ROM",   "Okafor",     "Running",
    "Power tree", "Schaefer",   "Open",
};

/* Cell texts of example 2, a headerless register dump. */
static const char *const table_cells_regs[] = {
    "R0", "0x00000123", "RW",
    "R1", "0xDEADBEEF", "RW",
    "R2", "0x00000000", "RO",
    "R3", "0x40021000", "RO",
};

/* Labels of example 3, indexed by the data row, so the callback stays cheap. */
#define TABLE_DEMO_NAME_NUM         (24)
static const char *const table_names[TABLE_DEMO_NAME_NUM] = {
    "sensor", "filter", "mixer", "resample", "encoder",
    "player", "network", "storage", "display", "input",
    "audio", "video", "timer", "watchdog", "power",
    "thermal", "logger", "stats", "ipc", "loader",
    "alloc", "pool", "cache", "queue",
};

/* Text of the cell being drawn, one buffer is enough because the widget asks
 * for one cell at a time and copies nothing. */
static char table_cell_buf[24];

/* Status line below example 3, refreshed by the click callback. */
static sgl_obj_t *table_status_label;

/**
 * @brief cell text provider of example 3
 * @param table table object, unused here
 * @param row row index, 0 is the header
 * @param col column index
 * @param user_data unused
 * @return text of the cell
 */
static const char* table_cell_provider(sgl_obj_t *table, uint16_t row, uint16_t col,
                                       void *user_data)
{
    uint16_t index;

    (void)table;
    (void)user_data;

    if (row == 0) {
        switch (col) {
        case 0:  return "Thread";
        case 1:  return "State";
        case 2:  return "CPU%";
        case 3:  return "Stack";
        default: return "Uptime";
        }
    }

    index = (uint16_t)(row - 1);
    switch (col) {
    case 0:
        return table_names[index % TABLE_DEMO_NAME_NUM];

    case 1:
        return (index % 3 == 0) ? "Ready" : ((index % 3 == 1) ? "Blocked" : "Running");

    case 2:
        sgl_snprintf(table_cell_buf, sizeof(table_cell_buf), "%d.%d",
                     (int)(index % 37), (int)(index % 10));
        return table_cell_buf;

    case 3:
        sgl_snprintf(table_cell_buf, sizeof(table_cell_buf), "%04d",
                     (int)(512 + index * 64));
        return table_cell_buf;

    default:
        sgl_snprintf(table_cell_buf, sizeof(table_cell_buf), "%03d:%02d",
                     (int)(index * 7 % 999), (int)(index * 13 % 60));
        return table_cell_buf;
    }
}

/**
 * @brief click callback of example 3, reports the picked cell
 * @param e event structure, e->obj is the table
 * @return none
 * @note the widget has already updated its selection when this runs
 */
static void table_picked_cb(sgl_event_t *e)
{
    int16_t row;
    int16_t col;

    if (e->type != SGL_EVENT_CLICKED) {
        return;
    }

    row = sgl_table_get_selected_row(e->obj);
    col = sgl_table_get_selected_col(e->obj);

    if (table_status_label != NULL) {
        sgl_label_set_text_fmt_dynamic(table_status_label, "picked row %d col %d: %s",
                                       (int)row, (int)col,
                                       sgl_table_get_cell_text(e->obj, row, col));
    }
}

/**
 * @brief put a caption line above a table
 * @param parent parent of the caption, the same as the table's
 * @param x caption left edge
 * @param y caption top edge
 * @param width caption width
 * @param text caption text
 * @return the caption label
 */
static sgl_obj_t* table_caption(sgl_obj_t *parent, int16_t x, int16_t y,
                                int16_t width, const char *text)
{
    sgl_obj_t *label;

    label = sgl_label_create(parent);
    sgl_obj_set_pos(label, x, y);
    sgl_obj_set_size(label, width, 16);
    sgl_label_set_font(label, &consolas14);
    sgl_label_set_text_color(label, SGL_COLOR_GRAY);
    sgl_label_set_text(label, text);

    return label;
}

/**
 * @brief create the table examples
 * @param parent parent object, NULL creates the tables on the active screen
 * @return none
 */
void sgl_table_examples(sgl_obj_t *parent)
{
    sgl_obj_t *table;

    /* example 1: the shortest path to a table, a row major text array plus
     * the row and column counts. No column width is set, so the three
     * columns share the inner width equally. The object is as tall as its
     * content: a header row plus five data rows of 20px, borders included. */
    table = sgl_table_create(parent);
    sgl_obj_set_pos(table, 10, 40);
    sgl_obj_set_size(table, 380, 122);
    sgl_table_set_font(table, &consolas14);
    sgl_table_set_col_num(table, 3);
    sgl_table_set_row_num(table, TABLE_ROW_NUM(table_cells_basic, 3));
    sgl_table_set_cells(table, table_cells_basic);
    sgl_table_set_header(table, true);
    sgl_table_set_zebra(table, true);
    table_caption(parent, 14, 22, 370, "1. header + zebra, auto columns");

    /* example 2: fixed widths for a monospace dump, the address column being
     * the one that needs the room. The header is off, so every row takes
     * part in the scrolling and the grid keeps its default 1px lines. */
    table = sgl_table_create(parent);
    sgl_obj_set_pos(table, 410, 40);
    sgl_obj_set_size(table, 380, 82);
    sgl_table_set_font(table, &consolas14);
    sgl_table_set_col_num(table, 3);
    sgl_table_set_row_num(table, TABLE_ROW_NUM(table_cells_regs, 3));
    sgl_table_set_cells(table, table_cells_regs);
    sgl_table_set_column_width(table, 0, 44);
    sgl_table_set_column_width(table, 1, 108);
    sgl_table_set_column_align(table, 2, SGL_TABLE_ALIGN_RIGHT);
    sgl_table_set_bg_color(table, sgl_rgb(28, 30, 34));
    sgl_table_set_line_color(table, sgl_rgb(64, 68, 76));
    sgl_table_set_border_color(table, sgl_rgb(96, 100, 108));
    sgl_table_set_text_color(table, sgl_rgb(214, 220, 228));
    sgl_table_set_selected_color(table, sgl_rgb(24, 96, 148));
    table_caption(parent, 414, 22, 370, "2. fixed widths, dark, no header");

    /* example 3: data comes from a callback, so nothing is stored per cell
     * and the row count is only limited by the provider. Twenty four data
     * rows never fit the object, hence the drag to scroll and the bar. */
    table = sgl_table_create(parent);
    sgl_obj_set_pos(table, 10, 194);
    sgl_obj_set_size(table, 780, 246);
    sgl_table_set_font(table, &consolas14);
    sgl_table_set_col_num(table, TABLE_DEMO_COL_NUM);
    sgl_table_set_row_num(table, TABLE_DEMO_ROW_NUM);
    sgl_table_set_cell_cb(table, table_cell_provider, NULL);
    sgl_table_set_header(table, true);
    sgl_table_set_zebra(table, true);
    sgl_table_set_column_width(table, 0, 120);
    sgl_table_set_column_width(table, 1, 100);
    sgl_table_set_column_align(table, 2, SGL_TABLE_ALIGN_RIGHT);
    sgl_table_set_column_align(table, 3, SGL_TABLE_ALIGN_RIGHT);
    sgl_table_set_column_align(table, 4, SGL_TABLE_ALIGN_RIGHT);
    sgl_table_set_selected_color(table, sgl_rgb(20, 88, 158));
    sgl_obj_set_event_cb(table, table_picked_cb, NULL);
    table_caption(parent, 14, 176, 770,
                  "3. callback rows, drag to scroll, click a cell to select it");

    table_status_label = sgl_label_create(parent);
    sgl_obj_set_pos(table_status_label, 14, 446);
    sgl_obj_set_size(table_status_label, 770, 18);
    sgl_label_set_font(table_status_label, &consolas14);
    sgl_label_set_text_color(table_status_label, SGL_COLOR_WHITE);
    sgl_label_set_text(table_status_label, "picked row -1 col -1");

    /* start with a row selected, so the selection colors are visible without
     * having to click, and jump the viewport to prove it follows */
    sgl_table_set_selected_row(table, 9);
    sgl_table_scroll_to_row(table, 6);
}
