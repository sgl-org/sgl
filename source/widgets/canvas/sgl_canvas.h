/* source/widgets/sgl_canvas.h
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

#ifndef __SGL_CANVAS_H__
#define __SGL_CANVAS_H__

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

/**
 * @brief For example, you can use the canvas widget:
 *        void painter_func(sgl_surf_t *surf, sgl_area_t *area, sgl_obj_t *obj)
 *        {
 *            for (int i = obj->coords.y1; i < obj->coords.y2; i += 10) {
 *                sgl_draw_fill_hline(surf, area, i, obj->coords.x1, obj->coords.x2, 5, SGL_COLOR_BLACK, 255);
 *            }
 *        }
 * 
 *        sgl_obj_t *canvas = sgl_canvas_create(NULL);
 *        sgl_obj_set_size(canvas, 800, 600);
 *        sgl_obj_set_pos_align(canvas, SGL_ALIGN_CENTER);
 *        sgl_canvas_set_painter_cb(canvas, painter_func);
 */

typedef void (*sgl_painter_cb_t)(sgl_surf_t *surf, sgl_area_t *area, sgl_obj_t* obj); 

/**
 * @brief sgl canvas struct
 * @obj: sgl general object
 * @painter: pointer to canvas painter function
 * @priv: pointer to canvas private data
 */
typedef struct sgl_canvas {
    sgl_obj_t  obj;
    sgl_painter_cb_t painter;
    void       *priv;
} sgl_canvas_t;

/**
 * @brief create a canvas object
 * @param parent parent of the canvas
 * @return canvas object
 */
sgl_obj_t* sgl_canvas_create(sgl_obj_t* parent);

/**
 * @brief set canvas painter
 * @param obj canvas object
 * @param painter painter function
 */
void sgl_canvas_set_painter_cb(sgl_obj_t *obj, sgl_painter_cb_t painter);

/**
 * @brief set canvas private data
 * @param obj canvas object
 * @param priv private data
 * @param none
 */
void sgl_canvas_set_private(sgl_obj_t *obj, void *priv);

#ifdef __cplusplus
}
#endif

#endif // !__SGL_CANVAS_H__
