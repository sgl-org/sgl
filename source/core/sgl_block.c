/* source/core/sgl_block.c
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
#include <sgl_block.h>
#include <string.h>

#define BLK_LOG_TAG    "blk: "

/**
 * @brief Block device IO control
 * @param dev Block device pointer
 * @param cmd IO control command
 * @param param IO control parameter
 * @return 0 on success, -1 on failure
 */
int sgl_block_dev_ioctl(const sgl_block_dev_t *dev, uint8_t cmd, void *param)
{
    if (!dev || !param) {
        SGL_LOG_ERROR(BLK_LOG_TAG"block device or parameter is NULL");
        return -1;
    }

    if (dev->info) {
        switch (cmd)
        {
        case SGL_BLK_CTRL_SYNC: return 0;
        case SGL_BLK_GET_SECTOR_SIZE:
            if (dev->info->sector_size != 0) {
                *(uint32_t *)param = dev->info->sector_size;
                return 0;
            }
            break;

        case SGL_BLK_GET_BLOCK_SIZE:
            if (dev->info->block_size != 0) {
                *(uint32_t *)param = dev->info->block_size;
                return 0;
            }
            break;

        case SGL_BLK_GET_SECTOR_COUNT:
            if (dev->info->sector_count != 0) {
                *(uint32_t *)param = dev->info->sector_count;
                return 0;
            }
            break;

        default:
            break;
        }
    }

    if (dev->ioctl) {
        return dev->ioctl((sgl_block_dev_t*)dev, cmd, param);
    }
    return -1;
}

/* W25Q16 chip information */
const sgl_block_dev_info_t sgl_w25q16_info = {
    .sector_count = 512,
    .sector_size = 4096,
    .block_size = 1,
    .erase_block_size = 4096,
    .page_size = 256,
};

/* W25Q32 chip information */
const sgl_block_dev_info_t sgl_w25q32_info = {
    .sector_count = 1024,
    .sector_size = 4096,
    .block_size = 1,
    .erase_block_size = 4096,
    .page_size = 256,
};

/* W25Q64 chip information */
const sgl_block_dev_info_t sgl_w25q64_info = {
    .sector_count = 2048,
    .sector_size = 4096,
    .block_size = 1,
    .erase_block_size = 4096,
    .page_size = 256,
};

/* W25Q128 chip information */
const sgl_block_dev_info_t sgl_w25q128_info = {
    .sector_count = 4096,
    .sector_size = 4096,
    .block_size = 1,
    .erase_block_size = 4096,
    .page_size = 256,
};

/* W25Q256 chip information */
const sgl_block_dev_info_t sgl_w25q256_info = {
    .sector_count = 8192,
    .sector_size = 4096,
    .block_size = 1,
    .erase_block_size = 4096,
    .page_size = 256,
};
