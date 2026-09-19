/* source/widgets/chart/piechart/sgl_piechart.h
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

#ifndef __SGL_PIECHART_H__
#define __SGL_PIECHART_H__


#include <sgl_core.h>
#include <sgl_draw.h>
#include <sgl_math.h>
#include <sgl_log.h>
#include <sgl_mm.h>
#include <sgl_cfgfix.h>
#include <string.h>
#include <sgl_anim.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @brief Legend position of piechart
 */
typedef enum sgl_piechart_legend_pos {
    SGL_PIECHART_LEGEND_POS_NONE = 0,   /**< Do not draw legend */
    SGL_PIECHART_LEGEND_POS_LEFT,       /**< Legend at left side of pie */
    SGL_PIECHART_LEGEND_POS_RIGHT,      /**< Legend at right side of pie */
    SGL_PIECHART_LEGEND_POS_TOP,        /**< Legend at top of pie */
    SGL_PIECHART_LEGEND_POS_BOTTOM,     /**< Legend at bottom of pie */
} sgl_piechart_legend_pos_t;


/**
 * @brief Legend layout direction
 */
typedef enum sgl_piechart_legend_dir {
    SGL_PIECHART_LEGEND_DIR_VERTICAL = 0,   /**< Legend items from top to bottom */
    SGL_PIECHART_LEGEND_DIR_HORIZONTAL = 1, /**< Legend items from left to right */
} sgl_piechart_legend_dir_t;

typedef struct sgl_piechart_option_bits {
    uint8_t legend_enable     : 1;
    uint8_t smooth            : 1;
    uint8_t legend_bg_enable  : 1;
    uint8_t open_anim_enable  : 1;
    uint8_t open_anim_playing : 1;
    uint8_t legend_dir        : 1;
    uint8_t reserved          : 2;
} sgl_piechart_option_bits_t;

typedef struct sgl_piechart_layout_bits {
    uint8_t legend_pos : 3;
    uint8_t reserved   : 5;
} sgl_piechart_layout_bits_t;

/**
 * @brief Single slice of piechart
 * @value: numeric value of slice (>=0)
 * @color: color of slice
 * @alpha: alpha of slice (0~255), 0 means use global alpha
 * @label: legend text of slice, must be persistent string
 */
typedef struct sgl_piechart_slice {
    const char *label;
    int32_t     value;
    sgl_color_t color;
    uint8_t     alpha;
} sgl_piechart_slice_t;


/**
 * @brief Pie chart widget object
 * @obj:   base object
 */
typedef struct sgl_piechart {
    sgl_obj_t              obj;
    const sgl_font_t *legend_font;         /**< legend text font */
    sgl_piechart_slice_t *slices;          /**< dynamic slice array */
    sgl_anim_path_algo_t  open_anim_path;  /**< easing function for open animation, NULL=linear */
    sgl_color_t       legend_text_color;   /**< legend text color */
    sgl_color_t       legend_bg_color;     /**< legend background color */
    sgl_color_t       legend_border_color; /**< legend border color */
    uint32_t          open_anim_start_tick;/**< animation start tick, ms */
    int32_t           total_value;         /**< cached sum of all slice values (>0) */
    uint16_t          radius_out;          /**< outer radius in pixels, 0: auto from object size */
    int16_t           start_angle;         /**< start angle of first slice, unit: degree */
    uint16_t          legend_area_size;    /**< thickness of legend area (width or height) */
    /* global pie config */
    sgl_piechart_option_bits_t option_bits;
    sgl_piechart_layout_bits_t layout_bits;
    uint8_t     inner_radius_rate; /**< inner radius = outer * rate / 100, 0: full pie */
    uint8_t     legend_box_size;   /**< legend color box size in pixels */
    uint8_t     legend_item_gap;   /**< gap between legend items in pixels */
    uint8_t     legend_padding;    /**< inner padding of legend area */
    uint8_t     slice_count;       /**< number of slices */
    uint8_t     alpha;              /**< global alpha of pie (0~255) */
    uint8_t     legend_alpha;       /**< legend alpha */
} sgl_piechart_t;


/**
 * @brief Create a piechart widget
 * @param parent parent object
 * @return piechart object
 */
sgl_obj_t* sgl_piechart_create(sgl_obj_t *parent);


/**
 * @brief Set number of slices
 * @param obj piechart object
 * @param count slice count
 * @note This function reallocates internal slice array, old labels pointer
 *       are discarded. User should re-set slice values and labels afterwards.
 */
void sgl_piechart_set_slice_count(sgl_obj_t *obj, uint8_t count);


/**
 * @brief Set slice value
 * @param obj piechart object
 * @param index slice index (0-based)
 * @param value value of slice
 */
void sgl_piechart_set_slice_value(sgl_obj_t *obj, uint8_t index, int32_t value);


/**
 * @brief Set slice color
 * @param obj piechart object
 * @param index slice index (0-based)
 * @param color color of slice
 */
void sgl_piechart_set_slice_color(sgl_obj_t *obj, uint8_t index, sgl_color_t color);


/**
 * @brief Set slice alpha
 * @param obj piechart object
 * @param index slice index (0-based)
 * @param alpha alpha of slice
 */
void sgl_piechart_set_slice_alpha(sgl_obj_t *obj, uint8_t index, uint8_t alpha);


/**
 * @brief Set slice legend text
 * @param obj piechart object
 * @param index slice index (0-based)
 * @param label zero-terminated UTF-8 string, must be persistent
 */
void sgl_piechart_set_slice_label(sgl_obj_t *obj, uint8_t index, const char *label);


/**
 * @brief Convenient function to set value, color and label of a slice
 */
static inline void sgl_piechart_set_slice(sgl_obj_t *obj, uint8_t index, int32_t value, sgl_color_t color, const char *label)
{
    sgl_piechart_set_slice_value(obj, index, value);
    sgl_piechart_set_slice_color(obj, index, color);
    sgl_piechart_set_slice_label(obj, index, label);
}


/**
 * @brief Set global alpha of piechart
 * @param obj piechart object
 * @param alpha alpha (0~255)
 */
static inline void sgl_piechart_set_alpha(sgl_obj_t *obj, uint8_t alpha)
{
    sgl_piechart_t *pie = sgl_container_of(obj, sgl_piechart_t, obj);
    pie->alpha = alpha;
    sgl_obj_set_dirty(obj);
}


/**
 * @brief Set start angle of first slice
 * @param obj piechart object
 * @param angle degree, can be any integer, will be normalized to [0, 360)
 */
static inline void sgl_piechart_set_start_angle(sgl_obj_t *obj, int16_t angle)
{
    sgl_piechart_t *pie = sgl_container_of(obj, sgl_piechart_t, obj);
    pie->start_angle = angle;
    sgl_obj_set_dirty(obj);
}


/**
 * @brief Set inner radius ratio (donut mode)
 * @param obj piechart object
 * @param rate percent of outer radius (0~100)
 */
static inline void sgl_piechart_set_inner_radius_rate(sgl_obj_t *obj, uint8_t rate)
{
    sgl_piechart_t *pie = sgl_container_of(obj, sgl_piechart_t, obj);
    if (rate > 100) rate = 100;
    pie->inner_radius_rate = rate;
    sgl_obj_set_dirty(obj);
}


/**
 * @brief Set outer radius of piechart
 * @param obj    piechart object
 * @param radius outer radius in pixels, 0: auto compute from object size
 */
static inline void sgl_piechart_set_radius(sgl_obj_t *obj, uint16_t radius)
{
    sgl_piechart_t *pie = sgl_container_of(obj, sgl_piechart_t, obj);
    pie->radius_out = radius;
    sgl_obj_set_dirty(obj);
}
 
 
/**
 * @brief Enable/disable smooth edge of slices
 * @param obj piechart object
 * @param enable true to enable
 */
static inline void sgl_piechart_set_smooth(sgl_obj_t *obj, bool enable)
{
    sgl_piechart_t *pie = sgl_container_of(obj, sgl_piechart_t, obj);
    pie->option_bits.smooth = enable ? 1U : 0U;
    sgl_obj_set_dirty(obj);
}


/**
 * @brief Enable/disable legend
 */
static inline void sgl_piechart_enable_legend(sgl_obj_t *obj, bool enable)
{
    sgl_piechart_t *pie = sgl_container_of(obj, sgl_piechart_t, obj);
    pie->option_bits.legend_enable = enable ? 1U : 0U;
    sgl_obj_set_dirty(obj);
}


/**
 * @brief Set legend position
 */
static inline void sgl_piechart_set_legend_pos(sgl_obj_t *obj, sgl_piechart_legend_pos_t pos)
{
    sgl_piechart_t *pie = sgl_container_of(obj, sgl_piechart_t, obj);
    pie->layout_bits.legend_pos = (uint8_t)pos;
    sgl_obj_set_dirty(obj);
}


/**
 * @brief Set legend direction (vertical / horizontal)
 */
static inline void sgl_piechart_set_legend_dir(sgl_obj_t *obj, sgl_piechart_legend_dir_t dir)
{
    sgl_piechart_t *pie = sgl_container_of(obj, sgl_piechart_t, obj);
    pie->option_bits.legend_dir = (dir == SGL_PIECHART_LEGEND_DIR_HORIZONTAL) ? 1U : 0U;
    sgl_obj_set_dirty(obj);
}


/**
 * @brief Set legend area size
 * @param size when legend is LEFT/RIGHT, it means width; when TOP/BOTTOM, it means height
 */
static inline void sgl_piechart_set_legend_area_size(sgl_obj_t *obj, uint16_t size)
{
    sgl_piechart_t *pie = sgl_container_of(obj, sgl_piechart_t, obj);
    pie->legend_area_size = size;
    sgl_obj_set_dirty(obj);
}


/**
 * @brief Set legend font
 */
static inline void sgl_piechart_set_legend_font(sgl_obj_t *obj, const sgl_font_t *font)
{
    sgl_piechart_t *pie = sgl_container_of(obj, sgl_piechart_t, obj);
    pie->legend_font = font;
    sgl_obj_set_dirty(obj);
}


/**
 * @brief Set legend text color
 */
static inline void sgl_piechart_set_legend_text_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_piechart_t *pie = sgl_container_of(obj, sgl_piechart_t, obj);
    pie->legend_text_color = color;
    sgl_obj_set_dirty(obj);
}


/**
 * @brief Set legend alpha
 */
static inline void sgl_piechart_set_legend_alpha(sgl_obj_t *obj, uint8_t alpha)
{
    sgl_piechart_t *pie = sgl_container_of(obj, sgl_piechart_t, obj);
    pie->legend_alpha = alpha;
    sgl_obj_set_dirty(obj);
}


/**
 * @brief Set legend color box size
 */
static inline void sgl_piechart_set_legend_box_size(sgl_obj_t *obj, uint8_t size)
{
    sgl_piechart_t *pie = sgl_container_of(obj, sgl_piechart_t, obj);
    pie->legend_box_size = size;
    sgl_obj_set_dirty(obj);
}


/**
 * @brief Enable/disable open animation (0->360 degree reveal on first show)
 * @param obj piechart object
 * @param enable true to enable
 */
static inline void sgl_piechart_enable_open_anim(sgl_obj_t *obj, bool enable)
{
    sgl_piechart_t *pie = sgl_container_of(obj, sgl_piechart_t, obj);
    pie->option_bits.open_anim_enable = enable ? 1U : 0U;
    pie->option_bits.open_anim_playing = 0U; /* reset state so next draw will restart animation */
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set open animation path algorithm (use SGL_ANIM_PATH_* macros)
 * @param obj       piechart object
 * @param path_algo path algorithm function, e.g. SGL_ANIM_PATH_EASE_IN
 */
static inline void sgl_piechart_set_open_anim_path(sgl_obj_t *obj, sgl_anim_path_algo_t path_algo)
{
    sgl_piechart_t *pie = sgl_container_of(obj, sgl_piechart_t, obj);
    pie->open_anim_path = path_algo;
}

/**
 * @brief Set legend padding
 */
static inline void sgl_piechart_set_legend_padding(sgl_obj_t *obj, uint8_t padding)
{
    sgl_piechart_t *pie = sgl_container_of(obj, sgl_piechart_t, obj);
    pie->legend_padding = padding;
    sgl_obj_set_dirty(obj);
}


/**
 * @brief Set legend item gap
 */
static inline void sgl_piechart_set_legend_item_gap(sgl_obj_t *obj, uint8_t gap)
{
    sgl_piechart_t *pie = sgl_container_of(obj, sgl_piechart_t, obj);
    pie->legend_item_gap = gap;
    sgl_obj_set_dirty(obj);
}


/**
 * @brief Enable/disable legend background block
 */
static inline void sgl_piechart_enable_legend_bg(sgl_obj_t *obj, bool enable)
{
    sgl_piechart_t *pie = sgl_container_of(obj, sgl_piechart_t, obj);
    pie->option_bits.legend_bg_enable = enable ? 1U : 0U;
    sgl_obj_set_dirty(obj);
}


/**
 * @brief Set legend background color
 */
static inline void sgl_piechart_set_legend_bg_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_piechart_t *pie = sgl_container_of(obj, sgl_piechart_t, obj);
    pie->legend_bg_color = color;
    sgl_obj_set_dirty(obj);
}


/**
 * @brief Set legend border color
 */
static inline void sgl_piechart_set_legend_border_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_piechart_t *pie = sgl_container_of(obj, sgl_piechart_t, obj);
    pie->legend_border_color = color;
    sgl_obj_set_dirty(obj);
}


#ifdef __cplusplus
}
#endif


#endif // __SGL_PIECHART_H__
