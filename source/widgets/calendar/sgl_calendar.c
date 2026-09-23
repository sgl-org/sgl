/* source/widgets/calendar/sgl_calendar.c
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

#include "sgl_calendar.h"

/* layout paddings (px) */
#define  SGL_CAL_HEAD_PAD        (4)   /* header bar vertical padding   */
#define  SGL_CAL_WEEK_PAD        (2)   /* week row vertical padding     */
#define  SGL_CAL_ARROW_PAD       (6)   /* arrow hit area horizontal pad */

/* week day names, starting from Monday (SGL_CALENDAR_WEEK_START = 1) */
static const char *sgl_cal_week_names[] = { "Mo", "Tu", "We", "Th", "Fr", "Sa", "Su" };

/**
 * @brief leap year test of the Gregorian calendar
 * @param year full year
 * @return non zero for a leap year
 */
static int sgl_cal_is_leap(int16_t year)
{
    return ((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0);
}

/**
 * @brief number of days of a month
 * @param year full year
 * @param month month (1-12)
 * @return day count (28-31)
 */
static uint8_t sgl_cal_days_in_month(int16_t year, uint8_t month)
{
    static const uint8_t days[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

    if (month == 2 && sgl_cal_is_leap(year))
        return 29;
    return days[month - 1];
}

/**
 * @brief day of week of the 1st day of a month (Sakamoto's algorithm)
 * @param year full year
 * @param month month (1-12)
 * @return day of week, 0 = Sunday .. 6 = Saturday
 */
static uint8_t sgl_cal_weekday_of_1st(int16_t year, uint8_t month)
{
    static const uint8_t t[] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };
    int16_t y = year;

    if (month < 3)
        y -= 1;
    return (uint8_t)((y + y / 4 - y / 100 + y / 400 + t[month - 1] + 1) % 7);
}

/**
 * @brief convert a day of week to a grid column honoring SGL_CALENDAR_WEEK_START
 * @param wday day of week, 0 = Sunday .. 6 = Saturday
 * @return column index (0-6)
 */
static uint8_t sgl_cal_wday_to_col(uint8_t wday)
{
    return (uint8_t)((wday + 7 - SGL_CALENDAR_WEEK_START) % 7);
}

/**
 * @brief compute the layout metrics from the current font
 * @param cal calendar widget
 * @param head_h output header bar height
 * @param week_h output week row height
 * @param cell_w output grid cell width
 * @param cell_h output grid cell height
 * @return none
 */
static void sgl_cal_metrics(const sgl_calendar_t *cal, int *head_h, int *week_h,
                            int *cell_w, int *cell_h)
{
    const int font_h = sgl_font_get_height(cal->font);
    const int w = cal->obj.coords.x2 - cal->obj.coords.x1 + 1 - 2 * cal->obj.border;
    const int h = cal->obj.coords.y2 - cal->obj.coords.y1 + 1 - 2 * cal->obj.border;

    *head_h = font_h + 2 * SGL_CAL_HEAD_PAD;
    *week_h = font_h + 2 * SGL_CAL_WEEK_PAD;
    *cell_w = w / 7;
    *cell_h = (h - *head_h - *week_h) / 6;
}

/**
 * @brief normalize a year/month pair into the valid range
 * @param year pointer to year
 * @param month pointer to month (1-12, may be out of range on entry)
 * @return none
 */
static void sgl_cal_normalize_month(int16_t *year, int8_t *month)
{
    while (*month < 1) {
        *month += 12;
        *year -= 1;
    }
    while (*month > 12) {
        *month -= 12;
        *year += 1;
    }
}

/**
 * @brief shift the displayed month by a signed delta
 * @param cal calendar widget
 * @param delta month delta (+1 next, -1 previous)
 * @return none
 */
static void sgl_cal_shift_month(sgl_calendar_t *cal, int8_t delta)
{
    int8_t month = (int8_t)cal->month + delta;
    int16_t year = cal->year;

    sgl_cal_normalize_month(&year, &month);
    cal->year = year;
    cal->month = (uint8_t)month;
    sgl_obj_set_dirty(&cal->obj);
}

/**
 * @brief map a grid cell index to a date
 * @param cal calendar widget
 * @param index cell index (0-41, row major)
 * @param year output year
 * @param month output month (1-12)
 * @param day output day of month
 * @return non zero when the date belongs to the displayed month
 */
static uint8_t sgl_cal_cell_to_date(const sgl_calendar_t *cal, uint8_t index,
                                    int16_t *year, uint8_t *month, uint8_t *day)
{
    const uint8_t first_col = sgl_cal_wday_to_col(sgl_cal_weekday_of_1st(cal->year, cal->month));
    const uint8_t dim = sgl_cal_days_in_month(cal->year, cal->month);
    int16_t d = (int16_t)index - first_col + 1;
    int16_t y = cal->year;
    int8_t m = (int8_t)cal->month;

    if (d < 1) {
        /* trailing days of the previous month */
        m -= 1;
        sgl_cal_normalize_month(&y, &m);
        d += sgl_cal_days_in_month(y, (uint8_t)m);
    }
    else if (d > dim) {
        /* leading days of the next month */
        d -= dim;
        m += 1;
        sgl_cal_normalize_month(&y, &m);
    }

    *year = y;
    *month = (uint8_t)m;
    *day = (uint8_t)d;
    return (uint8_t)(m == (int8_t)cal->month);
}

/**
 * @brief find the grid cell index of a date of the displayed month
 * @param cal calendar widget
 * @param day day of month of the displayed month
 * @return cell index (0-41)
 */
static uint8_t sgl_cal_date_to_cell(const sgl_calendar_t *cal, uint8_t day)
{
    const uint8_t first_col = sgl_cal_wday_to_col(sgl_cal_weekday_of_1st(cal->year, cal->month));
    return (uint8_t)(first_col + day - 1);
}

/**
 * @brief compute the screen area of one grid cell
 * @param cal calendar widget
 * @param index cell index (0-41)
 * @param cell output area
 * @return none
 */
static void sgl_cal_cell_area(const sgl_calendar_t *cal, uint8_t index, sgl_area_t *cell)
{
    const sgl_obj_t *obj = &cal->obj;
    int head_h, week_h, cell_w, cell_h;
    const uint8_t row = (uint8_t)(index / 7);
    const uint8_t col = (uint8_t)(index % 7);

    sgl_cal_metrics(cal, &head_h, &week_h, &cell_w, &cell_h);

    cell->x1 = obj->coords.x1 + obj->border + col * cell_w;
    cell->x2 = cell->x1 + cell_w - 1;
    cell->y1 = obj->coords.y1 + obj->border + head_h + week_h + row * cell_h;
    cell->y2 = cell->y1 + cell_h - 1;
}

/**
 * @brief draw one day number centered in its cell
 * @param cal calendar widget
 * @param surf drawing surface
 * @param index cell index (0-41)
 * @return none
 */
static void sgl_cal_draw_cell(sgl_calendar_t *cal, sgl_surf_t *surf, uint8_t index)
{
    sgl_obj_t *obj = &cal->obj;
    int16_t year;
    uint8_t month, day;
    uint8_t in_month;
    uint8_t selected, today;
    sgl_area_t cell;
    sgl_color_t color;
    char buf[4];
    int16_t cx, cy, radius;
    int16_t text_w, font_h;

    in_month = sgl_cal_cell_to_date(cal, index, &year, &month, &day);
    sgl_cal_cell_area(cal, index, &cell);

    selected = (uint8_t)(in_month && year == cal->sel_year &&
                         month == cal->sel_month && day == cal->sel_day);
    today = (uint8_t)(year == cal->today_year && month == cal->today_month &&
                      day == cal->today_day);

    cx = (int16_t)((cell.x1 + cell.x2) / 2);
    cy = (int16_t)((cell.y1 + cell.y2) / 2);
    radius = (int16_t)(sgl_min(cell.x2 - cell.x1 + 1, cell.y2 - cell.y1 + 1) / 2 - 2);
    if (radius < 2)
        radius = 2;

    /* cell background keeps the panel color so partial redraws stay clean */
    sgl_draw_fill_rect(surf, &obj->area, &cell, 0, cal->bg_color, cal->alpha);

    if (selected) {
        sgl_draw_fill_circle(surf, &obj->area, cx, cy, radius, cal->highlight_color, cal->alpha);
        color = cal->sel_text_color;
    }
    else {
        if (today) {
            sgl_draw_fill_circle_border(surf, &obj->area, cx, cy, radius,
                                        cal->highlight_color, 2, cal->alpha);
        }
        color = in_month ? cal->text_color : cal->other_month_color;
    }

    sgl_snprintf(buf, sizeof(buf), "%d", (int)day);
    text_w = (int16_t)sgl_font_get_string_width(buf, cal->font);
    font_h = sgl_font_get_height(cal->font);
    sgl_draw_string(surf, &obj->area, (int16_t)(cx - text_w / 2),
                    (int16_t)(cy - font_h / 2), buf, color, cal->alpha, cal->font);
}

/**
 * @brief draw the header bar (prev/next arrows and the year-month title)
 * @param cal calendar widget
 * @param surf drawing surface
 * @return none
 */
static void sgl_cal_draw_header(sgl_calendar_t *cal, sgl_surf_t *surf)
{
    sgl_obj_t *obj = &cal->obj;
    int head_h, week_h, cell_w, cell_h;
    const int16_t x1 = obj->coords.x1 + obj->border;
    const int16_t x2 = obj->coords.x2 - obj->border;
    const int16_t y1 = obj->coords.y1 + obj->border;
    char buf[24];
    int16_t text_w;
    int16_t aw, cy;
    sgl_area_t box;

    sgl_cal_metrics(cal, &head_h, &week_h, &cell_w, &cell_h);

    /* header background */
    box.x1 = x1;
    box.x2 = x2;
    box.y1 = y1;
    box.y2 = y1 + head_h - 1;
    sgl_draw_fill_rect(surf, &obj->area, &box, 0, cal->bg_color, cal->alpha);

    /* centered year-month title */
    sgl_snprintf(buf, sizeof(buf), "%d-%02d", (int)cal->year, (int)cal->month);
    text_w = (int16_t)sgl_font_get_string_width(buf, cal->font);
    sgl_draw_string(surf, &obj->area, (int16_t)((x1 + x2 - text_w) / 2),
                    (int16_t)(y1 + SGL_CAL_HEAD_PAD), buf, cal->text_color,
                    cal->alpha, cal->font);

    /* flat chevron arrows */
    aw = (int16_t)(head_h / 4);
    cy = (int16_t)(y1 + head_h / 2);

    /* left arrow "<" */
    sgl_draw_chevron_left(surf, &obj->area, (int16_t)(x1 + SGL_CAL_ARROW_PAD),
                          (int16_t)(cy - aw), aw, cal->highlight_color, 2, cal->alpha);

    /* right arrow ">" */
    sgl_draw_chevron_right(surf, &obj->area, (int16_t)(x2 - SGL_CAL_ARROW_PAD - aw),
                           (int16_t)(cy - aw), aw, cal->highlight_color, 2, cal->alpha);
}

/**
 * @brief draw the week day name row
 * @param cal calendar widget
 * @param surf drawing surface
 * @return none
 */
static void sgl_cal_draw_week_row(sgl_calendar_t *cal, sgl_surf_t *surf)
{
    sgl_obj_t *obj = &cal->obj;
    int head_h, week_h, cell_w, cell_h;
    const int16_t x1 = obj->coords.x1 + obj->border;
    const int16_t x2 = obj->coords.x2 - obj->border;
    const int16_t y1 = obj->coords.y1 + obj->border;
    uint8_t i;
    int16_t text_w;
    sgl_area_t box;

    sgl_cal_metrics(cal, &head_h, &week_h, &cell_w, &cell_h);

    box.x1 = x1;
    box.x2 = x2;
    box.y1 = y1 + head_h;
    box.y2 = box.y1 + week_h - 1;
    sgl_draw_fill_rect(surf, &obj->area, &box, 0, cal->bg_color, cal->alpha);

    for (i = 0; i < 7; i++) {
        const char *name = sgl_cal_week_names[i];
        text_w = (int16_t)sgl_font_get_string_width(name, cal->font);
        sgl_draw_string(surf, &obj->area,
                        (int16_t)(x1 + i * cell_w + (cell_w - text_w) / 2),
                        (int16_t)(box.y1 + SGL_CAL_WEEK_PAD),
                        name, cal->weekday_color, cal->alpha, cal->font);
    }
}

/**
 * @brief move the selection by a signed day delta, crossing months as needed
 * @param cal calendar widget
 * @param delta day delta (+/-1 or +/-7 from the arrow keys)
 * @return none
 */
static void sgl_cal_move_selection(sgl_calendar_t *cal, int8_t delta)
{
    sgl_obj_t *obj = &cal->obj;
    int16_t year = cal->sel_year;
    int8_t month = (int8_t)cal->sel_month;
    int16_t day = (int16_t)cal->sel_day + delta;
    uint8_t dim;
    uint8_t old_cell = 0, new_cell = 0;
    uint8_t same_month;
    sgl_area_t cell;

    if (cal->sel_day == 0)
        return;

    /* normalize the day into a valid date */
    while (day < 1) {
        month -= 1;
        sgl_cal_normalize_month(&year, &month);
        day += sgl_cal_days_in_month(year, (uint8_t)month);
    }
    dim = sgl_cal_days_in_month(year, (uint8_t)month);
    while (day > dim) {
        day -= dim;
        month += 1;
        sgl_cal_normalize_month(&year, &month);
        dim = sgl_cal_days_in_month(year, (uint8_t)month);
    }

    same_month = (uint8_t)(year == cal->year && month == (int8_t)cal->month);

    if (same_month) {
        /* minimal redraw: only the old and the new cell */
        old_cell = sgl_cal_date_to_cell(cal, cal->sel_day);
        new_cell = sgl_cal_date_to_cell(cal, (uint8_t)day);

        cal->sel_year = year;
        cal->sel_month = (uint8_t)month;
        cal->sel_day = (uint8_t)day;

        sgl_cal_cell_area(cal, old_cell, &cell);
        sgl_obj_update_area(&cell);
        sgl_cal_cell_area(cal, new_cell, &cell);
        sgl_obj_update_area(&cell);
    }
    else {
        /* crossed a month boundary: follow the selection with a full redraw */
        cal->sel_year = year;
        cal->sel_month = (uint8_t)month;
        cal->sel_day = (uint8_t)day;
        cal->year = year;
        cal->month = (uint8_t)month;
        sgl_obj_set_dirty(obj);
    }

    if (cal->on_date_changed) {
        cal->on_date_changed(obj, cal->sel_year, cal->sel_month, cal->sel_day);
    }
}

static void sgl_calendar_construct_cb(sgl_surf_t *surf, sgl_obj_t *obj, sgl_event_t *evt)
{
    sgl_calendar_t *cal = sgl_container_of(obj, sgl_calendar_t, obj);

    switch (evt->type) {
    case SGL_EVENT_DRAW_MAIN: {
        int head_h, week_h, cell_w, cell_h;
        uint8_t i;
        sgl_area_t box;

        sgl_cal_metrics(cal, &head_h, &week_h, &cell_w, &cell_h);

        /* grid background */
        box.x1 = obj->coords.x1 + obj->border;
        box.x2 = obj->coords.x2 - obj->border;
        box.y1 = obj->coords.y1 + obj->border + head_h + week_h;
        box.y2 = obj->coords.y2 - obj->border;
        sgl_draw_fill_rect(surf, &obj->area, &box, 0, cal->bg_color, cal->alpha);

        for (i = 0; i < 42; i++) {
            sgl_cal_draw_cell(cal, surf, i);
        }

        /* bars on top so overflowing cell pixels are covered */
        sgl_cal_draw_week_row(cal, surf);
        sgl_cal_draw_header(cal, surf);
    }
    break;

    case SGL_EVENT_CLICKED: {
        int head_h, week_h, cell_w, cell_h;
        const int16_t x1 = obj->coords.x1 + obj->border;
        const int16_t x2 = obj->coords.x2 - obj->border;
        const int16_t y1 = obj->coords.y1 + obj->border;
        int16_t col, row;
        uint8_t index;
        int16_t year;
        uint8_t month, day, in_month;

        /* key driven click (Enter) carries minimum coordinates */
        if (evt->pos.x == SGL_POS_MIN && evt->pos.y == SGL_POS_MIN) {
            if (cal->on_date_changed && cal->sel_day != 0) {
                cal->on_date_changed(obj, cal->sel_year, cal->sel_month, cal->sel_day);
            }
            break;
        }

        if (evt->pos.x < x1 || evt->pos.x > x2 || evt->pos.y < y1 || evt->pos.y > obj->coords.y2) {
            break;
        }

        sgl_cal_metrics(cal, &head_h, &week_h, &cell_w, &cell_h);

        /* header arrows */
        if (evt->pos.y < y1 + head_h) {
            const int16_t hit = (int16_t)(head_h / 2 + SGL_CAL_ARROW_PAD);

            if (evt->pos.x < x1 + hit) {
                sgl_cal_shift_month(cal, -1);
            }
            else if (evt->pos.x > x2 - hit) {
                sgl_cal_shift_month(cal, 1);
            }
            break;
        }

        /* week row: nothing to do */
        if (evt->pos.y < y1 + head_h + week_h) {
            break;
        }

        /* date grid hit test */
        col = (int16_t)((evt->pos.x - x1) / cell_w);
        row = (int16_t)((evt->pos.y - y1 - head_h - week_h) / cell_h);
        if (col < 0 || col > 6 || row < 0 || row > 5) {
            break;
        }

        index = (uint8_t)(row * 7 + col);
        in_month = sgl_cal_cell_to_date(cal, index, &year, &month, &day);

        if (!in_month) {
            /* tapping a trailing day flips to its month */
            cal->sel_year = year;
            cal->sel_month = month;
            cal->sel_day = day;
            cal->year = year;
            cal->month = month;
            sgl_obj_set_dirty(obj);
        }
        else {
            /* minimal redraw: old selection cell plus the new one */
            sgl_area_t cell;
            uint8_t old_sel = (uint8_t)(cal->sel_day != 0 &&
                                        cal->sel_year == cal->year &&
                                        cal->sel_month == cal->month);

            if (old_sel) {
                sgl_cal_cell_area(cal, sgl_cal_date_to_cell(cal, cal->sel_day), &cell);
                sgl_obj_update_area(&cell);
            }

            cal->sel_year = year;
            cal->sel_month = month;
            cal->sel_day = day;

            sgl_cal_cell_area(cal, index, &cell);
            sgl_obj_update_area(&cell);
        }

        if (cal->on_date_changed) {
            cal->on_date_changed(obj, cal->sel_year, cal->sel_month, cal->sel_day);
        }
    }
    break;

    case SGL_EVENT_KEY_LEFT:
        sgl_cal_move_selection(cal, -1);
        break;

    case SGL_EVENT_KEY_RIGHT:
        sgl_cal_move_selection(cal, 1);
        break;

    case SGL_EVENT_KEY_UP:
        sgl_cal_move_selection(cal, -7);
        break;

    case SGL_EVENT_KEY_DOWN:
        sgl_cal_move_selection(cal, 7);
        break;

    default:
        break;
    }
}

/**
 * @brief create a calendar widget
 * @param parent parent object, NULL creates it on the active screen
 * @return calendar object, NULL on failure
 */
sgl_obj_t* sgl_calendar_create(sgl_obj_t *parent)
{
    sgl_calendar_t *cal = sgl_malloc(sizeof(sgl_calendar_t));
    if (cal == NULL) {
        SGL_LOG_ERROR("sgl_calendar_create: malloc failed");
        return NULL;
    }

    /* set object all member to zero */
    memset(cal, 0, sizeof(sgl_calendar_t));

    sgl_obj_init(&cal->obj, parent);
    cal->obj.construct_fn = sgl_calendar_construct_cb;
    sgl_obj_set_clickable(&cal->obj);
    sgl_obj_set_editable(&cal->obj);

    cal->font = sgl_get_system_font();
    cal->alpha = SGL_THEME_ALPHA;
    cal->bg_color = SGL_THEME_COLOR;
    cal->text_color = SGL_THEME_TEXT_COLOR;
    cal->weekday_color = SGL_THEME_TEXT_COLOR;
    cal->other_month_color = sgl_rgb(170, 170, 170);
    cal->highlight_color = SGL_CALENDAR_HIGHLIGHT;
    cal->sel_text_color = sgl_rgb(255, 255, 255);

    /* no selection until sgl_calendar_set_date is called */
    cal->sel_day = 0;

    return &cal->obj;
}

/**
 * @brief set the selected date and show its month
 */
void sgl_calendar_set_date(sgl_obj_t *obj, int16_t year, uint8_t month, uint8_t day)
{
    sgl_calendar_t *cal = (sgl_calendar_t *)obj;

    if (month < 1 || month > 12)
        return;
    if (day < 1 || day > sgl_cal_days_in_month(year, month))
        return;

    cal->sel_year = year;
    cal->sel_month = month;
    cal->sel_day = day;
    cal->year = year;
    cal->month = month;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief get the selected date
 */
void sgl_calendar_get_date(sgl_obj_t *obj, int16_t *year, uint8_t *month, uint8_t *day)
{
    sgl_calendar_t *cal = (sgl_calendar_t *)obj;

    if (year)  *year = cal->sel_year;
    if (month) *month = cal->sel_month;
    if (day)   *day = cal->sel_day;
}

/**
 * @brief set the date highlighted as "today" (ring marker)
 */
void sgl_calendar_set_today(sgl_obj_t *obj, int16_t year, uint8_t month, uint8_t day)
{
    sgl_calendar_t *cal = (sgl_calendar_t *)obj;

    cal->today_year = year;
    cal->today_month = month;
    cal->today_day = day;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief show the given month without changing the selection
 */
void sgl_calendar_set_month(sgl_obj_t *obj, int16_t year, uint8_t month)
{
    sgl_calendar_t *cal = (sgl_calendar_t *)obj;

    if (month < 1 || month > 12)
        return;

    cal->year = year;
    cal->month = month;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the date changed callback
 */
void sgl_calendar_set_on_date_changed(sgl_obj_t *obj, sgl_calendar_cb_t cb)
{
    sgl_calendar_t *cal = (sgl_calendar_t *)obj;
    cal->on_date_changed = cb;
}

/**
 * @brief set the text font of the calendar
 */
void sgl_calendar_set_font(sgl_obj_t *obj, const sgl_font_t *font)
{
    sgl_calendar_t *cal = (sgl_calendar_t *)obj;
    cal->font = font;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the panel background color
 */
void sgl_calendar_set_bg_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_calendar_t *cal = (sgl_calendar_t *)obj;
    cal->bg_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the day number color of the current month
 */
void sgl_calendar_set_text_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_calendar_t *cal = (sgl_calendar_t *)obj;
    cal->text_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the week day header text color
 */
void sgl_calendar_set_weekday_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_calendar_t *cal = (sgl_calendar_t *)obj;
    cal->weekday_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the day number color of the trailing days from other months
 */
void sgl_calendar_set_other_month_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_calendar_t *cal = (sgl_calendar_t *)obj;
    cal->other_month_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the highlight color (selected day fill, today ring, arrows)
 */
void sgl_calendar_set_highlight_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_calendar_t *cal = (sgl_calendar_t *)obj;
    cal->highlight_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the text color on the selected day
 */
void sgl_calendar_set_sel_text_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_calendar_t *cal = (sgl_calendar_t *)obj;
    cal->sel_text_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the alpha of the calendar
 */
void sgl_calendar_set_alpha(sgl_obj_t *obj, uint8_t alpha)
{
    sgl_calendar_t *cal = (sgl_calendar_t *)obj;
    cal->alpha = alpha;
    sgl_obj_set_dirty(obj);
}
