/* source/mm/umm_malloc/sgl_mm.c
 *
 * MIT License
 *
 * Copyright(c) 2023-present All contributors of SGL
 * Document reference link: https://sgl-docs.readthedocs.io
 */

#include "umm_malloc.h"
#include <stdint.h>
#include <sgl_mm.h>
#include <sgl_log.h>
#include <sgl_cfgfix.h>

/*
 * Forward declaration: umm_multi_free_heap_size is declared in umm_malloc_cfg.h
 * (only when UMM_INFO is defined). We declare it here to avoid pulling in the
 * full internal config header.
 */
extern size_t umm_multi_free_heap_size(struct umm_heap_config *heap);

/*
 * Provide dummy definitions for linker symbols referenced by umm_multi_init()
 * in umm_malloc.c. We never call umm_multi_init() directly -- we use
 * umm_multi_init_heap() instead -- but the object file still references them.
 */
void *UMM_MALLOC_CFG_HEAP_ADDR;
uint32_t UMM_MALLOC_CFG_HEAP_SIZE;

/* Use a different name to avoid conflict with the typedef 'umm_heap' */
static struct umm_heap_config g_umm_heap_inst;
static size_t g_total_pool_size = 0;

void sgl_mm_init(void *mem_start, size_t len)
{
    umm_multi_init_heap(&g_umm_heap_inst, mem_start, len);
    g_total_pool_size = len;
}

void sgl_mm_add_pool(void *mem_start, size_t len)
{
    /*
     * umm_malloc does not support adding pools after init.
     * Track the additional size for monitor accuracy.
     */
    SGL_UNUSED(mem_start);
    g_total_pool_size += len;
    SGL_LOG_WARN("umm_malloc: add_pool not supported, ignoring");
}

void* sgl_malloc(size_t size)
{
    void *ret = umm_multi_malloc(&g_umm_heap_inst, size);
    if (!ret) {
        SGL_LOG_ERROR("out of memory");
    }
    return ret;
}

void* sgl_realloc(void *p, size_t size)
{
    void *ret = umm_multi_realloc(&g_umm_heap_inst, p, size);
    if (!ret) {
        SGL_LOG_ERROR("out of memory");
    }
    return ret;
}

void sgl_free(void *p)
{
    if (p) {
        umm_multi_free(&g_umm_heap_inst, p);
    }
}

sgl_mm_monitor_t sgl_mm_get_monitor(void)
{
    sgl_mm_monitor_t mon = {0};

    if (g_total_pool_size == 0) {
        return mon;
    }

    size_t free_bytes = umm_multi_free_heap_size(&g_umm_heap_inst);
    mon.total_size = g_total_pool_size;
    mon.free_size  = free_bytes;
    mon.used_size  = g_total_pool_size - free_bytes;

    uint32_t total_rate = (uint32_t)mon.used_size * 10000U / (uint32_t)g_total_pool_size;
    int int_part = (int)(total_rate / 100);
    int dec_part = (int)(total_rate % 100);
    mon.used_rate = (size_t)((int_part << 8) | (dec_part & 0xFF));

    return mon;
}
