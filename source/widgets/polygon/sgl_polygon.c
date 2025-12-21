/* source/widgets/polygon/sgl_polygon.c
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

#include "sgl_polygon.h"

// 多边形构造回调函数
static void sgl_polygon_construct_cb(sgl_surf_t *surf, sgl_obj_t* obj, sgl_event_t *evt)
{
    sgl_polygon_t *polygon = (sgl_polygon_t*)obj;
    
    if (evt->type == SGL_EVENT_DRAW_MAIN) {
        if (polygon->vertex_count < 3 || polygon->vertices == NULL) {
            return; // 至少需要3个顶点才能构成多边形
        }
        
        // 绘制填充
        if (polygon->fill_color.full != 0) {
            // 计算多边形的边界框（使用实际坐标）
            int16_t min_x = polygon->vertices[0].x, max_x = polygon->vertices[0].x;
            int16_t min_y = polygon->vertices[0].y, max_y = polygon->vertices[0].y;
            
            for (int i = 1; i < polygon->vertex_count; i++) {
                min_x = sgl_min(min_x, polygon->vertices[i].x);
                max_x = sgl_max(max_x, polygon->vertices[i].x);
                min_y = sgl_min(min_y, polygon->vertices[i].y);
                max_y = sgl_max(max_y, polygon->vertices[i].y);
            }
            
            sgl_area_t polygon_area = {
                .x1 = min_x,
                .x2 = max_x,
                .y1 = min_y,
                .y2 = max_y
            };
            
            sgl_area_t clip;
            if (sgl_surf_clip(surf, &polygon_area, &clip)) {
                if (sgl_area_selfclip(&clip, &obj->area)) {
                    for (int y = clip.y1; y <= clip.y2; y++) {
                        // 计算扫描线与多边形的交点
                        int intersections[64]; // 假设每条扫描线最多与64条边相交
                        int count = 0;
                        
                        for (int i = 0; i < polygon->vertex_count; i++) {
                            sgl_pos_t p1 = polygon->vertices[i];
                            sgl_pos_t p2 = polygon->vertices[(i + 1) % polygon->vertex_count];
                            
                            // 使用实际坐标，不进行位置偏移
                            
                            // 计算扫描线与边的交点
                            if ((p1.y > y) != (p2.y > y)) {
                                int intersect_x = p1.x + (y - p1.y) * (p2.x - p1.x) / (p2.y - p1.y);
                                intersections[count++] = intersect_x;
                            }
                        }
                        
                        // 对交点进行排序
                        for (int i = 0; i < count - 1; i++) {
                            for (int j = i + 1; j < count; j++) {
                                if (intersections[i] > intersections[j]) {
                                    int temp = intersections[i];
                                    intersections[i] = intersections[j];
                                    intersections[j] = temp;
                                }
                            }
                        }
                        
                        // 填充扫描线段
                        sgl_color_t *buf = sgl_surf_get_buf(surf, clip.x1 - surf->x, y - surf->y);
                        for (int i = 0; i < count; i += 2) {
                            int start = sgl_max(intersections[i], clip.x1);
                            int end = sgl_min(intersections[i + 1], clip.x2);
                            
                            if (start <= end) {
                                for (int x = start; x <= end; x++) {
                                    int buf_index = x - clip.x1;
                                    if (polygon->alpha == SGL_ALPHA_MAX) {
                                        buf[buf_index] = polygon->fill_color;
                                    } else {
                                        buf[buf_index] = sgl_color_mixer(polygon->fill_color, buf[buf_index], polygon->alpha);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        
        // 绘制边框
        if (polygon->border_width > 0 && polygon->border_color.full != 0) {
            // 明确绘制每一条边，确保所有边都被绘制
            for (int i = 0; i < polygon->vertex_count; i++) {
                // 获取当前顶点和下一个顶点（使用实际坐标）
                sgl_pos_t start = polygon->vertices[i];
                sgl_pos_t end = polygon->vertices[(i + 1) % polygon->vertex_count];
                
                // 确保坐标点顺序一致，避免因为坐标大小关系导致绘制异常
                sgl_draw_line_t line = {
                    .color = polygon->border_color,
                    .width = polygon->border_width,
                    .alpha = polygon->alpha
                };
                
                // 保证线段始终从较小坐标绘制到较大坐标
                if ((start.x < end.x) || (start.x == end.x && start.y < end.y)) {
                    line.start = start;
                    line.end = end;
                } else {
                    line.start = end;
                    line.end = start;
                }
                
                sgl_draw_line(surf, &line);
            }
        }
        
        // 绘制文本
        if (polygon->text && polygon->font) {
            // 计算多边形中心点
            int32_t center_x = 0, center_y = 0;
            for (int i = 0; i < polygon->vertex_count; i++) {
                center_x += polygon->vertices[i].x;
                center_y += polygon->vertices[i].y;
            }
            center_x /= polygon->vertex_count;
            center_y /= polygon->vertex_count;
            
            // 计算文本尺寸
            int16_t text_width = sgl_font_get_string_width(polygon->text, polygon->font);
            int16_t text_height = sgl_font_get_height(polygon->font);
            
            // 文本绘制位置（居中）
            int16_t text_x = center_x - text_width / 2;
            int16_t text_y = center_y - text_height / 2;
            
            sgl_draw_string(surf, &obj->area, text_x, text_y, polygon->text, polygon->text_color, polygon->alpha, polygon->font);
        }
    }
}

// 创建多边形对象
sgl_obj_t* sgl_polygon_create(sgl_obj_t* parent)
{
    sgl_polygon_t *polygon = (sgl_polygon_t*)sgl_malloc(sizeof(sgl_polygon_t));
    if (polygon == NULL) {
        return NULL;
    }
    
    memset(polygon, 0, sizeof(sgl_polygon_t));
    
    sgl_obj_t *obj = &polygon->obj;
    sgl_obj_init(obj, parent);
    obj->construct_fn = sgl_polygon_construct_cb;
    
    // 设置默认值
    polygon->vertex_count = 0;
    polygon->vertices = NULL;
    polygon->fill_color = sgl_rgb(127, 127, 127);
    polygon->border_color = sgl_rgb(0, 0, 0);
    polygon->border_width = 1;
    polygon->alpha = SGL_ALPHA_MAX;
    polygon->pixmap = NULL;
    polygon->text = NULL;
    polygon->font = NULL;
    polygon->text_color = sgl_rgb(0, 0, 0);
    
    return obj;
}

// 设置多边形顶点
void sgl_polygon_set_vertices(sgl_obj_t* obj, sgl_pos_t* vertices, uint16_t count)
{
    sgl_polygon_t *polygon = (sgl_polygon_t *)obj;
    if (polygon == NULL || vertices == NULL || count < 3) {
        return;
    }
    
    // 释放旧的顶点数据（如果有）
    if (polygon->vertices != NULL) {
        sgl_free(polygon->vertices);
    }
    
    // 分配新的顶点数据
    polygon->vertices = (sgl_pos_t*)sgl_malloc(sizeof(sgl_pos_t) * count);
    if (polygon->vertices == NULL) {
        polygon->vertex_count = 0;
        return;
    }
    
    // 复制顶点数据
    memcpy(polygon->vertices, vertices, sizeof(sgl_pos_t) * count);
    polygon->vertex_count = count;
    
    // 标记对象需要重绘
    sgl_obj_set_dirty(obj);
}

// 通过坐标数组设置多边形顶点
void sgl_polygon_set_vertex_coords(sgl_obj_t* obj, int16_t* x_coords, int16_t* y_coords, uint16_t count)
{
    sgl_polygon_t *polygon = (sgl_polygon_t *)obj;
    if (polygon == NULL || x_coords == NULL || y_coords == NULL || count < 3) {
        return;
    }
    
    // 释放旧的顶点数据（如果有）
    if (polygon->vertices != NULL) {
        sgl_free(polygon->vertices);
    }
    
    // 分配新的顶点数据
    polygon->vertices = (sgl_pos_t*)sgl_malloc(sizeof(sgl_pos_t) * count);
    if (polygon->vertices == NULL) {
        polygon->vertex_count = 0;
        return;
    }
    
    // 根据坐标数组构建顶点
    for (uint16_t i = 0; i < count; i++) {
        polygon->vertices[i].x = x_coords[i];
        polygon->vertices[i].y = y_coords[i];
    }
    polygon->vertex_count = count;
    
    // 标记对象需要重绘
    sgl_obj_set_dirty(obj);
}

// 通过二维坐标数组设置多边形顶点
void sgl_polygon_set_vertex_array(sgl_obj_t* obj, int16_t (*coords)[2], uint16_t count)
{
    sgl_polygon_t *polygon = (sgl_polygon_t *)obj;
    if (polygon == NULL || coords == NULL || count < 3) {
        return;
    }
    
    // 释放旧的顶点数据（如果有）
    if (polygon->vertices != NULL) {
        sgl_free(polygon->vertices);
    }
    
    // 分配新的顶点数据
    polygon->vertices = (sgl_pos_t*)sgl_malloc(sizeof(sgl_pos_t) * count);
    if (polygon->vertices == NULL) {
        polygon->vertex_count = 0;
        return;
    }
    
    // 根据二维坐标数组构建顶点
    for (uint16_t i = 0; i < count; i++) {
        polygon->vertices[i].x = coords[i][0];
        polygon->vertices[i].y = coords[i][1];
    }
    polygon->vertex_count = count;
    
    // 标记对象需要重绘
    sgl_obj_set_dirty(obj);
}

// 设置填充颜色
void sgl_polygon_set_fill_color(sgl_obj_t* obj, sgl_color_t color)
{
    sgl_polygon_t *polygon = (sgl_polygon_t *)obj;
    if (polygon == NULL) {
        return;
    }
    
    polygon->fill_color = color;
    sgl_obj_set_dirty(obj);
}

// 设置边框颜色
void sgl_polygon_set_border_color(sgl_obj_t* obj, sgl_color_t color)
{
    sgl_polygon_t *polygon = (sgl_polygon_t *)obj;
    if (polygon == NULL) {
        return;
    }
    
    polygon->border_color = color;
    sgl_obj_set_dirty(obj);
}

// 设置边框宽度
void sgl_polygon_set_border_width(sgl_obj_t* obj, uint8_t width)
{
    sgl_polygon_t *polygon = (sgl_polygon_t *)obj;
    if (polygon == NULL) {
        return;
    }
    
    polygon->border_width = width;
    sgl_obj_set_dirty(obj);
}

// 设置透明度
void sgl_polygon_set_alpha(sgl_obj_t* obj, uint8_t alpha)
{
    sgl_polygon_t *polygon = (sgl_polygon_t *)obj;
    if (polygon == NULL) {
        return;
    }
    
    polygon->alpha = alpha;
    sgl_obj_set_dirty(obj);
}

// 设置背景图片
void sgl_polygon_set_pixmap(sgl_obj_t* obj, const sgl_pixmap_t* pixmap)
{
    sgl_polygon_t *polygon = (sgl_polygon_t *)obj;
    if (polygon == NULL) {
        return;
    }
    
    polygon->pixmap = pixmap;
    sgl_obj_set_dirty(obj);
}

// 设置文本
void sgl_polygon_set_text(sgl_obj_t* obj, const char* text)
{
    sgl_polygon_t *polygon = (sgl_polygon_t *)obj;
    if (polygon == NULL) {
        return;
    }
    
    polygon->text = text;
    sgl_obj_set_dirty(obj);
}

// 设置字体
void sgl_polygon_set_font(sgl_obj_t* obj, const sgl_font_t* font)
{
    sgl_polygon_t *polygon = (sgl_polygon_t *)obj;
    if (polygon == NULL) {
        return;
    }
    
    polygon->font = font;
    sgl_obj_set_dirty(obj);
}

// 设置文本颜色
void sgl_polygon_set_text_color(sgl_obj_t* obj, sgl_color_t color)
{
    sgl_polygon_t *polygon = (sgl_polygon_t *)obj;
    if (polygon == NULL) {
        return;
    }
    
    polygon->text_color = color;
    sgl_obj_set_dirty(obj);
}
