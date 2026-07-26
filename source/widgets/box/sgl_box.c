/* source/widgets/sgl_box.c
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
#include <sgl_cfgfix.h>
#include <sgl_theme.h>
#include "sgl_box.h"


#define  SGL_BOX_SCROLL_WIDTH                  (4)

static void sgl_box_construct_cb(sgl_surf_t *surf, sgl_obj_t* obj, sgl_event_t *evt)
{
    sgl_box_t *box = sgl_container_of(obj, sgl_box_t, obj);
    int16_t height = obj->coords.y2 - obj->coords.y1 - 2 * box->bg.radius;
    int16_t width = obj->coords.x2 - obj->coords.x1 - 2 * box->bg.radius;
    int16_t scroll_height = sgl_max(height / 8, SGL_BOX_SCROLL_WIDTH);
    int16_t scroll_width = sgl_max(width / 8, SGL_BOX_SCROLL_WIDTH);
    sgl_area_t area;

    if(evt->type == SGL_EVENT_DRAW_MAIN) {
        area.x1 = obj->coords.x1 + box->bg.radius;
        area.y1 = obj->coords.y1 + box->bg.radius;
        area.x2 = obj->coords.x2 - box->bg.radius;
        area.y2 = obj->coords.y2 - box->bg.radius;

        box->bg.border_mask = obj->focus;
        sgl_draw_rect(surf, &obj->area, &obj->coords, &box->bg);

        // Draw scrollbars if enabled
        if(box->scroll_enable) {
            // Draw vertical scrollbar if vertical scrolling is enabled and showing is enabled
            if (((box->scroll_mode & SGL_BOX_SCROLL_VERTICAL_ONLY) || (box->scroll_mode & SGL_BOX_SCROLL_BOTH))
                && box->show_v_scrollbar) {
                area.x1 = obj->coords.x2 - SGL_BOX_SCROLL_WIDTH - box->bg.radius;
                area.y1 = obj->coords.y1 + box->bg.radius;
                area.x2 = obj->coords.x2 - box->bg.radius;
                
                // Calculate scroll bar position based on content size and current offset
                // Use elastic_scroll values to determine content size for proper scrollbar positioning
                int16_t content_height = height + (box->elastic_scroll_up + box->elastic_scroll_down);
                
                if(content_height > height && content_height > 0) {
                    // Calculate the scroll bar's position based on the current y_offset
                    float scroll_ratio = (float)(-box->y_offset) / (content_height - height);
                    scroll_ratio = sgl_clamp(scroll_ratio, 0.0f, 1.0f); // Clamp between 0 and 1
                    
                    int16_t max_scroll_pos = height - scroll_height;
                    area.y1 = obj->coords.y1 + box->bg.radius + (int16_t)(max_scroll_pos * scroll_ratio);
                    area.y2 = area.y1 + scroll_height;
                } else {
                    // If content fits, position scrollbar at the top
                    area.y1 = obj->coords.y1 + box->bg.radius;
                    area.y2 = area.y1 + scroll_height;
                }

                sgl_draw_fill_rect(surf, &obj->area, &area, SGL_BOX_SCROLL_WIDTH / 2, box->scroll_color, 128);
            }

            // Draw horizontal scrollbar if horizontal scrolling is enabled and showing is enabled
            if (((box->scroll_mode & SGL_BOX_SCROLL_HORIZONTAL_ONLY) || (box->scroll_mode & SGL_BOX_SCROLL_BOTH))
                && box->show_h_scrollbar) {
                // Position horizontal scrollbar near the bottom-left, closer to vertical scrollbar
                area.x1 = obj->coords.x1 + box->bg.radius;
                area.y1 = obj->coords.y2 - SGL_BOX_SCROLL_WIDTH - box->bg.radius;
                area.y2 = obj->coords.y2 - box->bg.radius;
                
                // Adjust the right edge to not overlap with vertical scrollbar if both are visible
                if ((box->scroll_mode & SGL_BOX_SCROLL_VERTICAL_ONLY) || (box->scroll_mode & SGL_BOX_SCROLL_BOTH)) {
                    area.x2 = obj->coords.x2 - SGL_BOX_SCROLL_WIDTH - box->bg.radius;
                } else {
                    area.x2 = obj->coords.x2 - box->bg.radius;
                }
                
                // Calculate scroll bar position based on content size and current offset
                int16_t content_width = width + (box->elastic_scroll_left + box->elastic_scroll_right);
                
                if(content_width > width && content_width > 0) {
                    // Calculate the scroll bar's position based on the current x_offset
                    float scroll_ratio = (float)(-box->x_offset) / (content_width - width);
                    scroll_ratio = sgl_clamp(scroll_ratio, 0.0f, 1.0f); // Clamp between 0 and 1
                    
                    int16_t max_scroll_pos = (area.x2 - area.x1) - scroll_width;
                    area.x1 = area.x1 + (int16_t)(max_scroll_pos * scroll_ratio);
                    area.x2 = area.x1 + scroll_width;
                } else {
                    // If content fits, position scrollbar at the left
                    area.x1 = obj->coords.x1 + box->bg.radius;
                    area.x2 = area.x1 + scroll_width;
                }

                sgl_draw_fill_rect(surf, &obj->area, &area, SGL_BOX_SCROLL_WIDTH / 2, box->scroll_color, 128);
            }
        }
    }
    else if(evt->type == SGL_EVENT_MOVE_UP || evt->type == SGL_EVENT_MOVE_DOWN) {
        // Check if vertical scrolling is enabled
        if((box->scroll_mode & SGL_BOX_SCROLL_VERTICAL_ONLY) || (box->scroll_mode & SGL_BOX_SCROLL_BOTH)) {
            // Update text offset - determine scroll direction based on event type
            int16_t distance = evt->distance;
            int16_t new_offset = box->y_offset + distance;
            
            // Calculate limits based on elastic_scroll values
            int16_t min_elastic, max_elastic;
            // Calculate content dimensions based on elastic_scroll values
            int16_t content_height = height + (box->elastic_scroll_up + box->elastic_scroll_down);
            // Determine maximum scroll distance
            int16_t max_scroll_distance = content_height - height;
            
            if(box->elastic_scroll_up > 0 || box->elastic_scroll_down > 0) {
                // If elastic scrolling is enabled (any direction limit > 0), limit to boundary plus elastic offset
                // Apply actual elastic limits
                min_elastic = -max_scroll_distance - box->elastic_scroll_down;  // Allow scrolling down with elastic limit
                max_elastic = box->elastic_scroll_up;                           // Allow scrolling up with elastic limit
            } else {
                // If elastic scrolling is disabled (both values are 0), limit to content boundaries
                min_elastic = -max_scroll_distance; // max scroll down
                max_elastic = 0;                    // max scroll up
            }

            int16_t constrained_new_offset = sgl_clamp(new_offset, min_elastic, max_elastic);
            int16_t offset_delta = constrained_new_offset - box->y_offset;

            box->y_offset = constrained_new_offset;

            // Move all children vertically
            sgl_obj_move_child_pos_y(obj, offset_delta);
        }
    }
    else if(evt->type == SGL_EVENT_MOVE_LEFT || evt->type == SGL_EVENT_MOVE_RIGHT) {
        // Check if horizontal scrolling is enabled
        if((box->scroll_mode & SGL_BOX_SCROLL_HORIZONTAL_ONLY) || (box->scroll_mode & SGL_BOX_SCROLL_BOTH)) {
            // Update text offset - determine scroll direction based on event type
            int16_t distance = evt->distance;
            int16_t new_offset = box->x_offset + distance;
            
            // Calculate limits based on elastic_scroll values
            int16_t min_elastic, max_elastic;
            // Calculate content dimensions based on elastic_scroll values
            int16_t content_width = width + (box->elastic_scroll_left + box->elastic_scroll_right);
            // Determine maximum scroll distance
            int16_t max_scroll_distance = content_width - width;
            
            if(box->elastic_scroll_left > 0 || box->elastic_scroll_right > 0) {
                // If elastic scrolling is enabled (any direction limit > 0), limit to boundary plus elastic offset
                // Apply actual elastic limits
                min_elastic = -max_scroll_distance - box->elastic_scroll_right;  // Allow scrolling right with elastic limit
                max_elastic = box->elastic_scroll_left;                          // Allow scrolling left with elastic limit
            } else {
                // If elastic scrolling is disabled (both values are 0), limit to content boundaries
                min_elastic = -max_scroll_distance; // max scroll right
                max_elastic = 0;                    // max scroll left
            }

            int16_t constrained_new_offset = sgl_clamp(new_offset, min_elastic, max_elastic);
            int16_t offset_delta = constrained_new_offset - box->x_offset;
            
            box->x_offset = constrained_new_offset;

            // Move all children horizontally
            sgl_obj_move_child_pos_x(obj, offset_delta);
        }
    }
    else if (evt->type == SGL_EVENT_PRESSED) {
        box->scroll_enable = 1;
        sgl_obj_set_dirty(obj);
    }
    else if (evt->type == SGL_EVENT_RELEASED) {
        box->scroll_enable = 0;
        sgl_obj_set_dirty(obj);
    }
}


/**
 * @brief create a box object
 * @param parent parent of the box
 * @return pointer to the box object
 */
sgl_obj_t* sgl_box_create(sgl_obj_t* parent)
{
    sgl_box_t *box = sgl_malloc(sizeof(sgl_box_t));
    if(box == NULL) {
        SGL_LOG_ERROR("sgl_box_create: malloc failed");
        return NULL;
    }

    /* set object all member to zero */
    memset(box, 0, sizeof(sgl_box_t));

    sgl_obj_t *obj = &box->obj;
    sgl_obj_init(&box->obj, parent);
    obj->construct_fn = sgl_box_construct_cb;
    sgl_obj_set_border_width(obj, SGL_THEME_BORDER_WIDTH);
    obj->focus = 1;

    sgl_obj_set_clickable(obj);
    sgl_obj_set_movable(obj);

    box->bg.alpha = SGL_THEME_ALPHA;
    box->bg.color = SGL_THEME_COLOR;
    box->bg.border_alpha = SGL_THEME_ALPHA;
    box->bg.radius = 0;
    box->bg.border = 1;
    sgl_obj_set_border_width(obj, 1);
    box->bg.border_color = SGL_THEME_BORDER_COLOR;
    box->scroll_color = SGL_THEME_SCROLL_FG_COLOR;

    box->x_offset = 0;
    box->y_offset = 0;
    box->scroll_enable = 0;
    box->show_v_scrollbar = 1;
    box->show_h_scrollbar = 1;
    box->scroll_mode = SGL_BOX_SCROLL_BOTH;
    box->elastic_scroll_up = 0;    // Default: unlimited scrolling up
    box->elastic_scroll_down = 0;  // Default: unlimited scrolling down
    box->elastic_scroll_left = 0;  // Default: unlimited scrolling left
    box->elastic_scroll_right = 0; // Default: unlimited scrolling right

    return obj;
}


/**
 * @brief set background color of the box
 * @param obj box object
 * @param color background color to be set
 * @return none
 */
void sgl_box_set_bg_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_box_t *box = sgl_container_of(obj, sgl_box_t, obj);
    box->bg.color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set radius of the box
 * @param obj box object
 * @param radius radius to be set
 * @return none
 */
void sgl_box_set_radius(sgl_obj_t *obj, uint8_t radius)
{
    sgl_box_t *box = sgl_container_of(obj, sgl_box_t, obj);
    sgl_obj_set_radius(obj, radius);
    box->bg.radius = obj->radius;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set border color of the box
 * @param obj box object
 * @param color border color to be set
 * @return none
 */
void sgl_box_set_border_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_box_t *box = sgl_container_of(obj, sgl_box_t, obj);
    box->bg.border_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set border width of the box
 * @param obj box object
 * @param width border width to be set
 * @return none
 */
void sgl_box_set_border_width(sgl_obj_t *obj, uint8_t width)
{
    sgl_box_t *box = sgl_container_of(obj, sgl_box_t, obj);
    box->bg.border = width;
    sgl_obj_set_border_width(obj, width);
}

/**
 * @brief set box pixmap
 * @param obj box object
 * @param pixmap pixmap to be set
 * @return none
 */
void sgl_box_set_pixmap(sgl_obj_t *obj, const sgl_pixmap_t *pixmap)
{
    sgl_box_t *box = sgl_container_of(obj, sgl_box_t, obj);
    box->bg.pixmap = pixmap;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set scrollbar color of the box
 * @param obj box object
 * @param color scrollbar color to be set
 * @return none
 */
void sgl_box_set_scrollbar_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_box_t *box = sgl_container_of(obj, sgl_box_t, obj);
    box->scroll_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set whether to show scrollbars
 * @param obj box object
 * @param show_vertical whether to show vertical scrollbar (1 to show, 0 to hide)
 * @param show_horizontal whether to show horizontal scrollbar (1 to show, 0 to hide)
 * @return none
 */
void sgl_box_set_show_scrollbar(sgl_obj_t *obj, uint8_t show_vertical, uint8_t show_horizontal)
{
    sgl_box_t *box = sgl_container_of(obj, sgl_box_t, obj);
    box->show_v_scrollbar = show_vertical;
    box->show_h_scrollbar = show_horizontal;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set alpha (transparency) of the box
 * @param obj box object
 * @param alpha alpha value to be set (0-255, where 0 is fully transparent and 255 is fully opaque)
 * @return none
 */
void sgl_box_set_alpha(sgl_obj_t *obj, uint8_t alpha)
{
    sgl_box_t *box = sgl_container_of(obj, sgl_box_t, obj);
    box->bg.alpha = alpha;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief Set the elastic scroll limits for up and down directions
 * @param obj box object
 * @param up_limit maximum pixels allowed when scrolling up (0 for unlimited)
 * @param down_limit maximum pixels allowed when scrolling down (0 for unlimited)
 * @param left_limit maximum pixels allowed when scrolling left (0 for unlimited)
 * @param right_limit maximum pixels allowed when scrolling right (0 for unlimited)
 * @return none
 */
void sgl_box_set_elastic_scroll(sgl_obj_t* obj, int16_t up_limit, int16_t down_limit, int16_t left_limit, int16_t right_limit)
{
    SGL_ASSERT(obj != NULL);
    sgl_box_t *box = (sgl_box_t*)obj;
    box->elastic_scroll_up = up_limit;
    box->elastic_scroll_down = down_limit;
    box->elastic_scroll_left = left_limit;
    box->elastic_scroll_right = right_limit;
}
