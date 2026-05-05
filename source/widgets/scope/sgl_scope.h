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
 
#define SGL_SCOPE_MAX_CHANNELS 4

typedef struct {
    sgl_obj_t obj;
    int16_t  *data_buffers[SGL_SCOPE_MAX_CHANNELS];   // per-channel user data buffers
    sgl_color_t waveform_colors[SGL_SCOPE_MAX_CHANNELS]; // waveform colors per channel
    uint32_t current_indices[SGL_SCOPE_MAX_CHANNELS];    // current write index per channel
    uint16_t display_counts[SGL_SCOPE_MAX_CHANNELS];     // display count per channel
    const sgl_font_t *y_label_font;    // font of Y axis labels
    uint32_t  data_len;                // data length per channel
    uint32_t  max_display_points;      // max display points
    sgl_color_t y_label_color;         // color of Y axis labels
    sgl_color_t bg_color;              // background color
    sgl_color_t grid_color;            // grid line color
    sgl_color_t border_color;          // border color
    int16_t min_value;                 // min value of data
    int16_t max_value;                 // max value of data
    int16_t running_min;               // min value of runtime
    int16_t running_max;               // max value of runtime
    uint8_t channel_count;             // number of channels (1-4)
    uint8_t auto_scale : 1;            // whether to automatically scale
    uint8_t show_y_labels : 1;         // whether to show Y axis labels
    uint8_t border_width;              // outer border width
    uint8_t line_width;                // width of waveform line
    uint8_t alpha;                     // aplha of waveform
    uint8_t grid_style;                // grid line style（0-solid line，other: dashed line
    uint8_t dirty_rect_count;          // local dirty rectangle count used by waveform updates
} sgl_scope_t;

/**
 * @brief create scope object
 * @param parent parent object
 * @return scope object
 */
sgl_obj_t* sgl_scope_create(sgl_obj_t* parent);

/**
 * @brief set scope channel count
 * @param obj scope object
 * @param channel_count number of channels (1-4)
 * @return none
 */
void sgl_scope_set_channel_count(sgl_obj_t* obj, uint8_t channel_count);

/**
 * @brief set scope data buffer for a specific channel
 * @param obj scope object
 * @param channel channel number (0-based)
 * @param data_buffer data buffer
 * @param data_len data length
 * @return none
 */
void sgl_scope_set_channel_data_buffer(sgl_obj_t* obj, uint8_t channel, int16_t *data_buffer, uint32_t data_len);

/**
 * @brief Append a new data point to the oscilloscope for a specific channel
 * @param obj The oscilloscope object
 * @param channel Channel number (0-based)
 * @param value The new data point
 * @note This function appends a new data point to the specified channel of the oscilloscope. 
 *       If the oscilloscope is configured to auto-scale, the function updates the running minimum and maximum values. 
 *       The function also updates the display count and marks the oscilloscope object as dirty.
 */
void sgl_scope_append_data(sgl_obj_t* obj, uint8_t channel, int16_t value);

/**
 * @brief get scope data for a specific channel
 * @param obj scope object
 * @param channel channel number (0-based)
 * @param index data index
 * @return data value
 */
int16_t sgl_scope_get_channel_data(sgl_obj_t* obj, uint8_t channel, uint32_t index);

/**
 * @brief set scope max display points
 * @param obj scope object
 * @param max_points max display points
 * @return none
 */
void sgl_scope_set_max_display_points(sgl_obj_t* obj, uint8_t max_points);

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
void sgl_scope_set_bg_color(sgl_obj_t* obj, sgl_color_t color);

/**
 * @brief set scope grid line color
 * @param obj scope object
 * @param color grid line color
 * @return none
 */
void sgl_scope_set_grid_color(sgl_obj_t* obj, sgl_color_t color);

/**
 * @brief set scope range
 * @param obj scope object
 * @param min_value minimum value
 * @param max_value maximum value
 * @return none
 */
void sgl_scope_set_range(sgl_obj_t* obj, uint16_t min_value, uint16_t max_value);

/**
 * @brief set scope line width
 * @param obj scope object
 * @param width line width
 * @return none
 */
void sgl_scope_set_line_width(sgl_obj_t* obj, uint8_t width);

/**
 * @brief enable/disable auto scale
 * @param obj scope object
 * @param enable enable/disable
 * @return none
 */
void sgl_scope_enable_auto_scale(sgl_obj_t* obj, bool enable);

/**
 * @brief set scope alpha
 * @param obj scope object
 * @param alpha alpha
 * @return none
 */
void sgl_scope_set_alpha(sgl_obj_t* obj, uint8_t alpha);

/**
 * @brief show/hide Y axis labels
 * @param obj scope object
 * @param show show/hide
 * @return none
 */
void sgl_scope_show_y_labels(sgl_obj_t* obj, bool show);

/**
 * @brief set scope Y axis labels font
 * @param obj scope object
 * @param font font
 * @return none
 */
void sgl_scope_set_y_label_font(sgl_obj_t* obj, const sgl_font_t *font);

/**
 * @brief set scope Y axis labels color
 * @param obj scope object
 * @param color color
 * @return none
 */
void sgl_scope_set_y_label_color(sgl_obj_t* obj, sgl_color_t color);

/**
 * @brief set scope border color
 * @param obj scope object
 * @param color border color
 * @return none
 */
void sgl_scope_set_border_color(sgl_obj_t* obj, sgl_color_t color);

/**
 * @brief set scope border width
 * @param obj scope object
 * @param width border width
 * @return none
 */
void sgl_scope_set_border_width(sgl_obj_t* obj, uint8_t width);

/**
 * @brief set scope grid line
 * @param obj scope object
 * @param style grid size, 0: solid line，other: dashed line
 * @return none
 */
void sgl_scope_set_grid_line(sgl_obj_t* obj, uint8_t grid);

/**
 * @brief set scope local dirty rectangle count for waveform updates
 * @param obj scope object
 * @param count local dirty rectangle count, 1..SGL_DIRTY_AREA_NUM_MAX
 * @return none
 */
void sgl_scope_set_dirty_rect_count(sgl_obj_t* obj, uint8_t count);

#ifdef __cplusplus
}
#endif

#endif // SGL_SCOPE_H
