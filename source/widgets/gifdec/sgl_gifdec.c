/* source/widgets/gifdec/sgl_gifdec.c
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
 *       -> sgl_gifdec_construct_cb (SGL_EVENT_DRAW_MAIN)
 *       -> gd_decode_frame_range() 分片把帧像素直写 SGL 屏幕 framebuffer
 *          (pitch = surf->w, 起点 = GIF 画布在屏幕的居中偏移)
 *       -> 帧间隔由定时器保证, 不再自轮询 set_dirty
 *
 * 内存 (与帧数、画面尺寸无关):
 *   arena = gd_estimate_arena(&cfg, SGL_GIFDEC_OUT_RGB565, 0)
 *           大小几乎完全由 cfg.lzw_max_entries 决定 —— LZW 字典 entry[] = 该值*4 字节,
 *           占 arena 绝大部分。粗略公式 (以 sizeof 实测为准):
 *             arena ≈ lzw_max_entries*4 + max_palette_entries*3*(1+enable_local_palette) + ~256B
 *           参考:
 *             create() 默认 cfg{lzw=1396, pal=32,  lct=0} => 约 6.0 KB
 *             脚本产物 cfg{lzw=808,  pal=128, lct=0}      => 约 3.9 KB
 *           注意 arena 直接吃 SGL 堆 (约 8KB), 改动 cfg 前务必先算 gd_estimate_arena, 否则极易 OOM。
 *   帧内容直接落在屏幕显存, 不再持有独立 frame_buf / 内部 sgl_img 控件。
 */

#include <sgl_core.h>
#include <sgl_log.h>
#include <sgl_mm.h>
#include <sgl_fs.h>
#include <sgl_draw.h>
#include <string.h>
#include "sgl_gifdec.h"
#include "../../components/timer/sgl_timer.h"
/* ------------------------------------------------------------------ */
/* 错误码                                                              */
/* ------------------------------------------------------------------ */
#define SGL_GIFDEC_OK              (0)
#define SGL_GIFDEC_ERR_IO          (-1)   /* 底层读写失败 / premature EOF */
#define SGL_GIFDEC_ERR_FORMAT      (-2)   /* 非 GIF / 版本不支持 / 码流非法 */
#define SGL_GIFDEC_ERR_NOMEM       (-3)   /* arena 不足 */
#define SGL_GIFDEC_ERR_TOO_LARGE   (-4)   /* 图像尺寸超过限制 */

/* ------------------------------------------------------------------ */
/* seek 语义 (取值与 SGL_SEEK_SET/CUR/END 保持一致)                     */
/* ------------------------------------------------------------------ */
#define SGL_GIFDEC_SEEK_SET        (0)
#define SGL_GIFDEC_SEEK_CUR        (1)
#define SGL_GIFDEC_SEEK_END        (2)

/* 字典项打包成一个 uint32:
 *   [11:0]  prefix   (SGL_GIFDEC_LZW_ROOT = 根项, 即单字符)
 *   [23:12] length   (该码对应的字符串长度)
 *   [31:24] suffix   (串尾字符; 根项即该字符本身)
 *
 * 之所以不用 {u16 length; u16 prefix; u8 suffix;} 结构体: 后者对齐后 6 字节,
 * 4096 项要 24KB; 打包后只要 16KB, 且 32-bit 取指比 3 个成员的多次访存更快。 */
#define SGL_GIFDEC_LZW_ROOT          (0xFFFu)
#define SGL_GIFDEC_LZW_PREFIX(v)     ((uint16_t)((v) & 0xFFFu))
#define SGL_GIFDEC_LZW_LENGTH(v)     ((uint16_t)(((v) >> 12) & 0xFFFu))
#define SGL_GIFDEC_LZW_SUFFIX(v)     ((uint8_t)((v) >> 24))
#define SGL_GIFDEC_LZW_PACK(p, l, s) (((uint32_t)((p) & 0xFFFu))         \
                            | (((uint32_t)((l) & 0xFFFu)) << 12) \
                            | ((uint32_t)(s) << 24))
/* ======================================================================
 * gifdec 解码器实现
 * ====================================================================== */

#ifndef SGL_GIFDEC_SUB_BLOCK_GUARD
#define SGL_GIFDEC_SUB_BLOCK_GUARD   (1u << 20)
#endif

#define SGL_GIFDEC_MIN(a, b)         (((a) < (b)) ? (a) : (b))

/* IO 预读缓冲大小: 早期为摊薄 get_key() 逐字节读取的开销而设。
 * 现已由 gd_io_t.mem 内存直读快路径取代 (见 gd_io_t / gif_getc): set_data/set_file
 * 都把整份 GIF 放在内存/flash, 直读模式下每字节退化为一条 mem[mem_pos++],
 * 既不走函数指针也不 memcpy, 比预读缓冲更快且零额外 RAM。
 * 因此默认 0 (关闭预读); 仅当改用真正的慢速流式 IO (mem==NULL 的回调) 时才考虑 >0。
 * 注: 内存直读仅在 IO_BUFSZ == 0 时生效。 */
#ifndef SGL_GIFDEC_IO_BUFSZ
#define SGL_GIFDEC_IO_BUFSZ         (0u)
#endif

/* LZW 首字符表: 开启后每个码字的 head 由 first[key] 在 O(1) 得到, 且链遍历可在
 * 走出可见窗口时提前终止 (O(窗口) 而非 O(str_len))。
 * 代价: 额外 cfg.lzw_max_entries 字节 arena (如 808 项 => +0.8 KB)。
 * SGL 堆只有 8KB、arena 已占其大半, 故默认关闭; 换用更大堆时可置 1。 */
#ifndef SGL_GIFDEC_LZW_FIRST_TABLE
#define SGL_GIFDEC_LZW_FIRST_TABLE  (0)
#endif

/* ------------------------------------------------------------------ */
/* 输出像素格式 (枚举值 == 每像素字节数)                                   */
/* ------------------------------------------------------------------ */
typedef enum gd_out_fmt {
    SGL_GIFDEC_OUT_RGB565 = 2,   /* 2 bytes/pixel, RGB565 (little-endian)   */
    SGL_GIFDEC_OUT_BGR888 = 3,   /* 3 bytes/pixel, byte order B,G,R         */
} gd_out_fmt_t;

/* ------------------------------------------------------------------ */
/* 播放状态                                                            */
/* ------------------------------------------------------------------ */
typedef enum sgl_gifdec_state {
    SGL_GIFDEC_STATE_IDLE = 0,
    SGL_GIFDEC_STATE_PLAYING,
    SGL_GIFDEC_STATE_PAUSED,
    SGL_GIFDEC_STATE_FINISHED,
} sgl_gifdec_state_t;

/* ------------------------------------------------------------------ */
/* IO 抽象: 由调用方实现并注入                                            */
/* ------------------------------------------------------------------ */
typedef struct gd_io {
    /* 读取 len 字节; 返回实际读到的字节数, <=0 表示失败/EOF */
    int32_t (*read)(void *ctx, void *buf, uint32_t len);
    /* 重定位; 返回新的绝对偏移, <0 表示失败 */
    int32_t (*seek)(void *ctx, int32_t offset, int whence);
    /* 回调私有数据 (如 fd、flash 设备句柄、内存游标...) */
    void *ctx;
    /* ===== 可选: 内存直读快路径 =====
     * mem != NULL 时, gif_read/gif_getc/gif_seek/gif_tell 直接操作 mem[mem_pos],
     * 完全跳过 read/seek 回调 —— 消除 LZW 逐字节读取的"函数指针 + while 循环 +
     * memcpy(1)"三重开销 (数据在 flash 时更是一条 load 指令, 无需拷贝)。
     * mem 必须与 read/seek 回调指向同一份字节 (open 时会用回调 seek 同步一次游标),
     * 之后解码全程只走 mem_pos, 不再触碰回调。mem_pos 存在 gd_gif_t 内。
     * 注: 仅在 SGL_GIFDEC_IO_BUFSZ == 0 (默认) 时生效; 内存直读本就无需再预读缓冲。 */
    const uint8_t *mem;
    uint32_t       mem_len;
} gd_io_t;

/* ------------------------------------------------------------------ */
/* arena: 调用方提供的内存池, 只分配不回收                                 */
/* ------------------------------------------------------------------ */
typedef struct gd_arena {
    uint8_t *buf;
    uint32_t size;
    uint32_t used;
} gd_arena_t;
             
typedef struct gd_lzw_table {
    uint16_t nentries;      /* 实际可用条目数 (<= max_entries) */
    uint16_t max_entries;   /* entry[] 容量 (由 cfg.lzw_max_entries 决定) */
    uint16_t virt;          /* 编码端条目计数 (可到 4096); 字典裁剪满后仍推进,
                               仅用于让 key_size 与编码端同步增长, 防止码长失步 */
    uint16_t root_size;     /* 已初始化的根项数 (0 = 尚未初始化) */
    uint8_t  full;          /* 1 = entry[] 已写满, 此后只推进 virt */
    uint32_t *entry;        /* arena 分配, 容量 max_entries 项 */
#if SGL_GIFDEC_LZW_FIRST_TABLE
    uint8_t  *first;        /* arena 分配, 容量 max_entries 字节; 每项串的首字符 */
#endif
} gd_lzw_table_t;
/* ------------------------------------------------------------------ */
/* 调色板 / GCE / 矩形 / GIF 句柄                                       */
/* ------------------------------------------------------------------ */
typedef struct gd_palette {
    int size;
    uint16_t max_entries;      /* colors[] 容量 (由 cfg.max_palette_entries 决定) */
    uint8_t *colors;           /* arena 分配, 容量 max_entries*3 字节;
                                  读入 RGB888 后原地打包为输出格式 LUT */
    uint16_t *lut;             /* 输出格式查表 (== (uint16_t *)colors);
                                  仅 RGB565 时有效, 否则为 NULL */
} gd_palette_t;

typedef struct gd_gce {
    uint16_t delay;        /* 单位 10ms */
    uint8_t tindex;        /* 透明色索引 */
    uint8_t disposal;      /* 0/1=保留, 2=恢复背景, 3=恢复上一帧 */
    uint8_t transparency;  /* 1 = 使用 tindex */
} gd_gce_t;

typedef struct gd_gif {
    gd_io_t     io;          /* 按值保存, 调用方的 gd_io_t 可以是栈上临时变量 */
    uint32_t    mem_pos;     /* 内存直读游标 (io.mem != NULL 时有效, 否则忽略) */
    gd_arena_t *arena;

    uint32_t    anim_start;  /* 第一个图像块的文件偏移, gd_rewind() 用 */

    uint16_t    width, height;
    uint16_t    loop_count;  /* NETSCAPE 扩展; 0 = 无限 */

    gd_gce_t    gce;

    gd_palette_t *palette;   /* 指向 gct (启用 LCT 时可为 lct) */
    gd_palette_t  lct;
    gd_palette_t  gct;

    uint16_t    fx, fy, fw, fh;      /* 当前帧矩形 (cur) */
    uint16_t    prv_fx, prv_fy, prv_fw, prv_fh;  /* 上一帧矩形 (prv), disposal 用;
                                        read_image_range_header() 在读新帧前把旧 cur 存到这里 */
    uint8_t     bgindex;

    /* disposal=3 恢复缓冲: 惰性从 arena 尾部按需分配, 见文件头说明第 8 点 */
    uint8_t    *restore;
    uint32_t    restore_size;        /* 字节 */
    uint32_t    restore_mark;        /* arena 水位; 重新分配时回退到这里 */
    uint8_t     restore_valid;
    uint8_t     prev_disposal;       /* 上一帧的 disposal, 下一帧开始时执行 */
    uint8_t     first_frame;

    gd_lzw_table_t *lzw;

    gd_out_fmt_t out_fmt;
    uint8_t      out_bpp;    /* 2 或 3 */
    uint8_t      bg_fill[3]; /* 透明像素填充色 (RGB888) */
    uint16_t     bg_value;   /* bg_fill 预打包成输出格式的值 (RGB565 时有效) */
    uint8_t      bg_packed;  /* 1 = bg_value 可用 (out_fmt == RGB565) */
    sgl_gifdec_cfg_t cfg;    /* 解码资源预算 (arena 分配表容量) */

#if SGL_GIFDEC_IO_BUFSZ
    /* ===== IO 预读缓冲 (SGL_GIFDEC_IO_BUFSZ == 0 时编译掉) ===== */
    uint8_t      rbuf[SGL_GIFDEC_IO_BUFSZ];
    uint16_t     rbuf_len;   /* 缓冲内有效字节数 */
    uint16_t     rbuf_pos;   /* 已消费字节数 */
#endif

    /* ===== paused streaming-decode state (gd_decode_frame_range) ===== */
    uint8_t      r_in_image;       /* 1=帧像素解码中, 0=完成/等待下一帧 */
    uint8_t      r_interlace;
    uint8_t      r_in_string;      /* 1=LZW 串跨分片暂停, 下一分片恢复 */
    uint8_t      r_prev_head;
    uint8_t      r_sub_len, r_shift, r_byte;
    uint8_t      r_table_is_full;
    uint16_t     r_key, r_clear, r_stop, r_string_key;
    uint16_t     r_key_size, r_init_key_size;
    int32_t      r_frm_off;
    int32_t      r_str_len;    /* 上一个已展开串的长度: 跨切片恢复时作为 lzw_add 的
                                  length 实参 (新条目长度 = 上一串长度 + 1), 必须保存 */
    int32_t      r_str_total;
    int32_t      r_str_start;
    int32_t      r_start, r_end;
    uint8_t      r_apply_disposal;
    uint8_t      r_apply_restore_valid;
    uint8_t      r_gif_end;        /* 1 = reached GIF terminator (cycle done) */
} gd_gif_t;

/* ------------------------------------------------------------------ */
/* arena                                                              */
/* ------------------------------------------------------------------ */
static void gd_arena_init(gd_arena_t *arena, void *buf, uint32_t size)
{
    if (arena == NULL) {
        return;
    }
    arena->buf  = (uint8_t *)buf;
    arena->size = (buf != NULL) ? size : 0u;
    arena->used = 0u;
}

static void *arena_alloc(gd_arena_t *arena, uint32_t size)
{
    uint32_t base;

    if (arena == NULL || arena->buf == NULL || size == 0u) {
        return NULL;
    }

    base = (arena->used + 3u) & ~((uint32_t)3u);
    if (base > arena->size) {
        return NULL;
    }
    if (size > (arena->size - base)) {
        return NULL;
    }

    arena->used = base + size;
    return (void *)(arena->buf + base);
}

static uint32_t gd_estimate_arena(const sgl_gifdec_cfg_t *cfg, gd_out_fmt_t fmt, uint32_t restore_px)
{
    uint32_t bpp = (fmt == SGL_GIFDEC_OUT_RGB565) ? 2u : 3u;
    uint32_t pal_copy = cfg->enable_local_palette ? 2u : 1u;   /* gct + (lct) */

    /* 注: cfg->stack_size 已废弃 (LZW 字符栈已删除, 像素在链遍历时逆序直写),
     *     不再计入预算; 保留字段仅为兼容已有 sgl_gifdec_cfg_t 初始化器。 */
    return (uint32_t)sizeof(gd_gif_t)
         + (uint32_t)sizeof(gd_lzw_table_t)
         + (uint32_t)cfg->lzw_max_entries * 4u                 /* entry[] */
#if SGL_GIFDEC_LZW_FIRST_TABLE
         + (uint32_t)cfg->lzw_max_entries                      /* first[] */
#endif
         + (uint32_t)cfg->max_palette_entries * 3u * pal_copy  /* colors[] */
         + (restore_px * bpp)
         + 64u;
}

/* ------------------------------------------------------------------ */
/* IO 封装: 全部校验返回值                                             */
/* ------------------------------------------------------------------ */
static int io_read(const gd_io_t *io, void *buf, uint32_t len)
{
    uint8_t *p = (uint8_t *)buf;
    uint32_t got = 0u;

    if (io == NULL || io->read == NULL) {
        return SGL_GIFDEC_ERR_IO;
    }

    while (got < len) {
        int32_t n = io->read(io->ctx, p + got, len - got);
        if (n <= 0) {
            return SGL_GIFDEC_ERR_IO;
        }
        got += (uint32_t)n;
    }
    return SGL_GIFDEC_OK;
}

static int32_t io_seek(const gd_io_t *io, int32_t offset, int whence)
{
    if (io == NULL || io->seek == NULL) {
        return SGL_GIFDEC_ERR_IO;
    }
    return io->seek(io->ctx, offset, whence);
}

#if SGL_GIFDEC_IO_BUFSZ
/* 预读: get_key() 逐字节读取是 LZW 解码最外层热点, 缓冲化后不再每次走函数指针 + memcpy */
static int gif_fill_buf(gd_gif_t *gif)
{
    int32_t n;

    if (gif->rbuf_pos < gif->rbuf_len) {
        return SGL_GIFDEC_OK;
    }
    n = gif->io.read(gif->io.ctx, gif->rbuf, (uint32_t)sizeof(gif->rbuf));
    if (n <= 0) {
        gif->rbuf_len = 0u;
        gif->rbuf_pos = 0u;
        return SGL_GIFDEC_ERR_IO;
    }
    gif->rbuf_pos = 0u;
    gif->rbuf_len = (uint16_t)n;
    return SGL_GIFDEC_OK;
}

static int gif_read(gd_gif_t *gif, void *buf, uint32_t len)
{
    uint8_t *p = (uint8_t *)buf;

    while (len > 0u) {
        uint32_t avail = (uint32_t)(gif->rbuf_len - gif->rbuf_pos);
        uint32_t n;

        if (avail == 0u) {
            if (gif_fill_buf(gif) != SGL_GIFDEC_OK) {
                return SGL_GIFDEC_ERR_IO;
            }
            avail = (uint32_t)(gif->rbuf_len - gif->rbuf_pos);
            if (avail == 0u) {
                return SGL_GIFDEC_ERR_IO;
            }
        }
        n = (len < avail) ? len : avail;
        memcpy(p, gif->rbuf + gif->rbuf_pos, n);
        gif->rbuf_pos += (uint16_t)n;
        p  += n;
        len -= n;
    }
    return SGL_GIFDEC_OK;
}

/* 缓冲内的字节在"逻辑读位置"之后, seek/tell 必须折算掉 */
static int gif_seek(gd_gif_t *gif, int32_t offset, int whence)
{
    int32_t pend = (int32_t)(gif->rbuf_len - gif->rbuf_pos);

    if (whence == SGL_GIFDEC_SEEK_CUR) {
        if (offset >= 0 && offset <= pend) {          /* 缓冲内前进 */
            gif->rbuf_pos += (uint16_t)offset;
            return SGL_GIFDEC_OK;
        }
        if (offset < 0 && (-offset) <= (int32_t)gif->rbuf_pos) {  /* 缓冲内回退 */
            gif->rbuf_pos -= (uint16_t)(-offset);
            return SGL_GIFDEC_OK;
        }
        offset -= pend;
    }
    gif->rbuf_len = 0u;
    gif->rbuf_pos = 0u;
    return (io_seek(&gif->io, offset, whence) < 0) ? SGL_GIFDEC_ERR_IO : SGL_GIFDEC_OK;
}

static int32_t gif_tell(gd_gif_t *gif)
{
    int32_t pos = io_seek(&gif->io, 0, SGL_GIFDEC_SEEK_CUR);

    if (pos < 0) {
        return pos;
    }
    return pos - (int32_t)(gif->rbuf_len - gif->rbuf_pos);
}
#else
static int gif_read(gd_gif_t *gif, void *buf, uint32_t len)
{
    if (gif->io.mem != NULL) {
        /* 内存直读: 越界即视为 premature EOF, 语义与 io_read 一致 */
        uint32_t avail = (gif->mem_pos < gif->io.mem_len)
                         ? (gif->io.mem_len - gif->mem_pos) : 0u;
        if (len > avail) {
            return SGL_GIFDEC_ERR_IO;
        }
        memcpy(buf, gif->io.mem + gif->mem_pos, len);
        gif->mem_pos += len;
        return SGL_GIFDEC_OK;
    }
    return io_read(&gif->io, buf, len);
}

static int gif_seek(gd_gif_t *gif, int32_t offset, int whence)
{
    if (gif->io.mem != NULL) {
        /* 与 sgl_gifdec_io_seek 相同的 clamp 语义 (越界钳到 [0, mem_len]) */
        int32_t base, np;
        switch (whence) {
        case SGL_GIFDEC_SEEK_SET: base = 0; break;
        case SGL_GIFDEC_SEEK_CUR: base = (int32_t)gif->mem_pos; break;
        default:                  base = (int32_t)gif->io.mem_len; break;
        }
        np = base + offset;
        if (np < 0) {
            np = 0;
        } else if ((uint32_t)np > gif->io.mem_len) {
            np = (int32_t)gif->io.mem_len;
        }
        gif->mem_pos = (uint32_t)np;
        return SGL_GIFDEC_OK;
    }
    return (io_seek(&gif->io, offset, whence) < 0) ? SGL_GIFDEC_ERR_IO : SGL_GIFDEC_OK;
}

static int32_t gif_tell(gd_gif_t *gif)
{
    if (gif->io.mem != NULL) {
        return (int32_t)gif->mem_pos;
    }
    return io_seek(&gif->io, 0, SGL_GIFDEC_SEEK_CUR);
}
#endif /* SGL_GIFDEC_IO_BUFSZ */

/* 单字节读取快路径: get_key() 逐字节取码是 LZW 解码最内层热点。
 * 内存直读模式 (且未启用预读缓冲) 下退化为一条 mem[mem_pos++], 无函数指针、
 * 无 memcpy、无 while 循环; 否则回退到 gif_read(1)。 */
static int gif_getc(gd_gif_t *gif, uint8_t *out)
{
#if !SGL_GIFDEC_IO_BUFSZ
    if (gif->io.mem != NULL) {
        if (gif->mem_pos >= gif->io.mem_len) {
            return SGL_GIFDEC_ERR_IO;
        }
        *out = gif->io.mem[gif->mem_pos++];
        return SGL_GIFDEC_OK;
    }
#endif
    return gif_read(gif, out, 1u);
}

static int gif_read_u16(gd_gif_t *gif, uint16_t *out)
{
    uint8_t b[2];

    if (gif_read(gif, b, 2) != SGL_GIFDEC_OK) {
        return SGL_GIFDEC_ERR_IO;
    }
    *out = (uint16_t)(b[0] | ((uint16_t)b[1] << 8));
    return SGL_GIFDEC_OK;
}

/* ------------------------------------------------------------------ */
/* 扩展块 / 子块                                                       */
/* ------------------------------------------------------------------ */
static int discard_sub_blocks(gd_gif_t *gif)
{
    uint32_t total = 0u;
    uint8_t size;

    do {
        if (gif_read(gif, &size, 1) != SGL_GIFDEC_OK) {
            return SGL_GIFDEC_ERR_IO;
        }
        if (size != 0u) {
            if (gif_seek(gif, (int32_t)size, SGL_GIFDEC_SEEK_CUR) != SGL_GIFDEC_OK) {
                return SGL_GIFDEC_ERR_IO;
            }
            total += size;
            if (total > SGL_GIFDEC_SUB_BLOCK_GUARD) {
                return SGL_GIFDEC_ERR_FORMAT;
            }
        }
    } while (size != 0u);

    return SGL_GIFDEC_OK;
}

static int read_graphic_control_ext(gd_gif_t *gif)
{
    uint8_t rdit;

    if (gif_seek(gif, 1, SGL_GIFDEC_SEEK_CUR) != SGL_GIFDEC_OK) {
        return SGL_GIFDEC_ERR_IO;
    }
    if (gif_read(gif, &rdit, 1) != SGL_GIFDEC_OK) {
        return SGL_GIFDEC_ERR_IO;
    }
    gif->gce.disposal     = (uint8_t)((rdit >> 2) & 3);
    gif->gce.transparency = (uint8_t)(rdit & 1);
    if (gif_read_u16(gif, &gif->gce.delay) != SGL_GIFDEC_OK) {
        return SGL_GIFDEC_ERR_IO;
    }
    if (gif_read(gif, &gif->gce.tindex, 1) != SGL_GIFDEC_OK) {
        return SGL_GIFDEC_ERR_IO;
    }
    return gif_seek(gif, 1, SGL_GIFDEC_SEEK_CUR);
}

static int read_application_ext(gd_gif_t *gif)
{
    char app_id[8];

    if (gif_seek(gif, 1, SGL_GIFDEC_SEEK_CUR) != SGL_GIFDEC_OK) {
        return SGL_GIFDEC_ERR_IO;
    }
    if (gif_read(gif, app_id, 8) != SGL_GIFDEC_OK) {
        return SGL_GIFDEC_ERR_IO;
    }
    if (gif_seek(gif, 3, SGL_GIFDEC_SEEK_CUR) != SGL_GIFDEC_OK) {
        return SGL_GIFDEC_ERR_IO;
    }

    if (memcmp(app_id, "NETSCAPE", sizeof(app_id)) == 0) {
        uint16_t loop;
        if (gif_seek(gif, 2, SGL_GIFDEC_SEEK_CUR) != SGL_GIFDEC_OK) {
            return SGL_GIFDEC_ERR_IO;
        }
        if (gif_read_u16(gif, &loop) != SGL_GIFDEC_OK) {
            return SGL_GIFDEC_ERR_IO;
        }
        gif->loop_count = loop;
        return gif_seek(gif, 1, SGL_GIFDEC_SEEK_CUR);
    }

    return discard_sub_blocks(gif);
}

static int read_ext(gd_gif_t *gif)
{
    uint8_t label;

    if (gif_read(gif, &label, 1) != SGL_GIFDEC_OK) {
        return SGL_GIFDEC_ERR_IO;
    }

    switch (label) {
    case 0xF9:
        return read_graphic_control_ext(gif);
    case 0xFF:
        return read_application_ext(gif);
    case 0x01:
        if (gif_seek(gif, 13, SGL_GIFDEC_SEEK_CUR) != SGL_GIFDEC_OK) {
            return SGL_GIFDEC_ERR_IO;
        }
        return discard_sub_blocks(gif);
    case 0xFE:
    default:
        return discard_sub_blocks(gif);
    }
}

/* ------------------------------------------------------------------ */
/* LZW                                                                */
/* ------------------------------------------------------------------ */
/* 根项只与 key_size (GIF 的 LZW min code size) 有关, 整个文件恒定;
 * clear code 频繁出现, 这里只初始化一次, 避免每次 clear 重刷 256~4096 项。 */
static void lzw_reset(gd_lzw_table_t *table, int key_size)
{
    int key;
    int roots = 1 << key_size;

    if (roots > table->max_entries) {
        roots = table->max_entries;
    }
    if (table->root_size != (uint16_t)roots) {
        for (key = 0; key < roots; key++) {
            table->entry[key] = SGL_GIFDEC_LZW_PACK(SGL_GIFDEC_LZW_ROOT, 1u, (uint8_t)key);
#if SGL_GIFDEC_LZW_FIRST_TABLE
            table->first[key] = (uint8_t)key;
#endif
        }
        table->root_size = (uint16_t)roots;
    }
    table->nentries = (uint16_t)(roots + 2);
    table->virt     = (uint16_t)(roots + 2);
    table->full     = 0u;
}

/* 返回 1 = 码长需 +1 (条目数跨过 2 的幂), 0 = 不变。
 * 字典被 cfg.lzw_max_entries 裁剪写满后不再写 entry, 但仍推进 virt,
 * 使 key_size 跟编码端同步增长 —— 否则编码端涨到 12bit 而解码端停住会整帧错位。 */
static int lzw_add(gd_lzw_table_t *table, uint16_t length, uint16_t prefix, uint8_t suffix)
{
    int grow = 0;

    if (table->nentries < table->max_entries) {
        table->entry[table->nentries] = SGL_GIFDEC_LZW_PACK(prefix, length, suffix);
#if SGL_GIFDEC_LZW_FIRST_TABLE
        table->first[table->nentries] = (prefix == SGL_GIFDEC_LZW_ROOT)
                                        ? suffix : table->first[prefix];
#endif
        table->nentries++;
    } else {
        table->full = 1u;
    }

    if (table->virt < 4096u) {
        table->virt++;
        if ((table->virt & (table->virt - 1u)) == 0u) {
            grow = 1;
        }
    }
    return grow;
}

/* KwKwK 修补: 把字典最后一项 (nentries-1, 即刚由 lzw_add 加入、suffix 尚待确定的槽)
 * 的 suffix 改为已知首字符 s, prefix/length 保持不变。read_image_data_range 的两处
 * 特例 (码字==最后一项 / 码字<最后一项) 共用此逻辑, 避免重复的 PACK 表达式。 */
static void lzw_patch_last_suffix(uint32_t *ent, uint16_t nentries, uint8_t s)
{
    uint32_t last = ent[nentries - 1u];
    ent[nentries - 1u] = SGL_GIFDEC_LZW_PACK(SGL_GIFDEC_LZW_PREFIX(last),
                                             SGL_GIFDEC_LZW_LENGTH(last), s);
}

/* 取 code key 对应字符串的首字符:
 *   - 开启 SGL_GIFDEC_LZW_FIRST_TABLE: O(1) 查表 (KwKwK 特例同样成立,
 *     因为该项的串 = 上一串 + 上一串首字符, 首字符即 first[prefix])
 *   - 否则沿 prefix 链走到串首 O(str_len) */
static uint8_t gd_string_head(const gd_gif_t *gif, const uint32_t *ent, uint32_t e,
                              uint16_t key, int str_len)
{
#if SGL_GIFDEC_LZW_FIRST_TABLE
    (void)ent; (void)e; (void)str_len;
    return gif->lzw->first[key];
#else
    uint32_t t2 = e;
    int d;

    (void)gif; (void)key;
    for (d = 0; d < str_len; d++) {
        if (d == str_len - 1) {
            return (uint8_t)SGL_GIFDEC_LZW_SUFFIX(t2);
        }
        if (SGL_GIFDEC_LZW_PREFIX(t2) == SGL_GIFDEC_LZW_ROOT) {
            break;
        }
        t2 = ent[SGL_GIFDEC_LZW_PREFIX(t2)];
    }
    return (uint8_t)SGL_GIFDEC_LZW_SUFFIX(t2);
#endif
}

static uint16_t get_key(gd_gif_t *gif, int key_size, uint8_t *sub_len, uint8_t *shift, uint8_t *byte)
{
    int bits_read;
    int rpad;
    int frag_size;
    uint16_t key = 0u;

    for (bits_read = 0; bits_read < key_size; bits_read += frag_size) {
        rpad = (*shift + bits_read) % 8;
        if (rpad == 0) {
            if (*sub_len == 0u) {
                if (gif_getc(gif, sub_len) != SGL_GIFDEC_OK) {
                    return 0x1000u;
                }
                if (*sub_len == 0u) {
                    return 0x1000u;
                }
            }
            if (gif_getc(gif, byte) != SGL_GIFDEC_OK) {
                return 0x1000u;
            }
            (*sub_len)--;
        }
        frag_size = SGL_GIFDEC_MIN(key_size - bits_read, 8 - rpad);
        key |= (uint16_t)(((uint16_t)((*byte) >> rpad)) << bits_read);
    }

    key &= (uint16_t)((1 << key_size) - 1);
    *shift = (uint8_t)((*shift + key_size) % 8);
    return key;
}

static int interlaced_line_index(int h, int y)
{
    int p;

    p = (h - 1) / 8 + 1;
    if (y < p) {
        return y * 8;
    }
    y -= p;
    p = (h - 5) / 8 + 1;
    if (y < p) {
        return y * 8 + 4;
    }
    y -= p;
    p = (h - 3) / 4 + 1;
    if (y < p) {
        return y * 4 + 2;
    }
    y -= p;
    return y * 2 + 1;
}

/* ------------------------------------------------------------------ */
/* 像素写出                                                            */
/* ------------------------------------------------------------------ */
static void put_rgb(uint8_t *dst, gd_out_fmt_t fmt, const uint8_t *c)
{
    if (fmt == SGL_GIFDEC_OUT_RGB565) {
        uint16_t v = (uint16_t)((((uint16_t)c[0] >> 3) << 11)
                              | (((uint16_t)c[1] >> 2) << 5)
                              |  ((uint16_t)c[2] >> 3));
        dst[0] = (uint8_t)(v & 0xFFu);
        dst[1] = (uint8_t)(v >> 8);
    } else {
        dst[0] = c[2];
        dst[1] = c[1];
        dst[2] = c[0];
    }
}

/* 写 2 字节 RGB565 (保持字节写, 目标地址可能非 2 字节对齐) */
static void gd_put_rgb565(uint8_t *dst, uint16_t v)
{
    dst[0] = (uint8_t)(v & 0xFFu);
    dst[1] = (uint8_t)(v >> 8);
}

static uint16_t gd_pack_rgb565(const uint8_t *c)
{
    return (uint16_t)((((uint16_t)c[0] >> 3) << 11)
                    | (((uint16_t)c[1] >> 2) << 5)
                    |  ((uint16_t)c[2] >> 3));
}

/* 把已读入的 colors[] (RGB888, 3 字节/项) 原地打包成输出格式查找表。
 * RGB565: 变成 2 字节/项, 像素写出从 "3 次读 + 5 次移位 + 分支" 降为 "1 次查表 + 2 次写"。
 * 原地安全性: 第 i 项先读 colors[3i..3i+2] 再写 colors[2i..2i+1], 而 2i+1 <= 3i (i>=1),
 * 且 i=0 时读写的是同一批已读过的字节, 故不会破坏尚未读取的项。
 * BGR888: 本身就是输出格式, 无需打包。 */
static void gd_palette_pack(gd_palette_t *pal, gd_out_fmt_t fmt)
{
    int i;

    if (fmt != SGL_GIFDEC_OUT_RGB565) {
        pal->lut = NULL;
        return;
    }
    for (i = 0; i < pal->size; i++) {
        uint16_t v = gd_pack_rgb565(&pal->colors[(uint32_t)i * 3u]);
        pal->colors[(uint32_t)i * 2u]      = (uint8_t)(v & 0xFFu);
        pal->colors[(uint32_t)i * 2u + 1u] = (uint8_t)(v >> 8);
    }
    pal->lut = (uint16_t *)(void *)pal->colors;
}

static void put_index(uint8_t *dst, const gd_gif_t *gif, uint8_t idx)
{
    if (gif->palette->lut != NULL) {
        gd_put_rgb565(dst, gif->palette->lut[idx]);
    } else {
        put_rgb(dst, gif->out_fmt, &gif->palette->colors[(uint32_t)idx * 3u]);
    }
}

static void put_bgcolor(uint8_t *dst, const gd_gif_t *gif)
{
    if (gif->gct.lut != NULL) {
        gd_put_rgb565(dst, gif->gct.lut[gif->bgindex]);
    } else {
        put_rgb(dst, gif->out_fmt, &gif->gct.colors[(uint32_t)gif->bgindex * 3u]);
    }
}

/* 透明像素: 写预打包的背景填充色 (bg_fill 的 RGB565 形式) */
static void put_bg_fill(uint8_t *dst, const gd_gif_t *gif)
{
    if (gif->bg_packed) {
        gd_put_rgb565(dst, gif->bg_value);
    } else {
        put_rgb(dst, gif->out_fmt, gif->bg_fill);
    }
}

/* 同一行背景是单一颜色: 只解一次像素值, 再按像素重复写 (原来每像素都要解调色板) */
static void gd_fill_bg_row(uint8_t *p, const gd_gif_t *gif, uint16_t n)
{
    uint8_t px[4];
    uint16_t x;

    put_bgcolor(px, gif);
    for (x = 0; x < n; x++) {
        p[0] = px[0];
        p[1] = px[1];
        if (gif->out_bpp >= 3) {
            p[2] = px[2];
        }
        p += gif->out_bpp;
    }
}

/* 帧内 (y,x) 像素在切片缓冲中的地址 (y/x 为帧局部坐标) */
static uint8_t *gd_pixel_addr(const gd_gif_t *gif, uint8_t *buf, uint32_t pitch,
                              int y, int x, int row0, int interlace)
{
    int oy = interlace ? interlaced_line_index((int)gif->fh, y) : y;

    return buf + (((uint32_t)((int)gif->fy + oy - row0) * pitch
                   + (uint32_t)((int)gif->fx + x)) * gif->out_bpp);
}

/* ========== slice-clipped helpers for streaming decode ========== */
/* row0 = canvas row of buf's first row (slice base). Writes land at (ry-row0). */
static uint8_t *rect_row_slice(const gd_gif_t *gif, uint8_t *buf, uint32_t pitch,
                               int ry, uint16_t rx, int row0)
{
    return buf + (((uint32_t)(ry - row0) * pitch) + (uint32_t)rx) * gif->out_bpp;
}

/* 三个 slice helper 共用的行裁剪: 画布行 [ry,ry+rh) ∩ 切片行 [row0,row1) */
static int gd_clip_rows(int ry, uint16_t rh, int row0, int row1, int *y0, int *y1)
{
    int a = ry;
    int b = ry + (int)rh;

    if (a < row0) a = row0;
    if (b > row1) b = row1;
    if (b <= a) {
        return 0;
    }
    *y0 = a;
    *y1 = b;
    return 1;
}

/* Fill bg color over canvas rows [ry,ry+rh) intersected with slice rows [row0,row1). */
static void fill_rect_bg_slice(gd_gif_t *gif, uint8_t *buf, uint32_t pitch,
                               uint16_t rx, int ry, uint16_t rw, uint16_t rh,
                               int row0, int row1)
{
    int y0, y1, yy;

    if (!gd_clip_rows(ry, rh, row0, row1, &y0, &y1)) {
        return;
    }
    for (yy = y0; yy < y1; yy++) {
        gd_fill_bg_row(rect_row_slice(gif, buf, pitch, yy, rx, row0), gif, rw);
    }
}

/* Copy current display rows [ry,ry+rh) intersected with slice rows [row0,row1)
   into gif->restore (row offset). */
static void copy_rect_out_slice(gd_gif_t *gif, uint8_t *buf, uint32_t pitch,
                                uint16_t rx, int ry, uint16_t rw, uint16_t rh,
                                int row0, int row1)
{
    int y0, y1, yy;
    uint8_t *dst;
    uint32_t stride = (uint32_t)rw * gif->out_bpp;

    if (!gd_clip_rows(ry, rh, row0, row1, &y0, &y1)) {
        return;
    }
    dst = gif->restore + (uint32_t)(y0 - ry) * stride;
    for (yy = y0; yy < y1; yy++) {
        memcpy(dst, rect_row_slice(gif, buf, pitch, yy, rx, row0), stride);
        dst += stride;
    }
}

/* Restore rows [ry,ry+rh) intersected with slice rows [row0,row1)
   from gif->restore (row offset). */
static void copy_rect_in_slice(gd_gif_t *gif, uint8_t *buf, uint32_t pitch,
                               uint16_t rx, int ry, uint16_t rw, uint16_t rh,
                               int row0, int row1)
{
    int y0, y1, yy;
    uint8_t *src;
    uint32_t stride = (uint32_t)rw * gif->out_bpp;

    if (!gd_clip_rows(ry, rh, row0, row1, &y0, &y1)) {
        return;
    }
    src = gif->restore + (uint32_t)(y0 - ry) * stride;
    for (yy = y0; yy < y1; yy++) {
        memcpy(rect_row_slice(gif, buf, pitch, yy, rx, row0), src, stride);
        src += stride;
    }
}

/* Read current frame descriptor header (canvas frame rect, LCT, disposal=3 restore alloc). */
static int restore_ensure(gd_gif_t *gif, uint32_t need);
static int read_image_data_range(gd_gif_t *gif, uint8_t *buf, uint32_t pitch,
                                 int row0, int row1);

static int read_image_range_header(gd_gif_t *gif)
{
    uint8_t fisrz;
    int interlace;

    /* 先把"上一帧矩形"固化到 prv_*: 下面读到的 fx..fh 会覆盖当前帧矩形。
       (旧实现把当前帧写进 pfx, 再在调用方另存一份 r_apfx 快照,
        命名与语义相反且同一份数据存了两遍) */
    gif->prv_fx = gif->fx; gif->prv_fy = gif->fy;
    gif->prv_fw = gif->fw; gif->prv_fh = gif->fh;

    if (gif_read_u16(gif, &gif->fx) != SGL_GIFDEC_OK) return SGL_GIFDEC_ERR_IO;
    if (gif_read_u16(gif, &gif->fy) != SGL_GIFDEC_OK) return SGL_GIFDEC_ERR_IO;
    if (gif->fx >= gif->width || gif->fy >= gif->height) return SGL_GIFDEC_ERR_FORMAT;
    if (gif_read_u16(gif, &gif->fw) != SGL_GIFDEC_OK) return SGL_GIFDEC_ERR_IO;
    if (gif_read_u16(gif, &gif->fh) != SGL_GIFDEC_OK) return SGL_GIFDEC_ERR_IO;
    if (gif->fw == 0u || gif->fh == 0u) return SGL_GIFDEC_ERR_FORMAT;
    if (gif->fw > (uint16_t)(gif->width - gif->fx)) gif->fw = (uint16_t)(gif->width - gif->fx);
    if (gif->fh > (uint16_t)(gif->height - gif->fy)) gif->fh = (uint16_t)(gif->height - gif->fy);
    if (gif->fw == 0u || gif->fh == 0u) return SGL_GIFDEC_ERR_FORMAT;
    if (gif_read(gif, &fisrz, 1) != SGL_GIFDEC_OK) return SGL_GIFDEC_ERR_IO;
    interlace = fisrz & 0x40;
    gif->r_interlace = (uint8_t)interlace;
    if (fisrz & 0x80) {
        if (gif->cfg.enable_local_palette) {
            gif->lct.size = 1 << ((fisrz & 0x07) + 1);
            if (gif->lct.size > gif->cfg.max_palette_entries) return SGL_GIFDEC_ERR_FORMAT;
            if (gif_read(gif, gif->lct.colors, (uint32_t)3 * (uint32_t)gif->lct.size) != SGL_GIFDEC_OK) return SGL_GIFDEC_ERR_IO;
            gd_palette_pack(&gif->lct, gif->out_fmt);
            gif->palette = &gif->lct;
        } else {
            return SGL_GIFDEC_ERR_FORMAT;   /* 未启用局部调色板 */
        }
    } else {
        gif->palette = &gif->gct;
    }
    gif->prev_disposal = gif->gce.disposal;
    gif->restore_valid = 0u;
    if (gif->gce.disposal == 3u) {
        uint32_t need = (uint32_t)gif->fw * (uint32_t)gif->fh * gif->out_bpp;
        if (restore_ensure(gif, need) != 0) {
            gif->restore_valid = 1u;
        }
    }
    return SGL_GIFDEC_OK;
}

/* Stream-decode one SGL slice: canvas rows [row0,row1) of the current frame into buf
 * (pitch = slice width; buf must point at the row0 canvas-row base of the slice buffer). */
int gd_decode_frame_range(gd_gif_t *gif, uint8_t *buf, uint32_t pitch_px, int row0, int row1)
{
    uint8_t sep;
    uint32_t pitch;
    int r;
    if (gif == NULL || buf == NULL) return -1;
    pitch = (pitch_px == 0u) ? (uint32_t)gif->width : pitch_px;
    if (pitch < (uint32_t)gif->width) return -1;
    if (row1 > (int)gif->height) row1 = (int)gif->height;
    if (row0 < 0) row0 = 0;
    if (row1 < row0) return -1;

    if (gif->first_frame) {
        fill_rect_bg_slice(gif, buf, pitch, 0, 0, gif->width, gif->height, row0, row1);
        gif->first_frame = 0u;
    }

    /* previous frame's disposal is applied to every slice of the current frame */
    if (!gif->r_in_image && row0 == 0) {
        gif->r_apply_disposal = gif->prev_disposal;
        /* restore_valid 会被 header 重置成"本帧"的, 所以必须在这里快照上一帧的 */
        gif->r_apply_restore_valid = gif->restore_valid;
        /* 上一帧矩形已由 header 固化在 prv_* 中, 不再需要额外快照 */
    }

    if (!gif->r_in_image) {
        if (gif_read(gif, &sep, 1) != SGL_GIFDEC_OK) return -1;
        while (sep != ',') {
            if (sep == ';') { gif->r_gif_end = 1u; gif->r_apply_disposal = 0u; gif->r_in_image = 0; return 0; }
            if (sep == '!') { if (read_ext(gif) != SGL_GIFDEC_OK) return -1; }
            else return -1;
            if (gif_read(gif, &sep, 1) != SGL_GIFDEC_OK) return -1;
        }
        gif->r_gif_end = 0u;
        if (read_image_range_header(gif) != SGL_GIFDEC_OK) return -1;
    }

    /* 上一帧 disposal 应用 (放 header 后, 此时已知本帧是否全幅; 矩形用 prv_* ——
       header 在读新帧前已把上一帧矩形固化到 prv_*)。dispose=2 且本帧为全幅时,
       本帧解码覆盖全部画布(透明像素也已显式填背景), 免 fill_rect 省掉整幅重复清写。 */
    if (gif->r_apply_disposal == 3u) {
        if (gif->r_apply_restore_valid) {
            copy_rect_in_slice(gif, buf, pitch, gif->prv_fx, gif->prv_fy, gif->prv_fw, gif->prv_fh, row0, row1);
        }
    } else if (gif->r_apply_disposal == 2u) {
        int g_full = (gif->fx == 0 && gif->fy == 0 &&
                      gif->fw == gif->width && gif->fh == gif->height);
        if (!g_full) {
            fill_rect_bg_slice(gif, buf, pitch, gif->prv_fx, gif->prv_fy, gif->prv_fw, gif->prv_fh, row0, row1);
        }
    }

    /* dispose=3: save current (pre-overwrite) rows of this slice for the next frame */
    if (gif->gce.disposal == 3u && gif->restore_valid) {
        copy_rect_out_slice(gif, buf, pitch, gif->fx, gif->fy, gif->fw, gif->fh, row0, row1);
    }

    r = read_image_data_range(gif, buf, pitch, row0, row1);
    if (r == 0) gif->r_apply_disposal = 0u;
    return r;
}
static int restore_ensure(gd_gif_t *gif, uint32_t need)
{
    if ((gif->restore != NULL) && (gif->restore_size >= need)) {
        return 1;
    }

    gif->arena->used = gif->restore_mark;
    gif->restore     = (uint8_t *)arena_alloc(gif->arena, need);
    if (gif->restore == NULL) {
        gif->arena->used  = gif->restore_mark;
        gif->restore_size = 0u;
        return 0;
    }
    gif->restore_size = need;
    return 1;
}

/* ------------------------------------------------------------------ */
/* 流式 LZW 解码: 像素直接写进调用方缓冲                                */
/* ------------------------------------------------------------------ */
/* Emit the portion of one LZW string that falls inside canvas rows [row0,row1).
 * row0/row1 are CANVAS rows; offsets are relative to frame (fy). buf must point
 * to the row0 canvas-row base inside the caller's slice buffer (pitch = slice width).
 * Returns 0 when the whole string was emitted, 1 when it continues into the next
 * slice (paused), -1 on overflow. */
static int emit_string_segment(gd_gif_t *gif, uint8_t *buf, uint32_t pitch,
                               uint16_t key, uint32_t e, int str_start,
                               int str_len, int row0, int row1,
                               int *nfo, uint8_t *head_out)
{
    const uint32_t *ent = gif->lzw->entry;
    int interlace   = gif->r_interlace;
    int transparent = gif->gce.transparency;
    /* 透明判定合并成单个 tmask: 无透明帧 tmask=256 (超出 uint8 范围), (int)ch==tmask 恒假,
     * 编译器可把整条透明写路径优化掉, 免每像素一次 transparent 判断 */
    int tmask       = transparent ? (int)gif->gce.tindex : 256;
    uint16_t fw = gif->fw;
    int frm_size = (int)fw * (int)gif->fh;
    int pos_lo = str_start;
    int pos_hi = str_start + str_len - 1;
    int W0 = (row0 - (int)gif->fy) * (int)fw; if (W0 < 0) W0 = 0;
    int W1 = (row1 - (int)gif->fy) * (int)fw; if (W1 > frm_size) W1 = frm_size; if (W1 < 0) W1 = 0;
    int seg_lo, seg_hi;
    uint32_t e2;
    uint8_t head;
    int d, y, x, left_in_row;
    uint8_t *px;

    if (W1 < W0) W1 = W0;

    seg_lo = pos_lo > W0 ? pos_lo : W0;
    seg_hi = pos_hi < W1 - 1 ? pos_hi : (W1 - 1);

    if (seg_hi < seg_lo) {
        /* 整串不在本切片可见范围内 */
        head = gd_string_head(gif, ent, e, key, str_len);
        if (pos_hi < W0) { *head_out = head; *nfo = pos_hi + 1; return 0; } /* 已在切片上方 */
        *head_out = head; *nfo = str_start; return 1;                        /* 完全在下方 */
    }

#if SGL_GIFDEC_LZW_FIRST_TABLE
    head = gd_string_head(gif, ent, e, key, str_len);   /* O(1) */
#else
    head = 0;   /* 在下面同一次链遍历中取 (d == str_len - 1), 不再单独走一遍 */
#endif

    /* 逆序直写: 从 seg_hi 走到 seg_lo。
     * 每个像素的写入地址彼此独立, 逆序写与顺序写结果完全一致, 因此不再需要字符栈;
     * 同时用 x/y 游标代替 "每像素 pos/fw、pos%fw 两次除法 + 行地址重算"。 */
    y           = seg_hi / (int)fw;
    x           = seg_hi % (int)fw;
    left_in_row = x + 1;
    px          = gd_pixel_addr(gif, buf, pitch, y, x, row0, interlace);

    e2 = e;
    for (d = 0; d < str_len; d++) {
        int pos = pos_hi - d;
        uint8_t ch = (uint8_t)SGL_GIFDEC_LZW_SUFFIX(e2);

        if (d == str_len - 1) {
            head = ch;   /* 串首字符 (pos_lo 处的字符) */
        }
        if (pos >= seg_lo && pos <= seg_hi) {
            if ((int)ch == tmask) {
                /* 透明像素填可配置背景色 (bg_fill, 默认=当前屏幕背景色):
                   不依赖 surf buffer 跨帧保留, 固件每帧清黑也能正确显示。 */
                put_bg_fill(px, gif);
            } else {
                put_index(px, gif, ch);
            }
            px -= gif->out_bpp;
            if ((--left_in_row == 0) && (pos > seg_lo)) {
                y--;
                x = (int)fw - 1;
                left_in_row = (int)fw;
                px = gd_pixel_addr(gif, buf, pitch, y, x, row0, interlace);
            }
        }
#if SGL_GIFDEC_LZW_FIRST_TABLE
        if (pos < seg_lo) {
            break;   /* head 已由查表得到, 后续位置都不可见, 提前终止链遍历 */
        }
#endif
        if (d == str_len - 1) break;
        if (SGL_GIFDEC_LZW_PREFIX(e2) == SGL_GIFDEC_LZW_ROOT) break;
        e2 = ent[SGL_GIFDEC_LZW_PREFIX(e2)];
    }

    *head_out = head;
    if (pos_hi <= seg_hi) { *nfo = pos_hi + 1; return 0; }
    *nfo = seg_hi + 1; return 1;
}


/* Pausable slice decoder: decode frame pixel data for canvas rows [row0,row1)
 * into buf (pitch = slice width; buf = row0 canvas-row base in the slice buffer).
 * State lives in gif->r_* so the decode may be suspended across SGL slices.
 * Returns 0 when the whole frame is done, 1 when more slices remain, -err on failure. */
static int read_image_data_range(gd_gif_t *gif, uint8_t *buf, uint32_t pitch,
                                 int row0, int row1)
{
    int init_key_size;
    int key_size;
    int table_is_full;
    int frm_off, frm_size;
    int str_len;
    int ret;
    uint16_t key, clear, stop;
    uint32_t e;
    uint32_t *ent;
    uint8_t prev_head;
    uint8_t sub_len, shift, byte;
    int target_off;
    unsigned long budget = 0;

    if (!gif->r_in_image) {
        uint8_t b;
        if (gif_read(gif, &b, 1) != SGL_GIFDEC_OK) return SGL_GIFDEC_ERR_IO;
        gif->r_key_size = (int)b;
        if (gif->r_key_size < 2 || gif->r_key_size > 8) return SGL_GIFDEC_ERR_FORMAT;
        gif->r_start = gif_tell(gif);
        if (gif->r_start < 0) return SGL_GIFDEC_ERR_IO;
        if (discard_sub_blocks(gif) != SGL_GIFDEC_OK) return SGL_GIFDEC_ERR_IO;
        gif->r_end = gif_tell(gif);
        if (gif->r_end < 0) return SGL_GIFDEC_ERR_IO;
        if (gif_seek(gif, gif->r_start, SGL_GIFDEC_SEEK_SET) != SGL_GIFDEC_OK) return SGL_GIFDEC_ERR_IO;
        gif->r_clear = (uint16_t)(1 << gif->r_key_size);
        gif->r_stop  = (uint16_t)(gif->r_clear + 1u);
        lzw_reset(gif->lzw, gif->r_key_size);
        gif->r_key_size++;
        gif->r_init_key_size = gif->r_key_size;
        gif->r_sub_len = 0; gif->r_shift = 0; gif->r_byte = 0;
        gif->r_key = get_key(gif, gif->r_key_size, &gif->r_sub_len, &gif->r_shift, &gif->r_byte);
        gif->r_frm_off = 0;
        gif->r_table_is_full = 0;
        gif->r_prev_head = 0;
        gif->r_str_len   = 0;
        gif->r_in_image = 1;
        gif->r_in_string = 0;
    }

    key_size = gif->r_key_size;
    table_is_full = gif->r_table_is_full;
    frm_off = gif->r_frm_off;
    ret = 0;   /* 上次 lzw_add 的返回值: 只在单次迭代内有效, 不必跨切片保存 */
    key = gif->r_key;
    e = 0u;    /* 只作为 lzw_add 的 suffix 占位, 随后一定被 head/prev_head 修正, 无需保存 */
    str_len = gif->r_str_len;
    prev_head = gif->r_prev_head;
    sub_len = gif->r_sub_len; shift = gif->r_shift; byte = gif->r_byte;
    init_key_size = gif->r_init_key_size;
    ent = gif->lzw->entry;
    frm_size = (int)gif->fw * (int)gif->fh;
    clear = gif->r_clear; stop = gif->r_stop;

    target_off = (row1 - (int)gif->fy) * (int)gif->fw;
    if (target_off < 0) target_off = 0;
    if (target_off > frm_size) target_off = frm_size;

    while (frm_off < target_off) {
        if (++budget > ((unsigned long)frm_size * 8u + 400000u)) return SGL_GIFDEC_ERR_FORMAT;

        if (!gif->r_in_string) {
            if (key == clear) {
                key_size = init_key_size;
                lzw_reset(gif->lzw, key_size - 1);
                table_is_full = 0;
            } else {
                /* 字典被 cfg.lzw_max_entries 裁剪写满后仍继续调用: lzw_add 内部不再写 entry,
                   但会推进 virt 计数, 使 key_size 与编码端同步增长, 避免码长失步整帧错位。 */
                ret = lzw_add(gif->lzw, (uint16_t)(str_len + 1), key, SGL_GIFDEC_LZW_SUFFIX(e));
                if (gif->lzw->full && !table_is_full) {
                    SGL_LOG_ERROR("sgl_gifdec: LZW dict full (%d entries) — raise cfg.lzw_max_entries",
                                  (int)gif->lzw->max_entries);
                    table_is_full = 1;
                }
            }
            key = get_key(gif, key_size, &sub_len, &shift, &byte);
            if (key == clear) continue;
            if (key == stop || key == 0x1000u) { gif_seek(gif, gif->r_end, SGL_GIFDEC_SEEK_SET); gif->r_in_image = 0; return 0; }
            if (ret == 1) { key_size++; if (key_size > 12) key_size = 12; }
            if (key >= gif->lzw->nentries) { gif->r_in_image = 0; return SGL_GIFDEC_ERR_FORMAT; }
            e = ent[key];
            str_len = (int)SGL_GIFDEC_LZW_LENGTH(e);
            if (str_len <= 0) { gif->r_in_image = 0; return SGL_GIFDEC_ERR_FORMAT; }
            if (!table_is_full && (key == (uint16_t)(gif->lzw->nentries - 1u))) {
                lzw_patch_last_suffix(ent, gif->lzw->nentries, prev_head);
                e = ent[key];
            }
            gif->r_in_string = 1;
            gif->r_string_key = key;
            gif->r_str_total = str_len;
            gif->r_str_start = frm_off;
        } else {
            key = gif->r_string_key;
            str_len = gif->r_str_total;
            e = ent[key];
        }

        {
            int nfo; uint8_t head;
            int sret = emit_string_segment(gif, buf, pitch, key, e, gif->r_str_start,
                                           str_len, row0, row1, &nfo, &head);
            if (sret < 0) { gif->r_in_image = 0; return SGL_GIFDEC_ERR_FORMAT; }
            if (sret == 0) {
                if (!table_is_full && (key < (uint16_t)(gif->lzw->nentries - 1u))) {
                    lzw_patch_last_suffix(ent, gif->lzw->nentries, head);
                }
                gif->r_in_string = 0;
                prev_head = head;
                frm_off = nfo;
            } else {
                frm_off = nfo;
                break;   /* string spans into next slice; pause here */
            }
        }
    }

    gif->r_key_size = key_size;
    gif->r_table_is_full = table_is_full;
    gif->r_frm_off = frm_off;
    gif->r_key = key;
    gif->r_prev_head = prev_head;
    gif->r_str_len = str_len;
    gif->r_sub_len = sub_len; gif->r_shift = shift; gif->r_byte = byte;

    if (frm_off >= frm_size && !gif->r_in_string) { gif_seek(gif, gif->r_end, SGL_GIFDEC_SEEK_SET); gif->r_in_image = 0; return 0; }
    return 1;
}

/* ------------------------------------------------------------------ */
/* 打开 / 关闭                                                         */
/* ------------------------------------------------------------------ */
gd_gif_t *gd_open_gif(const gd_io_t *io, gd_arena_t *arena, gd_out_fmt_t fmt,
                         const sgl_gifdec_cfg_t *cfg)
{
    uint8_t sigver[3];
    uint16_t width, height;
    uint8_t fdsz, bgidx, aspect;
    int gct_sz;
    gd_gif_t *gif;

    if (io == NULL || io->read == NULL || io->seek == NULL) {
        return NULL;
    }
    if (fmt != SGL_GIFDEC_OUT_RGB565 && fmt != SGL_GIFDEC_OUT_BGR888) {
        return NULL;
    }
    if (cfg == NULL) {
        return NULL;   /* 预算必须在分配 arena 之前就校验, 否则会白占一块 arena */
    }

    if (io_read(io, sigver, 3) != SGL_GIFDEC_OK) {
        return NULL;
    }
    if (memcmp(sigver, "GIF", 3) != 0) {
        return NULL;
    }
    if (io_read(io, sigver, 3) != SGL_GIFDEC_OK) {
        return NULL;
    }
    if (memcmp(sigver, "89a", 3) != 0 && memcmp(sigver, "87a", 3) != 0) {
        return NULL;
    }

    {
        uint8_t b[2];
        if (io_read(io, b, 2) != SGL_GIFDEC_OK) return NULL;
        width = (uint16_t)(b[0] | ((uint16_t)b[1] << 8));
        if (io_read(io, b, 2) != SGL_GIFDEC_OK) return NULL;
        height = (uint16_t)(b[0] | ((uint16_t)b[1] << 8));
    }
    if (io_read(io, &fdsz, 1) != SGL_GIFDEC_OK) {
        return NULL;
    }
    if (!(fdsz & 0x80)) {
        return NULL;
    }
    if (io_read(io, &bgidx, 1) != SGL_GIFDEC_OK) {
        return NULL;
    }
    if (io_read(io, &aspect, 1) != SGL_GIFDEC_OK) {
        return NULL;
    }
    (void)aspect;

    if (width == 0u || height == 0u) {
        return NULL;
    }

    gif = (gd_gif_t *)arena_alloc(arena, (uint32_t)sizeof(gd_gif_t));
    if (gif == NULL) {
        return NULL;
    }
    memset(gif, 0, sizeof(*gif));

    gif->io     = *io;
    gif->arena  = arena;
    gif->width  = width;
    gif->height = height;
    gif->out_fmt = fmt;
    gif->out_bpp = (uint8_t)fmt;
    gif->cfg = *cfg;

    /* 内存直读: 上面的 13 字节头部是走 io_read 回调读的, 回调游标已前进到 GCT 起点;
     * 用一次回调 seek(CUR) 把 mem_pos 对齐过去, 此后 gif_read/seek/tell 全走 mem_pos,
     * 不再触碰回调 (两个游标指向同一份字节, 保持一致)。 */
    gif->mem_pos = 0u;
    if (gif->io.mem != NULL) {
        int32_t p = io_seek(&gif->io, 0, SGL_GIFDEC_SEEK_CUR);
        gif->mem_pos = (p > 0) ? (uint32_t)p : 0u;
    }

    /* 按 cfg 预算从 arena 分配 lzw 表 (entry[]) 与调色板 colors[] */
    gif->lzw = (gd_lzw_table_t *)arena_alloc(arena, (uint32_t)sizeof(gd_lzw_table_t));
    if (gif->lzw == NULL) {
        return NULL;
    }
    gif->lzw->nentries = 0u;
    gif->lzw->max_entries = cfg->lzw_max_entries;
    gif->lzw->virt      = 0u;
    gif->lzw->root_size = 0u;   /* 0 = 根项尚未初始化, 首次 lzw_reset 时填充 */
    gif->lzw->full      = 0u;
    gif->lzw->entry = (uint32_t *)arena_alloc(arena, (uint32_t)cfg->lzw_max_entries * 4u);
    if (gif->lzw->entry == NULL) {
        return NULL;
    }
#if SGL_GIFDEC_LZW_FIRST_TABLE
    gif->lzw->first = (uint8_t *)arena_alloc(arena, (uint32_t)cfg->lzw_max_entries);
    if (gif->lzw->first == NULL) {
        return NULL;
    }
#endif

    /* 全局调色板: 先解析条目数, 按【实际 gct_sz】分配 colors (2 色 GIF 只占 6B,
     * 而非 cfg.max*3 的预留); gd_palette_pack 原地 3B->2B 也只需 gct_sz*3 空间。 */
    gct_sz = 1 << ((fdsz & 0x07) + 1);
    if (gct_sz > gif->cfg.max_palette_entries) {
        return NULL;   /* 全局调色板条目数超过 cfg.max_palette_entries */
    }
    gif->gct.max_entries = (uint16_t)gct_sz;
    gif->gct.colors = (uint8_t *)arena_alloc(arena, (uint32_t)gct_sz * 3u);
    if (gif->gct.colors == NULL) {
        return NULL;
    }
    /* 局部调色板 (LCT): 帧间大小可变, 保留按 cfg.max 预分配, 供各帧复用 */
    if (cfg->enable_local_palette) {
        gif->lct.max_entries = cfg->max_palette_entries;
        gif->lct.colors = (uint8_t *)arena_alloc(arena, (uint32_t)cfg->max_palette_entries * 3u);
        if (gif->lct.colors == NULL) {
            return NULL;
        }
    }
    gif->gct.size = gct_sz;
    if (gif_read(gif, gif->gct.colors, (uint32_t)3 * (uint32_t)gct_sz) != SGL_GIFDEC_OK) {
        return NULL;
    }
    gd_palette_pack(&gif->gct, gif->out_fmt);   /* 原地打包成输出格式 LUT */
    gif->palette = &gif->gct;
    gif->bgindex = bgidx;

    gif->restore_mark = arena->used;

    gif->first_frame = 1u;

    {
        int32_t pos = gif_tell(gif);
        if (pos < 0) {
            return NULL;
        }
        gif->anim_start = (uint32_t)pos;
    }

    return gif;
}


uint32_t gd_frame_delay_ms(const gd_gif_t *gif)
{
    uint32_t d;

    if (gif == NULL) {
        return 100u;
    }
    d = (uint32_t)gif->gce.delay * 10u;
    if (d < 20u) {
        return 100u;
    }
    return d;
}

void gd_rewind(gd_gif_t *gif)
{
    if (gif == NULL) {
        return;
    }
    (void)gif_seek(gif, (int32_t)gif->anim_start, SGL_GIFDEC_SEEK_SET);
    memset(&gif->gce, 0, sizeof(gif->gce));
    gif->palette       = &gif->gct;
    gif->prev_disposal = 0u;
    gif->restore_valid = 0u;
    gif->first_frame   = 1u;
    gif->prv_fx = gif->prv_fy = gif->prv_fw = gif->prv_fh = 0u;
}


/* ======================================================================
 *  sgl_gifdec 组件
 *
 * 内存 (与帧数无关, 直写屏幕显存):
 *   arena  = gd_estimate_arena(&cfg, SGL_GIFDEC_OUT_RGB565, 0)
 *          (大小由 cfg.lzw_max_entries 主导, 见文件头"内存"段的公式与参考值)
 *   帧内容 = 直接解码进 SGL 屏幕 framebuffer 的 GIF 居中区域
 *           (不再持有独立 frame_buf / 内部 sgl_img 控件)
 * ====================================================================== */

/* ------------------------------------------------------------------ */
/* 调试开关                                                            */
/* ------------------------------------------------------------------ */
/* 逐帧详细打印 (仅 CONFIG_SGL_DEBUG 生效):
 *   0 = 只打生命周期 / 错误 (推荐, 避免 UART 刷屏)
 *   1 = 每解码一帧打印帧号 / 播放状态 */
#ifndef SGL_GIFDEC_DEBUG_FRAME
#define SGL_GIFDEC_DEBUG_FRAME   (0)
#endif

#if SGL_GIFDEC_DEBUG_FRAME
#define SGL_GIFDEC_FRAME_LOG(...)   SGL_LOG_INFO(__VA_ARGS__)
#else
#define SGL_GIFDEC_FRAME_LOG(...)   do {} while(0)
#endif

/* ------------------------------------------------------------------ */
/* widget instance struct                                              */
/* ------------------------------------------------------------------ */
typedef struct sgl_gifdec {
    sgl_obj_t           obj;            /* must be first — sgl_container_of() */

    /* --- gifdec resources (owned by this widget, freed on destroy) --- */
    gd_gif_t           *gif;            /* gifdec handle (inside arena) */
    uint8_t            *arena_buf;      /* arena backing store */
    gd_arena_t          arena;          /* gifdec arena state */
    uint8_t            *file_buf;       /* set_file() file copy (0 = none) */

    /* --- memory source --- */
    const uint8_t      *data;           /* GIF bytes (set_data: caller-owned) */
    uint32_t            data_len;
    int32_t             pos;            /* read cursor */

    /* --- placement (居中, 相对 screen 0,0) --- */
    int16_t             gif_ox, gif_oy; /* GIF 画布左上角在屏幕上的坐标 */

    /* --- timing / counters --- */
    uint16_t            width, height;     /* GIF canvas size */
    uint16_t            frame_interval_ms; /* 0 = use GIF's own delay */
    uint16_t            loop_count;        /* 0 = infinite */
    uint16_t            repeat_left;       /* loops remaining */
    uint16_t            frame_idx;         /* current visible frame index */

    /* --- 帧推进定时器 (sgl_timer) --- */
    sgl_timer_t        *timer;             /* start 时创建, stop/pause/destroy 删除 */
    uint16_t            timer_interval;    /* 当前定时器的间隔(ms); 0 = 无定时器 */
    uint8_t             start_cb_fired;    /* start_cb 只触发一次 */

    /* --- callbacks --- */
    sgl_gifdec_cb_t     start_cb;
    sgl_gifdec_cb_t     ready_cb;
    sgl_gifdec_cb_t     complete_cb;

    /* --- 解码资源预算配置 (set_cfg 注入, 默认 1396/32/128/0) --- */
    sgl_gifdec_cfg_t    cfg;

    /* --- 透明像素填充背景色 (set_bg_color 注入; 未设置默认=当前屏幕背景色) --- */
    sgl_color16_t       bg_color;      /* 用户设置的背景色 (RGB565) */
    uint8_t             bg_color_set;  /* 1 = 已调用 set_bg_color */

    /* --- state (kept last to minimise padding) --- */
    sgl_gifdec_state_t  state;
} sgl_gifdec_t;

/* ------------------------------------------------------------------ */
/* gifdec memory IO callbacks                                          */
/* ------------------------------------------------------------------ */
static int32_t sgl_gifdec_io_read(void *ctx, void *buf, uint32_t len)
{
    sgl_gifdec_t *g = (sgl_gifdec_t *)ctx;
    int32_t left = (int32_t)(g->data_len - (uint32_t)g->pos);
    int32_t n = (int32_t)len < left ? (int32_t)len : left;
    if (n <= 0) {
        return 0;
    }
    memcpy(buf, g->data + g->pos, (uint32_t)n);
    g->pos += n;
    return n;
}

static int32_t sgl_gifdec_io_seek(void *ctx, int32_t offset, int whence)
{
    sgl_gifdec_t *g = (sgl_gifdec_t *)ctx;
    int32_t base, np;

    switch (whence) {
    case SGL_SEEK_SET: base = 0; break;
    case SGL_SEEK_CUR: base = g->pos; break;
    default:          base = (int32_t)g->data_len; break; /* SGL_SEEK_END */
    }
    np = base + offset;
    if (np < 0) {
        np = 0;
    } else if ((uint32_t)np > g->data_len) {
        np = (int32_t)g->data_len;
    }
    g->pos = np;
    return np;
}

/* ------------------------------------------------------------------ */
/* internal helpers                                                    */
/* ------------------------------------------------------------------ */

/* Free all gifdec resources owned by the widget (not the widget itself,
 * nor caller-owned set_data() data).  Safe to call repeatedly. */
static void sgl_gifdec_free_resources(sgl_gifdec_t *g)
{
    if (g->arena_buf != NULL) {
        sgl_free(g->arena_buf);
        g->arena_buf = NULL;
    }
    if (g->file_buf != NULL) {
        sgl_free(g->file_buf);
        g->file_buf = NULL;
    }
    if (g->timer != NULL) {
        sgl_timer_delete(g->timer);
        g->timer = NULL;
    }
    g->timer_interval = 0u;
    g->start_cb_fired = 0u;
    g->gif       = NULL;
    g->data      = NULL;
    g->data_len  = 0u;
    g->pos       = 0;
    g->width     = 0u;
    g->height    = 0u;
    g->frame_idx = 0u;
    g->state     = SGL_GIFDEC_STATE_IDLE;
}


/* 读取当前活跃屏幕(顶层 page/screen)的背景色 (RGB565 full)。
 * 用 SGL 官方接口 sgl_screen_act() 取当前屏对象 (sgl_page_t 布局, 首成员 obj),
 * 不做父链遍历 —— 父链可能成环/含异常指针, 遍历会死循环或野指针 deref 触发看门狗复位。
 * 屏幕用 pixmap 背景 (非纯色) 时读不到纯色, 返回 0 (黑)。 */
static uint16_t sgl_gifdec_screen_bg(void)
{
    sgl_obj_t *scr;
    sgl_page_t *pg;

    scr = sgl_screen_act();
    if (scr == NULL) {
        return 0u;
    }
    pg = (sgl_page_t *)scr;
    if (pg->pixmap != NULL) {
        return 0u;
    }
    return pg->color.full;
}

/* 把透明填充背景色固化进 gd_gif_t (RGB565 输出写 bg_value/bg_packed, BGR888 写 bg_fill);
 * apply 只在 load / set_bg_color 时调用一次:
 *   已调用 set_bg_color -> 用用户设置的背景色;
 *   否则 -> 用当前屏幕背景色 (GIF 透明区"透出"所在屏背景, 视觉正确);
 *   两者都读不到 -> 黑 (0x0000)。
 * 注意: 只作用于 GIF 画布内的透明像素, gifdec 不操作画布外区域。 */
static void sgl_gifdec_apply_bg_color(sgl_gifdec_t *g)
{
    uint16_t v;

    if (g->gif == NULL) {
        return;
    }
    v = g->bg_color_set ? g->bg_color.full : sgl_gifdec_screen_bg();
    /* 输出 RGB565: v 本身就是 RGB565, 直接作 bg_value, 免 565->888->565 往返转换 */
    if (g->gif->out_fmt == SGL_GIFDEC_OUT_RGB565) {
        g->gif->bg_value  = v;
        g->gif->bg_packed = 1u;
        return;
    }
    /* BGR888 输出: 才需要把 RGB565 解回 RGB888 存 bg_fill */
    g->gif->bg_fill[0] = (uint8_t)((((v >> 11) & 0x1Fu) << 3) | (((v >> 11) & 0x1Fu) >> 2));
    g->gif->bg_fill[1] = (uint8_t)((((v >> 5)  & 0x3Fu) << 2) | (((v >> 5)  & 0x3Fu) >> 4));
    g->gif->bg_fill[2] = (uint8_t)(((v & 0x1Fu) << 3) | ((v & 0x1Fu) >> 2));
    g->gif->bg_packed = 0u;
}

/* 打开 GIF + 配置画布尺寸/居中位置 (set_data/set_file 共用) */
static void sgl_gifdec_load(sgl_gifdec_t *g)
{
    uint32_t need;

    g->pos = 0;
    need = gd_estimate_arena(&g->cfg, SGL_GIFDEC_OUT_RGB565, 0u);
    g->arena_buf = (uint8_t *)sgl_malloc(need);
    if (g->arena_buf == NULL) {
        SGL_LOG_ERROR("sgl_gifdec_load: arena alloc failed");
        sgl_gifdec_free_resources(g);
        return;
    }
    gd_arena_init(&g->arena, g->arena_buf, need);

    /* 内存直读快路径: mem/mem_len 与回调指向同一份字节 (g->data), 解码全程走
     * gif->mem_pos, 免去逐字节的函数指针 + memcpy。回调仍用于 open 时的头部解析。 */
    gd_io_t io = { sgl_gifdec_io_read, sgl_gifdec_io_seek, g, g->data, g->data_len };
    g->gif = gd_open_gif(&io, &g->arena, SGL_GIFDEC_OUT_RGB565, &g->cfg);
    if (g->gif == NULL) {
        SGL_LOG_ERROR("sgl_gifdec_load: gd_open_gif failed (bad GIF?)");
        sgl_gifdec_free_resources(g);
        return;
    }

    g->width  = g->gif->width;
    g->height = g->gif->height;

    /* 应用透明填充背景色 (未调用 set_bg_color 则取当前屏幕背景色) */
    sgl_gifdec_apply_bg_color(g);

    /* 居中 (相对 screen 0,0) */
    g->gif_ox = (int16_t)((sgl_fbdev_resolution_width()  - g->width)  / 2);
    g->gif_oy = (int16_t)((sgl_fbdev_resolution_height() - g->height) / 2);
    if (g->gif_ox < 0) g->gif_ox = 0;
    if (g->gif_oy < 0) g->gif_oy = 0;

    /* 对象自身尺寸/位置 = GIF 画布区域; SGL 刷新时 flush 该区域 */
    sgl_obj_set_size(&g->obj, (int16_t)g->width, (int16_t)g->height);
    sgl_obj_set_pos(&g->obj, g->gif_ox, g->gif_oy);
    sgl_obj_set_dirty(&g->obj);

    SGL_LOG_INFO("sgl_gifdec: loaded %dx%d loop=%d arena=%dB @(%d,%d) len=%d",
                 (int)g->width, (int)g->height,
                 (int)g->gif->loop_count, (int)need,
                 (int)g->gif_ox, (int)g->gif_oy, (int)g->data_len);
}

/* ------------------------------------------------------------------ */
/* widget construct / event callback                                   */
/* ------------------------------------------------------------------ */
/* 帧推进定时器回调: 触发 SGL 刷新, DRAW_MAIN 里推进到下一帧。
 * 用定时器代替 DRAW_MAIN 内自轮询 set_dirty(), 每帧只刷新一次,
 * 消除此前"保持+推进"重复刷新导致的屏闪。 */
static void sgl_gifdec_timer_cb(const sgl_timer_t *timer, void *user_data)
{
    (void)timer;
    sgl_obj_t *obj = (sgl_obj_t *)user_data;
    if (obj != NULL) {
        sgl_obj_set_dirty(obj);
    }
}

/* 停止帧推进定时器 */
static void sgl_gifdec_stop_timer(sgl_gifdec_t *g)
{
    if (g->timer != NULL) {
        sgl_timer_delete(g->timer);
        g->timer = NULL;
    }
    g->timer_interval = 0u;
}

/* 启动/重建帧推进定时器 (start / resume / 变帧率 共用)。
 * sgl_timer 没有"改间隔"接口 (对运行中的 timer 重复 setup 会重复挂链), 所以改间隔
 * 只能删除后重建; 间隔没变时直接复用, 避免每帧 malloc/free。
 * 注: 重建后旧 timer 对象要等 sgl_timer_handler 转到它所在槽位才释放, 期间可能
 *     多触发一次回调 —— sgl_obj_set_dirty() 是幂等的 (只置标志位), 不会重复绘制。 */
static void sgl_gifdec_arm_timer(sgl_gifdec_t *g, uint32_t interval)
{
    if (interval == 0u) {
        interval = 40u;
    }
    if (interval > 2047u) {
        interval = 2047u;   /* sgl_timer 两级时间轮的上限 (SGL_ASSERT) */
    }
    if (g->timer != NULL && g->timer_interval == (uint16_t)interval) {
        return;
    }
    sgl_gifdec_stop_timer(g);
    g->timer_interval = (uint16_t)interval;
    g->timer = sgl_timer_create();
    if (g->timer != NULL) {
        sgl_timer_setup(g->timer, sgl_gifdec_timer_cb, (uint16_t)interval, -1, &g->obj);
    }
}

/* start/resume 共用的定时器间隔 */
static void sgl_gifdec_start_timer(sgl_gifdec_t *g)
{
    uint32_t interval = (g->frame_interval_ms != 0u)
                            ? (uint32_t)g->frame_interval_ms
                            : gd_frame_delay_ms(g->gif);
    sgl_gifdec_arm_timer(g, interval);
}

/* 画布内帧矩形外保留前帧(dispose=1): 依赖 SGL surf buffer 跨帧保留, 不显式填背景,
 * 以免覆盖前帧静止图标。(画布外由 SGL page 背景 + 其他组件自行渲染, gifdec 不操作画布外) */
static void sgl_gifdec_construct_cb(sgl_surf_t *surf, sgl_obj_t *obj, sgl_event_t *evt)
{
    sgl_gifdec_t *g = sgl_container_of(obj, sgl_gifdec_t, obj);

    if (evt->type == SGL_EVENT_DESTROYED) {
        SGL_LOG_INFO("sgl_gifdec: destroyed (state=%d)", (int)g->state);
        sgl_gifdec_free_resources(g);
        return;
    }
    if (evt->type != SGL_EVENT_DRAW_MAIN) {
        return;
    }
    if (g->gif == NULL || surf == NULL) {
        return;
    }
    if (g->state != SGL_GIFDEC_STATE_PLAYING) {
        return;
    }

    /* 只处理 GIF 画布区域: 本对象尺寸 = GIF 画布, SGL 只把对象区域的 slice 传进来,
     * 画布外由 SGL page 背景 + 其他组件自行渲染, gifdec 不触碰。 */

    /* row intersection of this SGL slice with the GIF canvas (screen coords) */
    int y0 = (surf->y1 > (int)g->gif_oy) ? (int)surf->y1 : (int)g->gif_oy;
    int y1 = ((int)surf->y2 + 1 < (int)g->gif_oy + (int)g->height)
                 ? ((int)surf->y2 + 1) : ((int)g->gif_oy + (int)g->height);
    if (y1 <= y0) {
        return;   /* this slice holds no GIF pixels */
    }
    int row0 = y0 - (int)g->gif_oy;   /* canvas row range start */
    int row1 = y1 - (int)g->gif_oy;   /* canvas row range end */

    /* frame boundary: the canvas-row-0 slice advances the frame.
       The playback timer (sgl_gifdec_timer_cb) guarantees the frame interval,
       so no self-polling set_dirty() is needed here (kills flicker). */
    if (row0 == 0 && g->gif->r_in_image == 0) {
        if (!g->start_cb_fired) {
            g->start_cb_fired = 1u;
            if (g->start_cb != NULL) g->start_cb(obj);
        }
    } else if (g->gif->r_in_image == 0) {
        /* 当前帧已解码完成, 且本分片非帧头分片(row0!=0):
           本分片无新 GIF 像素, 保留前帧内容(dispose=1, surf buffer 跨帧保留), 等待下一帧 */
        return;
    }

    /* write target: slice buffer position of canvas row row0, canvas col 0 */
    {
        int32_t slice_row = y0 - (int32_t)surf->y1;
        int32_t slice_col = (int32_t)g->gif_ox - (int32_t)surf->x1;
        uint8_t *dst = (uint8_t *)surf->buffer
                       + (slice_row * (int32_t)surf->w + slice_col) * (int32_t)g->gif->out_bpp;

        int ret = gd_decode_frame_range(g->gif, dst, (uint32_t)surf->w, row0, row1);

        /* 解码后画布内帧矩形外保留前帧(dispose=1), 不覆盖前帧图标 */

        if (ret == 0) {
            if (g->gif->r_gif_end) {
                /* whole GIF loop finished (hit terminator) */
                if (g->loop_count != 0u && g->repeat_left <= 1u) {
                    g->state = SGL_GIFDEC_STATE_FINISHED;
                    sgl_gifdec_stop_timer(g);
                    SGL_LOG_INFO("sgl_gifdec: play finished at frame=%d", (int)g->frame_idx);
                    if (g->complete_cb != NULL) g->complete_cb(obj);
                    return;
                }
                if (g->loop_count != 0u) g->repeat_left--;
                g->frame_idx = 0u;
                gd_rewind(g->gif);
                g->gif->r_in_image = 0u;
                g->gif->r_in_string = 0u;
                g->gif->r_gif_end = 0u;
                int r2 = gd_decode_frame_range(g->gif, dst, (uint32_t)surf->w, row0, row1);
                if (r2 < 0) {
                    SGL_LOG_ERROR("sgl_gifdec: rewind decode err");
                    g->state = SGL_GIFDEC_STATE_IDLE;
                    return;
                }
                if (g->ready_cb != NULL) g->ready_cb(obj);
            } else {
                /* current frame fully decoded */
                g->frame_idx++;
                SGL_GIFDEC_FRAME_LOG("sgl_gifdec: frame=%d", (int)g->frame_idx);
                if (g->ready_cb != NULL) g->ready_cb(obj);
            }

            /* 未被 set_frame_interval 覆盖时跟随 GIF 自身的每帧延时, 支持变帧率 GIF
               (旧实现只在 start/resume 时取一次 delay, 变帧率会整段按首帧延时播放) */
            if (g->frame_interval_ms == 0u) {
                sgl_gifdec_arm_timer(g, gd_frame_delay_ms(g->gif));
            }
        } else if (ret < 0) {
            SGL_LOG_ERROR("sgl_gifdec: decode error at frame=%d (state->IDLE)", (int)g->frame_idx);
            sgl_gifdec_stop_timer(g);
            g->state = SGL_GIFDEC_STATE_IDLE;
            return;
        }
    }
}
/* ======================================================================
 *                            PUBLIC API
 * ====================================================================== */

sgl_obj_t *sgl_gifdec_create(sgl_obj_t *parent)
{
    sgl_gifdec_t *g = (sgl_gifdec_t *)sgl_malloc(sizeof(sgl_gifdec_t));
    if (g == NULL) {
        SGL_LOG_ERROR("sgl_gifdec_create: malloc failed (%d bytes)",
                      (int)sizeof(sgl_gifdec_t));
        return NULL;
    }
    memset(g, 0, sizeof(sgl_gifdec_t));

    sgl_obj_t *obj = &g->obj;
    sgl_obj_init(obj, parent);
    obj->construct_fn = sgl_gifdec_construct_cb;

    g->loop_count = SGL_GIFDEC_REPEAT_INFINITE;   /* default: loop forever */
    g->state      = SGL_GIFDEC_STATE_IDLE;

    /* 默认解码资源预算 (未调用 set_cfg 时使用) */
    g->cfg.lzw_max_entries      = 1396u;
    g->cfg.max_palette_entries  = 32u;
    g->cfg.stack_size           = 128u;
    g->cfg.enable_local_palette = 0u;

    SGL_LOG_INFO("sgl_gifdec: created (%d bytes)", (int)sizeof(sgl_gifdec_t));

    /* 无子控件: 本对象自身承载 GIF, 直接直写屏幕显存 */
    return obj;
}

void sgl_gifdec_set_cfg(sgl_obj_t *obj, const sgl_gifdec_cfg_t *cfg)
{
    SGL_ASSERT(obj != NULL);
    sgl_gifdec_t *g = sgl_container_of(obj, sgl_gifdec_t, obj);
    if (g == NULL || cfg == NULL) {
        return;
    }
    /* 若已加载, 释放 arena, 下次 set_data/set_file 将按新预算重新分配 */
    if (g->arena_buf != NULL) {
        sgl_gifdec_free_resources(g);
    }
    g->cfg = *cfg;
}

void sgl_gifdec_set_data(sgl_obj_t *obj, const void *data, uint32_t len)
{
    SGL_ASSERT(obj != NULL);
    sgl_gifdec_t *g = sgl_container_of(obj, sgl_gifdec_t, obj);

    sgl_gifdec_free_resources(g);
    if (data == NULL || len == 0u) {
        SGL_LOG_ERROR("sgl_gifdec_set_data: NULL/zero");
        return;
    }

    /* 调用方字节数组仅被引用 (不拷贝), 需在控件存活期间保持有效 */
    g->data     = (const uint8_t *)data;
    g->data_len = len;
    sgl_gifdec_load(g);
}

void sgl_gifdec_set_file(sgl_obj_t *obj, const char *path)
{
    SGL_ASSERT(obj != NULL);
    sgl_gifdec_t *g = sgl_container_of(obj, sgl_gifdec_t, obj);

    if (path == NULL) {
        SGL_LOG_ERROR("sgl_gifdec_set_file: NULL path");
        return;
    }

    sgl_stat_t st;
    if (sgl_fs_stat(path, &st) != 0 || st.st_size == 0u) {
        SGL_LOG_ERROR("sgl_gifdec_set_file: stat failed: %s", path);
        return;
    }

    int fd = sgl_fs_open(path, SGL_O_RDONLY);
    if (fd < 0) {
        SGL_LOG_ERROR("sgl_gifdec_set_file: open failed: %s", path);
        return;
    }

    uint8_t *buf = (uint8_t *)sgl_malloc((size_t)st.st_size);
    if (buf == NULL) {
        SGL_LOG_ERROR("sgl_gifdec_set_file: malloc failed");
        sgl_fs_close(fd);
        return;
    }

    int rd = sgl_fs_read(fd, buf, st.st_size);
    sgl_fs_close(fd);
    if (rd != (int)st.st_size) {
        SGL_LOG_ERROR("sgl_gifdec_set_file: short read");
        sgl_free(buf);
        return;
    }

    /* 接管文件拷贝, 走与 set_data 相同的装载逻辑 */
    sgl_gifdec_free_resources(g);
    g->file_buf  = buf;
    g->data      = buf;
    g->data_len  = st.st_size;
    sgl_gifdec_load(g);
}

void sgl_gifdec_set_loop_count(sgl_obj_t *obj, uint16_t count)
{
    SGL_ASSERT(obj != NULL);
    sgl_gifdec_t *g = sgl_container_of(obj, sgl_gifdec_t, obj);
    g->loop_count  = count;
    g->repeat_left = count;
}

void sgl_gifdec_set_frame_interval(sgl_obj_t *obj, uint16_t ms)
{
    SGL_ASSERT(obj != NULL);
    sgl_gifdec_t *g = sgl_container_of(obj, sgl_gifdec_t, obj);
    g->frame_interval_ms = ms;
}

void sgl_gifdec_set_bg_color(sgl_obj_t *obj, sgl_color_t color)
{
    SGL_ASSERT(obj != NULL);
    sgl_gifdec_t *g = sgl_container_of(obj, sgl_gifdec_t, obj);
    g->bg_color      = color;
    g->bg_color_set  = 1u;
    /* 已加载则立即应用; 未加载则在 set_data/set_file 的 load 里应用 */
    sgl_gifdec_apply_bg_color(g);
}

void sgl_gifdec_set_start_cb(sgl_obj_t *obj, sgl_gifdec_cb_t cb)
{
    SGL_ASSERT(obj != NULL);
    sgl_gifdec_t *g = sgl_container_of(obj, sgl_gifdec_t, obj);
    g->start_cb = cb;
}

void sgl_gifdec_set_ready_cb(sgl_obj_t *obj, sgl_gifdec_cb_t cb)
{
    SGL_ASSERT(obj != NULL);
    sgl_gifdec_t *g = sgl_container_of(obj, sgl_gifdec_t, obj);
    g->ready_cb = cb;
}

void sgl_gifdec_set_complete_cb(sgl_obj_t *obj, sgl_gifdec_cb_t cb)
{
    SGL_ASSERT(obj != NULL);
    sgl_gifdec_t *g = sgl_container_of(obj, sgl_gifdec_t, obj);
    g->complete_cb = cb;
}

void sgl_gifdec_start(sgl_obj_t *obj)
{
    SGL_ASSERT(obj != NULL);
    sgl_gifdec_t *g = sgl_container_of(obj, sgl_gifdec_t, obj);

    if (g->gif == NULL) {
        SGL_LOG_ERROR("sgl_gifdec_start: no data — call set_data()/set_file() first");
        return;
    }

    g->repeat_left = g->loop_count;
    g->frame_idx   = 0u;
    g->state       = SGL_GIFDEC_STATE_PLAYING;
    g->start_cb_fired = 0u;    /* 重新播放时 start_cb 再触发一次 */

    /* 启动帧推进定时器: 每 interval 触发一次刷新, DRAW_MAIN 推进一帧 */
    sgl_gifdec_start_timer(g);

    SGL_LOG_INFO("sgl_gifdec: start playing (loop=%d)", (int)g->repeat_left);
    /* 只 dirty 本对象(GIF 画布区域); 画布外由 SGL page 背景 + 其他组件自行渲染, 不触碰 */
    sgl_obj_set_dirty(&g->obj);
}

void sgl_gifdec_stop(sgl_obj_t *obj)
{
    SGL_ASSERT(obj != NULL);
    sgl_gifdec_t *g = sgl_container_of(obj, sgl_gifdec_t, obj);
    sgl_gifdec_stop_timer(g);
    SGL_LOG_INFO("sgl_gifdec: stop (state %d -> IDLE)", (int)g->state);
    g->state = SGL_GIFDEC_STATE_IDLE;
}

void sgl_gifdec_pause(sgl_obj_t *obj)
{
    SGL_ASSERT(obj != NULL);
    sgl_gifdec_t *g = sgl_container_of(obj, sgl_gifdec_t, obj);
    if (g->state == SGL_GIFDEC_STATE_PLAYING) {
        g->state = SGL_GIFDEC_STATE_PAUSED;
        sgl_gifdec_stop_timer(g);
        SGL_LOG_INFO("sgl_gifdec: pause at frame=%d", (int)g->frame_idx);
    }
}

void sgl_gifdec_resume(sgl_obj_t *obj)
{
    SGL_ASSERT(obj != NULL);
    sgl_gifdec_t *g = sgl_container_of(obj, sgl_gifdec_t, obj);
    if (g->state == SGL_GIFDEC_STATE_PAUSED) {
        g->state = SGL_GIFDEC_STATE_PLAYING;
        /* 帧间隔由定时器保证, 不需要用 tick 做补偿; start_cb 也不再重复触发 */
        sgl_gifdec_start_timer(g);
        SGL_LOG_INFO("sgl_gifdec: resume from frame=%d", (int)g->frame_idx);
        sgl_obj_set_dirty(obj);
    }
}

void sgl_gifdec_reset(sgl_obj_t *obj)
{
    SGL_ASSERT(obj != NULL);
    sgl_gifdec_t *g = sgl_container_of(obj, sgl_gifdec_t, obj);

    if (g->gif != NULL) {
        gd_rewind(g->gif);
    }
    g->repeat_left = g->loop_count;
    g->frame_idx   = 0u;
    g->state       = SGL_GIFDEC_STATE_IDLE;
    SGL_LOG_INFO("sgl_gifdec: reset (state->IDLE)");
    sgl_obj_set_dirty(obj);
}

uint8_t sgl_gifdec_is_playing(sgl_obj_t *obj)
{
    SGL_ASSERT(obj != NULL);
    sgl_gifdec_t *g = sgl_container_of(obj, sgl_gifdec_t, obj);
    return (g->state == SGL_GIFDEC_STATE_PLAYING) ? 1u : 0u;
}

uint8_t sgl_gifdec_is_finished(sgl_obj_t *obj)
{
    SGL_ASSERT(obj != NULL);
    sgl_gifdec_t *g = sgl_container_of(obj, sgl_gifdec_t, obj);
    return (g->state == SGL_GIFDEC_STATE_FINISHED) ? 1u : 0u;
}

uint16_t sgl_gifdec_get_frame_idx(sgl_obj_t *obj)
{
    SGL_ASSERT(obj != NULL);
    sgl_gifdec_t *g = sgl_container_of(obj, sgl_gifdec_t, obj);
    return g->frame_idx;
}

uint16_t sgl_gifdec_get_frame_cnt(sgl_obj_t *obj)
{
    SGL_ASSERT(obj != NULL);
    (void)obj;
    return 0u;   /* 帧数统计未实现 */
}

uint16_t sgl_gifdec_get_width(sgl_obj_t *obj)
{
    SGL_ASSERT(obj != NULL);
    sgl_gifdec_t *g = sgl_container_of(obj, sgl_gifdec_t, obj);
    return g->width;
}

uint16_t sgl_gifdec_get_height(sgl_obj_t *obj)
{
    SGL_ASSERT(obj != NULL);
    sgl_gifdec_t *g = sgl_container_of(obj, sgl_gifdec_t, obj);
    return g->height;
}

void sgl_gifdec_destroy(sgl_obj_t *obj)
{
    SGL_ASSERT(obj != NULL);
    sgl_obj_delete_sync(obj);
}


