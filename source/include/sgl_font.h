/* source/include/sgl_font.h
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

#ifndef __SGL_FONT_H__
#define __SGL_FONT_H__

#include <sgl_cfgfix.h>
#include <stddef.h>
#include <sgl_list.h>
#include <sgl_types.h>
#include <sgl_core.h>

/* declare all font */

#if CONFIG_SGL_FONT_SONG23
extern const sgl_font_t song23;
#endif

#if CONFIG_SGL_FONT_CONSOLAS14
extern const sgl_font_t consolas14;
#if (CONFIG_SGL_FLASH_FONT)
extern const sgl_font_t consolas14_flash;
extern const sgl_font_t consolas14_flash_fixed;
void sgl_consolas14_flash_fixed_init(void);
uint32_t sgl_consolas14_flash_read_count(void);
#endif
#endif

#if CONFIG_SGL_FONT_CONSOLAS23
extern const sgl_font_t consolas23;
#endif

#if CONFIG_SGL_FONT_CONSOLAS24
extern const sgl_font_t consolas24;
#endif

#if CONFIG_SGL_FONT_CONSOLAS32
extern const sgl_font_t consolas32;
#endif

#if CONFIG_SGL_FONT_KAI33
extern const sgl_font_t kai33;
#endif

#if CONFIG_SGL_FONT_CONSOLAS24_COMPRESS
extern const sgl_font_t consolas24_compress;
#endif

/* External flash font support (CONFIG_SGL_FLASH_FONT)
 *
 * Reference LVGL binfont: keep font table/unicode in code (internal flash),
 * only the bitmap blob is stored in external flash and read on demand.
 *
 * Usage:
 *   1. set .bitmap = NULL in the font descriptor
 *   2. set .flash_addr to the base address of the bitmap blob in external flash,
 *      table[].bitmap_index is the offset relative to it
 *   3. set .flash_read to the platform read callback
 *
 * Example:
 *   static int32_t my_flash_read(uint32_t addr, void *buf, uint32_t len)
 *   {
 *       // read [addr, addr + len) from external flash into buf
 *       return (int32_t)len;
 *   }
 *
 *   const sgl_font_t my_flash_font = {
 *       .bitmap = NULL,
 *       .table = my_font_table,
 *       .font_table_size = SGL_ARRAY_SIZE(my_font_table),
 *       .font_height = 24,
 *       .base_line = 5,
 *       .bpp = 4,
 *       .unicode = my_font_unicode,
 *       .unicode_num = SGL_ARRAY_SIZE(my_font_unicode),
 *       .flash_read = my_flash_read,
 *       .flash_addr = 0x00100000,
 *   };
 */

#endif // !__SGL_FONT_H__
