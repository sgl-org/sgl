/* examples/digitalclock.c
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
 * DigitalClock widget examples (HH:MM:SS split-flap display):
 *  1. large consolas32 clock with default dark cards, top center
 *  2. compact amber-on-graphite clock with a snappier 250 ms flip, top left
 *
 * On startup both clocks sit at 00:00:00, then the real system time is
 * applied once so every card flips into place; a fast poll timer keeps them
 * ticking, and the seconds cards flip again on every real second.
 *
 * The poll interval is 100 ms on purpose: the timer wheel quantizes any
 * interval >= 128 ms to a multiple of 128 ms, so a "1000 ms" timer actually
 * fires every 896 ms. That drift makes a new second get noticed up to ~900 ms
 * late, so its flip animation starts late (the clock looks like it freezes for
 * a beat every ~9 s). Polling at 100 ms stays in the exact short wheel and
 * triggers each flip within 100 ms of the real second boundary; set_time is a
 * no-op while the digits are unchanged, so the extra polls cost nothing.
 *
 * Screen is 800x480.
 */

/* clocks driven by the shared poll timer */
static sgl_obj_t *g_dclocks = NULL;

/**
 * @brief timer callback, refreshes both clocks with the real system time
 * @param timer timer object (unused)
 * @param user_data user data (unused)
 * @return none
 */
static void digitalclock_tick_cb(const sgl_timer_t *timer, void *user_data)
{
    time_t rawtime;
    struct tm *t;
    int i;

    (void)timer;
    (void)user_data;

    rawtime = time(NULL);
    t = localtime(&rawtime);
    if (t == NULL) {
        return;
    }

    if (g_dclocks != NULL) {
        sgl_digitalclock_set_time(g_dclocks, (uint8_t)t->tm_hour,
                                    (uint8_t)t->tm_min, (uint8_t)t->tm_sec);
    }
}

/**
 * @brief create the digital clock examples
 * @param parent parent object, NULL creates the clocks on the active screen
 * @return none
 */
void sgl_digitalclock_examples(sgl_obj_t *parent)
{
    sgl_obj_t *clk;
    sgl_timer_t *timer;

    /* example 1: large clock, default dark cards and white digits */
    clk = sgl_digitalclock_create(parent);
    sgl_obj_set_pos(clk, 240, 8);
    sgl_obj_set_size(clk, 320, 68);
    sgl_digitalclock_set_font(clk, &consolas32);
    sgl_digitalclock_set_flip_time(clk, 150);
    /* make the moving leaf stand out against the dark settled card */
    sgl_digitalclock_set_flip_color(clk, sgl_rgb(150, 154, 166));
    sgl_digitalclock_set_flip_text_color(clk, sgl_rgb(24, 24, 28));
    sgl_digitalclock_set_flip_contrast(clk, 255);
    g_dclocks = clk;

    /* first update flips every card from 00:00:00 to the current time */
    digitalclock_tick_cb(NULL, NULL);

    timer = sgl_timer_create();
    if (timer != NULL) {
        sgl_timer_setup(timer, digitalclock_tick_cb, 100, -1, NULL);
    }
}
