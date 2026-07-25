/* source/widgets/sgl_battery.c
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

#include <sgl_core.h>
#include <sgl_draw.h>
#include <sgl_math.h>
#include <sgl_log.h>
#include <sgl_mm.h>
#include <sgl_theme.h>
#include <sgl_cfgfix.h>
#include "sgl_battery.h"


/**
 * @brief construct function of battery object
 * @param surf pointer to surface
 * @param obj pointer to object
 * @param evt pointer to event
 * @return none
 */
static void sgl_battery_construct_cb(sgl_surf_t *surf, sgl_obj_t* obj, sgl_event_t *evt)
{
    sgl_battery_t *battery = sgl_container_of(obj, sgl_battery_t, obj);
    
    if(evt->type == SGL_EVENT_DRAW_MAIN) {
        int16_t x1 = obj->coords.x1;
        int16_t y1 = obj->coords.y1;
        int16_t x2 = obj->coords.x2;
        int16_t y2 = obj->coords.y2;
        
        int16_t width = x2 - x1;
        int16_t height = y2 - y1;
        
        int16_t battery_width, battery_height;
        int16_t battery_x, battery_y;
        int16_t cap_width, cap_height;
        int16_t cap_x, cap_y;
        
        if (battery->direction == SGL_BATTERY_DIR_HORIZONTAL) {
            battery_width = width - battery->cap_size;
            battery_height = height - (height / 5);
            
            cap_width = battery->cap_size;
            cap_height = battery_height / 3;
            
            if (battery->cap_pos == SGL_BATTERY_CAP_RIGHT) {
                battery_x = x1;
                battery_y = y1 + (height - battery_height) / 2;
                cap_x = battery_x + battery_width;
                cap_y = battery_y + (battery_height - cap_height) / 2;
            } else {
                battery_x = x1 + cap_width;
                battery_y = y1 + (height - battery_height) / 2;
                cap_x = x1;
                cap_y = battery_y + (battery_height - cap_height) / 2;
            }
        } else {
            battery_height = height - battery->cap_size;
            battery_width = width - (width / 5);
            
            cap_height = battery->cap_size;
            cap_width = battery_width / 3;
            
            battery_y = y1 + cap_height;
            battery_x = x1 + (width - battery_width) / 2;
            cap_y = y1;
            cap_x = battery_x + (battery_width - cap_width) / 2;
        }
        
        int16_t border_width = 2;
        int16_t padding = 2;
        int16_t fill_width = battery_width - 2 * border_width - 2 * padding;
        int16_t fill_height = battery_height - 2 * border_width - 2 * padding;
        int16_t fill_x = battery_x + border_width + padding;
        int16_t fill_y = battery_y + border_width + padding;
        
        uint8_t level = battery->level > 100 ? 100 : battery->level;
        sgl_color_t fill_color = battery->fill_color;
        sgl_color_t border_color = battery->border_color;
        
        if (level < 20) {
            fill_color = battery->low_color;
        } else if (level < 50) {
            fill_color = battery->medium_color;
        } else {
            fill_color = battery->high_color;
        }
        
        sgl_area_t battery_rect = {
            .x1 = battery_x,
            .y1 = battery_y,
            .x2 = battery_x + battery_width,
            .y2 = battery_y + battery_height
        };
        
        sgl_area_t cap_rect = {
            .x1 = cap_x,
            .y1 = cap_y,
            .x2 = cap_x + cap_width,
            .y2 = cap_y + cap_height
        };
        
        sgl_draw_fill_rect(surf, &obj->area, &battery_rect, 3, 
            border_color, SGL_ALPHA_MAX);
        
        sgl_draw_fill_rect(surf, &obj->area, &cap_rect, 0, 
            border_color, SGL_ALPHA_MAX);
        
        sgl_area_t bg_rect = {
            .x1 = battery_x + border_width,
            .y1 = battery_y + border_width,
            .x2 = battery_x + battery_width - border_width,
            .y2 = battery_y + battery_height - border_width
        };
        sgl_draw_fill_rect(surf, &obj->area, &bg_rect, 1, 
            battery->bg_color, SGL_ALPHA_MAX);
        
        if (level > 0) {
            int16_t active_cells = (level * battery->num_cells + 99) / 100;
            if (active_cells > battery->num_cells) active_cells = battery->num_cells;
            
            if (battery->direction == SGL_BATTERY_DIR_HORIZONTAL) {
                int16_t min_gap = 2;
                int16_t total_min_gap = (battery->num_cells - 1) * min_gap;
                
                if (total_min_gap >= fill_width) {
                    min_gap = 1;
                    total_min_gap = battery->num_cells - 1;
                }
                
                int16_t cell_width = (fill_width - total_min_gap) / battery->num_cells;
                if (cell_width < 1) cell_width = 1;
                
                int16_t used_width = cell_width * battery->num_cells + total_min_gap;
                int16_t remaining_width = fill_width - used_width;
                
                int16_t cell_height = fill_height;
                
                if (battery->cap_pos == SGL_BATTERY_CAP_RIGHT) {
                    int16_t pos_x = fill_x;
                    
                    for (int i = 0; i < active_cells; i++) {
                        int16_t cur_cell_width = cell_width;
                        if (i < remaining_width) {
                            cur_cell_width++;
                        }
                        
                        sgl_area_t cell_rect = {
                            .x1 = pos_x,
                            .y1 = fill_y,
                            .x2 = pos_x + cur_cell_width,
                            .y2 = fill_y + cell_height
                        };
                        
                        sgl_draw_fill_rect(surf, &obj->area, &cell_rect, 1, 
                        fill_color, SGL_ALPHA_MAX);
                        
                        if (i < battery->num_cells - 1) {
                            pos_x += cur_cell_width + min_gap;
                        }
                    }
                } else {
                    int16_t pos_x = fill_x + fill_width;
                    
                    for (int i = 0; i < active_cells; i++) {
                        int16_t cur_cell_width = cell_width;
                        if (i < remaining_width) {
                            cur_cell_width++;
                        }
                        
                        pos_x -= cur_cell_width;
                        
                        sgl_area_t cell_rect = {
                            .x1 = pos_x,
                            .y1 = fill_y,
                            .x2 = pos_x + cur_cell_width,
                            .y2 = fill_y + cell_height
                        };
                        
                        sgl_draw_fill_rect(surf, &obj->area, &cell_rect, 1, 
                        fill_color, SGL_ALPHA_MAX);
                        
                        if (i < battery->num_cells - 1) {
                            pos_x -= min_gap;
                        }
                    }
                }
            } else {
                int16_t min_gap = 2;
                int16_t total_min_gap = (battery->num_cells - 1) * min_gap;
                
                if (total_min_gap >= fill_height) {
                    min_gap = 1;
                    total_min_gap = battery->num_cells - 1;
                }
                
                int16_t cell_height = (fill_height - total_min_gap) / battery->num_cells;
                if (cell_height < 1) cell_height = 1;
                
                int16_t used_height = cell_height * battery->num_cells + total_min_gap;
                int16_t remaining_height = fill_height - used_height;
                
                int16_t cell_width = fill_width;
                
                int16_t pos_y = fill_y;
                for (int i = battery->num_cells - 1; i >= 0; i--) {
                    int16_t cur_cell_height = cell_height;
                    if (i < remaining_height) {
                        cur_cell_height++;
                    }
                    
                    if (i < active_cells) {
                        sgl_area_t cell_rect = {
                            .x1 = fill_x,
                            .y1 = pos_y,
                            .x2 = fill_x + cell_width,
                            .y2 = pos_y + cur_cell_height
                        };
                        
                        sgl_draw_fill_rect(surf, &obj->area, &cell_rect, 1, 
                        fill_color, SGL_ALPHA_MAX);
                    }
                    
                    pos_y += cur_cell_height + min_gap;
                }
            }
        }

        if (battery->charging) {
            int16_t cx = battery_x + battery_width / 2;
            int16_t cy = battery_y + battery_height / 2;
            int16_t h = battery_height / 2;
            int16_t w = battery_width / 6;
            int16_t px1, py1, px2, py2, px3, py3, px4, py4, px5, py5, px6, py6;
            
            sgl_draw_line_t line = {
                .alpha = SGL_ALPHA_MAX,
                .width = 4,
                .color = battery->charging_color
            };

            if (battery->direction == SGL_BATTERY_DIR_HORIZONTAL) {
                px1 = cx - w / 2; py1 = cy - h / 2;
                px2 = cx + w * 2; py2 = cy + h / 9;
                px3 = cx + w / 4; py3 = cy - h / 8;
                px4 = cx + w / 2; py4 = cy + h / 2;
                px5 = cx - w * 2; py5 = cy - h / 9;
                px6 = cx - w / 4; py6 = cy + h / 8;
            } else {
                px1 = cx + w / 2; py1 = cy - h / 2;
                px2 = cx - w / 2; py2 = cy - h / 15;
                px3 = cx + w * 2; py3 = cy - h / 9;
                px4 = cx - w / 2; py4 = cy + h / 2;
                px5 = cx + w / 2; py5 = cy + h / 15;
                px6 = cx - w * 2; py6 = cy + h / 9;
            }
            
            line.x1 = px1; line.y1 = py1; line.x2 = px2; line.y2 = py2; sgl_draw_line(surf, &obj->area, &line);
            line.x1 = px2; line.y1 = py2; line.x2 = px3; line.y2 = py3; sgl_draw_line(surf, &obj->area, &line);
            line.x1 = px3; line.y1 = py3; line.x2 = px4; line.y2 = py4; sgl_draw_line(surf, &obj->area, &line);
            line.x1 = px4; line.y1 = py4; line.x2 = px5; line.y2 = py5; sgl_draw_line(surf, &obj->area, &line);
            line.x1 = px5; line.y1 = py5; line.x2 = px6; line.y2 = py6; sgl_draw_line(surf, &obj->area, &line);
            line.x1 = px6; line.y1 = py6; line.x2 = px1; line.y2 = py1; sgl_draw_line(surf, &obj->area, &line);
        }

        if (battery->show_percentage && battery->font) {
            char text[8];
            sgl_sprintf(text, "%d%%", level);

            sgl_area_t text_area = {
                .x1 = x1,
                .y1 = y1,
                .x2 = x2,
                .y2 = y2
            };

            int8_t x_offset = 0;
            int8_t y_offset = 0;
            if (battery->cap_pos == SGL_BATTERY_CAP_RIGHT) {
                x_offset = -battery->cap_size;
            } else if (battery->cap_pos == SGL_BATTERY_CAP_LEFT) {
                x_offset = battery->cap_size;
            } else if (battery->cap_pos == SGL_BATTERY_CAP_TOP) {
                y_offset = battery->cap_size;
            }
            sgl_draw_string(surf, &text_area, 
                x1 + (width - sgl_font_get_string_width(text, battery->font) + x_offset) / 2, 
                y1 + (height - battery->font->font_height + y_offset) / 2, 
                text, battery->text_color, SGL_ALPHA_MAX, battery->font);
        }
    }
}


/**
 * @brief create a battery object
 * @param parent parent of object
 * @return pointer to battery object
 */
sgl_obj_t* sgl_battery_create(sgl_obj_t* parent)
{
    sgl_battery_t *battery = sgl_malloc(sizeof(sgl_battery_t));
    if(battery == NULL) {
        SGL_LOG_ERROR("sgl_battery_create: malloc failed");
        return NULL;
    }

    memset(battery, 0, sizeof(sgl_battery_t));

    sgl_obj_t *obj = &battery->obj;
    sgl_obj_init(&battery->obj, parent);
    obj->construct_fn = sgl_battery_construct_cb;
    obj->border = 0;

    battery->level = 100;
    battery->border_color = sgl_rgb(255, 255,255);
    battery->fill_color = sgl_rgb(0, 255, 0);
    battery->low_color = sgl_rgb(255, 0, 0);
    battery->medium_color = sgl_rgb(255, 165, 0);
    battery->high_color = sgl_rgb(0, 255, 0);
    battery->bg_color = sgl_rgb(30, 30, 30);
    battery->num_cells = 6;
    battery->direction = SGL_BATTERY_DIR_HORIZONTAL;
    battery->cap_size = 4;
    battery->cap_pos = SGL_BATTERY_CAP_RIGHT;
    battery->charging = 0;
    battery->charging_color = sgl_rgb(255, 255, 0);
    battery->show_percentage = 0;
    battery->font = NULL;
    battery->text_color = sgl_rgb(255, 255, 255);

    return obj;
}


/**
 * @brief set battery level
 * @param obj pointer to battery object
 * @param level battery level (0-100)
 * @return none
 */
void sgl_battery_set_level(sgl_obj_t* obj, uint8_t level)
{
    sgl_battery_t *battery = sgl_container_of(obj, sgl_battery_t, obj);
    battery->level = level;
    sgl_obj_set_dirty(obj);
}


/**
 * @brief set border color
 * @param obj pointer to battery object
 * @param color border color
 * @return none
 */
void sgl_battery_set_border_color(sgl_obj_t* obj, sgl_color_t color)
{
    sgl_battery_t *battery = sgl_container_of(obj, sgl_battery_t, obj);
    battery->border_color = color;
    sgl_obj_set_dirty(obj);
}


/**
 * @brief set fill color
 * @param obj pointer to battery object
 * @param color fill color
 * @return none
 */
void sgl_battery_set_fill_color(sgl_obj_t* obj, sgl_color_t color)
{
    sgl_battery_t *battery = sgl_container_of(obj, sgl_battery_t, obj);
    battery->fill_color = color;
    sgl_obj_set_dirty(obj);
}


/**
 * @brief set low battery color (below 20%)
 * @param obj pointer to battery object
 * @param color low battery color
 * @return none
 */
void sgl_battery_set_low_color(sgl_obj_t* obj, sgl_color_t color)
{
    sgl_battery_t *battery = sgl_container_of(obj, sgl_battery_t, obj);
    battery->low_color = color;
    sgl_obj_set_dirty(obj);
}


/**
 * @brief set medium battery color (20-50%)
 * @param obj pointer to battery object
 * @param color medium battery color
 * @return none
 */
void sgl_battery_set_medium_color(sgl_obj_t* obj, sgl_color_t color)
{
    sgl_battery_t *battery = sgl_container_of(obj, sgl_battery_t, obj);
    battery->medium_color = color;
    sgl_obj_set_dirty(obj);
}


/**
 * @brief set high battery color (above 50%)
 * @param obj pointer to battery object
 * @param color high battery color
 * @return none
 */
void sgl_battery_set_high_color(sgl_obj_t* obj, sgl_color_t color)
{
    sgl_battery_t *battery = sgl_container_of(obj, sgl_battery_t, obj);
    battery->high_color = color;
    sgl_obj_set_dirty(obj);
}


/**
 * @brief set background color
 * @param obj pointer to battery object
 * @param color background color
 * @return none
 */
void sgl_battery_set_bg_color(sgl_obj_t* obj, sgl_color_t color)
{
    sgl_battery_t *battery = sgl_container_of(obj, sgl_battery_t, obj);
    battery->bg_color = color;
    sgl_obj_set_dirty(obj);
}


/**
 * @brief set number of cells
 * @param obj pointer to battery object
 * @param num number of cells (1-10)
 * @return none
 */
void sgl_battery_set_num_cells(sgl_obj_t* obj, uint8_t num)
{
    sgl_battery_t *battery = sgl_container_of(obj, sgl_battery_t, obj);
    battery->num_cells = (num < 1) ? 1 : (num > 10) ? 10 : num;
    sgl_obj_set_dirty(obj);
}


/**
 * @brief set battery direction
 * @param obj pointer to battery object
 * @param dir direction (horizontal or vertical)
 * @return none
 */
void sgl_battery_set_direction(sgl_obj_t* obj, sgl_battery_dir_t dir)
{
    sgl_battery_t *battery = sgl_container_of(obj, sgl_battery_t, obj);
    battery->direction = dir;
    sgl_obj_set_dirty(obj);
}


/**
 * @brief set battery cap size (in pixels)
 * @param obj pointer to battery object
 * @param size cap size in pixels, 0 means no visible cap
 * @return none
 */
void sgl_battery_set_cap_size(sgl_obj_t* obj, uint8_t size)
{
    sgl_battery_t *battery = sgl_container_of(obj, sgl_battery_t, obj);
    battery->cap_size = size;
    sgl_obj_set_dirty(obj);
}


/**
 * @brief set battery cap position
 * @param obj pointer to battery object
 * @param pos cap position (right, left, top, bottom)
 * @return none
 */
void sgl_battery_set_cap_pos(sgl_obj_t* obj, sgl_battery_cap_pos_t pos)
{
    sgl_battery_t *battery = sgl_container_of(obj, sgl_battery_t, obj);
    battery->cap_pos = pos;
    sgl_obj_set_dirty(obj);
}


/**
 * @brief set charging status
 * @param obj pointer to battery object
 * @param charging charging status (1 = charging, 0 = not charging)
 * @return none
 */
void sgl_battery_set_charging(sgl_obj_t* obj, bool charging)
{
    sgl_battery_t *battery = sgl_container_of(obj, sgl_battery_t, obj);
    battery->charging = charging;
    sgl_obj_set_dirty(obj);
}


/**
 * @brief set charging indicator color
 * @param obj pointer to battery object
 * @param color charging indicator color
 * @return none
 */
void sgl_battery_set_charging_color(sgl_obj_t* obj, sgl_color_t color)
{
    sgl_battery_t *battery = sgl_container_of(obj, sgl_battery_t, obj);
    battery->charging_color = color;
    sgl_obj_set_dirty(obj);
}


/**
 * @brief show/hide percentage text
 * @param obj pointer to battery object
 * @param show show percentage text (1 = show, 0 = hide)
 * @return none
 */
void sgl_battery_show_percentage(sgl_obj_t* obj, bool show)
{
    sgl_battery_t *battery = sgl_container_of(obj, sgl_battery_t, obj);
    battery->show_percentage = show;
    sgl_obj_set_dirty(obj);
}


/**
 * @brief set font for percentage text
 * @param obj pointer to battery object
 * @param font font for percentage text
 * @return none
 */
void sgl_battery_set_font(sgl_obj_t* obj, const sgl_font_t *font)
{
    sgl_battery_t *battery = sgl_container_of(obj, sgl_battery_t, obj);
    battery->font = font;
    sgl_obj_set_dirty(obj);
}


/**
 * @brief set text color
 * @param obj pointer to battery object
 * @param color text color
 * @return none
 */
void sgl_battery_set_text_color(sgl_obj_t* obj, sgl_color_t color)
{
    sgl_battery_t *battery = sgl_container_of(obj, sgl_battery_t, obj);
    battery->text_color = color;
    sgl_obj_set_dirty(obj);
}
