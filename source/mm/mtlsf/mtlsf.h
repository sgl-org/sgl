/* source/mm/mtlsf/mtlsf.h
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
 * AUTHORS OR COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __MTLSF_H__
#define __MTLSF_H__

#include <stddef.h>
#include <sgl_cfgfix.h>

#define MEM_SIZE_BYTES                  (CONFIG_SGL_HEAP_MEMORY_SIZE)
#define _FLS_STEP1(x)                   ((x) & 0xFFFFFFFF00000000ULL ? 32 + _FLS_STEP2((x) >> 32) : _FLS_STEP2(x))
#define _FLS_STEP2(x)                   ((x) & 0xFFFF0000ULL         ? 16 + _FLS_STEP3((x) >> 16) : _FLS_STEP3(x))
#define _FLS_STEP3(x)                   ((x) & 0xFF00ULL             ?  8 + _FLS_STEP4((x) >> 8)  : _FLS_STEP4(x))
#define _FLS_STEP4(x)                   ((x) & 0xF0ULL               ?  4 + _FLS_STEP5((x) >> 4)  : _FLS_STEP5(x))
#define _FLS_STEP5(x)                   ((x) & 0xCULL                ?  2 + _FLS_STEP6((x) >> 2)  : _FLS_STEP6(x))
#define _FLS_STEP6(x)                   ((x) & 0x2ULL                ?  1                         : 0)
#define _MEM_FLS64(x)                   ((x) == 0 ? 0 : _FLS_STEP1((unsigned long long)(x)))
# define MEM_FL_INDEX_MAX               (_MEM_FLS64((size_t)(MEM_SIZE_BYTES)) + 1)
#define MEM_MIN_POOL_SIZE               (2048U)

#define MEM_ALIGN_SIZE_LOG2             (3)
#define MEM_ALIGN_SIZE                  ((size_t)1 << MEM_ALIGN_SIZE_LOG2)
#define SL_INDEX_COUNT_LOG2             (4)
#define SL_INDEX_COUNT                  (1u << SL_INDEX_COUNT_LOG2)
#define FL_INDEX_MAX                    MEM_FL_INDEX_MAX
#define FL_INDEX_SHIFT                  (SL_INDEX_COUNT_LOG2 + MEM_ALIGN_SIZE_LOG2)
#define FL_INDEX_COUNT                  (FL_INDEX_MAX - FL_INDEX_SHIFT + 1)
#define SMALL_BLOCK_SIZE                (1u << FL_INDEX_SHIFT)
#define BH_FREE_BIT                     ((size_t)1)
#define BH_PREV_FREE_BIT                ((size_t)2)
#define BH_FLAG_MASK                    (BH_FREE_BIT | BH_PREV_FREE_BIT)

typedef struct mem_stats {
    size_t pool_bytes;
    size_t used_bytes;
    size_t free_bytes;
    size_t used_blocks;
    size_t free_blocks;
    size_t overhead_bytes;
    size_t largest_free;
} mem_stats_t;

typedef struct mem_blk_node {
    struct mem_blk_node *prev_phys_block;
    size_t               size;
    struct mem_blk_node *next_free;
    struct mem_blk_node *prev_free;
} mem_blk_node_t;

typedef struct mem_control {
    mem_blk_node_t block_null;
    unsigned int fl_bitmap;
    unsigned int sl_bitmap[FL_INDEX_COUNT];
    mem_blk_node_t *blocks[FL_INDEX_COUNT][SL_INDEX_COUNT];
    void  *pool_start;
    size_t pool_size;
} mem_control_t;

typedef void (*mem_walker)(void *ptr, size_t size, int used, void *user);

#define BLOCK_START_OFFSET              (offsetof(mem_blk_node_t, size) + sizeof(size_t))
#define BLOCK_OVERHEAD                  (sizeof(size_t))
#define BLOCK_SIZE_MIN                  (sizeof(mem_blk_node_t) - sizeof(void*))
#define BLOCK_SIZE_MAX                  ((size_t)1 << FL_INDEX_MAX)

/**
 * @brief Create a TLSF memory pool from a given memory region
 * @param[in] mem_start: start address of the memory region to manage
 * @param[in] len: length of the memory region in bytes
 * @return 0 is success, otherwise is failure
 */
int mtlsf_mem_init(void *mem_start, size_t len);

/**
 * @brief Allocate a block of memory from the TLSF pool
 * @param[in] size: requested size
 * @return void *: pointer to allocated memory, or NULL on failure
 */
void* mtlsf_malloc (size_t size);

/**
 * @brief Allocate a block of memory from the TLSF pool and zero it
 * @param[in] size: requested size
 * @return void *: pointer to allocated memory, or NULL on failure
 */
void *mtlsf_zalloc(size_t size);

/**
 * @brief Allocate and zero-initialize a block of memory
 * @param[in] nmemb: number of elements
 * @param[in] size: size of each element
 * @return void *: pointer to allocated memory, or NULL on failure
 */
void* mtlsf_calloc (size_t nmemb, size_t size);

/**
 * @brief Reallocate a block to a new size, preserving content
 * @param[in] ptr: existing pointer (NULL acts like malloc)
 * @param[in] size: new size (0 acts like free)
 * @return void *: pointer to reallocated memory, or NULL on failure
 */
void* mtlsf_realloc (void *ptr, size_t size);

/**
 * @brief Allocate memory with a specific alignment requirement
 * @param[in] align: alignment (must be power of 2)
 * @param[in] size: requested size
 * @return void *: aligned pointer to allocated memory, or NULL on failure
 */
void* mtlsf_memalign(size_t align, size_t size);

/**
 * @brief Free a previously allocated block back to the pool
 * @param[in] ptr: pointer to free (NULL is safe)
 */
void mtlsf_free  (void *ptr);

/**
 * @brief Get the usable size of an allocated block
 * @param[in] ptr: pointer to allocated memory
 * @return size_t: block size, or 0 if ptr is NULL
 */
size_t mtlsf_mem_block_size(void *ptr);

/**
 * @brief Walk all blocks in the pool, calling a callback for each
 * @param[in] walker: callback function (NULL uses the default walker)
 * @param[in] user: user context passed to the callback
 */
void mtlsf_mem_walk(mem_walker walker, void *user);

/**
 * @brief Dump pool statistics and walk all blocks
 */
void mtlsf_mem_dump(void);

#endif /* __MTLSF_H__ */
