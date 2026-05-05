/* source/widgets/polygon/sgl_polygon.c
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
 * 
 */
#include "sgl_polygon.h"

static void sgl_polygon_blend_pixel(sgl_surf_t *surf, const sgl_area_t *clip, int16_t x, int16_t y, sgl_color_t color, uint8_t alpha)
{
    if (surf == NULL || clip == NULL || alpha == 0 || x < clip->x1 || x > clip->x2 || y < clip->y1 || y > clip->y2) {
        return;
    }

    if (x < surf->x1 || x > surf->x2 || y < surf->y1 || y > surf->y2) {
        return;
    }

    sgl_color_t *buf = sgl_surf_get_buf(surf, x - surf->x1, y - surf->y1);
    *buf = (alpha == SGL_ALPHA_MAX) ? color : sgl_color_mixer(color, *buf, alpha);
}

static void sgl_polygon_draw_border_line(sgl_surf_t *surf,
                                         const sgl_area_t *clip,
                                         int16_t x1,
                                         int16_t y1,
                                         int16_t x2,
                                         int16_t y2,
                                         uint8_t width,
                                         sgl_color_t color,
                                         uint8_t alpha)
{
    int32_t bax = (int32_t)x2 - x1;
    int32_t bay = (int32_t)y2 - y1;
    int64_t len_sq = (int64_t)bax * bax + (int64_t)bay * bay;
    int16_t extent;
    sgl_area_t line_area;

    if (width == 0 || alpha == 0) {
        return;
    }

    if (len_sq == 0) {
        int16_t radius = (width + 1) / 2;
        for (int16_t py = y1 - radius; py <= y1 + radius; py++) {
            for (int16_t px = x1 - radius; px <= x1 + radius; px++) {
                int32_t dx = ((int32_t)px << 8) + 128 - (((int32_t)x1 << 8) + 128);
                int32_t dy = ((int32_t)py << 8) + 128 - (((int32_t)y1 << 8) + 128);
                int32_t dist = sgl_sqrt((uint32_t)(dx * dx + dy * dy));
                int32_t radius_fp = width << 7;
                int32_t fade = radius_fp + 128 - dist;
                if (fade > 0) {
                    uint8_t cov = (fade >= 255) ? 255 : (uint8_t)fade;
                    uint8_t mix = (uint8_t)(((uint16_t)cov * alpha) >> 8);
                    if (mix == 0 && cov != 0) mix = 1;
                    sgl_polygon_blend_pixel(surf, clip, px, py, color, mix);
                }
            }
        }
        return;
    }

    extent = (int16_t)((width + 3) / 2);
    line_area.x1 = sgl_min(x1, x2) - extent;
    line_area.x2 = sgl_max(x1, x2) + extent;
    line_area.y1 = sgl_min(y1, y2) - extent;
    line_area.y2 = sgl_max(y1, y2) + extent;

    if (!sgl_area_is_overlap((sgl_area_t *)clip, &line_area)) {
        return;
    }

    if (!sgl_area_selfclip(&line_area, (sgl_area_t *)clip)) {
        return;
    }

    for (int16_t py = line_area.y1; py <= line_area.y2; py++) {
        for (int16_t px = line_area.x1; px <= line_area.x2; px++) {
            int32_t pcx = ((int32_t)px << 8) + 128;
            int32_t pcy = ((int32_t)py << 8) + 128;
            int32_t ax = pcx - (((int32_t)x1 << 8) + 128);
            int32_t ay = pcy - (((int32_t)y1 << 8) + 128);
            int64_t dot = (int64_t)ax * ((int32_t)bax << 8) + (int64_t)ay * ((int32_t)bay << 8);
            int32_t qx, qy;
            int32_t dx, dy;
            int32_t dist;
            int32_t radius_fp = width << 7;
            int32_t fade;

            if (dot <= 0) {
                qx = ((int32_t)x1 << 8) + 128;
                qy = ((int32_t)y1 << 8) + 128;
            } else if (dot >= (len_sq << 16)) {
                qx = ((int32_t)x2 << 8) + 128;
                qy = ((int32_t)y2 << 8) + 128;
            } else {
                qx = (((int32_t)x1 << 8) + 128) + (int32_t)((((int64_t)bax << 8) * dot) / (len_sq << 16));
                qy = (((int32_t)y1 << 8) + 128) + (int32_t)((((int64_t)bay << 8) * dot) / (len_sq << 16));
            }

            dx = pcx - qx;
            dy = pcy - qy;
            dist = sgl_sqrt((uint32_t)((int64_t)dx * dx + (int64_t)dy * dy));
            fade = radius_fp + 128 - dist;

            if (fade > 0) {
                uint8_t cov = (fade >= 255) ? 255 : (uint8_t)fade;
                uint8_t mix = (uint8_t)(((uint16_t)cov * alpha) >> 8);
                if (mix == 0 && cov != 0) mix = 1;
                sgl_polygon_blend_pixel(surf, clip, px, py, color, mix);
            }
        }
    }
}

static void sgl_polygon_fill_scanline(sgl_surf_t *surf,
                                      const sgl_area_t *clip,
                                      sgl_polygon_t *polygon,
                                      sgl_obj_t *obj)
{
    int16_t intersections[SGL_POLYGON_VERTEX_MAX];
    bool solid_alpha = (polygon->alpha == SGL_ALPHA_MAX);

    for (int16_t y = clip->y1; y <= clip->y2; y++) {
        uint16_t hit_count = 0;
        int32_t scan_y = (((int32_t)y - obj->coords.y1) << 8) + 128;

        for (uint16_t i = 0, j = polygon->vertex_count - 1; i < polygon->vertex_count; j = i++) {
            int32_t x1 = ((int32_t)polygon->vertices[j].x << 8) + 128;
            int32_t y1 = ((int32_t)polygon->vertices[j].y << 8) + 128;
            int32_t x2 = ((int32_t)polygon->vertices[i].x << 8) + 128;
            int32_t y2 = ((int32_t)polygon->vertices[i].y << 8) + 128;

            if ((y1 > scan_y) == (y2 > scan_y) || y1 == y2) {
                continue;
            }

            intersections[hit_count++] = obj->coords.x1 +
                                         (int16_t)((x1 + (int32_t)(((int64_t)(scan_y - y1) * (x2 - x1)) / (y2 - y1)) - 128) >> 8);
        }

        if (hit_count < 2) {
            continue;
        }

        for (uint16_t i = 0; i + 1 < hit_count; i++) {
            for (uint16_t j = i + 1; j < hit_count; j++) {
                if (intersections[j] < intersections[i]) {
                    int16_t tmp = intersections[i];
                    intersections[i] = intersections[j];
                    intersections[j] = tmp;
                }
            }
        }

        sgl_color_t *buf = sgl_surf_get_buf(surf, clip->x1 - surf->x1, y - surf->y1);
        for (uint16_t i = 0; i + 1 < hit_count; i += 2) {
            int16_t x_start = sgl_max((int16_t)(intersections[i] + 1), clip->x1);
            int16_t x_end = sgl_min((int16_t)(intersections[i + 1] - 1), clip->x2);
            uint32_t len;

            if (x_start > x_end) {
                continue;
            }

            len = (uint32_t)(x_end - x_start + 1);

            if (solid_alpha) {
                sgl_color_set(buf + (x_start - clip->x1), polygon->fill_color, len);
            } else {
                sgl_color_t *blend = buf + (x_start - clip->x1);
                for (uint32_t x = 0; x < len; x++) {
                    blend[x] = sgl_color_mixer(polygon->fill_color, blend[x], polygon->alpha);
                }
            }
        }
    }
}


// Polygon construction callback function
static void sgl_polygon_construct_cb(sgl_surf_t *surf, sgl_obj_t* obj, sgl_event_t *evt)
{
    sgl_polygon_t *polygon = (sgl_polygon_t*)obj;

    if (evt->type == SGL_EVENT_DESTROYED) {
        polygon->vertex_count = 0;
        sgl_area_init(&obj->area);
        return;
    }
    
    if (evt->type != SGL_EVENT_DRAW_MAIN) {
        return;
    }
    
    if (polygon->vertex_count < 3) {
        return; // At least 3 vertices are required to form a polygon
    }

    if (!sgl_area_clip(&obj->parent->area, &obj->coords, &obj->area)) {
        sgl_area_init(&obj->area);
        return;
    }
    
    // Draw fill
    if (polygon->fill_color.full != 0) {
        sgl_area_t clip;
        if (sgl_surf_clip(surf, &obj->area, &clip)) {
            sgl_polygon_fill_scanline(surf, &clip, polygon, obj);
        }
    }
    
    // Draw border
    if (polygon->border_width > 0 && polygon->border_color.full != 0) {
        sgl_area_t border_area = obj->area;
        for (uint16_t i = 0; i < polygon->vertex_count; i++) {
            sgl_polygon_draw_border_line(surf,
                                         &border_area,
                                         obj->coords.x1 + (int16_t)polygon->vertices[i].x,
                                         obj->coords.y1 + (int16_t)polygon->vertices[i].y,
                                         obj->coords.x1 + (int16_t)polygon->vertices[(i + 1) % polygon->vertex_count].x,
                                         obj->coords.y1 + (int16_t)polygon->vertices[(i + 1) % polygon->vertex_count].y,
                                         polygon->border_width,
                                         polygon->border_color,
                                         polygon->alpha);
        }
    }

    // Draw text
    if (polygon->text && polygon->font) {
        // Calculate center point of polygon
        int32_t center_x = 0, center_y = 0;
        for (uint16_t i = 0; i < polygon->vertex_count; i++) {
            center_x += obj->coords.x1 + (int16_t)polygon->vertices[i].x;
            center_y += obj->coords.y1 + (int16_t)polygon->vertices[i].y;
        }
        center_x /= polygon->vertex_count;
        center_y /= polygon->vertex_count;
        
        // Calculate text dimensions
        int16_t text_width = sgl_font_get_string_width(polygon->text, polygon->font);
        int16_t text_height = sgl_font_get_height(polygon->font);
        
        // Text drawing position (centered)
        int16_t text_x = center_x - text_width / 2;
        int16_t text_y = center_y - text_height / 2;
        
        sgl_draw_string(surf, &obj->area, text_x, text_y, polygon->text, polygon->text_color, polygon->alpha, polygon->font);
    }
}
// Create polygon object
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
    
    // Set default values
    polygon->vertex_count = 0;
    polygon->fill_color = sgl_rgb(127, 127, 127);
    polygon->border_color = sgl_rgb(0, 0, 0);
    polygon->border_width = 1;
    polygon->alpha = SGL_ALPHA_MAX;
    polygon->pixmap = NULL;
    polygon->text = NULL;
    polygon->font = NULL;
    polygon->text_color = sgl_rgb(0, 0, 0);
    sgl_area_init(&obj->area);
    
    return obj;
}

// Set polygon vertices
void sgl_polygon_set_vertices(sgl_obj_t* obj, const sgl_polygon_pos_t* vertices, uint16_t count)
{
    sgl_polygon_t *polygon = (sgl_polygon_t *)obj;
    if (polygon == NULL || vertices == NULL || count < 3 || count > SGL_POLYGON_VERTEX_MAX) {
        return;
    }

    // Copy vertex data
    memcpy(polygon->vertices, vertices, sizeof(sgl_polygon_pos_t) * count);
    polygon->vertex_count = count;
    
    // Mark object as needing redraw
    sgl_obj_set_dirty(obj);
}

// Set polygon vertices by coordinate arrays
void sgl_polygon_set_vertex_coords(sgl_obj_t* obj, const int16_t* x_coords, const int16_t* y_coords, uint16_t count)
{
    sgl_polygon_t *polygon = (sgl_polygon_t *)obj;
    if (polygon == NULL || x_coords == NULL || y_coords == NULL || count < 3 || count > SGL_POLYGON_VERTEX_MAX) {
        return;
    }

    // Build vertices from coordinate arrays
    for (uint16_t i = 0; i < count; i++) {
        polygon->vertices[i].x = x_coords[i];
        polygon->vertices[i].y = y_coords[i];
    }
    polygon->vertex_count = count;
    
    // Mark object as needing redraw
    sgl_obj_set_dirty(obj);
}

// Set polygon vertices by 2D coordinate array
void sgl_polygon_set_vertex_array(sgl_obj_t* obj, const int16_t (*coords)[2], uint16_t count)
{
    sgl_polygon_t *polygon = (sgl_polygon_t *)obj;
    if (polygon == NULL || coords == NULL || count < 3 || count > SGL_POLYGON_VERTEX_MAX) {
        return;
    }

    // Build vertices from 2D coordinate array
    for (uint16_t i = 0; i < count; i++) {
        polygon->vertices[i].x = coords[i][0];
        polygon->vertices[i].y = coords[i][1];
    }
    polygon->vertex_count = count;
    
    // Mark object as needing redraw
    sgl_obj_set_dirty(obj);
}

// Set fill color
void sgl_polygon_set_fill_color(sgl_obj_t* obj, sgl_color_t color)
{
    sgl_polygon_t *polygon = (sgl_polygon_t *)obj;
    if (polygon == NULL) {
        return;
    }
    
    polygon->fill_color = color;
    sgl_obj_set_dirty(obj);
}

// Set border color
void sgl_polygon_set_border_color(sgl_obj_t* obj, sgl_color_t color)
{
    sgl_polygon_t *polygon = (sgl_polygon_t *)obj;
    if (polygon == NULL) {
        return;
    }
    
    polygon->border_color = color;
    sgl_obj_set_dirty(obj);
}

// Set border width
void sgl_polygon_set_border_width(sgl_obj_t* obj, uint8_t width)
{
    sgl_polygon_t *polygon = (sgl_polygon_t *)obj;
    if (polygon == NULL) {
        return;
    }
    
    polygon->border_width = width;
    sgl_obj_set_dirty(obj);
}

// Set alpha value
void sgl_polygon_set_alpha(sgl_obj_t* obj, uint8_t alpha)
{
    sgl_polygon_t *polygon = (sgl_polygon_t *)obj;
    if (polygon == NULL) {
        return;
    }
    
    polygon->alpha = alpha;
    sgl_obj_set_dirty(obj);
}

// Set background image
void sgl_polygon_set_pixmap(sgl_obj_t* obj, const sgl_pixmap_t* pixmap)
{
    sgl_polygon_t *polygon = (sgl_polygon_t *)obj;
    if (polygon == NULL) {
        return;
    }
    
    polygon->pixmap = pixmap;
    sgl_obj_set_dirty(obj);
}

// Set text
void sgl_polygon_set_text(sgl_obj_t* obj, const char* text)
{
    sgl_polygon_t *polygon = (sgl_polygon_t *)obj;
    if (polygon == NULL) {
        return;
    }
    
    polygon->text = text;
    sgl_obj_set_dirty(obj);
}

// Set font
void sgl_polygon_set_font(sgl_obj_t* obj, const sgl_font_t* font)
{
    sgl_polygon_t *polygon = (sgl_polygon_t *)obj;
    if (polygon == NULL) {
        return;
    }
    
    polygon->font = font;
    sgl_obj_set_dirty(obj);
}

// Set text color
void sgl_polygon_set_text_color(sgl_obj_t* obj, sgl_color_t color)
{
    sgl_polygon_t *polygon = (sgl_polygon_t *)obj;
    if (polygon == NULL) {
        return;
    }
    
    polygon->text_color = color;
    sgl_obj_set_dirty(obj);
}
