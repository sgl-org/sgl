/* source/widgets/spinner/sgl_spinner.c
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

#include <sgl_core.h>
#include <sgl_draw.h>
#include <sgl_math.h>
#include <sgl_log.h>
#include <sgl_mm.h>
#include <sgl_theme.h>
#include <sgl_cfgfix.h>
#include <string.h>
#include "sgl_spinner.h"

/* default rotation period of one full turn, ms */
#define SGL_SPINNER_DEFAULT_PERIOD              (1200)

/* default number of dots around the ring */
#define SGL_SPINNER_DEFAULT_SEG_CNT             (12)

/* auto dot radius = radius_out / SGL_SPINNER_DOT_DIV, at least 2 px */
#define SGL_SPINNER_DOT_DIV                     (6)

/**
 * @brief spinner construct callback
 * @param surf surface to draw on
 * @param obj spinner object
 * @param evt event pointer
 * @return none
 * @note the ring is drawn as seg_cnt dots placed on the mid radius; dot i is
 *       step * i degrees behind the leading angle and fades out linearly, so
 *       rotating the leading angle makes the dot train spin clockwise
 */
static void sgl_spinner_construct_cb(sgl_surf_t *surf, sgl_obj_t* obj, sgl_event_t *evt)
{
    sgl_spinner_t *spinner = sgl_container_of(obj, sgl_spinner_t, obj);
    int16_t cx, cy, mid_r, dot_r, step;
    sgl_area_t dot_area;
    uint8_t i;

    if (evt->type == SGL_EVENT_DESTROYED) {
        sgl_anim_delete_by_obj(obj);
        return;
    }

    if (evt->type != SGL_EVENT_DRAW_MAIN) {
        return;
    }

    cx = (obj->coords.x1 + obj->coords.x2) / 2;
    cy = (obj->coords.y1 + obj->coords.y2) / 2;

    /* no explicit radius: derive the ring from the object box */
    if (spinner->radius_out == 0) {
        int16_t w = obj->coords.x2 - obj->coords.x1 + 1;
        int16_t h = obj->coords.y2 - obj->coords.y1 + 1;
        int16_t r_out = (sgl_min(w, h) / 2) - 1;
        int16_t r_dot = sgl_max(r_out / SGL_SPINNER_DOT_DIV, 2);

        spinner->radius_out = r_out;
        spinner->radius_in = r_out - 2 * r_dot;
    }

    dot_r = (spinner->radius_out - spinner->radius_in) / 2;
    mid_r = (spinner->radius_in + spinner->radius_out) / 2;
    step = 360 / spinner->seg_cnt;

    for (i = 0; i < spinner->seg_cnt; i++) {
        /* dots trail the leading angle clockwise, 0 deg = top */
        int16_t a = sgl_mod360(spinner->angle - (int16_t)(i * step));
        int16_t dx = (int16_t)((mid_r * sgl_sin(a)) / SGL_SIN_FIXED_ONE);
        int16_t dy = -(int16_t)((mid_r * sgl_cos(a)) / SGL_SIN_FIXED_ONE);
        uint8_t alpha = (uint8_t)(spinner->alpha * (spinner->seg_cnt - i) / spinner->seg_cnt);

        /* tight per-dot area (plus AA margin) keeps the fill cheap */
        dot_area.x1 = cx + dx - dot_r - 1;
        dot_area.y1 = cy + dy - dot_r - 1;
        dot_area.x2 = cx + dx + dot_r + 1;
        dot_area.y2 = cy + dy + dot_r + 1;
        sgl_draw_fill_circle(surf, &dot_area, cx + dx, cy + dy, dot_r, spinner->color, alpha);
    }
}

/**
 * @brief animation path callback, drives the leading angle
 * @param anim animation object
 * @param value interpolated leading angle in degrees [0, 360]
 * @return none
 */
static void sgl_spinner_anim_cb(sgl_anim_t *anim, int32_t value)
{
    sgl_obj_t *obj = (sgl_obj_t*)anim->data;
    if (obj != NULL) {
        sgl_spinner_set_angle(obj, (int16_t)value);
    }
}

/**
 * @brief create a spinner object
 * @param parent parent object
 * @return spinner object
 */
sgl_obj_t* sgl_spinner_create(sgl_obj_t* parent)
{
    sgl_spinner_t *spinner = sgl_malloc(sizeof(sgl_spinner_t));
    if(spinner == NULL) {
        SGL_LOG_ERROR("sgl_spinner_create: malloc failed");
        return NULL;
    }

    /* set object all member to zero */
    memset(spinner, 0, sizeof(sgl_spinner_t));

    sgl_obj_t *obj = &spinner->obj;
    sgl_obj_init(&spinner->obj, parent);
    obj->clickable = 0;

    spinner->alpha = SGL_THEME_ALPHA;
    spinner->color = SGL_THEME_COLOR;
    spinner->seg_cnt = SGL_SPINNER_DEFAULT_SEG_CNT;
    spinner->period = SGL_SPINNER_DEFAULT_PERIOD;
    obj->construct_fn = sgl_spinner_construct_cb;

    return obj;
}

/**
 * @brief set spinner color
 * @param obj spinner object
 * @param color spinner color
 * @return none
 */
void sgl_spinner_set_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_spinner_t *spinner = sgl_container_of(obj, sgl_spinner_t, obj);
    spinner->color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set spinner alpha
 * @param obj spinner object
 * @param alpha spinner alpha (brightness of the head dot)
 * @return none
 */
void sgl_spinner_set_alpha(sgl_obj_t *obj, uint8_t alpha)
{
    sgl_spinner_t *spinner = sgl_container_of(obj, sgl_spinner_t, obj);
    spinner->alpha = alpha;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set spinner ring radius
 * @param obj spinner object
 * @param radius_in inner radius
 * @param radius_out outer radius
 * @return none
 */
void sgl_spinner_set_radius(sgl_obj_t *obj, int16_t radius_in, int16_t radius_out)
{
    sgl_spinner_t *spinner = sgl_container_of(obj, sgl_spinner_t, obj);
    spinner->radius_in = radius_in;
    spinner->radius_out = obj->radius = radius_out;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set spinner dot count
 * @param obj spinner object
 * @param cnt dot count, clamped to [3, 30]
 * @return none
 */
void sgl_spinner_set_seg_count(sgl_obj_t *obj, uint8_t cnt)
{
    sgl_spinner_t *spinner = sgl_container_of(obj, sgl_spinner_t, obj);
    spinner->seg_cnt = sgl_clamp(cnt, 3, 30);
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set spinner rotation period
 * @param obj spinner object
 * @param period_ms time of one full turn in ms
 * @return none
 */
void sgl_spinner_set_period(sgl_obj_t *obj, uint16_t period_ms)
{
    sgl_spinner_t *spinner = sgl_container_of(obj, sgl_spinner_t, obj);
    if (spinner->period == period_ms) {
        return;
    }
    spinner->period = period_ms;
    /* the running animation keeps the old duration, restart it */
    if (sgl_spinner_is_running(obj)) {
        sgl_spinner_stop(obj);
        sgl_spinner_start(obj);
    }
}

/**
 * @brief set spinner leading angle
 * @param obj spinner object
 * @param angle leading angle, deg, normalized to [0, 360)
 * @return none
 */
void sgl_spinner_set_angle(sgl_obj_t *obj, int16_t angle)
{
    sgl_spinner_t *spinner = sgl_container_of(obj, sgl_spinner_t, obj);
    angle = sgl_mod360(angle);
    if (angle == spinner->angle) {
        return;
    }
    spinner->angle = angle;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief get spinner leading angle
 * @param obj spinner object
 * @return leading angle, deg
 */
int16_t sgl_spinner_get_angle(sgl_obj_t *obj)
{
    sgl_spinner_t *spinner = sgl_container_of(obj, sgl_spinner_t, obj);
    return spinner->angle;
}

/**
 * @brief start the spinner rotation animation
 * @param obj spinner object
 * @return none
 */
void sgl_spinner_start(sgl_obj_t *obj)
{
    sgl_spinner_t *spinner = sgl_container_of(obj, sgl_spinner_t, obj);
    if (sgl_anim_get_by_obj(obj) != NULL) {
        return;
    }
    sgl_anim_setup_obj_action_loop(obj, 0, 360, spinner->period,
                                   sgl_spinner_anim_cb, SGL_ANIM_PATH_LINEAR, NULL);
}

/**
 * @brief stop the spinner rotation animation
 * @param obj spinner object
 * @return none
 */
void sgl_spinner_stop(sgl_obj_t *obj)
{
    sgl_anim_delete_by_obj(obj);
}

/**
 * @brief check whether the spinner is rotating
 * @param obj spinner object
 * @return true if rotating
 */
bool sgl_spinner_is_running(sgl_obj_t *obj)
{
    return (sgl_anim_get_by_obj(obj) != NULL);
}
