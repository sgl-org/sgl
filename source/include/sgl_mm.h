/* source/include/sgl_mm.h
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

#ifndef __SGL_MM_H__
#define __SGL_MM_H__

#include <sgl_core.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  memory monitor info
 * @total_size: total size of memory
 * @free_size: free size of memory
 * @used_size: used size of memory
 * @used_rate: used rate of memory:
 *             |  8 bit  |  8 bit |          
 *             |   int   |   dec  |
 */
typedef struct sgl_mm_monitor {
    size_t  total_size;
    size_t  free_size;
    size_t  used_size;
    size_t  used_rate;
} sgl_mm_monitor_t;

/**
 * @brief  initialize memory pool
 * @param  mem_start  start address of memory pool
 * @param  len  length of memory pool
 */
void sgl_mm_init(void *mem_start, size_t len);

/**
 * @brief  add memory pool
 * @param  mem_start  start address of memory pool
 * @param  len  length of memory pool
 */
void sgl_mm_add_pool(void *mem_start, size_t len);

/**
 * @brief  memory alloc, the function is unsafe, you should ensure that 
 *         the requested size is smaller than the free size of memory
 * @param  size   request size of memory
 * @return point to request memory address
*/
void* sgl_malloc(size_t size);

/**
 * @brief  memory realloc, the function is unsafe, you should ensure that 
 *         the requested size is smaller than the free size of memory
 * @param  p      the pointer of request size of memory
 * @param  size   request size of memory
 */
void* sgl_realloc(void *p, size_t size);

/**
 * @brief  memory free
 * @param  p  the pointer of request size of memory
 * @return none
*/
void sgl_free(void *p);

/**
 * @brief  get memory monitor info
 * @param  none
 * @return memory monitor info
 */
sgl_mm_monitor_t sgl_mm_get_monitor(void);

#ifdef __cplusplus
}
#endif

#endif // !__SGL_MM_H__
