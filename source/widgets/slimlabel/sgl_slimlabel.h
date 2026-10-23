/* source/widgets/slimlabel/sgl_slimlabel.h
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

#ifndef __SGL_SLIMLABEL_H__
#define __SGL_SLIMLABEL_H__

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

/*
 * A RAM-lean label.
 *
 * Unlike the full `sgl_label_t`, which keeps every visual property (font,
 * colors, alpha, alignment, background flag...) as per-instance fields, the
 * slim label stores only a borrowed pointer to a `slimlabel_style_t`. The style
 * is owned by the caller and is normally a `const` object placed in flash, so
 * many labels can share a single style with zero per-instance style memory.
 *
 * The whole public surface is intentionally minimal: a single `set_style` to
 * (re)bind a style, plus the text accessors. The `text` member is also a
 * borrowed pointer: the caller must keep the string alive for the lifetime of
 * the label.
 */

/**
 * @brief visual style shared by slim labels
 */
typedef struct slimlabel_style {
    sgl_color_t color;
    sgl_color_t bg_color;
    const sgl_font_t *font;
    uint8_t alpha;
    uint8_t align: 5;
    uint8_t bg_flag : 1;
    uint8_t long_mode : 1;
} slimlabel_style_t;

/**
 * @brief sgl slimlabel object
 * @obj: sgl general object
 * @text: borrowed pointer to the displayed text
 * @style: borrowed pointer to the visual style
 */
typedef struct sgl_slimlabel {
    sgl_obj_t        obj;
    char             *text;
    const slimlabel_style_t *style;
} sgl_slimlabel_t;

/**
 * @brief create a slimlabel object
 * @param parent parent of the slimlabel
 * @return pointer to the slimlabel object
 */
sgl_obj_t* sgl_slimlabel_create(sgl_obj_t* parent);

/**
 * @brief bind a style to the slimlabel
 * @param obj pointer to the slimlabel object
 * @param style pointer to the style (borrowed, typically a const object in flash)
 * @return none
 * @note the style must stay valid for the lifetime of the label
 */
void sgl_slimlabel_set_style(sgl_obj_t *obj, const slimlabel_style_t *style);

/**
 * @brief set the text of the slimlabel
 * @param obj pointer to the slimlabel object
 * @param text pointer to the text (must stay valid, it is not copied)
 * @return none
 */
void sgl_slimlabel_set_text(sgl_obj_t *obj, const char *text);

/**
 * @brief set the text buffer of the slimlabel
 * @param obj pointer to the slimlabel object
 * @param buf pointer to the text buffer (must stay valid, it is not copied)
 * @param buf_size size of the text buffer (used only to clear the buffer)
 * @return none
 */
void sgl_slimlabel_set_text_buffer(sgl_obj_t *obj, char *buf, uint16_t buf_size);

/**
 * @brief get the text of the slimlabel
 * @param obj pointer to the slimlabel object
 * @return pointer to the text
 */
char* sgl_slimlabel_get_text(sgl_obj_t *obj);

#ifdef __cplusplus
}
#endif

#endif // !__SGL_SLIMLABEL_H__
