/* source/widgets/barcode/sgl_barcode.h
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

#ifndef __SGL_BARCODE_H__
#define __SGL_BARCODE_H__

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

/* Max payload length in characters; longer strings are truncated. */
#ifndef SGL_BARCODE_TEXT_MAX_LEN
#define SGL_BARCODE_TEXT_MAX_LEN            (48)
#endif

/* Code 128 symbols: start + one symbol per character + check symbol. */
#ifndef SGL_BARCODE_SYM_MAX
#define SGL_BARCODE_SYM_MAX                 (SGL_BARCODE_TEXT_MAX_LEN + 2)
#endif

/* Recommended quiet zone, in narrow modules (ISO/IEC 15417 says 10). */
#ifndef SGL_BARCODE_QUIET_DEFAULT
#define SGL_BARCODE_QUIET_DEFAULT           (10)
#endif

/* Upper bound of the auto fitted narrow module, in pixels. */
#ifndef SGL_BARCODE_MODULE_MAX_DEFAULT
#define SGL_BARCODE_MODULE_MAX_DEFAULT      (3)
#endif

/* Gap between the bars and the human readable line, in pixels. */
#ifndef SGL_BARCODE_TEXT_GAP
#define SGL_BARCODE_TEXT_GAP                (2)
#endif

/**
 * @brief code set used to encode the payload
 */
typedef enum sgl_barcode_type {
    SGL_BARCODE_CODE128_AUTO = 0,
    SGL_BARCODE_CODE128_B,
    SGL_BARCODE_CODE128_C,
    SGL_BARCODE_TYPE_NUM,
} sgl_barcode_type_t;

/**
 * @brief horizontal placement of the symbol inside the object
 */
typedef enum sgl_barcode_align {
    SGL_BARCODE_ALIGN_CENTER = 0,
    SGL_BARCODE_ALIGN_LEFT,
    SGL_BARCODE_ALIGN_RIGHT,
    SGL_BARCODE_ALIGN_NUM,
} sgl_barcode_align_t;

/**
 * @brief barcode widget structure
 * @obj:         sgl general object
 * @text:        payload copy, drawn as the human readable line
 * @values:      encoded symbols, start code first, check code last
 * @sym_num:     number of valid entries in values
 * @modules:     total width of the symbol in narrow modules, quiet zones
 *               included, 0 when the payload is empty or invalid
 * @font:        font of the human readable line
 * @bg_color:    background color
 * @bar_color:   bar color
 * @alpha:       alpha of both the background and the bars
 * @module_px:   narrow module width, 0 = auto fit to the object width
 * @module_max:  ceiling of the auto fitted module width
 * @quiet:       quiet zone of each side, in narrow modules
 * @bar_height:  bar height in pixels, 0 = fill the available height
 * @type:        sgl_barcode_type_t
 * @align:       sgl_barcode_align_t
 * @show_text:   draw the payload below the bars
 */
typedef struct sgl_barcode {
    sgl_obj_t          obj;
    char               text[SGL_BARCODE_TEXT_MAX_LEN + 1];
    uint8_t            values[SGL_BARCODE_SYM_MAX];
    uint8_t            sym_num;
    uint16_t           modules;
    const sgl_font_t   *font;
    sgl_color_t        bg_color;
    sgl_color_t        bar_color;
    uint8_t            alpha;
    uint8_t            module_px;
    uint8_t            module_max;
    uint8_t            quiet;
    uint16_t           bar_height;
    uint8_t            type;
    uint8_t            align : 2;
    uint8_t            show_text : 1;
} sgl_barcode_t;

/**
 * @brief create a barcode object
 * @param parent parent of the barcode
 * @return barcode object
 */
sgl_obj_t* sgl_barcode_create(sgl_obj_t* parent);

/**
 * @brief set the payload to encode
 * @param obj barcode object
 * @param text payload, ASCII 32..126, longer text is truncated
 * @return none
 */
void sgl_barcode_set_text(sgl_obj_t *obj, const char *text);

/**
 * @brief get the encoded payload
 * @param obj barcode object
 * @return payload, empty string when nothing was set
 */
const char* sgl_barcode_get_text(sgl_obj_t *obj);

/**
 * @brief set the code set
 * @param obj barcode object
 * @param type SGL_BARCODE_CODE128_AUTO, _B or _C
 * @return none
 * @note re-encodes the current payload; SGL_BARCODE_CODE128_C falls back to
 *       code set B when the payload is not an even length digit string
 */
void sgl_barcode_set_type(sgl_obj_t *obj, uint8_t type);

/**
 * @brief set the narrow module width
 * @param obj barcode object
 * @param width module width in pixels, 0 to auto fit the object width
 * @return none
 */
void sgl_barcode_set_module_width(sgl_obj_t *obj, uint8_t width);

/**
 * @brief set the ceiling of the auto fitted module width
 * @param obj barcode object
 * @param width module width in pixels
 * @return none
 */
void sgl_barcode_set_module_max(sgl_obj_t *obj, uint8_t width);

/**
 * @brief set the quiet zone
 * @param obj barcode object
 * @param modules quiet zone of each side in narrow modules
 * @return none
 */
void sgl_barcode_set_quiet_zone(sgl_obj_t *obj, uint8_t modules);

/**
 * @brief set the bar height
 * @param obj barcode object
 * @param height bar height in pixels, 0 to fill the available height
 * @return none
 */
void sgl_barcode_set_bar_height(sgl_obj_t *obj, uint16_t height);

/**
 * @brief set the bar color
 * @param obj barcode object
 * @param color bar color
 * @return none
 */
void sgl_barcode_set_bar_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set the background color
 * @param obj barcode object
 * @param color background color
 * @return none
 */
void sgl_barcode_set_bg_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief set the alpha of the background and the bars
 * @param obj barcode object
 * @param alpha alpha, SGL_ALPHA_MAX is opaque
 * @return none
 */
void sgl_barcode_set_alpha(sgl_obj_t *obj, uint8_t alpha);

/**
 * @brief set whether the payload is printed below the bars
 * @param obj barcode object
 * @param show true to draw the human readable line
 * @return none
 */
void sgl_barcode_set_show_text(sgl_obj_t *obj, bool show);

/**
 * @brief set the font of the human readable line
 * @param obj barcode object
 * @param font font, NULL disables the human readable line
 * @return none
 */
void sgl_barcode_set_font(sgl_obj_t *obj, const sgl_font_t *font);

/**
 * @brief set the horizontal alignment of the symbol
 * @param obj barcode object
 * @param align SGL_BARCODE_ALIGN_CENTER, _LEFT or _RIGHT
 * @return none
 */
void sgl_barcode_set_align(sgl_obj_t *obj, uint8_t align);

/**
 * @brief get the width of the symbol in narrow modules
 * @param obj barcode object
 * @return module count including both quiet zones, 0 when nothing is encoded
 * @note multiply by the module width to know how many pixels the barcode
 *       needs, e.g. to size the object before calling sgl_barcode_set_text()
 */
uint16_t sgl_barcode_get_modules(sgl_obj_t *obj);

#ifdef __cplusplus
}
#endif

#endif /* __SGL_BARCODE_H__ */
