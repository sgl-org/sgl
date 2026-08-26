/* source/widgets/sgl_canvas.c
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
#include <sgl_theme.h>
#include <sgl_cfgfix.h>
#include <string.h>
#include "sgl_canvas.h"

static void sgl_canvas_construct_cb(sgl_surf_t *surf, sgl_obj_t* obj, sgl_event_t *evt)
{
    sgl_canvas_t *canvas = sgl_container_of(obj, sgl_canvas_t, obj);

    if(evt->type == SGL_EVENT_DRAW_MAIN) {
        SGL_ASSERT_INFO(canvas->painter != NULL, "you must set painter function");
        canvas->painter(surf, &obj->area, obj);
    }
}

/**
 * @brief create a canvas object
 * @param parent parent of the canvas
 * @return canvas object
 */
sgl_obj_t* sgl_canvas_create(sgl_obj_t* parent)
{
    sgl_canvas_t *canvas = sgl_malloc(sizeof(sgl_canvas_t));
    if(canvas == NULL) {
        SGL_LOG_ERROR("sgl_canvas_create: malloc failed");
        return NULL;
    }

    /* set object all member to zero */
    memset(canvas, 0, sizeof(sgl_canvas_t));

    sgl_obj_t *obj = &canvas->obj;
    sgl_obj_init(&canvas->obj, parent);
    obj->construct_fn = sgl_canvas_construct_cb;

    return obj;
}

/**
 * @brief set canvas painter
 * @param obj canvas object
 * @param painter painter function
 */
void sgl_canvas_set_painter_cb(sgl_obj_t *obj, sgl_painter_cb_t painter)
{
    SGL_ASSERT(obj != NULL);
    sgl_canvas_t *canvas = sgl_container_of(obj, sgl_canvas_t, obj);
    canvas->painter = painter;
}

/**
 * @brief set canvas private data
 * @param obj canvas object
 * @param priv private data
 * @param none
 */
void sgl_canvas_set_private(sgl_obj_t *obj, void *priv)
{
    SGL_ASSERT(obj != NULL);
    sgl_canvas_t *canvas = (sgl_canvas_t *)obj;
    canvas->priv = priv;
}
