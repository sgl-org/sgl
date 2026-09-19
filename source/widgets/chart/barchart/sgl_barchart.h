/* source/widgets/chart/barchart/sgl_barchart.h
 *
 * MIT License
 */

#ifndef __SGL_BARCHART_H__
#define __SGL_BARCHART_H__

#include <sgl_core.h>
#include <sgl_draw.h>
#include <sgl_math.h>
#include <sgl_log.h>
#include <sgl_mm.h>
#include <sgl_cfgfix.h>
#include <string.h>
#include <stdbool.h>
#include <sgl_anim.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SGL_BARCHART_MAX_AUTO_TICKS      (8)
#define SGL_BARCHART_DEFAULT_DIVISIONS   (4)
#define SGL_BARCHART_OPEN_ANIM_DURATION  (600U)

/**
 * @brief Axis selector of barchart
 */
typedef enum sgl_barchart_axis_id {
    SGL_BARCHART_AXIS_X = 0,
    SGL_BARCHART_AXIS_Y = 1,
} sgl_barchart_axis_id_t;

/**
 * @brief Open animation direction
 */
typedef enum sgl_barchart_open_anim_dir {
    SGL_BARCHART_OPEN_ANIM_NONE        = 0,
    SGL_BARCHART_OPEN_ANIM_FROM_LEFT   = 1,
    SGL_BARCHART_OPEN_ANIM_FROM_BOTTOM = 2,
} sgl_barchart_open_anim_dir_t;

/**
 * @brief Layout direction of bars
 */
typedef enum sgl_barchart_orientation {
    SGL_BARCHART_ORIENTATION_VERTICAL   = 0,
    SGL_BARCHART_ORIENTATION_HORIZONTAL = 1,
} sgl_barchart_orientation_t;

typedef struct sgl_barchart_axis_bits {
    uint8_t auto_scale  : 1;
    uint8_t show_grid   : 1;
    uint8_t grid_dashed : 1;
    uint8_t show_labels : 1;
    uint8_t show_ticks  : 1;
    uint8_t reserved    : 3;
} sgl_barchart_axis_bits_t;

typedef struct sgl_barchart_option_bits {
    uint8_t custom_plot_rect  : 1;
    uint8_t open_anim_enable  : 1;
    uint8_t open_anim_playing : 1;
    uint8_t orientation       : 1;
    uint8_t open_anim_dir     : 2;
    uint8_t reserved          : 2;
} sgl_barchart_option_bits_t;

typedef struct sgl_barchart_axis {
    sgl_barchart_axis_bits_t flag_bits;  /**< packed axis flags */
    const sgl_font_t *label_font;
    int32_t           min;
    int32_t           max;
    int32_t           step;
    sgl_color_t       grid_color;
    sgl_color_t       label_color;
    uint8_t           auto_divisions;
    uint8_t           grid_alpha;
    uint8_t           label_alpha;
} sgl_barchart_axis_t;

/**
 * @brief Single data series of barchart
 */
typedef struct sgl_barchart_series {
    const int32_t *y_data;
    const char    *label;
    sgl_color_t    color;
    uint16_t       point_count;
    uint8_t        alpha;
} sgl_barchart_series_t;

/**
 * @brief Bar chart widget object
 */
typedef struct sgl_barchart {
    sgl_obj_t              obj;
    sgl_barchart_axis_t    x_axis;
    sgl_barchart_axis_t    y_axis;
    sgl_area_t             plot_rel_rect;
    sgl_barchart_series_t *series;
    const char           **x_labels;
    sgl_anim_path_algo_t   open_anim_path;
    sgl_color_t            bg_color;
    sgl_color_t            border_color;
    uint32_t               open_anim_start_tick;
    int16_t                layout_left_margin;
    int16_t                layout_top_margin;
    int16_t                layout_right_margin;
    int16_t                layout_bottom_margin;
    uint16_t               open_anim_duration;
    sgl_barchart_option_bits_t option_bits; /**< packed chart flags, orientation and animation state */
    uint8_t                alpha;
    uint8_t                bg_alpha;
    uint8_t                series_count;
    uint8_t                x_label_count;
    uint8_t                bar_gap;
    uint8_t                category_gap;
} sgl_barchart_t;

/**
 * @brief Create a barchart widget
 * @param parent parent object
 * @return barchart object
 */
sgl_obj_t *sgl_barchart_create(sgl_obj_t *parent);

/**
 * @brief Set series count and reallocate series array
 * @param obj barchart object
 * @param count desired series count
 */
void sgl_barchart_set_series_count(sgl_obj_t *obj, uint8_t count);

/**
 * @brief Bind Y data array for a series
 * @param obj barchart object
 * @param index series index
 * @param y_data pointer to Y values array
 * @param point_count number of points in the series
 */
void sgl_barchart_set_series_y_array(sgl_obj_t *obj, uint8_t index,
                                     const int32_t *y_data,
                                     uint16_t point_count);

/**
 * @brief Set X axis label string array
 * @param obj barchart object
 * @param labels pointer to persistent label strings
 * @param count number of labels
 */
void sgl_barchart_set_x_labels(sgl_obj_t *obj, const char **labels, uint8_t count);

/**
 * @brief Request chart redraw with current data
 * @param obj barchart object
 */
void sgl_barchart_update(sgl_obj_t *obj);

/**
 * @brief Update one value in-place and request redraw
 * @param obj barchart object
 * @param series_index series index
 * @param point_index point index inside the series
 */
void sgl_barchart_update_value(sgl_obj_t *obj, uint8_t series_index, uint16_t point_index);

/**
 * @brief Get axis configuration by axis id
 */
static inline sgl_barchart_axis_t *sgl_barchart_get_axis(sgl_barchart_t *chart, sgl_barchart_axis_id_t axis)
{
    return (axis == SGL_BARCHART_AXIS_Y) ? &chart->y_axis : &chart->x_axis;
}

/**
 * @brief Set axis range and disable auto scale
 */
static inline void sgl_barchart_set_axis_range(sgl_obj_t *obj, sgl_barchart_axis_id_t axis,
                                               int32_t min, int32_t max)
{
    SGL_ASSERT(obj != NULL);
    sgl_barchart_t *chart = sgl_container_of(obj, sgl_barchart_t, obj);
    sgl_barchart_axis_t *a = sgl_barchart_get_axis(chart, axis);
    a->min = min;
    a->max = max;
    a->flag_bits.auto_scale = 0U;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Enable or disable axis auto scale
 */
static inline void sgl_barchart_enable_axis_auto_scale(sgl_obj_t *obj, sgl_barchart_axis_id_t axis, bool enable)
{
    SGL_ASSERT(obj != NULL);
    sgl_barchart_t *chart = sgl_container_of(obj, sgl_barchart_t, obj);
    sgl_barchart_get_axis(chart, axis)->flag_bits.auto_scale = enable ? 1U : 0U;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set axis tick step
 */
static inline void sgl_barchart_set_axis_step(sgl_obj_t *obj, sgl_barchart_axis_id_t axis, int32_t step)
{
    SGL_ASSERT(obj != NULL);
    sgl_barchart_t *chart = sgl_container_of(obj, sgl_barchart_t, obj);
    sgl_barchart_get_axis(chart, axis)->step = step;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set auto division count used when step is zero
 */
static inline void sgl_barchart_set_axis_auto_divisions(sgl_obj_t *obj, sgl_barchart_axis_id_t axis, uint8_t divisions)
{
    SGL_ASSERT(obj != NULL);
    sgl_barchart_t *chart = sgl_container_of(obj, sgl_barchart_t, obj);
    sgl_barchart_get_axis(chart, axis)->auto_divisions = divisions ? divisions : SGL_BARCHART_DEFAULT_DIVISIONS;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Enable or disable axis grid lines
 */
static inline void sgl_barchart_enable_axis_grid(sgl_obj_t *obj, sgl_barchart_axis_id_t axis, bool enable)
{
    SGL_ASSERT(obj != NULL);
    sgl_barchart_t *chart = sgl_container_of(obj, sgl_barchart_t, obj);
    sgl_barchart_get_axis(chart, axis)->flag_bits.show_grid = enable ? 1U : 0U;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set axis grid style
 * @param dashed 0: solid, non-zero: dashed
 */
static inline void sgl_barchart_set_axis_grid_style(sgl_obj_t *obj, sgl_barchart_axis_id_t axis, uint8_t dashed)
{
    SGL_ASSERT(obj != NULL);
    sgl_barchart_t *chart = sgl_container_of(obj, sgl_barchart_t, obj);
    sgl_barchart_get_axis(chart, axis)->flag_bits.grid_dashed = (dashed != 0U) ? 1U : 0U;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set axis grid color and alpha
 */
static inline void sgl_barchart_set_axis_grid_color(sgl_obj_t *obj, sgl_barchart_axis_id_t axis,
                                                    sgl_color_t color, uint8_t alpha)
{
    SGL_ASSERT(obj != NULL);
    sgl_barchart_t *chart = sgl_container_of(obj, sgl_barchart_t, obj);
    sgl_barchart_axis_t *a = sgl_barchart_get_axis(chart, axis);
    a->grid_color = color;
    a->grid_alpha = alpha;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Enable or disable axis labels
 */
static inline void sgl_barchart_enable_axis_labels(sgl_obj_t *obj, sgl_barchart_axis_id_t axis, bool enable)
{
    SGL_ASSERT(obj != NULL);
    sgl_barchart_t *chart = sgl_container_of(obj, sgl_barchart_t, obj);
    sgl_barchart_get_axis(chart, axis)->flag_bits.show_labels = enable ? 1U : 0U;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set axis label font
 */
static inline void sgl_barchart_set_axis_label_font(sgl_obj_t *obj, sgl_barchart_axis_id_t axis,
                                                    const sgl_font_t *font)
{
    SGL_ASSERT(obj != NULL);
    sgl_barchart_t *chart = sgl_container_of(obj, sgl_barchart_t, obj);
    sgl_barchart_get_axis(chart, axis)->label_font = font;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set axis label text color and alpha
 */
static inline void sgl_barchart_set_axis_label_color(sgl_obj_t *obj, sgl_barchart_axis_id_t axis,
                                                     sgl_color_t color, uint8_t alpha)
{
    SGL_ASSERT(obj != NULL);
    sgl_barchart_t *chart = sgl_container_of(obj, sgl_barchart_t, obj);
    sgl_barchart_axis_t *a = sgl_barchart_get_axis(chart, axis);
    a->label_color = color;
    a->label_alpha = alpha;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Enable or disable axis tick marks at chart edge
 */
static inline void sgl_barchart_enable_axis_ticks(sgl_obj_t *obj, sgl_barchart_axis_id_t axis, bool enable)
{
    SGL_ASSERT(obj != NULL);
    sgl_barchart_t *chart = sgl_container_of(obj, sgl_barchart_t, obj);
    sgl_barchart_get_axis(chart, axis)->flag_bits.show_ticks = enable ? 1U : 0U;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set chart background color
 */
static inline void sgl_barchart_set_bg_color(sgl_obj_t *obj, sgl_color_t color)
{
    SGL_ASSERT(obj != NULL);
    sgl_container_of(obj, sgl_barchart_t, obj)->bg_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set chart background alpha
 */
static inline void sgl_barchart_set_bg_alpha(sgl_obj_t *obj, uint8_t alpha)
{
    SGL_ASSERT(obj != NULL);
    sgl_container_of(obj, sgl_barchart_t, obj)->bg_alpha = alpha;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set chart border color
 */
static inline void sgl_barchart_set_border_color(sgl_obj_t *obj, sgl_color_t color)
{
    SGL_ASSERT(obj != NULL);
    sgl_container_of(obj, sgl_barchart_t, obj)->border_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set global chart alpha
 */
static inline void sgl_barchart_set_alpha(sgl_obj_t *obj, uint8_t alpha)
{
    SGL_ASSERT(obj != NULL);
    sgl_container_of(obj, sgl_barchart_t, obj)->alpha = alpha;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set custom plot area relative to widget top-left
 *
 * @param offset_left distance from widget left edge to plot left edge
 * @param offset_top distance from widget top edge to plot top edge
 * @param width plot width in pixels
 * @param height plot height in pixels
 *
 * When width or height is not positive, the plot area falls back to auto layout.
 */
static inline void sgl_barchart_set_plot_area_rel(sgl_obj_t *obj,
                                                  int16_t offset_left,
                                                  int16_t offset_top,
                                                  int16_t width,
                                                  int16_t height)
{
    SGL_ASSERT(obj != NULL);
    sgl_barchart_t *chart = sgl_container_of(obj, sgl_barchart_t, obj);
    if (width <= 0 || height <= 0) {
        chart->option_bits.custom_plot_rect = 0U;
        sgl_obj_set_dirty(obj);
        return;
    }
    chart->plot_rel_rect.x1 = offset_left;
    chart->plot_rel_rect.y1 = offset_top;
    chart->plot_rel_rect.x2 = (int16_t)(offset_left + width - 1);
    chart->plot_rel_rect.y2 = (int16_t)(offset_top + height - 1);
    chart->option_bits.custom_plot_rect = 1U;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Reset plot area to auto layout
 */
static inline void sgl_barchart_reset_plot_area(sgl_obj_t *obj)
{
    SGL_ASSERT(obj != NULL);
    sgl_container_of(obj, sgl_barchart_t, obj)->option_bits.custom_plot_rect = 0U;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set auto layout paddings around the plot area
 */
static inline void sgl_barchart_set_layout_padding(sgl_obj_t *obj,
                                                   int16_t left,
                                                   int16_t top,
                                                   int16_t right,
                                                   int16_t bottom)
{
    SGL_ASSERT(obj != NULL);
    sgl_barchart_t *chart = sgl_container_of(obj, sgl_barchart_t, obj);
    chart->layout_left_margin = left;
    chart->layout_top_margin = top;
    chart->layout_right_margin = right;
    chart->layout_bottom_margin = bottom;
    chart->option_bits.custom_plot_rect = 0U;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set series fill color and alpha
 * @param index series index
 */
static inline void sgl_barchart_set_series_color(sgl_obj_t *obj, uint8_t index,
                                                 sgl_color_t color, uint8_t alpha)
{
    SGL_ASSERT(obj != NULL);
    sgl_barchart_t *chart = sgl_container_of(obj, sgl_barchart_t, obj);
    if (chart->series == NULL || index >= chart->series_count) {
        SGL_LOG_ERROR("sgl_barchart_set_series_color: invalid index %d", index);
        return;
    }
    chart->series[index].color = color;
    chart->series[index].alpha = alpha;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set series label text
 * @param index series index
 * @param label persistent zero-terminated string
 */
static inline void sgl_barchart_set_series_label(sgl_obj_t *obj, uint8_t index, const char *label)
{
    SGL_ASSERT(obj != NULL);
    sgl_barchart_t *chart = sgl_container_of(obj, sgl_barchart_t, obj);
    if (chart->series == NULL || index >= chart->series_count) {
        SGL_LOG_ERROR("sgl_barchart_set_series_label: invalid index %d", index);
        return;
    }
    chart->series[index].label = label;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set spacing between bars and categories
 * @param bar_gap gap between adjacent bars within one category
 * @param category_gap gap between two categories
 */
static inline void sgl_barchart_set_bar_spacing(sgl_obj_t *obj, uint8_t bar_gap, uint8_t category_gap)
{
    SGL_ASSERT(obj != NULL);
    sgl_barchart_t *chart = sgl_container_of(obj, sgl_barchart_t, obj);
    chart->bar_gap = bar_gap;
    chart->category_gap = category_gap;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set chart orientation
 */
static inline void sgl_barchart_set_orientation(sgl_obj_t *obj, sgl_barchart_orientation_t orientation)
{
    SGL_ASSERT(obj != NULL);
    sgl_barchart_t *chart = sgl_container_of(obj, sgl_barchart_t, obj);
    chart->option_bits.orientation = (orientation == SGL_BARCHART_ORIENTATION_HORIZONTAL) ? 1U : 0U;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Enable or disable one-shot open animation
 */
static inline void sgl_barchart_enable_open_anim(sgl_obj_t *obj, bool enable)
{
    SGL_ASSERT(obj != NULL);
    sgl_barchart_t *chart = sgl_container_of(obj, sgl_barchart_t, obj);
    chart->option_bits.open_anim_enable = enable ? 1U : 0U;
    chart->option_bits.open_anim_playing = 0U;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set open animation direction
 */
static inline void sgl_barchart_set_open_anim_dir(sgl_obj_t *obj, sgl_barchart_open_anim_dir_t dir)
{
    SGL_ASSERT(obj != NULL);
    sgl_container_of(obj, sgl_barchart_t, obj)->option_bits.open_anim_dir = (uint8_t)dir;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set open animation duration in milliseconds
 * @param duration 0 uses default duration
 */
static inline void sgl_barchart_set_open_anim_duration(sgl_obj_t *obj, uint16_t duration)
{
    SGL_ASSERT(obj != NULL);
    sgl_container_of(obj, sgl_barchart_t, obj)->open_anim_duration = duration ? duration : SGL_BARCHART_OPEN_ANIM_DURATION;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set open animation easing path algorithm
 * @param path_algo easing function pointer, NULL means linear animation
 */
static inline void sgl_barchart_set_open_anim_path(sgl_obj_t *obj, sgl_anim_path_algo_t path_algo)
{
    SGL_ASSERT(obj != NULL);
    sgl_container_of(obj, sgl_barchart_t, obj)->open_anim_path = path_algo;
}

#ifdef __cplusplus
}
#endif

#endif
