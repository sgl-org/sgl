/* source/widgets/calendar/sgl_calendar.h
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

#ifndef __SGL_CALENDAR_H__
#define __SGL_CALENDAR_H__

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

/* first column of the week row: 0 = Sunday, 1 = Monday (default) */
#ifndef SGL_CALENDAR_WEEK_START
#define SGL_CALENDAR_WEEK_START    (1)
#endif

/* modern flat highlight color (material blue) */
#define SGL_CALENDAR_HIGHLIGHT     sgl_rgb(0, 120, 215)

/**
 * @brief date changed callback
 * @param obj calendar widget object
 * @param year selected year
 * @param month selected month (1-12)
 * @param day selected day of month (1-31)
 */
typedef void (*sgl_calendar_cb_t)(sgl_obj_t *obj, int16_t year, uint8_t month, uint8_t day);

/**
 * @brief sgl calendar widget (month view, flat modern style)
 */
typedef struct sgl_calendar {
    sgl_obj_t        obj;
    const sgl_font_t *font;

    int16_t          year;          /* year of the displayed month        */
    uint8_t          month;         /* displayed month (1-12)             */

    int16_t          sel_year;      /* selected date                      */
    uint8_t         sel_month;
    uint8_t         sel_day;

    int16_t          today_year;    /* today highlighted with a ring      */
    uint8_t         today_month;
    uint8_t         today_day;

    sgl_color_t      bg_color;          /* panel background               */
    sgl_color_t      text_color;        /* day numbers of current month   */
    sgl_color_t      weekday_color;     /* week day header text           */
    sgl_color_t      other_month_color; /* trailing days of other months  */
    sgl_color_t      highlight_color;   /* selection / today accent       */
    sgl_color_t      sel_text_color;    /* text on the selected day       */
    uint8_t          alpha;

    sgl_calendar_cb_t on_date_changed;  /* selection change notification  */
} sgl_calendar_t;

/**
 * @brief create a calendar widget
 * @param parent parent object, NULL creates it on the active screen
 * @return calendar object, NULL on failure
 */
sgl_obj_t* sgl_calendar_create(sgl_obj_t *parent);

/**
 * @brief set the selected date and show its month
 * @param obj calendar object
 * @param year year (e.g. 2026)
 * @param month month (1-12)
 * @param day day of month (1-31)
 * @return none
 */
void sgl_calendar_set_date(sgl_obj_t *obj, int16_t year, uint8_t month, uint8_t day);

/**
 * @brief get the selected date
 * @param obj calendar object
 * @param year output year, may be NULL
 * @param month output month (1-12), may be NULL
 * @param day output day of month, may be NULL
 * @return none
 */
void sgl_calendar_get_date(sgl_obj_t *obj, int16_t *year, uint8_t *month, uint8_t *day);

/**
 * @brief set the date highlighted as "today" (ring marker)
 * @param obj calendar object
 * @param year year
 * @param month month (1-12)
 * @param day day of month
 * @return none
 */
void sgl_calendar_set_today(sgl_obj_t *obj, int16_t year, uint8_t month, uint8_t day);

/**
 * @brief show the given month without changing the selection
 * @param obj calendar object
 * @param year year
 * @param month month (1-12)
 * @return none
 */
void sgl_calendar_set_month(sgl_obj_t *obj, int16_t year, uint8_t month);

/**
 * @brief set the date changed callback
 * @param obj calendar object
 * @param cb callback invoked when the user picks a date, NULL disables
 * @return none
 */
void sgl_calendar_set_on_date_changed(sgl_obj_t *obj, sgl_calendar_cb_t cb);

/**
 * @brief set the text font of the calendar
 * @param obj calendar object
 * @param font font of header, week row and day numbers
 * @return none
 */
void sgl_calendar_set_font(sgl_obj_t *obj, const sgl_font_t *font);

/**
 * @brief set the panel background color
 * @param obj calendar object
 * @param color background color
 * @return none
 */
void sgl_calendar_set_bg_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set the day number color of the current month
 * @param obj calendar object
 * @param color text color
 * @return none
 */
void sgl_calendar_set_text_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set the week day header text color
 * @param obj calendar object
 * @param color text color
 * @return none
 */
void sgl_calendar_set_weekday_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set the day number color of the trailing days from other months
 * @param obj calendar object
 * @param color text color
 * @return none
 */
void sgl_calendar_set_other_month_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set the highlight color (selected day fill, today ring, arrows)
 * @param obj calendar object
 * @param color highlight color
 * @return none
 */
void sgl_calendar_set_highlight_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set the text color on the selected day
 * @param obj calendar object
 * @param color text color
 * @return none
 */
void sgl_calendar_set_sel_text_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set the alpha of the calendar
 * @param obj calendar object
 * @param alpha alpha value
 * @return none
 */
void sgl_calendar_set_alpha(sgl_obj_t *obj, uint8_t alpha);

#ifdef __cplusplus
}
#endif

#endif // !__SGL_CALENDAR_H__
