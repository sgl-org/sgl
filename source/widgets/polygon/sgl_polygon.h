/* source/widgets/polygon/sgl_polygon.h
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

#ifndef __SGL_POLYGON_H__
#define __SGL_POLYGON_H__

#include <sgl_core.h>
#include <sgl_draw.h>
#include <sgl_math.h>
#include <sgl_log.h>
#include <sgl_mm.h>
#include <sgl_theme.h>
#include <sgl_cfgfix.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief sgl polygon struct
 */
typedef struct {
    sgl_obj_t obj;              // 基础对象
    uint16_t vertex_count;      // 顶点数量
    sgl_pos_t *vertices;        // 顶点坐标数组
    sgl_color_t fill_color;     // 填充颜色
    sgl_color_t border_color;   // 边框颜色
    uint8_t border_width;       // 边框宽度
    uint8_t alpha;              // 透明度
    const sgl_pixmap_t *pixmap; // 背景图片
    const char *text;           // 显示文本
    const sgl_font_t *font;     // 字体
    sgl_color_t text_color;     // 文本颜色
} sgl_polygon_t;

/**
 * @brief create a polygon object
 * @param parent parent of the polygon
 * @return polygon object
 */
sgl_obj_t* sgl_polygon_create(sgl_obj_t* parent);

/**
 * @brief set polygon vertices
 * @param obj polygon object
 * @param vertices vertices array
 * @param count vertices count
 * @return none
 */
void sgl_polygon_set_vertices(sgl_obj_t* obj, sgl_pos_t* vertices, uint16_t count);

/**
 * @brief set polygon vertices by coordinate arrays
 * @param obj polygon object
 * @param x_coords x coordinates array
 * @param y_coords y coordinates array
 * @param count vertices count
 * @return none
 */
void sgl_polygon_set_vertex_coords(sgl_obj_t* obj, int16_t* x_coords, int16_t* y_coords, uint16_t count);

/**
 * @brief set polygon vertices by 2D coordinate array
 * @param obj polygon object
 * @param coords 2D coordinates array
 * @param count vertices count
 * @return none
 */
void sgl_polygon_set_vertex_array(sgl_obj_t* obj, int16_t (*coords)[2], uint16_t count);

/**
 * @brief set polygon fill color
 * @param obj polygon object
 * @param color fill color
 * @return none
 */
void sgl_polygon_set_fill_color(sgl_obj_t* obj, sgl_color_t color);

/**
 * @brief set polygon border color
 * @param obj polygon object
 * @param color border color
 * @return none
 */
void sgl_polygon_set_border_color(sgl_obj_t* obj, sgl_color_t color);

/**
 * @brief set polygon border width
 * @param obj polygon object
 * @param width border width
 * @return none
 */
void sgl_polygon_set_border_width(sgl_obj_t* obj, uint8_t width);

/**
 * @brief set polygon alpha
 * @param obj polygon object
 * @param alpha alpha value
 * @return none
 */
void sgl_polygon_set_alpha(sgl_obj_t* obj, uint8_t alpha);

/**
 * @brief set polygon pixmap
 * @param obj polygon object
 * @param pixmap pixmap
 * @return none
 */
void sgl_polygon_set_pixmap(sgl_obj_t* obj, const sgl_pixmap_t* pixmap);

/**
 * @brief set polygon text
 * @param obj polygon object
 * @param text text to display
 * @return none
 */
void sgl_polygon_set_text(sgl_obj_t* obj, const char* text);

/**
 * @brief set polygon font
 * @param obj polygon object
 * @param font font to use for text
 * @return none
 */
void sgl_polygon_set_font(sgl_obj_t* obj, const sgl_font_t* font);

/**
 * @brief set polygon text color
 * @param obj polygon object
 * @param color text color
 * @return none
 */
void sgl_polygon_set_text_color(sgl_obj_t* obj, sgl_color_t color);

#ifdef __cplusplus
}
#endif

#endif // !__SGL_POLYGON_H__