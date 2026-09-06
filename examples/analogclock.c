/* examples/analogclock.c
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
 * AnalogClock widget examples with startup animation and live ticking:
 *  1. default theme clock with hour numbers
 *  2. dark face with colored hands
 *  3. numberless face, custom hand widths and hub
 *
 * On startup the hands sweep from 15 minutes before the current time up to the
 * current time over 200 ms (linear). Once the sweep finishes a 1-second timer
 * keeps the clocks ticking with the real system time.
 */

/* clocks driven by the shared animation / timer */
static sgl_obj_t *g_clocks[3] = {NULL, NULL, NULL};

/* how far ahead of the current time the sweep starts, in minutes */
#define ANALOGCLOCK_SWEEP_SPAN_MIN   (15)

/* second hand value used while the hands are sweeping */
static uint8_t g_sweep_sec = 0;

/**
 * @brief set all clocks to the given hour/minute, keeping the sweep second
 * @param total_min minutes since 00:00 (may exceed 24h, wraps inside set_time)
 * @return none
 */
static void analogclock_apply_sweep(int32_t total_min)
{
    uint8_t hour = (uint8_t)(total_min / 60);
    uint8_t min  = (uint8_t)(total_min % 60);

    for (int i = 0; i < 3; i++) {
        if (g_clocks[i] != NULL) {
            sgl_analogclock_set_time(g_clocks[i], hour, min, g_sweep_sec);
        }
    }
}

/**
 * @brief animation path callback, sweeps the hands up to the current time
 * @param anim animation object
 * @param value interpolated minutes since 00:00
 * @return none
 */
static void analogclock_anim_cb(sgl_anim_t *anim, int32_t value)
{
    (void)anim;
    analogclock_apply_sweep(value);
}

/**
 * @brief timer callback, refreshes every clock with the real system time
 * @param timer timer object (unused)
 * @param user_data user data (unused)
 * @return none
 */
static void analogclock_tick_cb(const sgl_timer_t *timer, void *user_data)
{
    time_t rawtime;
    struct tm *t;

    (void)timer;
    (void)user_data;

    rawtime = time(NULL);
    t = localtime(&rawtime);
    if (t == NULL) {
        return;
    }

    for (int i = 0; i < 3; i++) {
        if (g_clocks[i] != NULL) {
            sgl_analogclock_set_time(g_clocks[i], (uint8_t)t->tm_hour,
                                     (uint8_t)t->tm_min, (uint8_t)t->tm_sec);
        }
    }
}

/**
 * @brief animation finish callback, snaps to real time and starts the 1s timer
 * @param anim animation object
 * @return none
 */
static void analogclock_anim_finish_cb(sgl_anim_t *anim)
{
    sgl_timer_t *timer;

    (void)anim;

    /* show the exact current time immediately, then keep ticking every second */
    analogclock_tick_cb(NULL, NULL);

    timer = sgl_timer_create();
    if (timer != NULL) {
        sgl_timer_setup(timer, analogclock_tick_cb, 1000, -1, NULL);
    }
}

/**
 * @brief create the analog clock examples
 * @param parent parent object, NULL creates the clocks on the active screen
 * @return none
 */
void sgl_analogclock_examples(sgl_obj_t *parent)
{
    sgl_obj_t *clk;
    sgl_anim_t *anim;
    time_t rawtime;
    struct tm *timeinfo;
    int32_t curr_min;
    int32_t start_min;
    int32_t end_min;

    /* current system time is the sweep target */
    rawtime = time(NULL);
    timeinfo = localtime(&rawtime);
    if (timeinfo == NULL) {
        return;
    }
    g_sweep_sec = (uint8_t)timeinfo->tm_sec;

    /* Sweep the hands from 15 minutes before the current time up to the
     * current time. Keep both ends positive so the midnight wrap works. */
    curr_min = (int32_t)timeinfo->tm_hour * 60 + timeinfo->tm_min;
    start_min = curr_min - ANALOGCLOCK_SWEEP_SPAN_MIN;
    end_min   = curr_min;
    if (start_min < 0) {
        start_min += 24 * 60;
        end_min   += 24 * 60;
    }

    /* example 1: default theme clock with hour numbers */
    clk = sgl_analogclock_create(parent);
    sgl_obj_set_pos(clk, 0, 140);
    sgl_obj_set_size(clk, 200, 200);
    sgl_analogclock_set_font(clk, &consolas14);
    sgl_analogclock_set_time(clk, start_min / 60, start_min % 60, g_sweep_sec);   /* sweep start pose */
    g_clocks[0] = clk;

    /* example 2: dark face with colored hands */
    clk = sgl_analogclock_create(parent);
    sgl_obj_set_pos(clk, 300, 140);
    sgl_obj_set_size(clk, 200, 200);
    sgl_analogclock_set_font(clk, &consolas14);
    sgl_analogclock_set_bg_color(clk, sgl_rgb(20, 24, 32));
    sgl_analogclock_set_scale_color(clk, SGL_COLOR_WHITE);
    sgl_analogclock_set_text_color(clk, SGL_COLOR_WHITE);
    sgl_analogclock_set_hour_ptr_color(clk, SGL_COLOR_CYAN);
    sgl_analogclock_set_min_ptr_color(clk, SGL_COLOR_CYAN);
    sgl_analogclock_set_sec_ptr_color(clk, SGL_COLOR_RED_ORANGE);
    sgl_analogclock_set_time(clk, start_min / 60, start_min % 60, g_sweep_sec);
    g_clocks[1] = clk;

    /* example 3: numberless face, custom hand widths and hub */
    clk = sgl_analogclock_create(parent);
    sgl_obj_set_pos(clk, 600, 140);
    sgl_obj_set_size(clk, 200, 200);
    sgl_analogclock_set_font(clk, NULL);
    sgl_analogclock_set_bg_color(clk, SGL_COLOR_WHITE);
    sgl_analogclock_set_scale_color(clk, SGL_COLOR_BLACK);
    sgl_analogclock_set_hour_ptr_color(clk, SGL_COLOR_BLUE);
    sgl_analogclock_set_min_ptr_color(clk, SGL_COLOR_BLUE);
    sgl_analogclock_set_sec_ptr_color(clk, SGL_COLOR_RED);
    sgl_analogclock_set_scale_width(clk, 2);
    sgl_analogclock_set_hour_ptr_width(clk, 6);
    sgl_analogclock_set_min_ptr_width(clk, 4);
    sgl_analogclock_set_sec_ptr_width(clk, 1);
    sgl_analogclock_set_hub_radius(clk, 5);
    sgl_analogclock_set_time(clk, start_min / 60, start_min % 60, g_sweep_sec);
    g_clocks[2] = clk;

    anim = sgl_anim_create();
    if (anim != NULL) {
        sgl_anim_set_start_value(anim, start_min);
        sgl_anim_set_end_value(anim, end_min);
        sgl_anim_set_act_duration(anim, 200);   /* 200 ms */
        sgl_anim_set_path(anim, analogclock_anim_cb, SGL_ANIM_PATH_EASE_OUT);
        sgl_anim_set_finish_cb(anim, analogclock_anim_finish_cb);
        sgl_anim_set_auto_free(anim);
        sgl_anim_start(anim, SGL_ANIM_REPEAT_ONCE);
    }
}
