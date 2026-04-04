/* source/widgets/sgl_textline.c
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
#include <sgl_cfgfix.h>
#include <sgl_theme.h>
#include "sgl_textline.h"


static void sgl_textline_construct_cb(sgl_surf_t *surf, sgl_obj_t* obj, sgl_event_t *evt)
{
    sgl_textline_t *textline = sgl_container_of(obj, sgl_textline_t, obj);
    sgl_area_t text_area;
    SGL_ASSERT(textline->font != NULL);

    if(evt->type == SGL_EVENT_DRAW_MAIN) {
        obj->coords.y2 = obj->coords.y1 + (sgl_font_get_string_height(obj->coords.x2 - obj->coords.x1 + 1, textline->text, textline->font, textline->line_margin) + obj->radius * 2) - 1;
        sgl_area_clip(&obj->parent->area, &obj->coords, &obj->area);

        text_area.x1 = obj->coords.x1 + obj->radius;
        text_area.x2 = obj->coords.x2 - obj->radius;
        text_area.y1 = obj->coords.y1 + obj->radius;
        text_area.y2 = obj->coords.y2 - obj->radius;

        if (textline->bg_flag) {
            sgl_draw_fill_rect(surf, &obj->area, &obj->coords, obj->radius, textline->bg_color, textline->alpha);
        }

        sgl_draw_string_mult_line(surf, &text_area, obj->coords.x1 + obj->radius, obj->coords.y1 + obj->radius, textline->text, textline->color, textline->alpha, textline->font, textline->line_margin);
    }
}


/**
 * @brief create a textline object
 * @param parent parent of the textline
 * @return pointer to the textline object
 */
sgl_obj_t* sgl_textline_create(sgl_obj_t* parent)
{
    sgl_textline_t *textline = sgl_malloc(sizeof(sgl_textline_t));
    if(textline == NULL) {
        SGL_LOG_ERROR("sgl_textline_create: malloc failed");
        return NULL;
    }

    /* set object all member to zero */
    memset(textline, 0, sizeof(sgl_textline_t));

    sgl_obj_t *obj = &textline->obj;
    sgl_obj_init(&textline->obj, parent);
    obj->construct_fn = sgl_textline_construct_cb;

    textline->alpha = SGL_THEME_ALPHA;
    textline->bg_flag = true;
    textline->bg_color = SGL_THEME_COLOR;
    textline->color = SGL_THEME_TEXT_COLOR;
    textline->line_margin = 1;
    textline->text = "textline";
    textline->font = sgl_get_system_font();

    return obj;
}
