/*
 * umm_malloc_cfgport.h - SGL-specific configuration for umm_malloc
 *
 * MIT License
 *
 * Copyright(c) 2023-present All contributors of SGL
 */

#ifndef UMM_MALLOC_CFGPORT_H
#define UMM_MALLOC_CFGPORT_H

/*
 * Block body size: 8 bytes (default)
 * Each umm_block = header(4 bytes) + body(8 bytes) = 12 bytes
 */
#ifndef UMM_BLOCK_BODY_SIZE
#define UMM_BLOCK_BODY_SIZE                              (8)
#endif

/*
 * Single heap for SGL
 */
#ifndef UMM_NUM_HEAPS
#define UMM_NUM_HEAPS                                    (1)
#endif

/*
 * Allocation algorithm
 *
 * UMM_BEST_FIT  (default)
 *   Searches the entire free list and picks the block that best matches the
 *   requested size. Produces less fragmentation, ideal when memory is tight.
 *
 * UMM_FIRST_FIT
 *   Picks the first free block that fits. Faster allocation at the cost of
 *   potentially higher fragmentation.
 *
 * To switch to FIRST_FIT, uncomment the line below.
 *   DO NOT define both -- the build will error out.
 */
/* #define UMM_FIRST_FIT */

/*
 * Enable UMM_INFO for metrics support (sgl_mm_get_monitor)
 */
#define UMM_INFO

#endif /* UMM_MALLOC_CFGPORT_H */
