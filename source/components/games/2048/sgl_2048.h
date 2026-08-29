/* source/components/games/2048/sgl_2048.h
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

#ifndef __SGL_2048_H__
#define __SGL_2048_H__

#include <sgl.h>
#include <string.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SGL_2048_GRID_N          4
#define SGL_2048_ANIM_MS         250                                         /* slide duration */
#define SGL_2048_POP_MS          120                                         /* merge pop duration */
#define SGL_2048_MAX_MOVES       (SGL_2048_GRID_N * SGL_2048_GRID_N)         /* moving tiles per move */
#define SGL_2048_MAX_POPS        (SGL_2048_GRID_N * SGL_2048_GRID_N / 2)     /* merges per move */

/* geometry helpers reading the runtime layout */
#define SGL_2048_GRID_PX         (g_grid_px)
#define SGL_2048_CELL_SZ         (g_cell_sz)
#define SGL_2048_CELL_R          (g_cell_r)
#define SGL_2048_GRID_BG_R       (g_grid_bg_r)
#define SGL_2048_CELL_X(c)       (g_margin_x + (c) * (g_cell_sz + g_cell_gap))
#define SGL_2048_CELL_Y(r)       (g_margin_y + (r) * (g_cell_sz + g_cell_gap))

#define SGL_2048_C_BG            sgl_rgb(250, 248, 240)
#define SGL_2048_C_EMPTY         sgl_rgb(191, 177, 165)
#define SGL_2048_C_2             sgl_color_hex(0xEEE4DA)
#define SGL_2048_C_4             sgl_color_hex(0xEDE0C8)
#define SGL_2048_C_8             sgl_color_hex(0xF2B179)
#define SGL_2048_C_16            sgl_color_hex(0xF59563)
#define SGL_2048_C_32            sgl_color_hex(0xF67C5F)
#define SGL_2048_C_64            sgl_color_hex(0xF65E3B)
#define SGL_2048_C_128           sgl_color_hex(0xEDCF72)
#define SGL_2048_C_256           sgl_color_hex(0xEDCC61)
#define SGL_2048_C_512           sgl_color_hex(0xEDC850)
#define SGL_2048_C_1024          sgl_color_hex(0xEDC53F)
#define SGL_2048_C_2048          sgl_color_hex(0xEDC22E)
#define SGL_2048_C_TXT_D         sgl_color_hex(0x776E65)   /* dark text (2, 4) */
#define SGL_2048_C_TXT_L         SGL_COLOR_WHITE           /* light text (8+) */
#define SGL_2048_C_SBOX          sgl_color_hex(0x8F7A66)   /* score box background */

/**
 * @brief Start the 2048 game: build the whole UI under parent, sized to
 *        width x height. All geometry is derived from the size by
 *        layout_calc(), the grid is centered inside the area.
 * @param parent     parent object (usually the active screen)
 * @param width      game area width
 * @param height     game area height
 * @param title_font font for the "2048" title and the overlay text
 * @param score_font font for the SCORE box
 * @param tile_font  initial tile font (resized per value while playing)
 * @return none
 */
void sgl_game2048_start(sgl_obj_t *parent, int16_t width, int16_t height,
                        const sgl_font_t *title_font,
                        const sgl_font_t *score_font,
                        const sgl_font_t *tile_font);

/**
 * @brief Destroy the game: release any leftover temporary animation widgets.
 * @param parent     parent object (usually the active screen)
 * @return none
 */
void sgl_game2048_destroy(sgl_obj_t *parent);

#ifdef __cplusplus
}
#endif

#endif
