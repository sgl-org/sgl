/* source/include/sgl_block.h
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
#ifndef __SGL_BLOCK_H__
#define __SGL_BLOCK_H__

#include <sgl_core.h>

/* Block device ioctl commands */
#define SGL_BLK_CTRL_SYNC           (0)        /* Flush pending writes */
#define SGL_BLK_GET_SECTOR_COUNT    (1)        /* Get total sector count */
#define SGL_BLK_GET_SECTOR_SIZE     (2)        /* Get sector size in bytes */
#define SGL_BLK_GET_BLOCK_SIZE      (3)        /* Get erase block size (sectors) */
#define SGL_BLK_CTRL_TRIM           (4)        /* Trim unused sectors */

/* Block device ioctl result codes */
#define SGL_BLK_RES_OK              (0)        /* success */
#define SGL_BLK_RES_ERROR           (1)        /* generic error */
#define SGL_BLK_RES_WRPRT           (2)        /* write protected */
#define SGL_BLK_RES_NOTRDY          (3)        /* not ready */
#define SGL_BLK_RES_PARERR          (4)        /* parity error */

typedef struct sgl_block_dev_info {
    uint32_t sector_count;      /* total logical sectors */
    uint32_t sector_size;       /* bytes per sector */
    uint32_t block_size;        /* erase block size in sectors */
    uint32_t erase_block_size;  /* erase block size in bytes */
    uint32_t page_size;         /* programming/page size in bytes */
} sgl_block_dev_info_t;

/**
 * @brief File system device
 * @init: init disk device
 * @read_sectors: read sectors from disk devices
 * @write_sector: write sector to disk devices
 * @ioctl: ioctl command, it is optional
 * @status: status command, it is optional
 */
typedef struct sgl_block_dev {
    int (*init)(struct sgl_block_dev *dev);
    int (*read_sectors)(struct sgl_block_dev *dev, uint32_t sector, uint8_t *buf, uint32_t count);
    int (*write_sectors)(struct sgl_block_dev *dev, uint32_t sector, const uint8_t *buf, uint32_t count);
    int (*ioctl)(struct sgl_block_dev *dev, uint8_t cmd, void *param);
    int (*status)(struct sgl_block_dev *dev);
    const sgl_block_dev_info_t *info;
} sgl_block_dev_t;

/**
 * @brief Block device IO control
 * @param dev Block device pointer
 * @param cmd IO control command
 * @param param IO control parameter
 * @return 0 on success, -1 on failure
 */
int sgl_block_dev_ioctl(const sgl_block_dev_t *dev, uint8_t cmd, void *param);

/* W25Q16 chip information */
extern const sgl_block_dev_info_t sgl_w25q16_info;
/* W25Q32 chip information */
extern const sgl_block_dev_info_t sgl_w25q32_info;
/* W25Q64 chip information */
extern const sgl_block_dev_info_t sgl_w25q64_info;
/* W25Q128 chip information */
extern const sgl_block_dev_info_t sgl_w25q128_info;
/* W25Q256 chip information */
extern const sgl_block_dev_info_t sgl_w25q256_info;
/* SD card chip information */
extern const sgl_block_dev_info_t sdcard_info;

#endif // __SGL_BLOCK_H__
