/* source/widgets/sgl_filedialog.h
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

/**
 *   For example, pick a file from any registered block device:
 *
 *   static void on_picked(sgl_obj_t *dialog, const char *path, void *user_data)
 *   {
 *       if (path != NULL) {
 *           SGL_LOG_INFO("picked: %s", path);   // copy the string if needed
 *       } else {
 *           SGL_LOG_INFO("cancelled");
 *       }
 *   }
 *
 *   sgl_obj_t *dlg = sgl_filedialog_open_file(NULL, NULL);  // NULL: the dialog
 *                                                           // opens on "/", which
 *                                                           // lists the devices
 *   sgl_filedialog_set_confirm_cb(dlg, on_picked, NULL);
 *
 *   // or browse one device directly:
 *   sgl_obj_t *dlg2 = sgl_filedialog_open_dir(NULL, "/sd");
 *
 *   Like the file browser, the virtual "/" root lists every mounted block
 *   device by name (with its free/total capacity); entering a device that way
 *   and stepping back up with ".." returns to the device list.
 *
 *   While the device list is open the title bar shows a label instead of the
 *   bare "/" path: "DISK" by default, or whatever
 *   sgl_filedialog_set_root_text() was given. Any other directory keeps
 *   showing its own path.
 */
#ifndef __SGL_FILEDIALOG_H__
#define __SGL_FILEDIALOG_H__

#include <sgl_core.h>
#include <sgl_block.h>
#include "../filebrowser/sgl_filebrowser.h" /* reuse sgl_filebrowser_item_t / icon table */

#ifdef __cplusplus
extern "C" {
#endif

#define  SGL_FILEDIALOG_ICON_COLOR        sgl_rgb(121, 177, 254)
#define  SGL_FILEDIALOG_BTN_COLOR         sgl_rgb(86, 156, 214)
#define  SGL_FILEDIALOG_COLOR             sgl_rgb(255, 255, 255)
#define  SGL_FILEDIALOG_BORDER_COLOR      sgl_rgb(0, 0, 0)
#define  SGL_FILEDIALOG_ALPHA             255
#define  SGL_FILEDIALOG_TEXT_COLOR        sgl_rgb(0, 0, 0)

#define SGL_FILEDIALOG_PATH_MAX_LEN       (SGL_FILEBROWSER_PATH_MAX_LEN)
#define SGL_FILEDIALOG_NAME_MAX_LEN       (SGL_FILEBROWSER_NAME_MAX_LEN)

/* Title bar label shown on the virtual "/" root, where the real path would
 * read as a bare "/" (see sgl_filedialog_set_root_text) */
#define SGL_FILEDIALOG_ROOT_TEXT_MAX_LEN  (32)

/* Cache sizing, same sliding-window scheme as the file browser. */
#ifndef SGL_FILEDIALOG_CACHE_MULT
#define SGL_FILEDIALOG_CACHE_MULT         (2)
#endif
#ifndef SGL_FILEDIALOG_CACHE_MIN
#define SGL_FILEDIALOG_CACHE_MIN          (8)
#endif

/**
 * @brief Dialog mode
 * @SGL_FILEDIALOG_OPEN_FILE: the user picks a regular file
 * @SGL_FILEDIALOG_OPEN_DIR:  the user picks a directory
 */
typedef enum {
    SGL_FILEDIALOG_OPEN_FILE = 0,
    SGL_FILEDIALOG_OPEN_DIR  = 1,
} sgl_filedialog_mode_t;

/**
 * @brief Result callback, invoked once right before the dialog destroys itself
 * @param dialog: dialog object (do NOT destroy it inside the callback)
 * @param path:   selected full VFS path, or NULL when the dialog was cancelled.
 *                The buffer is owned by the dialog and only valid during the
 *                call - copy it if you need it afterwards.
 * @param user_data: pointer passed to sgl_filedialog_set_confirm_cb()
 */
typedef void (*sgl_filedialog_cb_t)(sgl_obj_t *dialog, const char *path, void *user_data);

/**
 * @brief File dialog object (internal layout mirrors sgl_filebrowser)
 */
typedef struct sgl_filedialog {
    sgl_obj_t obj;

    /* sliding-window entry cache (same scheme as sgl_filebrowser) */
    sgl_filebrowser_item_t *cache_items;
    uint16_t item_capacity;
    int16_t  cache_start_index;
    uint16_t cache_count;
    int      dir_handle;
    int16_t  dir_cursor;
    uint16_t item_num;
    int16_t  item_selected;
    sgl_filebrowser_item_t  selected_item;
    sgl_filebrowser_item_t *selected;

    /* dialog state */
    uint8_t  mode;          /* sgl_filedialog_mode_t */
    sgl_filedialog_cb_t confirm_cb;
    void *cb_user_data;

    /* style */
    const sgl_font_t *font;
    sgl_color_t item_text_color;
    sgl_color_t item_selected_color;
    sgl_color_t card_color;        /* idle row card background        */
    sgl_color_t card_border_color; /* idle row card border            */
    sgl_color_t sel_border_color;  /* selected row card border        */
    sgl_color_t bg_color;
    sgl_color_t border_color;
    sgl_color_t icon_color;
    sgl_color_t path_color;
    sgl_color_t btn_color;
    sgl_color_t btn_disabled_color;
    const sgl_filebrowser_icon_t *icons;
    uint8_t alpha;
    uint8_t icon_width;
    int16_t card_radius;
    uint8_t card_border_w;
    uint8_t card_hpad;
    sgl_scroll_t sc;

    char current_path[SGL_FILEDIALOG_PATH_MAX_LEN];
    char root_text[SGL_FILEDIALOG_ROOT_TEXT_MAX_LEN]; /* title on the "/" root */
    char result_path[SGL_FILEDIALOG_PATH_MAX_LEN];
} sgl_filedialog_t;

/**
 * @brief Open a "pick a file" dialog
 * @param parent: parent object; NULL creates the dialog on the active screen
 * @param dev_name: registered block device name, e.g. "sd" (the leading '/'
 *        of the mount point is optional: "/sd" works too); NULL opens on the
 *        virtual "/" root, which lists the mounted block devices
 * @return dialog object, or NULL on allocation failure
 */
sgl_obj_t *sgl_filedialog_open_file(sgl_obj_t *parent, const char *dev_name);

/**
 * @brief Open a "pick a directory" dialog
 * @param parent: parent object; NULL creates the dialog on the active screen
 * @param dev_name: registered block device name; NULL opens on the virtual
 *        "/" root, which lists the mounted block devices
 * @return dialog object, or NULL on allocation failure
 */
sgl_obj_t *sgl_filedialog_open_dir(sgl_obj_t *parent, const char *dev_name);

/**
 * @brief Set the result callback (called exactly once, then the dialog closes)
 * @param obj: dialog object
 * @param cb: callback, see sgl_filedialog_cb_t
 * @param user_data: user pointer passed through to the callback
 * @return none
 */
void sgl_filedialog_set_confirm_cb(sgl_obj_t *obj, sgl_filedialog_cb_t cb, void *user_data);

/**
 * @brief Set the label shown in the title bar on the virtual "/" root
 * @param obj: dialog object
 * @param text: label to show while the device list is open (e.g. "DISK").
 *              NULL or an empty string restores the default "DISK"
 * @return none
 * @note the real path ("/") is never shown at the root; every other directory
 *       still shows its own path, exactly as before
 */
void sgl_filedialog_set_root_text(sgl_obj_t *obj, const char *text);

/**
 * @brief Set the text font of the dialog
 * @param obj: dialog object
 * @param font: font to set; it also draws the list icons, so it must contain
 *              the icon glyphs
 * @return none
 */
void sgl_filedialog_set_text_font(sgl_obj_t *obj, const sgl_font_t *font);

/**
 * @brief Set the file type icons (same table format as the file browser)
 * @param obj: dialog object
 * @param icons: icons to set
 * @return none
 */
void sgl_filedialog_set_icons(sgl_obj_t *obj, const sgl_filebrowser_icon_t *icons);

/**
 * @brief Set the icon column width of the dialog
 * @param obj: dialog object
 * @param width: icon column width in pixels (0 = one row height)
 * @return none
 */
void sgl_filedialog_set_icon_width(sgl_obj_t *obj, uint16_t width);

/**
 * @brief Set the background color of the dialog
 * @param obj: dialog object
 * @param color: color to set
 * @return none
 */
void sgl_filedialog_set_bg_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief Set the text color of the dialog entries
 * @param obj: dialog object
 * @param color: color to set
 * @return none
 */
void sgl_filedialog_set_text_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief Set the rounded card style of the list rows (sgl_menu-like cards)
 * @param obj: dialog object
 * @param card_color: idle row card background color
 * @param card_border_color: idle row card border color
 * @param radius: card corner radius in pixels
 * @return none
 */
void sgl_filedialog_set_card_style(sgl_obj_t *obj, sgl_color_t card_color,
                                   sgl_color_t card_border_color, int16_t radius);

/**
 * @brief Set the selection highlight style of the rounded cards
 * @param obj: dialog object
 * @param sel_color: selected card background color
 * @param sel_border_color: selected card border color
 * @param border_width: selected card border width in pixels
 * @return none
 */
void sgl_filedialog_set_sel_style(sgl_obj_t *obj, sgl_color_t sel_color,
                                  sgl_color_t sel_border_color, uint8_t border_width);

#ifdef __cplusplus
}
#endif

#endif /* __SGL_FILEDIALOG_H__ */
