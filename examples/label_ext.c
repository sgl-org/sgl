/* examples/label_ext.c
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
 * label_ext widget examples (rotation focused):
 *  1. every 45-degree angle of the same string, laid out in a grid - shows
 *     text stays centered in its cell at any angle, no black boxes, no drift
 *  2. 90-degree fast path (vertical text) with background rect
 *  3. continuously rotating label driven by an animation (0 -> 360 loop)
 *  4. dynamic text update at 30 degrees (formatted buffer with live counter)
 *
 * Screen is 800x480.
 */

/* rotating label of example 3, driven by the shared animation */
static sgl_obj_t *g_spin = NULL;

/* formatted buffer of example 4 */
static char g_counter_buf[32] = {0};

/**
 * @brief animation path callback, drives the continuous rotation
 * @param anim animation object
 * @param value interpolated angle in degrees [0, 360]
 * @return none
 */
static void label_ext_spin_anim_cb(sgl_anim_t *anim, int32_t value)
{
    (void)anim;
    if (g_spin != NULL) {
        sgl_label_ext_set_text_rotation(g_spin, (int16_t)value);
    }
}

/**
 * @brief timer callback, refreshes the live counter text
 * @param timer timer object
 * @param user_data pointer to the label object
 * @return none
 */
static void label_ext_counter_cb(const sgl_timer_t *timer, void *user_data)
{
    static int count = 0;
    sgl_obj_t *label = (sgl_obj_t *)user_data;

    if (label != NULL) {
        /* sgl_vsnprintf only supports %d (no %u) */
        sgl_label_ext_set_text_fmt(label, "count %d", ++count);
    }
}

/**
 * @brief create the label_ext examples
 * @param parent parent object, NULL creates the labels on the active screen
 * @return none
 */
void sgl_label_ext_examples(sgl_obj_t *parent)
{
    sgl_obj_t *label;
    sgl_anim_t *anim;
    sgl_timer_t *tmr;
    static const int16_t angles[8] = { 0, 45, 90, 135, 180, 225, 270, 315 };

    /* example 1: one label per 45 degrees, centered in each cell */
    for (int i = 0; i < 8; i++) {
        const int col = i % 4;
        const int row = i / 4;

        /* faint cell frame to visualize the layout box each label rotates in
         * NOTE: keep created BEFORE the label_ext so the ext label paints on top */
        label = sgl_label_create(parent);
        sgl_obj_set_pos(label, 20 + col * 130, 40 + row * 140);
        sgl_obj_set_size(label, 120, 120);
        sgl_label_set_font(label, &consolas23);
        sgl_label_set_text_color(label, sgl_rgb(60, 60, 60));
        sgl_label_set_text(label, "+");
        sgl_label_set_text_align(label, SGL_ALIGN_CENTER);
        (void)label;

        label = sgl_label_ext_create(parent);
        sgl_obj_set_pos(label, 20 + col * 130, 40 + row * 140);
        sgl_obj_set_size(label, 120, 120);
        sgl_label_ext_set_font(label, &consolas23);
        sgl_label_ext_set_text(label, "SGL 360");
        sgl_label_ext_set_text_color(label, sgl_rgb(30, 90, 200));
        sgl_label_ext_set_text_align(label, SGL_ALIGN_CENTER);
        sgl_label_ext_set_text_rotation(label, angles[i]);
    }

    /* example 2: vertical text (90 deg) on a colored background card */
    label = sgl_label_ext_create(parent);
    sgl_obj_set_pos(label, 560, 40);
    sgl_obj_set_size(label, 60, 260);
    sgl_label_ext_set_font(label, &consolas23);
    sgl_label_ext_set_text(label, "VERTICAL");
    sgl_label_ext_set_text_color(label, SGL_COLOR_WHITE);
    sgl_label_ext_set_text_align(label, SGL_ALIGN_CENTER);
    sgl_label_ext_set_bg_color(label, sgl_rgb(21, 94, 160));
    sgl_label_ext_set_radius(label, 8);
    sgl_label_ext_set_text_rotation(label, 90);

    /* example 3: continuously spinning label, full turn every 6 seconds */
    g_spin = sgl_label_ext_create(parent);
    sgl_obj_set_pos(g_spin, 560, 310);
    sgl_obj_set_size(g_spin, 200, 120);
    sgl_label_ext_set_font(g_spin, &consolas23);
    sgl_label_ext_set_text(g_spin, "SPIN ME");
    sgl_label_ext_set_text_color(g_spin, SGL_COLOR_ORANGE);
    sgl_label_ext_set_text_align(g_spin, SGL_ALIGN_CENTER);

    anim = sgl_anim_create();
    sgl_anim_set_data(anim, g_spin);
    sgl_anim_set_path(anim, label_ext_spin_anim_cb, SGL_ANIM_PATH_LINEAR);
    sgl_anim_set_start_value(anim, 0);
    sgl_anim_set_end_value(anim, 360);
    sgl_anim_set_act_duration(anim, 6000);
    sgl_anim_start(anim, SGL_ANIM_REPEAT_LOOP);

    /* example 4: dynamic formatted text, rotated 30 degrees, updates 5 Hz */
    label = sgl_label_ext_create(parent);
    sgl_obj_set_pos(label, 40, 330);
    sgl_obj_set_size(label, 220, 50);
    sgl_label_ext_set_font(label, &consolas23);
    sgl_label_ext_set_text_buffer(label, g_counter_buf, sizeof(g_counter_buf));
    sgl_label_ext_set_text_color(label, SGL_COLOR_LIME);
    sgl_label_ext_set_text_align(label, SGL_ALIGN_CENTER);
    sgl_label_ext_set_text_rotation(label, 30);

    tmr = sgl_timer_create();
    if (tmr == NULL) {
        return;
    }
    sgl_timer_setup(tmr, label_ext_counter_cb, 200, -1, label);

    /* initial content goes through set_text_fmt so it is written into the
     * user buffer installed by set_text_buffer (set_text would replace the
     * pointer with a literal, and fmt updates would then write to rodata) */
    sgl_label_ext_set_text_fmt(label, "count %d", 0);
}
