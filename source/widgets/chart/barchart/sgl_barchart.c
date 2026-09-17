/* source/widgets/chart/barchart/sgl_barchart.c
 *
 * MIT License
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
#include "sgl_barchart.h"

#define SGL_BARCHART_TICK_LEN 4

static void sgl_barchart_construct_cb(sgl_surf_t *surf, sgl_obj_t *obj, sgl_event_t *evt);
static void sgl_barchart_update_axis_auto(sgl_barchart_t *chart);
static int32_t sgl_barchart_get_effective_step(const sgl_barchart_axis_t *axis);
static void sgl_barchart_format_value(char *buf, size_t size, int32_t value);
static void sgl_barchart_draw_grid_and_labels(sgl_surf_t *surf, sgl_obj_t *obj,
                                              sgl_barchart_t *chart,
                                              const sgl_area_t *plot_rect,
                                              const sgl_area_t *plot_clip);
static void sgl_barchart_draw_series(sgl_surf_t *surf, sgl_obj_t *obj,
                                     sgl_barchart_t *chart,
                                     const sgl_area_t *plot_rect,
                                     const sgl_area_t *plot_clip);
static void sgl_barchart_draw_dashed_line(sgl_surf_t *surf, sgl_area_t *area,
                                          int16_t x0, int16_t y0,
                                          int16_t x1, int16_t y1,
                                          uint16_t dash_len, uint16_t gap_len,
                                          sgl_color_t color, uint8_t alpha);
static bool sgl_barchart_calc_plot_rect(sgl_obj_t *obj, sgl_barchart_t *chart,
                                        const sgl_area_t *full_rect,
                                        sgl_area_t *plot_rect);
static bool sgl_barchart_get_bar_rect(const sgl_barchart_t *chart,
                                      const sgl_area_t *plot_rect,
                                      uint8_t series_index,
                                      uint16_t point_index,
                                      sgl_area_t *bar_rect);

sgl_obj_t *sgl_barchart_create(sgl_obj_t *parent)
{
    sgl_barchart_t *chart = sgl_malloc(sizeof(sgl_barchart_t));
    if (chart == NULL) {
        SGL_LOG_ERROR("sgl_barchart_create: malloc failed");
        return NULL;
    }

    memset(chart, 0, sizeof(sgl_barchart_t));

    sgl_obj_t *obj = &chart->obj;
    sgl_obj_init(obj, parent);
    obj->construct_fn = sgl_barchart_construct_cb;
    sgl_obj_set_border_width(obj, SGL_THEME_BORDER_WIDTH);

    chart->alpha = SGL_ALPHA_MAX;
    chart->bg_color = SGL_THEME_BG_COLOR;
    chart->bg_alpha = SGL_THEME_ALPHA;
    chart->border_color = SGL_THEME_BORDER_COLOR;

    chart->x_axis.min = 0;
    chart->x_axis.max = 5;
    chart->x_axis.step = 1;
    chart->x_axis.flag_bits.auto_scale = 0U;
    chart->x_axis.flag_bits.show_grid = 0U;
    chart->x_axis.flag_bits.grid_dashed = 1U;
    chart->x_axis.flag_bits.show_labels = 1U;
    chart->x_axis.auto_divisions = SGL_BARCHART_DEFAULT_DIVISIONS;
    chart->x_axis.grid_color = sgl_rgb(60, 60, 60);
    chart->x_axis.grid_alpha = 80;
    chart->x_axis.label_font = sgl_get_system_font();
    chart->x_axis.label_color = SGL_THEME_TEXT_COLOR;
    chart->x_axis.label_alpha = SGL_ALPHA_MAX;
    chart->x_axis.flag_bits.show_ticks = 1U;

    chart->y_axis.min = 0;
    chart->y_axis.max = 100;
    chart->y_axis.step = 0;
    chart->y_axis.flag_bits.auto_scale = 1U;
    chart->y_axis.flag_bits.show_grid = 1U;
    chart->y_axis.flag_bits.grid_dashed = 1U;
    chart->y_axis.flag_bits.show_labels = 1U;
    chart->y_axis.auto_divisions = SGL_BARCHART_DEFAULT_DIVISIONS;
    chart->y_axis.grid_color = sgl_rgb(60, 60, 60);
    chart->y_axis.grid_alpha = 80;
    chart->y_axis.label_font = sgl_get_system_font();
    chart->y_axis.label_color = SGL_THEME_TEXT_COLOR;
    chart->y_axis.label_alpha = SGL_ALPHA_MAX;
    chart->y_axis.flag_bits.show_ticks = 1U;

    chart->layout_left_margin = 44;
    chart->layout_top_margin = 4;
    chart->layout_right_margin = 4;
    chart->layout_bottom_margin = 24;
    chart->bar_gap = 4;
    chart->category_gap = 10;
    chart->option_bits.open_anim_dir = SGL_BARCHART_OPEN_ANIM_FROM_BOTTOM;
    chart->open_anim_duration = SGL_BARCHART_OPEN_ANIM_DURATION;

    return obj;
}

void sgl_barchart_set_series_count(sgl_obj_t *obj, uint8_t count)
{
    SGL_ASSERT(obj != NULL);
    sgl_barchart_t *chart = sgl_container_of(obj, sgl_barchart_t, obj);

    if (chart->series) {
        sgl_free(chart->series);
        chart->series = NULL;
        chart->series_count = 0;
    }

    if (count == 0) {
        sgl_obj_set_dirty(obj);
        return;
    }

    chart->series = sgl_malloc(sizeof(sgl_barchart_series_t) * count);
    if (chart->series == NULL) {
        SGL_LOG_ERROR("sgl_barchart_set_series_count: malloc failed");
        sgl_obj_set_dirty(obj);
        return;
    }

    memset(chart->series, 0, sizeof(sgl_barchart_series_t) * count);
    for (uint8_t i = 0; i < count; i++) {
        chart->series[i].color = SGL_THEME_COLOR;
        chart->series[i].alpha = SGL_ALPHA_MAX;
    }

    chart->series_count = count;
    sgl_obj_set_dirty(obj);
}

void sgl_barchart_set_series_y_array(sgl_obj_t *obj, uint8_t index,
                                     const int32_t *y_data,
                                     uint16_t point_count)
{
    SGL_ASSERT(obj != NULL);
    sgl_barchart_t *chart = sgl_container_of(obj, sgl_barchart_t, obj);
    if (chart->series == NULL || index >= chart->series_count) {
        SGL_LOG_ERROR("sgl_barchart_set_series_y_array: invalid index %d", index);
        return;
    }

    chart->series[index].y_data = y_data;
    chart->series[index].point_count = point_count;
    sgl_obj_set_dirty(obj);
}

void sgl_barchart_set_x_labels(sgl_obj_t *obj, const char **labels, uint8_t count)
{
    SGL_ASSERT(obj != NULL);
    sgl_barchart_t *chart = sgl_container_of(obj, sgl_barchart_t, obj);
    chart->x_labels = labels;
    chart->x_label_count = count;
    sgl_obj_set_dirty(obj);
}

void sgl_barchart_update(sgl_obj_t *obj)
{
    SGL_ASSERT(obj != NULL);
    sgl_obj_set_dirty(obj);
}

void sgl_barchart_update_value(sgl_obj_t *obj, uint8_t series_index, uint16_t point_index)
{
    SGL_ASSERT(obj != NULL);
    sgl_barchart_t *chart = sgl_container_of(obj, sgl_barchart_t, obj);

    if (chart->series == NULL || series_index >= chart->series_count) {
        SGL_LOG_ERROR("sgl_barchart_update_value: invalid series index %d", series_index);
        return;
    }
    if (chart->series[series_index].y_data == NULL || point_index >= chart->series[series_index].point_count) {
        SGL_LOG_ERROR("sgl_barchart_update_value: invalid point index %d", point_index);
        return;
    }

    sgl_obj_set_dirty(obj);
}

static void sgl_barchart_draw_dashed_line(sgl_surf_t *surf, sgl_area_t *area,
                                          int16_t x0, int16_t y0,
                                          int16_t x1, int16_t y1,
                                          uint16_t dash_len, uint16_t gap_len,
                                          sgl_color_t color, uint8_t alpha)
{
    if (dash_len == 0 || gap_len == 0) {
        sgl_draw_line_t desc = {
            .alpha = alpha,
            .width = 4,
            .color = color,
            .x1 = x0,
            .y1 = y0,
            .x2 = x1,
            .y2 = y1,
        };
        sgl_draw_line(surf, area, &desc);
        return;
    }

    int16_t dx = sgl_abs(x1 - x0);
    int16_t dy = sgl_abs(y1 - y0);
    int16_t sx = (x0 < x1) ? 1 : -1;
    int16_t sy = (y0 < y1) ? 1 : -1;
    int16_t err = dx - dy;
    uint32_t pattern_len = (uint32_t)dash_len + (uint32_t)gap_len;
    uint32_t pattern_pos = 0;
    sgl_area_t clip_area = { surf->x1, surf->y1, surf->x2, surf->y2 };
    sgl_area_selfclip(&clip_area, area);

    while (1) {
        if (pattern_pos < dash_len &&
            x0 >= clip_area.x1 && x0 <= clip_area.x2 &&
            y0 >= clip_area.y1 && y0 <= clip_area.y2) {
            sgl_color_t *buf = sgl_surf_get_buf(surf, x0 - surf->x1, y0 - surf->y1);
            *buf = (alpha == SGL_ALPHA_MAX) ? color : sgl_color_mixer(color, *buf, alpha);
        }
        if (x0 == x1 && y0 == y1) break;
        pattern_pos++;
        if (pattern_pos >= pattern_len) pattern_pos = 0;
        int16_t e2 = err << 1;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx) { err += dx; y0 += sy; }
    }
}

static void sgl_barchart_format_value(char *buf, size_t size, int32_t value)
{
    if (size == 0) return;
    if (value == 0) {
        buf[0] = '0';
        if (size > 1) buf[1] = '\0';
        return;
    }
    bool neg = (value < 0);
    uint32_t v = (uint32_t)(neg ? -value : value);
    char tmp[16];
    size_t i = 0, pos = 0;
    while (v && i < sizeof(tmp)) {
        tmp[i++] = (char)('0' + (v % 10U));
        v /= 10U;
    }
    if (neg && pos < size - 1) buf[pos++] = '-';
    while (i > 0 && pos < size - 1) buf[pos++] = tmp[--i];
    buf[pos] = '\0';
}

static int32_t sgl_barchart_get_effective_step(const sgl_barchart_axis_t *axis)
{
    int32_t range = axis->max - axis->min;
    if (range <= 0) return 1;
    if (axis->step > 0) return axis->step;
    uint8_t div = axis->auto_divisions ? axis->auto_divisions : SGL_BARCHART_DEFAULT_DIVISIONS;
    int32_t step = range / (int32_t)div;
    return (step <= 0) ? 1 : step;
}

static void sgl_barchart_update_axis_auto(sgl_barchart_t *chart)
{
    int32_t data_min_y = INT32_MAX;
    int32_t data_max_y = INT32_MIN;
    uint16_t max_points = 0;

    for (uint8_t i = 0; i < chart->series_count; i++) {
        sgl_barchart_series_t *s = &chart->series[i];
        if (s->y_data == NULL || s->point_count == 0) continue;
        if (s->point_count > max_points) max_points = s->point_count;
        for (uint16_t j = 0; j < s->point_count; j++) {
            int32_t vy = s->y_data[j];
            if (vy < data_min_y) data_min_y = vy;
            if (vy > data_max_y) data_max_y = vy;
        }
    }

    if (chart->x_axis.flag_bits.auto_scale) {
        chart->x_axis.min = 0;
        chart->x_axis.max = (max_points > 0) ? (int32_t)(max_points - 1) : 0;
    }

    if (!chart->y_axis.flag_bits.auto_scale) return;
    if (data_min_y == INT32_MAX) {
        chart->y_axis.min = 0;
        chart->y_axis.max = 10;
        return;
    }

    if (data_min_y > 0) data_min_y = 0;
    if (data_max_y < 0) data_max_y = 0;

    if (data_min_y == data_max_y) {
        chart->y_axis.min = data_min_y - 1;
        chart->y_axis.max = data_max_y + 1;
    } else {
        int32_t range = data_max_y - data_min_y;
        int32_t margin = range / 10;
        if (margin <= 0) margin = 1;
        chart->y_axis.min = data_min_y - ((data_min_y < 0) ? margin : 0);
        chart->y_axis.max = data_max_y + margin;
    }
}

static bool sgl_barchart_calc_plot_rect(sgl_obj_t *obj, sgl_barchart_t *chart,
                                        const sgl_area_t *full_rect,
                                        sgl_area_t *plot_rect)
{
    if (chart->option_bits.custom_plot_rect) {
        plot_rect->x1 = (int16_t)(full_rect->x1 + chart->plot_rel_rect.x1);
        plot_rect->y1 = (int16_t)(full_rect->y1 + chart->plot_rel_rect.y1);
        plot_rect->x2 = (int16_t)(full_rect->x1 + chart->plot_rel_rect.x2);
        plot_rect->y2 = (int16_t)(full_rect->y1 + chart->plot_rel_rect.y2);
    } else {
        const sgl_font_t *x_font = chart->x_axis.label_font ? chart->x_axis.label_font : sgl_get_system_font();
        int16_t bottom = chart->layout_bottom_margin;
        if (chart->x_axis.flag_bits.show_labels && x_font) {
            int16_t font_h = (int16_t)x_font->font_height + 4;
            if (bottom < font_h) bottom = font_h;
        }
        plot_rect->x1 = (int16_t)(full_rect->x1 + chart->layout_left_margin);
        plot_rect->y1 = (int16_t)(full_rect->y1 + chart->layout_top_margin);
        plot_rect->x2 = (int16_t)(full_rect->x2 - chart->layout_right_margin);
        plot_rect->y2 = (int16_t)(full_rect->y2 - bottom);
    }

    if (plot_rect->x1 < full_rect->x1) plot_rect->x1 = full_rect->x1;
    if (plot_rect->y1 < full_rect->y1) plot_rect->y1 = full_rect->y1;
    if (plot_rect->x2 > full_rect->x2) plot_rect->x2 = full_rect->x2;
    if (plot_rect->y2 > full_rect->y2) plot_rect->y2 = full_rect->y2;
    SGL_UNUSED(obj);
    return (bool)(plot_rect->x1 < plot_rect->x2 && plot_rect->y1 < plot_rect->y2);
}

static void sgl_barchart_draw_grid_and_labels(sgl_surf_t *surf, sgl_obj_t *obj,
                                              sgl_barchart_t *chart,
                                              const sgl_area_t *plot_rect,
                                              const sgl_area_t *plot_clip)
{
    const sgl_font_t *x_font = chart->x_axis.label_font ? chart->x_axis.label_font : sgl_get_system_font();
    const sgl_font_t *y_font = chart->y_axis.label_font ? chart->y_axis.label_font : sgl_get_system_font();
    int16_t plot_w = plot_rect->x2 - plot_rect->x1;
    int16_t plot_h = plot_rect->y2 - plot_rect->y1;
    if (plot_w <= 0 || plot_h <= 0) return;

    int32_t x_range = chart->x_axis.max - chart->x_axis.min;
    int32_t y_range = chart->y_axis.max - chart->y_axis.min;
    if (x_range <= 0) x_range = 1;
    if (y_range <= 0) y_range = 1;
    uint8_t base_alpha = chart->alpha ? chart->alpha : SGL_ALPHA_MAX;
    char buf[16];

    if (chart->y_axis.flag_bits.show_grid || chart->y_axis.flag_bits.show_labels) {
        int32_t step = sgl_barchart_get_effective_step(&chart->y_axis);
        int32_t v = chart->y_axis.min;
        uint8_t tick_idx = 0;
        sgl_area_t grid_area = *plot_clip;
        sgl_area_t y_label_area = { obj->coords.x1 + 2, plot_rect->y1, plot_rect->x1 - 2, plot_rect->y2 };
        sgl_area_t y_tick_area = { obj->coords.x1, plot_rect->y1, plot_rect->x1 - 1, plot_rect->y2 };
        sgl_area_selfclip(&y_tick_area, &obj->area);
        uint8_t grid_alpha = (uint8_t)(((uint16_t)chart->y_axis.grid_alpha * base_alpha) / 255U);
        uint8_t label_alpha = (uint8_t)(((uint16_t)chart->y_axis.label_alpha * base_alpha) / 255U);
        if (chart->y_axis.grid_alpha && grid_alpha == 0) grid_alpha = chart->y_axis.grid_alpha;
        if (chart->y_axis.label_alpha && label_alpha == 0) label_alpha = chart->y_axis.label_alpha;

        while (tick_idx < SGL_BARCHART_MAX_AUTO_TICKS && v <= chart->y_axis.max) {
            int16_t y = plot_rect->y2 - (int32_t)(v - chart->y_axis.min) * plot_h / y_range;
            if (chart->y_axis.flag_bits.show_grid && grid_alpha) {
                if (chart->y_axis.flag_bits.grid_dashed) {
                    sgl_barchart_draw_dashed_line(surf, &grid_area, plot_rect->x1, y, plot_rect->x2, y,
                                                  6, 4, chart->y_axis.grid_color, grid_alpha);
                } else {
                    sgl_draw_fill_hline(surf, &grid_area, y, plot_rect->x1, plot_rect->x2,
                                        1, chart->y_axis.grid_color, grid_alpha);
                }
            }
            if (chart->y_axis.flag_bits.show_labels && y_font && label_alpha && y_label_area.x2 > y_label_area.x1) {
                sgl_barchart_format_value(buf, sizeof(buf), v);
                sgl_draw_string(surf, &y_label_area,
                                y_label_area.x1,
                                y - (int16_t)y_font->font_height / 2,
                                buf,
                                chart->y_axis.label_color,
                                label_alpha,
                                y_font);
            }
            if (chart->y_axis.flag_bits.show_ticks && grid_alpha && y_tick_area.x2 >= y_tick_area.x1) {
                sgl_draw_fill_hline(surf, &y_tick_area, y,
                                    (int16_t)(plot_rect->x1 - SGL_BARCHART_TICK_LEN),
                                    (int16_t)(plot_rect->x1 - 1),
                                    1, chart->y_axis.grid_color, grid_alpha);
            }
            tick_idx++;
            v += step;
        }
    }

    if (chart->x_axis.flag_bits.show_grid || chart->x_axis.flag_bits.show_labels) {
        int32_t step = sgl_barchart_get_effective_step(&chart->x_axis);
        int32_t v = chart->x_axis.min;
        uint8_t tick_idx = 0;
        sgl_area_t grid_area = *plot_clip;
        sgl_area_t x_label_area = { plot_rect->x1, plot_rect->y2 + 2, obj->coords.x2, obj->coords.y2 };
        sgl_area_t x_tick_area = { plot_rect->x1, plot_rect->y2 + 1, plot_rect->x2, obj->coords.y2 };
        sgl_area_selfclip(&x_tick_area, &obj->area);
        uint8_t grid_alpha = (uint8_t)(((uint16_t)chart->x_axis.grid_alpha * base_alpha) / 255U);
        uint8_t label_alpha = (uint8_t)(((uint16_t)chart->x_axis.label_alpha * base_alpha) / 255U);
        if (chart->x_axis.grid_alpha && grid_alpha == 0) grid_alpha = chart->x_axis.grid_alpha;
        if (chart->x_axis.label_alpha && label_alpha == 0) label_alpha = chart->x_axis.label_alpha;

        while (tick_idx < SGL_BARCHART_MAX_AUTO_TICKS && v <= chart->x_axis.max) {
            int16_t x = plot_rect->x1 + (int32_t)(v - chart->x_axis.min) * plot_w / x_range;
            if (chart->x_axis.flag_bits.show_grid && grid_alpha) {
                if (chart->x_axis.flag_bits.grid_dashed) {
                    sgl_barchart_draw_dashed_line(surf, &grid_area, x, plot_rect->y1, x, plot_rect->y2,
                                                  6, 4, chart->x_axis.grid_color, grid_alpha);
                } else {
                    sgl_draw_fill_vline(surf, &grid_area, x, plot_rect->y1, plot_rect->y2,
                                        1, chart->x_axis.grid_color, grid_alpha);
                }
            }
            if (chart->x_axis.flag_bits.show_labels && x_font && label_alpha && x_label_area.y2 > x_label_area.y1) {
                const char *label_str = NULL;
                if (chart->x_labels && tick_idx < chart->x_label_count && chart->x_labels[tick_idx]) {
                    label_str = chart->x_labels[tick_idx];
                } else {
                    sgl_barchart_format_value(buf, sizeof(buf), v);
                    label_str = buf;
                }
                int32_t text_w = sgl_font_get_string_width(label_str, x_font);
                sgl_draw_string(surf, &x_label_area,
                                x - (int16_t)text_w / 2,
                                x_label_area.y1,
                                label_str,
                                chart->x_axis.label_color,
                                label_alpha,
                                x_font);
            }
            if (chart->x_axis.flag_bits.show_ticks && grid_alpha && x_tick_area.y2 >= x_tick_area.y1) {
                sgl_draw_fill_vline(surf, &x_tick_area, x,
                                    (int16_t)(plot_rect->y2 + 1),
                                    (int16_t)(plot_rect->y2 + SGL_BARCHART_TICK_LEN),
                                    1, chart->x_axis.grid_color, grid_alpha);
            }
            tick_idx++;
            v += step;
        }
    }
}

static bool sgl_barchart_get_bar_rect(const sgl_barchart_t *chart,
                                      const sgl_area_t *plot_rect,
                                      uint8_t series_index,
                                      uint16_t point_index,
                                      sgl_area_t *bar_rect)
{
    uint16_t point_count = 0;
    int16_t plot_w = plot_rect->x2 - plot_rect->x1 + 1;
    int16_t plot_h = plot_rect->y2 - plot_rect->y1 + 1;
    int32_t x_range = chart->x_axis.max - chart->x_axis.min;
    int32_t y_range = chart->y_axis.max - chart->y_axis.min;
    int32_t baseline_value;

    if (chart->series == NULL || bar_rect == NULL || plot_w <= 0 || plot_h <= 0) return false;
    for (uint8_t i = 0; i < chart->series_count; i++) {
        if (chart->series[i].point_count > point_count) point_count = chart->series[i].point_count;
    }
    if (point_count == 0 || point_index >= point_count) return false;

    int16_t category_w = plot_w / (int16_t)point_count;
    if (category_w <= 0) return false;
    int16_t usable_w = category_w - chart->category_gap;
    if (usable_w < (int16_t)chart->series_count) usable_w = (int16_t)chart->series_count;
    int16_t bar_w = usable_w;
    if (chart->series_count > 0) {
        bar_w = (int16_t)((usable_w - (int16_t)(chart->series_count - 1) * chart->bar_gap) / chart->series_count);
    }
    if (bar_w < 1) bar_w = 1;

    int32_t value = chart->series[series_index].y_data ? chart->series[series_index].y_data[point_index] : 0;
    if (chart->option_bits.orientation == SGL_BARCHART_ORIENTATION_HORIZONTAL) {
        int16_t cat_y1;
        int16_t y1;
        int16_t y2;
        int16_t baseline_x;
        int16_t value_x;

        if (x_range <= 0) return false;
        baseline_value = sgl_clamp(0, chart->x_axis.min, chart->x_axis.max);
        value = sgl_clamp(value, chart->x_axis.min, chart->x_axis.max);

        cat_y1 = (int16_t)(plot_rect->y1 + point_index * category_w + chart->category_gap / 2);
        y1 = (int16_t)(cat_y1 + series_index * (bar_w + chart->bar_gap));
        y2 = (int16_t)(y1 + bar_w - 1);
        baseline_x = plot_rect->x1 + (int32_t)(baseline_value - chart->x_axis.min) * plot_w / x_range;
        value_x = plot_rect->x1 + (int32_t)(value - chart->x_axis.min) * plot_w / x_range;

        bar_rect->x1 = sgl_min(value_x, baseline_x);
        bar_rect->x2 = sgl_max(value_x, baseline_x);
        bar_rect->y1 = y1;
        bar_rect->y2 = y2;
        return true;
    }

    if (y_range <= 0) return false;
    baseline_value = sgl_clamp(0, chart->y_axis.min, chart->y_axis.max);
    value = sgl_clamp(value, chart->y_axis.min, chart->y_axis.max);

    int16_t cat_x1 = (int16_t)(plot_rect->x1 + point_index * category_w + chart->category_gap / 2);
    int16_t x1 = (int16_t)(cat_x1 + series_index * (bar_w + chart->bar_gap));
    int16_t x2 = (int16_t)(x1 + bar_w - 1);
    int16_t baseline_y = plot_rect->y2 - (int32_t)(baseline_value - chart->y_axis.min) * plot_h / y_range;
    int16_t value_y = plot_rect->y2 - (int32_t)(value - chart->y_axis.min) * plot_h / y_range;

    bar_rect->x1 = x1;
    bar_rect->x2 = x2;
    bar_rect->y1 = sgl_min(value_y, baseline_y);
    bar_rect->y2 = sgl_max(value_y, baseline_y);
    return true;
}

static void sgl_barchart_draw_series(sgl_surf_t *surf, sgl_obj_t *obj,
                                     sgl_barchart_t *chart,
                                     const sgl_area_t *plot_rect,
                                     const sgl_area_t *plot_clip)
{
    uint16_t point_count = 0;
    int16_t plot_w = plot_rect->x2 - plot_rect->x1 + 1;
    int16_t plot_h = plot_rect->y2 - plot_rect->y1 + 1;
    int32_t x_range = chart->x_axis.max - chart->x_axis.min;
    int32_t y_range = chart->y_axis.max - chart->y_axis.min;
    if (plot_w <= 0 || plot_h <= 0) return;

    for (uint8_t i = 0; i < chart->series_count; i++) {
        if (chart->series[i].point_count > point_count) point_count = chart->series[i].point_count;
    }
    if (point_count == 0) return;

    int16_t category_w = plot_w / (int16_t)point_count;
    if (category_w <= 0) return;
    int16_t usable_w = category_w - chart->category_gap;
    if (usable_w < (int16_t)chart->series_count) usable_w = (int16_t)chart->series_count;
    int16_t bar_w = usable_w;
    if (chart->series_count > 0) {
        bar_w = (int16_t)((usable_w - (int16_t)(chart->series_count - 1) * chart->bar_gap) / chart->series_count);
    }
    if (bar_w < 1) bar_w = 1;

    uint8_t base_alpha = chart->alpha ? chart->alpha : SGL_ALPHA_MAX;

    for (uint16_t point = 0; point < point_count; point++) {
        for (uint8_t si = 0; si < chart->series_count; si++) {
            sgl_barchart_series_t *s = &chart->series[si];
            if (s->y_data == NULL || point >= s->point_count) continue;
            sgl_area_t bar;
            uint8_t alpha = (uint8_t)(((uint16_t)s->alpha * base_alpha) / 255U);
            if (s->alpha && alpha == 0) alpha = s->alpha;
            if (!sgl_barchart_get_bar_rect(chart, plot_rect, si, point, &bar)) continue;
            sgl_draw_fill_rect(surf, (sgl_area_t *)plot_clip, &bar, 0, s->color, alpha);
        }
    }
    SGL_UNUSED(obj);
}

static void sgl_barchart_construct_cb(sgl_surf_t *surf, sgl_obj_t *obj, sgl_event_t *evt)
{
    sgl_barchart_t *chart = sgl_container_of(obj, sgl_barchart_t, obj);
    if (evt->type != SGL_EVENT_DRAW_MAIN) return;
    if (obj->area.x2 < surf->x1 || obj->area.x1 > surf->x2 || obj->area.y2 < surf->y1 || obj->area.y1 > surf->y2) return;

    sgl_area_t full_rect = obj->coords;
    if (!sgl_area_selfclip(&full_rect, &obj->area)) return;

    sgl_draw_rect_t bg_desc = {
        .alpha = chart->bg_alpha,
        .border = obj->border,
        .border_mask = obj->focus,
        .color = chart->bg_color,
        .radius = obj->radius,
        .border_color = chart->border_color,
        .pixmap = NULL,
    };
    sgl_draw_rect(surf, &obj->area, &obj->coords, &bg_desc);

    sgl_barchart_update_axis_auto(chart);

    sgl_area_t plot_rect;
    if (!sgl_barchart_calc_plot_rect(obj, chart, &full_rect, &plot_rect)) return;

    sgl_area_t plot_clip = plot_rect;
    if (chart->option_bits.open_anim_enable) {
        bool anim_active = true;
        uint16_t duration = chart->open_anim_duration ? chart->open_anim_duration : SGL_BARCHART_OPEN_ANIM_DURATION;
        if (!chart->option_bits.open_anim_playing) {
            chart->option_bits.open_anim_playing = 1U;
            chart->open_anim_start_tick = sgl_tick_get();
        }
        uint32_t elaps = sgl_tick_get() - chart->open_anim_start_tick;
        if (elaps >= duration) {
            chart->option_bits.open_anim_enable = 0U;
            chart->option_bits.open_anim_playing = 0U;
            anim_active = false;
        } else {
            int32_t extent;
            if (chart->open_anim_path) {
                extent = chart->open_anim_path(elaps, duration, 0, 1000);
            } else {
                extent = (int32_t)((1000 * (int32_t)elaps) / duration);
            }
            if (extent < 0) extent = 0;
            if (extent > 1000) extent = 1000;

            if (chart->option_bits.orientation == SGL_BARCHART_ORIENTATION_HORIZONTAL) {
                if (chart->option_bits.open_anim_dir == SGL_BARCHART_OPEN_ANIM_FROM_BOTTOM) {
                    int16_t visible_h = (int16_t)(((plot_rect.y2 - plot_rect.y1 + 1) * extent) / 1000);
                    if (visible_h <= 0) {
                        if (anim_active) sgl_obj_set_dirty(obj);
                        return;
                    }
                    plot_clip.y1 = (int16_t)(plot_rect.y2 - visible_h + 1);
                } else {
                    int16_t visible_w = (int16_t)(((plot_rect.x2 - plot_rect.x1 + 1) * extent) / 1000);
                    if (visible_w <= 0) {
                        if (anim_active) sgl_obj_set_dirty(obj);
                        return;
                    }
                    plot_clip.x2 = (int16_t)(plot_rect.x1 + visible_w - 1);
                }
            } else {
                if (chart->option_bits.open_anim_dir == SGL_BARCHART_OPEN_ANIM_FROM_LEFT) {
                    int16_t visible_w = (int16_t)(((plot_rect.x2 - plot_rect.x1 + 1) * extent) / 1000);
                    if (visible_w <= 0) {
                        if (anim_active) sgl_obj_set_dirty(obj);
                        return;
                    }
                    plot_clip.x2 = (int16_t)(plot_rect.x1 + visible_w - 1);
                } else {
                    int16_t visible_h = (int16_t)(((plot_rect.y2 - plot_rect.y1 + 1) * extent) / 1000);
                    if (visible_h <= 0) {
                        if (anim_active) sgl_obj_set_dirty(obj);
                        return;
                    }
                    plot_clip.y1 = (int16_t)(plot_rect.y2 - visible_h + 1);
                }
            }
            if (anim_active) sgl_obj_set_dirty(obj);
        }
    }

    if (!sgl_area_selfclip(&plot_clip, &obj->area)) return;
    sgl_barchart_draw_grid_and_labels(surf, obj, chart, &plot_rect, &plot_clip);
    sgl_barchart_draw_series(surf, obj, chart, &plot_rect, &plot_clip);
}
