/* source/widgets/sgl_icon.h
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

#ifndef __SGL_ICON_H__
#define __SGL_ICON_H__

#include <sgl_core.h>
#include <sgl_draw.h>
#include <sgl_math.h>
#include <sgl_log.h>
#include <sgl_mm.h>
#include <sgl_cfgfix.h>
#include <string.h>

#define  SGL_ICON_ALPHA           255
#define  SGL_ICON_TEXT_COLOR      sgl_rgb(0, 0, 0)

typedef struct sgl_icon {
    sgl_obj_t          obj;
    const sgl_icon_pixmap_t *icon;
    sgl_color_t       color;
    uint8_t           alpha;
    uint8_t           align;
} sgl_icon_t;

/**
 * @brief create icon object
 * @param parent pointer to parent object
 * @return pointer to icon object
 */
sgl_obj_t* sgl_icon_create(sgl_obj_t* parent);

/**
 * @brief set icon color
 * @param obj pointer to icon object
 * @param color icon color
 * @return none
 */
void sgl_icon_set_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set icon alpha
 * @param obj pointer to icon object
 * @param alpha icon alpha
 * @return none
 */
void sgl_icon_set_alpha(sgl_obj_t *obj, uint8_t alpha);

/**
 * @brief set icon pixmap
 * @param obj pointer to icon object
 * @param icon pointer to icon pixmap
 * @return none
 */
void sgl_icon_set_icon(sgl_obj_t *obj, const sgl_icon_pixmap_t *icon);

/**
 * @brief set icon align
 * @param obj pointer to icon object
 * @param align icon align
 * @return none
 */
void sgl_icon_set_align(sgl_obj_t *obj, sgl_align_type_t align);

#endif // !__SGL_ICON_H__
