/* source/include/sgl_misc.h
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

#ifndef __SGL_MISC_H__
#define __SGL_MISC_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <sgl_types.h>
#include <sgl_core.h>

#if (CONFIG_SGL_BOOT_LOGO)

/**
 * @brief to show the sgl logo after sgl init
 * @param none
 * @return none
 * @note: you can call this function in your main function to show the sgl logo
 */
void sgl_boot_logo(void);

#endif // ! CONFIG_SGL_BOOT_LOGO

#if (CONFIG_SGL_MONITOR_TRACE)
#define  SGL_MONITOR_COORDS_WIDTH       CONFIG_SGL_MONITOR_COORDS_WIDTH
#define  SGL_MONITOR_COORDS_HEIGHT      CONFIG_SGL_MONITOR_COORDS_HEIGHT
#define  SGL_MONITOR_COORDS_X           (SGL_SCREEN_WIDTH - SGL_MONITOR_COORDS_WIDTH)
#define  SGL_MONITOR_COORDS_Y           (SGL_SCREEN_HEIGHT - SGL_MONITOR_COORDS_HEIGHT)
#define  SGL_MONITOR_COLOR              CONFIG_SGL_MONITOR_COLOR
#define  SGL_MONITOR_TEXT_COLOR         CONFIG_SGL_MONITOR_TEXT_COLOR
#define  SGL_MONITOR_ALPHA              CONFIG_SGL_MONITOR_ALPHA

#define  SGL_MONITOR_COORDS             (sgl_area_t){.x1 = SGL_MONITOR_COORDS_X,     \
                                                     .x2 = SGL_MONITOR_COORDS_X + SGL_MONITOR_COORDS_WIDTH - 1,     \
                                                     .y1 = SGL_MONITOR_COORDS_Y,     \
                                                     .y2 = SGL_MONITOR_COORDS_Y + SGL_MONITOR_COORDS_HEIGHT - 1,    \
                                                    }

void sgl_monitor_trace(sgl_surf_t *surf);
#endif // ! CONFIG_SGL_MONITOR_TRACE

/**
 * @brief Count number of options in \n-separated text
 * @param text newline-separated option string
 * @return number of options
 */
uint16_t sgl_string_option_get_count(const char *text);

/**
 * @brief Get byte offset of the Nth option in \n-separated text
 * @param text newline-separated option string
 * @param index zero-based option index
 * @return byte offset, or -1 if out of range
 */
int sgl_string_option_get_offset(const char *text, int index);

/**
 * @brief Get text length of one option at given byte offset
 * @param text option string
 * @param offset byte offset of the option
 * @return length (stops at \n or \0)
 */
int sgl_string_option_get_text_len(const char *text, int offset);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif // ! __SGL_MISC_H__
