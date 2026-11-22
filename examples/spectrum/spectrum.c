/* examples/spectrum.c
 *
 * MIT License
 *
 * Copyright(c) 2023-present All contributors of SGL
 * Document reference link: docs directory
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
 * Spectrum widget examples (screen 800x480, one row of four panels):
 *  1. vertical gradient bars, cyan -> blue
 *  2. flat solid bars, magenta
 *  3. segmented LED / block mode with vertical gradient, green
 *  4. left-to-right gradient bars (HGRADIENT), amber -> red
 *
 * All four are driven by one 50 ms timer with a pseudo-music envelope:
 * a slow sine envelope over the row plus random jitter, so the bars dance
 * like an audio analyser. Each panel keeps its own random phase so they
 * do not move in lockstep.
 */

#define SPEC_PERIOD_MS  10              /* 50 FPS is plenty for bars */

#define SPEC_BARS_GRAD  16              /* gradient panel bar count  */
#define SPEC_BARS_FLAT  16              /* flat panel bar count      */
#define SPEC_BARS_LED   12              /* LED panel bar count       */
#define SPEC_BARS_ROUND 12              /* flat panel bar count      */

static const int g_bars[4] = { SPEC_BARS_GRAD, SPEC_BARS_FLAT, SPEC_BARS_LED, SPEC_BARS_ROUND };

static sgl_obj_t *g_spec[4];            /* the four panels           */
static uint16_t   g_phase[4];           /* per-panel envelope phase  */
static sgl_timer_t *g_tick;             /* shared animation timer    */

/**
 * @brief pseudo-random bar value with a sine envelope across the row
 * @param p panel index
 * @param i bar index inside the panel
 * @param h usable bar height in px
 * @return bar value in px, 4..h
 */
static uint16_t spec_value(int p, int i, int h)
{
    /* one full 360 deg wave across the row, drifting with the phase, so
     * every frame shows a mix of tall and short bars (audio-analyser look) */
    int bars = g_bars[p];
    int16_t angle = (int16_t)(g_phase[p] + i * 360 / bars);
    int32_t env = sgl_sin(angle);                       /* Q15 -32768..32767 */
    int32_t v = h * 45 / 100 + ((env * (h * 45 / 100)) >> 15) + sgl_rand() % (h / 8);

    if (v < 4) {
        v = 4;
    }
    if (v > h) {
        v = h;
    }
    return (uint16_t)v;
}

/**
 * @brief advance every panel by one frame
 */
static void spectrum_tick_cb(const sgl_timer_t *timer, void *user_data)
{
    SGL_UNUSED(timer);
    SGL_UNUSED(user_data);

    for (int p = 0; p < 4; p++) {
        if (g_spec[p] == NULL) {
            continue;
        }
        g_phase[p] = (uint16_t)((g_phase[p] + 7) % 360);
        int bars = g_bars[p];
        int h = sgl_obj_get_height(g_spec[p]);
        for (int i = 0; i < bars; i++) {
            sgl_spectrum_set_bar_value(g_spec[p], (uint16_t)i, spec_value(p, i, h));
        }
    }
}

/**
 * @brief create the spectrum examples
 * @param parent parent object, NULL creates the panels on the active screen
 * @return none
 */
void sgl_spectrum_examples(sgl_obj_t *parent)
{
    const int16_t w   = 170;            /* panel width  */
    const int16_t h   = 120;            /* panel height */
    const int16_t gap = (800 - 4 * w) / 5;

    /* ---- 1. gradient bars ---- */
    g_spec[0] = sgl_spectrum_create(parent);
    sgl_obj_set_pos(g_spec[0], gap, 180);
    sgl_obj_set_size(g_spec[0], w, h);
    sgl_spectrum_set_bar_number(g_spec[0], SPEC_BARS_GRAD);
    sgl_spectrum_set_bar_mode(g_spec[0], SGL_SPECTRUM_MODE_BAR | SGL_SPECTRUM_MODE_GRADIENT);
    sgl_spectrum_set_bar_color(g_spec[0], sgl_rgb(0, 200, 255));
    sgl_spectrum_set_bar_color_low(g_spec[0], sgl_rgb(0, 40, 180));
    sgl_spectrum_set_alpha(g_spec[0], 255);

    /* ---- 2. flat bars ---- */
    g_spec[1] = sgl_spectrum_create(parent);
    sgl_obj_set_pos(g_spec[1], gap + w + gap, 180);
    sgl_obj_set_size(g_spec[1], w, h);
    sgl_spectrum_set_bar_number(g_spec[1], SPEC_BARS_FLAT);
    sgl_spectrum_set_bar_mode(g_spec[1], SGL_SPECTRUM_MODE_BAR);
    sgl_spectrum_set_bar_color(g_spec[1], sgl_rgb(255, 60, 160));
    sgl_spectrum_set_alpha(g_spec[1], 255);

    /* ---- 3. segmented LED block mode with gradient ---- */
    g_spec[2] = sgl_spectrum_create(parent);
    sgl_obj_set_pos(g_spec[2], gap + 2 * (w + gap), 180);
    sgl_obj_set_size(g_spec[2], w, h);
    sgl_spectrum_set_bar_number(g_spec[2], SPEC_BARS_LED);
    sgl_spectrum_set_bar_mode(g_spec[2], SGL_SPECTRUM_MODE_BLOCK | SGL_SPECTRUM_MODE_GRADIENT);
    sgl_spectrum_set_bar_color(g_spec[2], sgl_rgb(120, 255, 140));
    sgl_spectrum_set_bar_color_low(g_spec[2], sgl_rgb(0, 120, 0));
    sgl_spectrum_set_bar_hat_height(g_spec[2], 4);
    sgl_spectrum_set_alpha(g_spec[2], 255);

    /* ---- 4. left-to-right gradient bars ---- */
    g_spec[3] = sgl_spectrum_create(parent);
    sgl_obj_set_pos(g_spec[3], gap + 3 * (w + gap), 180);
    sgl_obj_set_size(g_spec[3], w, h);
    sgl_spectrum_set_bar_number(g_spec[3], SPEC_BARS_ROUND);
    sgl_spectrum_set_bar_mode(g_spec[3], SGL_SPECTRUM_MODE_BAR | SGL_SPECTRUM_MODE_HGRADIENT);
    sgl_spectrum_set_bar_color(g_spec[3], sgl_rgb(255, 180, 60));
    sgl_spectrum_set_bar_color_low(g_spec[3], sgl_rgb(200, 40, 20));
    sgl_spectrum_set_alpha(g_spec[3], 255);

    /* seed the values once so the first painted frame is already alive */
    for (int p = 0; p < 4; p++) {
        for (int i = 0; i < g_bars[p]; i++) {
            sgl_spectrum_set_bar_value(g_spec[p], (uint16_t)i, spec_value(p, i, h));
        }
    }

    /* one shared 30 FPS timer drives all four panels */
    g_tick = sgl_timer_create();
    if (g_tick != NULL) {
        sgl_timer_setup(g_tick, spectrum_tick_cb, SPEC_PERIOD_MS, -1, NULL);
    }
}
