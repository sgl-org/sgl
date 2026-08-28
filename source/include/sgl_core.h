/* source/include/sgl_core.h
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

#ifndef __SGL_CORE_H__
#define __SGL_CORE_H__

#include <stdint.h>
#include <stdarg.h>
#include <sgl_cfgfix.h>
#include <sgl_math.h>
#include <sgl_log.h>
#include <sgl_list.h>
#include <sgl_event.h>

#ifdef __cplusplus
extern "C" {
#endif

/* the maximum depth of object*/
#define  SGL_OBJ_DEPTH_MAX                 (16)
/* the maximum number of drawing buffers */
#define  SGL_DRAW_BUFFER_MAX               (2)
/* define default animation tick ms */
#define  SGL_SYSTEM_TICK_MS                CONFIG_SGL_SYSTICK_MS
/* define dirty area trace color */
#define  SGL_DIRTY_AREA_TRACE_COLOR        CONFIG_SGL_DIRTY_AREA_TRACE_COLOR

/* define focused color and border width */
#define  SGL_FOCUSED_COLOR                 CONFIG_SGL_FOCUSED_COLOR
#define  SGL_FOCUSED_WIDTH                 CONFIG_SGL_FOCUSED_WIDTH

#if (CONFIG_SGL_DIRTY_AREA_NUM_MAX)
#define  SGL_DIRTY_AREA_NUM_MAX            CONFIG_SGL_DIRTY_AREA_NUM_MAX
#endif

/* the ASCII offset of fonts */
#define  SGL_TEXT_ASCII_OFFSET             (32)

/**
* @brief This enumeration type defines the alignment of controls in sgl,
*        i.e. coordinate positions
*/
typedef enum sgl_align_type {
    SGL_ALIGN_CENTER = 0,
    SGL_ALIGN_TOP_MID,
    SGL_ALIGN_TOP_LEFT,
    SGL_ALIGN_TOP_RIGHT,
    SGL_ALIGN_BOT_MID,
    SGL_ALIGN_BOT_LEFT,
    SGL_ALIGN_BOT_RIGHT,
    SGL_ALIGN_LEFT_MID,
    SGL_ALIGN_RIGHT_MID,
    SGL_ALIGN_VERT_LEFT,
    SGL_ALIGN_VERT_RIGHT,
    SGL_ALIGN_VERT_MID,
    SGL_ALIGN_HORIZ_TOP,
    SGL_ALIGN_HORIZ_BOT,
    SGL_ALIGN_HORIZ_MID,
    SGL_ALIGN_NUM,

} sgl_align_type_t;

/**
* @brief This enumeration type defines the layout of controls in sgl,
*        i.e. coordinate positions
*/
typedef enum sgl_layout_type {
    SGL_LAYOUT_NONE = 0,
    SGL_LAYOUT_HORIZONTAL = 1,
    SGL_LAYOUT_VERTICAL = 2,
    SGL_LAYOUT_GRID = 3,
    SGL_LAYOUT_NUM = 4,
} sgl_layout_type_t;

/**
* @brief This structure describes the layout of the control, including the layout type,
*        number of columns, number of rows, column spacing, and row spacing
*
* @type: layout type
* @col_num: number of columns
* @row_num: number of rows
* @col_space: column spacing
* @row_space: row spacing
* @left_space: left spacing
* @right_space: right spacing
* @top_space: top spacing
* @bottom_space: bottom spacing
*/
typedef struct sgl_layout_desc {
    sgl_layout_type_t type;
    int16_t col_num;
    int16_t row_num;
    int16_t col_space;
    int16_t row_space;
    int16_t left_space;
    int16_t right_space;
    int16_t top_space;
    int16_t bottom_space;
} sgl_layout_desc_t;

/**
* @brief This structure is a structure that describes the position of the control,
*        where x represents the position of the x coordinate, which is the row coordinate position,
*        and y represents the position of the y coordinate, which is the column coordinate position
*
* @x: x coordinate
* @y: y coordinate
*/
typedef struct sgl_pos {
    int16_t x;
    int16_t y;
} sgl_pos_t;

/**
* @brief This structure describes the size of the object, including width and height, in pixels
*
* @w: width
* @h: height
*/
typedef struct sgl_size {
    int16_t w;
    int16_t h;
} sgl_size_t;

/**
* @brief This structure describes a rectangular region, where x1 and y1 represent the coordinates
*        of the upper left corner of the rectangle, and x2 and y2 represent the coordinates of the
*        lower right corner of the rectangle
*
* @x1: x position left corner of the rectangle
* @y1: y position left corner of the rectangle
* @x2: x position right corner of the rectangle
* @y2: y position right corner of the rectangle
*/
typedef struct sgl_area {
    int16_t x1;
    int16_t y1;
    int16_t x2;
    int16_t y2;
} sgl_area_t;

/**
* @brief This structure defines a 32 bit color bit field
*
* @blue: Blue color component
* @green: Green color component
* @red: Red color component
* @alpha: Color transparency
*/
typedef union {
    struct {
        uint8_t blue;
        uint8_t green;
        uint8_t red;
        uint8_t alpha;
    } ch;
    uint32_t full;
} sgl_color32_t;

/**
* @brief This structure defines a 24 bit color bit field
*
* @blue: Blue color component
* @green: Green color component
* @red: Red color component
*/
typedef union {
    struct {
        uint8_t blue;
        uint8_t green;
        uint8_t red;
    } ch;
    uint8_t full[3];
} sgl_color24_t;

/**
* @brief This structure defines a 16 bit color bit field
*
* @blue: Blue color component
* @green: Green color component
* @red: Red color component
* @alpha: Color transparency
*/
typedef union {
    struct {
        uint16_t blue : 5;
        uint16_t green : 6;
        uint16_t red : 5;
    } ch;
    uint16_t full;
} sgl_color16_t;

/**
* @brief This structure defines a 8 bit color bit field
*
* @blue: Blue color component
* @green: Green color component
* @red: Red color component
* @alpha: Color transparency
*/
typedef union {
    struct {
        uint8_t blue : 2;
        uint8_t green : 3;
        uint8_t red : 3;
    } ch;
    uint8_t full;
} sgl_color8_t;

#if (CONFIG_SGL_FBDEV_PIXEL_DEPTH == 32)
#define sgl_color_t sgl_color32_t
#elif (CONFIG_SGL_FBDEV_PIXEL_DEPTH == 24)
#define sgl_color_t sgl_color24_t
#elif (CONFIG_SGL_FBDEV_PIXEL_DEPTH == 16)
#define sgl_color_t sgl_color16_t
#elif (CONFIG_SGL_FBDEV_PIXEL_DEPTH == 8)
#define sgl_color_t sgl_color8_t
#endif

/**
 * @brief This structure defines a surface, which is a rectangular area of the screen.
 * @x1:     x1 coordinate
 * @y1:     y1 coordinate
 * @x2:     x2 coordinate
 * @y2:     y2 coordinate
 * @buffer: buffer pointer
 * @size:   bytes of buffer
 * @w:      surf width
 * @h:      surf height
 * @dirty:  pointer to dirty area
 */
typedef struct sgl_surf {
    int16_t      x1;
    int16_t      y1;
    int16_t      x2;
    int16_t      y2;
    sgl_color_t *buffer;
    uint32_t     size;
    uint16_t     w;
    uint16_t     h;
    sgl_area_t   *dirty;
} sgl_surf_t;

/**
* @brief This structure defines an image, with a bitmap pointing to the
*        bitmap of the image, while specifying the width and height of the image
*
* @width: pixmap width
* @height: pixmap height
* @format: bitmap format 0: no compression, 1:
* @bitmap: point to image bitmap
*/
typedef struct sgl_pixmap {
    uint32_t width : 13;
    uint32_t height : 13;
    uint32_t format : 6;
    union {
        const uint8_t *array;
        const uintptr_t addr;
    } bitmap;
} sgl_pixmap_t;

/**
 * @brief This structure defines an icon, with a bitmap pointing to the
 * @width: pixmap width
 * @height: pixmap height
 * @bitmap: point to icon bitmap
 */
typedef struct sgl_icon_pixmap {
    uint16_t       width;
    uint16_t       height;
    const uint8_t *bitmap;
} sgl_icon_pixmap_t;

/**
* @brief Font index table structure, used to describe the bitmap index positions of
*        all characters in a font, accelerating the search process
*
* @bitmap_index: point to bitmap index of font
* @adv_w: advance width of character width
* @box_h: height of font
* @box_w: width of font
* @ofs_x: offset of character x position
* @ofs_y: offset of character y position
*/
typedef struct sgl_font_table {
#if (CONFIG_SGL_FONT_SMALL_TABLE)
    const uint16_t bitmap_index;
    const uint16_t adv_w;
    const uint8_t  box_h;
    const uint8_t  box_w;
    const int8_t   ofs_x;
    const int8_t   ofs_y;
#else
    const uint32_t bitmap_index;
    const uint16_t adv_w;
    const uint16_t box_h;
    const uint16_t box_w;
    const int8_t   ofs_x;
    const int8_t   ofs_y;
#endif
} sgl_font_table_t;

/**
 * @brief This structure defines a font unicode information, which is a collection of fonts
 * @offset: offset of unicode for unicode list
 * @len: length of unicode list
 * @list: point to unicode list
 * @tab_offset: offset of font table
 */
typedef struct sgl_font_unicode {
    const uint32_t offset;
    const uint32_t len;
    const uint16_t *list;
    const uint32_t tab_offset;
} sgl_font_unicode_t;

/**
 * @brief External flash read callback for flash fonts (see CONFIG_SGL_FLASH_FONT)
 * @param addr absolute address in the external flash
 * @param buf destination buffer
 * @param len number of bytes to read
 * @return number of bytes actually read, or negative value on failure
 */
typedef int32_t (*sgl_flash_font_read_fn)(uint32_t addr, void *buf, uint32_t len);

/**
* @brief A structure used to describe information about a font, Defining a font set requires
*        the use of this structure to describe relevant information
*
* @bitmap: point to bitmap of font, set NULL when bitmap is stored in external flash
* @table: point to struct sgl_font_table
* @font_table_size: size of struct sgl_font_table
* @font_height: height of font
* @unicode: point to struct sgl_font_unicode struct
* @unicode_num: number of unicode parts
* @base_line: base line of font
* @bpp: The anti aliasing level of the font, only support 2, 4
* @compress: compress flag, 0: no compress, 1: compress
* @flash_read: external flash read callback, only used when bitmap == NULL
* @flash_addr: base address of the font bitmap blob in the external flash,
*              table[].bitmap_index is the offset relative to it
*/
typedef struct sgl_font {
    const uint8_t  *bitmap;
    const sgl_font_table_t  *table;
    const uint16_t  font_table_size;
    const uint16_t  font_height;
    const sgl_font_unicode_t *unicode;
    const uint32_t  unicode_num;
    const int16_t   base_line;
    const uint8_t   bpp;
    const uint8_t   compress;
#if (CONFIG_SGL_FLASH_FONT)
    sgl_flash_font_read_fn flash_read;
    uint32_t flash_addr;
#endif
} sgl_font_t;

/**
 * @brief Represents a fundamental UI object in the SGL (Simple Graphics Library) framework.
 *
 * This structure defines a generic GUI element that can be part of a hierarchical display tree.
 * Members:
 * @parent: Pointer to the parent object; NULL if this is a root-level object.
 * @child: Pointer to the first child in the list of children.
 * @sibling: Pointer to the next sibling under the same parent.
 * @area: The logical size (width, height) of the object, used for layout and measurement.
 * @coords: The current screen position (x, y) and dimensions after layout calculation.
 * @event_fn: Callback function invoked when an event (e.g., touch, click) targets this object.
 * @event_data: User-defined context data passed to the event callback.
 * @construct_fn: Initialization hook called during object creation to allocate resources or set defaults.
 * @destroyed: (1 bit) Set to 1 when the object is marked for destruction.
 * @dirty: (1 bit) Set to 1 when the object needs to be redrawn.
 * @hide: (1 bit) Set to 1 to exclude the object from rendering (hidden).
 * @needinit: (1 bit) Set to 1 if the object requires deferred initialization.
 * @layout: (2 bits) Layout mode for children:
 *          - 0: No auto-layout
 *          - 1: Horizontal layout (left to right)
 *          - 2: Vertical layout (top to bottom)
 *          - 3: Reserved
 * @clickable: (1 bit) Set to 1 if the object can receive click/touch events.
 * @movable: (1 bit) Set to 1 if the object can be dragged by the user.
 * @border: border width of object
 * @flexible: (1 bit, in uint16_t field) Indicates the object can expand to fill available space.
 * @evt_leave: (1 bit) Set to 1 if the object should receive "pointer leave" events.
 * @pressed: (1 bit) Tracks whether the object is currently being pressed.
 * @page: (1 bit) Reserved for page/view switching logic (e.g., in tabbed interfaces).
 * @radius: (12 bits) Corner radius in pixels for rounded rectangle rendering (max 4095).
 * @name: [Optional] Null-terminated string identifier for debugging or lookup.
 *        Only present if CONFIG_SGL_OBJ_USE_NAME is defined.
 */
typedef struct sgl_obj {
    struct sgl_obj  *parent;
    struct sgl_obj  *child;
    struct sgl_obj  *sibling;
    sgl_area_t      area;
    sgl_area_t      coords;
    void            (*event_fn)(sgl_event_t *e);
    void            *event_data;
    void            (*construct_fn)(sgl_surf_t *surf, struct sgl_obj *obj, sgl_event_t *event);
    uint8_t         destroyed : 1;
    uint8_t         dirty : 1;
    uint8_t         hide : 1;
    uint8_t         needinit : 1;
    uint8_t         focus : 1;
    uint8_t         editable : 1;
    uint8_t         clickable : 1;
    uint8_t         movable : 1;
    uint8_t         border;
    uint16_t        flexible : 1;
    uint16_t        pressed : 1;
    uint16_t        press_click : 1;
    uint16_t        page : 1;
    uint16_t        radius : 12;
#if CONFIG_SGL_OBJ_USE_NAME
    const char      *name;
#endif
} sgl_obj_t;

/**
 * @brief Represents a page object in the SGL graphics system.
 *
 * An sgl_page_t encapsulates a complete, renderable UI page or screen.
 * It combines a base graphical object, a drawing surface, a background color,
 * and an optional background pixmap. Pages serve as top-level containers
 * for UI elements and can be managed by a page manager or display stack.
 *
 * Members:
 * - obj      : Base object (inherits sgl_obj_t), providing position, size, visibility, etc.
 * - color    : Default background color used when no pixmap is set.
 * - alpha    : Alpha value for the background color (0-255).
 * - pixmap   : Optional pointer to a background pixmap. If non-NULL, it typically overrides 'color'
 *              as the background content during rendering (behavior depends on flush/render logic).
 */
typedef struct sgl_page {
    sgl_obj_t          obj;
    sgl_color_t        color;
    uint8_t            alpha;
    const sgl_pixmap_t *pixmap;
} sgl_page_t;

/**
 * @brief sgl framebuffer information struct
 * @buffer: framebuffer, this specify the memory address of the framebuffer
 * @buffer_size: framebuffer size
 * @xres: x resolution
 * @yres: y resolution
 * @flush_area: flush area callback function pointer, return the finished flag
 */
typedef struct sgl_fbinfo {
    void      *buffer[SGL_DRAW_BUFFER_MAX];
    uint32_t   buffer_size;
    int16_t    xres;
    int16_t    yres;
    void       (*flush_area)(sgl_area_t *area, sgl_color_t *src);
} sgl_fbinfo_t;

/**
 * @brief sgl framebuffer device struct
 * @fbinfo: framebuffer information, that specify the memory address of the framebuffer and resolution
 * @surf: Drawing surface associated with this page; defines the target buffer or area for rendering.
 * @dirty_num: dirty area number
 * @update_flag: update to widget flag
 * @full_dirty: full dirty flag
 * @fb_emit: current flush buffer index, 0: buffer0 is emitting, 1: buffer1 is emitting.
 * @fb_ready: framebuffer ready flag, 01: buffer0 is ready, 10: buffer1 is ready, 11: buffers are ready
 * @dirty: dirty area pool
 * @page: current page
 */
typedef struct sgl_fbdev {
    sgl_fbinfo_t      fbinfo;
    sgl_surf_t        surf;
    uint8_t           dirty_num;
    uint8_t           update_flag;
    volatile uint8_t  fb_emit;
    volatile uint8_t  fb_ready;
    sgl_area_t        dirty[SGL_DIRTY_AREA_NUM_MAX];
    sgl_obj_t         *active;
#if (CONFIG_SGL_DIRTY_AREA_TRACE)
    sgl_area_t        trace_dirty[SGL_DIRTY_AREA_NUM_MAX];
    uint8_t           trace_dirty_num;
    bool              trace_flag;
#endif
} sgl_fbdev_t;

/**
 * @brief sgl log print device struct
 * @logdev: log print callback function pointer
 * @fbdev: framebuffer device
 * @last_tick: last tick time, ms
 * @tick_ms: tick milliseconds
 * @font: system default font
 * @rotation: buffer only for rotation
 * @angle: angle value only for rotation
 */
typedef struct sgl_system {
    sgl_fbdev_t        fbdev;
    volatile uint32_t  last_tick;
    volatile uint32_t  tick_ms;
    const sgl_font_t   *font;
#if (CONFIG_SGL_FBDEV_ROTATION != 0)
    sgl_color_t        *rotation;
#elif (CONFIG_SGL_FBDEV_RUNTIME_ROTATION)
    sgl_color_t        *rotation;
    uint16_t            angle;
#endif
#if (CONFIG_SGL_DEBUG)
    void               (*logdev)(const char *str);
#endif
} sgl_system_t;

/**
 * @brief for each child object of parent
 * @param _child: pointer of child object
 * @param parent: pointer of parent object
 */
#define  sgl_obj_for_each_child(_child, parent)             for (_child = parent->child; _child != NULL; _child = _child->sibling)

/**
 * @brief for each child object of parent safely
 * @param _child: pointer of child object
 * @param parent: pointer of parent object
 */
#define sgl_obj_for_each_child_safe(_child, n, parent)      for (_child = parent->child, n = (_child ? _child->sibling : NULL); _child != NULL; \
                                                                 _child = n, n = (_child ? _child->sibling : NULL))

/**
 * @brief The macro define the rotation of the framebuffer device
 * @param area_dst: destination area
 * @param area_src: source area
 * @param dst: destination buffer
 * @param src: source buffer
 * @note it only support 90/180/270 degree rotation
 */
#define sgl_fbdev_rotate_90(area_dst, area_src, dst, src)   do {                                                                              \
                                                            for (uint16_t y = 0; y < height; y++) {                                           \
                                                                for (uint16_t x = 0; x < width; x++) {                                        \
                                                                    size_t src_idx = y * width + x;                                           \
                                                                    size_t dst_idx = (width - 1 - x) * height + y;                            \
                                                                    dst[dst_idx] = src[src_idx];                                              \
                                                                }                                                                             \
                                                            }                                                                                 \
                                                            area_dst.x1 = area_src->y1;                                                       \
                                                            area_dst.y1 = SGL_SCREEN_WIDTH - area_src->x2 - 1;                                \
                                                            area_dst.x2 = sgl_min(area_src->y2, SGL_SCREEN_HEIGHT - 1);                       \
                                                            area_dst.y2 = sgl_min(SGL_SCREEN_WIDTH - area_src->x1 - 1, SGL_SCREEN_WIDTH - 1); \
                                                            } while(0);

#define sgl_fbdev_rotate_180(area_dst, area_src, dst, src)  do {                                                                              \
                                                            size_t total = (size_t)(width * height);                                          \
                                                            for (size_t i = 0; i < total; i++) {                                              \
                                                                dst[i] = src[total - 1 - i];                                                  \
                                                            }                                                                                 \
                                                            area_dst.x1 = SGL_SCREEN_WIDTH  - area_src->x2 - 1;                               \
                                                            area_dst.y1 = SGL_SCREEN_HEIGHT - area_src->y2 - 1;                               \
                                                            area_dst.x2 = SGL_SCREEN_WIDTH  - area_src->x1 - 1;                               \
                                                            area_dst.y2 = SGL_SCREEN_HEIGHT - area_src->y1 - 1;                               \
                                                            } while(0);

#define sgl_fbdev_rotate_270(area_dst, area_src, dst, src)  do {                                                                              \
                                                            for (uint16_t y = 0; y < height; y++) {                                           \
                                                                for (uint16_t x = 0; x < width; x++) {                                        \
                                                                    size_t src_idx = y * width + x;                                           \
                                                                    size_t dst_idx = x * height + (height - 1 - y);                           \
                                                                    dst[dst_idx] = src[src_idx];                                              \
                                                                }                                                                             \
                                                            }                                                                                 \
                                                            area_dst.x1 = SGL_SCREEN_HEIGHT - area_src->y2 - 1;                               \
                                                            area_dst.y1 = area_src->x1;                                                       \
                                                            area_dst.x2 = sgl_min(area_dst.x1 + height - 1, SGL_SCREEN_HEIGHT - 1);           \
                                                            area_dst.y2 = sgl_min(area_dst.y1 + width - 1, SGL_SCREEN_WIDTH - 1);             \
                                                            } while(0);


/* dont to use this variable, it is used internally by sgl library */
extern sgl_system_t sgl_system;

/**
 * @brief Alpha blending table for 4 bpp and 2 bpp
 */
extern const uint8_t sgl_opa4_table[16];
extern const uint8_t sgl_opa2_table[4];

/**
 * @brief register the frame buffer device
 * @param fbinfo the frame buffer device information
 * @return int, 0 if success, -1 if failed
 * @note you must check the result of this function
 */
int sgl_fbdev_register(sgl_fbinfo_t *fbinfo);

/**
 * @brief register the frame buffer device
 * @param buffer0 the frame buffer device buffer0
 * @param buffer1 the frame buffer device buffer1
 * @param buffer_size the frame buffer device buffer size, for example: 16bit color and 320 * 10 line buffer, you should set it to 320 * 10
 * @param resolution_x the frame buffer device resolution x
 * @param resolution_y the frame buffer device resolution y
 * @return int, 0 if success, -1 if failed
 * @note you must check the result of this function
 */
int sgl_fbdev_register_dev(sgl_color_t *buffer0, sgl_color_t *buffer1, uint32_t buffer_size, uint16_t resolution_x, uint16_t resolution_y);

/**
 * @brief set framebuffer device flush ready
 * @param none
 * @return none
 * @note this function must be called in DMA callback function after framebuffer device flush.
 *       Flushes complete in FIFO order, so this marks the oldest emitting
 *       buffer as ready. The draw loop decides which buffer to draw next.
 */
static inline void sgl_fbdev_flush_ready(void)
{
    sgl_fbdev_t *fb = &sgl_system.fbdev;
    uint8_t ready = fb->fb_ready & 0x3;
    uint8_t bit = (ready == 0) ? (fb->fb_emit ^ 1) : (ready & 1);
    fb->fb_ready |= (1 << bit);
}

/**
 * @brief check if framebuffer device buffer need to wait ready
 * @param fbdev point to the framebuffer device
 * @return bool true if need to wait ready, false if not
 */
static inline bool sgl_fbdev_flush_wait_ready(sgl_fbdev_t *fbdev)
{
    /* single buffer only uses bit0, double buffer uses bit0 and bit1 */
    uint8_t mask = (fbdev->fbinfo.buffer[1] != NULL) ? 3 : 1;
    return (fbdev->fb_ready & mask) == 0;
}

/**
 * @brief get framebuffer device buffer resolution width
 * @param none
 * @return framebuffer device buffer resolution width
 */
static inline int16_t sgl_fbdev_resolution_width(void)
{
    return sgl_system.fbdev.fbinfo.xres;
}

/**
 * @brief get framebuffer device buffer resolution height
 * @param none
 * @return framebuffer device buffer resolution height
 */
#define  SGL_SCREEN_WIDTH  sgl_fbdev_resolution_width()

/**
 * @brief get framebuffer device buffer resolution height
 * @param none
 * @return framebuffer device buffer resolution height
 */
static inline int16_t sgl_fbdev_resolution_height(void)
{
    return sgl_system.fbdev.fbinfo.yres;
}

/**
 * @brief get framebuffer device buffer resolution width
 * @param none
 * @return framebuffer device buffer resolution width
 */
#define  SGL_SCREEN_HEIGHT  sgl_fbdev_resolution_height()

/**
 * @brief get framebuffer device buffer address
 * @param none
 * @return framebuffer device buffer address
 */
static inline void* sgl_fbdev_buffer_address(void)
{
    return sgl_system.fbdev.fbinfo.buffer[0];
}

/**
 * @brief framebuffer device flush function
 * @param area [in] area of flush, that is x1, y1, x2, y2: area of flush
 *                  area contains the coordinates of the area to be flushed
 *                  - x1: x coordinate of the top left corner of the area
 *                  - y1: y coordinate of the top left corner of the area
 *                  - x2: x coordinate of the bottom right corner of the area
 *                  - y2: y coordinate of the bottom right corner of the area
 * @param src [in] source color
 */
static inline void sgl_fbdev_flush_area(sgl_area_t *area, sgl_color_t *src)
{
#if CONFIG_SGL_COLOR16_SWAP
    uint16_t w = area->x2 - area->x1 + 1;
    uint16_t h = area->y2 - area->y1 + 1;
    uint16_t *dst = (uint16_t *)src;
    for (size_t i = 0; i < (size_t)(w * h); i++) {
        dst[i] = (dst[i] << 8) | (dst[i] >> 8);
    }
#endif

#if (CONFIG_SGL_FBDEV_ROTATION != 0)
    uint16_t width = area->x2 - area->x1 + 1;
    uint16_t height = area->y2 - area->y1 + 1;
    sgl_area_t area_dst = *area;

#if (CONFIG_SGL_FBDEV_ROTATION == 90)
    sgl_fbdev_rotate_90(area_dst, area, sgl_system.rotation, src);
#elif (CONFIG_SGL_FBDEV_ROTATION == 180)
    sgl_fbdev_rotate_180(area_dst, area, sgl_system.rotation, src);
#elif (CONFIG_SGL_FBDEV_ROTATION == 270)
    sgl_fbdev_rotate_270(area_dst, area, sgl_system.rotation, src);
#else
#error "CONFIG_SGL_FBDEV_ROTATION is invalid rotation value (only 0/90/180/270 supported)"
#endif
    sgl_system.fbdev.fbinfo.flush_area(&area_dst, sgl_system.rotation);
#elif (CONFIG_SGL_FBDEV_RUNTIME_ROTATION)
    uint16_t width = area->x2 - area->x1 + 1;
    uint16_t height = area->y2 - area->y1 + 1;
    sgl_area_t area_dst = *area;

    switch (sgl_system.angle) {
    case 0:
        sgl_system.fbdev.fbinfo.flush_area(area, src);
        return;
    case 90:
        sgl_fbdev_rotate_90(area_dst, area, sgl_system.rotation, src);
        break;
    case 180:
        sgl_fbdev_rotate_180(area_dst, area, sgl_system.rotation, src);
        break;
    case 270:
        sgl_fbdev_rotate_270(area_dst, area, sgl_system.rotation, src);
        break;
    default:
        SGL_LOG_ERROR("invalid angle: %d", sgl_system.angle);
        return;
    }
    sgl_system.fbdev.fbinfo.flush_area(&area_dst, sgl_system.rotation);
#else
    sgl_system.fbdev.fbinfo.flush_area(area, src);
#endif
}

#if (CONFIG_SGL_FBDEV_RUNTIME_ROTATION)
/**
 * @brief set framebuffer device rotation angle
 * @param angle [in] rotation angle, that is 0, 90, 180, 270
 * @return none
 * @note Rotation angle must be 0, 90, 180, 270
 */
void sgl_fbdev_set_angle(uint16_t angle);
#endif //CONFIG_SGL_FBDEV_RUNTIME_ROTATION

/**
 * @brief register log output device
 * @param log_puts log output function
 * @return none
 * @note if you want to use print log into uart or other devices, you must register log output device first
 */
static inline void sgl_logdev_register(void (*puts)(const char *str))
{
#if (CONFIG_SGL_DEBUG)
    sgl_system.logdev = puts;
#else
    SGL_UNUSED(puts);
#endif
}

/**
 * @brief log output function
 * @param str log string
 * @return none
 * @note if you want to use printf function, you must register log output device first
 */
static inline void sgl_log_stdout(const char *str)
{
#if (CONFIG_SGL_DEBUG)
    if (sgl_system.logdev) {
        sgl_system.logdev(str);
    }
#else
    SGL_UNUSED(str);
#endif
}

/**
 * @brief get pixmap bytes of per pixel
 * @param pixmap pointer to pixmap
 * @return pixmap bytes of per pixel
 */
uint8_t sgl_pixmal_get_pixel_bytes(const sgl_pixmap_t *pixmap);

/**
 * @brief get tick milliseconds
 * @param none
 * @return tick milliseconds
 */
static inline uint32_t sgl_tick_get(void)
{
    return sgl_system.tick_ms;
}

/**
 * @brief get last tick milliseconds
 * @param none
 * @return tick milliseconds
 */
static inline uint32_t sgl_last_tick_get(void)
{
    return sgl_system.last_tick;
}

/**
 * @brief increase tick milliseconds
 * @param ms milliseconds
 * @return none
 * @note in general, you should call this function in the 1ms tick interrupt handler
 *       of course, you can use polling function to increase tick milliseconds.
 */
static inline void sgl_tick_inc(uint32_t ms)
{
    sgl_system.tick_ms += ms;
}

/**
 * @brief sync tick milliseconds
 * @param tick_ms tick milliseconds
 * @return none
 */
static inline void sgl_tick_sync(uint32_t tick_ms)
{
    sgl_system.tick_ms = tick_ms;
}

/**
 * @brief init object node
 * @param obj: pointer of object
 * @return none
 */
static inline void sgl_obj_node_init(sgl_obj_t *obj)
{
    SGL_ASSERT(obj != NULL);

    obj->sibling = NULL;
    obj->child = NULL;
}

/**
 * @brief add object to parent
 * @param parent: pointer of parent object
 * @param obj: pointer of object
 * @return none
 */
void sgl_obj_add_child(sgl_obj_t *parent, sgl_obj_t *obj);

/**
 * @brief remove an object from its parent
 * @param obj object to remove
 * @return none
 */
void sgl_obj_remove(sgl_obj_t *obj);

/**
 * @brief check if object has child
 * @param  obj object
 * @return true or false, true means object has child, false means object has no child
 */
static inline bool sgl_obj_has_child(sgl_obj_t *obj) {
    SGL_ASSERT(obj != NULL);
    return (bool)obj->child;
}

/**
 * @brief get child of an object
 * @param obj the object
 * @return the child of the object
 */
static inline sgl_obj_t* sgl_obj_get_child(sgl_obj_t* obj)
{
    SGL_ASSERT(obj != NULL);
    return obj->child;
}

/**
 * @brief get last child of an object
 * @param obj the object
 * @return the last child of the object
 */
sgl_obj_t* sgl_obj_get_last_child(sgl_obj_t* obj);

/**
 * @brief check if object is last child
 * @param obj the object
 * @return true or false, true means object is last child, false means object is not last child
 */
static inline bool sgl_obj_is_last_child(sgl_obj_t* obj)
{
    SGL_ASSERT(obj != NULL);
    return (obj == obj->parent->child);
}

/**
 * @brief check if object has sibling
 * @param  obj object
 * @return true or false, true means object has sibling, false means object has no sibling
 */
static inline bool sgl_obj_has_sibling(sgl_obj_t *obj) {
    SGL_ASSERT(obj != NULL);
    return (bool)obj->sibling;
}

/**
 * @brief get sibling of an object
 * @param obj the object
 * @return the sibling of the object
 */
static inline sgl_obj_t* sgl_obj_get_next_sibling(sgl_obj_t* obj)
{
    SGL_ASSERT(obj != NULL);
    return obj->sibling;
}

/**
 * @brief get previous sibling of an object
 * @param obj the object
 * @return the previous sibling of the object
 */
sgl_obj_t* sgl_obj_get_prev_sibling(sgl_obj_t* obj);

/**
 * @brief get child count of an object
 * @param obj object
 * @return child count
 */
static inline size_t sgl_obj_get_child_count(sgl_obj_t *obj)
{
    SGL_ASSERT(obj != NULL);
    sgl_obj_t *child = NULL;
    size_t count = 0;

    sgl_obj_for_each_child(child, obj) {
        count ++;
    }

    return count;
}

/**
 * @brief merge an area into global dirty area
 * 
 * This function calculates how much rectangle 'a' would need to grow in each direction (left, right, top, bottom)
 * to fully enclose both 'a' and 'b'. The result is the sum of the expansions along all four sides.
 * Note: This is not the increase in area, th is a lightweight heuristic for merge cost in bounding-box algorithms.
 * 
 * @param area [in] Pointer to the area
 * @return none
 */
void sgl_dirty_area_push(sgl_area_t *area);

/**
 * @brief set system font
 * @param font pointer to font
 * @return none
 */
static inline void sgl_set_system_font(const sgl_font_t *font)
{
    SGL_ASSERT(font != NULL);
    sgl_system.font = font;
}

/**
 * @brief get system font
 * @param none
 * @return pointer to system font
 */
static inline const sgl_font_t* sgl_get_system_font(void)
{
    return sgl_system.font; 
}

/**
 * @brief Set object to dirty
 * @param obj point to object
 * @return none
 * @note this function will set object to dirty, include its children
 */
void sgl_obj_set_dirty(sgl_obj_t *obj);

/**
 * @brief Clear object dirty flag
 * @param obj point to object
 * @return none
 */
static inline void sgl_obj_clear_dirty(sgl_obj_t *obj)
{
    SGL_ASSERT(obj != NULL);
    obj->dirty = 0;
}

/**
 * @brief  Set the object to be destroyed
 * @param  obj: the object to set
 * @retval None
 * @note this function is used to set the destroyed flag of the object, then next draw cycle, the object will be removed
 *       the object should be not NULL.
 */
void sgl_obj_set_destroyed(sgl_obj_t *obj);

/**
 * @brief check object destroyed flag
 * @param obj point to object
 * @return flag, false - live, true - destroyed
 */
static inline bool sgl_obj_is_destroyed(sgl_obj_t *obj)
{
    SGL_ASSERT(obj != NULL);
    return (bool)obj->destroyed;
}

/**
 * @brief check object dirty flag
 * @param obj point to object
 * @return flag, false - clear, true - dirty
 */
static inline bool sgl_obj_is_dirty(sgl_obj_t *obj)
{
    SGL_ASSERT(obj != NULL);
    return (bool)obj->dirty;
}

/**
 * @brief set object need init coords
 * @param obj point to object
 * @return none
 */
static inline void sgl_obj_needinit(sgl_obj_t *obj)
{
    SGL_ASSERT(obj != NULL);
    obj->needinit = 1;
}

/**
 * @brief check object if need init coords
 * @param obj point to object
 * @return flag, true - need init coords
 */
static inline bool sgl_obj_is_needinit(sgl_obj_t *obj)
{
    SGL_ASSERT(obj != NULL);
    return (bool)obj->needinit;
}

/**
 * @brief set object focus flag
 * @param obj point to object
 * @return none
 */
static inline void sgl_obj_set_focused(sgl_obj_t *obj)
{
    SGL_ASSERT(obj != NULL);
    obj->focus = 1;
}

/**
 * @brief clear object focus flag
 * @param obj point to object
 * @return none
 */
static inline void sgl_obj_clear_focused(sgl_obj_t *obj)
{
    SGL_ASSERT(obj != NULL);
    obj->focus = 0;
}

/**
 * @brief check object focus flag
 * @param obj point to object
 * @return flag, true - focused
 */
static inline bool sgl_obj_is_focused(sgl_obj_t *obj)
{
    SGL_ASSERT(obj != NULL);
    return (bool)obj->focus;
}

/**
 * @brief set object editable flag
 * @param obj point to object
 * @return none
 */
static inline void sgl_obj_set_editable(sgl_obj_t *obj)
{
    SGL_ASSERT(obj != NULL);
    obj->editable = 1;
}

/**
 * @brief clear object editable flag
 * @param obj point to object
 * @return none
 */
static inline void sgl_obj_clear_editable(sgl_obj_t *obj)
{
    SGL_ASSERT(obj != NULL);
    obj->editable = 0;
}

/**
 * @brief check object editable flag
 * @param obj point to object
 * @return flag, true - editable
 */
static inline bool sgl_obj_is_editable(sgl_obj_t *obj)
{
    SGL_ASSERT(obj != NULL);
    return (bool)obj->editable;
}

/**
 * @brief clear object need init coords flag
 * @param obj point to object
 * @return none
 */
static inline void sgl_obj_clear_needinit(sgl_obj_t *obj)
{
    SGL_ASSERT(obj != NULL);
    obj->needinit = 0;
}

/**
 * @brief set object hidden flag
 * @param obj point to object
 * @return none
 */
static inline void sgl_obj_set_hidden(sgl_obj_t *obj)
{
    SGL_ASSERT(obj != NULL);
    obj->hide = 1;
    sgl_dirty_area_push(&obj->area);
}

/**
 * @brief set object visible
 * @param obj point to object
 * @return none
 */
static inline void sgl_obj_set_visible(sgl_obj_t *obj)
{
    SGL_ASSERT(obj != NULL);
    obj->hide = 0;
    sgl_dirty_area_push(&obj->area);
}

/**
 * @brief check object hidden flag
 * @param obj point to object
 * @return flag, false - show, true - hidden
 */
static inline bool sgl_obj_is_hidden(sgl_obj_t *obj)
{
    SGL_ASSERT(obj != NULL);
    return (bool)obj->hide;
}

/**
 * @brief set object to clickable
 * @param obj point to object
 * @return none
 * @note if object is clickable, it can receive click events, otherwise it will not receive
 */
static inline void sgl_obj_set_clickable(sgl_obj_t *obj)
{
    SGL_ASSERT(obj != NULL);
    obj->clickable = 1;
    obj->flexible  = 1;
}

/**
 * @brief set object to unclickable
 * @param obj point to object
 * @return none
 * @note if object is clickable, it can receive click events, otherwise it will not receive
 */
static inline void sgl_obj_set_unclickable(sgl_obj_t *obj)
{
    SGL_ASSERT(obj != NULL);
    obj->clickable = 0;
    obj->flexible  = 0;
}

/**
 * @brief check object is clickable
 * @param obj point to object
 * @return true: clickable, false: unclickable
 */
static inline bool sgl_obj_is_clickable(sgl_obj_t *obj)
{
    SGL_ASSERT(obj != NULL);
    return (bool)obj->clickable;
}

/**
 * @brief set object to mask key press event
 * @param obj point to object
 * @note
*/
static inline void sgl_obj_set_keypress_click(sgl_obj_t *obj)
{
    SGL_ASSERT(obj != NULL);
    obj->press_click = 1;
}

/**
 * @brief check object key press mask status
 * @param obj point to object
 * @return true: press mask, false: unpress mask
 */
static inline bool sgl_obj_is_keypress_click(sgl_obj_t *obj)
{
    SGL_ASSERT(obj != NULL);
    return obj->press_click;
}

/**
 * @brief set object flexible
 * @param obj point to object
 * @return none
 */
static inline void sgl_obj_set_flexible(sgl_obj_t *obj)
{
    SGL_ASSERT(obj != NULL);
    obj->flexible = 1;
}

/**
 * @brief set object unflexible
 * @param obj point to object
 * @return none
 */
static inline void sgl_obj_set_unflexible(sgl_obj_t *obj)
{
    SGL_ASSERT(obj != NULL);
    obj->flexible = 0;
}

/**
 * @brief check object flexible
 * @param obj point to object
 * @return true or false
 */
static inline bool sgl_obj_is_flexible(sgl_obj_t *obj)
{
    SGL_ASSERT(obj != NULL);
    return (bool)obj->flexible;
}

/**
 * @brief set object clickable
 * @param obj point to object
 * @return none
 */
static inline void sgl_obj_set_movable(sgl_obj_t *obj)
{
    SGL_ASSERT(obj != NULL);
    obj->movable = 1;
    obj->clickable = 1;
}

/**
 * @brief set object unmovable
 * @param obj point to object
 * @return none
 */
static inline void sgl_obj_set_unmovable(sgl_obj_t *obj)
{
    SGL_ASSERT(obj != NULL);
    obj->movable = 0;
}

/**
 * @brief check object movable
 * @param obj point to object
 * @return true or false
 */
static inline bool sgl_obj_is_movable(sgl_obj_t *obj)
{
    SGL_ASSERT(obj != NULL);
    return (bool)obj->movable;
}

/**
 * @brief update object area
 * @param area point to area that need update
 * @return none, this function will force update object area
 * @note this function will update object area, and the object area will be merged into the dirty area
 */
void sgl_update_area(sgl_area_t *area);

/**
 * @brief update object area
 * @param area point to area that need update
 * @return none, this function will force update object area
 * @note this function will update object area, it will be added into dirty area without merging
 */
void sgl_obj_update_area(sgl_area_t *area);

/**
 * @brief move object child position
 * @param obj point to object
 * @param ofs_x: x offset position
 * @param ofs_y: y offset position
 * @return none
 */
void sgl_obj_move_child_pos(sgl_obj_t *obj, int16_t ofs_x, int16_t ofs_y);

/**
 * @brief move object child x position
 * @param obj point to object
 * @param ofs_x: x offset position
 * @return none
 */
static inline void sgl_obj_move_child_pos_x(sgl_obj_t *obj, int16_t ofs_x)
{
    sgl_obj_move_child_pos(obj, ofs_x, 0);
    sgl_obj_set_dirty(obj);
}

/**
 * @brief move object child y position
 * @param obj point to object
 * @param ofs_y: y offset position
 * @return none
 */
static inline void sgl_obj_move_child_pos_y(sgl_obj_t *obj, int16_t ofs_y)
{
    sgl_obj_move_child_pos(obj, 0, ofs_y);
    sgl_obj_set_dirty(obj);
}

/**
 * @brief zoom object size
 * @param obj point to object
 * @param zoom zoom size
 * @return none
 * @note if you want to zoom out, the zoom should be positive, if you want to zoom in, the zoom should be negative
 */
void sgl_obj_size_zoom(sgl_obj_t *obj, int16_t zoom);

/**
 * @brief zoom object circle size
 * @param obj point to object
 * @param radius circle radius
 * @return none
 */
void sgl_obj_circle_zoom(sgl_obj_t *obj, int16_t radius);

/**
 * @brief move object up a level layout
 * @param obj point to object
 * @return none
 * @note Only move among sibling objects
 */
void sgl_obj_move_up(sgl_obj_t *obj);

/**
 * @brief move object down a level layout
 * @param obj point to object
 * @return none
 * @note Only move among sibling objects
 */
void sgl_obj_move_down(sgl_obj_t *obj);

/**
 * @brief move object top level layout
 * @param obj point to object
 * @return none
 * @note Only move among sibling objects
 */
void sgl_obj_move_top(sgl_obj_t *obj);

/**
 * @brief move object bottom level layout
 * @param obj point to object
 * @return none
 * @note Only move among sibling objects
 */
void sgl_obj_move_bottom(sgl_obj_t *obj);

/**
 * @brief Set object absolute position
 * @param obj point to object
 * @param abs_x: x absolute position
 * @param abs_y: y absolute position
 * @return none
 */
void sgl_obj_set_abs_pos(sgl_obj_t *obj, int16_t abs_x, int16_t abs_y);

/**
 * @brief Get object absolute position
 * @param obj point to object
 * @param abs_x: point to x absolute position
 * @param abs_y: point to y absolute position
 * @return none
 */
static inline sgl_pos_t sgl_obj_get_abs_pos(sgl_obj_t *obj)
{
    sgl_pos_t pos = {
        .x = obj->coords.x1,
        .y = obj->coords.y1
    };
    return pos;
}

/**
 * @brief Set object relative position
 * @param obj point to object
 * @param x: x relative position
 * @param y: y relative position
 * @return none
 * @note This x and y position is relative to the parent object
 */
static inline void sgl_obj_set_pos(sgl_obj_t *obj, int16_t x, int16_t y)
{
    sgl_obj_set_abs_pos(obj, obj->parent->coords.x1 + x, obj->parent->coords.y1 + y);
}

/**
 * @brief Get object position
 * @param obj point to object
 * @return sgl_pos_t: position of object
 * @note this function will return the top left corner position of the object relative to its parent
 */
static inline sgl_pos_t sgl_obj_get_pos(sgl_obj_t *obj)
{
    SGL_ASSERT(obj != NULL);

    sgl_pos_t pos;
    pos.x = obj->coords.x1 - obj->parent->coords.x1;
    pos.y = obj->coords.y1 - obj->parent->coords.y1;
    return pos;
}

/**
 * @brief Set object x relative position
 * @param obj point to object
 * @param x x position
 * @return none
 * @note this function will set the x position of the object, it's relative to the parent object
 */
static inline void sgl_obj_set_pos_x(sgl_obj_t *obj, int16_t x)
{
    sgl_obj_set_abs_pos(obj, obj->parent->coords.x1 + x, obj->coords.y1);
}

/**
 * @brief Get object x relative position
 * @param obj point to object
 * @return x position, it's relative to the parent object
 */
static inline size_t sgl_obj_get_pos_x(sgl_obj_t *obj)
{
    return (obj->coords.x1 - obj->parent->coords.x1);
}

/**
 * @brief Set object y relative position
 * @param obj point to object
 * @param y y position
 * @return none
 * @note this function will set the y position of the object, it's relative to the parent object
 */
static inline void sgl_obj_set_pos_y(sgl_obj_t *obj, int16_t y)
{
    sgl_obj_set_abs_pos(obj, obj->coords.x1, obj->parent->coords.y1 + y);
}

/**
 * @brief Get object y relative position
 * @param obj point to object
 * @return y position, it's relative to the parent object
 */
static inline int16_t sgl_obj_get_pos_y(sgl_obj_t *obj)
{
    return obj->coords.y1 - obj->parent->coords.y1;
}

/**
 * @brief Set the alignment position of the object relative to its parent object.
 * @param obj The object to set the alignment position.
 * @param type The alignment type.
 * @return none
 * @note type should be one of the sgl_align_type_t values:
 *       - SGL_ALIGN_CENTER    : Center the object in the parent object.
 *       - SGL_ALIGN_TOP_MID   : Align the object at the top middle of the parent object.
 *       - SGL_ALIGN_TOP_LEFT  : Align the object at the top left of the parent object.
 *       - SGL_ALIGN_TOP_RIGHT : Align the object at the top right of the parent object.
 *       - SGL_ALIGN_BOT_MID   : Align the object at the bottom middle of the parent object.
 *       - SGL_ALIGN_BOT_LEFT  : Align the object at the bottom left of the parent object.
 *       - SGL_ALIGN_BOT_RIGHT : Align the object at the bottom right of the parent object.
 *       - SGL_ALIGN_LEFT_MID  : Align the object at the left middle of the parent object.
 *       - SGL_ALIGN_RIGHT_MID : Align the object at the right middle of the parent object.
 * 
 * @warning You must set the size of object before calling this function.
 */
void sgl_obj_set_pos_align(sgl_obj_t *obj, sgl_align_type_t type);

/**
 * @brief Set the alignment position of the object relative to sibling object.
 * @param ref The reference object, it should be the sibling object.
 * @param obj The object to set the alignment position.
 * @param type The alignment type.
 * @return none
 * @note type should be one of the sgl_align_type_t values:
 *       - SGL_ALIGN_VERT_LEFT  : Align the object at the left side of the reference object.
 *       - SGL_ALIGN_VERT_RIGHT : Align the object at the right side of the reference object.
 *       - SGL_ALIGN_VERT_MID   : Align the object at the middle of the reference object.
 *       - SGL_ALIGN_HORIZ_TOP  : Align the object at the top side of the reference object.
 *       - SGL_ALIGN_HORIZ_BOT  : Align the object at the bottom side of the reference object.
 *       - SGL_ALIGN_HORIZ_MID  : Align the object at the middle of the reference object.
 * 
 * @warning You must set the size of object before calling this function.
 */
void sgl_obj_set_pos_align_ref(sgl_obj_t *ref, sgl_obj_t *obj, sgl_align_type_t type);

/**
 * @brief Set the layout of the object.
 * @param obj The object to set the layout.
 * @param desc The layout description.
 * @return none
 * @note The layout description should be one of the sgl_layout_desc_t values:
 *       - SGL_LAYOUT_NONE        : No layout.
 *       - SGL_LAYOUT_HORIZONTAL  : Horizontal layout.
 *       - SGL_LAYOUT_VERTICAL    : Vertical layout.
 *       - SGL_LAYOUT_GRID        : Grid layout.
 * @warning You must set col_num and row_num if the layout type is SGL_LAYOUT_GRID.
 */
void sgl_obj_set_layout(sgl_obj_t *obj, sgl_layout_desc_t *desc);

/**
 * @brief Set object size
 * @param obj point to object
 * @param width: width that you want to set
 * @param height: height that you want to set
 * @return none
 */
void sgl_obj_set_size(sgl_obj_t *obj, int16_t width, int16_t height);

/**
 * @brief Get object size
 * @param obj point to object
 * @return sgl_size_t: size of object
 */
static inline sgl_size_t sgl_obj_get_size(sgl_obj_t *obj)
{
    SGL_ASSERT(obj != NULL);
    sgl_size_t size;
    size.w = obj->coords.x2 - obj->coords.x1 + 1;
    size.h = obj->coords.y2 - obj->coords.y1 + 1;
    return size;
}

/**
 * @brief Set object width
 * @param obj point to object
 * @param width: width that you want to set
 * @return none
 */
void sgl_obj_set_width(sgl_obj_t *obj, int16_t width);

/**
 * @brief Get object width
 * @param obj point to object
 * @return object width
 */
static inline int16_t sgl_obj_get_width(sgl_obj_t *obj)
{
    SGL_ASSERT(obj != NULL);
    return obj->coords.x2 - obj->coords.x1 + 1;
}

/**
 * @brief Set object height
 * @param obj point to object
 * @param height: height that you want to set
 * @return none
 */
void sgl_obj_set_height(sgl_obj_t *obj, int16_t height);

/**
 * @brief Get object height
 * @param obj point to object
 * @return object height
 */
static inline int16_t sgl_obj_get_height(sgl_obj_t *obj)
{
    SGL_ASSERT(obj != NULL);
    return obj->coords.y2 - obj->coords.y1 + 1;
}

/**
 * @brief Set object border width
 * @param obj point to object
 * @param border: border width that you want to set
 * @return none
 */
void sgl_obj_set_border_width(sgl_obj_t *obj, uint8_t border);

/**
 * @brief Get object border width
 * @param obj point to object
 * @return object border width
 */
static inline int16_t sgl_obj_get_border_width(sgl_obj_t *obj)
{
    SGL_ASSERT(obj != NULL);
    return obj->border;
}

/**
 * @brief Get object fill rectangle
 * @param obj point to object
 * @return object fill rectangle
 * @note This function is used to obtain the inner area of an object, i.e., the region excluding its borders.
 */
static inline sgl_area_t sgl_obj_get_fill_rect(sgl_obj_t *obj)
{
    SGL_ASSERT(obj != NULL);
    sgl_area_t fill = {
        .x1 = sgl_max(obj->coords.x1 + obj->border, obj->area.x1),
        .y1 = sgl_max(obj->coords.y1 + obj->border, obj->area.y1),
        .x2 = sgl_min(obj->coords.x2 - obj->border, obj->area.x2),
        .y2 = sgl_min(obj->coords.y2 - obj->border, obj->area.y2),
    };
    return fill;
}

/**
 * @brief Set object event callback function
 * @param obj point to object
 * @param event_fn: event callback function
 * @param data: event callback function data
 * @return none
 */
static inline void sgl_obj_set_event_cb(sgl_obj_t *obj, void (*event_fn)(sgl_event_t *e), void *data)
{
    SGL_ASSERT(obj != NULL);
    obj->event_fn = event_fn;
    obj->event_data = data;
    obj->clickable = 1;
}

/**
 * @brief set the radius of object
 * @param obj object
 * @param radius: radius that you want to set
 * @return none
 * @note if radius is larger than object's width or height, fix radius will be returned
 */
void sgl_obj_set_radius(sgl_obj_t *obj, size_t radius);

/**
 * @brief get the radius of object
 * @param obj object
 * @return object radius
 */
static inline int16_t sgl_obj_get_radius(sgl_obj_t *obj)
{
    SGL_ASSERT(obj != NULL);
    return obj->radius;
}

/**
 * @brief set current object as screen object
 * @param obj object, that you want to set an object as active page
 * @return none
 */
void sgl_screen_load(sgl_obj_t *obj);

/**
 * @brief get current screen object
 * @param none
 * @return active current screen object
 */
static inline sgl_obj_t* sgl_screen_act(void)
{
    return sgl_system.fbdev.active;
}

/**
 * @brief sgl task handler function with sync mode
 * @param none
 * @return none
 * @note you can call this function to force update screen
 */
void sgl_task_handler_sync(void);

/**
 * @brief sgl task handler function
 * @param none
 * @return none
 * @note this function should be called in main loop or timer or thread
 */
void sgl_task_handler(void);

/**
 * @brief Create an object
 * @param parent parent object
 * @return sgl_obj_t
 * @note if parent is NULL, the object will be as an new page
 */
sgl_obj_t* sgl_obj_create(sgl_obj_t *parent);

/**
 * @brief  free an object
 * @param  obj: object to free
 * @retval none
 * @note this function will free all the children of the object
 */
void sgl_obj_free(sgl_obj_t *obj);

/**
 * @brief delete object
 * @param obj point to object
 * @return none
 * @note this function will set object and his childs to be destroyed, then next draw cycle, the object will be removed.
 *       if object is NULL, the all objects of active page will be delete, but the page object will not be deleted.
 *       if object is a page, the page object will be deleted and all its children will be deleted.
 */
void sgl_obj_delete(sgl_obj_t *obj);

/**
 * @brief delete object
 * @param obj point to object
 * @return none
 * @note this function will take effect immediately
 */
void sgl_obj_delete_sync(sgl_obj_t *obj);

/**
 * @brief delete object's all children
 * @param obj point to object
 * @return none
 * @note this function will delete object's all children, but not the object itself.
 */
void sgl_obj_delete_children(sgl_obj_t *obj);

/**
 * @brief delete object's all children immediately
 * @param obj point to object
 * @return none
 * @note this function will delete object's all children immediately, but not the object itself.
 */
void sgl_obj_delete_children_sync(sgl_obj_t *obj);

/**
 * @brief color mixer
 * @param fg_color : foreground color
 * @param bg_color : background color
 * @param factor   : color mixer factor
 * @return sgl_color_t: mixed color
 */
static inline sgl_color_t sgl_color_mixer(sgl_color_t fg_color, sgl_color_t bg_color, uint8_t factor)
{
    sgl_color_t ret;
#if (CONFIG_SGL_FBDEV_PIXEL_DEPTH == SGL_COLOR_RGB332)

    ret.ch.red   = bg_color.ch.red + ((fg_color.ch.red - bg_color.ch.red) * (factor >> 5) >> 3);
    ret.ch.green = bg_color.ch.green + ((fg_color.ch.green - bg_color.ch.green) * (factor >> 5) >> 3);
    ret.ch.blue  = bg_color.ch.blue + ((fg_color.ch.blue - bg_color.ch.blue) * (factor >> 6) >> 2);

#elif (CONFIG_SGL_FBDEV_PIXEL_DEPTH == SGL_COLOR_RGB565)

    factor = (uint32_t)((uint32_t)factor + 4) >> 3;
    uint32_t bg = (uint32_t)((uint32_t)bg_color.full | ((uint32_t)bg_color.full << 16)) & 0x07E0F81F; 
    uint32_t fg = (uint32_t)((uint32_t)fg_color.full | ((uint32_t)fg_color.full << 16)) & 0x07E0F81F;
    uint32_t result = ((((fg - bg) * factor) >> 5) + bg) & 0x7E0F81F;
    ret.full = (uint16_t)((result >> 16) | result);

#elif (CONFIG_SGL_FBDEV_PIXEL_DEPTH == SGL_COLOR_RGB888)

    ret.ch.red   = bg_color.ch.red + ((fg_color.ch.red - bg_color.ch.red) * factor >> 8);
    ret.ch.green = bg_color.ch.green + ((fg_color.ch.green - bg_color.ch.green) * factor >> 8);
    ret.ch.blue  = bg_color.ch.blue + ((fg_color.ch.blue - bg_color.ch.blue) * factor >> 8);

#elif (CONFIG_SGL_FBDEV_PIXEL_DEPTH == SGL_COLOR_ARGB8888)

    ret.ch.alpha = bg_color.ch.alpha + ((fg_color.ch.alpha - bg_color.ch.alpha) * factor >> 8);
    ret.ch.red   = bg_color.ch.red + ((fg_color.ch.red - bg_color.ch.red) * factor >> 8);
    ret.ch.green = bg_color.ch.green + ((fg_color.ch.green - bg_color.ch.green) * factor >> 8);
    ret.ch.blue  = bg_color.ch.blue + ((fg_color.ch.blue - bg_color.ch.blue) * factor >> 8);

#endif
    return ret;
}

/**
 * @brief Invert a color value
 * @param color the color to invert
 * @return inverted color
 * @note For RGB formats, each channel is inverted (max_value - channel).
 *       For ARGB8888, alpha channel is preserved.
 */
static inline sgl_color_t sgl_color_invert(sgl_color_t color)
{
    sgl_color_t ret;
#if (CONFIG_SGL_FBDEV_PIXEL_DEPTH == SGL_COLOR_RGB332)
    ret.ch.red   = 0x07 - color.ch.red;
    ret.ch.green = 0x07 - color.ch.green;
    ret.ch.blue  = 0x03 - color.ch.blue;
#elif (CONFIG_SGL_FBDEV_PIXEL_DEPTH == SGL_COLOR_RGB565)
    ret.ch.red   = 0x1F - color.ch.red;
    ret.ch.green = 0x3F - color.ch.green;
    ret.ch.blue  = 0x1F - color.ch.blue;
#elif (CONFIG_SGL_FBDEV_PIXEL_DEPTH == SGL_COLOR_RGB888)
    ret.ch.red   = 0xFF - color.ch.red;
    ret.ch.green = 0xFF - color.ch.green;
    ret.ch.blue  = 0xFF - color.ch.blue;
#elif (CONFIG_SGL_FBDEV_PIXEL_DEPTH == SGL_COLOR_ARGB8888)
    ret.ch.alpha = color.ch.alpha;
    ret.ch.red   = 0xFF - color.ch.red;
    ret.ch.green = 0xFF - color.ch.green;
    ret.ch.blue  = 0xFF - color.ch.blue;
#endif
    return ret;
}

/**
 * @brief Fills a block of memory with a solid color.
 *
 * Writes the specified `color` value to `len` consecutive elements starting at `dest`.
 * This is equivalent to a memset-like operation but for color values (typically 32-bit RGBA).
 *
 * @param[out] dest   Pointer to the start of the destination color buffer.
 * @param[in]  color  The color value to fill with.
 * @param[in]  len    Number of color elements to write (not bytes).
 */
static inline void sgl_color_set(sgl_color_t *dest, sgl_color_t color, uint32_t len)
{
    while (len--) {*dest++ = color; }
}

/**
 * @brief area init
 * @param area : area pointer
 * @return none
 */
static inline void sgl_area_init(sgl_area_t *area)
{
    SGL_ASSERT(area != NULL);
    area->x1 = area->y1 = SGL_POS_MAX;
    area->x2 = area->y2 = SGL_POS_MIN;
}

/**
 * @brief get pixel of pixmap
 * @pixmap: pointe to pixmap
 * @param x: x position
 * @param y: y position
 * @return sgl_color_t: pixel color
 */
static inline sgl_color_t sgl_pixmap_get_pixel(const sgl_pixmap_t *pixmap, int16_t x, int16_t y)
{
    SGL_ASSERT(pixmap != NULL);
    return ((sgl_color_t*)pixmap->bitmap.array)[y * pixmap->width + x];
}

/**
 * @brief get pixel of pixmap buf
 * @pixmap: pointe to pixmap
 * @param x: x position
 * @param y: y position
 * @return sgl_color_t: pixel color address
 */
static inline sgl_color_t* sgl_pixmap_get_buf(const sgl_pixmap_t *pixmap, int16_t x, int16_t y)
{
    SGL_ASSERT(pixmap != NULL);
    return &((sgl_color_t*)pixmap->bitmap.array)[y * pixmap->width + x];
}

/**
 * @brief check two area is overlap
 * @param area_a area a
 * @param area_b area b
 * @return true or false, true means overlap, false means not overlap
 * @note: this function is unsafe, you should check the area_a and area_b is not NULL by yourself
 */
static inline bool sgl_area_is_overlap(sgl_area_t *area_a, sgl_area_t *area_b)
{
    SGL_ASSERT(area_a != NULL && area_b != NULL);
    if (area_b->y1 > area_a->y2 || area_b->y2 < area_a->y1 || area_b->x1 > area_a->x2 || area_b->x2 < area_a->x1) {
        return false;
    }

    return true;
}

/**
 * @brief check surf and other area is overlap
 * @param surf surfcare
 * @param area area b
 * @return true or false, true means overlap, false means not overlap
 * @note: this function is unsafe, you should check the surfcare and area is not NULL by yourself
 */
static inline bool sgl_surf_area_is_overlap(sgl_surf_t *surf, sgl_area_t *area)
{
    return sgl_area_is_overlap((sgl_area_t*)surf, area);
}

/**
 * @brief  Get area intersection between two areas
 * @param area_a: area a
 * @param area_b: area b
 * @param clip: intersection area
 * @return true: intersect, otherwise false
 * @note: this function is unsafe, you should check the area_a and area_b and clip is not NULL by yourself
 */
bool sgl_area_clip(sgl_area_t *area_a, sgl_area_t *area_b, sgl_area_t *clip);

/**
 * @brief  Get area intersection between surface and area
 * @param surf: surface
 * @param area: area
 * @param clip: intersection area
 * @return true: intersect, otherwise false
 * @note: this function is unsafe, you should check the surf and area is not NULL by yourself
 */
static inline bool sgl_surf_clip(sgl_surf_t *surf, sgl_area_t *area, sgl_area_t *clip)
{
    return sgl_area_clip((sgl_area_t*)surf, area, clip);
}

/**
 * @brief clip area with another area
 * @param clip [in][out] clip area
 * @param area [in] area
 * @return true if clip area is valid, otherwise two area is not overlapped
 * @note: this function is unsafe, you should check the clip and area is not NULL by yourself
 */
bool sgl_area_selfclip(sgl_area_t *clip, sgl_area_t *area);

/**
 * @brief merge two area, the merge is result of the two area clip
 * @param area_a [in] area1
 * @param area_b [in] area2
 * @param merge  [out] merge result
 * @return none
 * @note: this function is unsafe, you should check the area_a and area_b and merge is not NULL by yourself
 */
static inline void sgl_area_merge(sgl_area_t *area_a, sgl_area_t *area_b, sgl_area_t *merge)
{
    SGL_ASSERT(area_a != NULL && area_b != NULL && merge != NULL);
    merge->x1 = sgl_min(area_a->x1, area_b->x1);
    merge->x2 = sgl_max(area_a->x2, area_b->x2);
    merge->y1 = sgl_min(area_a->y1, area_b->y1);
    merge->y2 = sgl_max(area_a->y2, area_b->y2);
}

/**
 * @brief merge two area, the merge is a new area
 * @param merge [in][out] merge area
 * @param area [in] area
 * @return none
 * @note: this function is unsafe, you should check the merge and area is not NULL by yourself
 */
static inline void sgl_area_selfmerge(sgl_area_t *merge, sgl_area_t *area)
{
    SGL_ASSERT(merge != NULL && area != NULL);
    merge->x1 = sgl_min(merge->x1, area->x1);
    merge->x2 = sgl_max(merge->x2, area->x2);
    merge->y1 = sgl_min(merge->y1, area->y1);
    merge->y2 = sgl_max(merge->y2, area->y2);
}

/**
 * @brief check object draw complete
 * @param obj object
 * @param surf surface
 * @param clip clip area
 * @return true: draw complete, otherwise false
 * @note: this function is unsafe, you should check the obj, surf and clip is not NULL by yourself
 */
static inline bool sgl_obj_draw_complete(sgl_obj_t *obj, sgl_surf_t *surf, sgl_area_t *clip)
{
    return (clip->y2 == surf->dirty->y2 || clip->y2 == obj->area.y2);
}

/**
 * @brief sgl global initialization
 * @param none
 * @return int, 0 means success, others means failed
 * @note You should call this function before using sgl and you should call this function after register framebuffer device.
 *       This function is unsafe, you should check the return value by yourself.
 */
int sgl_init(void);

/**
 * @brief initialize object
 * @param obj object
 * @param parent parent object
 * @return int, 0 means successful, -1 means failed
 */
int sgl_obj_init(sgl_obj_t *obj, sgl_obj_t *parent);

/**
 * @brief Convert UTF-8 string to Unicode
 * @param utf8_str Pointer to the UTF-8 string to be converted
 * @param p_unicode_buffer Pointer to the buffer where the converted Unicode will be stored
 * @return The number of bytes in the UTF-8 string
 */
uint32_t sgl_utf8_to_unicode(const char *utf8_str, uint32_t *p_unicode_buffer);

/**
 * @brief Search for the index of a Unicode character in the font table
 * @param font Pointer to the font structure containing character data
 * @param unicode Unicode of the character to be searched
 * @return Index of the character in the font table
 */
uint32_t sgl_search_unicode_ch_index(const sgl_font_t *font, uint32_t unicode);

/**
 * @brief get height in font
 * @param font pointer to sgl_font_t
 * @return height of font, -1 means error
 * @note this function is used to get the height of a character in a font
 */
static inline int16_t sgl_font_get_height(const sgl_font_t *font)
{
    return font != NULL ? font->font_height : 0;
}

/**
 * @brief get the width of a string
 * @param str string
 * @param font sgl font
 * @return width of string
 */
int32_t sgl_font_get_string_width(const char *str, const sgl_font_t *font);

/**
 * @brief get the height of a string, which is in a rect area
 * @param width width of the rect area
 * @param str string
 * @param font sgl font of the string
 * @param line_space peer line space
 * @return height size of string
 */
int32_t sgl_font_get_string_height(int16_t width, const char *str, const sgl_font_t *font, uint8_t line_space);

/**
 * @brief get the alignment position
 * @param parent_size parent size
 * @param size object size
 * @param type alignment type
 * @return alignment position
 */
sgl_pos_t sgl_get_align_pos(sgl_size_t *parent_size, sgl_size_t *size, sgl_align_type_t type);

/**
 * @brief get the text position in the area
 * @param area point to area
 * @param font point to font
 * @param text text string
 * @param offset text offset
 * @param type alignment type
 * @return sgl_pos_t position of text
 */
sgl_pos_t sgl_get_text_pos(sgl_area_t *area, const sgl_font_t *font, const char *text, int16_t offset, sgl_align_type_t type);

/**
 * @brief get the icon position of area
 * @param area point to area
 * @param icon point to icon
 * @param offset offset
 * @param type align type
 */
sgl_pos_t sgl_get_icon_pos(sgl_area_t *area, const sgl_icon_pixmap_t *icon, int16_t offset, sgl_align_type_t type);

/**
 * @brief set page background color
 * @param obj point to object
 * @param color background color
 * @return none
 */
void sgl_page_set_color(sgl_obj_t* obj, sgl_color_t color);

/**
 * @brief set page background pixmap
 * @param obj point to object
 * @param pixmap background pixmap
 * @return none
 */
void sgl_page_set_pixmap(sgl_obj_t* obj, const sgl_pixmap_t *pixmap);

/**
 * @brief set page background alpha
 * @param obj point to object
 * @param alpha background alpha
 * @return none
 */
void sgl_page_set_alpha(sgl_obj_t* obj, uint8_t alpha);

/**
 * @brief get parent of an object
 * @param obj the object
 * @return the parent of the object
 */
static inline sgl_obj_t* sgl_obj_get_parent(sgl_obj_t* obj)
{
    SGL_ASSERT(obj != NULL);
    return obj->parent;
}

/**
 * @brief format a string, a simple version of vsnprintf
 * @param buf buffer
 * @param size buffer size
 * @param fmt format string
 * @param ap argument list
 * @return number of characters written
 */
int sgl_vsnprintf(char *buf, size_t size, const char *fmt, va_list ap);

/**
 * @brief format a string, a simple version of snprintf
 * @param buf buffer
 * @param size buffer size
 * @param fmt format string
 * @param ... arguments
 * @return number of characters written
 */
int sgl_snprintf(char *buf, size_t size, const char *fmt, ...);

/**
 * @brief format a string, a simple version of sprintf (with width alignment support)
 * @param buf buffer
 * @param fmt format string
 * @param ... arguments
 * @return number of characters written
 */
int sgl_sprintf(char *buf, const char *fmt, ...);

#if (CONFIG_SGL_OBJ_USE_NAME && CONFIG_SGL_DEBUG)
/**
 * @brief set object name
 * @param obj The object to set the name
 * @param name The name to set.
 * @return None.
 */
static inline void sgl_obj_set_name(sgl_obj_t *obj, const char *name)
{
    SGL_ASSERT(obj != NULL);
    obj->name = name;
}

/**
 * @brief print object name that include this all child
 * @param obj point to object
 * @return none
 */
void sgl_obj_print_name(sgl_obj_t *obj);

#endif

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif //__SGL_CORE_H__
