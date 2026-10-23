/* source/widgets/slimlabel/sgl_slimlabel.c
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
#include <string.h>
#include "sgl_slimlabel.h"

/*
 * Built-in fallback style. Every freshly created slim label points at it until
 * the caller binds its own style via sgl_slimlabel_set_style(). Because it is a
 * single shared object owned by this module, it adds no per-instance memory.
 */
static slimlabel_style_t s_default_style;
static bool              s_default_ready = false;

/**
 * @brief initialize the shared fallback style the first time it is needed
 * @param none
 * @return none
 */
static void sgl_slimlabel_default_style_init(void)
{
    if (s_default_ready) {
        return;
    }

    memset(&s_default_style, 0, sizeof(s_default_style));
    s_default_style.color     = SGL_THEME_TEXT_COLOR;
    s_default_style.bg_color  = sgl_rgb(0, 0, 0);
    s_default_style.font      = sgl_get_system_font();
    s_default_style.alpha     = SGL_ALPHA_MAX;
    s_default_style.align     = 0;
    s_default_style.bg_flag   = 0;
    s_default_style.long_mode = 0;

    s_default_ready = true;
}

/**
 * @brief construct the slimlabel object
 * @param surf pointer to the surface
 * @param obj pointer to the slimlabel object
 * @param evt pointer to the event
 * @return none
 */
static void sgl_slimlabel_construct_cb(sgl_surf_t *surf, sgl_obj_t* obj, sgl_event_t *evt)
{
    sgl_slimlabel_t *label = sgl_container_of(obj, sgl_slimlabel_t, obj);
    const slimlabel_style_t *style = label->style;

    if (evt->type == SGL_EVENT_DRAW_MAIN) {
        SGL_ASSERT(style != NULL && style->font != NULL);

        if (style->bg_flag) {
            sgl_draw_fill_rect(surf, &obj->area, &obj->coords, obj->radius, style->bg_color, style->alpha);
        }

        sgl_pos_t align_pos = sgl_get_text_pos(&obj->coords, style->font, label->text, 0,
                                               (sgl_align_type_t)style->align);

        sgl_draw_string(surf, &obj->area, align_pos.x, align_pos.y,
                        label->text, style->color, style->alpha, style->font);
    }
    /* style and text are both borrowed, so there is nothing to release on destroy */
}

/**
 * @brief create a slimlabel object
 * @param parent parent of the slimlabel
 * @return pointer to the slimlabel object
 */
sgl_obj_t* sgl_slimlabel_create(sgl_obj_t* parent)
{
    sgl_slimlabel_t *label = (sgl_slimlabel_t*)sgl_malloc(sizeof(sgl_slimlabel_t));
    if (label == NULL) {
        SGL_LOG_ERROR("sgl_slimlabel_create: malloc failed");
        return NULL;
    }

    /* set object all member to zero */
    memset(label, 0, sizeof(sgl_slimlabel_t));

    sgl_slimlabel_default_style_init();

    sgl_obj_t *obj = &label->obj;
    sgl_obj_init(obj, parent);
    obj->construct_fn = sgl_slimlabel_construct_cb;

    label->text  = "";
    label->style = (const slimlabel_style_t*)&s_default_style;
    return obj;
}

/**
 * @brief bind a style to the slimlabel
 * @param obj pointer to the slimlabel object
 * @param style pointer to the style (borrowed, typically a const object in flash)
 * @return none
 */
void sgl_slimlabel_set_style(sgl_obj_t *obj, const slimlabel_style_t *style)
{
    sgl_slimlabel_t *label = sgl_container_of(obj, sgl_slimlabel_t, obj);
    if (style == NULL || style == label->style) {
        return;
    }
    label->style = style;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the text of the slimlabel
 * @param obj pointer to the slimlabel object
 * @param text pointer to the text (not copied, must stay valid)
 * @return none
 */
void sgl_slimlabel_set_text(sgl_obj_t *obj, const char *text)
{
    sgl_slimlabel_t *label = sgl_container_of(obj, sgl_slimlabel_t, obj);
    label->text = (char*)text;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the text buffer of the slimlabel
 * @param obj pointer to the slimlabel object
 * @param buf pointer to the text buffer (not copied, must stay valid)
 * @param buf_size size of the text buffer (used only to clear the buffer)
 * @return none
 */
void sgl_slimlabel_set_text_buffer(sgl_obj_t *obj, char *buf, uint16_t buf_size)
{
    sgl_slimlabel_t *label = sgl_container_of(obj, sgl_slimlabel_t, obj);
    (void)buf_size;
    label->text = buf;
    buf[0] = '\0';
    sgl_obj_set_dirty(obj);
}

/**
 * @brief get the text of the slimlabel
 * @param obj pointer to the slimlabel object
 * @return pointer to the text
 */
char* sgl_slimlabel_get_text(sgl_obj_t *obj)
{
    sgl_slimlabel_t *label = sgl_container_of(obj, sgl_slimlabel_t, obj);
    return label->text;
}
