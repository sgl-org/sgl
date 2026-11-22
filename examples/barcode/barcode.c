/* examples/barcode.c
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

#include <sgl.h>

/**
 * Barcode widget examples (Code 128, ISO/IEC 15417):
 *  1. mixed payload: AUTO picks code set B, bars plus human readable line
 *  2. same information as digits: AUTO picks code set C, about half the width
 *  3. explicit 1px module, quiet zone 4, left aligned, no readable line
 *  4. inverted colors, module width capped at 2px, short bars
 *  5. live re-encode: click it to cycle the payload, the module width is
 *     fitted to the new payload on the fly
 *
 * The demo is laid out for a 800x480 panel: three full width rows and one row
 * split in two halves, each barcode captioned with the width it encodes to.
 */

/* Payloads cycled by example 5; every one of them fits a 380px wide object,
 * the narrowest module being 1px. */
#define BARCODE_LIVE_PAYLOAD_NUM      (4)
static const char *const barcode_live_payloads[BARCODE_LIVE_PAYLOAD_NUM] = {
    "SN-20261024-A17",
    "202610240830",
    "sgl-docs.readthedocs.io",
    "123456",
};

/* Index of the payload currently shown by example 5. */
static uint8_t barcode_live_index;

/* Caption of example 5, refreshed whenever the payload changes. */
static sgl_obj_t *barcode_live_caption;

/**
 * @brief put a caption line above a barcode
 * @param parent parent of the caption, the same as the barcode's
 * @param bc barcode the caption belongs to
 * @param x caption left edge
 * @param y caption top edge
 * @param width caption width
 * @param text caption text, the encoded module count is appended
 * @return the caption label
 */
static sgl_obj_t* barcode_caption(sgl_obj_t *parent, sgl_obj_t *bc, int16_t x, int16_t y,
                                  int16_t width, const char *text)
{
    sgl_obj_t *label;

    label = sgl_label_create(parent);
    sgl_obj_set_pos(label, x, y);
    sgl_obj_set_size(label, width, 16);
    sgl_label_set_font(label, &consolas14);
    sgl_label_set_text_color(label, SGL_COLOR_GRAY);
    /* the module count is what all the sizing options come down to, so the
     * caption reads it back from the widget instead of hard coding it */
    sgl_label_set_text_fmt_dynamic(label, "%s  %d modules", text,
                                   (int)sgl_barcode_get_modules(bc));

    return label;
}

/**
 * @brief click callback of example 5, encodes the next payload
 * @param e event structure, e->obj is the barcode
 * @return none
 */
static void barcode_live_cb(sgl_event_t *e)
{
    if (e->type != SGL_EVENT_CLICKED) {
        return;
    }

    barcode_live_index = (uint8_t)((barcode_live_index + 1) % BARCODE_LIVE_PAYLOAD_NUM);
    sgl_barcode_set_text(e->obj, barcode_live_payloads[barcode_live_index]);

    if (barcode_live_caption != NULL) {
        sgl_label_set_text_fmt_dynamic(barcode_live_caption, "%s  %d modules",
                                       "5. click to re-encode",
                                       (int)sgl_barcode_get_modules(e->obj));
    }
}

/**
 * @brief create the barcode examples
 * @param parent parent object, NULL creates the barcodes on the active screen
 * @return none
 */
void sgl_barcode_examples(sgl_obj_t *parent)
{
    sgl_obj_t *bc;

    /* example 1: mixed payload, so AUTO falls back to code set B. The module
     * is grown to the object width but stays under the default ceiling of
     * 3px, which keeps the barcode scannable at a normal viewing distance. */
    bc = sgl_barcode_create(parent);
    sgl_obj_set_pos(bc, 10, 24);
    sgl_obj_set_size(bc, 780, 96);
    sgl_barcode_set_font(bc, &consolas14);
    sgl_barcode_set_show_text(bc, true);
    sgl_barcode_set_text(bc, "SGL-BARCODE-DEMO");
    barcode_caption(parent, bc, 14, 6, 420, "1. code 128B, auto fit, bars + text");

    /* example 2: the digits of example 1 alone, code set C packs two
     * characters per symbol, so the very same widget needs far fewer
     * modules and AUTO picks it without being told. */
    bc = sgl_barcode_create(parent);
    sgl_obj_set_pos(bc, 10, 142);
    sgl_obj_set_size(bc, 780, 96);
    sgl_barcode_set_font(bc, &consolas14);
    sgl_barcode_set_show_text(bc, true);
    sgl_barcode_set_text(bc, "202610240830");
    barcode_caption(parent, bc, 14, 124, 420, "2. code 128C, digit pairs are denser");

    /* example 3: a fixed 1px module for a dense code printed next to its
     * caption, the symbol hugs the left edge and the quiet zone is reduced
     * to 4 modules per side. */
    bc = sgl_barcode_create(parent);
    sgl_obj_set_pos(bc, 10, 260);
    sgl_obj_set_size(bc, 780, 84);
    sgl_barcode_set_module_width(bc, 1);
    sgl_barcode_set_align(bc, SGL_BARCODE_ALIGN_LEFT);
    sgl_barcode_set_quiet_zone(bc, 4);
    /* colors are free to choose, but a scanner reads contrast, so stay on a
     * dark bar over a light background. */
    sgl_barcode_set_bar_color(bc, sgl_rgb(16, 48, 96));
    sgl_barcode_set_text(bc, "ISO/IEC 15417");
    barcode_caption(parent, bc, 14, 242, 420, "3. 1px module, quiet zone 4, left");

    /* example 4: inverted palette for a dark UI. The auto fitted module is
     * capped at 2px and the bars stop short of the readable line. */
    bc = sgl_barcode_create(parent);
    sgl_obj_set_pos(bc, 10, 378);
    sgl_obj_set_size(bc, 380, 90);
    sgl_barcode_set_bg_color(bc, sgl_rgb(28, 30, 34));
    sgl_barcode_set_bar_color(bc, SGL_COLOR_WHITE);
    sgl_barcode_set_module_max(bc, 2);
    sgl_barcode_set_bar_height(bc, 56);
    sgl_barcode_set_font(bc, &consolas14);
    sgl_barcode_set_show_text(bc, true);
    sgl_barcode_set_text(bc, "BARCODE-CTRL");
    barcode_caption(parent, bc, 14, 360, 370, "4. inverted, max 2px");

    /* example 5: clickable. Each click re-encodes another payload and the
     * widget refits the module width: the digit payload grows the bars, the
     * long URL shrinks them, nothing else is touched. */
    bc = sgl_barcode_create(parent);
    sgl_obj_set_pos(bc, 410, 378);
    sgl_obj_set_size(bc, 380, 90);
    sgl_barcode_set_font(bc, &consolas14);
    sgl_barcode_set_show_text(bc, true);
    sgl_barcode_set_text(bc, barcode_live_payloads[0]);
    barcode_live_caption = barcode_caption(parent, bc, 414, 360, 370, "5. click me");
    sgl_obj_set_event_cb(bc, barcode_live_cb, NULL);
}
