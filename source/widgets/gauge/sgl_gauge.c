/* source/widgets/sgl_gauge.c
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

#include <sgl_core.h>
#include <sgl_draw.h>
#include <sgl_math.h>
#include <sgl_log.h>
#include <sgl_mm.h>
#include <sgl_theme.h>
#include <sgl_cfgfix.h>
#include <string.h>
#include "sgl_gauge.h"

static void gauge_calc_needle_area(const sgl_gauge_t *gauge, int16_t cx, int16_t cy, int16_t r,
                                   int16_t pointer_s, int16_t pointer_e, sgl_area_t *area)
{
    int needle_angle_deg = sgl_mod360(90 + gauge->angle_start + gauge->value * gauge->scale_angle / gauge->scale_step);
    int32_t n_sin = sgl_sin(needle_angle_deg);
    int32_t n_cos = sgl_cos(needle_angle_deg);

    int32_t px_needle = ((r - pointer_s) * n_cos) / SGL_SIN_FIXED_ONE + cx + 1;
    int32_t py_needle = ((r - pointer_s) * n_sin) / SGL_SIN_FIXED_ONE + cy + 1;
    int32_t nx_needle = ((r - pointer_e) * n_cos) / SGL_SIN_FIXED_ONE + cx + 1;
    int32_t ny_needle = ((r - pointer_e) * n_sin) / SGL_SIN_FIXED_ONE + cy + 1;

    area->x1 = sgl_min(px_needle, nx_needle);
    area->x2 = sgl_max(px_needle, nx_needle);
    area->y1 = sgl_min(py_needle, ny_needle);
    area->y2 = sgl_max(py_needle, ny_needle);
}

void gauge_update_value(sgl_gauge_t *gauge, int16_t value)
{
    const int16_t cx = (gauge->obj.coords.x1 + gauge->obj.coords.x2) / 2;
    const int16_t cy = (gauge->obj.coords.y1 + gauge->obj.coords.y2) / 2;
    const int16_t r = sgl_max(gauge->obj.radius, sgl_obj_get_width(&gauge->obj) / 2 - 1);
    const int16_t scale_in = gauge->arc_width + 6 + sgl_max(gauge->scale_length, 4);
    const int16_t hub_r = sgl_max((r + 8) / 8, gauge->hub_r);
    const int16_t pointer_s = scale_in + 4 + gauge->pointer_width;
    const int16_t pointer_e = r - hub_r - gauge->pointer_width;
    sgl_area_t area;

    gauge_calc_needle_area(gauge, cx, cy, r, pointer_s, pointer_e, &area);
    gauge->value = value;

    sgl_area_t new_area;
    gauge_calc_needle_area(gauge, cx, cy, r, pointer_s, pointer_e, &new_area);
    sgl_area_selfmerge(&area, &new_area);

    area.x1 -= gauge->pointer_width;
    area.x2 += gauge->pointer_width;
    area.y1 -= gauge->pointer_width;
    area.y2 += gauge->pointer_width;
    sgl_update_area(&area);
}

static void sgl_gauge_construct_cb(sgl_surf_t *surf, sgl_obj_t* obj, sgl_event_t *evt)
{
    sgl_gauge_t *gauge = sgl_container_of(obj, sgl_gauge_t, obj);
    const int16_t cx = (obj->coords.x1 + obj->coords.x2) / 2;
    const int16_t cy = (obj->coords.y1 + obj->coords.y2) / 2;
    const int16_t r = sgl_max(obj->radius, sgl_obj_get_width(obj) / 2 - 1);
    const int16_t hub_r = sgl_max((r + 8) / 8, gauge->hub_r);
    const int16_t scale_len = sgl_max(gauge->scale_length, 4);
    const int16_t scale_out = gauge->arc_width + 6;
    const int16_t scale_in = scale_out + scale_len;
    const int16_t text_cr = r - scale_in - (sgl_font_get_height(gauge->font) / 2) - 4;
    const int16_t pointer_s = scale_in + 4 + gauge->pointer_width, pointer_e = r - hub_r - gauge->pointer_width;
    int16_t scale_mask = gauge->scale_start, txt_x, txt_y;
    uint16_t count = 0;
    char text[16];
    int16_t text_len;
    sgl_color_t scale_color;

    if(evt->type == SGL_EVENT_DRAW_MAIN) {
        sgl_draw_arc_t arc = {
            .alpha = gauge->alpha,
            .color = gauge->arc_color,
            .mode = SGL_ARC_MODE_NORMAL,
            .cx = cx,
            .cy = cy,
            .start_angle = gauge->angle_start,
            .end_angle = gauge->angle_end,
        };

        /* draw gauge background and hub circle */
        sgl_draw_fill_circle(surf, &obj->area, cx, cy, r, gauge->bg_color, gauge->alpha);
        sgl_draw_fill_circle(surf, &obj->area, cx, cy, hub_r, gauge->hub_color, gauge->alpha);

        /* draw gauge arc */
        arc.radius_in = r - gauge->arc_width - 1;
        arc.radius_out = r - 1;
        sgl_draw_fill_arc(surf, &obj->area, &arc);

        for (int16_t angle = gauge->angle_start; angle <= gauge->angle_end; angle += gauge->scale_angle) {
            scale_color = scale_mask < gauge->scale_warning ? gauge->scale_color : SGL_COLOR_RED;

            int16_t calc_angle = angle + 90;
            int32_t sin_val = sgl_sin(calc_angle);
            int32_t cos_val = sgl_cos(calc_angle);

            int32_t x_out = ((r - scale_out) * cos_val) / SGL_SIN_FIXED_ONE + cx;
            int32_t y_out = ((r - scale_out) * sin_val) / SGL_SIN_FIXED_ONE + cy;
            int32_t x_in  = ((r - scale_in) * cos_val) / SGL_SIN_FIXED_ONE + cx;
            int32_t y_in  = ((r - scale_in) * sin_val) / SGL_SIN_FIXED_ONE + cy;

            sgl_sprintf(text, "%d", scale_mask);
            text_len = sgl_font_get_string_width(text, gauge->font);

            if ((count % (gauge->text_interval + 1)) == 0) {
                int32_t tx = (text_cr * cos_val) / SGL_SIN_FIXED_ONE + cx;
                int32_t ty = (text_cr * sin_val) / SGL_SIN_FIXED_ONE + cy;
                txt_x = tx - (text_len) / 2 - 2;
                txt_y = ty - (sgl_font_get_height(gauge->font) / 2);

                if ((angle - gauge->angle_start) < 360) {
                    sgl_draw_line_fill_slanted(surf, &obj->area, x_out, y_out, x_in, y_in, gauge->scale_width * 2, scale_color, gauge->alpha);
                    sgl_draw_string(surf, &obj->area, txt_x, txt_y, text, gauge->text_color, gauge->alpha, gauge->font);
                }
            }
            else {
                sgl_draw_line_fill_slanted(surf, &obj->area, x_out, y_out, x_in, y_in, gauge->scale_width, scale_color, gauge->alpha);
            }
            count ++;
            scale_mask += gauge->scale_step;
        }

        int needle_angle_deg = sgl_mod360(90 + gauge->angle_start + gauge->value * gauge->scale_angle / gauge->scale_step);
        int32_t n_sin = sgl_sin(needle_angle_deg);
        int32_t n_cos = sgl_sin(needle_angle_deg + 90);

        int32_t px_needle = ((r - pointer_s) * n_cos) / SGL_SIN_FIXED_ONE + cx + 1;
        int32_t py_needle = ((r - pointer_s) * n_sin) / SGL_SIN_FIXED_ONE + cy + 1;
        int32_t nx_needle = ((r - pointer_e) * n_cos) / SGL_SIN_FIXED_ONE + cx + 1;
        int32_t ny_needle = ((r - pointer_e) * n_sin) / SGL_SIN_FIXED_ONE + cy + 1;

        sgl_draw_line_fill_slanted(surf, &obj->area, px_needle, py_needle, nx_needle, ny_needle, gauge->pointer_width, gauge->pointer_color, gauge->alpha);
    }
}

/**
 * @brief create a gauge object
 * @param parent parent of the gauge
 * @return gauge object
 */
sgl_obj_t* sgl_gauge_create(sgl_obj_t* parent)
{
    sgl_gauge_t *gauge = sgl_malloc(sizeof(sgl_gauge_t));
    if(gauge == NULL) {
        SGL_LOG_ERROR("sgl_gauge_create: malloc failed");
        return NULL;
    }

    /* set object all member to zero */
    memset(gauge, 0, sizeof(sgl_gauge_t));

    sgl_obj_t *obj = &gauge->obj;
    sgl_obj_init(&gauge->obj, parent);
    obj->construct_fn = sgl_gauge_construct_cb;

    gauge->alpha = SGL_THEME_ALPHA;
    gauge->bg_color = SGL_THEME_BG_COLOR;
    gauge->arc_color = SGL_THEME_COLOR;
    gauge->pointer_color = SGL_COLOR_RED;
    gauge->scale_color = SGL_THEME_COLOR;
    gauge->text_color = SGL_THEME_COLOR;
    gauge->scale_width = 1;
    gauge->pointer_width = 2;
    gauge->hub_color = SGL_THEME_COLOR;
    gauge->angle_start = 30;
    gauge->angle_end = 330;
    gauge->scale_angle = 15;
    gauge->scale_step = 10;
    gauge->scale_warning = INT16_MAX;
    gauge->value = 0;
    gauge->arc_width = 2;
    gauge->text_interval = 0x3;
    gauge->font = sgl_get_system_font();

    return obj;
}

/**
 * @brief set gauge background color
 * @param obj gauge object
 * @param color gauge background color
 * @return none
 */
void sgl_gauge_set_bg_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_gauge_t *gauge = sgl_container_of(obj, sgl_gauge_t, obj);
    gauge->bg_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set gauge arc color
 * @param obj gauge object
 * @param color gauge arc color
 * @return none
 */
void sgl_gauge_set_arc_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_gauge_t *gauge = sgl_container_of(obj, sgl_gauge_t, obj);
    gauge->arc_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set gauge pointer color
 * @param obj gauge object
 * @param color gauge pointer color
 * @return none
 */
void sgl_gauge_set_pointer_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_gauge_t *gauge = sgl_container_of(obj, sgl_gauge_t, obj);
    gauge->pointer_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set gauge scale color
 * @param obj gauge object
 * @param color gauge scale color
 * @return none
 */
void sgl_gauge_set_scale_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_gauge_t *gauge = sgl_container_of(obj, sgl_gauge_t, obj);
    gauge->scale_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set gauge text color
 * @param obj gauge object
 * @param color gauge text color
 * @return none
 */
void sgl_gauge_set_text_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_gauge_t *gauge = sgl_container_of(obj, sgl_gauge_t, obj);
    gauge->text_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set gauge hub color
 * @param obj gauge object
 * @param color gauge hub color
 * @return none
 */
void sgl_gauge_set_hub_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_gauge_t *gauge = sgl_container_of(obj, sgl_gauge_t, obj);
    gauge->hub_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set gauge arc width
 * @param obj gauge object
 * @param width gauge arc width
 * @return none
 */
void sgl_gauge_set_arc_width(sgl_obj_t *obj, uint8_t width)
{
    sgl_gauge_t *gauge = sgl_container_of(obj, sgl_gauge_t, obj);
    gauge->arc_width = width;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set gauge pointer width
 * @param obj gauge object
 * @param width gauge pointer width
 * @return none
 */
void sgl_gauge_set_pointer_width(sgl_obj_t *obj, uint8_t width)
{
    sgl_gauge_t *gauge = sgl_container_of(obj, sgl_gauge_t, obj);
    gauge->pointer_width = width;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set gauge scale width
 * @param obj gauge object
 * @param width gauge scale width
 * @return none
 */
void sgl_gauge_set_scale_width(sgl_obj_t *obj, uint8_t width)
{
    sgl_gauge_t *gauge = sgl_container_of(obj, sgl_gauge_t, obj);
    gauge->scale_width = width;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set gauge scale length
 * @param obj gauge object
 * @param length gauge scale length
 * @return none
 */
void sgl_gauge_set_scale_length(sgl_obj_t *obj, uint8_t length)
{
    sgl_gauge_t *gauge = sgl_container_of(obj, sgl_gauge_t, obj);
    gauge->scale_length = length;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set gauge scale angle
 * @param obj gauge object
 * @param angle gauge scale angle
 * @return none
 */
void sgl_gauge_set_scale_angle(sgl_obj_t *obj, int16_t angle)
{
    sgl_gauge_t *gauge = sgl_container_of(obj, sgl_gauge_t, obj);
    gauge->scale_angle = angle;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set gauge scale warning value
 * @param obj gauge object
 * @param value gauge scale warning value
 * @return none
 */
void sgl_gauge_set_scale_warning_value(sgl_obj_t *obj, int16_t value)
{
    sgl_gauge_t *gauge = sgl_container_of(obj, sgl_gauge_t, obj);
    gauge->scale_warning = value;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set gauge angle range
 * @param obj gauge object
 * @param start gauge angle start
 * @param end gauge angle end
 * @return none
 */
void sgl_gauge_set_angle_range(sgl_obj_t *obj, int16_t start, int16_t end)
{
    sgl_gauge_t *gauge = sgl_container_of(obj, sgl_gauge_t, obj);
    gauge->angle_start = start;
    gauge->angle_end = end;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set gauge value
 * @param obj gauge object
 * @param value gauge value
 * @return none
 */
void sgl_gauge_set_value(sgl_obj_t *obj, int16_t value)
{
    sgl_gauge_t *gauge = sgl_container_of(obj, sgl_gauge_t, obj);
    if (value != gauge->value) {
        gauge_update_value(gauge, value);
    }
}

/**
 * @brief get gauge value
 * @param obj gauge object
 * @return gauge value
 */
int16_t sgl_gauge_get_value(sgl_obj_t *obj)
{
    sgl_gauge_t *gauge = sgl_container_of(obj, sgl_gauge_t, obj);
    return gauge->value;
}

/**
 * @brief set gauge font
 * @param obj gauge object
 * @param font gauge font
 * @return none
 */
void sgl_gauge_set_font(sgl_obj_t *obj, const sgl_font_t *font)
{
    sgl_gauge_t *gauge = sgl_container_of(obj, sgl_gauge_t, obj);
    gauge->font = font;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set gauge hub radius
 * @param obj gauge object
 * @param radius gauge hub radius
 * @return none
 */
void sgl_gauge_set_hub_radiue(sgl_obj_t *obj, uint8_t radius)
{
    sgl_gauge_t *gauge = sgl_container_of(obj, sgl_gauge_t, obj);
    gauge->hub_r = radius;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set gauge scale start value
 * @param obj gauge object
 * @param value gauge scale start value
 * @return none
 */
void sgl_gauge_set_scale_start_value(sgl_obj_t *obj, int16_t value)
{
    sgl_gauge_t *gauge = sgl_container_of(obj, sgl_gauge_t, obj);
    gauge->scale_start = value;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set gauge scale step value
 * @param obj gauge object
 * @param value gauge scale step value
 * @return none
 */
void sgl_gauge_set_scale_step_value(sgl_obj_t *obj, uint16_t value)
{
    sgl_gauge_t *gauge = sgl_container_of(obj, sgl_gauge_t, obj);
    gauge->scale_step = value;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set gauge text interval
 * @param obj gauge object
 * @param interval gauge text interval
 * @return none
 */
void sgl_gauge_set_text_interval(sgl_obj_t *obj, uint8_t interval)
{
    sgl_gauge_t *gauge = sgl_container_of(obj, sgl_gauge_t, obj);
    gauge->text_interval = interval;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set gauge alpha
 * @param obj gauge object
 * @param alpha gauge alpha
 * @return none
 */
void sgl_gauge_set_alpha(sgl_obj_t *obj, uint8_t alpha)
{
    sgl_gauge_t *gauge = sgl_container_of(obj, sgl_gauge_t, obj);
    gauge->alpha = alpha;
    sgl_obj_set_dirty(obj);
}
