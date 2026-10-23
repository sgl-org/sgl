/* source/fs/kvfs/sgl_kvfs.h
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
#ifndef __SGL_KVFS_H__
#define __SGL_KVFS_H__

#include <sgl_fs.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Key length limit (excluding the terminating '\0'), i.e. the maximum
 *        file-name length under a kvfs mount point.
 */
#define SGL_KVFS_MAX_KEY_LEN    31

/**
 * @brief Value length limit, i.e. the maximum size of one kvfs file
 */
#define SGL_KVFS_MAX_VAL_LEN    65535   /* on-disk record uses 16-bit length */

/**
 * @brief KVFS mount configuration
 * @start_sector: first sector owned by this kvfs volume on the device.
 *                The superblock occupies it; the record log starts at the
 *                next sector. 0 when cfg == NULL.
 * @sector_count: total sectors owned (including the superblock sector).
 *                0 means "from start_sector to the end of the device".
 */
typedef struct {
    uint32_t start_sector;
    uint32_t sector_count;
} sgl_kvfs_config_t;

/**
 * @brief Register KVFS file system type with sgl_fs VFS.
 *        This is the only entry point: after calling it once, access the
 *        volume through the generic sgl_fs_xxx API, e.g.
 *          sgl_fs_mount("/kv", "kvfs", dev, &cfg);
 *          sgl_fs_open("/kv/<key>", ...);   // one key == one flat file
 *        A volume with a missing/invalid superblock is formatted
 *        automatically on mount.
 * @return 0 on success, -1 on failure
 */
int sgl_kvfs_register(void);

#ifdef __cplusplus
}
#endif

#endif /* __SGL_KVFS_H__ */
