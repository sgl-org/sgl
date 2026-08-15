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
 *   For example, to create a file browser control with custom icons for different file types:
 *
 *   #define SGL_ICON_NORNAL_FILE          "\xef\x80\x96"  // unicode 0XF016
 *   #define SGL_ICON_RETURN_FILE          "\xef\x84\x92"  // unicode 0XF112
 *   #define SGL_ICON_TEXT_FILE            "\xef\x83\xb6"  // unicode 0XF0F6
 *   #define SGL_ICON_DIR_FILE             "\xef\x81\xbc"  // unicode 0XF07C
 *   #define SGL_ICON_PDF_FILE             "\xef\x87\x81"  // unicode 0XF1C1
 *   #define SGL_ICON_WORD_FILE            "\xef\x87\x82"  // unicode 0XF1C2
 *   #define SGL_ICON_EXCEL_FILE           "\xef\x87\x83"  // unicode 0XF1C3
 *   #define SGL_ICON_PPT_FILE             "\xef\x87\x84"  // unicode 0XF1C4
 *   #define SGL_ICON_IMAGE_FILE           "\xef\x87\x85"  // unicode 0XF1C5
 *   #define SGL_ICON_ZIP_FILE             "\xef\x87\x86"  // unicode 0XF1C6
 *   #define SGL_ICON_MUSIC_FILE           "\xef\x87\x87"  // unicode 0XF1C7
 *   #define SGL_ICON_VIDEO_FILE           "\xef\x87\x88"  // unicode 0XF1C8
 *   #define SGL_ICON_CPP_FILE             "\xef\x87\x89"  // unicode 0XF1C9
 *   #define SGL_ICON_DISK                 "\xef\x9f\x82"  // unicode 0XF7C2

 *   static sgl_filebrowser_icon_t filebrowser_icons[] = {
 *       { "*", SGL_ICON_NORNAL_FILE },
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
 *   sgl_filebrowser_set_dir(filebrowser, "/");
 *   sgl_filebrowser_set_text_font(filebrowser, &sgl_font_file);
 *   sgl_filebrowser_set_icons(filebrowser, filebrowser_icons);
 *   sgl_filebrowser_set_path_prefix(filebrowser, SGL_ICON_DISK " SD:");
 */
#ifndef __SGL_FILEBROWSER_H__
#define __SGL_FILEBROWSER_H__

#include <sgl_core.h>
#include <sgl_misc.h>

#define SGL_FILEBROWSER_PATH_MAX_LEN  (128)
#define SGL_FILEBROWSER_NAME_MAX_LEN  (64)

/* Cache size = visible_rows * SGL_FILEBROWSER_CACHE_MULT.
 * 2 = one screen ahead + one screen buffer (recommended).
 * 1 = tightest memory, more re-fills when scrolling.
 * 3 = fewer re-fills, more RAM. */
#ifndef SGL_FILEBROWSER_CACHE_MULT
#define SGL_FILEBROWSER_CACHE_MULT    (2)
#endif

/* Absolute floor for cache slots (must be >= visible + a few). */
#ifndef SGL_FILEBROWSER_CACHE_MIN
#define SGL_FILEBROWSER_CACHE_MIN     (8)
#endif

typedef struct sgl_filebrowser_icon {
    const char *pattern;
    const char *icon;
} sgl_filebrowser_icon_t;

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
    const sgl_pixmap_t *pixmap;
    sgl_color_t item_text_color;
    sgl_color_t item_selected_color;
    sgl_color_t bg_color;
    sgl_color_t border_color;
    sgl_color_t icon_color;
    sgl_color_t path_color;
    sgl_filebrowser_icon_t *icons;
    uint8_t alpha;
    uint8_t icon_width;
    sgl_scroll_t sc;                   /**< shared scroll physics state (sgl_misc) */
    char current_path[SGL_FILEBROWSER_PATH_MAX_LEN];
    char full_path[SGL_FILEBROWSER_PATH_MAX_LEN];
    const char *path_prefix;
} sgl_filebrowser_t;

/**
 * @brief Create a file browser object.
 * @param parent: parent object
 * @return file browser object
 */
sgl_obj_t*  sgl_filebrowser_create(sgl_obj_t *parent);

/**
 * @brief Set the file browser object path.
 * @param obj: file browser object
 * @param path: path to set
 */
void sgl_filebrowser_set_dir(sgl_obj_t *obj, const char *path);

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
 * @param font: font to set
 * @return none
 */
void sgl_filebrowser_set_text_font(sgl_obj_t *obj, const sgl_font_t *font);

/**
 * @brief Set the pixmap of the file browser object.
 * @param obj: file browser object
 * @param pixmap: pixmap to set
 * @return none
 */
void sgl_filebrowser_set_pixmap(sgl_obj_t *obj, const sgl_pixmap_t *pixmap);

/**
 * @brief Set the text icons of the file browser object.
 * @param obj: file browser object
 * @param icons: icons to set
 * @return none
 */
void sgl_filebrowser_set_icons(sgl_obj_t *obj, sgl_filebrowser_icon_t *icons);

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

/**
 * @brief Set the path prefix of the file browser object.
 * @param obj: file browser object
 * @param prefix: path prefix to set
 * @return none
 */
void sgl_filebrowser_set_path_prefix(sgl_obj_t *obj, const char *prefix);

#endif /* __SGL_FILEBROWSER_H__ */
