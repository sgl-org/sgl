/* source/fs/fatfs/sgl_fatfs.h
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
#ifndef __SGL_FATFS_H__
#define __SGL_FATFS_H__

#include <sgl_fs.h>

#define SGL_FATFS_MAX_LFN      (64)

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief FatFS mount configuration
 * @vol_id:     volume ID (0-9), maps to FatFS logical drive number
 * @opt:        mount option (0 = auto, 1 = force mount)
 */
typedef struct {
    uint8_t vol_id;
    uint8_t opt;
} sgl_fatfs_config_t;

/**
 * @brief Register FatFS file system type with sgl_fs VFS.
 *        Call this once before sgl_fs_mount("fatfs", ...).
 * @return 0 on success, -1 on failure
 */
int sgl_fatfs_register(void);

#ifdef __cplusplus
}
#endif

#endif /* __SGL_FATFS_H__ */
