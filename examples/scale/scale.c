/*
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

#define SCALE_EXAMPLE_CENTER_X       (400)
#define SCALE_EXAMPLE_MIN_SIZE       (180)
#define SCALE_EXAMPLE_MAX_SIZE       (3000)
#define SCALE_EXAMPLE_TOP_Y          (150)
#define SCALE_EXAMPLE_MIN_TICKS      (9)
#define SCALE_EXAMPLE_MAX_TICKS      (21)
#define SCALE_EXAMPLE_ANIM_DURATION  (500)

static void scale_example_apply(sgl_obj_t *scale, int32_t size)
{
    int32_t tick_count = SCALE_EXAMPLE_MIN_TICKS +
        ((size - SCALE_EXAMPLE_MIN_SIZE) *
         (SCALE_EXAMPLE_MAX_TICKS - SCALE_EXAMPLE_MIN_TICKS)) /
        (SCALE_EXAMPLE_MAX_SIZE - SCALE_EXAMPLE_MIN_SIZE);

    sgl_obj_set_pos(scale, SCALE_EXAMPLE_CENTER_X - size / 2,
                    SCALE_EXAMPLE_TOP_Y);
    sgl_obj_set_size(scale, (int16_t)size, (int16_t)size);
    sgl_scale_set_total_tick_count(scale, (uint16_t)tick_count);
}

static void scale_example_anim_cb(sgl_anim_t *anim, int32_t size)
{
    scale_example_apply(sgl_anim_get_data(anim), size);
}

static void scale_example_anim_finish_cb(sgl_anim_t *anim)
{
    int32_t start = anim->start_value;

    sgl_anim_set_start_value(anim, anim->end_value);
    sgl_anim_set_end_value(anim, start);
}

void sgl_scale_examples(sgl_obj_t *parent)
{
    sgl_obj_t *scale = sgl_scale_create(parent);
    sgl_anim_t *anim;

    if (scale == NULL) {
        SGL_LOG_ERROR("sgl_scale_examples: scale creation failed");
        return;
    }

    sgl_scale_set_mode(scale, SGL_SCALE_MODE_CIRCULAR);
    sgl_scale_set_range(scale, 0, 100);
    sgl_scale_set_major_tick_every(scale, 4);
    sgl_scale_set_angle_range(scale, 0, 360);
    sgl_scale_set_major_tick_style(scale, 16, 2, SGL_COLOR_BLUE);
    sgl_scale_set_minor_tick_style(scale, 8, 1, SGL_COLOR_DARK_GRAY);
    sgl_scale_set_axis_style(scale, 2, SGL_COLOR_BLACK);
    sgl_scale_set_font(scale, &consolas14);
    scale_example_apply(scale, SCALE_EXAMPLE_MIN_SIZE);

    anim = sgl_anim_create();
    if (anim == NULL) {
        SGL_LOG_ERROR("sgl_scale_examples: animation creation failed");
        return;
    }

    sgl_anim_set_data(anim, scale);
    sgl_anim_set_start_value(anim, SCALE_EXAMPLE_MIN_SIZE);
    sgl_anim_set_end_value(anim, SCALE_EXAMPLE_MAX_SIZE);
    sgl_anim_set_act_duration(anim, SCALE_EXAMPLE_ANIM_DURATION);
    sgl_anim_set_path(anim, scale_example_anim_cb, SGL_ANIM_PATH_EASE_IN_OUT);
    sgl_anim_set_finish_cb(anim, scale_example_anim_finish_cb);
    sgl_anim_start(anim, SGL_ANIM_REPEAT_LOOP);
}
