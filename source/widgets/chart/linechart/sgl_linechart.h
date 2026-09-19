/* source/widgets/chart/linechart/sgl_linechart.h
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

#ifndef __SGL_LINECHART_H__
#define __SGL_LINECHART_H__


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


#define SGL_LINECHART_MAX_AUTO_TICKS      (8)
#define SGL_LINECHART_DEFAULT_DIVISIONS   (4)
#define SGL_LINECHART_OPEN_ANIM_DURATION  (600U)


/**
 * @brief Axis selector of linechart
 */
typedef enum sgl_linechart_axis_id {
    SGL_LINECHART_AXIS_X = 0,
    SGL_LINECHART_AXIS_Y = 1,
} sgl_linechart_axis_id_t;


/**
 * @brief Point shape of linechart series
 */
typedef enum sgl_linechart_point_shape {
    SGL_LINECHART_POINT_SHAPE_NONE   = 0,
    SGL_LINECHART_POINT_SHAPE_CIRCLE = 1,
    SGL_LINECHART_POINT_SHAPE_SQUARE = 2,
} sgl_linechart_point_shape_t;


/**
 * @brief Visual mode of a data series
 */
typedef enum sgl_linechart_series_mode {
    SGL_LINECHART_SERIES_MODE_LINE           = 0, /**< Connected line only */
    SGL_LINECHART_SERIES_MODE_SCATTER        = 1, /**< Points only (scatter plot) */
    SGL_LINECHART_SERIES_MODE_LINE_AND_POINT = 2, /**< Line with points */
} sgl_linechart_series_mode_t;


/**
 * @brief Open animation direction
 */
typedef enum sgl_linechart_open_anim_dir {
    SGL_LINECHART_OPEN_ANIM_NONE      = 0,
    SGL_LINECHART_OPEN_ANIM_FROM_LEFT = 1, /**< Reveal from left to right */
    SGL_LINECHART_OPEN_ANIM_FROM_TOP  = 2, /**< Reveal from top to bottom */
} sgl_linechart_open_anim_dir_t;

typedef struct sgl_linechart_axis_bits {
    uint8_t auto_scale  : 1;
    uint8_t show_grid   : 1;
    uint8_t grid_dashed : 1;
    uint8_t show_labels : 1;
    uint8_t show_ticks  : 1;
    uint8_t reserved    : 3;
} sgl_linechart_axis_bits_t;

typedef struct sgl_linechart_series_bits {
    uint8_t show_line   : 1;
    uint8_t show_points : 1;
    uint8_t fill_under  : 1;
    uint8_t point_shape : 2;
    uint8_t reserved    : 3;
} sgl_linechart_series_bits_t;

typedef struct sgl_linechart_option_bits {
    uint8_t custom_plot_rect  : 1;
    uint8_t open_anim_enable  : 1;
    uint8_t open_anim_playing : 1;
    uint8_t open_anim_dir     : 2;
    uint8_t reserved          : 3;
} sgl_linechart_option_bits_t;

/**
 * @brief Axis configuration of linechart
 * @min:           minimum logical value of axis
 * @max:           maximum logical value of axis
 * @step:          tick step when >0, 0 means auto compute by @auto_divisions
 * @auto_scale:    when set, min/max will be computed from all series data
 * @show_grid:     enable grid lines on this axis
 * @grid_dashed:   0: solid grid, non-zero: dashed grid
 * @show_labels:   show numeric labels on this axis
 * @auto_divisions: suggested number of steps when auto computing step (>=1)
 * @grid_color:    grid line color
 * @grid_alpha:    grid line alpha
 * @label_font:    font of axis label text (NULL: system font)
 * @label_color:   label text color
 * @label_alpha:   label text alpha
 */
typedef struct sgl_linechart_axis {
    sgl_linechart_axis_bits_t flag_bits;
    const sgl_font_t *label_font;
    int32_t           min;
    int32_t           max;
    int32_t           step;
    sgl_color_t       grid_color;
    sgl_color_t       label_color;
    uint8_t           auto_divisions;
    uint8_t           grid_alpha;
    uint8_t           label_alpha;
} sgl_linechart_axis_t;


/**
 * @brief Single data series of linechart
 * @x_data:       pointer to X values array, NULL means use index [0..point_count-1]
 * @y_data:       pointer to Y values array, must not be NULL for valid series
 * @point_count:  number of points in this series
 * @show_line:    draw connecting polyline
 * @show_points:  draw point markers
 * @fill_under:   fill area between line and X axis baseline
 * @point_shape:  point shape @ref sgl_linechart_point_shape_t
 * @line_width:   logical line width (1, 2, ...) before internal scaling
 * @point_radius: marker radius in pixels
 * @line_color:   polyline and point color
 * @line_alpha:   polyline alpha
 * @fill_color:   area fill color when @fill_under is set
 * @fill_alpha:   area fill alpha (0 disables fill)
 * @label:        optional series label text, must be persistent string
 */
typedef struct sgl_linechart_series {
    sgl_linechart_series_bits_t style_bits;
    const int32_t    *x_data;
    const int32_t    *y_data;
    const char       *label;
    sgl_color_t       line_color;
    sgl_color_t       fill_color;
    uint16_t          point_count;
    uint8_t           line_width;
    uint8_t           point_radius;
    uint8_t           line_alpha;
    uint8_t           fill_alpha;
} sgl_linechart_series_t;


/**
 * @brief Line chart widget object
 * @obj:           base object
 * @alpha:         global content alpha (0~255)
 * @bg_color:      chart background color
 * @bg_alpha:      chart background alpha
 * @border_color:  border color (uses obj->border as width)
 * @x_axis:        X axis configuration
 * @y_axis:        Y axis configuration
 * @series_count:  number of series
 * @series:        dynamic array of @ref sgl_linechart_series_t
 * @open_anim_enable:  enable one-shot open animation on first draw
 * @open_anim_playing: internal flag: animation currently active
 * @open_anim_dir: animation direction
 * @open_anim_start_tick: start tick of open animation
 * @open_anim_path: optional easing path algorithm, NULL for linear
 */
typedef struct sgl_linechart {
    sgl_obj_t             obj;
    sgl_linechart_axis_t  x_axis;
    sgl_linechart_axis_t  y_axis;
    sgl_area_t            plot_rel_rect;   /**< Custom plot area relative to widget (x1,y1,x2,y2) */
    sgl_linechart_series_t *series;
    const char           **x_labels;       /**< Optional X axis labels, array of persistent strings */
    sgl_anim_path_algo_t  open_anim_path;
    sgl_color_t           bg_color;
    sgl_color_t           border_color;
    uint32_t              open_anim_start_tick;
    sgl_linechart_option_bits_t option_bits;   /**< packed plot/layout/animation flags */
    uint8_t               alpha;
    uint8_t               bg_alpha;
    uint8_t               series_count;
    uint8_t               x_label_count;   /**< Number of X axis labels */
} sgl_linechart_t;


/**
 * @brief Create a linechart widget
 * @param parent parent object
 * @return linechart object
 */
sgl_obj_t* sgl_linechart_create(sgl_obj_t *parent);
 
 
/**
 * @brief Set series count and reallocate series array
 * @param obj      linechart object
 * @param count    desired series count
 * @note Existing series configuration will be discarded.
 */
void sgl_linechart_set_series_count(sgl_obj_t *obj, uint8_t count);
 
 
/**
 * @brief Bind data buffers for a series
 * @param obj         linechart object
 * @param index       series index (0-based)
 * @param x_data      pointer to X data array (NULL: use index as X)
 * @param y_data      pointer to Y data array (must not be NULL)
 * @param point_count number of points in the series
 */
void sgl_linechart_set_series_data(sgl_obj_t *obj, uint8_t index,
                                   const int32_t *x_data,
                                   const int32_t *y_data,
                                   uint16_t point_count);
 
 
/**
 * @brief Bind only Y data array for a series (X uses index 0..point_count-1).
 * @param obj         linechart object
 * @param index       series index (0-based)
 * @param y_data      pointer to Y data array
 * @param point_count number of points in the series
 */
void sgl_linechart_set_series_y_array(sgl_obj_t *obj, uint8_t index,
                                      const int32_t *y_data,
                                      uint16_t point_count);
 
 
/**
 * @brief Set X axis label string array.
 * @param obj    linechart object
 * @param labels pointer to label string array; strings must be persistent
 * @param count  number of labels
 */
void sgl_linechart_set_x_labels(sgl_obj_t *obj, const char **labels, uint8_t count);
void sgl_linechart_update_value(sgl_obj_t *obj, uint8_t series_index, uint16_t point_index);
 
 
/**
 * @brief Request chart to redraw with latest data.
 *        Use this if you modify data in-place without re-binding arrays.
 */
static inline void sgl_linechart_update(sgl_obj_t *obj)
{
    SGL_ASSERT(obj != NULL);
    sgl_obj_set_dirty(obj);
}
 
 
/* ---------- Inline helpers for axis configuration ---------- */

static inline sgl_linechart_axis_t* sgl_linechart_get_axis(sgl_linechart_t *chart, sgl_linechart_axis_id_t axis)
{
    return (axis == SGL_LINECHART_AXIS_Y) ? &chart->y_axis : &chart->x_axis;
}


/**
 * @brief Set axis range (disable auto-scale for this axis)
 */
static inline void sgl_linechart_set_axis_range(sgl_obj_t *obj, sgl_linechart_axis_id_t axis,
                                                int32_t min, int32_t max)
{
    SGL_ASSERT(obj != NULL);
    sgl_linechart_t *chart = sgl_container_of(obj, sgl_linechart_t, obj);
    sgl_linechart_axis_t *a = sgl_linechart_get_axis(chart, axis);
    a->min = min;
    a->max = max;
    a->flag_bits.auto_scale = 0U;
    sgl_obj_set_dirty(obj);
}


/**
 * @brief Enable or disable axis auto scale (min/max from data)
 */
static inline void sgl_linechart_enable_axis_auto_scale(sgl_obj_t *obj, sgl_linechart_axis_id_t axis, bool enable)
{
    SGL_ASSERT(obj != NULL);
    sgl_linechart_t *chart = sgl_container_of(obj, sgl_linechart_t, obj);
    sgl_linechart_axis_t *a = sgl_linechart_get_axis(chart, axis);
    a->flag_bits.auto_scale = enable ? 1U : 0U;
    sgl_obj_set_dirty(obj);
}


/**
 * @brief Set axis tick step (0 means auto compute)
 */
static inline void sgl_linechart_set_axis_step(sgl_obj_t *obj, sgl_linechart_axis_id_t axis, int32_t step)
{
    SGL_ASSERT(obj != NULL);
    sgl_linechart_t *chart = sgl_container_of(obj, sgl_linechart_t, obj);
    sgl_linechart_axis_t *a = sgl_linechart_get_axis(chart, axis);
    a->step = step;
    sgl_obj_set_dirty(obj);
}


/**
 * @brief Set axis auto division count used when step == 0
 */
static inline void sgl_linechart_set_axis_auto_divisions(sgl_obj_t *obj, sgl_linechart_axis_id_t axis, uint8_t divisions)
{
    SGL_ASSERT(obj != NULL);
    sgl_linechart_t *chart = sgl_container_of(obj, sgl_linechart_t, obj);
    sgl_linechart_axis_t *a = sgl_linechart_get_axis(chart, axis);
    a->auto_divisions = divisions ? divisions : SGL_LINECHART_DEFAULT_DIVISIONS;
    sgl_obj_set_dirty(obj);
}


/**
 * @brief Enable or disable axis grid lines
 */
static inline void sgl_linechart_enable_axis_grid(sgl_obj_t *obj, sgl_linechart_axis_id_t axis, bool enable)
{
    SGL_ASSERT(obj != NULL);
    sgl_linechart_t *chart = sgl_container_of(obj, sgl_linechart_t, obj);
    sgl_linechart_axis_t *a = sgl_linechart_get_axis(chart, axis);
    a->flag_bits.show_grid = enable ? 1U : 0U;
    sgl_obj_set_dirty(obj);
}


/**
 * @brief Set axis grid style
 * @param dashed 0: solid, non-zero: dashed
 */
static inline void sgl_linechart_set_axis_grid_style(sgl_obj_t *obj, sgl_linechart_axis_id_t axis, uint8_t dashed)
{
    SGL_ASSERT(obj != NULL);
    sgl_linechart_t *chart = sgl_container_of(obj, sgl_linechart_t, obj);
    sgl_linechart_axis_t *a = sgl_linechart_get_axis(chart, axis);
    a->flag_bits.grid_dashed = (dashed != 0U) ? 1U : 0U;
    sgl_obj_set_dirty(obj);
}


/**
 * @brief Set axis grid color and alpha
 */
static inline void sgl_linechart_set_axis_grid_color(sgl_obj_t *obj, sgl_linechart_axis_id_t axis,
                                                     sgl_color_t color, uint8_t alpha)
{
    SGL_ASSERT(obj != NULL);
    sgl_linechart_t *chart = sgl_container_of(obj, sgl_linechart_t, obj);
    sgl_linechart_axis_t *a = sgl_linechart_get_axis(chart, axis);
    a->grid_color = color;
    a->grid_alpha = alpha;
    sgl_obj_set_dirty(obj);
}


/**
 * @brief Enable or disable axis labels
 */
static inline void sgl_linechart_enable_axis_labels(sgl_obj_t *obj, sgl_linechart_axis_id_t axis, bool enable)
{
    SGL_ASSERT(obj != NULL);
    sgl_linechart_t *chart = sgl_container_of(obj, sgl_linechart_t, obj);
    sgl_linechart_axis_t *a = sgl_linechart_get_axis(chart, axis);
    a->flag_bits.show_labels = enable ? 1U : 0U;
    sgl_obj_set_dirty(obj);
}


/**
 * @brief Set axis label font
 */
static inline void sgl_linechart_set_axis_label_font(sgl_obj_t *obj, sgl_linechart_axis_id_t axis,
                                                     const sgl_font_t *font)
{
    SGL_ASSERT(obj != NULL);
    sgl_linechart_t *chart = sgl_container_of(obj, sgl_linechart_t, obj);
    sgl_linechart_axis_t *a = sgl_linechart_get_axis(chart, axis);
    a->label_font = font;
    sgl_obj_set_dirty(obj);
}


/**
 * @brief Set axis label text color and alpha
 */
static inline void sgl_linechart_set_axis_label_color(sgl_obj_t *obj, sgl_linechart_axis_id_t axis,
                                                      sgl_color_t color, uint8_t alpha)
{
    SGL_ASSERT(obj != NULL);
    sgl_linechart_t *chart = sgl_container_of(obj, sgl_linechart_t, obj);
    sgl_linechart_axis_t *a = sgl_linechart_get_axis(chart, axis);
    a->label_color = color;
    a->label_alpha = alpha;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Enable or disable axis tick marks at axis edge
 */
static inline void sgl_linechart_enable_axis_ticks(sgl_obj_t *obj, sgl_linechart_axis_id_t axis, bool enable)
{
    SGL_ASSERT(obj != NULL);
    sgl_linechart_t *chart = sgl_container_of(obj, sgl_linechart_t, obj);
    sgl_linechart_axis_t *a = sgl_linechart_get_axis(chart, axis);
    a->flag_bits.show_ticks = enable ? 1U : 0U;
    sgl_obj_set_dirty(obj);
}


/* ---------- Inline helpers for chart appearance ---------- */

/**
 * @brief Set chart background color
 */
static inline void sgl_linechart_set_bg_color(sgl_obj_t *obj, sgl_color_t color)
{
    SGL_ASSERT(obj != NULL);
    sgl_linechart_t *chart = sgl_container_of(obj, sgl_linechart_t, obj);
    chart->bg_color = color;
    sgl_obj_set_dirty(obj);
}


/**
 * @brief Set chart background alpha
 */
static inline void sgl_linechart_set_bg_alpha(sgl_obj_t *obj, uint8_t alpha)
{
    SGL_ASSERT(obj != NULL);
    sgl_linechart_t *chart = sgl_container_of(obj, sgl_linechart_t, obj);
    chart->bg_alpha = alpha;
    sgl_obj_set_dirty(obj);
}


/**
 * @brief Set chart border color (uses obj->border as width)
 */
static inline void sgl_linechart_set_border_color(sgl_obj_t *obj, sgl_color_t color)
{
    SGL_ASSERT(obj != NULL);
    sgl_linechart_t *chart = sgl_container_of(obj, sgl_linechart_t, obj);
    chart->border_color = color;
    sgl_obj_set_dirty(obj);
}


/**
 * @brief Set global chart alpha (applied to series/grid/labels)
 */
static inline void sgl_linechart_set_alpha(sgl_obj_t *obj, uint8_t alpha)
{
    SGL_ASSERT(obj != NULL);
    sgl_linechart_t *chart = sgl_container_of(obj, sgl_linechart_t, obj);
    chart->alpha = alpha;
    sgl_obj_set_dirty(obj);
}


/* ---------- Inline helpers for plot area layout ---------- */

/**
 * @brief Set custom plot area relative to widget top-left.
 *
 * @param offset_left  distance from widget left border to plot area's left edge
 * @param offset_top   distance from widget top border to plot area's top edge
 * @param width        plot area width in pixels
 * @param height       plot area height in pixels
 *
 * When width/height <= 0, the layout falls back to auto mode.
 * The specified area does not include widget border; it is relative to obj->coords.x1/y1.
 */
static inline void sgl_linechart_set_plot_area_rel(sgl_obj_t *obj,
                                                   int16_t offset_left,
                                                   int16_t offset_top,
                                                   int16_t width,
                                                   int16_t height)
{
    SGL_ASSERT(obj != NULL);
    sgl_linechart_t *chart = sgl_container_of(obj, sgl_linechart_t, obj);

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
 * @brief Reset plot area to auto layout (use internal margin calculation).
 */
static inline void sgl_linechart_reset_plot_area(sgl_obj_t *obj)
{
    SGL_ASSERT(obj != NULL);
    sgl_linechart_t *chart = sgl_container_of(obj, sgl_linechart_t, obj);
    chart->option_bits.custom_plot_rect = 0U;
    sgl_obj_set_dirty(obj);
}


/* ---------- Inline helpers for series configuration ---------- */

/**
 * @brief Set series line/point mode
 */
static inline void sgl_linechart_set_series_mode(sgl_obj_t *obj, uint8_t index, sgl_linechart_series_mode_t mode)
{
    SGL_ASSERT(obj != NULL);
    sgl_linechart_t *chart = sgl_container_of(obj, sgl_linechart_t, obj);
    if (index >= chart->series_count || chart->series == NULL) {
        SGL_LOG_ERROR("sgl_linechart_set_series_mode: invalid index %d", index);
        return;
    }
    sgl_linechart_series_t *s = &chart->series[index];
    switch (mode) {
    default:
    case SGL_LINECHART_SERIES_MODE_LINE:
        s->style_bits.show_line = 1U;
        s->style_bits.show_points = 0U;
        break;
    case SGL_LINECHART_SERIES_MODE_SCATTER:
        s->style_bits.show_line = 0U;
        s->style_bits.show_points = 1U;
        break;
    case SGL_LINECHART_SERIES_MODE_LINE_AND_POINT:
        s->style_bits.show_line = 1U;
        s->style_bits.show_points = 1U;
        break;
    }
    sgl_obj_set_dirty(obj);
}


/**
 * @brief Set series line color
 */
static inline void sgl_linechart_set_series_line_color(sgl_obj_t *obj, uint8_t index, sgl_color_t color)
{
    SGL_ASSERT(obj != NULL);
    sgl_linechart_t *chart = sgl_container_of(obj, sgl_linechart_t, obj);
    if (index >= chart->series_count || chart->series == NULL) {
        SGL_LOG_ERROR("sgl_linechart_set_series_line_color: invalid index %d", index);
        return;
    }
    chart->series[index].line_color = color;
    sgl_obj_set_dirty(obj);
}


/**
 * @brief Set series line alpha
 */
static inline void sgl_linechart_set_series_line_alpha(sgl_obj_t *obj, uint8_t index, uint8_t alpha)
{
    SGL_ASSERT(obj != NULL);
    sgl_linechart_t *chart = sgl_container_of(obj, sgl_linechart_t, obj);
    if (index >= chart->series_count || chart->series == NULL) {
        SGL_LOG_ERROR("sgl_linechart_set_series_line_alpha: invalid index %d", index);
        return;
    }
    chart->series[index].line_alpha = alpha;
    sgl_obj_set_dirty(obj);
}


/**
 * @brief Set series line width (logical units, 1,2,...)
 */
static inline void sgl_linechart_set_series_line_width(sgl_obj_t *obj, uint8_t index, uint8_t width)
{
    SGL_ASSERT(obj != NULL);
    sgl_linechart_t *chart = sgl_container_of(obj, sgl_linechart_t, obj);
    if (index >= chart->series_count || chart->series == NULL) {
        SGL_LOG_ERROR("sgl_linechart_set_series_line_width: invalid index %d", index);
        return;
    }
    chart->series[index].line_width = width ? width : 1;
    sgl_obj_set_dirty(obj);
}


/**
 * @brief Enable or disable series point markers
 */
static inline void sgl_linechart_enable_series_points(sgl_obj_t *obj, uint8_t index, bool enable)
{
    SGL_ASSERT(obj != NULL);
    sgl_linechart_t *chart = sgl_container_of(obj, sgl_linechart_t, obj);
    if (index >= chart->series_count || chart->series == NULL) {
        SGL_LOG_ERROR("sgl_linechart_enable_series_points: invalid index %d", index);
        return;
    }
    chart->series[index].style_bits.show_points = enable ? 1U : 0U;
    sgl_obj_set_dirty(obj);
}


/**
 * @brief Set series point shape and radius
 */
static inline void sgl_linechart_set_series_point_style(sgl_obj_t *obj, uint8_t index,
                                                        sgl_linechart_point_shape_t shape,
                                                        uint8_t radius)
{
    SGL_ASSERT(obj != NULL);
    sgl_linechart_t *chart = sgl_container_of(obj, sgl_linechart_t, obj);
    if (index >= chart->series_count || chart->series == NULL) {
        SGL_LOG_ERROR("sgl_linechart_set_series_point_style: invalid index %d", index);
        return;
    }
    sgl_linechart_series_t *s = &chart->series[index];
    s->style_bits.point_shape = (uint8_t)shape;
    if (radius) {
        s->point_radius = radius;
    }
    sgl_obj_set_dirty(obj);
}


/**
 * @brief Enable/disable area fill under series line
 */
static inline void sgl_linechart_enable_series_fill(sgl_obj_t *obj, uint8_t index, bool enable)
{
    SGL_ASSERT(obj != NULL);
    sgl_linechart_t *chart = sgl_container_of(obj, sgl_linechart_t, obj);
    if (index >= chart->series_count || chart->series == NULL) {
        SGL_LOG_ERROR("sgl_linechart_enable_series_fill: invalid index %d", index);
        return;
    }
    chart->series[index].style_bits.fill_under = enable ? 1U : 0U;
    sgl_obj_set_dirty(obj);
}


/**
 * @brief Set series fill color and alpha
 */
static inline void sgl_linechart_set_series_fill_color(sgl_obj_t *obj, uint8_t index,
                                                       sgl_color_t color, uint8_t alpha)
{
    SGL_ASSERT(obj != NULL);
    sgl_linechart_t *chart = sgl_container_of(obj, sgl_linechart_t, obj);
    if (index >= chart->series_count || chart->series == NULL) {
        SGL_LOG_ERROR("sgl_linechart_set_series_fill_color: invalid index %d", index);
        return;
    }
    sgl_linechart_series_t *s = &chart->series[index];
    s->fill_color = color;
    s->fill_alpha = alpha;
    sgl_obj_set_dirty(obj);
}


/**
 * @brief Set series label text (for external legend or tooltips)
 * @param label zero-terminated UTF-8 string, must be persistent
 */
static inline void sgl_linechart_set_series_label(sgl_obj_t *obj, uint8_t index, const char *label)
{
    SGL_ASSERT(obj != NULL);
    sgl_linechart_t *chart = sgl_container_of(obj, sgl_linechart_t, obj);
    if (index >= chart->series_count || chart->series == NULL) {
        SGL_LOG_ERROR("sgl_linechart_set_series_label: invalid index %d", index);
        return;
    }
    chart->series[index].label = label;
    sgl_obj_set_dirty(obj);
}


/* ---------- Inline helpers for open animation ---------- */

/**
 * @brief Enable or disable linechart open animation
 */
static inline void sgl_linechart_enable_open_anim(sgl_obj_t *obj, bool enable)
{
    SGL_ASSERT(obj != NULL);
    sgl_linechart_t *chart = sgl_container_of(obj, sgl_linechart_t, obj);
    chart->option_bits.open_anim_enable = enable ? 1U : 0U;
    chart->option_bits.open_anim_playing = 0U;
    sgl_obj_set_dirty(obj);
}


/**
 * @brief Set open animation direction
 */
static inline void sgl_linechart_set_open_anim_dir(sgl_obj_t *obj, sgl_linechart_open_anim_dir_t dir)
{
    SGL_ASSERT(obj != NULL);
    sgl_linechart_t *chart = sgl_container_of(obj, sgl_linechart_t, obj);
    chart->option_bits.open_anim_dir = (uint8_t)dir;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set open animation path algorithm (use SGL_ANIM_PATH_* macros)
 * @param obj       linechart object
 * @param path_algo easing function, e.g. SGL_ANIM_PATH_EASE_IN_OUT
 */
static inline void sgl_linechart_set_open_anim_path(sgl_obj_t *obj, sgl_anim_path_algo_t path_algo)
{
    SGL_ASSERT(obj != NULL);
    sgl_linechart_t *chart = sgl_container_of(obj, sgl_linechart_t, obj);
    chart->open_anim_path = path_algo;
}

#ifdef __cplusplus
}
#endif

#endif /* __SGL_LINECHART_H__ */
