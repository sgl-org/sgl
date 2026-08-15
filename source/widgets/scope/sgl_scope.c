/* source/widgets/sgl_scope.c
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

#include <stdio.h>
#include <limits.h>
#include <sgl_theme.h>
#include "sgl_scope.h"


// Draw a dashed line using Bresenham's algorithm with dash pattern
static void draw_dashed_line(sgl_surf_t *surf, sgl_area_t *area, int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t gap, sgl_color_t color)
{
    int16_t dx = sgl_abs(x1 - x0);
    int16_t dy = sgl_abs(y1 - y0);
    int16_t sx = (x0 < x1) ? 1 : -1;
    int16_t sy = (y0 < y1) ? 1 : -1;
    int16_t err = dx - dy;
    int16_t e2;
    int16_t dash_len = 0;

    sgl_area_t clip_area = {
        .x1 = surf->x1,
        .y1 = surf->y1,
        .x2 = surf->x2,
        .y2 = surf->y2
    };

    sgl_area_selfclip(&clip_area, area);

    while (1) {
        // Draw dash segment
        if (dash_len < gap) {
            // Check if point is within clipping area
            if (x0 >= clip_area.x1 && x0 <= clip_area.x2 && y0 >= clip_area.y1 && y0 <= clip_area.y2) {
                sgl_color_t *buf = sgl_surf_get_buf(surf, x0 - surf->x1, y0 - surf->y1);
                *buf = color;
            }
            dash_len++;
        } else if (dash_len < 2 * gap) {
            // Skip drawing (gap segment)
            dash_len++;
        } else {
            // Reset dash counter
            dash_len = 0;
        }
        
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
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

static void scope_plot_point(sgl_surf_t *surf, sgl_area_t *area, int16_t x, int16_t y, int16_t width, sgl_color_t color)
{
    if (width <= 0) width = 1;

    // Clamp the vertical extent of the point to the plot area AND to the
    // surface's current slice.  The framebuffer is drawn in horizontal
    // slices (surf->y1..surf->y2), so we must never write outside the
    // current slice's buffer region or we overflow the buffer.
    int16_t y0 = y - width / 2;
    int16_t y1 = y0 + width - 1;
    if (y0 < area->y1) y0 = area->y1;
    if (y1 > area->y2) y1 = area->y2;
    if (y0 < surf->y1) y0 = surf->y1;
    if (y1 > surf->y2) y1 = surf->y2;

    if (x < area->x1 || x > area->x2 || x < surf->x1 || x > surf->x2 || y0 > y1) return;

    sgl_color_t *buf = sgl_surf_get_buf(surf, x - surf->x1, y0 - surf->y1);
    for (int16_t yy = y0; yy <= y1; yy++) {
        *buf = color;
        buf += surf->w;  // advance to next row (surf->w is the row stride)
    }
}

static void scope_construct_cb(sgl_surf_t *surf, sgl_obj_t* obj, sgl_event_t *evt)
{
    sgl_scope_t *scope = (sgl_scope_t*)obj;

    if(evt->type == SGL_EVENT_DRAW_MAIN) {
        // Skip drawing if object is completely outside screen bounds
        if (obj->area.x2 < surf->x1 || obj->area.x1 > surf->x2 ||
            obj->area.y2 < surf->y1 || obj->area.y1 > surf->y2) {
            return; // Object is fully off-screen; no need to draw
        }

        // Draw background
        sgl_draw_rect_t bg_rect = {
            .color = scope->bg_color,
            .alpha = scope->alpha,
            .radius = 0,
            .border = scope->border_width,
        };

        sgl_draw_rect(surf, &obj->area, &obj->coords, &bg_rect);

        sgl_area_t plot_area = obj->coords;
        if (scope->border_width > 0) {
            plot_area.x1 += scope->border_width;
            plot_area.y1 += scope->border_width;
            plot_area.x2 -= scope->border_width;
            plot_area.y2 -= scope->border_width;
        }
        if (plot_area.x1 > plot_area.x2 || plot_area.y1 > plot_area.y2) {
            return;
        }

        // Compute waveform display parameters
        int16_t display_min = scope->min_value;
        int16_t display_max = scope->max_value;
        int16_t actual_min = display_min;  // Actual min/max of waveform data (for labels)
        int16_t actual_max = display_max;  // Actual max of waveform data (for labels)
        
        // If auto-scaling is enabled, recalculate min/max from current buffer data
        if (scope->auto_scale) {
            // Recalculate running_min and running_max based on current data in all channels
            if (scope->display_counts[0] > 0) {
                display_min = scope->data_buffers[0][0];
                display_max = scope->data_buffers[0][0];
                
                // Iterate through all channels and all data points to find min/max
                for (uint8_t ch = 0; ch < scope->channel_count; ch++) {
                    uint32_t end_index = (scope->display_counts[ch] < scope->data_len) ? scope->display_counts[ch] : scope->data_len;
                    for (uint32_t i = 0; i < end_index; i++) {
                        int16_t val = scope->data_buffers[ch][i];
                        if (val < display_min) display_min = val;
                        if (val > display_max) display_max = val;
                    }
                }
                
                // Save actual data min/max for label display
                actual_min = display_min;
                actual_max = display_max;
                
                // Update running values for next frame
                scope->running_min = display_min;
                scope->running_max = display_max;
            }
            
            // Add margin to prevent waveform from touching borders
            int32_t margin = (int32_t)(display_max - display_min) / 10;
            if (margin == 0) margin = 1;
            
            display_min = (display_min > INT16_MIN + margin) ? display_min - margin : INT16_MIN;
            display_max = (display_max < INT16_MAX - margin) ? display_max + margin : INT16_MAX;
        }
        
        // Avoid division by zero if min equals max
        if (display_min == display_max) {
            if (display_max < INT16_MAX) {
                display_max++;
            } else {
                display_min--;
            }
        }
        
        // Draw grid lines
        int16_t width = plot_area.x2 - plot_area.x1;
        int16_t height = plot_area.y2 - plot_area.y1;
        int16_t x_center = (plot_area.x1 + plot_area.x2) / 2;
        int16_t y_center = plot_area.y1 + (int32_t)(height * (display_max - (display_min + display_max) / 2)) / (display_max - display_min);
        
        // Draw horizontal center line (midpoint of display range)
        if (scope->grid_style) {
            // Draw dashed line
            draw_dashed_line(surf, &plot_area, plot_area.x1, y_center, plot_area.x2, y_center, scope->grid_style, scope->grid_color);
        } else {
            // Draw solid line
            sgl_draw_fill_hline(surf, &plot_area, y_center, plot_area.x1, plot_area.x2, 1, scope->grid_color, scope->alpha);
        }

        // Draw vertical center line
        if (scope->grid_style) {
            // Draw dashed line
            draw_dashed_line(surf, &plot_area, x_center, plot_area.y1, x_center, plot_area.y2, scope->grid_style, scope->grid_color);
        } else {
            // Draw solid line
            sgl_draw_fill_vline(surf, &plot_area, x_center, plot_area.y1, plot_area.y2, 1, scope->grid_color, scope->alpha);
        }

        // Draw vertical grid lines (10 divisions)
        for (int i = 1; i < 10; i++) {
            int16_t x_pos = plot_area.x1 + (width * i / 10);

            if (scope->grid_style) {
                // Draw dashed line
                draw_dashed_line(surf, &plot_area, x_pos, plot_area.y1, x_pos, plot_area.y2, scope->grid_style, scope->grid_color);
            } else {
                // Draw solid line
                sgl_draw_fill_vline(surf, &plot_area, x_pos, plot_area.y1, plot_area.y2, 1, scope->grid_color, scope->alpha);
            }
        }
        
        // Draw horizontal grid lines (10 divisions)
        for (int i = 1; i < 10; i++) {
            int16_t y_pos = plot_area.y1 + (height * i / 10); 
            if (scope->grid_style) {
                // Draw dashed line
                draw_dashed_line(surf, &plot_area, plot_area.x1, y_pos, plot_area.x2, y_pos, scope->grid_style, scope->grid_color);
            } else {
                // Draw solid line
                sgl_draw_fill_hline(surf, &plot_area, y_pos, plot_area.x1, plot_area.x2, 1, scope->grid_color, scope->alpha);
            }
        }

        int16_t plot_width = plot_area.x2 - plot_area.x1;

        for (uint8_t ch = 0; ch < scope->channel_count; ch++) {
            if (scope->display_counts[ch] == 0) continue;

            // Determine number of points to display
            uint32_t display_points = scope->max_display_points > 0 ? scope->max_display_points : scope->data_len;
            if (display_points > scope->data_len) display_points = scope->data_len;

            // Number of actual data points to render
            uint32_t data_points = scope->display_counts[ch] < display_points ? scope->display_counts[ch] : display_points;

            // Cap the number of points to the plot width so each point maps to
            // exactly one x column (waveform width == data point count).
            if (data_points > (uint32_t)(plot_width + 1)) {
                data_points = (uint32_t)(plot_width + 1);
            }

            // Draw waveform from right (most recent) to left (oldest).
            // Point i=0 is the most recent sample at the rightmost column.
            for (uint32_t i = 0; i < data_points; i++) {
                uint32_t idx = (scope->current_indices[ch] >= i + 1)
                             ? scope->current_indices[ch] - (i + 1)
                             : scope->data_len - (i + 1 - scope->current_indices[ch]);

                int16_t value = scope->data_buffers[ch][idx];

                // Clamp value to display range
                value = sgl_clamp(value, display_min, display_max);

                int16_t x = plot_area.x2 - (int16_t)i;  // one column per point
                int16_t y = plot_area.y2 - ((int32_t)(value - display_min) * height) / (display_max - display_min);

                scope_plot_point(surf, &plot_area, x, y, scope->line_width, scope->waveform_colors[ch]);
            }
        }

        // Draw Y-axis labels if enabled and font is set
        if (scope->show_y_labels && scope->y_label_font) {
            char label_text[16];
            sgl_area_t text_area = {
                .x1 = obj->coords.x1 + 2,
                .y1 = obj->coords.y1,
                .x2 = obj->coords.x1 + 50,
                .y2 = obj->coords.y2
            };

            sgl_area_selfclip(&text_area, &obj->area);
            
            // Display actual maximum value of waveform data
            sprintf(label_text, "%d", actual_max);
            sgl_draw_string(surf, &text_area, obj->coords.x1 + 2, obj->coords.y1 + 2, 
                        label_text, scope->y_label_color, scope->alpha, scope->y_label_font);
            
            // Display actual minimum value of waveform data
            sprintf(label_text, "%d", actual_min);
            sgl_draw_string(surf, &text_area, obj->coords.x1 + 2, obj->coords.y2 - scope->y_label_font->font_height - 2, 
                        label_text, scope->y_label_color, scope->alpha, scope->y_label_font);
            
            // Display mid-range value of actual waveform
            int16_t mid_value = (actual_max + actual_min) / 2;
            sprintf(label_text, "%d", mid_value);
            sprintf(label_text, "%u", mid_value);
            sgl_draw_string(surf, &text_area, obj->coords.x1 + 2, y_center - scope->y_label_font->font_height/2, 
                        label_text, scope->y_label_color, scope->alpha, scope->y_label_font);
        }
    }
}

/**
 * @brief create scope object
 * @param parent parent object
 * @return scope object
 */
sgl_obj_t* sgl_scope_create(sgl_obj_t* parent)
{
    sgl_scope_t *scope = sgl_malloc(sizeof(sgl_scope_t));
    if(scope == NULL) {
        return NULL;
    }
    
    memset(scope, 0, sizeof(sgl_scope_t));
    
    sgl_obj_t *obj = &scope->obj;
    sgl_obj_init(obj, parent);
    obj->construct_fn = scope_construct_cb;
    sgl_obj_set_border_width(obj, SGL_THEME_BORDER_WIDTH);
    
    // Initialize default parameters for single channel
    scope->channel_count = 1;        // Default to 1 channel
    scope->data_buffers = NULL;       // Will be allocated per channel
    scope->waveform_colors = NULL;    // Will be allocated per channel
    scope->current_indices = NULL;    // Will be allocated per channel
    scope->display_counts = NULL;     // Will be allocated per channel
    
    // Allocate default channel arrays
    scope->data_buffers = sgl_malloc(sizeof(int16_t*) * scope->channel_count);
    scope->waveform_colors = sgl_malloc(sizeof(sgl_color_t) * scope->channel_count);
    scope->current_indices = sgl_malloc(sizeof(uint32_t) * scope->channel_count);
    scope->display_counts = sgl_malloc(sizeof(uint16_t) * scope->channel_count);
    
    if (!scope->data_buffers || !scope->waveform_colors || !scope->current_indices || !scope->display_counts) {
        sgl_free(scope->data_buffers);
        sgl_free(scope->waveform_colors);
        sgl_free(scope->current_indices);
        sgl_free(scope->display_counts);
        sgl_free(scope);
        return NULL;
    }
    
    scope->data_buffers[0] = NULL;    // User must set via sgl_scope_set_channel_data_buffer
    scope->waveform_colors[0] = sgl_rgb(0, 255, 0);   // Green waveform for channel 0
    scope->current_indices[0] = 0;
    scope->display_counts[0] = 0;
    
    scope->bg_color = sgl_rgb(0, 0, 0);           // Black background
    scope->grid_color = sgl_rgb(50, 50, 50);      // Gray grid lines
    scope->border_width = 0;                      // border width is 0
    scope->border_color = sgl_rgb(150, 150, 150); // Light gray outer border
    scope->min_value = 0;
    scope->max_value = 0xFFFF;
    scope->running_min = INT16_MAX;   // Initialize runtime minimum to max int16_t value
    scope->running_max = INT16_MIN;   // Initialize runtime maximum to min int16_t value
    scope->auto_scale = 1;        // Enable auto-scaling by default
    scope->line_width = 2;        // Default line thickness
    scope->max_display_points = 0; // Display all points by default
    scope->show_y_labels = 0;      // Hide Y-axis labels by default
    scope->alpha = SGL_ALPHA_MAX;  // Fully opaque by default
    scope->grid_style = 0;         // Solid grid lines by default
    scope->y_label_font = NULL;    // No font by default
    scope->y_label_color = sgl_rgb(255, 255, 255); // White label color
    scope->data_len = 0;            // No data buffer initially

    return obj;
}

/* scan a contiguous buffer slice for the clamped value envelope; the slice is
 * indexed directly by buffer index, no per-sample remapping is needed */
static inline void scope_scan_envelope(const int16_t *buf, int32_t lo, int32_t hi,
                                       int16_t disp_min, int16_t disp_max,
                                       int16_t *vmin, int16_t *vmax)
{
    for (int32_t idx = lo; idx <= hi; idx++) {
        int16_t v = sgl_clamp(buf[idx], disp_min, disp_max);
        if (v < *vmin) *vmin = v;
        if (v > *vmax) *vmax = v;
    }
}

/**
 * @brief mark tight dirty areas after one sample is appended
 * @param scope scope object
 * @param plot plot area (widget coords inset by the border)
 * @param channel channel that received the sample
 * @param value the appended sample
 * @param outgoing sample that was overwritten in the ring buffer
 * @param was_full whether the ring buffer was already full before the append
 * @note point i is drawn at column x2 - i, so each band's columns map to a
 *       contiguous newest-first index range; scanning those samples yields the
 *       band's vertical envelope and thus its dirty rectangle.
 */
static void scope_append_area(sgl_scope_t *scope, sgl_area_t *plot, uint8_t channel,
                                    int16_t value, int16_t outgoing, uint8_t was_full)
{
    int16_t plot_w = plot->x2 - plot->x1;
    int16_t plot_h = plot->y2 - plot->y1;
    int16_t disp_min = scope->min_value;
    int16_t disp_max = scope->max_value;
    int32_t disp_range;
    int32_t cols = (int32_t)plot_w + 1;

    if (scope->auto_scale) {
        if (scope->running_min > scope->running_max ||
            value < scope->running_min || value > scope->running_max ||
            (was_full && (outgoing == scope->running_min || outgoing == scope->running_max))) {
            sgl_update_area(plot);
            return;
        }
        /* stable range: same margin expansion as scope_construct_cb() */
        disp_min = scope->running_min;
        disp_max = scope->running_max;
        int32_t margin = (int32_t)(disp_max - disp_min) / 10;
        if (margin == 0) margin = 1;
        disp_min = (disp_min > INT16_MIN + margin) ? disp_min - margin : INT16_MIN;
        disp_max = (disp_max < INT16_MAX - margin) ? disp_max + margin : INT16_MAX;
    }

    if (disp_min == disp_max) {
        if (disp_max < INT16_MAX) {
            disp_max++;
        } else {
            disp_min--;
        }
    }
    disp_range = (int32_t)disp_max - disp_min;

    /* number of points actually shown, same caps as scope_construct_cb() */
    uint32_t display_points = scope->max_display_points > 0 ? scope->max_display_points : scope->data_len;
    if (display_points > scope->data_len) display_points = scope->data_len;
    uint32_t dpts = scope->display_counts[channel] < display_points ? scope->display_counts[channel] : display_points;
    if (dpts > (uint32_t)(plot_w + 1)) {
        dpts = (uint32_t)(plot_w + 1);
    }
    if (dpts == 0) {
        return;
    }

    /* highest newest-first index worth scanning: the displayed points plus at
     * most one sample pushed off the left display edge that still exists in
     * the ring buffer */
    uint32_t scan_max = dpts;
    if (scan_max > (uint32_t)scope->display_counts[channel] - 1) {
        scan_max = (uint32_t)scope->display_counts[channel] - 1;
    }

    /* if the ring is shorter than the plot, the overwritten sample was still
     * on screen; remember it so its former pixels are covered */
    int32_t extra_band = -1;
    int16_t extra_value = 0;
    if (was_full && scope->data_len <= (uint32_t)(plot_w + 1)) {
        int32_t off = plot_w - ((int32_t)scope->data_len - 1);
        extra_band = off * SGL_SCOPE_DIRTY_BAND_NUM / cols;
        extra_value = sgl_clamp(outgoing, disp_min, disp_max);
    }

    /* newest-first index i lives in ring slot cur-1-i, so each band maps to
     * one contiguous buffer slice (wrapping at most once) whose head/tail
     * indices are computed once; the samples are then fetched directly by
     * buffer index */
    const int16_t *buf = scope->data_buffers[channel];
    int32_t ring_len = (int32_t)scope->data_len;
    int32_t cur = (int32_t)scope->current_indices[channel];

    for (uint8_t k = 0; k < SGL_SCOPE_DIRTY_BAND_NUM; k++) {
        /* band k maps to newest-first indices [plot_w - off_hi, plot_w - off_lo];
         * extend by one index to also cover the pixels before the 1-column
         * left shift */
        int32_t i_lo = plot_w - (SGL_SCOPE_DIRTY_BAND_HI(cols, k) - 1);
        int32_t i_hi = plot_w - SGL_SCOPE_DIRTY_BAND_LO(cols, k) + 1;
        if (i_lo < 0) i_lo = 0;
        if (i_hi > (int32_t)scan_max) i_hi = (int32_t)scan_max;
        if (i_lo > i_hi) {
            continue;
        }

        int32_t newest = cur - 1 - i_lo;
        int32_t oldest = cur - 1 - i_hi;
        if (newest < 0) newest += ring_len;
        if (oldest < 0) oldest += ring_len;

        int16_t vmin = INT16_MAX, vmax = INT16_MIN;
        if (oldest <= newest) {
            scope_scan_envelope(buf, oldest, newest, disp_min, disp_max, &vmin, &vmax);
        } else {
            /* the slice wraps around the end of the ring buffer */
            scope_scan_envelope(buf, oldest, ring_len - 1, disp_min, disp_max, &vmin, &vmax);
            scope_scan_envelope(buf, 0, newest, disp_min, disp_max, &vmin, &vmax);
        }
        if ((int32_t)k == extra_band) {
            if (extra_value < vmin) vmin = extra_value;
            if (extra_value > vmax) vmax = extra_value;
        }

        SGL_SCOPE_PUSH_DIRTY_BAND(scope, plot, plot_h, vmin, vmax, disp_min, disp_range, k);
    }
}

/**
 * @brief Append a new data point to the oscilloscope for a specific channel
 * @param obj The oscilloscope object
 * @param channel Channel number (0-based)
 * @param value The new data point
 * @note This function appends a new data point to the specified channel of the oscilloscope. 
 *       If the oscilloscope is configured to auto-scale, the function updates the running minimum and maximum values. 
 *       The function also updates the display count and marks the oscilloscope object as dirty.
 */
void sgl_scope_append_data(sgl_obj_t* obj, uint8_t channel, int16_t value)
{
    sgl_scope_t *scope = (sgl_scope_t*)obj;
    
    if (channel >= scope->channel_count || !scope->data_buffers[channel]) {
        return;
    }

    /* remember the sample about to be overwritten: it may still bound the
     * auto-scale range or own on-screen pixels (see scope_append_area) */
    int16_t outgoing = scope->data_buffers[channel][scope->current_indices[channel]];
    uint8_t was_full = (scope->display_counts[channel] >= scope->data_len);

    // Simply append the data point to the buffer
    scope->data_buffers[channel][scope->current_indices[channel]] = value;

    if (sgl_is_pow2(scope->data_len)) {
        scope->current_indices[channel] = (scope->current_indices[channel] + 1) & (scope->data_len - 1);
    } else {
        scope->current_indices[channel] = (scope->current_indices[channel] + 1) % scope->data_len;
    }

    // update display count for this channel
    if (scope->display_counts[channel] < scope->data_len) {
        scope->display_counts[channel]++;
    }

    sgl_area_t plot_area = obj->coords;
    if (scope->border_width > 0) {
        plot_area.x1 += scope->border_width;
        plot_area.y1 += scope->border_width;
        plot_area.x2 -= scope->border_width;
        plot_area.y2 -= scope->border_width;
    }
    if (plot_area.x1 <= plot_area.x2 && plot_area.y1 <= plot_area.y2) {
        scope_append_area(scope, &plot_area, channel, value, outgoing, was_full);
    }
}

/**
 * @brief set scope channel count
 * @param obj scope object
 * @param channel_count number of channels (1-4)
 * @return none
 */
void sgl_scope_set_channel_count(sgl_obj_t* obj, uint8_t channel_count)
{
    sgl_scope_t *scope = sgl_container_of(obj, sgl_scope_t, obj);
    
    if (channel_count == 0 || channel_count > 4) {
        SGL_LOG_ERROR("Invalid channel count: %d (must be 1-4)\n", channel_count);
        return;
    }
    
    // Free old arrays
    if (scope->data_buffers) {
        for (uint8_t i = 0; i < scope->channel_count; i++) {
            if (scope->data_buffers[i]) {
                sgl_free(scope->data_buffers[i]);
            }
        }
        sgl_free(scope->data_buffers);
    }
    if (scope->waveform_colors) sgl_free(scope->waveform_colors);
    if (scope->current_indices) sgl_free(scope->current_indices);
    if (scope->display_counts) sgl_free(scope->display_counts);
    
    // Update channel count
    scope->channel_count = channel_count;
    
    // Allocate new arrays
    scope->data_buffers = sgl_malloc(sizeof(int16_t*) * channel_count);
    scope->waveform_colors = sgl_malloc(sizeof(sgl_color_t) * channel_count);
    scope->current_indices = sgl_malloc(sizeof(uint32_t) * channel_count);
    scope->display_counts = sgl_malloc(sizeof(uint16_t) * channel_count);
    
    if (!scope->data_buffers || !scope->waveform_colors || !scope->current_indices || !scope->display_counts) {
        SGL_LOG_ERROR("Failed to allocate memory for channel arrays\n");
        return;
    }
    
    // Initialize channel data
    for (uint8_t i = 0; i < channel_count; i++) {
        scope->data_buffers[i] = NULL;  // User must set via sgl_scope_set_channel_data_buffer
        scope->current_indices[i] = 0;
        scope->display_counts[i] = 0;
        
        // Set default colors for each channel
        switch (i) {
            case 0:
                scope->waveform_colors[i] = sgl_rgb(0, 255, 0);   // Green
                break;
            case 1:
                scope->waveform_colors[i] = sgl_rgb(255, 0, 0);   // Red
                break;
            case 2:
                scope->waveform_colors[i] = sgl_rgb(0, 0, 255);   // Blue
                break;
            case 3:
                scope->waveform_colors[i] = sgl_rgb(255, 255, 0); // Yellow
                break;
            default:
                scope->waveform_colors[i] = sgl_rgb(255, 255, 255); // White
                break;
        }
    }
    
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set scope data buffer for a specific channel
 * @param obj scope object
 * @param channel channel number (0-based)
 * @param data_buffer data buffer
 * @param data_len data length
 * @return none
 */
void sgl_scope_set_channel_data_buffer(sgl_obj_t* obj, uint8_t channel, int16_t *data_buffer, uint32_t data_len)
{
    sgl_scope_t *scope = sgl_container_of(obj, sgl_scope_t, obj);
    
    if (channel >= scope->channel_count) {
        SGL_LOG_ERROR("Invalid channel: %d (max %d)\n", channel, scope->channel_count - 1);
        return;
    }
    
    scope->data_buffers[channel] = data_buffer;
    scope->data_len = data_len;
    scope->current_indices[channel] = 0;
    scope->display_counts[channel] = 0;
    
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set scope waveform color for a specific channel
 * @param obj scope object
 * @param channel channel number (0-based)
 * @param color waveform color
 * @return none
 */
void sgl_scope_set_channel_waveform_color(sgl_obj_t* obj, uint8_t channel, sgl_color_t color)
{
    sgl_scope_t *scope = sgl_container_of(obj, sgl_scope_t, obj);
    
    if (channel >= scope->channel_count) {
        SGL_LOG_ERROR("Invalid channel: %d (max %d)\n", channel, scope->channel_count - 1);
        return;
    }
    
    scope->waveform_colors[channel] = color;
    sgl_obj_set_dirty(obj);
}
