/* examples/scope.c
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
 * Scope widget example (multi-channel oscilloscope-style waveform display):
 *  A single 2-channel scope with live-scrolling sine and square waves,
 *  demonstrated using a timer to append new samples continuously.
 *
 * Key features:
 *  • FIFO ring buffer model: each channel has [width] samples; appends shift left when full.
 *  • Multi-channel: two waveforms overlay on the same plot area with different colors.
 *  • Value range mapping: v_min/v_max scale the samples vertically.
 *
 * @note The sample buffer array stride MUST equal the plot width (cap = widget_width - 2*border).
 */

#define SCOPE_WIDTH   200           /* widget width, must match buffer column count */
#define SCOPE_HEIGHT  100           /* widget height */
#define SCOPE_BORDER  2             /* border width */
#define SCOPE_CAP     (SCOPE_WIDTH - 2 * SCOPE_BORDER)  /* =196, the ring capacity */
#define SCOPE_CH      2             /* number of channels */
#define TIMER_PERIOD  30            /* ms between data points */

/* number of full waveform cycles shown across the whole plot width */
#define SCOPE_CYCLES  4
/* phase advance per sample in degrees (sgl_sinf takes degrees, 360 = one cycle) */
#define SCOPE_DEG_STEP  (360.0f * SCOPE_CYCLES / SCOPE_CAP)

static int16_t s_scope_buf[SCOPE_CH][SCOPE_CAP];
static sgl_color_t s_scope_colors[SCOPE_CH] = {SGL_COLOR_GREEN, SGL_COLOR_YELLOW};
static sgl_obj_t *g_scope = NULL;
static sgl_timer_t *g_scope_timer = NULL;
static float s_phase_deg = 0.0f;  /* 0..360 degrees, wraps once per cycle */

/**
 * Timer callback: generate next sample for each channel and append to scope.
 * ch0: sine wave (-32767..32767), ch1: square wave (+/- 20000).
 * NOTE: sgl_sinf() takes an angle in DEGREES and returns -1.0..1.0.
 */
static void scope_tick_cb(const sgl_timer_t *timer __attribute__((unused)), void *user_data __attribute__((unused)))
{
    if (!g_scope) return;

    /* sine: full int16 amplitude; sgl_sinf expects degrees, returns -1.0..1.0 */
    int16_t sine_val = (int16_t)(32767.0f * sgl_sinf(s_phase_deg));
    /* square: high on the first half cycle, low on the second half */
    int16_t square_val = (s_phase_deg < 180.0f) ? 20000 : -20000;

    sgl_scope_append_data(g_scope, 0, sine_val);
    sgl_scope_append_data(g_scope, 1, square_val);

    s_phase_deg += SCOPE_DEG_STEP;
    if (s_phase_deg >= 360.0f) s_phase_deg -= 360.0f;
}

/**
 * @brief create the scope example
 * @param parent parent object, NULL creates the scope on the active screen
 * @return none
 */
void sgl_scope_examples(sgl_obj_t *parent)
{
    sgl_obj_t *scope;
    sgl_timer_t *timer;

    /* Create the scope widget and immediately bind buffers so construct_cb
     * can render waveforms on first draw pass. */
    scope = sgl_scope_create(parent);
    if (scope == NULL) return;
    g_scope = scope;
    
    /* Configure geometry first */
    sgl_obj_set_pos(scope, 20, 60);
    sgl_obj_set_size(scope, SCOPE_WIDTH, SCOPE_HEIGHT);
    sgl_scope_set_border_width(scope, SCOPE_BORDER);
    sgl_scope_set_vrange(scope, -32768, 32767);         /* full int16 range */
    sgl_scope_set_bg_color(scope, SGL_COLOR_BLACK);
    sgl_scope_set_grid_color(scope, SGL_COLOR_GRAY);
    sgl_scope_set_border_color(scope, SGL_COLOR_CYAN);
    sgl_scope_set_alpha(scope, 255);

    /* Bind buffers now - this ensures wave_buffers and channel_count are set
     * before the first construct_cb/draw happens. */
    sgl_scope_set_buffers(scope, (int16_t *)s_scope_buf, s_scope_colors, SCOPE_CH);

    /* Create a timer to feed new samples and make the waveform scroll live */
    timer = sgl_timer_create();
    if (timer != NULL) {
        sgl_timer_setup(timer, scope_tick_cb, TIMER_PERIOD, -1, NULL);
        g_scope_timer = timer;
    }
}
