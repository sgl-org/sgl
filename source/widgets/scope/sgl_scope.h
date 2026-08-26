/* source/widgets/sgl_scope.h
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
#ifndef __SGL_SCOPE_H__
#define __SGL_SCOPE_H__

#include <sgl_core.h>
#include <sgl_draw.h>
#include <sgl_math.h>
#include <sgl_log.h>
#include <sgl_mm.h>
#include <sgl_cfgfix.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* max number of waveform channels */
#define SGL_SCOPE_MAX_CHANNELS      (4)

/* number of dirty strips the plot is split into while the FIFO is full
 * (scrolling): each strip is 1/N of the plot width and is marked only
 * as tall as the y extent of the samples it covers. 1 = whole-plot mark.
 * Strips beyond SGL_DIRTY_AREA_NUM_MAX simply merge into the last area */
#ifndef SGL_SCOPE_SCROLL_STRIPS
#define SGL_SCOPE_SCROLL_STRIPS     (4)
#endif
#if SGL_SCOPE_SCROLL_STRIPS < 1
#error "SGL_SCOPE_SCROLL_STRIPS must be >= 1"
#endif

/**
 * @brief scope widget, multi-channel oscilloscope style waveform display
 *
 * @note FIFO model: each channel owns the user-provided sample buffer as a
 *       ring FIFO. `sgl_scope_append_data()` is the producer (writes at `in`,
 *       overwrites the oldest sample and advances `out` when full); the draw
 *       pass is the consumer (scans oldest -> newest, one sample per pixel
 *       column). The buffer length of each channel MUST equal the widget
 *       width in pixels, so the hot draw loop needs no scaling at all.
 *
 * @wave_buffers  base of user-owned [channel_count][width] sample array
 * @wave_colors   user-owned [channel_count] waveform color array
 * @in / @out     per-channel FIFO write index / oldest displayed index
 * @count         per-channel number of valid samples
 */
typedef struct {
    sgl_obj_t obj;
    int16_t *wave_buffers;             // user-owned [channels][width] samples
    sgl_color_t *wave_colors;          // user-owned [channels] wave colors
    uint16_t in[SGL_SCOPE_MAX_CHANNELS];     // FIFO write index
    uint16_t out[SGL_SCOPE_MAX_CHANNELS];    // FIFO read (oldest) index
    uint16_t count[SGL_SCOPE_MAX_CHANNELS];  // valid samples per channel
    uint16_t cap;                      // ring capacity (= plot width), 0 = unset
    int16_t v_min;                     // waveform full-scale lower value
    int16_t v_max;                     // waveform full-scale upper value
    sgl_color_t bg_color;              // background color
    sgl_color_t grid_color;            // grid line color
    sgl_color_t border_color;          // border color
    uint8_t channel_count;             // number of channels (1-4)
    uint8_t border_width;              // border width in pixels
    uint8_t alpha;                     // alpha of waveform
} sgl_scope_t;

/**
 * @brief create scope object
 * @param parent parent object
 * @return scope object
 */
sgl_obj_t* sgl_scope_create(sgl_obj_t* parent);

/**
 * @brief bind the sample/color arrays to the scope
 * @param obj scope object
 * @param wave_buffers base of [wave_count][widget_width] sample array
 * @param wave_colors [wave_count] waveform color array
 * @param wave_count number of channels (1 - SGL_SCOPE_MAX_CHANNELS)
 * @note resets the FIFO of every channel
 */
void sgl_scope_set_buffers(sgl_obj_t *obj, int16_t *wave_buffers, sgl_color_t *wave_colors, uint8_t wave_count);

/**
 * @brief Append a new data point to the oscilloscope for a specific channel
 * @param obj The oscilloscope object
 * @param channel Channel number (0-based)
 * @param value The new data point
 * @note producer side of the FIFO: writes at `in` and advances it; when the
 *       ring is full the oldest sample is overwritten and `out` advances.
 *       Refresh is done with dirty rectangles instead of the whole-widget
 *       dirty flag: while the ring is still filling up only the vertical
 *       span of the newest column changes; once the ring is full every new
 *       sample shifts the whole waveform left, so the plot is split into
 *       SGL_SCOPE_SCROLL_STRIPS equal-width strips and each strip is marked
 *       only as tall as the y extent of the samples it covers (the border
 *       never changes)
 */
void sgl_scope_append_data(sgl_obj_t* obj, uint8_t channel, int16_t value);

/**
 * @brief clear all samples of a channel (or all channels when channel == 0xFF)
 * @param obj scope object
 * @param channel channel number (0-based), 0xFF clears all
 */
void sgl_scope_clear(sgl_obj_t *obj, uint8_t channel);

/**
 * @brief set the value range mapped to the widget height
 * @param obj scope object
 * @param v_min value mapped to the bottom edge
 * @param v_max value mapped to the top edge
 */
void sgl_scope_set_vrange(sgl_obj_t *obj, int16_t v_min, int16_t v_max);

/**
 * @brief set scope waveform color for a specific channel
 * @param obj scope object
 * @param channel channel number (0-based)
 * @param color waveform color
 * @return none
 */
void sgl_scope_set_channel_waveform_color(sgl_obj_t* obj, uint8_t channel, sgl_color_t color);

/**
 * @brief set scope background color
 * @param obj scope object
 * @param color background color
 * @return none
 */
static inline void sgl_scope_set_bg_color(sgl_obj_t* obj, sgl_color_t color)
{
    sgl_scope_t *scope = sgl_container_of(obj, sgl_scope_t, obj);
    scope->bg_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set scope grid line color
 * @param obj scope object
 * @param color grid line color
 * @return none
 */
static inline void sgl_scope_set_grid_color(sgl_obj_t* obj, sgl_color_t color)
{
    sgl_scope_t *scope = sgl_container_of(obj, sgl_scope_t, obj);
    scope->grid_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set scope alpha
 * @param obj scope object
 * @param alpha alpha
 * @return none
 */
static inline void sgl_scope_set_alpha(sgl_obj_t* obj, uint8_t alpha)
{
    sgl_scope_t *scope = sgl_container_of(obj, sgl_scope_t, obj);
    scope->alpha = alpha;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set scope border color
 * @param obj scope object
 * @param color border color
 * @return none
 */
static inline void sgl_scope_set_border_color(sgl_obj_t* obj, sgl_color_t color)
{
    sgl_scope_t *scope = sgl_container_of(obj, sgl_scope_t, obj);
    scope->border_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set scope border width
 * @param obj scope object
 * @param width border width
 * @return none
 */
static inline void sgl_scope_set_border_width(sgl_obj_t* obj, uint8_t width)
{
    sgl_scope_t *scope = sgl_container_of(obj, sgl_scope_t, obj);
    scope->border_width = width;
    sgl_obj_set_border_width(obj, width);
}

#ifdef __cplusplus
}
#endif

#endif // SGL_SCOPE_H
