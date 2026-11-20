/* source/widgets/sgl_explorer.h
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

/**
 *   The explorer opens at the virtual "/" root, which lists every mounted
 *   block device by name; clicking a device enters its file system, and
 *   ".." at a device root returns to the device list.
 *
 *   The root title defaults to "DISK" and can be changed with
 *   sgl_explorer_set_root_text(); other directories show their own paths.
 *
 *   Icon mappings associate filename patterns with sgl_icon_pixmap_t images,
 *   which are drawn with sgl_draw_icon().
 */
#ifndef __SGL_EXPLORER_H__
#define __SGL_EXPLORER_H__

#include <sgl_core.h>

#ifdef __cplusplus
extern "C" {
#endif

#define  SGL_EXPLORER_ACCENT_COLOR       sgl_rgb(121, 177, 254)
#define  SGL_EXPLORER_COLOR              sgl_rgb(255, 255, 255)
#define  SGL_EXPLORER_BORDER_COLOR       sgl_rgb(0, 0, 0)
#define  SGL_EXPLORER_ALPHA              255
#define  SGL_EXPLORER_TEXT_COLOR         sgl_rgb(0, 0, 0)
#define  SGL_EXPLORER_PATH_MAX_LEN       (128)
#define  SGL_EXPLORER_NAME_MAX_LEN       (64)

/* Title bar label shown on the virtual "/" root, where the real path would
 * read as a bare "/" (see sgl_explorer_set_root_text) */
#define SGL_EXPLORER_ROOT_TEXT_MAX_LEN   (32)

/* Cache size = visible_rows * SGL_EXPLORER_CACHE_MULT.
 * 2 = one screen ahead + one screen buffer (recommended).
 * 1 = tightest memory, more re-fills when scrolling.
 * 3 = fewer re-fills, more RAM. */
#ifndef SGL_EXPLORER_CACHE_MULT
#define SGL_EXPLORER_CACHE_MULT          (2)
#endif

/* Absolute floor for cache slots (must be >= visible + a few). */
#ifndef SGL_EXPLORER_CACHE_MIN
#define SGL_EXPLORER_CACHE_MIN           (8)
#endif

typedef struct sgl_explorer_icon {
    /* Filename suffix, or "*", "..", "dir". Root device rows use "/". */
    const char *pattern;
    const sgl_icon_pixmap_t *icon;
} sgl_explorer_icon_t;

/**
 * @brief One directory entry cached in RAM.
 * @text  : file name
 * @type  : SGL_S_IFDIR or SGL_S_IFREG
 * @icon  : pre-matched icon pixmap (may be NULL)
 * @icon_w: cached width of @icon (0 if @icon is NULL)
 */
typedef struct sgl_explorer_item {
    char       text[SGL_EXPLORER_NAME_MAX_LEN];
    uint32_t   type;
    const sgl_icon_pixmap_t *icon;
    int16_t    icon_w;
} sgl_explorer_item_t;

typedef struct sgl_explorer {
    sgl_obj_t obj;
    sgl_explorer_item_t *cache_items;
    uint16_t item_capacity;
    int16_t  cache_start_index;
    uint16_t cache_count;
    int      dir_handle;
    int16_t  dir_cursor;
    sgl_explorer_item_t  selected_item;
    sgl_explorer_item_t *selected;
    int16_t item_selected;
    uint16_t item_num;
    int16_t  pending_dir_index;
    const sgl_font_t   *font;
    sgl_color_t item_text_color;
    sgl_color_t item_selected_color;
    sgl_color_t card_color;        /* idle row card background        */
    sgl_color_t card_border_color; /* idle row card border            */
    sgl_color_t sel_border_color;  /* selected row card border        */
    sgl_color_t bg_color;
    sgl_color_t border_color;
    sgl_color_t path_color;
    const sgl_explorer_icon_t *icons;
    uint8_t alpha;
    uint8_t icon_width;
    int16_t card_radius;
    uint8_t card_border_w;
    uint8_t card_hpad;
    sgl_scroll_t sc;                   /**< shared scroll physics state (sgl_misc) */
    char current_path[SGL_EXPLORER_PATH_MAX_LEN];
    char root_text[SGL_EXPLORER_ROOT_TEXT_MAX_LEN]; /* title on the "/" root */
} sgl_explorer_t;

/**
 * @brief Create an explorer object.
 * @param parent: parent object
 * @return explorer object
 */
sgl_obj_t* sgl_explorer_create(sgl_obj_t *parent);

/**
 * @brief Get the selected path of the explorer object.
 * @param obj: explorer object
 * @param buf: buffer to store the path
 * @param buf_size: size of the buffer
 * @return selected path
 */
int sgl_explorer_get_selected_path(sgl_obj_t *obj, char *buf, size_t buf_size);

/**
 * @brief Get the selected name of the explorer object.
 * @param obj: explorer object
 * @return selected name
 */
const char* sgl_explorer_get_selected_name(sgl_obj_t *obj);

/**
 * @brief Get the selected type of the explorer object.
 * @param obj: explorer object
 * @return selected type
 */
uint32_t sgl_explorer_get_selected_type(sgl_obj_t *obj);

/**
 * @brief Refresh the explorer object.
 * @param obj: explorer object
 * @return none
 */
void sgl_explorer_refresh(sgl_obj_t *obj);

/**
 * @brief Set the label shown in the title bar on the virtual "/" root
 * @param obj: explorer object
 * @param text: label to show while the device list is open (e.g. "DISK").
 *              NULL or an empty string restores the default "DISK"
 * @return none
 * @note the real path ("/") is never shown at the root; every other directory
 *       still shows its own path, exactly as before
 */
void sgl_explorer_set_root_text(sgl_obj_t *obj, const char *text);

/**
 * @brief Set the radius of the explorer object.
 * @param obj: explorer object
 * @param radius: radius to set
 * @return none
 */
void  sgl_explorer_set_radius(sgl_obj_t *obj, uint8_t radius);

/**
 * @brief Set the text color of the explorer object.
 * @param obj: explorer object
 * @param color: text color to set
 * @return none
 */
void sgl_explorer_set_text_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief Set the rounded card style of the list rows (sgl_menu-like cards)
 * @param obj: explorer object
 * @param card_color: idle row card background color
 * @param card_border_color: idle row card border color
 * @param radius: card corner radius in pixels
 * @return none
 */
void sgl_explorer_set_card_style(sgl_obj_t *obj, sgl_color_t card_color,
                                    sgl_color_t card_border_color, int16_t radius);

/**
 * @brief Set the selection highlight style of the rounded cards
 * @param obj: explorer object
 * @param sel_color: selected card background color
 * @param sel_border_color: selected card border color
 * @param border_width: selected card border width in pixels
 * @return none
 */
void sgl_explorer_set_sel_style(sgl_obj_t *obj, sgl_color_t sel_color,
                                   sgl_color_t sel_border_color, uint8_t border_width);

/**
 * @brief Set the selected color of the explorer object.
 * @param obj: explorer object
 * @param color: selected color to set
 * @return none
 */
void sgl_explorer_set_selected_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief Set the border color of the explorer object.
 * @param obj: explorer object
 * @param color: border color to set
 * @return none
 */
void sgl_explorer_set_border_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief Set the background color of the explorer object.
 * @param obj: explorer object
 * @param color: background color to set
 * @return none
 */
void sgl_explorer_set_bg_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief Set the path color of the explorer object.
 * @param obj: explorer object
 * @param color: path color to set
 * @return none
 */
void sgl_explorer_set_path_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief Set the text font of the explorer object.
 * @param obj: explorer object
 * @param font: font to set
 * @return none
 */
void sgl_explorer_set_text_font(sgl_obj_t *obj, const sgl_font_t *font);

/**
 * @brief Set the icons of the explorer object.
 * @param obj: explorer object
 * @param icons: icons to set
 * @return none
 */
void sgl_explorer_set_icons(sgl_obj_t *obj, const sgl_explorer_icon_t *icons);

/**
 * @brief Set the icon width of the explorer object.
 * @param obj: explorer object
 * @param width: icon width to set
 * @return none
 */
void sgl_explorer_set_icon_width(sgl_obj_t *obj, uint16_t width);

/**
 * @brief Set the alpha of the explorer object.
 * @param obj: explorer object
 * @param alpha: alpha to set
 * @return none
 */
void sgl_explorer_set_alpha(sgl_obj_t *obj, uint8_t alpha);

/**
 * @brief Set the border width of the explorer object.
 * @param obj: explorer object
 * @param width: border width to set
 * @return none
 */
void sgl_explorer_set_border_width(sgl_obj_t *obj, uint8_t width);

#ifdef __cplusplus
}
#endif

#endif /* __SGL_EXPLORER_H__ */
