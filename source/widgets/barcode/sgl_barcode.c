/* source/widgets/barcode/sgl_barcode.c
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
#include <string.h>
#include "sgl_barcode.h"

/*
 * Code 128 (ISO/IEC 15417) symbol widths, six elements per value:
 * bar, space, bar, space, bar, space. Each entry sums up to the 11 modules of
 * one symbol. Index is the symbol value, so value v starts at v * 6, values
 * 103..105 being START A/B/C. The stop symbol is not part of the table: its
 * pattern is the only one with seven elements (13 modules).
 */
static const char code128_tab[] =
    "212222" "222122" "222221" "121223" "121322" "131222" "122213" "122312"  /*   0..  7 */
    "132212" "221213" "221312" "231212" "112232" "122132" "122231" "113222"  /*   8.. 15 */
    "123122" "123221" "223211" "221132" "221231" "213212" "223112" "312131"  /*  16.. 23 */
    "311222" "321122" "321221" "312212" "322112" "322211" "212123" "212321"  /*  24.. 31 */
    "232121" "111323" "131123" "131321" "112313" "132113" "132311" "211313"  /*  32.. 39 */
    "231113" "231311" "112133" "112331" "132131" "113123" "113321" "133121"  /*  40.. 47 */
    "313121" "211331" "231131" "213113" "213311" "213131" "311123" "311321"  /*  48.. 55 */
    "331121" "312113" "312311" "332111" "314111" "221411" "431111" "111224"  /*  56.. 63 */
    "111422" "121124" "121421" "141122" "141221" "112214" "112412" "122114"  /*  64.. 71 */
    "122411" "142112" "142211" "241211" "221114" "413111" "241112" "134111"  /*  72.. 79 */
    "111242" "121142" "121241" "114212" "124112" "124211" "411212" "421112"  /*  80.. 87 */
    "421211" "212141" "214121" "412121" "111143" "111341" "131141" "114113"  /*  88.. 95 */
    "114311" "411113" "411311" "113141" "114131" "311141" "411131" "211412"  /*  96..103 */
    "211214" "211232";                                                       /* 104..105 */

#define CODE128_STOP_PATTERN                "2331112"
#define CODE128_SYM_MODULES                 (11)
#define CODE128_STOP_MODULES                (13)
#define CODE128_START_A                     (103)
#define CODE128_START_B                     (104)
#define CODE128_START_C                     (105)

/* elements of a regular symbol, and of the stop symbol */
#define CODE128_SYM_ELEMENTS                (6)
#define CODE128_STOP_ELEMENTS               (7)

/**
 * @brief tell whether the payload is a non empty even length digit string,
 *        the only form code set C can encode
 */
static bool code128_is_code_c_friendly(const char *text, uint16_t len)
{
    uint16_t i;

    if (len == 0 || (len & 1) || len > SGL_BARCODE_TEXT_MAX_LEN) {
        return false;
    }

    for (i = 0; i < len; i++) {
        if (text[i] < '0' || text[i] > '9') {
            return false;
        }
    }

    return true;
}

/**
 * @brief tell whether every character of the payload fits code set B
 */
static bool code128_is_code_b_friendly(const char *text, uint16_t len)
{
    uint16_t i;

    if (len == 0 || len > SGL_BARCODE_TEXT_MAX_LEN) {
        return false;
    }

    for (i = 0; i < len; i++) {
        if (text[i] < ' ' || (uint8_t)text[i] > 127) {
            return false;
        }
    }

    return true;
}

/**
 * @brief encode the payload of the widget into symbol values
 * @param bc barcode widget
 * @return none
 * @note values[] holds START, the data symbols and the check symbol; the stop
 *       symbol is drawn from its own pattern, modules holds the full width
 */
static void barcode_encode(sgl_barcode_t *bc)
{
    uint16_t len = (uint16_t)strlen(bc->text);
    uint16_t sum;
    uint16_t i;
    uint8_t sym = 0;
    bool use_c;

    bc->sym_num = 0;
    bc->modules = 0;

    if (len == 0) {
        return;
    }

    use_c = code128_is_code_c_friendly(bc->text, len);
    if (bc->type == SGL_BARCODE_CODE128_B) {
        use_c = false;
    }

    if (use_c) {
        bc->values[sym++] = CODE128_START_C;
        for (i = 0; i < len; i += 2) {
            bc->values[sym++] = (uint8_t)((bc->text[i] - '0') * 10 + (bc->text[i + 1] - '0'));
        }
    }
    else {
        if (!code128_is_code_b_friendly(bc->text, len)) {
            SGL_LOG_ERROR("sgl_barcode: payload not encodable, ASCII 32..127 only");
            return;
        }
        bc->values[sym++] = CODE128_START_B;
        for (i = 0; i < len; i++) {
            bc->values[sym++] = (uint8_t)(bc->text[i] - ' ');
        }
    }

    /* check symbol: start value plus position weighted data values */
    sum = bc->values[0];
    for (i = 1; i < sym; i++) {
        sum += (uint16_t)(i * bc->values[i]);
    }
    bc->values[sym++] = (uint8_t)(sum % 103);

    bc->sym_num = sym;
    bc->modules = (uint16_t)sym * CODE128_SYM_MODULES + CODE128_STOP_MODULES
                  + (uint16_t)bc->quiet * 2;
}

/**
 * @brief resolve the narrow module width in pixels
 * @param bc barcode widget
 * @param obj_w width of the object in pixels
 * @return module width, at least 1 pixel
 * @note an explicit width is clamped down when it does not fit, a barcode cut
 *       by its own bounding box cannot be scanned
 */
static uint8_t barcode_module_width(sgl_barcode_t *bc, int16_t obj_w)
{
    uint8_t width;

    if (bc->module_px) {
        width = bc->module_px;
    }
    else {
        width = (uint8_t)(obj_w / bc->modules);
        if (width > bc->module_max) {
            width = bc->module_max;
        }
    }

    while (width > 1 && (int32_t)width * bc->modules > obj_w) {
        width --;
    }

    return width ? width : 1;
}

/**
 * @brief draw one symbol out of its width pattern
 * @param surf surface to draw on
 * @param area clipping area of the object
 * @param rect drawing box, x2 is updated to the end of the symbol
 * @param pattern element widths, alternating bar and space, starting with a bar
 * @param elements number of entries of the pattern
 * @param module narrow module width in pixels
 * @param y1 top of the bars
 * @param y2 bottom of the bars
 * @param color bar color
 * @param alpha bar alpha
 * @return none
 */
static void barcode_draw_symbol(sgl_surf_t *surf, sgl_area_t *area, sgl_area_t *rect,
                                const char *pattern, uint8_t elements, uint8_t module,
                                int16_t y1, int16_t y2, sgl_color_t color, uint8_t alpha)
{
    uint8_t i;
    int16_t w;

    for (i = 0; i < elements; i++) {
        w = (int16_t)(pattern[i] - '0') * module;
        if ((i & 1) == 0) {
            rect->x2 = rect->x1 + w - 1;
            rect->y1 = y1;
            rect->y2 = y2;
            sgl_draw_fill_rect(surf, area, rect, 0, color, alpha);
        }
        rect->x1 += w;
    }
}

static void sgl_barcode_construct_cb(sgl_surf_t *surf, sgl_obj_t *obj, sgl_event_t *evt)
{
    sgl_barcode_t *bc = sgl_container_of(obj, sgl_barcode_t, obj);
    int16_t obj_w = sgl_obj_get_width(obj);
    int16_t obj_h = sgl_obj_get_height(obj);
    int16_t text_h, bar_h, width, x, y1, y2;
    uint8_t module, i;
    sgl_area_t rect;

    if (evt->type != SGL_EVENT_DRAW_MAIN) {
        return;
    }

    /* background */
    sgl_draw_fill_rect(surf, &obj->area, &obj->coords, obj->radius, bc->bg_color, bc->alpha);

    if (bc->modules == 0 || obj_w <= 0 || obj_h <= 0) {
        return;
    }

    /* vertical split between the bars and the human readable line */
    text_h = (bc->show_text && bc->font) ? sgl_font_get_height(bc->font) : 0;
    if (text_h) {
        text_h += SGL_BARCODE_TEXT_GAP;
    }
    bar_h = bc->bar_height ? sgl_min(bc->bar_height, obj_h - text_h) : obj_h - text_h;
    if (bar_h < 1) {
        return;
    }

    /* horizontal placement of the symbol */
    module = barcode_module_width(bc, obj_w);
    width = (int16_t)module * bc->modules;
    if (width > obj_w) {
        width = obj_w;
    }
    if (bc->align == SGL_BARCODE_ALIGN_LEFT) {
        x = obj->coords.x1;
    }
    else if (bc->align == SGL_BARCODE_ALIGN_RIGHT) {
        x = obj->coords.x2 - width + 1;
    }
    else {
        x = obj->coords.x1 + (obj_w - width) / 2;
    }

    y1 = obj->coords.y1;
    y2 = y1 + bar_h - 1;
    rect.x1 = x + (int16_t)module * bc->quiet;   /* left quiet zone */
    rect.y1 = y1;
    rect.y2 = y2;

    for (i = 0; i < bc->sym_num; i++) {
        barcode_draw_symbol(surf, &obj->area, &rect, &code128_tab[bc->values[i] * CODE128_SYM_ELEMENTS],
                            CODE128_SYM_ELEMENTS, module, y1, y2, bc->bar_color, bc->alpha);
    }
    barcode_draw_symbol(surf, &obj->area, &rect, CODE128_STOP_PATTERN,
                        CODE128_STOP_ELEMENTS, module, y1, y2, bc->bar_color, bc->alpha);

    /* human readable payload, centered on the symbol */
    if (text_h && bc->font) {
        int16_t text_w = (int16_t)sgl_font_get_string_width(bc->text, bc->font);
        int16_t tx = x + (width - text_w) / 2;
        int16_t ty = y2 + 1 + SGL_BARCODE_TEXT_GAP;

        if (tx < obj->coords.x1) {
            tx = obj->coords.x1;
        }
        if (tx + text_w > obj->coords.x2 + 1) {
            tx = obj->coords.x2 + 1 - text_w;
        }
        sgl_draw_string(surf, &obj->area, tx, ty, bc->text, bc->bar_color, bc->alpha, bc->font);
    }
}

/**
 * @brief create a barcode object
 * @param parent parent of the barcode
 * @return barcode object
 */
sgl_obj_t* sgl_barcode_create(sgl_obj_t* parent)
{
    sgl_barcode_t *bc = sgl_malloc(sizeof(sgl_barcode_t));
    sgl_obj_t *obj;

    if (bc == NULL) {
        SGL_LOG_ERROR("sgl_barcode_create: malloc failed");
        return NULL;
    }

    /* set object all member to zero */
    memset(bc, 0, sizeof(sgl_barcode_t));

    obj = &bc->obj;
    sgl_obj_init(obj, parent);
    sgl_obj_set_radius(obj, 0);
    sgl_obj_set_border_width(obj, 0);
    obj->construct_fn = sgl_barcode_construct_cb;

    bc->alpha = SGL_ALPHA_MAX;
    bc->bg_color = SGL_COLOR_WHITE;
    bc->bar_color = SGL_COLOR_BLACK;
    bc->type = SGL_BARCODE_CODE128_AUTO;
    bc->align = SGL_BARCODE_ALIGN_CENTER;
    bc->quiet = SGL_BARCODE_QUIET_DEFAULT;
    bc->module_max = SGL_BARCODE_MODULE_MAX_DEFAULT;

    return obj;
}

/**
 * @brief set the payload to encode
 * @param obj barcode object
 * @param text payload, ASCII 32..126, longer text is truncated
 * @return none
 */
void sgl_barcode_set_text(sgl_obj_t *obj, const char *text)
{
    sgl_barcode_t *bc = sgl_container_of(obj, sgl_barcode_t, obj);

    if (text == NULL) {
        bc->text[0] = '\0';
    }
    else {
        size_t len = strlen(text);
        if (len > SGL_BARCODE_TEXT_MAX_LEN) {
            len = SGL_BARCODE_TEXT_MAX_LEN;
        }
        memcpy(bc->text, text, len);
        bc->text[len] = '\0';
    }

    barcode_encode(bc);
    sgl_obj_set_dirty(obj);
}

/**
 * @brief get the encoded payload
 * @param obj barcode object
 * @return payload, empty string when nothing was set
 */
const char* sgl_barcode_get_text(sgl_obj_t *obj)
{
    sgl_barcode_t *bc = sgl_container_of(obj, sgl_barcode_t, obj);

    return bc->text;
}

/**
 * @brief set the code set
 * @param obj barcode object
 * @param type SGL_BARCODE_CODE128_AUTO, _B or _C
 * @return none
 */
void sgl_barcode_set_type(sgl_obj_t *obj, uint8_t type)
{
    sgl_barcode_t *bc = sgl_container_of(obj, sgl_barcode_t, obj);

    if (type >= SGL_BARCODE_TYPE_NUM || bc->type == type) {
        return;
    }

    bc->type = type;
    barcode_encode(bc);
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the narrow module width
 * @param obj barcode object
 * @param width module width in pixels, 0 to auto fit the object width
 * @return none
 */
void sgl_barcode_set_module_width(sgl_obj_t *obj, uint8_t width)
{
    sgl_barcode_t *bc = sgl_container_of(obj, sgl_barcode_t, obj);

    bc->module_px = width;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the ceiling of the auto fitted module width
 * @param obj barcode object
 * @param width module width in pixels
 * @return none
 */
void sgl_barcode_set_module_max(sgl_obj_t *obj, uint8_t width)
{
    sgl_barcode_t *bc = sgl_container_of(obj, sgl_barcode_t, obj);

    bc->module_max = width ? width : 1;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the quiet zone
 * @param obj barcode object
 * @param modules quiet zone of each side in narrow modules
 * @return none
 */
void sgl_barcode_set_quiet_zone(sgl_obj_t *obj, uint8_t modules)
{
    sgl_barcode_t *bc = sgl_container_of(obj, sgl_barcode_t, obj);

    bc->quiet = modules;
    barcode_encode(bc);
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the bar height
 * @param obj barcode object
 * @param height bar height in pixels, 0 to fill the available height
 * @return none
 */
void sgl_barcode_set_bar_height(sgl_obj_t *obj, uint16_t height)
{
    sgl_barcode_t *bc = sgl_container_of(obj, sgl_barcode_t, obj);

    bc->bar_height = height;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the bar color
 * @param obj barcode object
 * @param color bar color
 * @return none
 */
void sgl_barcode_set_bar_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_barcode_t *bc = sgl_container_of(obj, sgl_barcode_t, obj);

    bc->bar_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the background color
 * @param obj barcode object
 * @param color background color
 * @return none
 */
void sgl_barcode_set_bg_color(sgl_obj_t *obj, sgl_color_t color)
{
    sgl_barcode_t *bc = sgl_container_of(obj, sgl_barcode_t, obj);

    bc->bg_color = color;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the alpha of the background and the bars
 * @param obj barcode object
 * @param alpha alpha, SGL_ALPHA_MAX is opaque
 * @return none
 */
void sgl_barcode_set_alpha(sgl_obj_t *obj, uint8_t alpha)
{
    sgl_barcode_t *bc = sgl_container_of(obj, sgl_barcode_t, obj);

    bc->alpha = alpha;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set whether the payload is printed below the bars
 * @param obj barcode object
 * @param show true to draw the human readable line
 * @return none
 */
void sgl_barcode_set_show_text(sgl_obj_t *obj, bool show)
{
    sgl_barcode_t *bc = sgl_container_of(obj, sgl_barcode_t, obj);

    bc->show_text = show ? 1 : 0;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the font of the human readable line
 * @param obj barcode object
 * @param font font, NULL disables the human readable line
 * @return none
 */
void sgl_barcode_set_font(sgl_obj_t *obj, const sgl_font_t *font)
{
    sgl_barcode_t *bc = sgl_container_of(obj, sgl_barcode_t, obj);

    bc->font = font;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief set the horizontal alignment of the symbol
 * @param obj barcode object
 * @param align SGL_BARCODE_ALIGN_CENTER, _LEFT or _RIGHT
 * @return none
 */
void sgl_barcode_set_align(sgl_obj_t *obj, uint8_t align)
{
    sgl_barcode_t *bc = sgl_container_of(obj, sgl_barcode_t, obj);

    if (align >= SGL_BARCODE_ALIGN_NUM) {
        return;
    }

    bc->align = align;
    sgl_obj_set_dirty(obj);
}

/**
 * @brief get the width of the symbol in narrow modules
 * @param obj barcode object
 * @return module count including both quiet zones, 0 when nothing is encoded
 */
uint16_t sgl_barcode_get_modules(sgl_obj_t *obj)
{
    sgl_barcode_t *bc = sgl_container_of(obj, sgl_barcode_t, obj);

    return bc->modules;
}

8808422