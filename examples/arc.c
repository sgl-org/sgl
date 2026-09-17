/* examples/arc.c
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
 * Arc widget examples:
 *  1. ring sweeping counterclockwise: start angle retreats 360 -> 0 with the
 *     end pinned at 360, so the arc grows counterclockwise from the top -
 *     the exact mirror of example 3
 *  2. 270 deg gauge sweep, wrapping across 0 deg (135 -> 45)
 *  3. animated loader: end angle sweeps 0 -> 360 in a loop, round caps and a
 *     gray track (RING_SMOOTH mode)
 *  4. static 180 deg arc with round caps (NORMAL_SMOOTH mode)
 *
 * Angle convention: 0 deg = top, sweeping clockwise. All arcs are interactive
 * out of the box: press or drag on an arc and its end angle follows the touch
 * point (built-in widget behaviour).
 *
 * Screen is 800x480, arcs are laid out in one row.
 */

/* loader arc driven by the shared animation */
static sgl_obj_t *g_loader = NULL;

/* counterclockwise ring driven by the shared animation */
static sgl_obj_t *g_ring = NULL;

/**
 * @brief animation path callback, drives the loader end angle
 * @param anim animation object
 * @param value interpolated end angle in degrees [0, 360]
 * @return none
 */
static void arc_loader_anim_cb(sgl_anim_t *anim, int32_t value)
{
    (void)anim;
    if (g_loader != NULL) {
        sgl_arc_set_end_angle(g_loader, (int16_t)value);
    }
}

/**
 * @brief animation path callback, drives the counterclockwise ring
 * @param anim animation object
 * @param value interpolated start angle in degrees [0, 360]
 * @return none
 */
static void arc_ring_anim_cb(sgl_anim_t *anim, int32_t value)
{
    (void)anim;
    if (g_ring != NULL) {
        sgl_arc_set_start_angle(g_ring, (int16_t)value);
    }
}

/**
 * @brief create the arc examples
 * @param parent parent object, NULL creates the arcs on the active screen
 * @return none
 */
void sgl_arc_examples(sgl_obj_t *parent)
{
    sgl_obj_t *arc;
    sgl_anim_t *anim;

    /* example 1: ring sweeping counterclockwise (mirror of example 3):
     * start angle is animated 360 -> 0 with the end pinned at 360, so the
     * blue arc grows counterclockwise from the top */
    arc = sgl_arc_create(parent);
    sgl_obj_set_pos(arc, 30, 170);
    sgl_obj_set_size(arc, 140, 140);
    sgl_arc_set_radius(arc, 45, 62);
    sgl_arc_set_color(arc, SGL_COLOR_BLUE);
    sgl_arc_set_start_angle(arc, 360);
    sgl_arc_set_end_angle(arc, 360);
    g_ring = arc;

    /* example 2: 270 deg gauge sweep, wraps across 0 deg */
    arc = sgl_arc_create(parent);
    sgl_obj_set_pos(arc, 230, 170);
    sgl_obj_set_size(arc, 140, 140);
    sgl_arc_set_radius(arc, 45, 62);
    sgl_arc_set_color(arc, SGL_COLOR_RED_ORANGE);
    sgl_arc_set_start_angle(arc, 135);
    sgl_arc_set_end_angle(arc, 45);

    /* example 3: animated loader with round caps and a track */
    arc = sgl_arc_create(parent);
    sgl_obj_set_pos(arc, 430, 170);
    sgl_obj_set_size(arc, 140, 140);
    sgl_arc_set_radius(arc, 45, 62);
    sgl_arc_set_mode(arc, SGL_ARC_MODE_RING_SMOOTH);
    sgl_arc_set_color(arc, SGL_COLOR_SPRING_GREEN);
    sgl_arc_set_bg_color(arc, SGL_COLOR_DARK_GRAY);
    sgl_arc_set_start_angle(arc, 0);
    sgl_arc_set_end_angle(arc, 0);
    g_loader = arc;

    /* example 4: 180 deg arc with round caps, sweeping over the top */
    arc = sgl_arc_create(parent);
    sgl_obj_set_pos(arc, 630, 170);
    sgl_obj_set_size(arc, 140, 140);
    sgl_arc_set_radius(arc, 45, 62);
    sgl_arc_set_mode(arc, SGL_ARC_MODE_NORMAL_SMOOTH);
    sgl_arc_set_color(arc, SGL_COLOR_VIOLET);
    sgl_arc_set_start_angle(arc, 270);
    sgl_arc_set_end_angle(arc, 90);

    /* loader animation: end angle 0 -> 360 over 1.8 s, looping forever, so
     * the green arc grows clockwise. The small delay lets the first frame
     * draw, which fixes the arc center used by the incremental updates */
    anim = sgl_anim_create();
    if (anim != NULL) {
        sgl_anim_set_start_value(anim, 0);
        sgl_anim_set_end_value(anim, 360);
        sgl_anim_set_act_duration(anim, 1800);
        sgl_anim_set_act_delay(anim, 300);
        sgl_anim_set_path(anim, arc_loader_anim_cb, SGL_ANIM_PATH_LINEAR);
        sgl_anim_set_auto_free(anim);
        sgl_anim_start(anim, SGL_ANIM_REPEAT_LOOP);
    }

    /* ring animation: start angle 360 -> 0 over 1.8 s, looping forever, so
     * the blue arc grows counterclockwise - the mirror of the loader */
    anim = sgl_anim_create();
    if (anim != NULL) {
        sgl_anim_set_start_value(anim, 360);
        sgl_anim_set_end_value(anim, 0);
        sgl_anim_set_act_duration(anim, 1800);
        sgl_anim_set_act_delay(anim, 300);
        sgl_anim_set_path(anim, arc_ring_anim_cb, SGL_ANIM_PATH_LINEAR);
        sgl_anim_set_auto_free(anim);
        sgl_anim_start(anim, SGL_ANIM_REPEAT_LOOP);
    }
}
