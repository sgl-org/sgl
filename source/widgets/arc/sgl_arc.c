/* source/widgets/sgl_arc.c
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
#include "sgl_arc.h"

/**
 * @brief Check whether an angle lies inside the arc range [angle_s, angle_e].
 *        The arc sweeps clockwise (0 deg = bottom, 90 deg = left, ...) and may
 *        wrap across 0 deg.
 * @param angle  angle to test, in [0, 360)
 * @param angle_s start angle, in [0, 360)
 * @param angle_e end angle, in [0, 360)
 * @return true if angle is inside the arc range
 */
static bool arc_angle_in_range(int16_t angle, int16_t angle_s, int16_t angle_e)
{
    if (angle_s <= angle_e) {
        return (angle >= angle_s && angle <= angle_e);
    }
    /* range wraps across 0 deg */
    return (angle >= angle_s || angle <= angle_e);
}

/**
 * @brief Evaluate the point on a circle of given radius at given angle and
 *        fold it into the running bounding box. Coordinates are relative to
 *        the arc center (0,0).
 *        Angle convention: 0 deg = bottom, 90 deg = left, 180 deg = top,
 *        270 deg = right (clockwise sweep). Point = (-R*sin(a), R*cos(a)).
 * @param radius  radius of the circle
 * @param angle   angle in degrees [0, 360)
 * @param x_min   in/out min x
 * @param x_max   in/out max x
 * @param y_min   in/out min y
 * @param y_max   in/out max y
 */
static void arc_fold_point(int16_t radius, int16_t angle, int16_t *x_min, int16_t *x_max, int16_t *y_min, int16_t *y_max)
{
    int16_t x = -(int16_t)((radius * sgl_sin(angle)) / SGL_SIN_FIXED_ONE);
    int16_t y = (int16_t)((radius * sgl_cos(angle)) / SGL_SIN_FIXED_ONE);

    if (x < *x_min) *x_min = x;
    if (x > *x_max) *x_max = x;
    if (y < *y_min) *y_min = y;
    if (y > *y_max) *y_max = y;
}

/**
 * @brief Fold the bounding box of a round cap into the running box. The cap is
 *        a circle of radius cap_r centered on the mid radius at the given angle.
 * @param mid_r  mid radius (cap center distance from arc center)
 * @param angle  cap angle in degrees [0, 360)
 * @param cap_r  cap radius
 * @param x_min  in/out min x
 * @param x_max  in/out max x
 * @param y_min  in/out min y
 * @param y_max  in/out max y
 */
static void arc_fold_cap(int16_t mid_r, int16_t angle, int16_t cap_r, int16_t *x_min, int16_t *x_max, int16_t *y_min, int16_t *y_max)
{
    int16_t cx = -(int16_t)((mid_r * sgl_sin(angle)) / SGL_SIN_FIXED_ONE);
    int16_t cy = (int16_t)((mid_r * sgl_cos(angle)) / SGL_SIN_FIXED_ONE);

    if (cx - cap_r < *x_min) *x_min = cx - cap_r;
    if (cx + cap_r > *x_max) *x_max = cx + cap_r;
    if (cy - cap_r < *y_min) *y_min = cy - cap_r;
    if (cy + cap_r > *y_max) *y_max = cy + cap_r;
}

/**
 * @brief Compute the tight bounding box of an annular sector (ring segment)
 *        defined by inner/outer radius and start/end angle, and store it into
 *        area. The box is expressed relative to the arc center (0,0); the
 *        caller is responsible for translating it by the actual center.
 *        Angle convention: 0 deg = bottom, clockwise sweep.
 * @param radius_in  inner radius of the ring
 * @param radius_out outer radius of the ring
 * @param angle_s    start angle in degrees [0, 360)
 * @param angle_e    end angle in degrees [0, 360)
 * @param mode       arc mode; round-cap modes expand the box by the cap radius
 * @param area       out: tight bounding box of the ring segment (relative to center)
 * @note angle_s may be greater than angle_e (the arc wraps across 0 deg).
 *       The tight box of a circular arc sector is exactly the box of its two
 *       endpoints plus the axis-crossing points (0/90/180/270 deg) that fall
 *       inside the range. The inner arc is always contained in the outer box.
 *       For round-cap modes the ends stick out by the cap radius, so the box
 *       is expanded accordingly.
 */
static void arc_update_area(int16_t radius_in, int16_t radius_out, int16_t angle_s, int16_t angle_e, uint8_t mode, sgl_area_t *area)
{
    int16_t x_min, x_max, y_min, y_max;
    int16_t axis_angle;
    int i;

    angle_s = sgl_mod360(angle_s);
    angle_e = sgl_mod360(angle_e);

    /* endpoints on the outer radius */
    x_min = x_max = -(int16_t)(((radius_out + 1) * sgl_sin(angle_s)) / SGL_SIN_FIXED_ONE);
    y_min = y_max = (int16_t)(((radius_out + 1) * sgl_cos(angle_s)) / SGL_SIN_FIXED_ONE);
    arc_fold_point(radius_out + 1, angle_e, &x_min, &x_max, &y_min, &y_max);

    /* axis-crossing points of the outer arc within range:
     * 0 deg  -> (0, +R)  bottom (y_max)
     * 90 deg -> (-R, 0)  left   (x_min)
     * 180 deg-> (0, -R)  top    (y_min)
     * 270 deg-> (+R, 0)  right  (x_max)
     */
    for (i = 0; i < 4; i++) {
        axis_angle = i * 90;
        if (arc_angle_in_range(axis_angle, angle_s, angle_e)) {
            arc_fold_point(radius_out + 1, axis_angle, &x_min, &x_max, &y_min, &y_max);
        }
    }

    /* fold the inner arc too: it is contained in the outer box, so this never
     * enlarges the result, but keeps the box correct for degenerate cases. */
    arc_fold_point(radius_in - 1, angle_s, &x_min, &x_max, &y_min, &y_max);
    arc_fold_point(radius_in - 1, angle_e, &x_min, &x_max, &y_min, &y_max);
    for (i = 0; i < 4; i++) {
        axis_angle = i * 90;
        if (arc_angle_in_range(axis_angle, angle_s, angle_e)) {
            arc_fold_point(radius_in - 1, axis_angle, &x_min, &x_max, &y_min, &y_max);
        }
    }

    /* round-cap modes: each end is capped by a circle of radius
     * (radius_out - radius_in)/2 centered on the mid radius, so the box must
     * be expanded by that cap radius at both endpoints. */
    if (mode == SGL_ARC_MODE_NORMAL_SMOOTH || mode == SGL_ARC_MODE_RING_SMOOTH) {
        int16_t mid_r = (radius_in + radius_out) / 2;
        int16_t cap_r = (radius_out - radius_in) / 2 + 1;
        arc_fold_cap(mid_r, angle_s, cap_r, &x_min, &x_max, &y_min, &y_max);
        arc_fold_cap(mid_r, angle_e, cap_r, &x_min, &x_max, &y_min, &y_max);
    }

    area->x1 = x_min;
    area->y1 = y_min;
    area->x2 = x_max;
    area->y2 = y_max;
}

static void sgl_arc_construct_cb(sgl_surf_t *surf, sgl_obj_t* obj, sgl_event_t *evt)
{
    sgl_arc_t *arc = sgl_container_of(obj, sgl_arc_t, obj);
    int16_t tb_angle = 0;

    if(evt->type == SGL_EVENT_DRAW_MAIN) {
        arc->desc.cx = (obj->coords.x2 + obj->coords.x1) / 2;
        arc->desc.cy = (obj->coords.y2 + obj->coords.y1) / 2;

        if(arc->desc.start_angle == 0 && arc->desc.end_angle == 360) {
            sgl_draw_fill_ring(surf, &obj->area, arc->desc.cx, arc->desc.cy, arc->desc.radius_in, arc->desc.radius_out, arc->desc.color, arc->desc.alpha);
        }
        else if (arc->desc.start_angle != arc->desc.end_angle) {
            sgl_draw_fill_arc(surf, &obj->area, &arc->desc);
        }
    }
    else if(evt->type == SGL_EVENT_PRESSED ||
        evt->type == SGL_EVENT_MOVE_DOWN || evt->type == SGL_EVENT_MOVE_UP || evt->type == SGL_EVENT_MOVE_LEFT || evt->type == SGL_EVENT_MOVE_RIGHT
    ) {
        tb_angle = sgl_atan2(evt->pos.x - arc->desc.cx, evt->pos.y - arc->desc.cy);
        tb_angle = 360 - tb_angle;
        if ((tb_angle != arc->desc.end_angle) && tb_angle >= 0 && tb_angle <= 360) {
            arc->desc.end_angle = tb_angle;
        }

        sgl_obj_set_dirty(obj);
    }
    else if(SGL_EVENT_DRAW_INIT) {
        if(arc->desc.radius_out < 0) {
            arc->desc.radius_out = (obj->coords.x2 - obj->coords.x1) / 2;
        }

        if(arc->desc.radius_in < 0) {
            arc->desc.radius_in = arc->desc.radius_out - 2;
        }
    }
}


/**
 * @brief create an arc object
 * @param parent parent object
 * @return arc object
 */
sgl_obj_t* sgl_arc_create(sgl_obj_t* parent)
{
    sgl_arc_t *arc = sgl_malloc(sizeof(sgl_arc_t));
    if(arc == NULL) {
        SGL_LOG_ERROR("sgl_arc_create: malloc failed");
        return NULL;
    }

    /* set object all member to zero */
    memset(arc, 0, sizeof(sgl_arc_t));

    sgl_obj_t *obj = &arc->obj;
    sgl_obj_init(&arc->obj, parent);
    obj->needinit = 1;
    obj->clickable = 1;
    obj->movable = 1;

    arc->desc.alpha = SGL_THEME_ALPHA;
    arc->desc.mode = SGL_ARC_MODE_NORMAL;
    arc->desc.color = SGL_THEME_BG_COLOR;
    arc->desc.bg_color = SGL_THEME_COLOR;
    arc->desc.start_angle = 0;
    arc->desc.end_angle = 360;
    arc->desc.radius_out = -1;
    arc->desc.radius_in = -1;
    arc->desc.cx = -1;
    arc->desc.cy = -1;

    obj->construct_fn = sgl_arc_construct_cb;

    return obj;
}

/**
 * @brief set arc object color
 * @param obj arc object
 * @param color arc color
 * @return none
 */
void sgl_arc_set_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_arc_t *arc = sgl_container_of(obj, sgl_arc_t, obj);
    arc->desc.color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set arc object background color
 * @param obj arc object
 * @param color arc background color
 * @return none
 */
void sgl_arc_set_bg_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_arc_t *arc = sgl_container_of(obj, sgl_arc_t, obj);
    arc->desc.bg_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set arc object alpha
 * @param obj arc object
 * @param alpha arc alpha
 * @return none
 */
void sgl_arc_set_alpha(sgl_obj_t *obj, uint8_t alpha)
{
    sgl_arc_t *arc = sgl_container_of(obj, sgl_arc_t, obj);
    arc->desc.alpha = alpha;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set arc object radius
 * @param obj arc object
 * @param radius_in arc radius_in
 * @param radius_out arc radius_out
 * @return none
 */
void sgl_arc_set_radius(sgl_obj_t *obj, int16_t radius_in, int16_t radius_out)
{
    sgl_arc_t *arc = sgl_container_of(obj, sgl_arc_t, obj);
    if (obj->radius > 0) {
        sgl_obj_size_zoom(obj, radius_out - obj->radius);
    }
    arc->desc.radius_in = radius_in;
    arc->desc.radius_out = obj->radius = radius_out;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set arc object mode
 * @param obj arc object
 * @param mode arc mode
 * @return none
 */
void sgl_arc_set_mode(sgl_obj_t *obj, uint8_t mode)
{
    sgl_arc_t *arc = sgl_container_of(obj, sgl_arc_t, obj);
    arc->desc.mode = mode;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set arc object start angle
 * @param obj arc object
 * @param angle arc start angle
 * @return none
 * @note angle should be in range [0, 360]
 */
void sgl_arc_set_start_angle(sgl_obj_t *obj, int16_t angle)
{
    sgl_area_t area;
    sgl_arc_t *arc = sgl_container_of(obj, sgl_arc_t, obj);
    arc_update_area(arc->desc.radius_in, arc->desc.radius_out, arc->desc.start_angle, angle, arc->desc.mode, &area);
    area.x1 += arc->desc.cx;
    area.x2 += arc->desc.cx;
    area.y1 += arc->desc.cy;
    area.y2 += arc->desc.cy;
    sgl_update_area(&area);
    arc->desc.start_angle = angle;
}

/**
 * @brief set arc object end angle
 * @param obj arc object
 * @param angle arc end angle
 * @return none
 * @note angle should be in range [0, 360]
 */
void sgl_arc_set_end_angle(sgl_obj_t *obj, int16_t angle)
{
    sgl_area_t area;
    sgl_arc_t *arc = sgl_container_of(obj, sgl_arc_t, obj);
    if (angle == arc->desc.start_angle) {
        sgl_obj_set_dirty(obj);
    }
    else {
        arc_update_area(arc->desc.radius_in, arc->desc.radius_out, arc->desc.end_angle, angle, arc->desc.mode, &area);
        area.x1 += arc->desc.cx;
        area.x2 += arc->desc.cx;
        area.y1 += arc->desc.cy;
        area.y2 += arc->desc.cy;
        sgl_update_area(&area);
    }
    arc->desc.end_angle = angle;
}

int16_t sgl_arc_get_included_angle(sgl_obj_t *obj)
{
    sgl_arc_t *arc = sgl_container_of(obj, sgl_arc_t, obj);
    int16_t angle = arc->desc.end_angle - arc->desc.start_angle;

    while (angle < 0) {
        angle += 360;
    }
    while (angle > 360) {
        angle -= 360;
    }

    return angle;
}
