/* source/widgets/chart/linechart/sgl_linechart.c
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

#include <limits.h>
#include <sgl_core.h>
#include <sgl_draw.h>
#include <sgl_math.h>
#include <sgl_log.h>
#include <sgl_mm.h>
#include <sgl_theme.h>
#include <sgl_cfgfix.h>
#include <string.h>
#include "sgl_linechart.h"

#define SGL_LINECHART_TICK_LEN 4


static void sgl_linechart_construct_cb(sgl_surf_t *surf, sgl_obj_t* obj, sgl_event_t *evt);
static void sgl_linechart_update_axis_auto(sgl_linechart_t *chart);
static int32_t sgl_linechart_get_effective_step(const sgl_linechart_axis_t *axis);
static void sgl_linechart_format_value(char *buf, size_t size, int32_t value);
static void sgl_linechart_draw_grid_and_labels(sgl_surf_t *surf, sgl_obj_t *obj,
                                               sgl_linechart_t *chart,
                                               const sgl_area_t *plot_rect,
                                               const sgl_area_t *plot_clip);
static void sgl_linechart_draw_series(sgl_surf_t *surf, sgl_obj_t *obj,
                                      sgl_linechart_t *chart,
                                      const sgl_area_t *plot_rect,
                                      const sgl_area_t *plot_clip);
static void sgl_linechart_draw_dashed_line(sgl_surf_t *surf, sgl_area_t *area,
                                           int16_t x0, int16_t y0,
                                           int16_t x1, int16_t y1,
                                           uint16_t dash_len, uint16_t gap_len,
                                           sgl_color_t color, uint8_t alpha);
static void sgl_linechart_draw_segment_line(sgl_surf_t *surf, const sgl_area_t *area,
                                            int16_t x0, int16_t y0,
                                            int16_t x1, int16_t y1,
                                            sgl_color_t color, uint8_t width, uint8_t alpha);
static void sgl_linechart_fill_segment_under(sgl_surf_t *surf, const sgl_area_t *area,
                                             int16_t x0, int16_t y0,
                                             int16_t x1, int16_t y1,
                                             int16_t baseline_y,
                                             sgl_color_t color, uint8_t alpha);
static void sgl_linechart_draw_point_marker(sgl_surf_t *surf, const sgl_area_t *area,
                                            int16_t x, int16_t y,
                                            sgl_linechart_point_shape_t shape,
                                            uint8_t radius,
                                            sgl_color_t color, uint8_t alpha);


/**
 * @brief Create a linechart widget
 * @param parent parent object
 * @return linechart object
 */
sgl_obj_t* sgl_linechart_create(sgl_obj_t *parent)
{
    sgl_linechart_t *chart = sgl_malloc(sizeof(sgl_linechart_t));
    if (chart == NULL) {
        SGL_LOG_ERROR("sgl_linechart_create: malloc failed");
        return NULL;
    }

    memset(chart, 0, sizeof(sgl_linechart_t));

    sgl_obj_t *obj = &chart->obj;
    sgl_obj_init(obj, parent);
    obj->construct_fn = sgl_linechart_construct_cb;
    sgl_obj_set_border_width(obj, SGL_THEME_BORDER_WIDTH);

    chart->alpha = SGL_ALPHA_MAX;
    chart->bg_color = SGL_THEME_BG_COLOR;
    chart->bg_alpha = SGL_THEME_ALPHA;
    chart->border_color = SGL_THEME_BORDER_COLOR;

    /* X axis default */
    chart->x_axis.min           = 0;
    chart->x_axis.max           = 100;
    chart->x_axis.step          = 0;
    chart->x_axis.flag_bits.auto_scale = 1U;
    chart->x_axis.flag_bits.show_grid = 1U;
    chart->x_axis.flag_bits.grid_dashed = 1U;
    chart->x_axis.flag_bits.show_labels = 1U;
    chart->x_axis.auto_divisions= SGL_LINECHART_DEFAULT_DIVISIONS;
    chart->x_axis.grid_color    = sgl_rgb(60, 60, 60);
    chart->x_axis.grid_alpha    = 80;
    chart->x_axis.label_font    = sgl_get_system_font();
    chart->x_axis.label_color   = SGL_THEME_TEXT_COLOR;
    chart->x_axis.label_alpha   = SGL_ALPHA_MAX;

    /* Y axis default */
    chart->y_axis.min           = 0;
    chart->y_axis.max           = 100;
    chart->y_axis.step          = 0;
    chart->y_axis.flag_bits.auto_scale = 1U;
    chart->y_axis.flag_bits.show_grid = 1U;
    chart->y_axis.flag_bits.grid_dashed = 1U;
    chart->y_axis.flag_bits.show_labels = 1U;
    chart->y_axis.auto_divisions= SGL_LINECHART_DEFAULT_DIVISIONS;
    chart->y_axis.grid_color    = sgl_rgb(60, 60, 60);
    chart->y_axis.grid_alpha    = 80;
    chart->y_axis.label_font    = sgl_get_system_font();
    chart->y_axis.label_color   = SGL_THEME_TEXT_COLOR;
    chart->y_axis.label_alpha   = SGL_ALPHA_MAX;

    chart->series_count         = 0;
    chart->series               = NULL;
    chart->x_labels             = NULL;
    chart->x_label_count        = 0;
 
    chart->option_bits.open_anim_enable = 0U;
    chart->option_bits.open_anim_playing = 0U;
    chart->option_bits.open_anim_dir = SGL_LINECHART_OPEN_ANIM_FROM_LEFT;
    chart->open_anim_start_tick = 0;
#if (CONFIG_SGL_ANIMATION)
    chart->open_anim_path       = NULL;
#endif

    return obj;
}


/**
 * @brief Set series count and reallocate series array
 */
void sgl_linechart_set_series_count(sgl_obj_t *obj, uint8_t count)
{
    SGL_ASSERT(obj != NULL);
    sgl_linechart_t *chart = sgl_container_of(obj, sgl_linechart_t, obj);

    if (chart->series && chart->series_count) {
        sgl_free(chart->series);
        chart->series = NULL;
        chart->series_count = 0;
    }

    if (count == 0) {
        sgl_obj_set_dirty(obj);
        return;
    }

    sgl_linechart_series_t *series = sgl_malloc(sizeof(sgl_linechart_series_t) * count);
    if (series == NULL) {
        SGL_LOG_ERROR("sgl_linechart_set_series_count: malloc failed");
        sgl_obj_set_dirty(obj);
        return;
    }

    memset(series, 0, sizeof(sgl_linechart_series_t) * count);

    for (uint8_t i = 0; i < count; i++) {
        series[i].style_bits.show_line = 1U;
        series[i].style_bits.show_points = 1U;
        series[i].style_bits.fill_under = 0U;
        series[i].style_bits.point_shape = SGL_LINECHART_POINT_SHAPE_CIRCLE;
        series[i].line_width   = 2;
        series[i].point_radius = 3;
        series[i].line_color   = SGL_THEME_COLOR;
        series[i].line_alpha   = SGL_ALPHA_MAX;
        series[i].fill_color   = SGL_THEME_COLOR;
        series[i].fill_alpha   = 0;
        series[i].label        = NULL;
        series[i].x_data       = NULL;
        series[i].y_data       = NULL;
        series[i].point_count  = 0;
    }

    chart->series = series;
    chart->series_count = count;
    sgl_obj_set_dirty(obj);
}


/**
 * @brief Bind data buffers for a series
 */
void sgl_linechart_set_series_data(sgl_obj_t *obj, uint8_t index,
                                   const int32_t *x_data,
                                   const int32_t *y_data,
                                   uint16_t point_count)
{
    SGL_ASSERT(obj != NULL);
    sgl_linechart_t *chart = sgl_container_of(obj, sgl_linechart_t, obj);
 
    if (chart->series == NULL || index >= chart->series_count) {
        SGL_LOG_ERROR("sgl_linechart_set_series_data: invalid index %d", index);
        return;
    }
 
    sgl_linechart_series_t *s = &chart->series[index];
    s->x_data = x_data;
    s->y_data = y_data;
    s->point_count = point_count;
 
    sgl_obj_set_dirty(obj);
}
 
 
/**
 * @brief Bind only Y data array for a series (X uses index 0..point_count-1).
 */
void sgl_linechart_set_series_y_array(sgl_obj_t *obj, uint8_t index,
                                      const int32_t *y_data,
                                      uint16_t point_count)
{
    sgl_linechart_set_series_data(obj, index, NULL, y_data, point_count);
}
 
 
/**
 * @brief Set X axis label string array.
 */
void sgl_linechart_set_x_labels(sgl_obj_t *obj, const char **labels, uint8_t count)
{
    SGL_ASSERT(obj != NULL);
    sgl_linechart_t *chart = sgl_container_of(obj, sgl_linechart_t, obj);
    chart->x_labels      = labels;
    chart->x_label_count = count;
    sgl_obj_set_dirty(obj);
}

void sgl_linechart_update_value(sgl_obj_t *obj, uint8_t series_index, uint16_t point_index)
{
    SGL_ASSERT(obj != NULL);
    sgl_linechart_t *chart = sgl_container_of(obj, sgl_linechart_t, obj);

    if (chart->series == NULL || series_index >= chart->series_count) {
        SGL_LOG_ERROR("sgl_linechart_update_value: invalid series index %d", series_index);
        return;
    }
    if (chart->series[series_index].y_data == NULL || point_index >= chart->series[series_index].point_count) {
        SGL_LOG_ERROR("sgl_linechart_update_value: invalid point index %d", point_index);
        return;
    }

    sgl_obj_set_dirty(obj);
}
 
 
/**
 * @brief Draw dashed line (Bresenham with pattern)
 */
static void sgl_linechart_draw_dashed_line(sgl_surf_t *surf, sgl_area_t *area,
                                           int16_t x0, int16_t y0,
                                           int16_t x1, int16_t y1,
                                           uint16_t dash_len, uint16_t gap_len,
                                           sgl_color_t color, uint8_t alpha)
{
    if (dash_len == 0 || gap_len == 0) {
        /* Fallback to solid line */
        sgl_draw_line_t desc = {
            .alpha = alpha,
            .width = 4,
            .color = color,
            .x1    = x0,
            .y1    = y0,
            .x2    = x1,
            .y2    = y1,
        };
        sgl_draw_line(surf, area, &desc);
        return;
    }

    int16_t dx = sgl_abs(x1 - x0);
    int16_t dy = sgl_abs(y1 - y0);
    int16_t sx = (x0 < x1) ? 1 : -1;
    int16_t sy = (y0 < y1) ? 1 : -1;
    int16_t err = dx - dy;
    int16_t e2;

    uint32_t pattern_len = (uint32_t)dash_len + (uint32_t)gap_len;
    uint32_t pattern_pos = 0;

    sgl_area_t clip_area = {
        .x1 = surf->x1,
        .y1 = surf->y1,
        .x2 = surf->x2,
        .y2 = surf->y2,
    };

    sgl_area_selfclip(&clip_area, area);

    while (1) {
        if (pattern_pos < dash_len) {
            if (x0 >= clip_area.x1 && x0 <= clip_area.x2 &&
                y0 >= clip_area.y1 && y0 <= clip_area.y2) {

                sgl_color_t *buf = sgl_surf_get_buf(surf, x0 - surf->x1, y0 - surf->y1);
                if (alpha == SGL_ALPHA_MAX) {
                    *buf = color;
                } else {
                    *buf = sgl_color_mixer(color, *buf, alpha);
                }
            }
        }

        if (x0 == x1 && y0 == y1) {
            break;
        }

        pattern_pos++;
        if (pattern_pos >= pattern_len) {
            pattern_pos = 0;
        }

        e2 = err << 1;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}


/**
 * @brief Draw a line segment with variable width using SDF-based line
 */
static void sgl_linechart_draw_segment_line(sgl_surf_t *surf, const sgl_area_t *area,
                                            int16_t x0, int16_t y0,
                                            int16_t x1, int16_t y1,
                                            sgl_color_t color, uint8_t width, uint8_t alpha)
{
    if (width == 0 || alpha == SGL_ALPHA_MIN) {
        return;
    }

    uint8_t eff_width;
   if (width <= 1) {
        eff_width = 1;          /* logical 1 -> 1px line */
    }
    else if (width == 2) {
        eff_width = 2;          /* logical 2 */
    }
    else {
        eff_width = width;
    }

    sgl_draw_line_t desc = {
        .alpha = alpha,
        .width = eff_width,
        .color = color,
        .x1    = x0,
        .y1    = y0,
        .x2    = x1,
        .y2    = y1,
    };

    sgl_draw_line(surf, (sgl_area_t *)area, &desc);
}


/**
 * @brief Fill area under a segment down to baseline using vertical strips
 */
static void sgl_linechart_fill_segment_under(sgl_surf_t *surf, const sgl_area_t *area,
                                             int16_t x0, int16_t y0,
                                             int16_t x1, int16_t y1,
                                             int16_t baseline_y,
                                             sgl_color_t color, uint8_t alpha)
{
    if (alpha == SGL_ALPHA_MIN) {
        return;
    }

    int16_t dx = x1 - x0;
    int16_t dy = y1 - y0;
    int16_t steps = sgl_abs(dx);

    if (steps == 0) {
        int16_t y_top = sgl_min(y0, baseline_y);
        int16_t y_bottom = sgl_max(y0, baseline_y);
        sgl_draw_fill_vline(surf, (sgl_area_t *)area, x0, y_top, y_bottom, 1, color, alpha);
        return;
    }

    for (int16_t i = 0; i <= steps; i++) {
        int16_t x = x0 + (int32_t)dx * i / steps;
        int16_t y = y0 + (int32_t)dy * i / steps;

        int16_t y_top = sgl_min(y, baseline_y);
        int16_t y_bottom = sgl_max(y, baseline_y);
        if (i!=0)//group member @heliangzhen provided a fix for the line drawing issue
        {
            sgl_draw_fill_vline(surf, (sgl_area_t *)area, x, y_top, y_bottom, 1, color, alpha);
        }
    }
}


/**
 * @brief Draw point marker (circle or square)
 */
static void sgl_linechart_draw_point_marker(sgl_surf_t *surf, const sgl_area_t *area,
                                            int16_t x, int16_t y,
                                            sgl_linechart_point_shape_t shape,
                                            uint8_t radius,
                                            sgl_color_t color, uint8_t alpha)
{
    if (shape == SGL_LINECHART_POINT_SHAPE_NONE || radius == 0 || alpha == SGL_ALPHA_MIN) {
        return;
    }

    if (shape == SGL_LINECHART_POINT_SHAPE_CIRCLE) {
        sgl_draw_fill_circle(surf, (sgl_area_t *)area, x, y, radius, color, alpha);
    }
    else if (shape == SGL_LINECHART_POINT_SHAPE_SQUARE) {
        sgl_area_t rect = {
            .x1 = x - (int16_t)radius,
            .y1 = y - (int16_t)radius,
            .x2 = x + (int16_t)radius,
            .y2 = y + (int16_t)radius,
        };
        sgl_draw_fill_rect(surf, (sgl_area_t *)area, &rect, 0, color, alpha);
    }
}


/**
 * @brief Format integer value into decimal string
 */
static void sgl_linechart_format_value(char *buf, size_t size, int32_t value)
{
    if (size == 0) {
        return;
    }

    size_t pos = 0;
    if (value == 0) {
        buf[0] = '0';
        if (size > 1) buf[1] = '\0';
        return;
    }

    bool neg = (value < 0);
    uint32_t v = (uint32_t)(neg ? -value : value);
    char tmp[16];
    size_t i = 0;

    while (v && i < sizeof(tmp)) {
        tmp[i++] = (char)('0' + (v % 10U));
        v /= 10U;
    }

    if (neg && pos < size - 1) {
        buf[pos++] = '-';
    }

    while (i > 0 && pos < size - 1) {
        buf[pos++] = tmp[--i];
    }

    buf[pos] = '\0';
}


/**
 * @brief Compute effective tick step for axis
 */
static int32_t sgl_linechart_get_effective_step(const sgl_linechart_axis_t *axis)
{
    int32_t range = axis->max - axis->min;
    if (range <= 0) {
        return 1;
    }

    if (axis->step > 0) {
        return axis->step;
    }

    uint8_t div = axis->auto_divisions ? axis->auto_divisions : SGL_LINECHART_DEFAULT_DIVISIONS;
    int32_t step = range / (int32_t)div;
    if (step <= 0) {
        step = 1;
    }

    return step;
}


/**
 * @brief Update axis min/max when auto-scale is enabled
 */
static void sgl_linechart_update_axis_auto(sgl_linechart_t *chart)
{
    int32_t data_min_x = INT32_MAX;
    int32_t data_max_x = INT32_MIN;
    int32_t data_min_y = INT32_MAX;
    int32_t data_max_y = INT32_MIN;

    for (uint8_t i = 0; i < chart->series_count; i++) {
        sgl_linechart_series_t *s = &chart->series[i];
        if (s->y_data == NULL || s->point_count == 0) {
            continue;
        }

        for (uint16_t j = 0; j < s->point_count; j++) {
            int32_t vy = s->y_data[j];
            if (vy < data_min_y) data_min_y = vy;
            if (vy > data_max_y) data_max_y = vy;

            int32_t vx = s->x_data ? s->x_data[j] : (int32_t)j;
            if (vx < data_min_x) data_min_x = vx;
            if (vx > data_max_x) data_max_x = vx;
        }
    }

    if (data_min_x == INT32_MAX || data_min_y == INT32_MAX) {
        /* No data, keep previous or set default ranges */
        if (chart->x_axis.flag_bits.auto_scale) {
            chart->x_axis.min = 0;
            chart->x_axis.max = 10;
        }
        if (chart->y_axis.flag_bits.auto_scale) {
            chart->y_axis.min = 0;
            chart->y_axis.max = 10;
        }
        return;
    }

    if (chart->x_axis.flag_bits.auto_scale) {
        if (data_min_x == data_max_x) {
            chart->x_axis.min = data_min_x - 1;
            chart->x_axis.max = data_max_x + 1;
        }
        else {
            int32_t range = data_max_x - data_min_x;
            int32_t margin = range / 10;
            if (margin <= 0) margin = 1;
            chart->x_axis.min = data_min_x - margin;
            chart->x_axis.max = data_max_x + margin;
        }
    }

    if (chart->y_axis.flag_bits.auto_scale) {
        if (data_min_y == data_max_y) {
            chart->y_axis.min = data_min_y - 1;
            chart->y_axis.max = data_max_y + 1;
        }
        else {
            int32_t range = data_max_y - data_min_y;
            int32_t margin = range / 10;
            if (margin <= 0) margin = 1;
            chart->y_axis.min = data_min_y - margin;
            chart->y_axis.max = data_max_y + margin;
        }
    }
}


/**
 * @brief Draw grid lines and axis labels
 */
static void sgl_linechart_draw_grid_and_labels(sgl_surf_t *surf, sgl_obj_t *obj,
                                               sgl_linechart_t *chart,
                                               const sgl_area_t *plot_rect,
                                               const sgl_area_t *plot_clip)
{
    const sgl_font_t *x_font = chart->x_axis.label_font ? chart->x_axis.label_font : sgl_get_system_font();
    const sgl_font_t *y_font = chart->y_axis.label_font ? chart->y_axis.label_font : sgl_get_system_font();

    int16_t plot_w = plot_rect->x2 - plot_rect->x1;
    int16_t plot_h = plot_rect->y2 - plot_rect->y1;
    if (plot_w <= 0 || plot_h <= 0) {
        return;
    }

    int32_t x_range = chart->x_axis.max - chart->x_axis.min;
    int32_t y_range = chart->y_axis.max - chart->y_axis.min;
    if (x_range <= 0) x_range = 1;
    if (y_range <= 0) y_range = 1;

    uint8_t base_alpha = chart->alpha ? chart->alpha : SGL_ALPHA_MAX;

    /* Y axis: horizontal grid and labels (left side) */
    if (chart->y_axis.flag_bits.show_grid || (chart->y_axis.flag_bits.show_labels && y_font)) {
        int32_t step = sgl_linechart_get_effective_step(&chart->y_axis);
        int32_t v = chart->y_axis.min;
        uint8_t tick_idx = 0;
        char buf[16];

        sgl_area_t grid_area = *plot_clip;
        sgl_area_t y_label_area = {
            .x1 = obj->coords.x1 + 2,
            .y1 = plot_rect->y1,
            .x2 = plot_rect->x1 - 2,
            .y2 = plot_rect->y2,
        };
        sgl_area_t y_tick_area = {
            .x1 = obj->coords.x1,
            .y1 = plot_rect->y1,
            .x2 = (int16_t)(plot_rect->x1 - 1),
            .y2 = plot_rect->y2,
        };
        sgl_area_selfclip(&y_tick_area, &obj->area);

        uint8_t grid_alpha = chart->y_axis.grid_alpha;
        if (grid_alpha) {
            uint16_t mix = (uint16_t)grid_alpha * base_alpha / 255;
            if (mix == 0) mix = grid_alpha;
            grid_alpha = (uint8_t)mix;
        }

        uint8_t label_alpha = chart->y_axis.label_alpha;
        if (label_alpha) {
            uint16_t mix = (uint16_t)label_alpha * base_alpha / 255;
            if (mix == 0) mix = label_alpha;
            label_alpha = (uint8_t)mix;
        }

        while (tick_idx < SGL_LINECHART_MAX_AUTO_TICKS && v <= chart->y_axis.max) {
            int16_t y = plot_rect->y2 - (int32_t)(v - chart->y_axis.min) * plot_h / y_range;

            if (chart->y_axis.flag_bits.show_grid && grid_alpha) {
                if (chart->y_axis.flag_bits.grid_dashed) {
                    sgl_linechart_draw_dashed_line(surf, &grid_area,
                                                   plot_rect->x1, y,
                                                   plot_rect->x2, y,
                                                   6, 4,
                                                   chart->y_axis.grid_color, grid_alpha);
                }
                else {
                    sgl_draw_fill_hline(surf, &grid_area, y,
                                        plot_rect->x1, plot_rect->x2,
                                        1, chart->y_axis.grid_color, grid_alpha);
                }
            }

            if (chart->y_axis.flag_bits.show_labels && y_font && label_alpha && y_label_area.x2 > y_label_area.x1) {
                sgl_linechart_format_value(buf, sizeof(buf), v);

                int16_t text_y = y - (int16_t)y_font->font_height / 2;
                if (text_y < y_label_area.y1) {
                    text_y = y_label_area.y1;
                }
                if (text_y > y_label_area.y2 - (int16_t)y_font->font_height) {
                    text_y = y_label_area.y2 - (int16_t)y_font->font_height;
                }

                sgl_draw_string(surf, &y_label_area,
                                y_label_area.x1,
                                text_y,
                                buf,
                                chart->y_axis.label_color,
                                label_alpha,
                                y_font);
            }

            if (chart->y_axis.flag_bits.show_ticks && grid_alpha && y_tick_area.x2 >= y_tick_area.x1) {
                sgl_draw_fill_hline(surf, &y_tick_area, y,
                                    (int16_t)(plot_rect->x1 - SGL_LINECHART_TICK_LEN),
                                    (int16_t)(plot_rect->x1 - 1),
                                    1, chart->y_axis.grid_color, grid_alpha);
            }

            tick_idx++;
            v += step;
        }
    }

    /* X axis: vertical grid and labels (bottom side) */
    if (chart->x_axis.flag_bits.show_grid || (chart->x_axis.flag_bits.show_labels && x_font)) {
        int32_t step = sgl_linechart_get_effective_step(&chart->x_axis);
        int32_t v = chart->x_axis.min;
        uint8_t tick_idx = 0;
        char buf[16];

        sgl_area_t grid_area = *plot_clip;
        sgl_area_t x_label_area = {
            .x1 = plot_rect->x1,
            .y1 = plot_rect->y2 + 2,
            .x2 = obj->coords.x2,
            .y2 = obj->coords.y2,
        };
        sgl_area_t x_tick_area = {
            .x1 = plot_rect->x1,
            .y1 = (int16_t)(plot_rect->y2 + 1),
            .x2 = plot_rect->x2,
            .y2 = obj->coords.y2,
        };
        sgl_area_selfclip(&x_tick_area, &obj->area);

        uint8_t grid_alpha = chart->x_axis.grid_alpha;
        if (grid_alpha) {
            uint16_t mix = (uint16_t)grid_alpha * base_alpha / 255;
            if (mix == 0) mix = grid_alpha;
            grid_alpha = (uint8_t)mix;
        }

        uint8_t label_alpha = chart->x_axis.label_alpha;
        if (label_alpha) {
            uint16_t mix = (uint16_t)label_alpha * base_alpha / 255;
            if (mix == 0) mix = label_alpha;
            label_alpha = (uint8_t)mix;
        }

        while (tick_idx < SGL_LINECHART_MAX_AUTO_TICKS && v <= chart->x_axis.max) {
            int16_t x = plot_rect->x1 + (int32_t)(v - chart->x_axis.min) * plot_w / x_range;

            if (chart->x_axis.flag_bits.show_grid && grid_alpha) {
                if (chart->x_axis.flag_bits.grid_dashed) {
                    sgl_linechart_draw_dashed_line(surf, &grid_area,
                                                   x, plot_rect->y1,
                                                   x, plot_rect->y2,
                                                   6, 4,
                                                   chart->x_axis.grid_color, grid_alpha);
                }
                else {
                    sgl_draw_fill_vline(surf, &grid_area, x,
                                        plot_rect->y1, plot_rect->y2,
                                        1, chart->x_axis.grid_color, grid_alpha);
                }
            }

            if (chart->x_axis.flag_bits.show_labels && x_font && label_alpha && x_label_area.y2 > x_label_area.y1) {
                const char *label_str = NULL;
 
                if (chart->x_labels && tick_idx < chart->x_label_count && chart->x_labels[tick_idx]) {
                    /* Use user-provided X axis label string */
                    label_str = chart->x_labels[tick_idx];
                }
                else {
                    /* Fallback: numeric label */
                    sgl_linechart_format_value(buf, sizeof(buf), v);
                    label_str = buf;
                }
 
                int32_t text_w = sgl_font_get_string_width(label_str, x_font);
 
                int16_t text_x = x - (int16_t)text_w / 2;
                if (text_x < x_label_area.x1) {
                    text_x = x_label_area.x1;
                }
                if (text_x > x_label_area.x2 - (int16_t)text_w) {
                    text_x = x_label_area.x2 - (int16_t)text_w;
                }
 
                int16_t text_y = x_label_area.y1;
                if (text_y > x_label_area.y2 - (int16_t)x_font->font_height) {
                    text_y = x_label_area.y2 - (int16_t)x_font->font_height;
                }
 
                sgl_draw_string(surf, &x_label_area,
                                text_x,
                                text_y,
                                label_str,
                                chart->x_axis.label_color,
                                label_alpha,
                                x_font);
            }

            if (chart->x_axis.flag_bits.show_ticks && grid_alpha && x_tick_area.y2 >= x_tick_area.y1) {
                sgl_draw_fill_vline(surf, &x_tick_area, x,
                                    (int16_t)(plot_rect->y2 + 1),
                                    (int16_t)(plot_rect->y2 + SGL_LINECHART_TICK_LEN),
                                    1, chart->x_axis.grid_color, grid_alpha);
            }

            tick_idx++;
            v += step;
        }
    }
}


/**
 * @brief Draw all data series
 */
static void sgl_linechart_draw_series(sgl_surf_t *surf, sgl_obj_t *obj,
                                      sgl_linechart_t *chart,
                                      const sgl_area_t *plot_rect,
                                      const sgl_area_t *plot_clip)
{
    int16_t plot_w = plot_rect->x2 - plot_rect->x1;
    int16_t plot_h = plot_rect->y2 - plot_rect->y1;
    if (plot_w <= 0 || plot_h <= 0) {
        return;
    }
 
    int32_t x_range = chart->x_axis.max - chart->x_axis.min;
    int32_t y_range = chart->y_axis.max - chart->y_axis.min;
    if (x_range <= 0) x_range = 1;
    if (y_range <= 0) y_range = 1;
 
    uint8_t base_alpha = chart->alpha ? chart->alpha : SGL_ALPHA_MAX;
    int16_t baseline_y = plot_rect->y2; /* baseline at axis minimum */
 
    /* 为了避免最左/最右的数据点只显示一半，根据点半径为折线图预留水平边距 */
    int16_t pad_x = 0;
    for (uint8_t si = 0; si < chart->series_count; si++) {
        sgl_linechart_series_t *s = &chart->series[si];
        if (s->style_bits.show_points && s->point_radius > pad_x) {
            pad_x = s->point_radius;
        }
    }
 
    int16_t plot_x1 = plot_rect->x1;
    int16_t plot_x2 = plot_rect->x2;
    if (pad_x > 0 && (plot_w > (pad_x * 2))) {
        plot_x1 = (int16_t)(plot_x1 + pad_x);
        plot_x2 = (int16_t)(plot_x2 - pad_x);
        plot_w  = (int16_t)(plot_x2 - plot_x1);
    }
 
    for (uint8_t si = 0; si < chart->series_count; si++) {
        sgl_linechart_series_t *s = &chart->series[si];
        if (s->y_data == NULL || s->point_count == 0) {
            continue;
        }
 
        uint8_t line_alpha = s->line_alpha ? s->line_alpha : SGL_ALPHA_MAX;
        uint16_t mix = (uint16_t)line_alpha * base_alpha / 255;
        if (mix == 0) mix = line_alpha;
        uint8_t eff_line_alpha = (uint8_t)mix;
 
        uint8_t fill_alpha = s->fill_alpha;
        if (fill_alpha) {
            uint16_t mix_fill = (uint16_t)fill_alpha * base_alpha / 255;
            if (mix_fill == 0) mix_fill = fill_alpha;
            fill_alpha = (uint8_t)mix_fill;
        }
 
        int16_t prev_x = 0, prev_y = 0;
        bool    prev_valid = false;
 
        for (uint16_t i = 0; i < s->point_count; i++) {
            int32_t vx = s->x_data ? s->x_data[i] : (int32_t)i;
            int32_t vy = s->y_data[i];
 
            /* clamp to axis range to avoid overflow */
            vx = sgl_clamp(vx, chart->x_axis.min, chart->x_axis.max);
            vy = sgl_clamp(vy, chart->y_axis.min, chart->y_axis.max);
 
            int16_t x = plot_x1 + (int32_t)(vx - chart->x_axis.min) * plot_w / x_range;
            int16_t y = plot_rect->y2 - (int32_t)(vy - chart->y_axis.min) * plot_h / y_range;
 
            if (prev_valid) {
                if (s->style_bits.fill_under && fill_alpha) {
                    sgl_linechart_fill_segment_under(surf, plot_clip,
                                                     prev_x, prev_y,
                                                     x, y,
                                                     baseline_y,
                                                     s->fill_color,
                                                     fill_alpha);
                }
 
                if (s->style_bits.show_line) {
                    sgl_linechart_draw_segment_line(surf, plot_clip,
                                                    prev_x, prev_y,
                                                    x, y,
                                                    s->line_color,
                                                    s->line_width,
                                                    eff_line_alpha);
                }
            }
 
            if (s->style_bits.show_points) {
                sgl_linechart_draw_point_marker(surf, plot_clip,
                                                x, y,
                                                (sgl_linechart_point_shape_t)s->style_bits.point_shape,
                                                s->point_radius,
                                                s->line_color,
                                                eff_line_alpha);
            }
 
            prev_x = x;
            prev_y = y;
            prev_valid = true;
        }
    }
 
    SGL_UNUSED(obj);
}


/**
 * @brief Linechart construct callback
 */
static void sgl_linechart_construct_cb(sgl_surf_t *surf, sgl_obj_t* obj, sgl_event_t *evt)
{
    sgl_linechart_t *chart = sgl_container_of(obj, sgl_linechart_t, obj);

    if (evt->type != SGL_EVENT_DRAW_MAIN) {
        return;
    }

    /* Quick reject if object is completely outside screen */
    if (obj->area.x2 < surf->x1 || obj->area.x1 > surf->x2 ||
        obj->area.y2 < surf->y1 || obj->area.y1 > surf->y2) {
        return;
    }

    sgl_area_t full_rect = obj->coords;
    if (!sgl_area_selfclip(&full_rect, &obj->area)) {
        return;
    }

    /* Draw background and border */
    sgl_draw_rect_t bg_desc = {
        .alpha       = chart->bg_alpha,
        .border      = obj->border,
        .color       = chart->bg_color,
        .radius      = obj->radius,
        .border_color= chart->border_color,
        .border_mask = obj->focus,
        .pixmap      = NULL,
    };
    sgl_draw_rect(surf, &obj->area, &obj->coords, &bg_desc);

    /* Layout: reserve space for axis labels, or use custom plot area */
    const sgl_font_t *x_font = chart->x_axis.label_font ? chart->x_axis.label_font : sgl_get_system_font();
    const sgl_font_t *y_font = chart->y_axis.label_font ? chart->y_axis.label_font : sgl_get_system_font();
 
    sgl_area_t plot_rect;
 
    if (chart->option_bits.custom_plot_rect) {
        /* User-defined plot area (relative to widget top-left) */
        plot_rect.x1 = (int16_t)(full_rect.x1 + chart->plot_rel_rect.x1);
        plot_rect.y1 = (int16_t)(full_rect.y1 + chart->plot_rel_rect.y1);
        plot_rect.x2 = (int16_t)(full_rect.x1 + chart->plot_rel_rect.x2);
        plot_rect.y2 = (int16_t)(full_rect.y1 + chart->plot_rel_rect.y2);
 
        /* Clip to widget area to avoid drawing outside */
        if (plot_rect.x1 < full_rect.x1) plot_rect.x1 = full_rect.x1;
        if (plot_rect.y1 < full_rect.y1) plot_rect.y1 = full_rect.y1;
        if (plot_rect.x2 > full_rect.x2) plot_rect.x2 = full_rect.x2;
        if (plot_rect.y2 > full_rect.y2) plot_rect.y2 = full_rect.y2;
    }
    else {
        int16_t top_margin    = 4;
        int16_t right_margin  = 4;
        int16_t bottom_margin = 4;
        int16_t left_margin   = 4;
 
        if (chart->x_axis.flag_bits.show_labels && x_font) {
            bottom_margin += (int16_t)x_font->font_height + 4;
        }
        if (chart->y_axis.flag_bits.show_labels && y_font) {
            left_margin += 40; /* fixed width area for Y labels */
        }
 
        plot_rect.x1 = (int16_t)(full_rect.x1 + left_margin);
        plot_rect.y1 = (int16_t)(full_rect.y1 + top_margin);
        plot_rect.x2 = (int16_t)(full_rect.x2 - right_margin);
        plot_rect.y2 = (int16_t)(full_rect.y2 - bottom_margin);
    }
 
    if (plot_rect.x1 >= plot_rect.x2 || plot_rect.y1 >= plot_rect.y2) {
        return;
    }

    /* Update auto-scale axes based on series data */
    sgl_linechart_update_axis_auto(chart);

    /* Handle open animation (clip visible plot area) */
    sgl_area_t plot_clip = plot_rect;

    if (chart->option_bits.open_anim_enable) {
        bool anim_active = true;

        if (!chart->option_bits.open_anim_playing) {
            chart->option_bits.open_anim_playing = 1U;
            chart->open_anim_start_tick = sgl_tick_get();
        }

        uint32_t now   = sgl_tick_get();
        uint32_t elaps = now - chart->open_anim_start_tick;

        if (elaps >= SGL_LINECHART_OPEN_ANIM_DURATION) {
            chart->option_bits.open_anim_enable = 0U;
            chart->option_bits.open_anim_playing = 0U;
            anim_active              = false;
        }
        else {
            int32_t extent;
#if (CONFIG_SGL_ANIMATION)
            if (chart->open_anim_path) {
                extent = chart->open_anim_path(elaps, SGL_LINECHART_OPEN_ANIM_DURATION, 0, 1000);
            }
            else
#endif
            {
                extent = (int32_t)((1000 * (int32_t)elaps) / (int32_t)SGL_LINECHART_OPEN_ANIM_DURATION);
            }

            if (extent < 0) extent = 0;
            if (extent > 1000) extent = 1000;

            int16_t w = plot_rect.x2 - plot_rect.x1 + 1;
            int16_t h = plot_rect.y2 - plot_rect.y1 + 1;

            if (chart->option_bits.open_anim_dir == SGL_LINECHART_OPEN_ANIM_FROM_LEFT) {
                int16_t visible_w = (int16_t)((int32_t)w * extent / 1000);
                if (visible_w <= 0) {
                    /* nothing visible yet, but keep requesting next frame so animation can progress */
                    if (anim_active) {
                        sgl_obj_set_dirty(obj);
                    }
                    return;
                }
                plot_clip.x2 = (int16_t)(plot_rect.x1 + visible_w - 1);
            }
            else if (chart->option_bits.open_anim_dir == SGL_LINECHART_OPEN_ANIM_FROM_TOP) {
                int16_t visible_h = (int16_t)((int32_t)h * extent / 1000);
                if (visible_h <= 0) {
                    /* nothing visible yet, but keep requesting next frame so animation can progress */
                    if (anim_active) {
                        sgl_obj_set_dirty(obj);
                    }
                    return;
                }
                plot_clip.y2 = (int16_t)(plot_rect.y1 + visible_h - 1);
            }

            /* request next frame */
            if (anim_active) {
                sgl_obj_set_dirty(obj);
            }
        }
    }

    if (!sgl_area_selfclip(&plot_clip, &obj->area)) {
        return;
    }

    /* Draw grid and labels */
    sgl_linechart_draw_grid_and_labels(surf, obj, chart, &plot_rect, &plot_clip);

    /* Draw series */
    sgl_linechart_draw_series(surf, obj, chart, &plot_rect, &plot_clip);
}
