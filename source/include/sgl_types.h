/* source/include/sgl_types.h
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

#ifndef __SGL_TYPES_H__
#define __SGL_TYPES_H__

#include <stdint.h>
#include <stdbool.h>
#include <sgl_cfgfix.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Get the offset of internal members of the structure
 * @struct_t: structure typedef 
 * @member: member in structure
 * @return offset of member in the structure
*/
#define sgl_offsetof(struct_t, member)          ((size_t)&((struct_t*)0)->member)

/**
 * @brief Get the address of the structure instance.
 * @ptr: address of the structure member.
 * @type: type of the structure.
 * @member: member name of the ptr in structure.
 * @return pointer to address of structure 
 */
#define sgl_container_of(ptr, type, member)     ((type *)((char *)ptr - sgl_offsetof(type, member)))

#ifndef SGL_ARRAY_SIZE
#define SGL_ARRAY_SIZE(array)                   (sizeof(array) / sizeof(array[0]))
#endif

#define sgl_align_up(x, align)                  (((x) + ((align) - 1)) & ~((align) - 1))
#define sgl_round_up(x, round)                  ((((x) + ((round) - 1)) / (round)) * (round))

#define SGL_UNUSED(x)                           (void)(x)

#define sgl_floor_even(n)                       ((n) & ~1U)
#define sgl_ceil_even(n)                        (((n) + 1U) & ~1U)
#define sgl_floor_odd(n)                        ((n) & 1U ? (n) : (n) - 1U)
#define sgl_ceil_odd(n)                         ((n) | 1U)

#define SGL_POS_INVALID                         (0xefff)
#define SGL_POS_MAX                             (8192)
#define SGL_POS_MIN                             (-8192)
#define SGL_RADIUS_INVALID                      (0xFFF)

#define SGL_AREA_MAX                            {.x1 = SGL_POS_MIN, .y1 = SGL_POS_MIN, .x2 = SGL_POS_MAX, .y2 = SGL_POS_MAX}
#define SGL_AREA_INVALID                        {.x1 = SGL_POS_MAX, .y1 = SGL_POS_MAX, .x2 = SGL_POS_MIN, .y2 = SGL_POS_MIN}

#define SGL_WIDTH_INVALID                       (-1)
#define SGL_WIDTH_MAX                           (8192)
#define SGL_WIDTH_MIN                           (0)
#define SGL_HEIGHT_INVALID                      (-1)
#define SGL_HEIGHT_MAX                          (8192)
#define SGL_HEIGHT_MIN                          (0)

#define SGL_DIRECT_HORIZONTAL                   (0)
#define SGL_DIRECT_VERTICAL                     (1)

#define SGL_ALPHA_MAX                           (255)
#define SGL_ALPHA_MIN                           (0)
#define SGL_ALPHA_NUM                           (256)
#define SGL_ALPHA_PRCNT(x)                      ((x) * SGL_ALPHA_MAX / 100)
#define SGL_ALPHA_UNCLICKABLE                   (64)

#define SGL_STYLE_OK                            (0)
#define SGL_STYLE_FAILED                        (UINT32_MAX)
#define SGL_STYLE_INVALID                       (UINT32_MAX)

#define SGL_COLOR_RGB332                        (8)
#define SGL_COLOR_RGB565                        (16)
#define SGL_COLOR_RGB888                        (24)
#define SGL_COLOR_ARGB8888                      (32)

/* the pixmap format */
#define  SGL_PIXMAP_FMT_NONE                    (0)
#define  SGL_PIXMAP_FMT_RGB332                  (1)
#define  SGL_PIXMAP_FMT_ARGB2222                (2)
#define  SGL_PIXMAP_FMT_RGB565                  (3)
#define  SGL_PIXMAP_FMT_ARGB4444                (4)
#define  SGL_PIXMAP_FMT_RGB888                  (5)
#define  SGL_PIXMAP_FMT_ARGB8888                (6)
#define  SGL_PIXMAP_FMT_RLE_RGB332              (7)
#define  SGL_PIXMAP_FMT_RLE_ARGB2222            (8)
#define  SGL_PIXMAP_FMT_RLE_RGB565              (9)
#define  SGL_PIXMAP_FMT_RLE_ARGB4444            (10)
#define  SGL_PIXMAP_FMT_RLE_RGB888              (11)
#define  SGL_PIXMAP_FMT_RLE_ARGB8888            (12)
#define  SGL_PIXMAP_FMT_QOI_RGB565              (13)
#define  SGL_PIXMAP_FMT_MAX                     (14)

/* the font format */
#define  SGL_FONT_FMT_NORMAL                    (0)
#define  SGL_FONT_FMT_COMPRESSED                (1)
#define  SGL_FONT_FMT_EXT_FLASH                 (2)
#define  SGL_FONT_FMT_EXT_FLASH_FIXED           (3)
#define  SGL_FONT_FMT_TRUETYPE                  (4)
#define  SGL_FONT_FMT_MAX                       (5)

#ifdef __GNUC__            /* gcc compiler   */
#ifndef likely
#  define likely(x)                             __builtin_expect(!!(x), 1)
#  define unlikely(x)                           __builtin_expect(!!(x), 0)
#endif
#define sgl_weak_fn                             __attribute__((weak))
#define sgl_section(sec)                        __attribute__((section(#sec)))
#define sgl_packed                              __attribute__((packed))
#define sgl_align(x)                            __attribute__((aligned(x)))
#elif defined(__clang__)   /* clang compiler */
#ifndef likely
#  define likely(x)                             __builtin_expect(!!(x), 1)
#  define unlikely(x)                           __builtin_expect(!!(x), 0)
#endif
#define sgl_weak_fn                             __attribute__((weak))
#define sgl_section(sec)                        __attribute__((section(#sec)))
#define sgl_packed                              __attribute__((packed))
#define sgl_align(x)                            __attribute__((aligned(x)))
#elif defined(__CC_ARM)    /* RealView compiler (Keil ARMCC) */
#ifndef likely
#  define likely(x)                             __builtin_expect(!!(x), 1)
#  define unlikely(x)                           __builtin_expect(!!(x), 0)
#endif
#define sgl_weak_fn                             __weak
#define sgl_section(sec)                        __attribute__((section(#sec)))
#define sgl_packed                              __attribute__((packed))
#define sgl_align(x)                            __attribute__((aligned(x)))
#elif defined(__ICCARM__)  /* IAR compiler    */
#ifndef likely
#  if __VER__ >= 9100000
#    define likely(x)                           __iar_builtin_expect(!!(x), 1)
#    define unlikely(x)                         __iar_builtin_expect(!!(x), 0)
#  else
#    define likely(x)                           (x)
#    define unlikely(x)                         (x)
#  endif
#endif
#define sgl_weak_fn                             __weak
#define sgl_section(sec)                        __section(#sec)
#define sgl_packed                              __packed
#define sgl_align(x)                            _Pragma("data_alignment=" #x)
#elif defined(_MSC_VER)    /* MSVC compiler   */
#ifndef likely
#  define likely(x)                             (x)
#  define unlikely(x)                           (x)
#endif
#define sgl_weak_fn                             __declspec(selectany)
#define sgl_packed
#define sgl_align(x)                            __declspec(align(x))
#elif defined(__MINGW32__) /* MinGW compiler  */
#ifndef likely
#  define likely(x)                             __builtin_expect(!!(x), 1)
#  define unlikely(x)                           __builtin_expect(!!(x), 0)
#endif
#define sgl_weak_fn                             __attribute__((weak))
#define sgl_section(sec)                        __attribute__((section(#sec)))
#define sgl_packed                              __attribute__((packed))
#define sgl_align(x)                            __attribute__((aligned(x)))
#else                      /* others compiler */
#ifndef likely
#  define likely(x)                             (x)
#  define unlikely(x)                           (x)
#endif
#define sgl_packed
#define sgl_align(x)
#warning "Weak linkage not supported for this compiler"                    
#endif

#define  sgl_check_ptr_break(ptr)               if (unlikely((ptr) == NULL)) { SGL_LOG_ERROR("Function: %s, Line: %d, "#ptr" is NULL", __func__, __LINE__); return;}
#define  sgl_check_ptr_return(ptr, r)           if (unlikely((ptr) == NULL)) { SGL_LOG_ERROR("Function: %s, Line: %d, "#ptr" is NULL", __func__, __LINE__); return (r);}

/**
 * @brief Run once function
 * @note you must sure the statement is thread safe
 */
#define  SGL_RUN_ONCE(statement)                do { \
                                                    static bool _run_once_flag = false;     \
                                                    if (unlikely(_run_once_flag == false)){ \
                                                        statement;                          \
                                                        _run_once_flag = true;              \
                                                    }                                       \
                                                } while(0)

// prototype: sgl_rgb(uint8_t r, uint8_t g, uint8_t b)
#if (CONFIG_SGL_FBDEV_PIXEL_DEPTH == SGL_COLOR_ARGB8888 || CONFIG_SGL_FBDEV_PIXEL_DEPTH == SGL_COLOR_RGB888)
#define sgl_rgb(r,g,b)                          (sgl_color_t){ .ch.blue    = (b),                                     \
                                                               .ch.green   = (g),                                     \
                                                               .ch.red     = (r),}

#define sgl_color_hex(hex)                      (sgl_color_t){ .ch.blue    = ((uint8_t)((hex) >> 0) & 0xFF),          \
                                                               .ch.green   = ((uint8_t)((hex) >> 8) & 0xFF),          \
                                                               .ch.red     = ((uint8_t)((hex) >> 16) & 0xFF),}

#define sgl_color_value(color)                  ((color).ch.blue | ((color).ch.green << 8) | ((color).ch.red << 16))

#define sgl_rgb222_to_color(rgb222)             (sgl_color_t){ .ch.blue    = ((((rgb222) >> 0) & 0x03) << 6),         \
                                                               .ch.green   = ((((rgb222) >> 2) & 0x03) << 6),         \
                                                               .ch.red     = ((((rgb222) >> 4) & 0x03) << 6),}

#define sgl_rgb332_to_color(rgb332)             (sgl_color_t){ .ch.blue    = ((((rgb332) >> 0) & 0x03) << 6),         \
                                                               .ch.green   = ((((rgb332) >> 2) & 0x07) << 5),         \
                                                               .ch.red     = ((((rgb332) >> 5) & 0x07) << 5),}
                                                            
#define sgl_rgb444_to_color(rgb444)             (sgl_color_t){ .ch.blue    = ((((rgb444) >> 0) & 0xF) << 4),          \
                                                               .ch.green   = ((((rgb444) >> 4) & 0xF) << 4),          \
                                                               .ch.red     = ((((rgb444) >> 8) & 0xF) << 4),}

#define sgl_rgb565_to_color(rgb565)             (sgl_color_t){ .ch.blue    = ((((rgb565) >> 0) & 0x1F) << 3),         \
                                                               .ch.green   = ((((rgb565) >> 5) & 0x3F) << 2),         \
                                                               .ch.red     = ((((rgb565) >> 11) & 0x1F) << 3),}

#define sgl_rgb888_to_color(rgb888)             (sgl_color_t){ .ch.blue    = ((rgb888) >> 0),                         \
                                                               .ch.green   = ((rgb888) >> 8),                         \
                                                               .ch.red     = ((rgb888) >> 16),}

#elif (CONFIG_SGL_FBDEV_PIXEL_DEPTH == SGL_COLOR_RGB565)
#define sgl_rgb(r,g,b)                          (sgl_color_t){ .ch.blue    = (b) >> 3,                                \
                                                               .ch.green   = (g) >> 2,                                \
                                                               .ch.red     = (r) >> 3,}

#define sgl_color_hex(hex)                      (sgl_color_t){ .ch.blue    = ((uint8_t)((hex) >> 0) & 0x1F),          \
                                                               .ch.green   = ((uint8_t)((hex) >> 5) & 0x3F),          \
                                                               .ch.red     = ((uint8_t)((hex) >> 11) & 0x1F),}

#define sgl_color_value(color)                  ((color).ch.blue | ((color).ch.green << 5) | ((color).ch.red << 11))

#define sgl_rgb222_to_color(rgb222)             (sgl_color_t){ .ch.blue    = ((((rgb222) >> 0) & 0x03) << 3),         \
                                                               .ch.green   = ((((rgb222) >> 2) & 0x03) << 4),         \
                                                               .ch.red     = ((((rgb222) >> 4) & 0x03) << 3),}

#define sgl_rgb332_to_color(rgb332)             (sgl_color_t){ .ch.blue    = ((((rgb332) >> 0) & 0x03) << 3),         \
                                                               .ch.green   = ((((rgb332) >> 2) & 0x03) << 3),         \
                                                               .ch.red     = ((((rgb332) >> 5) & 0x03) << 2),}

#define sgl_rgb444_to_color(rgb444)             (sgl_color_t){ .ch.blue    = ((((rgb444) >> 0) & 0xF) << 1),          \
                                                               .ch.green   = ((((rgb444) >> 4) & 0xF) << 2),          \
                                                               .ch.red     = ((((rgb444) >> 8) & 0xF) << 1),}

#define sgl_rgb565_to_color(rgb565)             (sgl_color_t){ .ch.blue    = ((rgb565) >> 0) & 0x1F,                  \
                                                               .ch.green   = ((rgb565) >> 5) & 0x3F,                  \
                                                               .ch.red     = ((rgb565) >> 11) & 0x1F,}

#define sgl_rgb888_to_color(rgb888)             (sgl_color_t){ .ch.blue    = (((rgb888) >> 0) >> 3),                  \
                                                               .ch.green   = (((rgb888) >> 8) >> 2),                  \
                                                               .ch.red     = (((rgb888) >> 16) >> 3),}

#elif (CONFIG_SGL_FBDEV_PIXEL_DEPTH == SGL_COLOR_RGB332)
#define sgl_rgb(r,g,b)                          (sgl_color_t){ .ch.blue    = (b >> 6),                                \
                                                               .ch.green   = (g >> 5),                                \
                                                               .ch.red     = (r >> 5),}

#define sgl_color_hex(hex)                      (sgl_color_t){ .ch.blue    = ((uint8_t)((hex) >> 0) & 0x03),          \
                                                               .ch.green   = ((uint8_t)((hex) >> 3) & 0x07),          \
                                                               .ch.red     = ((uint8_t)((hex) >> 5) & 0x07),}

#define sgl_color_value(color)                  ((color).ch.blue | ((color).ch.green << 3) | ((color).ch.red << 5))

#define sgl_rgb222_to_color(rgb222)             (sgl_color_t){ .ch.blue    = ((((rgb222) >> 0) & 0x03) >> 0),         \
                                                               .ch.green   = ((((rgb222) >> 2) & 0x03) >> 1),         \
                                                               .ch.red     = ((((rgb222) >> 4) & 0x03) >> 1),}

#define sgl_rgb332_to_color(rgb332)             (sgl_color_t){ .ch.blue    = (((rgb332) >> 0) & 0x03),                \
                                                               .ch.green   = (((rgb332) >> 3) & 0x07),                \
                                                               .ch.red     = (((rgb332) >> 5) & 0x07),}

#define sgl_rgb444_to_color(rgb444)             (sgl_color_t){ .ch.blue    = ((((rgb444) >> 0) & 0xF) >> 2),          \
                                                               .ch.green   = ((((rgb444) >> 4) & 0xF) >> 1),          \
                                                               .ch.red     = ((((rgb444) >> 8) & 0xF) >> 1),}

#define sgl_rgb565_to_color(rgb565)             (sgl_color_t){ .ch.blue    = ((((rgb565) >> 0) & 0x1F) >> 3),         \
                                                               .ch.green   = ((((rgb565) >> 5) & 0x3F) >> 3),         \
                                                               .ch.red     = ((((rgb565) >> 11) & 0x1F) >> 2),}

#define sgl_rgb888_to_color(rgb888)             (sgl_color_t){ .ch.blue    = (((rgb888) >> 0)>> 6),                   \
                                                               .ch.green   = (((rgb888) >> 8) >> 5),                  \
                                                               .ch.red     = (((rgb888) >> 16) >> 6),}

#endif

/**
 * Define some basic colors
*/
#define SGL_COLOR_RED                           sgl_rgb(0xFF, 0, 0)
#define SGL_COLOR_GREEN                         sgl_rgb(0, 0xFF, 0)
#define SGL_COLOR_BLUE                          sgl_rgb(0, 0, 0xFF)
#define SGL_COLOR_WHITE                         sgl_rgb(0xFF, 0xFF, 0xFF)
#define SGL_COLOR_BLACK                         sgl_rgb(0, 0, 0)
#define SGL_COLOR_CYAN                          sgl_rgb(0, 0xFF, 0xFF)
#define SGL_COLOR_MAGENTA                       sgl_rgb(0xFF, 0, 0xFF)
#define SGL_COLOR_YELLOW                        sgl_rgb(0xFF, 0xFF, 0)
#define SGL_COLOR_GRAY                          sgl_rgb(0x80, 0x80, 0x80)
#define SGL_COLOR_DARK_GRAY                     sgl_rgb(0x40, 0x40, 0x40)
#define SGL_COLOR_LIGHT_GRAY                    sgl_rgb(0xC0, 0xC0, 0xC0)
#define SGL_COLOR_BRIGHT_PURPLE                 sgl_rgb(0xFF, 0x00, 0xFF)
#define SGL_COLOR_BRIGHT_BLUE                   sgl_rgb(0x00, 0xFF, 0xFF)
#define SGL_COLOR_ORANGE                        sgl_rgb(0xFF, 0xA5, 0x00)
#define SGL_COLOR_DARK_ORANGE                   sgl_rgb(0xFF, 0x8C, 0x00)
#define SGL_COLOR_RED_ORANGE                    sgl_rgb(0xFF, 0x45, 0x00)
#define SGL_COLOR_GOLD                          sgl_rgb(0xFF, 0xD7, 0x00)
#define SGL_COLOR_GOLDENROD                     sgl_rgb(0xDA, 0xA5, 0x20)
#define SGL_COLOR_BROWN                         sgl_rgb(0xA5, 0x2A, 0x2A)
#define SGL_COLOR_SADDLE_BROWN                  sgl_rgb(0x8B, 0x45, 0x13)
#define SGL_COLOR_TOMATO                        sgl_rgb(0xFF, 0x63, 0x47)
#define SGL_COLOR_CORAL                         sgl_rgb(0xFF, 0x7F, 0x50)
#define SGL_COLOR_SALMON                        sgl_rgb(0xFA, 0x80, 0x72)
#define SGL_COLOR_LIME                          sgl_rgb(0x32, 0xCD, 0x32)
#define SGL_COLOR_OLIVE                         sgl_rgb(0x80, 0x80, 0x00)
#define SGL_COLOR_DARK_OLIVE_GREEN              sgl_rgb(0x55, 0x6B, 0x2F)
#define SGL_COLOR_FOREST_GREEN                  sgl_rgb(0x22, 0x8B, 0x2F)
#define SGL_COLOR_DARK_GREEN                    sgl_rgb(0x00, 0x64, 0x00)
#define SGL_COLOR_LAWN_GREEN                    sgl_rgb(0x7C, 0xFC, 0x00)
#define SGL_COLOR_CHARTREUSE                    sgl_rgb(0x7F, 0xFF, 0x00)
#define SGL_COLOR_YELLOW_GREEN                  sgl_rgb(0x9A, 0xCD, 0x32)
#define SGL_COLOR_SPRING_GREEN                  sgl_rgb(0x00, 0xFF, 0x7F)
#define SGL_COLOR_DEEP_SKY_BLUE                 sgl_rgb(0x00, 0xBF, 0xFF)
#define SGL_COLOR_DODGER_BLUE                   sgl_rgb(0x1E, 0x90, 0xFF)
#define SGL_COLOR_ROYAL_BLUE                    sgl_rgb(0x41, 0x69, 0xE1)
#define SGL_COLOR_MIDNIGHT_BLUE                 sgl_rgb(0x19, 0x19, 0x70)
#define SGL_COLOR_NAVY                          sgl_rgb(0x00, 0x00, 0x80)
#define SGL_COLOR_TEAL                          sgl_rgb(0x00, 0x80, 0x80)
#define SGL_COLOR_DARK_CYAN                     sgl_rgb(0x00, 0x8B, 0x8B)
#define SGL_COLOR_LIGHT_SEA_GREEN               sgl_rgb(0x20, 0xB2, 0xAA)
#define SGL_COLOR_CADET_BLUE                    sgl_rgb(0x5F, 0x9E, 0xA0)
#define SGL_COLOR_INDIGO                        sgl_rgb(0x4B, 0x00, 0x82)
#define SGL_COLOR_VIOLET                        sgl_rgb(0xEE, 0x82, 0xEE)
#define SGL_COLOR_DARK_VIOLET                   sgl_rgb(0x94, 0x00, 0xD3)
#define SGL_COLOR_MEDIUM_VIOLET_RED             sgl_rgb(0xC7, 0x15, 0x85)
#define SGL_COLOR_PINK                          sgl_rgb(0xFF, 0xC0, 0xCB)
#define SGL_COLOR_HOT_PINK                      sgl_rgb(0xFF, 0x69, 0xB4)
#define SGL_COLOR_DEEP_PINK                     sgl_rgb(0xFF, 0x14, 0x93)
#define SGL_COLOR_PLUM                          sgl_rgb(0xDD, 0xA0, 0xDD)
#define SGL_COLOR_ORCHID                        sgl_rgb(0xDA, 0x70, 0xD6)
#define SGL_COLOR_TAN                           sgl_rgb(0xD2, 0xB4, 0x8C)
#define SGL_COLOR_WHEAT                         sgl_rgb(0xF5, 0xDE, 0xB3)
#define SGL_COLOR_SAND                          sgl_rgb(0xC2, 0xB2, 0x90)
#define SGL_COLOR_BEIGE                         sgl_rgb(0xF5, 0xF5, 0xDC)
#define SGL_COLOR_IVORY                         sgl_rgb(0xFF, 0xFF, 0xF0)
#define SGL_COLOR_LAVENDER                      sgl_rgb(0xE6, 0xE6, 0xFA)
#define SGL_COLOR_MISTY_ROSE                    sgl_rgb(0xFF, 0xE4, 0xE1)
#define SGL_COLOR_SEASHELL                      sgl_rgb(0xFF, 0xF5, 0xEE)
#define SGL_COLOR_FLORAL_WHITE                  sgl_rgb(0xFF, 0xFA, 0xF0)

#ifdef __cplusplus
}
#endif

#endif //__SGL_TYPES_H__
