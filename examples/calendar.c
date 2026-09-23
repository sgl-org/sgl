/* examples/calendar.c
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
#include <time.h>

/**
 * Calendar widget examples (modern flat month view):
 *  1. default theme calendar set to the current system date
 *  2. dark custom themed calendar
 *
 * Click a day to select it, click the header arrows to flip months, or use
 * the arrow keys to move the selection across months. The picked date is
 * reported through the on_date_changed callback.
 */

/**
 * @brief date changed callback, logs the picked date
 * @param obj calendar widget object
 * @param year selected year
 * @param month selected month (1-12)
 * @param day selected day of month
 * @return none
 */
static void calendar_date_changed_cb(sgl_obj_t *obj, int16_t year, uint8_t month, uint8_t day)
{
    (void)obj;
    SGL_LOG_INFO("calendar: picked %04d-%02d-%02d", (int)year, (int)month, (int)day);
}

/**
 * @brief create the calendar examples
 * @param parent parent object, NULL creates the calendars on the active screen
 * @return none
 */
void sgl_calendar_examples(sgl_obj_t *parent)
{
    sgl_obj_t *cal;
    time_t rawtime;
    struct tm *now;
    int16_t year;
    uint8_t month, day;

    /* current system date as the default selection and today marker */
    rawtime = time(NULL);
    now = localtime(&rawtime);
    if (now == NULL) {
        return;
    }
    year = (int16_t)(now->tm_year + 1900);
    month = (uint8_t)(now->tm_mon + 1);
    day = (uint8_t)now->tm_mday;

    /* example 1: default theme calendar */
    cal = sgl_calendar_create(parent);
    sgl_obj_set_pos(cal, 40, 30);
    sgl_obj_set_size(cal, 280, 260);
    sgl_calendar_set_font(cal, &consolas14);
    sgl_calendar_set_today(cal, year, month, day);
    sgl_calendar_set_date(cal, year, month, day);
    sgl_calendar_set_on_date_changed(cal, calendar_date_changed_cb);

    /* example 2: dark custom themed calendar */
    cal = sgl_calendar_create(parent);
    sgl_obj_set_pos(cal, 400, 30);
    sgl_obj_set_size(cal, 280, 260);
    sgl_calendar_set_font(cal, &consolas14);
    sgl_calendar_set_bg_color(cal, sgl_rgb(30, 34, 42));
    sgl_calendar_set_text_color(cal, sgl_rgb(230, 230, 230));
    sgl_calendar_set_weekday_color(cal, sgl_rgb(140, 150, 165));
    sgl_calendar_set_other_month_color(cal, sgl_rgb(90, 96, 106));
    sgl_calendar_set_highlight_color(cal, sgl_rgb(66, 165, 245));
    sgl_calendar_set_sel_text_color(cal, sgl_rgb(255, 255, 255));
    sgl_calendar_set_today(cal, year, month, day);
    sgl_calendar_set_date(cal, year, month, day);
    sgl_calendar_set_on_date_changed(cal, calendar_date_changed_cb);
}
