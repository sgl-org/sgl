/* source/include/sgl_cfgfix.h
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

#ifndef __SGL_CFGFIX_H__
#define __SGL_CFGFIX_H__

#include <sgl_config.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief sgl config fix file, it will fix the macro if not defined
 * @note please add the macro in this file if you want to add new macro into source code
 */

#ifndef CONFIG_SGL_FBDEV_PIXEL_DEPTH
#define CONFIG_SGL_FBDEV_PIXEL_DEPTH                               (16)  /* FBDEV pixel depth (16/24/32) */
#endif

#ifndef CONFIG_SGL_FBDEV_ROTATION
#define CONFIG_SGL_FBDEV_ROTATION                                  (0)  /* FBDEV rotation (0/90/180/270 deg) */
#endif

#ifndef CONFIG_SGL_FBDEV_RUNTIME_ROTATION
#define CONFIG_SGL_FBDEV_RUNTIME_ROTATION                          (0)  /* Runtime display rotation enable */
#endif

#ifndef CONFIG_SGL_FBDEV_EVEN_COORDS
#define CONFIG_SGL_FBDEV_EVEN_COORDS                               (0)  /* FBDEV even coordinates */
#endif

#ifndef CONFIG_SGL_USE_FBDEV_VRAM
#define CONFIG_SGL_USE_FBDEV_VRAM                                  (0)  /* Use FBDEV VRAM directly */
#endif

#ifndef CONFIG_SGL_SYSTICK_MS
#define CONFIG_SGL_SYSTICK_MS                                      (10)  /* System tick interval (ms) */
#endif

#ifndef CONFIG_SGL_COLOR16_SWAP
#define CONFIG_SGL_COLOR16_SWAP                                    (0)  /* Swap 16-bit color bytes */
#endif

#ifndef CONFIG_SGL_EVENT_QUEUE_SIZE
#define CONFIG_SGL_EVENT_QUEUE_SIZE                                (16)  /* Max input event queue size */
#endif

#ifndef CONFIG_SGL_FOCUSED_COLOR
#define CONFIG_SGL_FOCUSED_COLOR                                   sgl_rgb(0x00, 0xFF, 0x00) /* Focused color */
#endif

#ifndef CONFIG_SGL_FOCUSED_WIDTH
#define CONFIG_SGL_FOCUSED_WIDTH                                   (1)  /* Focused width */
#endif

#ifndef CONFIG_SGL_DIRTY_AREA_NUM_MAX
#define CONFIG_SGL_DIRTY_AREA_NUM_MAX                              (16)  /* Max dirty regions for partial refresh */
#endif

#ifndef CONFIG_SGL_DIRTY_AREA_TRACE
#define CONFIG_SGL_DIRTY_AREA_TRACE                                (0)  /* Dirty region debug trace */
#endif

#ifndef CONFIG_SGL_DIRTY_AREA_TRACE_COLOR
#define CONFIG_SGL_DIRTY_AREA_TRACE_COLOR                          sgl_rgb(0, 0, 0)  /* Dirty region debug trace color */
#endif

#ifndef CONFIG_SGL_MONITOR_TRACE
#define CONFIG_SGL_MONITOR_TRACE                                   (0)  /* Monitor debug trace */
#endif

#ifndef CONFIG_SGL_MONITOR_COORDS_WIDTH
#define CONFIG_SGL_MONITOR_COORDS_WIDTH                            (81)  /* Monitor width */
#endif

#ifndef CONFIG_SGL_MONITOR_COORDS_HEIGHT
#define CONFIG_SGL_MONITOR_COORDS_HEIGHT                           (30)  /* Monitor height */
#endif

#ifndef CONFIG_SGL_MONITOR_COLOR
#define CONFIG_SGL_MONITOR_COLOR                                   (SGL_COLOR_BLACK)  /* Monitor background */
#endif

#ifndef CONFIG_SGL_MONITOR_TEXT_COLOR
#define CONFIG_SGL_MONITOR_TEXT_COLOR                              (SGL_COLOR_WHITE)  /* Monitor text */
#endif

#ifndef CONFIG_SGL_MONITOR_ALPHA
#define CONFIG_SGL_MONITOR_ALPHA                                   (128)  /* Monitor page alpha */
#endif

#ifndef CONFIG_SGL_PIXMAP_BILINEAR_INTERP
#define CONFIG_SGL_PIXMAP_BILINEAR_INTERP                          (0)  /* Pixmap bilinear interpolation */
#endif

#ifndef CONFIG_SGL_ANIMATION
#define CONFIG_SGL_ANIMATION                                       (0)  /* Global animation enable */
#endif

#ifndef CONFIG_SGL_DEBUG
#   define CONFIG_SGL_DEBUG                                        (0)  /* Global debug log enable */
#elif (CONFIG_SGL_DEBUG == 1)
#   ifndef CONFIG_SGL_LOG_COLOR
#       define CONFIG_SGL_LOG_COLOR                                (0)  /* Colored log output */
#   endif
#   ifndef CONFIG_SGL_LOG_LEVEL
#       define CONFIG_SGL_LOG_LEVEL                                (1)  /* Log level (err/warn/info/debug) */
#   endif
#endif

#ifndef CONFIG_SGL_OBJ_USE_NAME
#define CONFIG_SGL_OBJ_USE_NAME                                    (0)  /* Enable object name support */
#endif

#ifndef CONFIG_SGL_HEAP_ALGO
#define CONFIG_SGL_HEAP_ALGO                                       (lwmem)  /* Heap allocation algorithm */
#endif

#ifndef CONFIG_SGL_HEAP_MEMORY_SIZE
#   define CONFIG_SGL_HEAP_MEMORY_SIZE                             (10240)  /* GUI heap size (bytes) */
#   ifndef CONFIG_SGL_TLSF_INDEX_MAX
#       define CONFIG_SGL_TLSF_INDEX_MAX                           (13)  /* Max font link index */
#   endif
#endif

#ifndef CONFIG_SGL_FONT_COMPRESSED
#define CONFIG_SGL_FONT_COMPRESSED                                 (0)  /* Enable font compression */
#endif

#ifndef CONFIG_SGL_FONT_SMALL_TABLE
#define CONFIG_SGL_FONT_SMALL_TABLE                                (1)  /* Use small font table */
#endif

#ifndef CONFIG_SGL_FONT_SONG23
#define CONFIG_SGL_FONT_SONG23                                     (0)  /* Enable Song23 font */
#endif

#ifndef CONFIG_SGL_FONT_CONSOLAS14
#define CONFIG_SGL_FONT_CONSOLAS14                                 (0)  /* Enable Consolas14 font */
#endif

#ifndef CONFIG_SGL_FONT_CONSOLAS23
#define CONFIG_SGL_FONT_CONSOLAS23                                 (0)  /* Enable Consolas23 font */
#endif

#ifndef CONFIG_SGL_FONT_CONSOLAS24
#define CONFIG_SGL_FONT_CONSOLAS24                                 (0)  /* Enable Consolas24 font */
#endif

#ifndef CONFIG_SGL_FONT_CONSOLAS32
#define CONFIG_SGL_FONT_CONSOLAS32                                 (0)  /* Enable Consolas32 font */
#endif

#if !(CONFIG_SGL_THEME_DARK || CONFIG_SGL_THEME_LIGHT)
#   ifndef CONFIG_SGL_THEME_DEFAULT
#   define CONFIG_SGL_THEME_DEFAULT                                (1)  /* Default theme if not specified */
#   endif
#endif

#ifndef CONFIG_SGL_BOOT_LOGO
#define CONFIG_SGL_BOOT_LOGO                                       (1)  /* Show boot logo */
#endif

#ifdef __cplusplus
}
#endif

#endif // !__SGL_CFGFIX_H__
