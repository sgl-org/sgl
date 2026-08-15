/* source/widgets/sgl_img.h
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

#ifndef __SGL_IMG_H__
#define __SGL_IMG_H__

#include <sgl_core.h>
#include <sgl_draw.h>
#include <sgl_math.h>
#include <sgl_log.h>
#include <sgl_mm.h>
#include <sgl_cfgfix.h>
#include <string.h>

/**
 * description:
 * 1. Extern Flash image object:
 *      you can use this object to draw image from external flash memory
 *      for example:
 *          void flash_port_read_data_from_flash(const size_t addr, uint8_t *buf, uint32_t len)
 *          {
 *              //read the data to the buf from the addr of flash
 *              Flash_Read(addr, buf, len);
 *          }
 *          sgl_pixmap_t test_pixmap = {
 *              .width = 142,
 *              .height = 69,
 *              .bitmap = addr_of_extern_flash,
 *              .format = SGL_PIXMAP_FMT_RGB565,
 *          };
 *          sgl_obj_t *img = sgl_img_create(NULL);
 *          sgl_obj_set_pos(img, 10, 10);
 *          sgl_obj_set_size(img, 142, 69);
 *          sgl_img_set_pixmap(img, &test_pixmap);
 *          sgl_img_set_read_ops(img, flash_port_read_data_from_flash);
 *  
 * 2. RLE compress image object:
 *      you can use this object to draw image from external flash memory
 *      for example:
 *          extern const uint8_t pixmap_data[19596];
 *          sgl_pixmap_t test_pixmap = {
 *              .width = 142,
 *              .height = 69,
 *              .bitmap = pixmap_data,
 *              .format = SGL_PIXMAP_FMT_RLE_RGB565,
 *          };
 *          sgl_obj_t *img = sgl_img_create(NULL);
 *          sgl_obj_set_pos(img, 10, 10);
 *          sgl_obj_set_size(img, 142, 69);
 *          sgl_img_set_pixmap(img, &test_pixmap);
 * 
 * 3. Mult pixmap image object:
 *      you can use this object to draw image from external flash memory
 *      for example:
 *          void flash_port_read_data_from_flash(const size_t addr, uint8_t *buf, uint32_t len)
 *          {
 *              //read the data to the buf from the addr of flash
 *              Flash_Read(addr, buf, len);
 *          }
 *          sgl_pixmap_t test_pixmap[128] = {
 *              {
 *                  .width = 142,
 *                  .height = 69,
 *                  .bitmap = addr_of_extern_flash_1,
 *                  .format = SGL_PIXMAP_FMT_RGB565,
 *              },
 *              {
 *                  .width = 142,
 *                  .height = 69,
 *                  .bitmap = addr_of_extern_flash_2,
 *                  .format = SGL_PIXMAP_FMT_RGB565,
 *              },
 *              {
 *                  .width = 142,
 *                  .height = 69,
 *                  .bitmap = addr_of_extern_flash_3,
 *                  .format = SGL_PIXMAP_FMT_RGB565,
 *              },
 *              ...
 *          };
 *          sgl_obj_t *img = sgl_img_create(NULL);
 *          sgl_obj_set_pos(img, 10, 10);
 *          sgl_obj_set_size(img, 142, 69);
 *          sgl_img_set_pixmap(img, test_pixmap);
 *          sgl_img_set_pixmap_num(img, 128, true);
 *          sgl_img_set_read_ops(img, flash_port_read_data_from_flash);
 */

 /* TODO: add ext img buffer size config */
#define SGL_IMG_BUFFER_SIZE   (CONFIG_SGL_IMG_BUFFER_LINES)

/**
 * @brief sgl img struct
 * @obj: sgl general object
 * @desc: pointer to img draw descriptor
 */
typedef struct sgl_img {
    sgl_obj_t       obj;
    const sgl_pixmap_t *pixmap;
    void            (*read)(const size_t addr, uint8_t *buf, uint32_t len_bytes);
    /* RLE compress context */
    uint32_t        index;
    sgl_color_t     color;
    uint8_t         alpha;
    uint8_t         pixmap_auto;
    uint8_t         pixmap_idx;
    uint8_t         pixmap_num;
    uint8_t         remainder;
    uint8_t         pix_alpha;
    uint8_t         *flash_buffer;
    uint16_t        buffer_lines;   /* Number of lines the flash_buffer can hold */
    uint16_t        buffer_line;    /* Current starting line in flash_buffer */
    uint32_t        *qoi_line_offsets; /* QOI per-line offset table (allocated on demand) */
} sgl_img_t;

/**
 * @brief create an img object
 * @param parent parent of the img
 * @return img object
 */
sgl_obj_t* sgl_img_create(sgl_obj_t* parent);

/**
 * @brief set img pixmap
 * @param obj img object
 * @param pixmap img pixmap
 * @return none
 */
void sgl_img_set_pixmap(sgl_obj_t *obj, const sgl_pixmap_t *pixmap);

/**
 * @brief set img read operation
 * @param obj img object
 * @param read img read operation
 * @return none
 */
void sgl_img_set_read_ops(sgl_obj_t *obj, void (*read)(const size_t addr, uint8_t *out, uint32_t len_bytes));

/**
 * @brief set img alpha
 * @param obj img object
 * @param alpha img alpha
 * @return none
 */
void sgl_img_set_alpha(sgl_obj_t *obj, uint8_t alpha);

/**
 * @brief set img pixmap number
 * @param obj img object
 * @param num img pixmap number
 * @param auto_refresh img pixmap auto refresh
 * @return none
 * @note if auto_refresh is true, the img will refresh automatically after pixmap flush conplete
 * @warning the num max is 255
 */
void sgl_img_set_pixmap_num(sgl_obj_t *obj, uint8_t num, bool auto_refresh);

/**
 * @brief set img next pixmap
 * @param obj img object
 * @return none
 */
void sgl_img_set_pixmap_next(sgl_obj_t *obj);

/**
 * @brief set img pixmap current index
 * @param obj img object
 * @param index img pixmap index
 * @return none
 */
void sgl_img_set_pixmap_index(sgl_obj_t *obj, uint8_t index);

#endif // !__SGL_IMG_H__
