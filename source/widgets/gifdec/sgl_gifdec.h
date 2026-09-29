/* source/widgets/gifdec/sgl_gifdec.h
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
 * 
 * source/widgets/gifdec/sgl_gifdec.c
 *
 * sgl_gifdec — 基于 gifdec(https://github.com/lecram/gifdec) 流式解码的 SGL GIF 播放组件。
 *
 * 播放驱动 (定时器驱动, 直写屏幕显存, 无中间帧缓冲):
 *   sgl_gifdec_timer_cb (sgl_timer) 按帧间隔触发一次刷新
 *   -> sgl_gifdec_construct_cb (SGL_EVENT_DRAW_MAIN)
 *   -> gd_decode_frame_range() 分片流式把帧像素直写 SGL 屏幕 framebuffer
 *      (pitch = surf->w, 起点 = GIF 画布在屏幕的居中偏移)
 *
 * 内存 (与帧数、画面尺寸无关):
 *   arena = gd_estimate_arena(&cfg, RGB565, 0)
 *           大小由 cfg.lzw_max_entries 主导 (LZW 字典 entry[] = 该值*4 字节);
 *           公式与参考值见 sgl_gifdec.c 文件头"内存"段。
 *   帧内容直接落在屏幕显存, 不持有独立 frame_buf / 内部 sgl_img 子控件
 */
/* ------------------------------------------------------------------ */
#ifndef __SGL_GIFDEC_H__
#define __SGL_GIFDEC_H__
/* ------------------------------------------------------------------ */
#include <sgl_core.h>
/* ------------------------------------------------------------------ */
/* loop count constants
 * sgl_gifdec_set_loop_count() 取播放轮数:
 *   0                       = 无限循环 (跟随 GIF 文件 loop 语义)
 *   1                       = 播放一遍后停止
 *   N                       = 播放 N 遍后停止
 */
#define SGL_GIFDEC_REPEAT_INFINITE          (0u)

/* ------------------------------------------------------------------ */
/* GIF 解码资源预算配置 (sgl_gifdec_set_cfg 注入)                        */
/* ------------------------------------------------------------------ */
/* 替代原四个编译期宏 (SGL_GIFDEC_LZW_MAX_ENTRIES / MAX_PALETTE_ENTRIES /
 * STACK_SIZE / ENABLE_LOCAL_PALETTE)。参数值取自
 * optimize_gif_global_palette.py 生成的 C 数组头注释 (LZW 字典 / 调色板宏 /
 * LZW 栈 / 局部调色板)。须在 sgl_gifdec_set_data()/set_file() 之前调用。 */
typedef struct sgl_gifdec_cfg {
    uint16_t lzw_max_entries;       /* LZW 字典最大条目数 (12-bit 上限 4095) */
    uint16_t max_palette_entries;   /* 全局/局部调色板最大条目数 (colors[] 容量) */
    uint16_t stack_size;            /* 【已废弃】原 LZW 字符栈字节数; 字符栈已删除
                                       (像素在链遍历时逆序直写), 本字段不再参与 arena
                                       预算, 保留仅为兼容已有初始化器。 */
    uint8_t  enable_local_palette;  /* 1=支持局部调色板(LCT); 0=裁剪(省一份调色板 RAM) */
} sgl_gifdec_cfg_t;

/* ------------------------------------------------------------------ */
/**
 * @brief gifdec callback function type
 * @param obj top-level sgl_obj_t pointer of the gifdec widget
 */
typedef void (*sgl_gifdec_cb_t)(sgl_obj_t *obj);

/* ------------------------------------------------------------------ */
/* 播放控制                                                            */
/* ------------------------------------------------------------------ */
/**
 * @brief create a gifdec widget (自绘: 解码帧直写屏幕显存, 无子 sgl_img)
 * @param parent parent object (e.g. screen, page)
 * @return pointer to embedded sgl_obj_t, or NULL on OOM
 */
sgl_obj_t *sgl_gifdec_create(sgl_obj_t *parent);

/**
 * @brief 配置 GIF 解码资源预算 (LZW 字典 / 调色板 / 局部调色板; stack_size 已废弃)
 * @param obj gifdec widget
 * @param cfg 配置结构体 (通常取自脚本生成的 C 数组里的 cfg 实例)
 * @note  须在 sgl_gifdec_set_data()/set_file() 之前调用; 若已加载会重新分配。
 */
void sgl_gifdec_set_cfg(sgl_obj_t *obj, const sgl_gifdec_cfg_t *cfg);

/**
 * @brief attach a GIF file's raw bytes held in memory (e.g. a const array
 *        baked into flash).  The bytes are referenced, not copied; they must
 *        outlive the widget.
 * @param obj  gifdec widget
 * @param data GIF file bytes (GIF89a)
 * @param len  byte count
 * @note  On success the widget probes width/height (用于居中定位), 未扫描总帧数;
 *        sgl_gifdec_start() 从 frame 0 开始播放.
 */
void sgl_gifdec_set_data(sgl_obj_t *obj, const void *data, uint32_t len);

/**
 * @brief open a GIF file through sgl_fs and play it.  Requires the file
 *        system to be mounted (sgl_fs_mount).  The file is copied into an
 *        internally-owned buffer, so it can be closed/reused afterwards.
 * @param obj  gifdec widget
 * @param path file path, e.g. "0:/dir/anim.gif"
 * @note  If no file system is mounted this is a no-op; use set_data() instead.
 */
void sgl_gifdec_set_file(sgl_obj_t *obj, const char *path);

/**
 * @brief configure the number of animation loops.
 * @param count SGL_GIFDEC_REPEAT_INFINITE(0) = loop forever;
 *              1 = play once; N = play N times.
 */
void sgl_gifdec_set_loop_count(sgl_obj_t *obj, uint16_t count);

/**
 * @brief set the delay between consecutive frames in milliseconds.
 * @param ms 0 = use the GIF's own per-frame delay (gd_frame_delay_ms);
 *           otherwise this overrides it for all frames.
 */
void sgl_gifdec_set_frame_interval(sgl_obj_t *obj, uint16_t ms);

/**
 * @brief 设置 GIF 透明像素的填充背景色 (画布内的透明区), gif不透明时无效.
 * @param obj   gifdec widget
 * @param color 背景色 (sgl_color_t, 本项目为 RGB565)。
 * @note  仅对"透明帧"生效: 透明像素 (等于该帧 tindex 的像素) 显式填此背景色,
 *        不依赖屏幕 surf buffer 跨帧保留, 固件每帧清黑也能正确显示。
 *        不调用本函数时, 透明区默认填【当前屏幕背景色】(sgl_screen_act() 当前活跃屏
 *        的背景色; 屏幕用 pixmap 背景读不到纯色时退回黑色)。
 *        本设置只作用于 GIF 画布内的透明像素; gifdec 不操作画布外区域
 *        (画布外由 SGL page 背景 + 其他组件自行渲染)。
 *        可在 set_data()/set_file() 之前或之后调用 (未加载时存入, load 时应用)。
 */
void sgl_gifdec_set_bg_color(sgl_obj_t *obj, sgl_color_t color);

/* callbacks */
void sgl_gifdec_set_start_cb(sgl_obj_t *obj, sgl_gifdec_cb_t cb);
void sgl_gifdec_set_ready_cb(sgl_obj_t *obj, sgl_gifdec_cb_t cb);
void sgl_gifdec_set_complete_cb(sgl_obj_t *obj, sgl_gifdec_cb_t cb);

/**
 * @brief (re)start playback from frame 0.
 */
void sgl_gifdec_start(sgl_obj_t *obj);

/** @brief stop playback and freeze on the last drawn frame. */
void sgl_gifdec_stop(sgl_obj_t *obj);

/** @brief pause playback, preserving the current frame. */
void sgl_gifdec_pause(sgl_obj_t *obj);

/** @brief resume playback from the paused frame. */
void sgl_gifdec_resume(sgl_obj_t *obj);

/** @brief seek back to frame 0 WITHOUT starting playback (state -> IDLE). */
void sgl_gifdec_reset(sgl_obj_t *obj);

/* query */
uint8_t  sgl_gifdec_is_playing(sgl_obj_t *obj);
uint8_t  sgl_gifdec_is_finished(sgl_obj_t *obj);
uint16_t sgl_gifdec_get_frame_idx(sgl_obj_t *obj);
uint16_t sgl_gifdec_get_frame_cnt(sgl_obj_t *obj);
uint16_t sgl_gifdec_get_width(sgl_obj_t *obj);
uint16_t sgl_gifdec_get_height(sgl_obj_t *obj);

/**
 * @brief delete the widget and free all internally-allocated resources
 *        (arena buffer / file buffer).  The GIF data passed to set_data() is
 *        caller-owned and not freed.  (帧像素直写屏幕显存, 无独立帧缓冲。)
 */
void sgl_gifdec_destroy(sgl_obj_t *obj);
/* ------------------------------------------------------------------ */
#endif /* !__SGL_GIFDEC_H__ */
/* ------------------------------------------------------------------ */

// void app_gif_play_start(void)
// {
//     if (s_gifdec != NULL) {
//         return;   /* 已在播放 */
//     }

//     /* 独立 screen 显示 GIF, 避免与开机动画/菜单抢屏 */
//     s_gif_screen = sgl_screen_create(s_ui_mgr);
//     if (s_gif_screen == NULL) {
//         LOG_E("app_gif_play: screen create failed");
//         return;
//     }
//     sgl_screen_switch(s_ui_mgr, s_gif_screen);

//     /* sgl_gifdec 组件: 载入内存字节 + 覆盖帧间隔 + 居中 + 播放 */
//     s_gifdec = sgl_gifdec_create(s_gif_screen);
//     if (s_gifdec == NULL) {
//         LOG_E("app_gif_play: sgl_gifdec_create failed");
//         return;
//     }
//     sgl_gifdec_set_cfg(s_gifdec, &gif_resized_cfg);
//     sgl_gifdec_set_data(s_gifdec, gif_resized_bin, gif_resized_len);
//     sgl_gifdec_set_frame_interval(s_gifdec, GIF_FRAME_MS);
//     sgl_gifdec_start(s_gifdec);
// }

