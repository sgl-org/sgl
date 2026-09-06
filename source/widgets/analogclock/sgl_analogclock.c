/* source/widgets/sgl_analogclock.c
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

#include <sgl_theme.h>
#include <string.h>
#include "sgl_analogclock.h"

static void analogclock_update_area(sgl_analogclock_t *clock)
{
    const int16_t cx = (clock->obj.coords.x1 + clock->obj.coords.x2) / 2;
    const int16_t cy = (clock->obj.coords.y1 + clock->obj.coords.y2) / 2;
    const int16_t r = sgl_max(clock->obj.radius, sgl_obj_get_width(&clock->obj) / 2 - 1);
    
    const int16_t border_w = sgl_min(clock->obj.border, r);
    const int16_t inner_r = r - border_w;
    if (inner_r <= 0) return;

    sgl_area_t total_area = {0};
    sgl_area_t tmp_area;
    bool first = true;

    #define CALC_MAX_NEEDLE_AREA_INT(len_ratio_permille, width) do { \
        int16_t ptr_len = (int16_t)((int32_t)inner_r * (len_ratio_permille) / 1000); \
        int16_t expand = (int16_t)(((int32_t)(width) * 3) / 2); \
        int16_t rad = ptr_len + expand; \
        tmp_area.x1 = cx - rad; \
        tmp_area.x2 = cx + rad; \
        tmp_area.y1 = cy - rad; \
        tmp_area.y2 = cy + rad; \
        if (first) { total_area = tmp_area; first = false; } \
        else { sgl_area_selfmerge(&total_area, &tmp_area); } \
    } while(0)

    CALC_MAX_NEEDLE_AREA_INT(500, clock->hour_ptr_width);
    CALC_MAX_NEEDLE_AREA_INT(700, clock->min_ptr_width);
    CALC_MAX_NEEDLE_AREA_INT(850, clock->sec_ptr_width);

    #undef CALC_MAX_NEEDLE_AREA_INT

    if (!first) {
        sgl_area_selfclip(&total_area, &clock->obj.area);
        sgl_update_area(&total_area);
    }
}

static void sgl_analogclock_construct_cb(sgl_surf_t *surf, sgl_obj_t* obj, sgl_event_t *evt)
{
    sgl_analogclock_t *clock = sgl_container_of(obj, sgl_analogclock_t, obj);
    
    if (evt->type == SGL_EVENT_DRAW_MAIN) {
        const int16_t cx = (obj->coords.x1 + obj->coords.x2) / 2;
        const int16_t cy = (obj->coords.y1 + obj->coords.y2) / 2;
        const int16_t r = sgl_max(obj->radius, sgl_obj_get_width(obj) / 2 - 1);
        
        const int16_t border_w = sgl_min(obj->border, r);
        const int16_t inner_r = r - border_w;
        const int16_t h_len = inner_r / 2;
        const int16_t m_len = (inner_r * 160) >> 8;
        const int16_t s_len_1 = (inner_r * 217) >> 8;
        const int16_t s_len_2 = (inner_r * 39) >> 8;

        sgl_color_t sub_scale_color = sgl_color_mixer(clock->scale_color, clock->bg_color, 128);
        if (inner_r <= 0) return; 

        const int16_t hub_r = sgl_max(5, clock->hub_r);
        const int16_t scale_out = inner_r - 2; 
        const int16_t scale_in = scale_out - sgl_max(clock->scale_len, 4);
        sgl_draw_fill_circle(surf, &obj->area, cx, cy, r, clock->bg_color, clock->alpha);

        if (border_w > 0) {
            sgl_draw_fill_ring(surf, &obj->area, cx, cy, inner_r, r, clock->bg_color, clock->alpha);
        }

        for (int i = 0, j = 0; i < 60; i++, j++) {
            int16_t angle = i * 6;
            int16_t calc_angle = angle - 90;
            int32_t sin_val = sgl_sin(calc_angle);
            int32_t cos_val = sgl_cos(calc_angle);

            int32_t x_out = (scale_out * cos_val) / SGL_SIN_FIXED_ONE + cx;
            int32_t y_out = (scale_out * sin_val) / SGL_SIN_FIXED_ONE + cy;
            int32_t x_in  = (scale_in * cos_val) / SGL_SIN_FIXED_ONE + cx;
            int32_t y_in  = (scale_in * sin_val) / SGL_SIN_FIXED_ONE + cy;

            if (j == 5) {
                sgl_draw_line_fill_slanted(surf, &obj->area, x_out, y_out, x_in, y_in, 
                                   clock->scale_width, clock->scale_color, clock->alpha);
                j = 0;
            }
            else {
                sgl_draw_line_fill_slanted(surf, &obj->area, x_out, y_out, x_in, y_in, 
                                   clock->scale_width, sub_scale_color, clock->alpha);
            }

            if (clock->font && j == 0) {
                char text[4];
                sgl_sprintf(text, "%d", i == 0 ? 12 : i / 5);
                int16_t text_r = scale_in - sgl_font_get_height(clock->font) - 2;
                int32_t tx = (text_r * cos_val) / SGL_SIN_FIXED_ONE + cx;
                int32_t ty = (text_r * sin_val) / SGL_SIN_FIXED_ONE + cy;
                int16_t tw = sgl_font_get_string_width(text, clock->font);
                int16_t th = sgl_font_get_height(clock->font);

                sgl_draw_string(surf, &obj->area, 
                                tx - tw / 2, ty - th / 2, 
                                text, clock->text_color, clock->alpha, clock->font);
            }
        }

        int16_t h_angle = ((clock->hour % 12) * 30 + clock->min / 2) - 90;
        int16_t m_angle = (clock->min * 6) - 90;
        int16_t s_angle = (clock->sec * 6) - 90;

        {
            int32_t n_sin = sgl_sin(h_angle);
            int32_t n_cos = sgl_cos(h_angle);
            int32_t px = (h_len * n_cos) / SGL_SIN_FIXED_ONE + cx;
            int32_t py = (h_len * n_sin) / SGL_SIN_FIXED_ONE + cy;

            int32_t sx = cx + (s_len_2 * n_cos) / SGL_SIN_FIXED_ONE;
            int32_t sy = cy + (s_len_2 * n_sin) / SGL_SIN_FIXED_ONE;

            sgl_draw_line_fill_slanted(surf, &obj->area, sx, sy, px, py, 
                                   clock->hour_ptr_width, clock->hour_ptr_color, clock->alpha);
            sgl_draw_line_fill_slanted(surf, &obj->area, cx, cy, sx, sy,
                                   clock->sec_ptr_width, clock->hour_ptr_color, clock->alpha);
        }

        {
            int32_t n_sin = sgl_sin(m_angle);
            int32_t n_cos = sgl_cos(m_angle);

            int32_t px = (m_len * n_cos) / SGL_SIN_FIXED_ONE + cx;
            int32_t py = (m_len * n_sin) / SGL_SIN_FIXED_ONE + cy;

            int32_t sx = cx + (s_len_2 * n_cos) / SGL_SIN_FIXED_ONE;
            int32_t sy = cy + (s_len_2 * n_sin) / SGL_SIN_FIXED_ONE;

            sgl_draw_line_fill_slanted(surf, &obj->area, sx, sy, px, py, 
                                   clock->min_ptr_width, clock->min_ptr_color, clock->alpha);
            sgl_draw_line_fill_slanted(surf, &obj->area, cx, cy, sx, sy,
                                   clock->sec_ptr_width, clock->min_ptr_color, clock->alpha);
        }

        sgl_draw_fill_circle(surf, &obj->area, cx - 1, cy - 1, hub_r + 1, clock->min_ptr_color, clock->alpha);

        {
            int32_t n_sin = sgl_sin(s_angle);
            int32_t n_cos = sgl_cos(s_angle);
            int32_t px = (s_len_1 * n_cos) / SGL_SIN_FIXED_ONE + cx;
            int32_t py = (s_len_1 * n_sin) / SGL_SIN_FIXED_ONE + cy;

            int32_t sx = cx - (s_len_2 * n_cos) / SGL_SIN_FIXED_ONE;
            int32_t sy = cy - (s_len_2 * n_sin) / SGL_SIN_FIXED_ONE;
            sgl_draw_line_fill_slanted(surf, &obj->area, sx, sy, px, py, 
                                   clock->sec_ptr_width, clock->sec_ptr_color, clock->alpha);
        }

        sgl_draw_fill_circle(surf, &obj->area, cx - 1, cy - 1, hub_r, clock->sec_ptr_color, clock->alpha);
        sgl_draw_fill_circle(surf, &obj->area, cx - 1, cy - 1, hub_r - 2, clock->bg_color, clock->alpha);
    }
}

/**
 * @brief Create an analog clock widget.
 * 
 * Allocates memory for a new analog clock object, initializes its properties 
 * with default theme values, and sets up the drawing callback.
 * 
 * @param parent Pointer to the parent object.
 * @return Pointer to the newly created sgl_obj_t object, or NULL if memory allocation fails.
 */
sgl_obj_t* sgl_analogclock_create(sgl_obj_t* parent)
{
    sgl_analogclock_t *clock = sgl_malloc(sizeof(sgl_analogclock_t));
    if (clock == NULL) {
        SGL_LOG_ERROR("sgl_analogclock_create: malloc failed");
        return NULL;
    }

    memset(clock, 0, sizeof(sgl_analogclock_t));

    sgl_obj_t *obj = &clock->obj;
    sgl_obj_init(obj, parent);
    obj->construct_fn = sgl_analogclock_construct_cb;
    sgl_obj_set_border_width(obj, 0);

    clock->alpha = SGL_THEME_ALPHA;
    clock->bg_color = SGL_THEME_BG_COLOR;
    clock->scale_color = SGL_THEME_COLOR;
    clock->text_color = SGL_THEME_COLOR;
    clock->border_color = SGL_THEME_COLOR;
    clock->hour_ptr_color = SGL_THEME_COLOR;
    clock->min_ptr_color = SGL_THEME_COLOR;
    clock->sec_ptr_color = SGL_COLOR_RED;

    clock->scale_width = 1;
    clock->hour_ptr_width = 5;
    clock->min_ptr_width = 5;
    clock->sec_ptr_width = 2;
    clock->hub_r = 6;
    clock->scale_len = 8;
    clock->font = sgl_get_system_font();

    return obj;
}

/**
 * @brief Set the time value for the analog clock.
 * 
 * Updates the hour, minute, and second values of the clock. If the new time 
 * differs from the current time, it updates the clock's internal state and 
 * triggers a redraw of the affected area.
 * 
 * @param obj Pointer to the analog clock object.
 * @param hour The hour value (0-23).
 * @param min The minute value (0-59).
 * @param sec The second value (0-59).
 */
void sgl_analogclock_set_time(sgl_obj_t *obj, uint8_t hour, uint8_t min, uint8_t sec)
{
    sgl_analogclock_t *clock = sgl_container_of(obj, sgl_analogclock_t, obj);
    
    hour = hour % 24;
    min = min > 59 ? 59 : min;
    sec = sec > 59 ? 59 : sec;

    if (clock->hour != hour || clock->min != min || clock->sec != sec) {
        clock->hour = hour;
        clock->min = min;
        clock->sec = sec;
        analogclock_update_area(clock);
    }
}

/**
 * @brief Set the font for the clock's hour numbers.
 * @param obj Pointer to the analog clock object.
 * @param font Pointer to the font to be used.
 */
void sgl_analogclock_set_font(sgl_obj_t *obj, const sgl_font_t *font)
{
    sgl_analogclock_t *clock = sgl_container_of(obj, sgl_analogclock_t, obj);
    clock->font = font;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set the background color of the clock.
 * @param obj Pointer to the analog clock object.
 * @param color The background color to set.
 */
void sgl_analogclock_set_bg_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_analogclock_t *clock = sgl_container_of(obj, sgl_analogclock_t, obj);
    clock->bg_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set the border color of the clock.
 * @param obj Pointer to the analog clock object.
 * @param color The border color to set.
 */
void sgl_analogclock_set_border_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_analogclock_t *clock = sgl_container_of(obj, sgl_analogclock_t, obj);
    clock->border_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set the scale (tick marks) color of the clock.
 * @param obj Pointer to the analog clock object.
 * @param color The scale color to set.
 */
void sgl_analogclock_set_scale_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_analogclock_t *clock = sgl_container_of(obj, sgl_analogclock_t, obj);
    clock->scale_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set the hour pointer (hand) color of the clock.
 * @param obj Pointer to the analog clock object.
 * @param color The hour pointer color to set.
 */
void sgl_analogclock_set_hour_ptr_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_analogclock_t *clock = sgl_container_of(obj, sgl_analogclock_t, obj);
    clock->hour_ptr_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set the minute pointer (hand) color of the clock.
 * @param obj Pointer to the analog clock object.
 * @param color The minute pointer color to set.
 */
void sgl_analogclock_set_min_ptr_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_analogclock_t *clock = sgl_container_of(obj, sgl_analogclock_t, obj);
    clock->min_ptr_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set the second pointer (hand) color of the clock.
 * @param obj Pointer to the analog clock object.
 * @param color The second pointer color to set.
 */
void sgl_analogclock_set_sec_ptr_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_analogclock_t *clock = sgl_container_of(obj, sgl_analogclock_t, obj);
    clock->sec_ptr_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set the text (hour numbers) color of the clock.
 * @param obj Pointer to the analog clock object.
 * @param color The text color to set.
 */
void sgl_analogclock_set_text_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_analogclock_t *clock = sgl_container_of(obj, sgl_analogclock_t, obj);
    clock->text_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set the border width of the clock.
 * @param obj Pointer to the analog clock object.
 * @param width The border width to set.
 */
void sgl_analogclock_set_border_width(sgl_obj_t *obj, uint8_t width)
{
    sgl_obj_set_border_width(obj, width);
}

/**
 * @brief Set the center hub radius of the clock.
 * @param obj Pointer to the analog clock object.
 * @param radius The hub radius to set.
 */
void sgl_analogclock_set_hub_radius(sgl_obj_t *obj, uint8_t radius)
{
    sgl_analogclock_t *clock = sgl_container_of(obj, sgl_analogclock_t, obj);
    clock->hub_r = radius;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set the scale (tick marks) width of the clock.
 * @param obj Pointer to the analog clock object.
 * @param width The scale width to set.
 */
void sgl_analogclock_set_scale_width(sgl_obj_t *obj, uint8_t width)
{
    sgl_analogclock_t *clock = sgl_container_of(obj, sgl_analogclock_t, obj);
    clock->scale_width = width;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set the hour pointer (hand) width of the clock.
 * @param obj Pointer to the analog clock object.
 * @param width The hour pointer width to set.
 */
void sgl_analogclock_set_hour_ptr_width(sgl_obj_t *obj, uint8_t width)
{
    sgl_analogclock_t *clock = sgl_container_of(obj, sgl_analogclock_t, obj);
    clock->hour_ptr_width = width;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set the minute pointer (hand) width of the clock.
 * @param obj Pointer to the analog clock object.
 * @param width The minute pointer width to set.
 */
void sgl_analogclock_set_min_ptr_width(sgl_obj_t *obj, uint8_t width)
{
    sgl_analogclock_t *clock = sgl_container_of(obj, sgl_analogclock_t, obj);
    clock->min_ptr_width = width;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set the second pointer (hand) width of the clock.
 * @param obj Pointer to the analog clock object.
 * @param width The second pointer width to set.
 */
void sgl_analogclock_set_sec_ptr_width(sgl_obj_t *obj, uint8_t width)
{
    sgl_analogclock_t *clock = sgl_container_of(obj, sgl_analogclock_t, obj);
    clock->sec_ptr_width = width;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set the overall alpha (transparency) of the clock.
 * @param obj Pointer to the analog clock object.
 * @param alpha The alpha value to set (0-255).
 */
void sgl_analogclock_set_alpha(sgl_obj_t *obj, uint8_t alpha)
{
    sgl_analogclock_t *clock = sgl_container_of(obj, sgl_analogclock_t, obj);
    clock->alpha = alpha;
    sgl_obj_set_dirty(obj);
}
