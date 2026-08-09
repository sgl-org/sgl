/* source/include/sgl_draw.h
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

#ifndef __SGL_DRAW_H__
#define __SGL_DRAW_H__ 

#include <sgl_core.h>

#ifdef __cplusplus
extern "C" {
#endif

#define  SGL_ARC_MODE_NORMAL                                (0)
#define  SGL_ARC_MODE_RING                                  (1)
#define  SGL_ARC_MODE_NORMAL_SMOOTH                         (2)
#define  SGL_ARC_MODE_RING_SMOOTH                           (3)

/**
 * @brief rect description
 * @color: color of rect
 * @alpha: alpha of rect
 * @border_alpha: alpha of border
 * @border_mask: mask of border
 * @border: border of rect
 * @border_color: border color of rect
 * @pixmap: pixmap of rect
 */
typedef struct sgl_draw_rect {
    uint8_t                 alpha;
    uint8_t                 border;
    uint8_t                 border_alpha;
    uint8_t                 border_mask;
    sgl_color_t             color;
    int16_t                 radius;
    sgl_color_t             border_color;
    const sgl_pixmap_t      *pixmap;
} sgl_draw_rect_t;


/**
 * @brief line draw description
 * @x1: x1 coordinate
 * @y1: y1 coordinate
 * @x2: x2 coordinate
 * @y2: y2 coordinate
 * @color: color
 * @alpha: alpha
 * @width: width of line
 */
typedef struct sgl_draw_line {
    uint8_t          alpha;
    uint8_t          width;
    sgl_color_t      color;
    int16_t          x1;
    int16_t          y1;
    int16_t          x2;
    int16_t          y2;
} sgl_draw_line_t;


/**
 * @brief rectangle description
 * @cx: center x of rectangle
 * @cy: center y of rectangle
 * @color: color of rectangle
 * @radius: radius of rectangle
 * @alpha: alpha of rectangle
 * @border: border of rectangle
 * @border_color: border color of rectangle
 * @pixmap: pixmap of rectangle
 */
typedef struct sgl_draw_circle {
    uint8_t            alpha;
    uint8_t            border;
    sgl_color_t        color;
    int16_t            radius;
    sgl_color_t        border_color;
    int16_t            cx;
    int16_t            cy;
    const sgl_pixmap_t *pixmap;
} sgl_draw_circle_t;


/**
 * @brief arc description
 * @cx: center x
 * @cy: center y
 * @radius_in: inner radius of arc
 * @radius_out: outer radius of arc
 * @start_angle: start angle of arc
 * @end_angle: end angle of arc
 * @color: color of arc
 * @alpha: alpha of arc
 * @mode: mode of arc
 * @bg_color: background color of arc
 */
typedef struct sgl_draw_arc {
    uint8_t          alpha;
    sgl_color_t      color;
    int16_t          cx;
    int16_t          cy;
    int16_t          radius_in;
    int16_t          radius_out;
    uint32_t         start_angle: 9;
    uint32_t         end_angle: 9;
    uint32_t         mode: 2;
    sgl_color_t      bg_color;
} sgl_draw_arc_t;


/**
 * @brief icon description
 * @icon: icon pixmap
 * @color: icon color
 * @alpha: alpha of icon
 */
typedef struct sgl_draw_icon {
    uint8_t           alpha;
    uint8_t           align;
    sgl_color_t       color;
    const sgl_icon_pixmap_t *icon;
} sgl_draw_icon_t;


/** 
 * @brief clip area width of surface
 * @note if you want to check the area is overlap with surface, you can use this macro
 *       it will direct return if the area is not overlap with surface, otherwise, continue
 */
#if (CONFIG_SGL_USE_FBDEV_VRAM)
#define sgl_surf_clip_area_return(surf, rect, clip)         if (!sgl_area_clip(surf->dirty, rect, clip)) return
#else
#define sgl_surf_clip_area_return(surf, rect, clip)         if (!sgl_surf_clip(surf, rect, clip)) return
#endif


/**
 * @brief set pixel on surface
 * @param surf: pointer of surface
 * @param x: x coordinate
 * @param y: y coordinate
 * @param color: color of pixel
 * @note this function is not clip, you should clip it before call this function, and the coordinate should be in the surface.
 */
static inline void sgl_surf_set_pixel(sgl_surf_t *surf, int16_t x, int16_t y, sgl_color_t color) 
{
    surf->buffer[y * surf->w + x] = color;
}


/**
 * @brief get start buffer address that to set pixel on surface
 * @param surf: pointer of surface
 * @param x: x coordinate
 * @param y: y coordinate
 * @return pointer of start buffer address
 * @note this function is not clip, you should clip it before call this function, and the coordinate should be in the surface.
 */
static inline sgl_color_t* sgl_surf_get_buf(sgl_surf_t *surf, int16_t x, int16_t y)
{
    return &surf->buffer[y * surf->w + x];
}


/**
 * @brief get pixel on surface
 * @param surf: pointer of surface
 * @param x: x coordinate
 * @param y: y coordinate
 * @return color of pixel
 * @note this function is not clip, you should clip it before call this function, and the coordinate should be in the surface.
 */
static inline sgl_color_t sgl_surf_get_pixel(sgl_surf_t *surf, int16_t x, int16_t y) 
{
    return surf->buffer[y * surf->w + x];
}


/**
 * @brief draw a horizontal line on surface
 * @param surf: pointer of surface
 * @param y: y coordinate
 * @param x1: x1 coordinate
 * @param x2: x2 coordinate
 * @param color: color of line
 * @note this function is not clip, you should clip it before call this function, and the coordinate should be in the surface.
 */
static inline void sgl_surf_hline(sgl_surf_t *surf, int16_t y, int16_t x1, int16_t x2, sgl_color_t color) 
{
    sgl_color_t *dst = surf->buffer + y * surf->w + x1;
    for (int16_t i = x1; i <= x2; i++) {
        *dst = color;
        dst++;
    }
}


/**
 * @brief draw a vertical line on surface
 * @param surf: pointer of surface
 * @param x: x coordinate
 * @param y1: y1 coordinate
 * @param y2: y2 coordinate
 * @param color: color of line
 * @note this function is not clip, you should clip it before call this function, and the coordinate should be in the surface.
 */
static inline void sgl_surf_vline(sgl_surf_t *surf, int16_t x, int16_t y1, int16_t y2, sgl_color_t color) 
{
    sgl_color_t *dst = surf->buffer + y1 * surf->w + x;
    for (int16_t i = y1; i <= y2; i++) {
        *dst = color;
        dst += surf->w;
    }
}


/**
 * @brief draw a wireframe rectangle with alpha
 * @param surf point to surface
 * @param area area of rectangle that you want to draw
 * @param rect point to rectangle that you want to draw
 * @param radius radius of round
 * @param width width of wireframe
 * @param color color of rectangle
 * @param alpha alpha of rectangle
 * @return none
 */
void sgl_draw_wireframe(sgl_surf_t *surf, sgl_area_t *area, sgl_area_t *rect, int16_t radius, int16_t width, sgl_color_t color, uint8_t alpha);


/**
 * @brief fill a round rectangle with alpha
 * @param surf point to surface
 * @param area area of rectangle that you want to draw
 * @param rect point to rectangle that you want to draw
 * @param radius radius of round
 * @param color color of rectangle
 * @param alpha alpha of rectangle
 * @return none
 */
void sgl_draw_fill_rect(sgl_surf_t *surf, sgl_area_t *area, sgl_area_t *rect, int16_t radius, sgl_color_t color, uint8_t alpha);


/**
 * @brief fill a round rectangle with rich independent corner radiuses with alpha
 * @param surf point to surface
 * @param area area of rectangle that you want to draw
 * @param rect point to rectangle that you want to draw
 * @param tl_radius  Top-Left corner radius
 * @param tr_radius Top-Right corner radius
 * @param bl_radius Bottom-Left corner radius
 * @param br_radius   Bottom-Right corner radius
 * @param color color of rectangle
 * @param alpha alpha of rectangle
 * @return none
 */
void sgl_draw_fill_rich_rect(sgl_surf_t *surf, sgl_area_t *area, sgl_area_t *rect, 
                             int16_t tl_radius, int16_t tr_radius, int16_t bl_radius, int16_t br_radius, 
                             sgl_color_t color, uint8_t alpha);


/**
 * @brief draw only the border ring of a round rectangle, the interior is left untouched
 * @param surf point to surface
 * @param area area of rectangle that you want to draw
 * @param rect point to rectangle that you want to draw
 * @param radius radius of round
 * @param border_color color of border
 * @param border_width width of border
 * @param border_alpha alpha of border
 * @return none
 */
void sgl_draw_fill_rect_border(sgl_surf_t *surf, sgl_area_t *area, sgl_area_t *rect, int16_t radius, sgl_color_t border_color, uint8_t border_width, uint8_t border_alpha);

/**
 * @brief draw only the border ring of a round rectangle with independent corner radii, the interior is left untouched
 * @param surf point to surface
 * @param area area of rectangle that you want to draw
 * @param rect point to rectangle that you want to draw
 * @param tl_radius radius of top-left corner
 * @param tr_radius radius of top-right corner
 * @param bl_radius radius of bottom-left corner
 * @param br_radius radius of bottom-right corner
 * @param border_color color of border
 * @param border_width width of border
 * @param border_alpha alpha of border
 * @return none
 */
void sgl_draw_fill_rect_border_rich(sgl_surf_t *surf, sgl_area_t *area, sgl_area_t *rect, 
                                    int16_t tl_radius, int16_t tr_radius, int16_t bl_radius, int16_t br_radius, 
                                    sgl_color_t border_color, uint8_t border_width, uint8_t border_alpha);

/**
 * @brief fill a round rectangle pixmap with alpha
 * @param surf point to surface
 * @param area area of rectangle that you want to draw
 * @param rect point to rectangle that you want to draw
 * @param radius radius of round
 * @param pixmap pixmap of rectangle
 * @param alpha alpha of rectangle
 * @return none
 */
void sgl_draw_fill_rect_pixmap(sgl_surf_t *surf, sgl_area_t *area, sgl_area_t *rect, int16_t radius, const sgl_pixmap_t *pixmap, uint8_t alpha);


/**
 * @brief fill a round rectangle pixmap with individual corner radii and alpha
 * @param surf point to surface
 * @param area area of rectangle that you want to draw
 * @param rect point to rectangle that you want to draw
 * @param tl_radius radius of top-left corner
 * @param tr_radius radius of top-right corner
 * @param bl_radius radius of bottom-left corner
 * @param br_radius radius of bottom-right corner
 * @param pixmap pixmap of rectangle
 * @param alpha alpha of rectangle
 * @return none
 */
void sgl_draw_fill_rect_pixmap_rich(sgl_surf_t *surf, sgl_area_t *area, sgl_area_t *rect, 
                                    int16_t tl_radius, int16_t tr_radius, int16_t bl_radius, int16_t br_radius,
                                    const sgl_pixmap_t *pixmap, uint8_t alpha);

/**
 * @brief fill a round rectangle with alpha
 * @param surf point to surface
 * @param area area of rectangle that you want to draw
 * @param rect point to rectangle that you want to draw
 * @param desc rectangle description
 * @return none
 */
void sgl_draw_rect(sgl_surf_t *surf, sgl_area_t *area, sgl_area_t *rect, sgl_draw_rect_t *desc);


/**
 * @brief Draw a circle
 * @param surf Surface
 * @param area Area of the circle
 * @param cx X coordinate of the center
 * @param cy Y coordinate of the center
 * @param radius Radius of the circle
 * @param color Color of the circle
 * @param alpha Alpha of the circle
 * @return none
 */
void sgl_draw_fill_circle(sgl_surf_t *surf, sgl_area_t *area, int16_t cx, int16_t cy, int16_t radius, sgl_color_t color, uint8_t alpha);


/**
 * @brief Draw a circle with pixmap and alpha
 * @param surf Surface
 * @param area Area of the circle
 * @param cx X coordinate of the center
 * @param cy Y coordinate of the center
 * @param radius Radius of the circle
 * @param pixmap Pixmap of image
 * @param alpha Alpha of the circle
 * @return none
 */
void sgl_draw_fill_circle_pixmap(sgl_surf_t *surf, sgl_area_t *area, int16_t cx, int16_t cy, int16_t radius, const sgl_pixmap_t *pixmap, uint8_t alpha);

/**
 * @brief Draw only the border ring of a circle, the interior is left untouched
 * @param surf Surface
 * @param area Area of the circle
 * @param cx X coordinate of the center
 * @param cy Y coordinate of the center
 * @param radius Radius of the circle
 * @param border_color Color of the border
 * @param border_width Width of the border
 * @param alpha Alpha of the border
 * @return none
 */
void sgl_draw_fill_circle_border(sgl_surf_t *surf, sgl_area_t *area, int16_t cx, int16_t cy, int16_t radius, sgl_color_t border_color, int16_t border_width, uint8_t alpha);

/**
 * @brief draw task, the task contains the draw information and canvas
 * @param surf surface pointer
 * @param area the area of the task
 * @param desc the draw information
 * @return none
 */
void sgl_draw_circle(sgl_surf_t *surf, sgl_area_t *area, sgl_draw_circle_t *desc);


/**
 * @brief draw icon with alpha
 * @param surf   surface
 * @param area   area of icon
 * @param coords coords of icon
 * @param icon   icon pixmap
 * @param alpha  alpha of icon
 */
void sgl_draw_icon( sgl_surf_t *surf, sgl_area_t *area, int16_t x, int16_t y, sgl_color_t color, uint8_t alpha, const sgl_icon_pixmap_t *icon);


/**
 * @brief Draw a character on the surface with alpha blending
 * @param surf Pointer to the surface where the character will be drawn
 * @param area Pointer to the area where the character will be drawn
 * @param x X coordinate where the character will be drawn
 * @param y Y coordinate where the character will be drawn
 * @param ch_index Index of the character in the font table
 * @param color Foreground color of the character
 * @param alpha Alpha value for blending
 * @param font Pointer to the font structure containing character data
 * @return none
 * @note this function is only support bpp:4
 */
void sgl_draw_character( sgl_surf_t *surf, sgl_area_t *area, int16_t x, int16_t y, uint32_t ch_index, sgl_color_t color, uint8_t alpha, const sgl_font_t *font);


/**
 * @brief Draw a string on the surface with alpha blending
 * @param surf Pointer to the surface where the string will be drawn
 * @param area Pointer to the area where the string will be drawn
 * @param x X coordinate of the top-left corner of the string
 * @param y Y coordinate of the top-left corner of the string
 * @param str Pointer to the string to be drawn
 * @param color Foreground color of the string
 * @param alpha Alpha value for blending
 * @param font Pointer to the font structure containing character data
 * @return none
 */
void sgl_draw_string(sgl_surf_t *surf, sgl_area_t *area, int16_t x, int16_t y, const char *str, sgl_color_t color, uint8_t alpha, const sgl_font_t *font);


/**
 * @brief Draw a string on the surface with alpha blending and multiple lines
 * @param surf Pointer to the surface where the string will be drawn
 * @param area Pointer to the area where the string will be drawn
 * @param x X coordinate of the top-left corner of the string
 * @param y Y coordinate of the top-left corner of the string
 * @param str Pointer to the string to be drawn
 * @param color Foreground color of the string
 * @param alpha Alpha value for blending
 * @param font Pointer to the font structure containing character data
 * @param line_margin Margin between lines
 * @return none
 */
void sgl_draw_string_mult_line(sgl_surf_t *surf, sgl_area_t *area, int16_t x, int16_t y, const char *str, sgl_color_t color, uint8_t alpha, const sgl_font_t *font, uint8_t line_margin);


/**
 * @brief draw a ring on surface with alpha
 * @param surf: pointer of surface
 * @param area: pointer of area
 * @param cx: ring center x
 * @param cy: ring center y
 * @param radius_in: ring inner radius
 * @param radius_out: ring outer radius
 * @param color: ring color
 * @param alpha: ring alpha
 * @return none
 */
void sgl_draw_fill_ring(sgl_surf_t *surf, sgl_area_t *area, int16_t cx, int16_t cy, int16_t radius_in, int16_t radius_out, sgl_color_t color, uint8_t alpha);


/**
 * @brief draw a horizontal line with alpha
 * @param surf surface
 * @param area area that contains the line
 * @param y line y position
 * @param x1 line start x position
 * @param x2 line end x position
 * @param color line color
 * @param alpha alpha of color
 * @return none
 */
void sgl_draw_fill_hline(sgl_surf_t *surf, sgl_area_t *area, int16_t y, int16_t x1, int16_t x2, uint8_t width, sgl_color_t color, uint8_t alpha);


/**
 * @brief draw a vertical line with alpha
 * @param surf surface
 * @param area area that contains the line
 * @param x x coordinate
 * @param y1 y1 coordinate
 * @param y2 y2 coordinate
 * @param color line color
 * @param alpha alpha of color
 * @return none
 */
void sgl_draw_fill_vline(sgl_surf_t *surf, sgl_area_t *area, int16_t x, int16_t y1, int16_t y2, uint8_t width, sgl_color_t color, uint8_t alpha);


/**
 * @brief draw a slanted line with alpha
 * @param surf surface
 * @param area area that contains the line
 * @param x1 line start x position
 * @param y1 line start y position
 * @param x2 line end x position
 * @param y2 line end y position
 * @param thickness line width
 * @param color line color
 * @param alpha alpha of color
 * @return none
 * @note This algorithm is SDF algorithm
 */
void sgl_draw_line_fill_slanted(sgl_surf_t *surf, sgl_area_t *area, int16_t x1, int16_t y1, int16_t x2, int16_t y2, int16_t thickness, sgl_color_t color, uint8_t alpha);


/**
 * @brief Draw a dashed line using Bresenham's algorithm.
 * 
 * @param surf   Pointer to the surface structure.
 * @param area   Pointer to the area structure defining the valid drawing region.
 * @param x1     Start X-coordinate.
 * @param y1     Start Y-coordinate.
 * @param x2     End X-coordinate.
 * @param y2     End Y-coordinate.
 * @param gap    Length of the dash and the gap in pixels. Must be > 0.
 * @param color  Color of the line.
 * @return none
 * @note: This function is Non-anti-aliased!!!!
 */
void sgl_draw_dashed_line_noaa(sgl_surf_t *surf, sgl_area_t *area, int16_t x1, int16_t y1, int16_t x2, int16_t y2, int16_t gap, sgl_color_t color);

/**
 * @brief Fast line drawing with customizable width and alpha blending
 * @param surf   Pointer to the target surface
 * @param area   Clipping area for rendering
 * @param x0     Start X-coordinate.
 * @param y0     Start Y-coordinate.
 * @param x1     End X-coordinate.
 * @param y1     End Y-coordinate.
 * @param color  Line color
 * @param width  Line width in pixels
 * @param alpha  Alpha transparency value
 * @note: This function is Non-anti-aliased!!!!
 */
void sgl_draw_line_noaa(sgl_surf_t *surf, sgl_area_t *area, int16_t x1, int16_t y1, int16_t x2, int16_t y2, sgl_color_t color, uint8_t width, uint8_t alpha);


/**
 * @brief draw a line
 * @param surf surface
 * @param area area that contains the line
 * @param desc line description
 * @return none
 */
void sgl_draw_line(sgl_surf_t *surf, sgl_area_t *area, sgl_draw_line_t *desc);


/**
 * @brief draw an arc with alpha
 * @param surf pointer to surface
 * @param area pointer to area
 * @param desc pointer to arc description
 * @return none
 */
void sgl_draw_fill_arc(sgl_surf_t *surf, sgl_area_t *area, sgl_draw_arc_t *desc);


/**
 * @brief calculate a point color by bilinear interpolate (with mask support)
 * @param buffer point to image pixmap start buffer (RGB)
 * @param w      width of buffer
 * @param h      height of buffer
 * @param fx     x coordinate of point (fixed point, SGL_FIXED_SHIFT bits fraction)
 * @param fy     y coordinate of point (fixed point, SGL_FIXED_SHIFT bits fraction)
 * @return point color (RGB: interpolated if mask non-0, transparent/black if mask 0)
 */
sgl_color_t sgl_draw_biln_color(const sgl_color_t *buffer, int16_t w, int16_t h, int32_t fx, int32_t fy);


/**
 * @brief transform a surface
 * @param dst destination surface
 * @param src source surface
 * @param area area of surface
 * @param x x coordinate of surface
 * @param y y coordinate of surface
 * @param rotation rotation angle
 * @return none
 * @note This function has implemented angle normalization to the range of 0 to 360 degrees.
 */
void sgl_draw_xform_surf(sgl_surf_t *dst, sgl_surf_t *src, sgl_area_t *area, int16_t x, int16_t y, int16_t rotation);


/**
 * @brief Coverage mask used by the bezier stroke engine (1 byte per pixel).
 *        The caller allocates data with (x2-x1+1)*(y2-y1+1) bytes.
 */
typedef struct {
    uint8_t *data;   /* coverage buffer */
    int32_t  x1;     /* left   (absolute X, inclusive) */
    int32_t  y1;     /* top    (absolute Y, inclusive) */
    int32_t  x2;     /* right  (absolute X, inclusive) */
    int32_t  y2;     /* bottom (absolute Y, inclusive) */
} sgl_bezier_mask_t;

/**
 * @brief Draw a quadratic bezier curve (anti-aliased, round-capped)
 * @param surf      pointer to surface
 * @param area      pointer to area
 * @param x0,y0     start point
 * @param x1,y1     control point
 * @param x2,y2     end point
 * @param thickness stroke half-width (radius) in pixels
 * @param color     curve color
 * @param alpha     curve alpha
 * @param mask      caller-provided coverage mask covering the curve bounds
 * @return none
 */
void sgl_draw_bezier_quad(sgl_surf_t *surf, sgl_area_t *area,
                          int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                          int16_t x2, int16_t y2,
                          int16_t thickness, sgl_color_t color, uint8_t alpha,
                          sgl_bezier_mask_t *mask);

/**
 * @brief Draw a cubic bezier curve (anti-aliased, round-capped)
 * @param surf      pointer to surface
 * @param area      pointer to area
 * @param x0,y0     start point
 * @param x1,y1     first control point
 * @param x2,y2     second control point
 * @param x3,y3     end point
 * @param thickness stroke half-width (radius) in pixels
 * @param color     curve color
 * @param alpha     curve alpha
 * @param mask      caller-provided coverage mask covering the curve bounds
 * @return none
 */
void sgl_draw_bezier_cubic(sgl_surf_t *surf, sgl_area_t *area,
                           int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                           int16_t x2, int16_t y2, int16_t x3, int16_t y3,
                           int16_t thickness, sgl_color_t color, uint8_t alpha,
                           sgl_bezier_mask_t *mask);


#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
