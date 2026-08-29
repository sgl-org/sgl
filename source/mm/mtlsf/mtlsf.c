/**
 * MIT License
 * 
 * Copyright (c) 2026 - present @ skyper
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include "mtlsf.h"
#include <sgl_mm.h>
#include <sgl_log.h>
#include <sgl_cfgfix.h>

/**
 * @brief Base address of the memory region managed by the allocator, the
 *        control structure is placed at the very beginning of this region
 */
static void *g_mem_base = NULL;

/**
 * @brief Round a size up to the nearest multiple of align (power of 2)
 * @param[in] x: value to align
 * @param[in] align: alignment (must be power of 2)
 * @return size_t: aligned value
 */
static inline size_t align_up(size_t x, size_t align)
{
    return (x + (align - 1)) & ~(align - 1);
}

/**
 * @brief Round a size down to the nearest multiple of align (power of 2)
 * @param[in] x: value to align
 * @param[in] align: alignment (must be power of 2)
 * @return size_t: aligned value
 */
static inline size_t align_down(size_t x, size_t align)
{
    return x - (x & (align - 1));
}

/**
 * @brief Round a pointer up to the nearest alignment boundary (power of 2)
 * @param[in] ptr: pointer to align
 * @param[in] align: alignment (must be power of 2)
 * @return void *: aligned pointer
 */
static inline void *align_ptr(const void *ptr, size_t align)
{
    return (void *)(((uintptr_t)ptr + (align - 1)) & ~(uintptr_t)(align - 1));
}

/**
 * @brief Get the memory control structure
 * @return mem_control_t *: pointer to the memory control structure
 */
static inline mem_control_t *get_control(void)
{
    return (mem_control_t *)g_mem_base;
}

/**
 * @brief Find the index of the least significant set bit (ffs = find first set)
 * @param[in] w: word to scan
 * @return int: bit index (0-based), or -1 if w is 0
 */
static inline int mem_ffs(unsigned int w)
{
    return w ? __builtin_ctz(w) : -1;
}

/**
 * @brief Find the index of the most significant set bit (fls = find last set)
 * @param[in] w: word to scan
 * @return int: bit index (0-based), or -1 if w is 0
 */
static inline int mem_fls(unsigned int w)
{
    return w ? (31 - __builtin_clz(w)) : -1;
}

/**
 * @brief Find the last set bit index for size_t (handles 64-bit width)
 * @param[in] s: size_t value to scan
 * @return int: bit index (0-based), or -1 if s is 0
 */
static inline int mem_fls_sizet(size_t s)
{
    unsigned int hi = (unsigned int)(s >> 32);
    if (hi) {
        return 32 + mem_fls(hi);
    }
    return mem_fls((unsigned int)s);
}

/**
 * @brief Get the usable size of a block (excluding flag bits)
 * @param[in] b: block header
 * @return size_t: usable size
 */
static inline size_t block_size(const mem_blk_node_t *b)
{
    return b->size & ~BH_FLAG_MASK;
}

/**
 * @brief Set the size of a block, preserving the flag bits
 * @param[in] b: block header
 * @param[in] s: new size
 */
static inline void block_set_size(mem_blk_node_t *b, size_t s)
{
    b->size = s | (b->size & BH_FLAG_MASK);
}

/**
 * @brief Check if block is the sentinel (last) block
 * @param[in] b: block header
 * @return int: 1 if last, 0 otherwise
 */
static inline int  block_is_last(const mem_blk_node_t *b)
{
    return block_size(b) == 0;
}

/**
 * @brief Check if block is marked as free
 * @param[in] b: block header
 * @return int: 1 if free, 0 if used
 */
static inline int  block_is_free(const mem_blk_node_t *b)
{
    return (b->size & BH_FREE_BIT) != 0;
}

/**
 * @brief Mark block as free (set BH_FREE_BIT)
 * @param[in] b: block header
 */
static inline void block_set_free(mem_blk_node_t *b)
{
    b->size |= BH_FREE_BIT;
}

/**
 * @brief Mark block as used (clear BH_FREE_BIT)
 * @param[in] b: block header
 */
static inline void block_set_used(mem_blk_node_t *b)
{
    b->size &= ~BH_FREE_BIT;
}

/**
 * @brief Check if the previous block is free (via BH_PREV_FREE_BIT in this block)
 * @param[in] b: block header
 * @return int: 1 if previous block is free, 0 otherwise
 */
static inline int  block_is_prev_free(const mem_blk_node_t *b)
{
    return (b->size & BH_PREV_FREE_BIT) != 0;
}

/**
 * @brief Mark previous block as free (set BH_PREV_FREE_BIT in this block)
 * @param[in] b: block header
 */
static inline void block_set_prev_free(mem_blk_node_t *b)
{
    b->size |= BH_PREV_FREE_BIT;
}

/**
 * @brief Mark previous block as used (clear BH_PREV_FREE_BIT in this block)
 * @param[in] b: block header
 */
static inline void block_set_prev_used(mem_blk_node_t *b)
{
    b->size &= ~BH_PREV_FREE_BIT;
}

/**
 * @brief Get the block header from a user pointer (subtract header offset)
 * @param[in] p: user pointer (returned by mtlsf_malloc etc.)
 * @return mem_blk_node_t *: pointer to the block header
 */
static inline mem_blk_node_t *block_from_ptr(const void *p)
{
    return (mem_blk_node_t *)((unsigned char *)p - BLOCK_START_OFFSET);
}

/**
 * @brief Get the user pointer from a block header (skip header overhead)
 * @param[in] b: block header
 * @return void *: pointer to the usable data area
 */
static inline void *block_to_ptr(const mem_blk_node_t *b)
{
    return (void *)((unsigned char *)b + BLOCK_START_OFFSET);
}

/**
 * @brief Get the next physical block in memory
 * @param[in] b: current block header
 * @return mem_blk_node_t *: next block header
 */
static inline mem_blk_node_t *block_next(const mem_blk_node_t *b)
{
    mem_blk_node_t *n = (mem_blk_node_t *)((unsigned char *)block_to_ptr(b)
                                           + block_size(b) - BLOCK_OVERHEAD);
    return n;
}

/**
 * @brief Get the previous physical block (requires prev block to be free)
 * @param[in] b: block header
 * @return mem_blk_node_t *: previous block header
 */
static inline mem_blk_node_t *block_prev(const mem_blk_node_t *b)
{
    SGL_ASSERT(block_is_prev_free(b) && "prev must be free");
    return b->prev_phys_block;
}

/**
 * @brief Link the next block's prev_phys_block back to this block
 * @param[in] b: current block header
 * @return mem_blk_node_t *: next block header
 */
static inline mem_blk_node_t *block_link_next(mem_blk_node_t *b)
{
    mem_blk_node_t *n = block_next(b);
    n->prev_phys_block = b;
    return n;
}

/**
 * @brief Mark a block as free and update the next block's prev-free flag
 * @param[in] b: block header to mark free
 */
static inline void block_mark_as_free(mem_blk_node_t *b)
{
    mem_blk_node_t *n = block_link_next(b);
    block_set_prev_free(n);
    block_set_free(b);
}

/**
 * @brief Mark a block as used and update the next block's prev-free flag
 * @param[in] b: block header to mark used
 */
static inline void block_mark_as_used(mem_blk_node_t *b)
{
    mem_blk_node_t *n = block_next(b);
    block_set_prev_used(n);
    block_set_used(b);
}

/**
 * @brief Map a block size to first-level (fl) and second-level (sl) index
 * @param[in] size: block size to map
 * @param[out] fli: first-level index
 * @param[out] sli: second-level index
 */
static void mapping_insert(size_t size, int *fli, int *sli)
{
    int fl, sl;
    if (size < SMALL_BLOCK_SIZE) {
        fl = 0;
        sl = (int)(size / (SMALL_BLOCK_SIZE / SL_INDEX_COUNT));
    } else {
        fl = mem_fls_sizet(size);
        sl = (int)((size >> (fl - SL_INDEX_COUNT_LOG2)) ^ (1u << SL_INDEX_COUNT_LOG2));
        fl -= (FL_INDEX_SHIFT - 1);
    }

    *fli = fl;
    *sli = sl;
}

/**
 * @brief Search for a size round up to the nearest valid block size, then map to (fl, sl)
 * @param[in] size: requested size (may be rounded up)
 * @param[out] fli: first-level index
 * @param[out] sli: second-level index
 */
static void mapping_search(size_t size, int *fli, int *sli)
{
    if (size >= SMALL_BLOCK_SIZE) {
        size_t round = ((size_t)1 << (mem_fls_sizet(size) - SL_INDEX_COUNT_LOG2)) - 1;
        size += round;
    }
    mapping_insert(size, fli, sli);
}

/**
 * @brief Search the bitmap for a suitable free block >= the requested (fl, sl)
 * @param[in] c: control structure
 * @param[in,out] fli: first-level index (may be updated if a larger FL is used)
 * @param[in,out] sli: second-level index (may be updated)
 * @return mem_blk_node_t *: a suitable free block, or NULL if none found
 */
static mem_blk_node_t *search_suitable_block(mem_control_t *c, int *fli, int *sli)
{
    int fl = *fli, sl = *sli;
    unsigned int fl_map;
    unsigned int sl_map = c->sl_bitmap[fl] & (~0u << sl);

    if (!sl_map) {
        fl_map = c->fl_bitmap & (~0u << (fl + 1));
        if (!fl_map) {
            return NULL;
        }

        fl = mem_ffs(fl_map);
        *fli = fl;
        sl_map = c->sl_bitmap[fl];
    }
    sl = mem_ffs(sl_map);
    *sli = sl;

    return c->blocks[fl][sl];
}

static void remove_free_block(mem_control_t *c, mem_blk_node_t *b, int fl, int sl)
{
    mem_blk_node_t *prev = b->prev_free;
    mem_blk_node_t *next = b->next_free;
    next->prev_free = prev;
    prev->next_free = next;

    if (c->blocks[fl][sl] == b) {
        c->blocks[fl][sl] = next;
        if (next == &c->block_null) {
            c->sl_bitmap[fl] &= ~(1u << sl);
            if (!c->sl_bitmap[fl]) {
                c->fl_bitmap &= ~(1u << fl);
            }
        }
    }
}

/**
 * @brief Insert a free block into the free list at (fl, sl)
 * @param[in] c: control structure
 * @param[in] b: block to insert
 * @param[in] fl: first-level index
 * @param[in] sl: second-level index
 */
static void insert_free_block(mem_control_t *c, mem_blk_node_t *b, int fl, int sl)
{
    mem_blk_node_t *cur = c->blocks[fl][sl];
    b->next_free = cur;
    b->prev_free = &c->block_null;
    cur->prev_free = b;
    c->blocks[fl][sl] = b;
    c->fl_bitmap    |= (1u << fl);
    c->sl_bitmap[fl] |= (1u << sl);
}

/**
 * @brief Remove a free block from the free lists (looks up its (fl, sl) by size)
 * @param[in] c: control structure
 * @param[in] b: block to remove
 */
static void block_remove(mem_control_t *c, mem_blk_node_t *b)
{
    int fl, sl;
    mapping_insert(block_size(b), &fl, &sl);
    remove_free_block(c, b, fl, sl);
}

/**
 * @brief Insert a free block into the free lists (looks up its (fl, sl) by size)
 * @param[in] c: control structure
 * @param[in] b: block to insert
 */
static void block_insert(mem_control_t *c, mem_blk_node_t *b)
{
    int fl, sl;
    mapping_insert(block_size(b), &fl, &sl);
    insert_free_block(c, b, fl, sl);
}

/**
 * @brief Check if a block can be split into two: used portion + remaining free block
 * @param[in] b: block to check
 * @param[in] size: requested size for the first portion
 * @return int: 1 if splittable, 0 otherwise
 */
static int block_can_split(mem_blk_node_t *b, size_t size)
{
    return block_size(b) >= sizeof(mem_blk_node_t) + size;
}

/**
 * @brief Split a block into two: first of given size, second as a new free remainder
 * @param[in] b: block to split
 * @param[in] size: size for the first portion (must be valid per block_can_split)
 * @return mem_blk_node_t *: the remainder (free) block
 */
static mem_blk_node_t *block_split(mem_blk_node_t *b, size_t size)
{
    mem_blk_node_t *rem = (mem_blk_node_t *)((unsigned char *)block_to_ptr(b)
                                             + size - BLOCK_OVERHEAD);
    size_t rem_size = block_size(b) - (size + BLOCK_OVERHEAD);
    block_set_size(rem, rem_size);
    SGL_ASSERT(block_size(rem) >= BLOCK_SIZE_MIN);
    block_set_size(b, size);
    block_mark_as_free(rem);
    return rem;
}

/**
 * @brief Absorb a following block into the current block (coalescing)
 * @param[in] prev: block that will absorb
 * @param[in] b: block being absorbed (must be adjacent)
 * @return mem_blk_node_t *: the merged block (prev)
 */
static mem_blk_node_t *block_absorb(mem_blk_node_t *prev, mem_blk_node_t *b)
{
    prev->size += block_size(b) + BLOCK_OVERHEAD;
    block_link_next(prev);
    return prev;
}

/**
 * @brief Coalesce block with its previous free neighbor
 * @param[in] c: control structure
 * @param[in] b: block to merge with its predecessor
 * @return mem_blk_node_t *: the merged block (previous block's address)
 */
static mem_blk_node_t *block_merge_prev(mem_control_t *c, mem_blk_node_t *b)
{
    if (block_is_prev_free(b)) {
        mem_blk_node_t *p = block_prev(b);
        block_remove(c, p);
        b = block_absorb(p, b);
    }
    return b;
}

/**
 * @brief Coalesce block with its next free neighbor
 * @param[in] c: control structure
 * @param[in] b: block to merge with its successor
 * @return mem_blk_node_t *: the merged block
 */
static mem_blk_node_t *block_merge_next(mem_control_t *c, mem_blk_node_t *b)
{
    mem_blk_node_t *n = block_next(b);
    if (!block_is_last(n) && block_is_free(n)) {
        block_remove(c, n);
        b = block_absorb(b, n);
    }
    return b;
}

/**
 * @brief Trim a free block to the requested size; insert the remainder
 * @param[in] c: control structure
 * @param[in] b: free block to trim
 * @param[in] size: target size for the block
 */
static void block_trim_free(mem_control_t *c, mem_blk_node_t *b, size_t size)
{
    SGL_ASSERT(block_is_free(b));
    if (block_can_split(b, size)) {
        mem_blk_node_t *rem = block_split(b, size);
        block_link_next(b);
        block_set_prev_free(rem);

        rem = block_merge_next(c, rem);
        block_insert(c, rem);
    }
}

/**
 * @brief Trim a used block to the requested size; insert the remainder as free
 * @param[in] c: control structure
 * @param[in] b: used block to trim
 * @param[in] size: target size for the block
 */
static void block_trim_used(mem_control_t *c, mem_blk_node_t *b, size_t size)
{
    SGL_ASSERT(!block_is_free(b));
    if (block_can_split(b, size)) {
        mem_blk_node_t *rem = block_split(b, size);
        block_set_prev_used(rem);
        rem = block_merge_next(c, rem);
        block_insert(c, rem);
    }
}

/**
 * @brief Trim leading bytes from a free block to create a gap for alignment
 * @param[in] c: control structure
 * @param[in] b: free block to split
 * @param[in] size: size of the leading gap
 * @return mem_blk_node_t *: the remainder block (after the gap)
 */
static mem_blk_node_t *block_trim_free_leading(mem_control_t *c, mem_blk_node_t *b, size_t size)
{
    mem_blk_node_t *rem = b;

    if (block_can_split(b, size)) {
        rem = block_split(b, size - BLOCK_OVERHEAD);
        block_set_prev_free(rem);
        block_link_next(b);
        block_insert(c, b);
    }
    return rem;
}

/**
 * @brief Locate a suitable free block for the requested size, remove it from lists
 * @param[in] c: control structure
 * @param[in] size: requested size
 * @return mem_blk_node_t *: a free block with enough space, or NULL
 */
static mem_blk_node_t *block_locate_free(mem_control_t *c, size_t size)
{
    int fl = 0, sl = 0;
    mem_blk_node_t *b = NULL;

    if (size) {
        mapping_search(size, &fl, &sl);
        if (fl < FL_INDEX_COUNT) {
            b = search_suitable_block(c, &fl, &sl);
        }
    }
    if (b) {
        SGL_ASSERT(block_size(b) >= size);
        remove_free_block(c, b, fl, sl);
    }
    return b;
}

/**
 * @brief Prepare a block for use: trim to size, mark used, return user pointer
 * @param[in] c: control structure
 * @param[in] b: block to prepare
 * @param[in] size: requested size
 * @return void *: user pointer to the block's data area, or NULL
 */
static void *block_prepare_used(mem_control_t *c, mem_blk_node_t *b, size_t size)
{
    if (!b) {
        return NULL;
    }

    block_trim_free(c, b, size);
    block_mark_as_used(b);
    return block_to_ptr(b);
}

/**
 * @brief Adjust a request size: align up, enforce min/max bounds
 * @param[in] size: requested allocation size
 * @param[in] align: alignment requirement (must be power of 2)
 * @return size_t: adjusted size, or 0 if invalid
 */
static size_t adjust_request_size(size_t size, size_t align)
{
    size_t a;

    if (!size) {
        return 0;
    }
    a = align_up(size, align);
    if (a >= BLOCK_SIZE_MAX) {
        return 0;
    }
    return a < BLOCK_SIZE_MIN ? BLOCK_SIZE_MIN : a;
}

/**
 * @brief Initialize the control structure (all free lists point to block_null)
 * @param[in] c: control structure to initialize
 */
static void control_construct(mem_control_t *c)
{
    c->block_null.next_free = &c->block_null;
    c->block_null.prev_free = &c->block_null;
    c->fl_bitmap = 0;

    for (int i = 0; i < FL_INDEX_COUNT; ++i) {
        c->sl_bitmap[i] = 0;
        for (int j = 0; j < SL_INDEX_COUNT; ++j) {
            c->blocks[i][j] = &c->block_null;
        }
    }
}

/**
 * @brief Create a TLSF memory pool from a given memory region
 * @param[in] mem_start: start address of the memory region to manage
 * @param[in] len: length of the memory region in bytes
 * @return 0 is success, otherwise is failure
 */
int mtlsf_mem_init(void *mem_start, size_t len)
{
    mem_control_t *c = NULL;
    unsigned char *pool;
    mem_blk_node_t *b;
    mem_blk_node_t *sent;
    size_t ctl_size;
    size_t pool_bytes;

    if (mem_start == NULL || len < MEM_MIN_POOL_SIZE) {
        return -ENOMEM;
    }

    g_mem_base = mem_start;
    c = (mem_control_t *)g_mem_base;
    ctl_size = align_up(sizeof(mem_control_t), MEM_ALIGN_SIZE);
    if (len <= ctl_size + 4 * sizeof(mem_blk_node_t)) {
        return -ENOMEM;
    }

    control_construct(c);

    pool = (unsigned char *)g_mem_base + ctl_size;
    pool_bytes = align_down(len - ctl_size - 2 * BLOCK_OVERHEAD, MEM_ALIGN_SIZE);

    if (pool_bytes < BLOCK_SIZE_MIN || pool_bytes > BLOCK_SIZE_MAX) {
        return -ENOMEM;
    }

    b = (mem_blk_node_t *)(pool - BLOCK_OVERHEAD);
    b->size = 0;
    block_set_size(b, pool_bytes);
    block_set_free(b);
    block_set_prev_used(b);
    block_insert(c, b);

    sent = block_link_next(b);
    sent->size = 0;
    block_set_used(sent);
    block_set_prev_free(sent);

    c->pool_start = pool;
    c->pool_size  = pool_bytes;
    return 0;
}

/**
 * @brief Allocate a block of memory from the TLSF pool
 * @param[in] size: requested size
 * @return void *: pointer to allocated memory, or NULL on failure
 */
void *mtlsf_malloc(size_t size)
{
    mem_control_t *c;
    size_t adj;
    mem_blk_node_t *b;

    c = get_control();
    adj = adjust_request_size(size, MEM_ALIGN_SIZE);
    b = block_locate_free(c, adj);
    return block_prepare_used(c, b, adj);
}

/**
 * @brief Allocate a block of memory from the TLSF pool and zero it
 * @param[in] size: requested size
 * @return void *: pointer to allocated memory, or NULL on failure
 */
void *mtlsf_zalloc(size_t size)
{
    void *ptr = mtlsf_malloc(size);
    if (ptr) {
        memset(ptr, 0, size);
    }
    return ptr;
}

/**
 * @brief Allocate and zero-initialize a block of memory
 * @param[in] nmemb: number of elements
 * @param[in] size: size of each element
 * @return void *: pointer to allocated memory, or NULL on failure
 */
void *mtlsf_calloc(size_t nmemb, size_t size)
{
    size_t total;
    void *p;

    if (nmemb && size > (size_t)-1 / nmemb) {
        return NULL;
    }
    total = nmemb * size;
    p = mtlsf_malloc(total);

    if (p) {
        memset(p, 0, total);
    }
    return p;
}

/**
 * @brief Free a previously allocated block back to the pool
 * @param[in] ptr: pointer to free (NULL is safe)
 */
void mtlsf_free(void *ptr)
{
    mem_control_t *c;
    mem_blk_node_t *b;

    if (!ptr) {
        return;
    }

    c = get_control();
    b = block_from_ptr(ptr);
    SGL_ASSERT(!block_is_free(b) && "double free");
    block_mark_as_free(b);

    b = block_merge_prev(c, b);
    b = block_merge_next(c, b);
    block_insert(c, b);
}

/**
 * @brief Reallocate a block to a new size, preserving content
 * @param[in] ptr: existing pointer (NULL acts like malloc)
 * @param[in] size: new size (0 acts like free)
 * @return void *: pointer to reallocated memory, or NULL on failure
 */
void *mtlsf_realloc(void *ptr, size_t size)
{
    mem_control_t *c;
    mem_blk_node_t *b;
    mem_blk_node_t *n;
    size_t cur;
    size_t combined;
    size_t adj;

    if (!ptr) {
        return mtlsf_malloc(size);
    }
    if (size == 0) {
        mtlsf_free(ptr);
        return NULL;
    }

    c = get_control();
    b = block_from_ptr(ptr);
    n = block_next(b);
    cur = block_size(b);
    combined = cur + block_size(n) + BLOCK_OVERHEAD;

    adj = adjust_request_size(size, MEM_ALIGN_SIZE);
    if (adj == 0) {
        return NULL;
    }

    if (adj > cur && (!block_is_free(n) || adj > combined)) {
        void *p = mtlsf_malloc(size);
        if (p) {
            size_t mn = cur < size ? cur : size;
            memcpy(p, ptr, mn);
            mtlsf_free(ptr);
        }
        return p;
    }

    if (adj > cur) {
        block_merge_next(c, b);
        block_mark_as_used(b);
    }

    block_trim_used(c, b, adj);
    return ptr;
}

/**
 * @brief Allocate memory with a specific alignment requirement
 * @param[in] align: alignment (must be power of 2)
 * @param[in] size: requested size
 * @return void *: aligned pointer to allocated memory, or NULL on failure
 */
void *mtlsf_memalign(size_t align, size_t size)
{
    mem_control_t *c = get_control();
    size_t adj;
    size_t gap_min;
    size_t need;
    mem_blk_node_t *b;
    void *p;
    void *aligned;
    size_t gap;

    if (align & (align - 1)) {
        return NULL;
    }

    adj = adjust_request_size(size, MEM_ALIGN_SIZE);
    if (adj == 0) {
        return NULL;
    }

    gap_min = sizeof(mem_blk_node_t);
    need = align > MEM_ALIGN_SIZE
                  ? adjust_request_size(adj + align + gap_min, align)
                  : adj;

    b = block_locate_free(c, need);
    if (!b) {
        return NULL;
    }

    p = block_to_ptr(b);
    aligned = align_ptr(p, align);
    gap = (size_t)((uintptr_t)aligned - (uintptr_t)p);
    if (gap && gap < gap_min) {
        size_t remain = gap_min - gap;
        size_t off = remain > align ? remain : align;
        aligned = align_ptr((unsigned char *)aligned + off, align);
        gap = (size_t)((uintptr_t)aligned - (uintptr_t)p);
    }

    if (gap) {
        b = block_trim_free_leading(c, b, gap);
    }
    return block_prepare_used(c, b, adj);
}

/**
 * @brief Get the usable size of an allocated block
 * @param[in] ptr: pointer to allocated memory
 * @return size_t: block size, or 0 if ptr is NULL
 */
size_t mtlsf_mem_block_size(void *ptr)
{
    if (!ptr) {
        return 0;
    }
    return block_size(block_from_ptr(ptr));
}

/**
 * @brief Default walker callback that prints block info via SGL log
 * @param[in] ptr: pointer to block data
 * @param[in] size: block size
 * @param[in] used: non-zero if block is used
 * @param[in] user: user context (unused)
 */
static void default_walker(void *ptr, size_t size, int used, void *user)
{
    SGL_UNUSED(user);
    SGL_LOG_TRACE("  %p %s size=%d (block=%p)",
           ptr, used ? "USED" : "FREE", (int)size, (void *)block_from_ptr(ptr));
}

/**
 * @brief Walk all blocks in the pool, calling a callback for each
 * @param[in] walker: callback function (NULL uses default_walker)
 * @param[in] user: user context passed to the callback
 */
void mtlsf_mem_walk(mem_walker walker, void *user)
{
    mem_control_t *c = get_control();
    mem_blk_node_t *b;

    if (!c) {
        return;
    }

    if (!walker) {
        walker = default_walker;
    }

    b = (mem_blk_node_t *)((unsigned char *)c->pool_start - BLOCK_OVERHEAD);
    while (b && !block_is_last(b)) {
        walker(block_to_ptr(b), block_size(b), !block_is_free(b), user);
        b = block_next(b);
    }
}

/**
 * @brief Stats walker: accumulate used/free block counts and sizes
 * @param[in] ptr: pointer to block data
 * @param[in] size: block size
 * @param[in] used: non-zero if used
 * @param[in] user: mem_stats_t pointer
 */
static void stats_walker(void *ptr, size_t size, int used, void *user)
{
    (void)ptr;
    mem_stats_t *s = (mem_stats_t*)user;

    if (used) {
        s->used_bytes += size;
        s->used_blocks++;
    } else {
        s->free_bytes += size;
        s->free_blocks++;
        if (size > s->largest_free) {
            s->largest_free = size;
        }
    }
}

/**
 * @brief Get memory usage statistics for the pool
 * @param[out] out: stats structure to fill
 */
static void mem_get_stats(mem_stats_t *out)
{
    mem_control_t *c;

    if (!out) {
        return;
    }
    memset(out, 0, sizeof(*out));
    c = get_control();
    out->pool_bytes = c->pool_size;
    mtlsf_mem_walk(stats_walker, out);

    out->overhead_bytes = out->used_blocks * BLOCK_OVERHEAD
                        + sizeof(mem_control_t)
                        + 2 * BLOCK_OVERHEAD;
}

/**
 * @brief Dump pool statistics and walk all blocks
 */
void mtlsf_mem_dump(void)
{
    mem_stats_t s;
    mem_get_stats(&s);

    SGL_LOG_TRACE("MEM: pool=%d  used=%d (%d blk)  free=%d (%d blk)  largest_free=%d",
           (int)s.pool_bytes, (int)s.used_bytes, (int)s.used_blocks,
           (int)s.free_bytes, (int)s.free_blocks, (int)s.largest_free);

    mtlsf_mem_walk(NULL, NULL);
}
