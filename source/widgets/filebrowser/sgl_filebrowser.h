/* source/widgets/sgl_filebrowser.h
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
 *   For example, to create a file browser control with icons for different
 *   file types. The SGL_ICON_* strings below are predefined by this header
 *   (FontAwesome codepoints, drawn with the text font), so there is no need
 *   to define them again:
 *
 *   static sgl_filebrowser_icon_t filebrowser_icons[] = {
 *       { "*", SGL_ICON_NORMAL_FILE },
 *       { "..", SGL_ICON_RETURN_FILE },
 *       { "dir", SGL_ICON_DIR_FILE },
 *       { ".txt", SGL_ICON_TEXT_FILE },
 *       { ".md", SGL_ICON_TEXT_FILE },
 *       { ".ini", SGL_ICON_TEXT_FILE },
 *       { ".pdf", SGL_ICON_PDF_FILE },
 *       { ".doc", SGL_ICON_WORD_FILE },
 *       { ".docx", SGL_ICON_WORD_FILE },
 *       { ".xls", SGL_ICON_EXCEL_FILE },
 *       { ".xlsx", SGL_ICON_EXCEL_FILE },
 *       { ".ppt", SGL_ICON_PPT_FILE },
 *       { ".pptx", SGL_ICON_PPT_FILE },
 *       { ".png", SGL_ICON_IMAGE_FILE },
 *       { ".bmp", SGL_ICON_IMAGE_FILE },
 *       { ".jpg", SGL_ICON_IMAGE_FILE },
 *       { ".jpeg", SGL_ICON_IMAGE_FILE },
 *       { ".zip", SGL_ICON_ZIP_FILE },
 *       { ".rar", SGL_ICON_ZIP_FILE },
 *       { ".7z", SGL_ICON_ZIP_FILE },
 *       { ".mp3", SGL_ICON_MUSIC_FILE },
 *       { ".wav", SGL_ICON_MUSIC_FILE },
 *       { ".mp4", SGL_ICON_VIDEO_FILE },
 *       { ".avi", SGL_ICON_VIDEO_FILE },
 *       { ".c", SGL_ICON_CPP_FILE },
 *       { ".cpp", SGL_ICON_CPP_FILE },
 *       { ".h", SGL_ICON_CPP_FILE },
 *       { NULL, NULL },
 *   };
 *
 *   sgl_obj_t *filebrowser = sgl_filebrowser_create(NULL);
 *   sgl_obj_set_pos(filebrowser, 10, 30);
 *   sgl_obj_set_size(filebrowser, 240, 240);
 *   sgl_filebrowser_set_text_font(filebrowser, &consolas14);
 *   sgl_filebrowser_set_icons(filebrowser, filebrowser_icons);
 *
 *   The browser opens at the virtual "/" root, which lists every mounted
 *   block device by name; clicking a device enters its file system, and
 *   ".." at a device root returns to the device list.
 *
 *   While the device list is open the title bar shows a label instead of the
 *   bare "/" path: "DISK" by default, or whatever
 *   sgl_filebrowser_set_root_text() was given. Any other directory keeps
 *   showing its own path.
 */
#ifndef __SGL_FILEBROWSER_H__
#define __SGL_FILEBROWSER_H__

#include <sgl_core.h>

#ifdef __cplusplus
extern "C" {
#endif

#define  SGL_FILEBROWSER_ICON_COLOR         sgl_rgb(121, 177, 254)
#define  SGL_FILEBROWSER_COLOR              sgl_rgb(255, 255, 255)
#define  SGL_FILEBROWSER_BORDER_COLOR       sgl_rgb(0, 0, 0)
#define  SGL_FILEBROWSER_ALPHA              255
#define  SGL_FILEBROWSER_TEXT_COLOR         sgl_rgb(0, 0, 0)
#define  SGL_FILEBROWSER_PATH_MAX_LEN       (128)
#define  SGL_FILEBROWSER_NAME_MAX_LEN       (64)

/* Title bar label shown on the virtual "/" root, where the real path would
 * read as a bare "/" (see sgl_filebrowser_set_root_text) */
#define SGL_FILEBROWSER_ROOT_TEXT_MAX_LEN   (32)

/* Cache size = visible_rows * SGL_FILEBROWSER_CACHE_MULT.
 * 2 = one screen ahead + one screen buffer (recommended).
 * 1 = tightest memory, more re-fills when scrolling.
 * 3 = fewer re-fills, more RAM. */
#ifndef SGL_FILEBROWSER_CACHE_MULT
#define SGL_FILEBROWSER_CACHE_MULT          (2)
#endif

/* Absolute floor for cache slots (must be >= visible + a few). */
#ifndef SGL_FILEBROWSER_CACHE_MIN
#define SGL_FILEBROWSER_CACHE_MIN           (8)
#endif

typedef struct sgl_filebrowser_icon {
    const char *pattern;
    const char *icon;
} sgl_filebrowser_icon_t;

/* Ready-made icon strings for the list rows: FontAwesome free-solid codepoints
 * (UTF-8 byte escapes, no escape sequences are interpreted by the compiler).
 * They are drawn with the widget text font, so that font must contain these
 * glyphs - see the generator tool sgl_font_conv_cli. Using another icon set?
 * Redefine them before including this header (each one is guarded by #ifndef). */
#ifndef SGL_ICON_NORMAL_FILE
#define SGL_ICON_NORMAL_FILE   "\xef\x80\x96"   /* U+F016 file            */
#endif
#ifndef SGL_ICON_RETURN_FILE
#define SGL_ICON_RETURN_FILE   "\xef\x84\x92"   /* U+F112 reply ("..")    */
#endif
#ifndef SGL_ICON_TEXT_FILE
#define SGL_ICON_TEXT_FILE     "\xef\x85\x9c"   /* U+F15C file-lines      */
#endif
#ifndef SGL_ICON_DIR_FILE
#define SGL_ICON_DIR_FILE      "\xef\x81\xbc"   /* U+F07C folder-open     */
#endif
#ifndef SGL_ICON_DIR
#define SGL_ICON_DIR           "\xef\x81\xbb"   /* U+F07B folder          */
#endif
#ifndef SGL_ICON_DISK
#define SGL_ICON_DISK          "\xef\x82\xa0"   /* U+F0A0 hard-drive      */
#endif
#ifndef SGL_ICON_PDF_FILE
#define SGL_ICON_PDF_FILE      "\xef\x87\x81"   /* U+F1C1 file-pdf        */
#endif
#ifndef SGL_ICON_WORD_FILE
#define SGL_ICON_WORD_FILE     "\xef\x87\x82"   /* U+F1C2 file-word       */
#endif
#ifndef SGL_ICON_EXCEL_FILE
#define SGL_ICON_EXCEL_FILE    "\xef\x87\x83"   /* U+F1C3 file-excel      */
#endif
#ifndef SGL_ICON_PPT_FILE
#define SGL_ICON_PPT_FILE      "\xef\x87\x84"   /* U+F1C4 file-powerpoint */
#endif
#ifndef SGL_ICON_IMAGE_FILE
#define SGL_ICON_IMAGE_FILE    "\xef\x87\x85"   /* U+F1C5 file-image      */
#endif
#ifndef SGL_ICON_ZIP_FILE
#define SGL_ICON_ZIP_FILE      "\xef\x87\x86"   /* U+F1C6 file-zipper     */
#endif
#ifndef SGL_ICON_MUSIC_FILE
#define SGL_ICON_MUSIC_FILE    "\xef\x87\x87"   /* U+F1C7 file-audio      */
#endif
#ifndef SGL_ICON_VIDEO_FILE
#define SGL_ICON_VIDEO_FILE    "\xef\x87\x88"   /* U+F1C8 file-video      */
#endif
#ifndef SGL_ICON_CPP_FILE
#define SGL_ICON_CPP_FILE      "\xef\x87\x89"   /* U+F1C9 file-code       */
#endif
#ifndef SGL_ICON_PICTURE
#define SGL_ICON_PICTURE       "\xef\x80\xbe"   /* U+F03E image           */
#endif
#ifndef SGL_ICON_MUSIC
#define SGL_ICON_MUSIC         "\xef\x80\x81"   /* U+F001 music           */
#endif
#ifndef SGL_ICON_FILM
#define SGL_ICON_FILM          "\xef\x80\x88"   /* U+F008 film            */
#endif

/**
 * @brief One directory entry cached in RAM.
 * @text  : file name
 * @type  : SGL_S_IFDIR or SGL_S_IFREG
 * @icon  : pre-matched icon glyph string (may be NULL)
 * @icon_w: cached rendered width of @icon (0 if @icon is NULL)
 */
typedef struct sgl_filebrowser_item {
    char       text[SGL_FILEBROWSER_NAME_MAX_LEN];
    uint32_t   type;
    const char *icon;
    int16_t    icon_w;
} sgl_filebrowser_item_t;

typedef struct sgl_filebrowser {
    sgl_obj_t obj;
    sgl_filebrowser_item_t *cache_items;
    uint16_t item_capacity;
    int16_t  cache_start_index;
    uint16_t cache_count;
    int      dir_handle;
    int16_t  dir_cursor;
    sgl_filebrowser_item_t  selected_item;
    sgl_filebrowser_item_t *selected;
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
    sgl_color_t icon_color;
    sgl_color_t path_color;
    const sgl_filebrowser_icon_t *icons;
    uint8_t alpha;
    uint8_t icon_width;
    int16_t card_radius;
    uint8_t card_border_w;
    uint8_t card_hpad;
    sgl_scroll_t sc;                   /**< shared scroll physics state (sgl_misc) */
    char current_path[SGL_FILEBROWSER_PATH_MAX_LEN];
    char root_text[SGL_FILEBROWSER_ROOT_TEXT_MAX_LEN]; /* title on the "/" root */
} sgl_filebrowser_t;

/**
 * @brief Create a file browser object.
 * @param parent: parent object
 * @return file browser object
 */
sgl_obj_t* sgl_filebrowser_create(sgl_obj_t *parent);

/**
 * @brief Get the selected path of the file browser object.
 * @param obj: file browser object
 * @param buf: buffer to store the path
 * @param buf_size: size of the buffer
 * @return selected path
 */
int sgl_filebrowser_get_selected_path(sgl_obj_t *obj, char *buf, size_t buf_size);

/**
 * @brief Get the selected name of the file browser object.
 * @param obj: file browser object
 * @return selected name
 */
const char* sgl_filebrowser_get_selected_name(sgl_obj_t *obj);

/**
 * @brief Get the selected type of the file browser object.
 * @param obj: file browser object
 * @return selected type
 */
uint32_t sgl_filebrowser_get_selected_type(sgl_obj_t *obj);

/**
 * @brief Refresh the file browser object.
 * @param obj: file browser object
 * @return none
 */
void sgl_filebrowser_refresh(sgl_obj_t *obj);

/**
 * @brief Set the label shown in the title bar on the virtual "/" root
 * @param obj: file browser object
 * @param text: label to show while the device list is open (e.g. "DISK").
 *              NULL or an empty string restores the default "DISK"
 * @return none
 * @note the real path ("/") is never shown at the root; every other directory
 *       still shows its own path, exactly as before
 */
void sgl_filebrowser_set_root_text(sgl_obj_t *obj, const char *text);

/**
 * @brief Set the radius of the file browser object.
 * @param obj: file browser object
 * @param radius: radius to set
 * @return none
 */
void  sgl_filebrowser_set_radius(sgl_obj_t *obj, uint8_t radius);

/**
 * @brief Set the text color of the file browser object.
 * @param obj: file browser object
 * @param color: text color to set
 * @return none
 */
void sgl_filebrowser_set_text_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief Set the rounded card style of the list rows (sgl_menu-like cards)
 * @param obj: file browser object
 * @param card_color: idle row card background color
 * @param card_border_color: idle row card border color
 * @param radius: card corner radius in pixels
 * @return none
 */
void sgl_filebrowser_set_card_style(sgl_obj_t *obj, sgl_color_t card_color,
                                    sgl_color_t card_border_color, int16_t radius);

/**
 * @brief Set the selection highlight style of the rounded cards
 * @param obj: file browser object
 * @param sel_color: selected card background color
 * @param sel_border_color: selected card border color
 * @param border_width: selected card border width in pixels
 * @return none
 */
void sgl_filebrowser_set_sel_style(sgl_obj_t *obj, sgl_color_t sel_color,
                                   sgl_color_t sel_border_color, uint8_t border_width);

/**
 * @brief Set the icon color of the file browser object.
 * @param obj: file browser object
 * @param color: icon color to set
 * @return none
 */
void sgl_filebrowser_set_icon_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief Set the selected color of the file browser object.
 * @param obj: file browser object
 * @param color: selected color to set
 * @return none
 */
void sgl_filebrowser_set_selected_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief Set the border color of the file browser object.
 * @param obj: file browser object
 * @param color: border color to set
 * @return none
 */
void sgl_filebrowser_set_border_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief Set the background color of the file browser object.
 * @param obj: file browser object
 * @param color: background color to set
 * @return none
 */
void sgl_filebrowser_set_bg_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief Set the path color of the file browser object.
 * @param obj: file browser object
 * @param color: path color to set
 * @return none
 */
void sgl_filebrowser_set_path_color(sgl_obj_t *obj, sgl_color_t color);

/**
 * @brief Set the text font of the file browser object.
 * @param obj: file browser object
 * @param font: font to set; it also draws the list icons, so it must contain
 *              the icon glyphs
 * @return none
 */
void sgl_filebrowser_set_text_font(sgl_obj_t *obj, const sgl_font_t *font);

/**
 * @brief Set the text icons of the file browser object.
 * @param obj: file browser object
 * @param icons: icons to set
 * @return none
 */
void sgl_filebrowser_set_icons(sgl_obj_t *obj, const sgl_filebrowser_icon_t *icons);

/**
 * @brief Set the icon width of the file browser object.
 * @param obj: file browser object
 * @param width: icon width to set
 * @return none
 */
void sgl_filebrowser_set_icon_width(sgl_obj_t *obj, uint16_t width);

/**
 * @brief Set the alpha of the file browser object.
 * @param obj: file browser object
 * @param alpha: alpha to set
 * @return none
 */
void sgl_filebrowser_set_alpha(sgl_obj_t *obj, uint8_t alpha);

/**
 * @brief Set the border width of the file browser object.
 * @param obj: file browser object
 * @param width: border width to set
 * @return none
 */
void sgl_filebrowser_set_border_width(sgl_obj_t *obj, uint8_t width);

#ifdef __cplusplus
}
#endif

#endif /* __SGL_FILEBROWSER_H__ */
