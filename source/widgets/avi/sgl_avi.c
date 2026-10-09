/* source/widgets/avi/sgl_avi.c
 *
 * AVI container player widget: video = MJPEG (one JPEG frame per "00dc"
 * chunk), audio = uncompressed PCM ("01wb" chunks), output through the
 * platform callbacks registered with sgl_avi_set_audio_port().
 *
 * The JPEG decoder below is the inlined TJpgDec R0.03.
 *
 * Playback engine (rewritten, derived from the reference implementation in
 * sgl/video_test):
 *  - audio and video share one file descriptor. Reads use explicit offsets,
 *    and the playback task serializes the audio pump before video decoding,
 *    so the shared file cursor is repositioned before each chunk is read.
 *  - each video chunk is bulk-read into a staging buffer first and the
 *    JPEG decoder then runs completely from RAM instead of interleaving
 *    small SD transactions into the huffman hot loop. Oversized chunks
 *    fall back to a streaming decode straight from the file.
 *  - frame positions come from the idx1 table (exact offsets, binary
 *    search seek). The reader also works on files without an idx1 via a
 *    sequential chunk walk.
 *  - the audio feed position is the master clock: the video decoder only
 *    advances when the audio clock has reached the next frame time, and
 *    the picture is presented on the ISR-consumed (played) audio clock,
 *    with a stall watchdog so the picture can never freeze waiting on a
 *    stalled clock.
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
#include <sgl_cfgfix.h>
#include <sgl_theme.h>
#include "sgl_avi.h"

/*===========================================================================*/
/*  Built-in JPEG decoder (inlined TJpgDec R0.03, no external component)     */
/*---------------------------------------------------------------------------*/
/*  Based on TJpgDec - Tiny JPEG Decompressor R0.03          (C)ChaN, 2021   */
/*  http://elm-chan.org/fsw/tjpgd/00index.html                               */
/*                                                                           */
/*  The TJpgDec is a generic JPEG decompressor module for tiny embedded      */
/*  systems. This is a free software that opened for education, research     */
/*  and commercial developments under license policy of following terms.     */
/*                                                                           */
/*  Copyright (C) 2021, ChaN, all right reserved.                            */
/*                                                                           */
/*  * The TJpgDec module is a free software and there is NO WARRANTY.        */
/*  * No restriction on use. You can use, modify and redistribute it for     */
/*    personal, non-profit or commercial products UNDER YOUR RESPONSIBILITY. */
/*  * Redistributions of source code must retain the above copyright notice. */
/*                                                                           */
/*  The decoder is inlined into this file and all symbols are static so      */
/*  that no external tjpgd component is needed. It is configured for this    */
/*  project: RGB565 output, 1/1..1/8 scaling, fast huffman decode (LUT).     */
/*===========================================================================*/

/* stream input buffer, must be a power of 2. 4KB halves the number of
 * buffer-refill calls in the huffman hot loop compared to the 2KB default
 * and each refill is then a multi-sector transfer on the block device */
#ifndef MJDEC_SZBUF
#define MJDEC_SZBUF         4096 /* Size of stream input buffer, must be a power of 2 */
#endif
#ifndef MJDEC_USE_SCALE
#define MJDEC_USE_SCALE     1 /* Use descaling feature (1:enabled, 0:disabled) */
#endif
#ifndef MJDEC_TBLCLIP
#define MJDEC_TBLCLIP       1 /* Use table for saturation (1:enabled, 0:disabled) */
#endif
#ifndef MJDEC_FASTDECODE
#define MJDEC_FASTDECODE    2 /* 0:Basic, 1:Faster, 2:Faster+ (more RAM) */
#endif

/* Match the decoder output format to SGL's configured framebuffer depth. */
#if CONFIG_SGL_FBDEV_PIXEL_DEPTH == SGL_COLOR_RGB565
#define MJDEC_FORMAT 1
#elif CONFIG_SGL_FBDEV_PIXEL_DEPTH == SGL_COLOR_RGB888
#define MJDEC_FORMAT 0
#else
#error "sgl_avi supports CONFIG_SGL_FBDEV_PIXEL_DEPTH 16 (RGB565) or 24 (RGB888)"
#endif

/* The inlined decoder implementation is specialized for these options. */
#if MJDEC_TBLCLIP != 1 || MJDEC_FASTDECODE != 2
#error "sgl_avi: the inlined decoder requires MJDEC_TBLCLIP=1 and MJDEC_FASTDECODE=2"
#endif

#if MJDEC_FASTDECODE >= 1
typedef int16_t mj_yuv_t;
#else
typedef uint8_t mj_yuv_t;
#endif

/* Error code */
typedef enum {
    MJDR_OK = 0, /* 0: Succeeded */
    MJDR_INTR,   /* 1: Interrupted by output function */
    MJDR_INP,    /* 2: Device error or wrong termination of input stream */
    MJDR_MEM1,   /* 3: Insufficient memory pool for the image */
    MJDR_MEM2,   /* 4: Insufficient stream input buffer */
    MJDR_PAR,    /* 5: Parameter error */
    MJDR_FMT1,   /* 6: Data format error (may be broken data) */
    MJDR_FMT2,   /* 7: Right format but not supported */
    MJDR_FMT3    /* 8: Not supported JPEG standard */
} MJRESULT;

/* Rectangular region in the output image */
typedef struct {
    uint16_t left, right, top, bottom;
} MJRECT;

/* Decompressor object structure */
typedef struct MJDEC MJDEC;
struct MJDEC {
    size_t dctr;              /* Number of bytes available in the input buffer */
    uint8_t *dptr;            /* Current data read ptr */
    uint8_t *inbuf;           /* Bit stream input buffer */
    uint8_t dbit;             /* Number of bits available in wreg */
    uint8_t scale;            /* Output scaling ratio */
    uint8_t msx, msy;         /* MCU size in unit of block (width, height) */
    uint8_t qtid[3];          /* Quantization table ID of each component, Y, Cb, Cr */
    uint8_t ncomp;            /* Number of color components 1:grayscale, 3:color */
    int16_t dcv[3];           /* Previous DC element of each component */
    uint16_t nrst;            /* Restart interval */
    uint16_t rst;             /* Restart count */
    uint16_t rsc;             /* Expected restart sequence ID */
    uint16_t width, height;   /* Size of the input image (pixel) */
    uint8_t *huffbits[2][2];  /* Huffman bit distribution tables [id][dcac] */
    uint16_t *huffcode[2][2]; /* Huffman code word tables [id][dcac] */
    uint8_t *huffdata[2][2];  /* Huffman decoded data tables [id][dcac] */
    int32_t *qttbl[4];        /* Dequantizer tables [id] */
#if MJDEC_FASTDECODE >= 1
    uint32_t wreg;  /* Working shift register */
    uint8_t marker; /* Detected marker (0:None) */
#if MJDEC_FASTDECODE == 2
    uint8_t longofs[2][2];   /* Table offset of long code [id][dcac] */
    uint16_t *hufflut_ac[2]; /* Fast huffman decode tables for AC short code [id] */
    uint8_t *hufflut_dc[2];  /* Fast huffman decode tables for DC short code [id] */
#endif
#endif
    void *workbuf;                                /* Working buffer for IDCT and RGB output */
    mj_yuv_t *mcubuf;                             /* Working buffer for the MCU */
    void *pool;                                   /* Pointer to available memory pool */
    void *pool_original;                          /* Pointer to original pool */
    size_t sz_pool;                               /* Size of memory pool (bytes available) */
    size_t (*infunc)(MJDEC *, uint8_t *, size_t); /* Pointer to jpeg stream input function */
    void *device; /* Pointer to I/O device identifier for the session */
};

#if MJDEC_FASTDECODE == 2
#define HUFF_BIT 8 /* LUT uses about 2.5 KiB instead of 6 KiB at 10 bits */
#define HUFF_LEN (1 << HUFF_BIT)
#define HUFF_MASK (HUFF_LEN - 1)
#endif

/*-----------------------------------------------*/
/* Zigzag-order to raster-order conversion table */
/*-----------------------------------------------*/

static const uint8_t Zig[64] = {0,  1,  8,  16, 9,  2,  3,  10, 17, 24, 32, 25, 18, 11, 4,  5,
                                12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13, 6,  7,  14, 21, 28,
                                35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
                                58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63};

/*-------------------------------------------------*/
/* Input scale factor of Arai algorithm            */
/* (scaled up 16 bits for fixed point operations)  */
/*-------------------------------------------------*/

static const uint16_t Ipsf[64] = {
    /* See also aa_idct.png */
    (uint16_t)(1.00000 * 8192), (uint16_t)(1.38704 * 8192), (uint16_t)(1.30656 * 8192),
    (uint16_t)(1.17588 * 8192), (uint16_t)(1.00000 * 8192), (uint16_t)(0.78570 * 8192),
    (uint16_t)(0.54120 * 8192), (uint16_t)(0.27590 * 8192), (uint16_t)(1.38704 * 8192),
    (uint16_t)(1.92388 * 8192), (uint16_t)(1.81226 * 8192), (uint16_t)(1.63099 * 8192),
    (uint16_t)(1.38704 * 8192), (uint16_t)(1.08979 * 8192), (uint16_t)(0.75066 * 8192),
    (uint16_t)(0.38268 * 8192), (uint16_t)(1.30656 * 8192), (uint16_t)(1.81226 * 8192),
    (uint16_t)(1.70711 * 8192), (uint16_t)(1.53636 * 8192), (uint16_t)(1.30656 * 8192),
    (uint16_t)(1.02656 * 8192), (uint16_t)(0.70711 * 8192), (uint16_t)(0.36048 * 8192),
    (uint16_t)(1.17588 * 8192), (uint16_t)(1.63099 * 8192), (uint16_t)(1.53636 * 8192),
    (uint16_t)(1.38268 * 8192), (uint16_t)(1.17588 * 8192), (uint16_t)(0.92388 * 8192),
    (uint16_t)(0.63638 * 8192), (uint16_t)(0.32442 * 8192), (uint16_t)(1.00000 * 8192),
    (uint16_t)(1.38704 * 8192), (uint16_t)(1.30656 * 8192), (uint16_t)(1.17588 * 8192),
    (uint16_t)(1.00000 * 8192), (uint16_t)(0.78570 * 8192), (uint16_t)(0.54120 * 8192),
    (uint16_t)(0.27590 * 8192), (uint16_t)(0.78570 * 8192), (uint16_t)(1.08979 * 8192),
    (uint16_t)(1.02656 * 8192), (uint16_t)(0.92388 * 8192), (uint16_t)(0.78570 * 8192),
    (uint16_t)(0.61732 * 8192), (uint16_t)(0.42522 * 8192), (uint16_t)(0.21677 * 8192),
    (uint16_t)(0.54120 * 8192), (uint16_t)(0.75066 * 8192), (uint16_t)(0.70711 * 8192),
    (uint16_t)(0.63638 * 8192), (uint16_t)(0.54120 * 8192), (uint16_t)(0.42522 * 8192),
    (uint16_t)(0.29290 * 8192), (uint16_t)(0.14932 * 8192), (uint16_t)(0.27590 * 8192),
    (uint16_t)(0.38268 * 8192), (uint16_t)(0.36048 * 8192), (uint16_t)(0.32442 * 8192),
    (uint16_t)(0.27590 * 8192), (uint16_t)(0.21678 * 8192), (uint16_t)(0.14932 * 8192),
    (uint16_t)(0.07612 * 8192)};

/*---------------------------------------------*/
/* Conversion table for fast clipping process  */
/*---------------------------------------------*/

#if MJDEC_TBLCLIP

#define BYTECLIP(v) Clip8[(unsigned int)(v) & 0x3FF]

/* saturation table, built once at runtime (index = (int)v & 0x3FF,
 * covers the full -512..511 range the IDCT can produce) */
static uint8_t Clip8[1024];
static uint8_t clip8_ready = 0;

static void mj_clip8_init(void)
{
    unsigned int i;
    if (clip8_ready) {
        return;
    }
    for (i = 0; i < 256; i++) {
        Clip8[i] = (uint8_t)i; /* 0..255 -> identity */
    }
    for (i = 256; i < 512; i++) {
        Clip8[i] = 255; /* 256..511 -> saturate high */
    }
    for (i = 512; i < 1024; i++) {
        Clip8[i] = 0; /* -512..-1 -> saturate low */
    }
    clip8_ready = 1;
}

#else /* MJDEC_TBLCLIP */

static uint8_t BYTECLIP(int val)
{
    if (val < 0)
        return 0;
    if (val > 255)
        return 255;
    return (uint8_t)val;
}

#endif

/*-----------------------------------------------------------------------*/
/* Allocate a memory block from memory pool                              */
/*-----------------------------------------------------------------------*/

static void *alloc_pool(/* Pointer to allocated memory block (NULL:no memory available) */
                        MJDEC *jd,   /* Pointer to the decompressor object */
                        size_t ndata /* Number of bytes to allocate */
)
{
    char *rp = 0;

    ndata = (ndata + 3) & ~3; /* Align block size to the word boundary */

    if (jd->sz_pool >= ndata) {
        jd->sz_pool -= ndata;
        rp = (char *)jd->pool;           /* Get start of available memory pool */
        jd->pool = (void *)(rp + ndata); /* Allocate required bytes */
    }

    return (void *)rp; /* Return allocated memory block (NULL:no memory to allocate) */
}

/*-----------------------------------------------------------------------*/
/* Create de-quantization and prescaling tables with a DQT segment       */
/*-----------------------------------------------------------------------*/

static MJRESULT create_qt_tbl(                     /* 0:OK, !0:Failed */
                              MJDEC *jd,           /* Pointer to the decompressor object */
                              const uint8_t *data, /* Pointer to the quantizer tables */
                              size_t ndata         /* Size of input data */
)
{
    unsigned int i, zi;
    uint8_t d;
    int32_t *pb;

    while (ndata) { /* Process all tables in the segment */
        if (ndata < 65)
            return MJDR_FMT1; /* Err: table size is unaligned */
        ndata -= 65;
        d = *data++; /* Get table property */
        if (d & 0xF0)
            return MJDR_FMT1;                      /* Err: not 8-bit resolution */
        i = d & 3;                                 /* Get table ID */
        pb = alloc_pool(jd, 64 * sizeof(int32_t)); /* Allocate a memory block for the table */
        if (!pb)
            return MJDR_MEM1;      /* Err: not enough memory */
        jd->qttbl[i] = pb;         /* Register the table */
        for (i = 0; i < 64; i++) { /* Load the table */
            zi = Zig[i];           /* Zigzag-order to raster-order conversion */
            pb[zi] =
                (int32_t)((uint32_t)*data++ *
                          Ipsf[zi]); /* Apply scale factor of Arai algorithm to the de-quantizers */
        }
    }

    return MJDR_OK;
}

/*-----------------------------------------------------------------------*/
/* Create huffman code tables with a DHT segment                         */
/*-----------------------------------------------------------------------*/

static MJRESULT create_huffman_tbl(                     /* 0:OK, !0:Failed */
                                   MJDEC *jd,           /* Pointer to the decompressor object */
                                   const uint8_t *data, /* Pointer to the packed huffman tables */
                                   size_t ndata         /* Size of input data */
)
{
    unsigned int i, j, b, cls, num;
    size_t np;
    uint8_t d, *pb, *pd;
    uint16_t hc, *ph;

    while (ndata) { /* Process all tables in the segment */
        if (ndata < 17)
            return MJDR_FMT1; /* Err: wrong data size */
        ndata -= 17;
        d = *data++; /* Get table number and class */
        if (d & 0xEE)
            return MJDR_FMT1; /* Err: invalid class/number */
        cls = d >> 4;
        num = d & 0x0F;          /* class = dc(0)/ac(1), table number = 0/1 */
        pb = alloc_pool(jd, 16); /* Allocate a memory block for the bit distribution table */
        if (!pb)
            return MJDR_MEM1; /* Err: not enough memory */
        jd->huffbits[num][cls] = pb;
        for (np = i = 0; i < 16; i++) { /* Load number of patterns for 1 to 16-bit code */
            np += (pb[i] = *data++);    /* Get sum of code words for each code */
        }
        ph = alloc_pool(
            jd, np * sizeof(uint16_t)); /* Allocate a memory block for the code word table */
        if (!ph)
            return MJDR_MEM1; /* Err: not enough memory */
        jd->huffcode[num][cls] = ph;
        hc = 0;
        for (j = i = 0; i < 16; i++) { /* Re-build huffman code word table */
            b = pb[i];
            while (b--)
                ph[j++] = hc++;
            hc <<= 1;
        }

        if (ndata < np)
            return MJDR_FMT1; /* Err: wrong data size */
        ndata -= np;
        pd = alloc_pool(jd, np); /* Allocate a memory block for the decoded data */
        if (!pd)
            return MJDR_MEM1; /* Err: not enough memory */
        jd->huffdata[num][cls] = pd;
        for (i = 0; i < np; i++) { /* Load decoded data corresponds to each code word */
            d = *data++;
            if (!cls && d > 11)
                return MJDR_FMT1;
            pd[i] = d;
        }
#if MJDEC_FASTDECODE == 2
        { /* Create fast huffman decode table */
            unsigned int span, td, ti;
            uint16_t *tbl_ac = 0;
            uint8_t *tbl_dc = 0;

            if (cls) {
                tbl_ac = alloc_pool(jd, HUFF_LEN * sizeof(uint16_t)); /* LUT for AC elements */
                if (!tbl_ac)
                    return MJDR_MEM1; /* Err: not enough memory */
                jd->hufflut_ac[num] = tbl_ac;
                memset(tbl_ac, 0xFF,
                       HUFF_LEN * sizeof(uint16_t)); /* Default value (0xFFFF: may be long code) */
            } else {
                tbl_dc = alloc_pool(jd, HUFF_LEN * sizeof(uint8_t)); /* LUT for DC elements */
                if (!tbl_dc)
                    return MJDR_MEM1; /* Err: not enough memory */
                jd->hufflut_dc[num] = tbl_dc;
                memset(tbl_dc, 0xFF,
                       HUFF_LEN * sizeof(uint8_t)); /* Default value (0xFF: may be long code) */
            }
            for (i = b = 0; b < HUFF_BIT; b++) { /* Create LUT */
                for (j = pb[b]; j; j--) {
                    ti = ph[i] << (HUFF_BIT - 1 - b) &
                         HUFF_MASK; /* Index of input pattern for the code */
                    if (cls) {
                        td = pd[i++] |
                             ((b + 1)
                              << 8); /* b15..b8: code length, b7..b0: zero run and data length */
                        for (span = 1 << (HUFF_BIT - 1 - b); span;
                             span--, tbl_ac[ti++] = (uint16_t)td)
                            ;
                    } else {
                        td =
                            pd[i++] | ((b + 1) << 4); /* b7..b4: code length, b3..b0: data length */
                        for (span = 1 << (HUFF_BIT - 1 - b); span;
                             span--, tbl_dc[ti++] = (uint8_t)td)
                            ;
                    }
                }
            }
            jd->longofs[num][cls] = i; /* Code table offset for long code */
        }
#endif
    }

    return MJDR_OK;
}

/*-----------------------------------------------------------------------*/
/* Extract a huffman decoded data from input stream                      */
/*-----------------------------------------------------------------------*/

static int huffext(                 /* >=0: decoded data, <0: error code */
                   MJDEC *jd,       /* Pointer to the decompressor object */
                   unsigned int id, /* Table ID (0:Y, 1:C) */
                   unsigned int cls /* Table class (0:DC, 1:AC) */
)
{
    size_t dc = jd->dctr;
    uint8_t *dp = jd->dptr;
    unsigned int d, flg = 0;

    const uint8_t *hb, *hd;
    const uint16_t *hc;
    unsigned int nc, bl, wbit = jd->dbit % 32;
    uint32_t w = jd->wreg & ((1UL << wbit) - 1);

    while (wbit < 16) { /* Prepare 16 bits into the working register */
        if (jd->marker) {
            d = 0xFF; /* Input stream has stalled for a marker. Generate stuff bits */
        } else {
            if (!dc) {          /* Buffer empty, re-fill input buffer */
                dp = jd->inbuf; /* Top of input buffer */
                dc = jd->infunc(jd, dp, MJDEC_SZBUF);
                if (!dc)
                    return 0 - (int)MJDR_INP; /* Err: read error or wrong stream termination */
            }
            d = *dp++;
            dc--;
            if (flg) {   /* In flag sequence? */
                flg = 0; /* Exit flag sequence */
                if (d != 0)
                    jd->marker = d; /* Not an escape of 0xFF but a marker */
                d = 0xFF;
            } else {
                if (d == 0xFF) { /* Is start of flag sequence? */
                    flg = 1;
                    continue; /* Enter flag sequence, get trailing byte */
                }
            }
        }
        w = w << 8 | d; /* Shift 8 bits in the working register */
        wbit += 8;
    }
    jd->dctr = dc;
    jd->dptr = dp;
    jd->wreg = w;

    /* Table search for the short codes */
    d = (unsigned int)(w >> (wbit - HUFF_BIT)); /* Short code as table index */
    if (cls) {                                  /* AC element */
        d = jd->hufflut_ac[id][d];              /* Table decode */
        if (d != 0xFFFF) {                      /* It is done if hit in short code */
            jd->dbit = wbit - (d >> 8);         /* Snip the code length */
            return d & 0xFF;                    /* b7..0: zero run and following data bits */
        }
    } else {                            /* DC element */
        d = jd->hufflut_dc[id][d];      /* Table decode */
        if (d != 0xFF) {                /* It is done if hit in short code */
            jd->dbit = wbit - (d >> 4); /* Snip the code length */
            return d & 0xF;             /* b3..0: following data bits */
        }
    }

    /* Incremental search for the codes longer than HUFF_BIT */
    hb = jd->huffbits[id][cls] + HUFF_BIT;             /* Bit distribution table */
    hc = jd->huffcode[id][cls] + jd->longofs[id][cls]; /* Code word table */
    hd = jd->huffdata[id][cls] + jd->longofs[id][cls]; /* Data table */
    bl = HUFF_BIT + 1;
    for (; bl <= 16; bl++) { /* Incremental search */
        nc = *hb++;
        if (nc) {
            d = w >> (wbit - bl);
            do {                          /* Search the code word in this bit level */
                if (d == *hc++) {         /* Matched? */
                    jd->dbit = wbit - bl; /* Snip the huffman code */
                    return *hd;           /* Return the decoded data */
                }
                hd++;
            } while (--nc);
        }
    }

    return 0 - (int)MJDR_FMT1; /* Err: code not found (may be corrupted data) */
}

/*-----------------------------------------------------------------------*/
/* Extract N bits from input stream                                      */
/*-----------------------------------------------------------------------*/

static int bitext(                  /* >=0: extracted data, <0: error code */
                  MJDEC *jd,        /* Pointer to the decompressor object */
                  unsigned int nbit /* Number of bits to extract (1 to 16) */
)
{
    size_t dc = jd->dctr;
    uint8_t *dp = jd->dptr;
    unsigned int d, flg = 0;
    unsigned int wbit = jd->dbit % 32;
    uint32_t w = jd->wreg & ((1UL << wbit) - 1);

    while (wbit < nbit) { /* Prepare nbit bits into the working register */
        if (jd->marker) {
            d = 0xFF; /* Input stream stalled, generate stuff bits */
        } else {
            if (!dc) {          /* Buffer empty, re-fill input buffer */
                dp = jd->inbuf; /* Top of input buffer */
                dc = jd->infunc(jd, dp, MJDEC_SZBUF);
                if (!dc)
                    return 0 - (int)MJDR_INP; /* Err: read error or wrong stream termination */
            }
            d = *dp++;
            dc--;
            if (flg) {   /* In flag sequence? */
                flg = 0; /* Exit flag sequence */
                if (d != 0)
                    jd->marker = d; /* Not an escape of 0xFF but a marker */
                d = 0xFF;
            } else {
                if (d == 0xFF) { /* Is start of flag sequence? */
                    flg = 1;
                    continue; /* Enter flag sequence */
                }
            }
        }
        w = w << 8 | d; /* Get 8 bits into the working register */
        wbit += 8;
    }
    jd->wreg = w;
    jd->dbit = wbit - nbit;
    jd->dctr = dc;
    jd->dptr = dp;

    return (int)(w >> ((wbit - nbit) % 32));
}

/*-----------------------------------------------------------------------*/
/* Process restart interval                                              */
/*-----------------------------------------------------------------------*/

static MJRESULT mj_restart(MJDEC *jd,    /* Pointer to the decompressor object */
                           uint16_t rstn /* Expected restart sequence number */
)
{
    unsigned int i;
    uint8_t *dp = jd->dptr;
    size_t dc = jd->dctr;
    uint16_t marker;

    if (jd->marker) { /* Generate a maker if it has been detected */
        marker = 0xFF00 | jd->marker;
        jd->marker = 0;
    } else {
        marker = 0;
        for (i = 0; i < 2; i++) { /* Get a restart marker */
            if (!dc) {            /* No input data is available, re-fill input buffer */
                dp = jd->inbuf;
                dc = jd->infunc(jd, dp, MJDEC_SZBUF);
                if (!dc)
                    return MJDR_INP;
            }
            marker = (marker << 8) | *dp++; /* Get a byte */
            dc--;
        }
        jd->dptr = dp;
        jd->dctr = dc;
    }

    /* Check the marker */
    if ((marker & 0xFFD8) != 0xFFD0 || (marker & 7) != (rstn & 7)) {
        return MJDR_FMT1; /* Err: expected RSTn marker was not detected (may be corrupted data) */
    }

    jd->dbit = 0;                             /* Discard stuff bits */
    jd->dcv[2] = jd->dcv[1] = jd->dcv[0] = 0; /* Reset DC offsets */
    return MJDR_OK;
}

/* Apply Inverse-DCT in Arai Algorithm (see also aa_idct.png)            */
/*-----------------------------------------------------------------------*/

static void
block_idct(int32_t *src, /* Input block data (de-quantized and pre-scaled for Arai Algorithm) */
           mj_yuv_t *dst /* Pointer to the destination to store the block as byte array */
)
{
    const int32_t M13 = (int32_t)(1.41421 * 4096), M2 = (int32_t)(1.08239 * 4096),
                  M4 = (int32_t)(2.61313 * 4096), M5 = (int32_t)(1.84776 * 4096);
    int32_t v0, v1, v2, v3, v4, v5, v6, v7;
    int32_t t10, t11, t12, t13;
    int i;

    /* Process columns */
    for (i = 0; i < 8; i++) {
        v0 = src[8 * 0]; /* Get even elements */
        v1 = src[8 * 2];
        v2 = src[8 * 4];
        v3 = src[8 * 6];

        t10 = v0 + v2; /* Process the even elements */
        t12 = v0 - v2;
        t11 = (v1 - v3) * M13 >> 12;
        v3 += v1;
        t11 -= v3;
        v0 = t10 + v3;
        v3 = t10 - v3;
        v1 = t11 + t12;
        v2 = t12 - t11;

        v4 = src[8 * 7]; /* Get odd elements */
        v5 = src[8 * 1];
        v6 = src[8 * 5];
        v7 = src[8 * 3];

        t10 = v5 - v4; /* Process the odd elements */
        t11 = v5 + v4;
        t12 = v6 - v7;
        v7 += v6;
        v5 = (t11 - v7) * M13 >> 12;
        v7 += t11;
        t13 = (t10 + t12) * M5 >> 12;
        v4 = t13 - (t10 * M2 >> 12);
        v6 = t13 - (t12 * M4 >> 12) - v7;
        v5 -= v6;
        v4 -= v5;

        src[8 * 0] = v0 + v7; /* Write-back transformed values */
        src[8 * 7] = v0 - v7;
        src[8 * 1] = v1 + v6;
        src[8 * 6] = v1 - v6;
        src[8 * 2] = v2 + v5;
        src[8 * 5] = v2 - v5;
        src[8 * 3] = v3 + v4;
        src[8 * 4] = v3 - v4;

        src++; /* Next column */
    }

    /* Process rows */
    src -= 8;
    for (i = 0; i < 8; i++) {
        v0 = src[0] + (128L << 8); /* Get even elements (remove DC offset (-128) here) */
        v1 = src[2];
        v2 = src[4];
        v3 = src[6];

        t10 = v0 + v2; /* Process the even elements */
        t12 = v0 - v2;
        t11 = (v1 - v3) * M13 >> 12;
        v3 += v1;
        t11 -= v3;
        v0 = t10 + v3;
        v3 = t10 - v3;
        v1 = t11 + t12;
        v2 = t12 - t11;

        v4 = src[7]; /* Get odd elements */
        v5 = src[1];
        v6 = src[5];
        v7 = src[3];

        t10 = v5 - v4; /* Process the odd elements */
        t11 = v5 + v4;
        t12 = v6 - v7;
        v7 += v6;
        v5 = (t11 - v7) * M13 >> 12;
        v7 += t11;
        t13 = (t10 + t12) * M5 >> 12;
        v4 = t13 - (t10 * M2 >> 12);
        v6 = t13 - (t12 * M4 >> 12) - v7;
        v5 -= v6;
        v4 -= v5;

        /* Descale the transformed values 8 bits and output a row */
#if MJDEC_FASTDECODE >= 1
        dst[0] = (int16_t)((v0 + v7) >> 8);
        dst[7] = (int16_t)((v0 - v7) >> 8);
        dst[1] = (int16_t)((v1 + v6) >> 8);
        dst[6] = (int16_t)((v1 - v6) >> 8);
        dst[2] = (int16_t)((v2 + v5) >> 8);
        dst[5] = (int16_t)((v2 - v5) >> 8);
        dst[3] = (int16_t)((v3 + v4) >> 8);
        dst[4] = (int16_t)((v3 - v4) >> 8);
#else
        dst[0] = BYTECLIP((v0 + v7) >> 8);
        dst[7] = BYTECLIP((v0 - v7) >> 8);
        dst[1] = BYTECLIP((v1 + v6) >> 8);
        dst[6] = BYTECLIP((v1 - v6) >> 8);
        dst[2] = BYTECLIP((v2 + v5) >> 8);
        dst[5] = BYTECLIP((v2 - v5) >> 8);
        dst[3] = BYTECLIP((v3 + v4) >> 8);
        dst[4] = BYTECLIP((v3 - v4) >> 8);
#endif

        dst += 8;
        src += 8; /* Next row */
    }
}

/*-----------------------------------------------------------------------*/
/* Load all blocks in an MCU into working buffer                         */
/*-----------------------------------------------------------------------*/

static MJRESULT mcu_load(MJDEC *jd /* Pointer to the decompressor object */
)
{
    int32_t *tmp = (int32_t *)jd->workbuf; /* Block working buffer for de-quantize and IDCT */
    int d, e;
    unsigned int blk, nby, i, bc, z, id, cmp;
    mj_yuv_t *bp;
    const int32_t *dqf;

    nby = jd->msx * jd->msy; /* Number of Y blocks (1, 2 or 4) */
    bp = jd->mcubuf;         /* Pointer to the first block of MCU */

    for (blk = 0; blk < nby + 2; blk++) {      /* Get nby Y blocks and two C blocks */
        cmp = (blk < nby) ? 0 : blk - nby + 1; /* Component number 0:Y, 1:Cb, 2:Cr */

        if (cmp && jd->ncomp != 3) { /* Clear C blocks if not exist (monochrome image) */
            for (i = 0; i < 64; bp[i++] = 128)
                ;

        } else {              /* Load Y/C blocks from input stream */
            id = cmp ? 1 : 0; /* Huffman table ID of this component */

            /* Extract a DC element from input stream */
            d = huffext(jd, id, 0); /* Extract a huffman coded data (bit length) */
            if (d < 0)
                return (MJRESULT)(0 - d); /* Err: invalid code or input */
            bc = (unsigned int)d;
            d = jd->dcv[cmp];       /* DC value of previous block */
            if (bc) {               /* If there is any difference from previous block */
                e = bitext(jd, bc); /* Extract data bits */
                if (e < 0)
                    return (MJRESULT)(0 - e); /* Err: input */
                bc = 1 << (bc - 1);           /* MSB position */
                if (!(e & bc))
                    e -= (bc << 1) - 1;    /* Restore negative value if needed */
                d += e;                    /* Get current value */
                jd->dcv[cmp] = (int16_t)d; /* Save current DC value for next block */
            }
            dqf = jd->qttbl[jd->qtid[cmp]]; /* De-quantizer table ID for this component */
            tmp[0] = d * dqf[0] >>
                     8; /* De-quantize, apply scale factor of Arai algorithm and descale 8 bits */

            /* Extract following 63 AC elements from input stream. At 1/8
             * scale every block is emitted from its DC value, so no AC
             * coefficient workspace is consumed and clearing it is wasted. */
            if (!(MJDEC_USE_SCALE && jd->scale == 3))
                memset(&tmp[1], 0, 63 * sizeof(int32_t));
            z = 1; /* Top of the AC elements (in zigzag-order) */
            do {
                d = huffext(jd, id,
                            1); /* Extract a huffman coded value (zero runs and bit length) */
                if (d == 0)
                    break; /* EOB? */
                if (d < 0)
                    return (MJRESULT)(0 - d); /* Err: invalid code or input error */
                bc = (unsigned int)d;
                z += bc >> 4; /* Skip leading zero run */
                if (z >= 64)
                    return MJDR_FMT1;   /* Too long zero run */
                if (bc &= 0x0F) {       /* Bit length? */
                    d = bitext(jd, bc); /* Extract data bits */
                    if (d < 0)
                        return (MJRESULT)(0 - d); /* Err: input device */
                    bc = 1 << (bc - 1);           /* MSB position */
                    if (!(d & bc))
                        d -= (bc << 1) - 1;   /* Restore negative value if needed */
                    i = Zig[z];               /* Get raster-order index */
                    tmp[i] = d * dqf[i] >> 8; /* De-quantize, apply scale factor of Arai algorithm
                                                 and descale 8 bits */
                }
            } while (++z < 64); /* Next AC element */

            if (MJDEC_FORMAT != 2 ||
                !cmp) { /* C components may not be processed if in grayscale output */
                if (z == 1 ||
                    (MJDEC_USE_SCALE &&
                     jd->scale == 3)) { /* If no AC element or scale ratio is 1/8, IDCT can be
                                           ommited and the block is filled with DC value */
                    d = (mj_yuv_t)((*tmp / 256) + 128);
                    if (MJDEC_FASTDECODE >= 1) {
                        for (i = 0; i < 64; bp[i++] = d)
                            ;
                    } else {
                        memset(bp, d, 64);
                    }
                } else {
                    block_idct(tmp, bp); /* Apply IDCT and store the block to the MCU buffer */
                }
            }
        }

        bp += 64; /* Next block */
    }

    return MJDR_OK; /* All blocks have been loaded successfully */
}

/* Divide by a power of two with C's truncation-toward-zero semantics. */
static int div_pow2_toward_zero(int value, unsigned int shift)
{
    if (!shift)
        return value;
    if (value < 0)
        return -((-value) >> shift);
    return value >> shift;
}

/*-----------------------------------------------------------------------*/
/* Output an MCU: Convert YCbCr directly to RGB565                       */
/*-----------------------------------------------------------------------*/

static MJRESULT mcu_output(MJDEC *jd, /* Pointer to the decompressor object */
                           int (*outfunc)(MJDEC *, void *, MJRECT *), /* RGB output function */
                           unsigned int x, /* MCU location in the image */
                           unsigned int y  /* MCU location in the image */
)
{
    const int CVACC =
        (sizeof(int) > 2) ? 1024 : 128; /* Adaptive accuracy for both 16-/32-bit systems */
    unsigned int mx, my, rx, ry;
    int yy, cb, cr;
    MJRECT rect;

    mx = jd->msx * 8;
    my = jd->msy * 8; /* MCU size (pixel) */
    rx = (x + mx <= jd->width)
             ? mx
             : jd->width -
                   x; /* Output rectangular size (it may be clipped at right/bottom end of image) */
    ry = (y + my <= jd->height) ? my : jd->height - y;
    if (MJDEC_USE_SCALE) {
        rx >>= jd->scale;
        ry >>= jd->scale;
        if (!rx || !ry)
            return MJDR_OK; /* Skip this MCU if all pixel is to be rounded off */
        x >>= jd->scale;
        y >>= jd->scale;
    }
    rect.left = x;
    rect.right = x + rx - 1; /* Rectangular area in the frame buffer */
    rect.top = y;
    rect.bottom = y + ry - 1;

    /* Convert and scale directly from YCbCr into the configured output
     * format. Chroma samples are shared by neighboring pixels in subsampled
     * JPEGs, so average each stored chroma sample once. */
    {
#if MJDEC_FORMAT == 1
        uint16_t *out565 = (uint16_t *)jd->workbuf;
#else
        uint8_t *out888 = (uint8_t *)jd->workbuf;
#endif
        const unsigned int factor = 1U << jd->scale;
        const unsigned int y_blocks = jd->msx * jd->msy;
        const unsigned int chroma_w = (mx == 16) ? factor >> 1 : factor;
        const unsigned int chroma_h = (my == 16) ? factor >> 1 : factor;
        const unsigned int chroma_shift = (mx == 16) + (my == 16);
        const unsigned int y_shift = jd->scale * 2;
        mj_yuv_t *chroma = jd->mcubuf + y_blocks * 64;
        unsigned int ox, oy, sx, sy;

        for (oy = 0; oy < ry; oy++) {
            const unsigned int src_y0 = oy * factor;
            for (ox = 0; ox < rx; ox++) {
                const unsigned int src_x0 = ox * factor;
                const unsigned int chroma_x0 = (mx == 16) ? src_x0 >> 1 : src_x0;
                const unsigned int chroma_y0 = (my == 16) ? src_y0 >> 1 : src_y0;
                int sum_y = 0;
                int sum_cb = 0, sum_cr = 0;
                if (factor != 1) {
                    const unsigned int cb_shift = jd->scale * 2 - chroma_shift;
                    for (sy = 0; sy < factor; sy++) {
                        const unsigned int src_y = src_y0 + sy;
                        const unsigned int y_row = (src_y >> 3) * jd->msx * 64 +
                                                   (src_y & 7) * 8;
                        for (sx = 0; sx < factor; sx++) {
                            const unsigned int src_x = src_x0 + sx;
                            const unsigned int y_block = (src_x >> 3) * 64;
                            sum_y += (int)jd->mcubuf[y_row + y_block + (src_x & 7)];
                        }
                    }
                    yy = sum_y >> y_shift;

                    for (sy = 0; sy < chroma_h; sy++) {
                        const unsigned int crow = (chroma_y0 + sy) * 8;
                        for (sx = 0; sx < chroma_w; sx++) {
                            const unsigned int cx = chroma_x0 + sx;
                            const unsigned int ci = crow + cx;
                            sum_cb += (int)chroma[ci] - 128;
                            sum_cr += (int)chroma[64 + ci] - 128;
                        }
                    }
                    /* Signed division rounds toward zero, matching C's /.
                     * Avoid a target-dependent signed right-shift result. */
                    cb = div_pow2_toward_zero(sum_cb, cb_shift);
                    cr = div_pow2_toward_zero(sum_cr, cb_shift);
                } else {
                    const unsigned int src_x = src_x0;
                    const unsigned int src_y = src_y0;
                    const unsigned int y_block = (src_y >> 3) * jd->msx + (src_x >> 3);
                    const unsigned int y_index = y_block * 64 + (src_y & 7) * 8 + (src_x & 7);
                    const unsigned int cx = (mx == 16) ? src_x >> 1 : src_x;
                    const unsigned int cy = (my == 16) ? src_y >> 1 : src_y;
                    const unsigned int ci = cy * 8 + cx;
                    yy = (int)jd->mcubuf[y_index];
                    cb = (int)chroma[ci] - 128;
                    cr = (int)chroma[64 + ci] - 128;
                }
                {
                    unsigned int r = BYTECLIP(yy + ((int)(1.402 * CVACC) * cr) / CVACC);
                    unsigned int g = BYTECLIP(
                        yy - ((int)(0.344 * CVACC) * cb + (int)(0.714 * CVACC) * cr) / CVACC);
                    unsigned int b = BYTECLIP(yy + ((int)(1.772 * CVACC) * cb) / CVACC);
#if MJDEC_FORMAT == 1
                    *out565++ = (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
#else
                    *out888++ = (uint8_t)r;
                    *out888++ = (uint8_t)g;
                    *out888++ = (uint8_t)b;
#endif
                }
            }
        }
        return outfunc(jd, jd->workbuf, &rect) ? MJDR_OK : MJDR_INTR;
    }

}

/*-----------------------------------------------------------------------*/
/* Analyze the JPEG image and Initialize decompressor object             */
/*-----------------------------------------------------------------------*/

#define LDB_WORD(ptr)                                                                              \
    (uint16_t)(((uint16_t)*((uint8_t *)(ptr)) << 8) | (uint16_t)*(uint8_t *)((ptr) + 1))

static MJRESULT mj_prepare(MJDEC *jd, /* Blank decompressor object */
                           size_t (*infunc)(MJDEC *, uint8_t *,
                                            size_t), /* JPEG strem input function */
                           void *pool,     /* Working buffer for the decompression session */
                           size_t sz_pool, /* Size of working buffer */
                           void *dev       /* I/O device identifier for the session */
)
{
    uint8_t *seg, b;
    uint16_t marker;
    unsigned int n, i, ofs;
    size_t len;
    MJRESULT rc;

    memset(jd, 0, sizeof(MJDEC)); /* Clear decompression object (this might be a problem if
                                     machine's null pointer is not all bits zero) */
    jd->pool = pool;              /* Work memroy */
    jd->sz_pool = sz_pool;        /* Size of given work memory */
    jd->infunc = infunc;          /* Stream input function */
    jd->device = dev;             /* I/O device identifier */

    jd->inbuf = seg = alloc_pool(jd, MJDEC_SZBUF); /* Allocate stream input buffer */
    if (!seg)
        return MJDR_MEM1;

    ofs = marker = 0; /* Find SOI marker */
    do {
        if (jd->infunc(jd, seg, 1) != 1)
            return MJDR_INP; /* Err: SOI was not detected */
        ofs++;
        marker = marker << 8 | seg[0];
    } while (marker != 0xFFD8);

    for (;;) { /* Parse JPEG segments */
        /* Get a JPEG marker */
        if (jd->infunc(jd, seg, 4) != 4)
            return MJDR_INP;
        marker = LDB_WORD(seg);  /* Marker */
        len = LDB_WORD(seg + 2); /* Length field */
        if (len <= 2 || (marker >> 8) != 0xFF)
            return MJDR_FMT1;
        len -= 2;       /* Segent content size */
        ofs += 4 + len; /* Number of bytes loaded */

        switch (marker & 0xFF) {
        case 0xC0: /* SOF0 (baseline JPEG) */
            if (len > MJDEC_SZBUF)
                return MJDR_MEM2;
            if (jd->infunc(jd, seg, len) != len)
                return MJDR_INP; /* Load segment data */

            jd->width = LDB_WORD(&seg[3]);  /* Image width in unit of pixel */
            jd->height = LDB_WORD(&seg[1]); /* Image height in unit of pixel */
            jd->ncomp = seg[5];             /* Number of color components */
            if (jd->ncomp != 3 && jd->ncomp != 1)
                return MJDR_FMT3; /* Err: Supports only Grayscale and Y/Cb/Cr */

            /* Check each image component */
            for (i = 0; i < jd->ncomp; i++) {
                b = seg[7 + 3 * i];                            /* Get sampling factor */
                if (i == 0) {                                  /* Y component */
                    if (b != 0x11 && b != 0x22 && b != 0x21) { /* Check sampling factor */
                        return MJDR_FMT3; /* Err: Supports only 4:4:4, 4:2:0 or 4:2:2 */
                    }
                    jd->msx = b >> 4;
                    jd->msy = b & 15; /* Size of MCU [blocks] */
                } else {              /* Cb/Cr component */
                    if (b != 0x11)
                        return MJDR_FMT3; /* Err: Sampling factor of Cb/Cr must be 1 */
                }
                jd->qtid[i] = seg[8 + 3 * i]; /* Get dequantizer table ID for this component */
                if (jd->qtid[i] > 3)
                    return MJDR_FMT3; /* Err: Invalid ID */
            }
            break;

        case 0xDD: /* DRI - Define Restart Interval */
            if (len > MJDEC_SZBUF)
                return MJDR_MEM2;
            if (jd->infunc(jd, seg, len) != len)
                return MJDR_INP; /* Load segment data */

            jd->nrst = LDB_WORD(seg); /* Get restart interval (MCUs) */
            break;

        case 0xC4: /* DHT - Define Huffman Tables */
            if (len > MJDEC_SZBUF)
                return MJDR_MEM2;
            if (jd->infunc(jd, seg, len) != len)
                return MJDR_INP; /* Load segment data */

            rc = create_huffman_tbl(jd, seg, len); /* Create huffman tables */
            if (rc)
                return rc;
            break;

        case 0xDB: /* DQT - Define Quaitizer Tables */
            if (len > MJDEC_SZBUF)
                return MJDR_MEM2;
            if (jd->infunc(jd, seg, len) != len)
                return MJDR_INP; /* Load segment data */

            rc = create_qt_tbl(jd, seg, len); /* Create de-quantizer tables */
            if (rc)
                return rc;
            break;

        case 0xDA: /* SOS - Start of Scan */
            if (len > MJDEC_SZBUF)
                return MJDR_MEM2;
            if (jd->infunc(jd, seg, len) != len)
                return MJDR_INP; /* Load segment data */

            if (!jd->width || !jd->height)
                return MJDR_FMT1; /* Err: Invalid image size */
            if (seg[0] != jd->ncomp)
                return MJDR_FMT3; /* Err: Wrong color components */

            /* Check if all tables corresponding to each components have been loaded */
            for (i = 0; i < jd->ncomp; i++) {
                b = seg[2 + 2 * i]; /* Get huffman table ID */
                if (b != 0x00 && b != 0x11)
                    return MJDR_FMT3; /* Err: Different table number for DC/AC element */
                n = i ? 1 : 0;        /* Component class */
                if (!jd->huffbits[n][0] ||
                    !jd->huffbits[n][1]) { /* Check huffman table for this component */
                    return MJDR_FMT1;      /* Err: Nnot loaded */
                }
                if (!jd->qttbl[jd->qtid[i]]) { /* Check dequantizer table for this component */
                    return MJDR_FMT1;          /* Err: Not loaded */
                }
            }

            /* Allocate working buffer for MCU and pixel output */
            n = jd->msy * jd->msx; /* Number of Y blocks in the MCU */
            if (!n)
                return MJDR_FMT1;  /* Err: SOF0 has not been loaded */
            len = n * 64 * 2 + 64; /* Allocate buffer for IDCT and RGB output */
            if (len < 256)
                len = 256; /* but at least 256 byte is required for IDCT */
            jd->workbuf = alloc_pool(
                jd,
                len); /* and it may occupy a part of following MCU working buffer for RGB output */
            if (!jd->workbuf)
                return MJDR_MEM1; /* Err: not enough memory */
            jd->mcubuf =
                alloc_pool(jd, (n + 2) * 64 * sizeof(mj_yuv_t)); /* Allocate MCU working buffer */
            if (!jd->mcubuf)
                return MJDR_MEM1; /* Err: not enough memory */

            /* Align stream read offset to MJDEC_SZBUF */
            if (ofs %= MJDEC_SZBUF) {
                jd->dctr = jd->infunc(jd, seg + ofs, (size_t)(MJDEC_SZBUF - ofs));
            }
            jd->dptr = seg + ofs - (MJDEC_FASTDECODE ? 0 : 1);

            return MJDR_OK; /* Initialization succeeded. Ready to decompress the JPEG image. */

        case 0xC1:            /* SOF1 */
        case 0xC2:            /* SOF2 */
        case 0xC3:            /* SOF3 */
        case 0xC5:            /* SOF5 */
        case 0xC6:            /* SOF6 */
        case 0xC7:            /* SOF7 */
        case 0xC9:            /* SOF9 */
        case 0xCA:            /* SOF10 */
        case 0xCB:            /* SOF11 */
        case 0xCD:            /* SOF13 */
        case 0xCE:            /* SOF14 */
        case 0xCF:            /* SOF15 */
        case 0xD9:            /* EOI */
            return MJDR_FMT3; /* Unsuppoted JPEG standard (may be progressive JPEG) */

        default: /* Unknown segment (comment, exif or etc..) */
            /* Skip segment data (null pointer specifies to remove data from the stream) */
            if (jd->infunc(jd, 0, len) != len)
                return MJDR_INP;
        }
    }
}

/*-----------------------------------------------------------------------*/
/* Start to decompress the JPEG picture                                  */
/*-----------------------------------------------------------------------*/

static MJRESULT mj_decomp(MJDEC *jd, /* Initialized decompression object */
                          int (*outfunc)(MJDEC *, void *, MJRECT *), /* RGB output function */
                          uint8_t scale /* Output de-scaling factor (0 to 3) */
)
{
    unsigned int x, y, mx, my;
    uint16_t rst, rsc;
    MJRESULT rc;

    if (scale > (MJDEC_USE_SCALE ? 3 : 0))
        return MJDR_PAR;
    jd->scale = scale;

    mx = jd->msx * 8;
    my = jd->msy * 8; /* Size of the MCU (pixel) */

    jd->dcv[2] = jd->dcv[1] = jd->dcv[0] = 0; /* Initialize DC values */
    rst = rsc = 0;

    rc = MJDR_OK;
    for (y = 0; y < jd->height; y += my) {       /* Vertical loop of MCUs */
        for (x = 0; x < jd->width; x += mx) {    /* Horizontal loop of MCUs */
            if (jd->nrst && rst++ == jd->nrst) { /* Process restart interval if enabled */
                rc = mj_restart(jd, rsc++);
                if (rc != MJDR_OK)
                    return rc;
                rst = 1;
            }
            rc = mcu_load(
                jd); /* Load an MCU (decompress huffman coded stream, dequantize and apply IDCT) */
            if (rc != MJDR_OK)
                return rc;
            rc = mcu_output(jd, outfunc, x,
                            y); /* Output the MCU (YCbCr to RGB, scaling and output) */
            if (rc != MJDR_OK)
                return rc;
        }
    }

    return rc;
}

/*==========================================================================*/
/*                            AVI container layer                            */
/*==========================================================================*/

/* RIFF chunk identifiers, read as little-endian 32-bit words */
#define CC_RIFF                  0x46464952u /* "RIFF" */
#define CC_AVIF                  0x20495641u /* "AVI " */
#define CC_LIST                  0x5453494Cu /* "LIST" */
#define CC_AVIH                  0x68697661u /* "avih" */
#define CC_HDRL                  0x6C726468u /* "hdrl" */
#define CC_STRL                  0x6C727473u /* "strl" */
#define CC_STRH                  0x68727473u /* "strh" */
#define CC_STRF                  0x66727473u /* "strf" */
#define CC_MOVI                  0x69766F6Du /* "movi" */
#define CC_IDX1                  0x31786469u /* "idx1" */
#define CC_VIDS                  0x73646976u /* "vids" */
#define CC_AUDS                  0x73647561u /* "auds" */
#define CC_MJPG                  0x47504A4Du /* "MJPG" */
#define CC_00DC                  0x63643030u /* "00dc" */
#define CC_01DC                  0x63643130u /* "01dc" */
#define CC_00WB                  0x62773030u /* "00wb" */
#define CC_01WB                  0x62773130u /* "01wb" */

#define CC_ID_NONE               0u

#define AVI_MIN_FRAME_SIZE       32u /* ignore tiny/bogus chunks */
#define AVI_DEFAULT_FPS          10u
#define AVI_WALK_CHUNK_GUARD     4096 /* max chunks per video walk */
#define AVI_PUMP_CHUNK_GUARD     64   /* max chunks per pump walk */
#define AVI_POS_NONE (-1)

#ifndef AVI_AUDIDX_MAX
#define AVI_AUDIDX_MAX           512u /* audio index entry cap (sparse when exceeded) */
#endif

#define AVI_AUDSEEK_WALK_GUARD   1024 /* max chunk headers walked per audio seek */

#ifndef AVI_VIDX_MAX
#define AVI_VIDX_MAX             512u /* video index entry cap; idx1 sampled evenly beyond */
#endif

#define AVI_VSEEK_WALK_GUARD     1024 /* max chunk headers walked per video seek */
#define AVI_CLOCK_STALL_MS       500u /* audio clock watchdog */

/* video frame index entry, offsets are absolute chunk DATA positions */
typedef struct {
    int32_t offset; /* chunk data start in file */
    int32_t size;   /* chunk data size */
} avi_vidx_t;

/* audio chunk index entry used for byte-accurate seeking */
typedef struct {
    int32_t offset; /* chunk data start in file */
    int32_t cstart; /* pcm bytes before this chunk */
} avi_audidx_t;

/**
 * @brief avi player object
 */
struct sgl_avi {
    sgl_obj_t obj; /* widget object, must stay first */

    /* file handle */
    int fd;       /* shared descriptor for header, video and audio */
    char *path;   /* copy of the file path */
    int32_t file_size; /* total file size in bytes */

    /* playback state */
    sgl_avi_state_t state;
    uint8_t loop;       /* replay from start at stream end */
    uint8_t fps;        /* frame rate 1..60 */
    uint16_t period_ms; /* floor(1000 / fps) */
    uint32_t period_us; /* exact frame period in microseconds (1000000/fps) */

    /* RIFF layout */
    int32_t movi_first; /* offset of the first chunk in movi */
    int32_t movi_end;   /* offset just past the last movi chunk */
    uint32_t vflag;     /* video chunk fourcc, e.g. "00dc" */
    uint32_t aflag;     /* audio chunk fourcc, e.g. "01wb" */

    /* frame index */
    avi_vidx_t *vidx; /* frame index (from idx1), may be NULL */
    int32_t vidx_count;
    int32_t vidx_step;    /* frames per index entry, 1 = dense, 0 = no index */
    int32_t vidx_total;   /* exact video entry count in idx1, including sparse tail */
    int32_t frames_total; /* frames from avih, fallback for no idx */

    /* decoded pixmap */
    uint8_t *pixbuf;      /* configured RGB frame, pix_w * pix_h pixels */
    int32_t pixbuf_size;  /* allocated capacity in pixels */
    int16_t pix_w, pix_h; /* decoded frame size after scaling */
    uint8_t jd_scale;     /* last chosen descale factor 0..3 */
    uint8_t decode_scale; /* requested minimum descale factor 0..3 */

    /* decoder */
    uint8_t *vbuf;     /* RAM staging buffer for one chunk */
    int32_t vbuf_size; /* staging buffer capacity */
    uint8_t *jd_pool;  /* decoder work pool */

    /* video cursors */
    int32_t show_frame;    /* frame in pixbuf, AVI_POS_NONE if none */
    int32_t next_frame;    /* next frame index to decode */
    uint8_t has_frame;     /* pixbuf holds a decoded frame */
    uint8_t frame_pending; /* decoded frame waiting for the clock */
    int32_t v_pos;         /* forward-walk cursor (chunk header) */
    uint32_t v_id;         /* current chunk id while walking */
    int32_t v_size;        /* current chunk size while walking */

    /* audio stream */
    int32_t a_pos;  /* forward-walk cursor (chunk header) */
    uint32_t a_id;  /* current chunk id while walking */
    int32_t a_size; /* current chunk size, 0 = between chunks */
    uint8_t audio;  /* pcm stream present (and usable) */
    uint8_t audio_channels;
    uint8_t audio_bits;
    uint8_t audio_align;     /* bytes per sample frame */
    uint32_t audio_rate;     /* samples per second */
    int32_t aud_bytes_total; /* total pcm bytes in the file */
    avi_audidx_t *audidx;    /* byte to chunk map, may be NULL */
    int32_t audidx_count;
    uint8_t *audio_buf; /* core-owned PCM ring */
    uint32_t audio_buf_capacity;
    uint32_t audio_buf_head;
    uint32_t audio_buf_tail;
    uint32_t audio_buf_count;
    int32_t aud_off;      /* offset of the remaining chunk data */
    int32_t aud_left;     /* remaining bytes in the current chunk */
    int32_t a_next;       /* header offset of the next audio chunk */
    uint32_t audio_frame; /* sample frames fed to the device */
    int32_t played_base;  /* consumed-byte base for the clock */

    /* pacing helpers */
    uint32_t last_played;    /* last consumed count seen by the cycle */
    uint32_t last_adv_tick;  /* last tick the clock advanced */
    uint32_t play_tick_base; /* tick when playback (re)started */
    uint32_t play_ms_accum;  /* played ms accumulated before a pause */
};

/* decoder io device shared by the RAM and streaming input paths */
typedef struct {
    sgl_avi_t *avi;
    const uint8_t *data; /* RAM staging buffer, NULL = streaming */
    int32_t left;        /* bytes remaining in the staging buffer */
} avi_vstream_t;

static avi_vstream_t g_vstream;
static sgl_avi_audio_port_t g_audio_port;
static uint8_t g_audio_port_ready;

static uint32_t avi_audio_consumed_bytes(void)
{
    if (!g_audio_port_ready) {
        return 0;
    }
    return g_audio_port.get_consumed_bytes(g_audio_port.user_data);
}

static void avi_audio_ring_reset(sgl_avi_t *avi)
{
    avi->audio_buf_head = 0;
    avi->audio_buf_tail = 0;
    avi->audio_buf_count = 0;
}

/* read a little-endian 16-bit value */
static inline uint16_t avi_rd16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

/* read a little-endian 32-bit value, first char in the lowest byte */
static inline uint32_t avi_rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* positioned read: seek then read, returns bytes read or -1 */
static int32_t avi_pread(int fd, void *buf, int32_t offset, int32_t len)
{
    if (len <= 0) {
        return 0;
    }
    if (sgl_fs_seek(fd, offset, SGL_SEEK_SET) < 0) {
        return -1;
    }
    return (int32_t)sgl_fs_read(fd, buf, (uint32_t)len);
}

/*--------------------------------------------------------------------------*/
/* RIFF header parsing                                                      */
/*--------------------------------------------------------------------------*/

/**
 * @brief parse one "strl" list, detect the stream types and formats
 * @param avi pointer to the player
 * @param start offset of the "strl" fourcc inside the list
 * @param end offset just past the list content
 * @param stream_no index of this stream inside hdrl
 */
static void avi_parse_strl(sgl_avi_t *avi, int32_t start, int32_t end, int stream_no)
{
    uint8_t hdr[8];
    uint8_t buf[24];
    int32_t pos = start;
    uint32_t strh_type = 0;
    uint32_t strh_handler = 0;

    while (pos + 8 <= end) {
        uint32_t id, size;

        if (avi_pread(avi->fd, hdr, pos, 8) != 8)
            break;
        id = avi_rd32(hdr);
        size = avi_rd32(hdr + 4);
        /* a chunk cannot span more than its list; the file_size guard
         * first keeps the int32 cast below well defined */
        if (size > (uint32_t)avi->file_size || pos + 8 + (int32_t)size > end)
            break;

        if (id == CC_STRH && size >= 8) {
            /* strh: fccType at +0, fccHandler at +4 */
            if (avi_pread(avi->fd, buf, pos + 8, 8) == 8) {
                strh_type = avi_rd32(buf);
                strh_handler = avi_rd32(buf + 4);
            }
        } else if (id == CC_STRF) {
            if (strh_type == CC_VIDS && size >= 20) {
                /* bitmapinfoheader: biCompression at +16 */
                if (avi_pread(avi->fd, buf, pos + 8, 20) == 20) {
                    uint32_t comp = avi_rd32(buf + 16);
                    /* only MJPG (handler or bi compression) is decodable */
                    if (strh_handler == CC_MJPG || comp == CC_MJPG) {
                        avi->vflag = (stream_no == 0) ? CC_00DC : CC_01DC;
                        avi->aflag = (stream_no == 0) ? CC_01WB : CC_00WB;
                    }
                }
            } else if (strh_type == CC_AUDS && stream_no <= 1 && size >= 16) {
                /* waveformat is 16 bytes, waveformatex is 18: read 16 */
                if (avi_pread(avi->fd, buf, pos + 8, 16) == 16) {
                    uint16_t fmt_tag = avi_rd16(buf);
                    uint16_t channels = avi_rd16(buf + 2);
                    uint32_t rate = avi_rd32(buf + 4);
                    uint16_t align = avi_rd16(buf + 12);
                    uint16_t bits = avi_rd16(buf + 14);

                    if (fmt_tag == 1 && channels > 0 && channels <= 2 && rate > 0 && align > 0 &&
                        align <= 4 && (bits == 8 || bits == 16)) {
                        avi->audio = 1;
                        avi->audio_channels = (uint8_t)channels;
                        avi->audio_bits = (uint8_t)bits;
                        avi->audio_align = (uint8_t)align;
                        avi->audio_rate = rate;
                        avi->aflag = (stream_no == 0) ? CC_00WB : CC_01WB;
                    }
                }
            }
        }
        pos += 8 + (int32_t)size + (int32_t)(size & 1);
    }
}

/**
 * @brief parse the "hdrl" list: avih chunk plus one strl list per stream
 */
static void avi_parse_hdrl(sgl_avi_t *avi, int32_t start, int32_t end)
{
    uint8_t hdr[8];
    uint8_t sub[4];
    uint8_t buf[24];
    int32_t pos = start;
    int stream_no = 0;

    while (pos + 8 <= end) {
        uint32_t id, size;

        if (avi_pread(avi->fd, hdr, pos, 8) != 8)
            break;
        id = avi_rd32(hdr);
        size = avi_rd32(hdr + 4);
        /* a chunk cannot span more than its list */
        if (size > (uint32_t)avi->file_size || pos + 8 + (int32_t)size > end)
            break;

        if (id == CC_AVIH && size >= 24) {
            /* dwMicroSecPerFrame at +0, dwTotalFrames at +16 */
            if (avi_pread(avi->fd, buf, pos + 8, 24) == 24) {
                uint32_t usec = avi_rd32(buf);
                uint32_t nframes = avi_rd32(buf + 16);

                if (usec >= 10000u && usec <= 1000000u) {
                    uint32_t fps = (1000000u + usec / 2) / usec;
                    if (fps < 1u)
                        fps = 1u;
                    if (fps > 60u)
                        fps = 60u;
                    avi->fps = (uint8_t)fps;
                }
                if (nframes > 0u && nframes < 1000000u) {
                    avi->frames_total = (int32_t)nframes;
                }
            }
        } else if (id == CC_LIST && size >= 4) {
            if (avi_pread(avi->fd, sub, pos + 8, 4) == 4 && avi_rd32(sub) == CC_STRL) {
                avi_parse_strl(avi, pos + 12, pos + 8 + (int32_t)size, stream_no);
                stream_no++;
            }
        }
        pos += 8 + (int32_t)size + (int32_t)(size & 1);
    }
}

/**
 * @brief locate the idx1 chunk and build the frame/audio indexes
 * @param base absolute offset of the "movi" fourcc, idx offsets are
 *             relative to it
 */
static void avi_scan_idx(sgl_avi_t *avi, int32_t base)
{
    uint8_t hdr[8];
    uint8_t blk[512];
    uint32_t id, size;
    int32_t pos = avi->movi_end;
    int32_t n, i, off, remaining;
    int32_t v_count = 0, a_count = 0;
    int32_t filled = 0, aud_pos = 0;
    int32_t aud_step = 1, aud_seen = 0;
    int32_t v_step = 1, v_seen = 0, v_cap;

    /* walk the top level chunks after movi to find idx1, skipping
     * anything else in between (JUNK chunks are common here) */
    id = 0;
    size = 0;
    while (pos + 8 <= avi->file_size) {
        if (avi_pread(avi->fd, hdr, pos, 8) != 8)
            break;
        id = avi_rd32(hdr);
        size = avi_rd32(hdr + 4);
        if (id == CC_IDX1)
            break;
        /* skip anything else (JUNK etc.); the file_size guard keeps the
         * int32 cast well defined for hostile size fields */
        if (size >= (uint32_t)avi->file_size || pos + 8 + (int32_t)size > avi->file_size)
            break;
        pos += 8 + (int32_t)size + (int32_t)(size & 1);
    }

    if (pos + 8 > avi->file_size || id != CC_IDX1)
        return;
    if (size < 16u || (size & 15u))
        return;

    n = (int32_t)(size / 16);

    /* pass 1: count the entries we care about */
    off = pos + 8;
    remaining = n;
    while (remaining > 0) {
        int32_t take = remaining > 32 ? 32 : remaining;

        if (avi_pread(avi->fd, blk, off, take * 16) != take * 16)
            break;
        for (i = 0; i < take; i++) {
            uint32_t ck = avi_rd32(blk + i * 16);
            if (ck == avi->vflag)
                v_count++;
            else if (avi->aflag != 0 && ck == avi->aflag)
                a_count++;
        }
        remaining -= take;
        off += take * 16;
    }
    if (v_count == 0)
        return;

    /* bound the index for MCU targets: past AVI_VIDX_MAX entries the idx1
     * is sampled evenly (vidx_step frames per entry). Sequential playback
     * walks the movi chunks at full frame rate and does not need the index;
     * seeking lands on the nearest sampled frame and walks the few chunks
     * in between, so the footprint stays bounded no matter the duration */
    v_step = (v_count + (int32_t)AVI_VIDX_MAX - 1) / (int32_t)AVI_VIDX_MAX;
    if (v_step < 1) {
        v_step = 1;
    }
    v_cap = (v_count + v_step - 1) / v_step;

    avi->vidx = (avi_vidx_t *)sgl_malloc(sizeof(avi_vidx_t) * (size_t)v_cap);
    if (avi->vidx == NULL)
        return;

    if (a_count > 0) {
        /* store at most AVI_AUDIDX_MAX entries, sampled evenly over the
         * whole stream: seeking later than the last entry then only needs
         * a short forward walk instead of restarting at movi */
        aud_step = (a_count + (int32_t)AVI_AUDIDX_MAX - 1) / (int32_t)AVI_AUDIDX_MAX;
        if (aud_step < 1) {
            aud_step = 1;
        }
        {
            size_t cap = (size_t)a_count > (size_t)AVI_AUDIDX_MAX ? (size_t)AVI_AUDIDX_MAX
                                                                  : (size_t)a_count;
            avi->audidx = (avi_audidx_t *)sgl_malloc(sizeof(avi_audidx_t) * cap);
        }
    }

    /* pass 2: fill the tables */
    off = pos + 8;
    remaining = n;
    while (remaining > 0) {
        int32_t take = remaining > 32 ? 32 : remaining;

        if (avi_pread(avi->fd, blk, off, take * 16) != take * 16)
            break;
        for (i = 0; i < take; i++) {
            const uint8_t *e = blk + i * 16;
            uint32_t ck = avi_rd32(e);
            int32_t ofs = (int32_t)avi_rd32(e + 8);
            int32_t csz = (int32_t)avi_rd32(e + 12);

            if (ck == avi->vflag && v_seen < v_count) {
                if ((v_seen % v_step) == 0 && filled < v_cap) {
                    avi->vidx[filled].offset = base + ofs;
                    avi->vidx[filled].size = csz;
                    filled++;
                }
                v_seen++;
            } else if (avi->aflag != 0 && ck == avi->aflag) {
                if (avi->audidx != NULL && (aud_seen % aud_step) == 0 &&
                    avi->audidx_count < (int32_t)AVI_AUDIDX_MAX) {
                    avi->audidx[avi->audidx_count].offset = base + ofs;
                    avi->audidx[avi->audidx_count].cstart = aud_pos;
                    avi->audidx_count++;
                }
                aud_seen++;
                aud_pos += csz;
            }
        }
        remaining -= take;
        off += take * 16;
    }

    avi->vidx_count = filled;
    avi->vidx_total = v_count;
    avi->vidx_step = (filled > 0) ? v_step : 0;
    avi->aud_bytes_total = aud_pos;
}

/**
 * @brief muxers disagree on the idx1 offset convention (chunk data, chunk
 *        header or movi list start). Probe the first frame for the JPEG
 *        SOI marker and rebase every index entry to the data convention.
 */
static void avi_probe_offsets(sgl_avi_t *avi)
{
    static const int32_t delta_tab[4] = {0, -8, 4, 8};
    uint8_t magic[2];
    int32_t i, d = 0;

    if (avi->vidx_count <= 0)
        return;

    for (i = 0; i < 4; i++) {
        d = delta_tab[i];
        if (avi_pread(avi->fd, magic, avi->vidx[0].offset + d, 2) == 2 && magic[0] == 0xFF &&
            magic[1] == 0xD8) {
            break;
        }
        d = 0;
    }
    if (d == 0)
        return; /* standard convention, nothing to fix */

    for (i = 0; i < avi->vidx_count; i++) {
        avi->vidx[i].offset += d;
    }
    for (i = 0; i < avi->audidx_count; i++) {
        avi->audidx[i].offset += d;
    }
}

/**
 * @brief walk the top level RIFF chunks: parse hdrl, locate movi,
 *        then build the indexes
 * @return 0 on success, -1 on a malformed file
 */
static int avi_parse_header(sgl_avi_t *avi)
{
    uint8_t hdr[8];
    uint8_t sub[4];
    uint32_t id, size, riff_size;
    int32_t pos, limit;

    if (avi_pread(avi->fd, hdr, 0, 12) != 12) {
        SGL_LOG_ERROR("avi: header read failed");
        return -1;
    }
    if (avi_rd32(hdr) != CC_RIFF || avi_rd32(hdr + 8) != CC_AVIF) {
        /* print the actual signature: 0xE0FFD8FF = raw JPEG stream (the
         * old .mjpeg format renamed to .avi), "ftyp" = MP4 container */
        SGL_LOG_ERROR("avi: not RIFF/AVI, got '%c%c%c%c' '%c%c%c%c' (%x %x)", hdr[0], hdr[1],
                      hdr[2], hdr[3], hdr[8], hdr[9], hdr[10], hdr[11], avi_rd32(hdr),
                      avi_rd32(hdr + 8));
        return -1;
    }

    riff_size = avi_rd32(hdr + 4);
    limit = avi->file_size;
    if (riff_size >= 8u && (int64_t)riff_size + 8 <= (int64_t)avi->file_size) {
        limit = (int32_t)riff_size + 8;
    }

    pos = 12;
    id = 0;
    while (pos + 8 <= limit) {
        if (avi_pread(avi->fd, hdr, pos, 8) != 8)
            break;
        id = avi_rd32(hdr);
        size = avi_rd32(hdr + 4);
        /* top level chunks must stay inside the file (movi lists of long
         * videos easily exceed a fixed cap, so only range matters) */
        if (size >= (uint32_t)avi->file_size) {
            SGL_LOG_ERROR("avi: chunk %x bad size %x at %x", id, size, pos);
            break;
        }
        if (pos + 8 + (int32_t)size > limit) {
            SGL_LOG_ERROR("avi: chunk %x overruns file at %x", id, pos);
            break;
        }

        if (id == CC_LIST && size >= 8) {
            if (avi_pread(avi->fd, sub, pos + 8, 4) != 4)
                break;

            if (avi_rd32(sub) == CC_HDRL) {
                avi_parse_hdrl(avi, pos + 12, pos + 8 + (int32_t)size);
            } else if (avi_rd32(sub) == CC_MOVI) {
                avi->movi_first = pos + 12;              /* first chunk header */
                avi->movi_end = pos + 8 + (int32_t)size; /* end of the list */
                break;
            }
        }
        pos += 8 + (int32_t)size + (int32_t)(size & 1);
    }

    if (avi->movi_first < 0 || avi->movi_first >= avi->movi_end) {
        SGL_LOG_ERROR("avi: movi LIST not found (stopped at %x id %x)", pos, id);
        return -1;
    }

    /* idx1 offsets are relative to the "movi" fourcc (pos + 8 above) */
    avi_scan_idx(avi, avi->movi_first - 4);
    avi_probe_offsets(avi);

    /* extend movi_end to cover the last indexed frame, then clamp */
    if (avi->vidx_count > 0) {
        int32_t last = avi->vidx_count - 1;
        int32_t end = avi->vidx[last].offset + avi->vidx[last].size;

        end += (end & 1);
        if (end > avi->movi_end) {
            avi->movi_end = end;
        }
    }
    if (avi->movi_end > avi->file_size) {
        avi->movi_end = avi->file_size;
    }
    return 0;
}

/**
 * @brief make sure the pixmap buffer fits a w x h frame
 * @return 0 on success, -1 on allocation failure
 */
#define AVI_OUTPUT_BPP ((MJDEC_FORMAT == 1) ? 2 : 3)

static int avi_ensure_pixbuf(sgl_avi_t *avi, int32_t w, int32_t h)
{
    int32_t need = w * h;

    if (w <= 0 || h <= 0) {
        return -1;
    }
    if (avi->pixbuf != NULL && avi->pixbuf_size >= need) {
        avi->pix_w = (int16_t)w;
        avi->pix_h = (int16_t)h;
        return 0;
    }
    if (avi->pixbuf != NULL) {
        sgl_free(avi->pixbuf);
        avi->pixbuf = NULL;
        avi->pixbuf_size = 0;
    }
    avi->pixbuf = (uint8_t *)sgl_malloc((size_t)need * AVI_OUTPUT_BPP);
    if (avi->pixbuf == NULL) {
        SGL_LOG_ERROR("avi: pixmap alloc failed");
        return -1;
    }
    avi->pixbuf_size = need;
    avi->pix_w = (int16_t)w;
    avi->pix_h = (int16_t)h;
    return 0;
}

/*--------------------------------------------------------------------------*/
/* Frame decoding                                                           */
/*--------------------------------------------------------------------------*/

/**
 * @brief output callback: copy one decoded MCU rect into the RGB565 pixmap
 * @return nonzero to continue decoding; zero aborts the decode
 */
static int avi_out_func(MJDEC *jd, void *bitmap, MJRECT *rect)
{
    sgl_avi_t *avi = g_vstream.avi;
    const uint8_t *src = (const uint8_t *)bitmap;
    uint8_t *dst;
    int32_t rw = (int32_t)(rect->right - rect->left + 1);
    int32_t y, rows;

    if (avi == NULL || avi->pixbuf == NULL) {
        return 0; /* abort decode on missing pixmap */
    }

    dst = avi->pixbuf + ((int32_t)rect->top * avi->pix_w + rect->left) * AVI_OUTPUT_BPP;
    rows = (int32_t)rect->bottom - (int32_t)rect->top + 1;

    for (y = 0; y < rows; y++) {
        memcpy(dst, src, (size_t)rw * AVI_OUTPUT_BPP);
        src += (size_t)rw * AVI_OUTPUT_BPP;
        dst += (size_t)avi->pix_w * AVI_OUTPUT_BPP;
    }
    return 1; /* continue */
}

/**
 * @brief input callback over the RAM staging buffer. A NULL buff asks the
 *        decoder how many bytes remain, the stream is then simply skipped
 */
static size_t avi_vin_func(MJDEC *jd, uint8_t *buff, size_t ndata)
{
    avi_vstream_t *vs = (avi_vstream_t *)jd->device;
    int32_t n;

    if ((int32_t)ndata > vs->left) {
        n = vs->left;
    } else {
        n = (int32_t)ndata;
    }
    if (n <= 0) {
        return 0;
    }
    if (buff != NULL) {
        memcpy(buff, vs->data, (size_t)n);
    }
    vs->data += n;
    vs->left -= n;
    return (size_t)n;
}

/* streaming input straight from the file, used for oversized chunks */
static size_t avi_sin_func(MJDEC *jd, uint8_t *buff, size_t ndata)
{
    sgl_avi_t *avi = (sgl_avi_t *)jd->device;
    int32_t got;

    if (buff == NULL) {
        /* forward the stream position without touching the buffer */
        sgl_fs_seek(avi->fd, (int32_t)ndata, SGL_SEEK_CUR);
        return ndata;
    }
    got = (int32_t)sgl_fs_read(avi->fd, buff, (uint32_t)ndata);
    return got > 0 ? (size_t)got : 0;
}

/**
 * @brief decode the MJPEG chunk at offset. Chunks fitting the staging
 *        buffer are decoded fully from RAM, larger ones stream from the
 *        file. The descale factor is chosen so the pixmap budget holds
 * @return 0 on success, -1 on any failure
 */
static int avi_decode_chunk(sgl_avi_t *avi, int32_t offset, int32_t size)
{
    MJDEC jd;
    MJRESULT rc;
    int32_t read_len = 0;

    if (size < (int32_t)AVI_MIN_FRAME_SIZE || offset < 0 || offset + size > avi->file_size) {
        return -1;
    }

    /* decoder pool is allocated once and reused for every frame */
    if (avi->jd_pool == NULL) {
        avi->jd_pool = (uint8_t *)sgl_malloc(SGL_AVI_JDEC_POOL_SIZE);
        if (avi->jd_pool == NULL) {
            SGL_LOG_ERROR("avi: jd pool alloc failed");
            return -1;
        }
    }

    if (avi->vbuf != NULL && size <= avi->vbuf_size) {
        /* fast path: bulk read the chunk, decode from RAM */
        read_len = avi_pread(avi->fd, avi->vbuf, offset, size);
        if (read_len <= 0) {
            return -1;
        }
        g_vstream.avi = avi;
        g_vstream.data = avi->vbuf;
        g_vstream.left = read_len;

        rc = mj_prepare(&jd, avi_vin_func, avi->jd_pool, SGL_AVI_JDEC_POOL_SIZE, &g_vstream);
    } else {
        /* oversized chunk: stream it straight from the file */
        if (sgl_fs_seek(avi->fd, offset, SGL_SEEK_SET) < 0) {
            return -1;
        }
        g_vstream.avi = avi;
        g_vstream.data = NULL;
        g_vstream.left = 0;

        rc = mj_prepare(&jd, avi_sin_func, avi->jd_pool, SGL_AVI_JDEC_POOL_SIZE, avi);
    }
    if (rc != MJDR_OK) {
        return -1;
    }

    /* pick the largest descale factor that fits the pixmap budget */
    {
        int32_t w = (int32_t)jd.width;
        int32_t h = (int32_t)jd.height;
        uint8_t scale = avi->decode_scale;

        while (scale < 3 &&
               (int64_t)(w >> scale) * (h >> scale) * AVI_OUTPUT_BPP >
                   (int64_t)SGL_AVI_PIXMAP_MAX) {
            scale++;
        }
        if (avi_ensure_pixbuf(avi, w >> scale, h >> scale) != 0) {
            return -1;
        }
        avi->jd_scale = scale;
    }

    rc = mj_decomp(&jd, avi_out_func, avi->jd_scale);
    return rc == MJDR_OK ? 0 : -1;
}

/**
 * @brief decode the next frame, using the index when available
 * @return 0 on success, -1 at stream end or on a decode error
 */
static int avi_decode_next(sgl_avi_t *avi)
{
    int32_t frame = avi->next_frame;

    if (avi->vidx_count > 0 && avi->vidx_step <= 1) {
        /* dense index: every MJPEG frame is a keyframe, decode directly */
        if (frame >= avi->vidx_count) {
            return -1;
        }
        if (avi_decode_chunk(avi, avi->vidx[frame].offset, avi->vidx[frame].size) != 0) {
            return -1;
        }
    } else {
        /* guarded forward walk over the movi chunks: no idx1, or a sampled
         * index where decoding every frame keeps the full frame rate while
         * the index footprint stays bounded */
        int32_t guard = 0;

        if (avi->v_pos < 0) {
            return -1;
        }
        while (guard++ < AVI_WALK_CHUNK_GUARD) {
            uint8_t hdr[8];

            if (avi->v_size != 0) {
                /* past the current chunk data, honor the pad byte */
                avi->v_pos += 8 + avi->v_size + (avi->v_size & 1);
                avi->v_size = 0;
                avi->v_id = CC_ID_NONE;
            }
            if (avi->v_pos + 8 > avi->movi_end) {
                return -1; /* stream end */
            }
            if (avi_pread(avi->fd, hdr, avi->v_pos, 8) != 8) {
                return -1;
            }
            avi->v_id = avi_rd32(hdr);
            avi->v_size = (int32_t)avi_rd32(hdr + 4);
            if (avi->v_size < 0 || avi->v_size > avi->file_size ||
                avi->v_pos + 8 + avi->v_size > avi->movi_end) {
                return -1;
            }
            if (avi->v_id == avi->vflag) {
                if (avi_decode_chunk(avi, avi->v_pos + 8, avi->v_size) != 0) {
                    return -1;
                }
                break;
            }
            /* skip the non video chunk */
            avi->v_pos += 8 + avi->v_size + (avi->v_size & 1);
            avi->v_size = 0;
            avi->v_id = CC_ID_NONE;
        }
        if (avi->v_id != avi->vflag) {
            return -1;
        }
    }

    avi->has_frame = 1;
    avi->show_frame = frame;
    avi->next_frame = frame + 1;
    return 0;
}

/*--------------------------------------------------------------------------*/
/* Audio pump                                                               */
/*--------------------------------------------------------------------------*/

/**
 * @brief feed the audio device from the shared file descriptor. Walks the
 *        movi chunks forward, refilling the device buffer until full
 */
static void avi_audio_pump(sgl_avi_t *avi)
{
    int32_t want, got, written, aligned;
    uint32_t ring_free, ring_contiguous, port_contiguous;
    int progress;

    if (!avi->audio || !g_audio_port_ready || avi->fd < 0 || avi->audio_align == 0 ||
        avi->audio_buf == NULL || avi->audio_buf_capacity == 0) {
        return;
    }

    for (int32_t pass = 0; pass < AVI_PUMP_CHUNK_GUARD; pass++) {
        progress = 0;

        if (avi->audio_buf_count < avi->audio_buf_capacity && avi->a_size == 0) {
            /* advance to the next audio chunk */
            int32_t guard = 0;
            uint8_t hdr[8];
            int32_t pos = avi->a_pos;
            int found = 0;

            while (guard++ < AVI_PUMP_CHUNK_GUARD) {
                uint32_t id;
                int32_t size;

                if (pos < 0 || pos + 8 > avi->movi_end) {
                    break;
                }
                if (avi_pread(avi->fd, hdr, pos, 8) != 8) {
                    break;
                }
                id = avi_rd32(hdr);
                size = (int32_t)avi_rd32(hdr + 4);
                if (size < 0 || size > avi->file_size || pos + 8 + size > avi->movi_end) {
                    break;
                }
                if (id == avi->aflag) {
                    avi->aud_off = pos + 8;
                    avi->aud_left = size;
                    avi->a_next = pos + 8 + size + (size & 1);
                    found = 1;
                    break;
                }
                pos += 8 + size + (size & 1);
            }
            if (found) {
                avi->a_size = avi->aud_left;
            }
        }

        if (avi->a_size > 0 && avi->audio_buf_count < avi->audio_buf_capacity) {
            ring_free = avi->audio_buf_capacity - avi->audio_buf_count;
            ring_contiguous = avi->audio_buf_capacity - avi->audio_buf_head;
            want = avi->aud_left;
            if (want > (int32_t)ring_free)
                want = (int32_t)ring_free;
            if (want > (int32_t)ring_contiguous)
                want = (int32_t)ring_contiguous;
            if (want > SGL_AVI_AUDIO_CHUNK)
                want = SGL_AVI_AUDIO_CHUNK;
            want -= want % (int32_t)avi->audio_align;
            if (want > 0) {
                got = avi_pread(avi->fd, avi->audio_buf + avi->audio_buf_head, avi->aud_off, want);
                if (got > 0) {
                    aligned = got - (got % (int32_t)avi->audio_align);
                    if (aligned > 0) {
                        avi->audio_buf_head =
                            (avi->audio_buf_head + (uint32_t)aligned) % avi->audio_buf_capacity;
                        avi->audio_buf_count += (uint32_t)aligned;
                        avi->aud_off += aligned;
                        avi->aud_left -= aligned;
                        avi->a_size -= aligned;
                        progress = 1;
                    } else {
                        avi->aud_left = 0;
                        avi->a_size = 0;
                    }
                    if (avi->aud_left <= 0) {
                        avi->a_pos = avi->a_next;
                        avi->aud_left = 0;
                        avi->a_size = 0;
                        avi->a_id = CC_ID_NONE;
                    }
                }
            }
        }

        if (avi->audio_buf_count > 0) {
            port_contiguous = avi->audio_buf_capacity - avi->audio_buf_tail;
            want = (int32_t)avi->audio_buf_count;
            if (want > (int32_t)port_contiguous)
                want = (int32_t)port_contiguous;
            if (want > SGL_AVI_AUDIO_CHUNK)
                want = SGL_AVI_AUDIO_CHUNK;
            want -= want % (int32_t)avi->audio_align;
            if (want > 0) {
                written = g_audio_port.write(g_audio_port.user_data,
                                             avi->audio_buf + avi->audio_buf_tail, (uint32_t)want);
                if (written > want)
                    written = want;
                if (written > 0) {
                    aligned = written - (written % (int32_t)avi->audio_align);
                    if (aligned > 0) {
                        avi->audio_buf_tail =
                            (avi->audio_buf_tail + (uint32_t)aligned) % avi->audio_buf_capacity;
                        avi->audio_buf_count -= (uint32_t)aligned;
                        avi->audio_frame += (uint32_t)(aligned / (int32_t)avi->audio_align);
                        progress = 1;
                    }
                }
            }
        }

        if (!progress)
            break;
    }
}

/**
 * @brief initialize the audio hardware once, open the pcm stream
 * @return 0 on success, -1 when audio is not available
 * @note  the audio is fed by the playback animation: the 0->1000 linear
 *        ramp fires its path callback on every 1ms tick, and avi_cycle
 *        pumps the audio at its top, so no separate timer is needed
 */
static int avi_audio_start(sgl_avi_t *avi)
{
    if (!avi->audio || avi->audio_align == 0 || avi->audio_rate == 0) {
        return -1;
    }
    if (!g_audio_port_ready) {
        SGL_LOG_WARN("avi: no audio port registered; playing video only");
        avi->audio = 0;
        return -1;
    }
    if (g_audio_port.start(g_audio_port.user_data, avi->audio_rate, avi->audio_channels,
                           avi->audio_bits) != 0) {
        SGL_LOG_ERROR("avi: audio port start failed");
        avi->audio = 0; /* device refused, play silent */
        return -1;
    }
    /* consumed counter restarted at zero, rebase the clock so it
     * continues from the current sample frame count (pause/resume) */
    avi->played_base =
        (int32_t)(avi->audio_frame * avi->audio_align) - (int32_t)avi_audio_consumed_bytes();
    return 0;
}

/**
 * @brief stop the audio stream
 */
static void avi_audio_stop(sgl_avi_t *avi)
{
    if (g_audio_port_ready) {
        g_audio_port.stop(g_audio_port.user_data);
    }
}

/**
 * @brief playback position in ms derived from the consumed-byte counter
 */
static int32_t avi_audio_played_ms(sgl_avi_t *avi)
{
    int64_t total;

    if (!avi->audio || avi->audio_align == 0 || avi->audio_rate == 0) {
        return 0;
    }
    total = (int64_t)(avi->played_base + (int32_t)avi_audio_consumed_bytes()) * 1000;
    return (int32_t)(total / ((int64_t)avi->audio_rate * avi->audio_align));
}

/**
 * @brief unified presentation clock. Audio files follow the consumed-byte
 *        counter, silent files follow the sgl tick with pause accumulation
 */
static int32_t avi_played_ms(sgl_avi_t *avi)
{
    if (avi->audio) {
        return avi_audio_played_ms(avi);
    }
    if (avi->state != SGL_AVI_STATE_PLAYING) {
        return (int32_t)avi->play_ms_accum;
    }
    return (int32_t)(avi->play_ms_accum + (sgl_tick_get() - avi->play_tick_base));
}

/**
 * @brief timestamp of a frame slot in milliseconds, using the exact frame
 *        period (66.67ms for 15fps) instead of the truncated period_ms
 *        (66ms), which would drift ~0.6s per minute ahead of the audio
 * @param avi pointer to the player
 * @param frame zero-based frame index
 * @return slot time in milliseconds, rounded down
 */
static int32_t avi_frame_slot_ms(const sgl_avi_t *avi, int32_t frame)
{
    return (int32_t)(((int64_t)frame * avi->period_us) / 1000);
}

/* frames per index entry, 1 when the index is dense or absent */
static int32_t avi_vidx_stride(const sgl_avi_t *avi)
{
    return avi->vidx_step > 1 ? avi->vidx_step : 1;
}

/* total video frame count; index entries are sampled when the idx1 is
 * sparse, so the entry count alone is not the frame count */
static int32_t avi_vidx_total(const sgl_avi_t *avi)
{
    if (avi->vidx_count > 0) {
        /* v_count is exact; the last sampled index need not be the final frame. */
        return avi->vidx_total > 0 ? avi->vidx_total : avi->frames_total;
    }
    return avi->frames_total;
}

/**
 * @brief advance the video walk cursor by count video chunks (skipping
 *        audio and any other chunks). Used after a seek onto a sampled
 *        index entry to reach the exact requested frame
 * @return 0 on success, -1 when the stream ends before the target
 */
static int avi_walk_video_skip(sgl_avi_t *avi, int32_t count)
{
    int32_t pos = avi->v_pos;
    int32_t guard = 0;

    while (count > 0 && guard++ < AVI_VSEEK_WALK_GUARD) {
        uint8_t hdr[8];
        uint32_t id;
        int32_t size;

        if (pos < 0 || pos + 8 > avi->movi_end || avi_pread(avi->fd, hdr, pos, 8) != 8) {
            return -1;
        }
        id = avi_rd32(hdr);
        size = (int32_t)avi_rd32(hdr + 4);
        if (size < 0 || size > avi->file_size || pos + 8 + size > avi->movi_end) {
            return -1;
        }
        if (id != avi->vflag) {
            pos += 8 + size + (size & 1);
            continue;
        }
        /* video chunk: count it, stop with the cursor on the target one */
        if (--count == 0) {
            break;
        }
        pos += 8 + size + (size & 1);
    }
    if (count > 0) {
        return -1;
    }
    avi->v_pos = pos;
    return 0;
}

/**
 * @brief restart the animation timer at the current frame period
 */
static void avi_anim_restart(sgl_avi_t *avi);

/**
 * @brief one playback step: feed audio, decode ahead, present on time
 */
static void avi_cycle(sgl_avi_t *avi)
{
    if (avi == NULL || avi->state != SGL_AVI_STATE_PLAYING) {
        return;
    }

    /* keep the audio device topped up */
    if (avi->audio) {
        avi_audio_pump(avi);
    }

    if (!avi->frame_pending) {
        int32_t played_ms = avi_played_ms(avi);
        int32_t slot = avi_frame_slot_ms(avi, avi->next_frame);
        int need = 1;

        /* keep exactly one frame of decode-ahead over the play clock. Once
         * the audio index is drained there is nothing left to wait for */
        if (avi->audio) {
            int32_t drained =
                (avi->a_pos >= avi->movi_end) && avi->aud_left <= 0 && avi->audio_buf_count == 0;
            if (slot > played_ms + (int32_t)avi->period_ms && !drained) {
                need = 0;
            }
        }

        /* stall watchdog: a stalled audio clock must not freeze the video */
        {
            uint32_t now = sgl_tick_get();
            uint32_t consumed = avi->audio ? avi_audio_consumed_bytes() : now;
            if (consumed != avi->last_played) {
                avi->last_played = consumed;
                avi->last_adv_tick = now;
            } else if (now - avi->last_adv_tick > AVI_CLOCK_STALL_MS) {
                need = 1;
            }
        }

        if (need && avi_decode_next(avi) != 0) {
            /* stream end */
            if (avi->loop) {
                avi->v_pos = avi->movi_first;
                avi->a_pos = avi->movi_first;
                avi->a_next = avi->movi_first;
                avi->v_size = 0;
                avi->v_id = CC_ID_NONE;
                avi->a_size = 0;
                avi->a_id = CC_ID_NONE;
                avi->aud_off = 0;
                avi->aud_left = 0;
                avi->audio_frame = 0;
                avi_audio_ring_reset(avi);
                if (g_audio_port_ready) {
                    g_audio_port.flush(g_audio_port.user_data);
                }
                avi->next_frame = 0;
                avi->show_frame = AVI_POS_NONE;
                /* clock keeps counting, rewind the base under it */
                avi->played_base = (int32_t)(0U - avi_audio_consumed_bytes());
                return;
            }
            avi->state = SGL_AVI_STATE_STOPPED;
            avi_audio_stop(avi);
            sgl_anim_delete_by_obj(&avi->obj);
            return;
        }
        avi->frame_pending = (need != 0);
    }

    /* present the pending frame when its slot is due (one period early
     * already includes the decode latency) */
    if (avi->frame_pending) {
        int32_t played_ms = avi_played_ms(avi);
        int32_t slot = avi_frame_slot_ms(avi, avi->show_frame);
        int present = 1;

        if (played_ms + (int32_t)avi->period_ms < slot) {
            present = 0;
        }

        if (present) {
            sgl_area_t r;

            avi->frame_pending = 0;
            if (avi->pixbuf != NULL && avi->pix_w > 0 && avi->pix_h > 0) {
                r.x1 = avi->obj.coords.x1 + ((sgl_obj_get_width(&avi->obj) - avi->pix_w) / 2);
                r.y1 = avi->obj.coords.y1 + ((sgl_obj_get_height(&avi->obj) - avi->pix_h) / 2);
                r.x2 = r.x1 + avi->pix_w - 1;
                r.y2 = r.y1 + avi->pix_h - 1;
                sgl_update_area(&r);
            } else {
                sgl_obj_set_dirty(&avi->obj);
            }
        }
    }
}

/**
 * @brief animation path callback, the timer hook used to run avi_cycle
 */
static void avi_anim_path_cb(sgl_anim_t *anim, int32_t value)
{
    sgl_obj_t *obj = (sgl_obj_t *)anim->data;
    sgl_avi_t *avi = sgl_container_of(obj, sgl_avi_t, obj);

    (void)value;
    avi_cycle(avi);
}

/**
 * @brief (re)arm the frame timer, replacing any running animation.
 *        The path cb fires on every 1ms tick of the 0->1000 ramp, which
 *       drives avi_cycle far faster than any frame period we use
 */
static void avi_anim_restart(sgl_avi_t *avi)
{
    sgl_anim_delete_by_obj(&avi->obj);
    sgl_anim_setup_obj_action_loop(&avi->obj, 0, 1000, avi->period_ms, avi_anim_path_cb,
                                   SGL_ANIM_PATH_LINEAR, NULL);
}

/*--------------------------------------------------------------------------*/
/* Widget drawing                                                           */
/*--------------------------------------------------------------------------*/

/**
 * @brief blit the decoded pixmap into the surface, centered in the widget.
 *        Rows or columns outside the pixmap are painted black
 */
static void avi_blit(sgl_avi_t *avi, sgl_surf_t *surf, sgl_area_t *clip)
{
    int16_t ox = (int16_t)(avi->obj.coords.x1 + ((sgl_obj_get_width(&avi->obj) - avi->pix_w) / 2));
    int16_t oy = (int16_t)(avi->obj.coords.y1 + ((sgl_obj_get_height(&avi->obj) - avi->pix_h) / 2));
    sgl_color_t *buf;
    int32_t y;

    for (y = clip->y1; y <= clip->y2; y++) {
        buf = sgl_surf_get_buf(surf, clip->x1 - surf->x1, y - surf->y1);

        if (y < oy || y >= oy + avi->pix_h) {
            /* fully outside the pixmap */
            sgl_color_set(buf, SGL_COLOR_BLACK, (uint32_t)(clip->x2 - clip->x1 + 1));
        } else {
            int32_t sx1 = clip->x1 > ox ? clip->x1 : ox;
            int32_t sx2 = (clip->x2 < ox + avi->pix_w - 1) ? clip->x2 : ox + avi->pix_w - 1;

            if (sx1 > clip->x1) {
                sgl_color_set(buf, SGL_COLOR_BLACK, (uint32_t)(sx1 - clip->x1));
            }
            if (sx1 <= sx2) {
                const uint8_t *src = avi->pixbuf +
                                     ((int32_t)(y - oy) * avi->pix_w + (sx1 - ox)) * AVI_OUTPUT_BPP;
#if MJDEC_FORMAT == 1
                memcpy(buf + (sx1 - clip->x1), src,
                       (size_t)(sx2 - sx1 + 1) * sizeof(sgl_color_t));
#else
                int32_t x;
                sgl_color_t *dst = buf + (sx1 - clip->x1);
                for (x = sx1; x <= sx2; x++) {
                    dst->full[0] = src[2]; /* SGL RGB888 memory order is B,G,R. */
                    dst->full[1] = src[1];
                    dst->full[2] = src[0];
                    dst++;
                    src += 3;
                }
#endif
            }
            if (sx2 < clip->x2) {
                sgl_color_set(buf + (sx2 - clip->x1 + 1), SGL_COLOR_BLACK,
                              (uint32_t)(clip->x2 - sx2));
            }
        }
    }
}

/**
 * @brief widget construct callback
 */
static void sgl_avi_construct_cb(sgl_surf_t *surf, sgl_obj_t *obj, sgl_event_t *evt)
{
    sgl_avi_t *avi = sgl_container_of(obj, sgl_avi_t, obj);
    sgl_area_t clip = SGL_AREA_INVALID;
    sgl_color_t *buf;
    int32_t y;

    if (evt->type == SGL_EVENT_DRAW_MAIN) {
        /* safety net: if the playback animation is ever lost while
         * playing (deleted by a core cleanup path or any other bug),
         * re-arm it so playback resumes instead of freezing. Draw
         * events keep coming because every presented frame invalidates
         * the frame area */
        if (avi->state == SGL_AVI_STATE_PLAYING && sgl_anim_get_by_obj(obj) == NULL) {
            avi_anim_restart(avi);
        }
        if (!sgl_surf_clip(surf, &obj->coords, &clip)) {
            return;
        }
        if (avi->has_frame && avi->pixbuf != NULL && avi->pix_w > 0 && avi->pix_h > 0) {
            avi_blit(avi, surf, &clip);
        } else {
            /* no frame yet: paint the clip black */
            for (y = clip.y1; y <= clip.y2; y++) {
                buf = sgl_surf_get_buf(surf, clip.x1 - surf->x1, y - surf->y1);
                sgl_color_set(buf, SGL_COLOR_BLACK, (uint32_t)(clip.x2 - clip.x1 + 1));
                buf += surf->w;
            }
        }
    } else if (evt->type == SGL_EVENT_DESTROYED) {
        avi->state = SGL_AVI_STATE_STOPPED;
        sgl_anim_delete_by_obj(obj);
        avi_audio_stop(avi);

        if (avi->fd >= 0) {
            sgl_fs_close(avi->fd);
            avi->fd = -1;
        }
        if (avi->path != NULL) {
            sgl_free(avi->path);
            avi->path = NULL;
        }
        if (avi->pixbuf != NULL) {
            sgl_free(avi->pixbuf);
            avi->pixbuf = NULL;
            avi->pixbuf_size = 0;
        }
        if (avi->vbuf != NULL) {
            sgl_free(avi->vbuf);
            avi->vbuf = NULL;
            avi->vbuf_size = 0;
        }
        if (avi->jd_pool != NULL) {
            sgl_free(avi->jd_pool);
            avi->jd_pool = NULL;
        }
        if (avi->audio_buf != NULL) {
            sgl_free(avi->audio_buf);
            avi->audio_buf = NULL;
            avi->audio_buf_capacity = 0;
        }
        if (avi->vidx != NULL) {
            sgl_free(avi->vidx);
            avi->vidx = NULL;
        }
        if (avi->audidx != NULL) {
            sgl_free(avi->audidx);
            avi->audidx = NULL;
        }
        avi->vidx_count = 0;
        avi->vidx_step = 0;
        avi->audidx_count = 0;
    }
}

/**
 * @brief close the open file and drop all parsed state
 */
static void avi_release_file(sgl_avi_t *avi)
{
    avi->state = SGL_AVI_STATE_STOPPED;
    sgl_anim_delete_by_obj(&avi->obj);
    avi_audio_stop(avi);

    if (avi->fd >= 0) {
        sgl_fs_close(avi->fd);
        avi->fd = -1;
    }
    if (avi->path != NULL) {
        sgl_free(avi->path);
        avi->path = NULL;
    }
    if (avi->vidx != NULL) {
        sgl_free(avi->vidx);
        avi->vidx = NULL;
    }
    if (avi->audidx != NULL) {
        sgl_free(avi->audidx);
        avi->audidx = NULL;
    }
    if (avi->audio_buf != NULL) {
        sgl_free(avi->audio_buf);
        avi->audio_buf = NULL;
        avi->audio_buf_capacity = 0;
    }
    avi_audio_ring_reset(avi);
    avi->vidx_count = 0;
    avi->vidx_step = 0;
    avi->vidx_total = 0;
    avi->audidx_count = 0;
    avi->has_frame = 0;
    avi->show_frame = AVI_POS_NONE;
    avi->next_frame = 0;
    avi->file_size = 0;
    avi->movi_first = -1;
    avi->movi_end = 0;
    avi->frames_total = 0;
}

/*--------------------------------------------------------------------------*/
/* Public API                                                               */
/*--------------------------------------------------------------------------*/

/**
 * @brief create an AVI player object
 * @param parent parent object; NULL creates the player on the active screen
 * @return pointer to the player object, or NULL if allocation fails
 */
sgl_obj_t *sgl_avi_create(sgl_obj_t *parent)
{
    sgl_avi_t *avi = (sgl_avi_t *)sgl_malloc(sizeof(sgl_avi_t));
    if (avi == NULL) {
        SGL_LOG_ERROR("sgl_avi_create: malloc failed");
        return NULL;
    }
    memset(avi, 0, sizeof(sgl_avi_t));

    if (sgl_obj_init(&avi->obj, parent) != 0) {
        sgl_free(avi);
        return NULL;
    }
    avi->obj.construct_fn = sgl_avi_construct_cb;

    avi->fd = -1;
    avi->fps = (uint8_t)AVI_DEFAULT_FPS;
    avi->period_ms = (uint16_t)(1000 / AVI_DEFAULT_FPS);
    avi->period_us = 1000000u / AVI_DEFAULT_FPS;
    avi->vflag = CC_00DC;
    avi->aflag = CC_01WB;
    avi->movi_first = -1;
    avi->show_frame = AVI_POS_NONE;
    avi->play_tick_base = sgl_tick_get();
    avi->play_ms_accum = 0;
    return &avi->obj;
}

/**
 * @brief register the platform PCM output callbacks
 * @param port callback table to register; NULL unregisters the current port
 * @return 0 on success, or -1 if a required callback is missing
 * @note register before sgl_avi_play(); video can play without an audio port
 */
int sgl_avi_set_audio_port(const sgl_avi_audio_port_t *port)
{
    if (port == NULL) {
        memset(&g_audio_port, 0, sizeof(g_audio_port));
        g_audio_port_ready = 0;
        return 0;
    }
    if (port->start == NULL || port->stop == NULL || port->write == NULL ||
        port->get_consumed_bytes == NULL || port->flush == NULL) {
        return -1;
    }

    g_audio_port = *port;
    g_audio_port_ready = 1;
    return 0;
}

/**
 * @brief open an AVI file and parse its stream headers and indexes
 * @param obj AVI player object
 * @param path file path accessible through the mounted SGL filesystem
 * @return 0 on success, or -1 if the file cannot be opened or parsed
 * @note supports MJPEG video and optional uncompressed PCM audio
 */
int sgl_avi_load_file(sgl_obj_t *obj, const char *path)
{
    sgl_avi_t *avi;
    sgl_stat_t st;
    char *copy;
    size_t len;
    int fd;

    if (obj == NULL || path == NULL) {
        return -1;
    }
    avi = sgl_container_of(obj, sgl_avi_t, obj);

    fd = sgl_fs_open(path, SGL_O_RDONLY);
    if (fd < 0) {
        SGL_LOG_ERROR("avi: open failed");
        return -1;
    }
    if (sgl_fs_stat(path, &st) != 0 || st.st_size < 64 ||
        (uint64_t)st.st_size > (uint64_t)INT32_MAX) {
        sgl_fs_close(fd);
        SGL_LOG_ERROR("avi: stat failed or file too small");
        return -1;
    }

    /* drop any previously loaded file */
    avi_release_file(avi);
    avi->fd = fd;
    avi->file_size = (int32_t)st.st_size;

    len = strlen(path) + 1;
    copy = (char *)sgl_malloc(len);
    if (copy == NULL) {
        avi_release_file(avi);
        return -1;
    }
    memcpy(copy, path, len);
    avi->path = copy;

    /* default stream assumptions, corrected by the header walk */
    avi->audio = 0;
    avi->audio_channels = 1;
    avi->audio_bits = 16;
    avi->audio_align = 2;
    avi->audio_rate = 22050;
    avi->fps = (uint8_t)AVI_DEFAULT_FPS;
    avi->period_ms = (uint16_t)(1000 / AVI_DEFAULT_FPS);
    avi->period_us = 1000000u / AVI_DEFAULT_FPS;
    avi->movi_first = -1;
    avi->movi_end = avi->file_size;

    if (avi_parse_header(avi) != 0) {
        SGL_LOG_ERROR("avi: bad AVI header");
        avi_release_file(avi);
        return -1;
    }

    avi->period_ms = (uint16_t)(1000u / avi->fps);
    if (avi->period_ms == 0) {
        avi->period_ms = 1;
    }
    avi->period_us = 1000000u / avi->fps;

    if (avi->audio) {
        avi->audio_buf = (uint8_t *)sgl_malloc(SGL_AVI_AUDIO_BUFFER_SIZE);
        if (avi->audio_buf != NULL) {
            avi->audio_buf_capacity = SGL_AVI_AUDIO_BUFFER_SIZE;
            avi_audio_ring_reset(avi);
        } else {
            SGL_LOG_WARN("avi: PCM ring allocation failed; playing video only");
            avi->audio = 0;
        }
    }

    /* staging buffer for the RAM decode path */
    if (avi->vbuf == NULL) {
        avi->vbuf = (uint8_t *)sgl_malloc(SGL_AVI_VBUF_MAX);
        if (avi->vbuf != NULL) {
            avi->vbuf_size = SGL_AVI_VBUF_MAX;
        }
    }

    /* saturation table for the IDCT output */
    mj_clip8_init();

    /* position the cursors on the first chunk */
    avi->v_pos = avi->movi_first;
    avi->a_pos = avi->movi_first;
    avi->a_next = avi->movi_first;
    avi->v_size = 0;
    avi->v_id = CC_ID_NONE;
    avi->a_size = 0;
    avi->a_id = CC_ID_NONE;
    avi->aud_off = 0;
    avi->aud_left = 0;
    avi->audio_frame = 0;
    avi_audio_ring_reset(avi);
    avi->played_base = 0;
    avi->play_ms_accum = 0;
    avi->play_tick_base = sgl_tick_get();
    avi->next_frame = 0;
    avi->show_frame = AVI_POS_NONE;
    avi->has_frame = 0;
    avi->frame_pending = 0;

    SGL_LOG_INFO("avi: frames %d (idx %d x%d) fps %d audio %d size %d",
                 (int)avi_vidx_total(avi), (int)avi->vidx_count,
                 avi->vidx_count > 0 ? (int)avi_vidx_stride(avi) : 0, (int)avi->fps,
                 (int)avi->audio, (int)avi->file_size);
    return 0;
}

/**
 * @brief start or resume AVI audio and video playback
 * @param obj AVI player object
 * @return none
 * @note does nothing if no file is open or playback is already running
 */
void sgl_avi_play(sgl_obj_t *obj)
{
    sgl_avi_t *avi;

    if (obj == NULL)
        return;
    avi = sgl_container_of(obj, sgl_avi_t, obj);
    if (avi->fd < 0 || avi->state == SGL_AVI_STATE_PLAYING) {
        return;
    }

    if (avi->audio) {
        avi_audio_start(avi);
    }
    avi->state = SGL_AVI_STATE_PLAYING;
    avi->play_tick_base = sgl_tick_get();
    avi->last_played = avi->audio ? avi_audio_consumed_bytes() : avi->play_tick_base;
    avi->last_adv_tick = avi->play_tick_base;
    avi_anim_restart(avi);
}

/**
 * @brief pause playback and keep the current frame on screen
 * @param obj AVI player object
 * @return none
 */
void sgl_avi_pause(sgl_obj_t *obj)
{
    sgl_avi_t *avi;

    if (obj == NULL)
        return;
    avi = sgl_container_of(obj, sgl_avi_t, obj);
    if (avi->state != SGL_AVI_STATE_PLAYING) {
        return;
    }
    /* freeze the tick clock at the current playback position */
    avi->play_ms_accum += sgl_tick_get() - avi->play_tick_base;
    avi->state = SGL_AVI_STATE_PAUSED;
    sgl_anim_delete_by_obj(obj);
    avi_audio_stop(avi);
}

/**
 * @brief stop playback and rewind audio and video to the first frame
 * @param obj AVI player object
 * @return none
 */
void sgl_avi_stop(sgl_obj_t *obj)
{
    sgl_avi_t *avi;

    if (obj == NULL)
        return;
    avi = sgl_container_of(obj, sgl_avi_t, obj);
    if (avi->fd < 0) {
        return;
    }

    avi->state = SGL_AVI_STATE_STOPPED;
    sgl_anim_delete_by_obj(obj);
    avi_audio_stop(avi);

    /* rewind all cursors */
    avi->v_pos = avi->movi_first;
    avi->a_pos = avi->movi_first;
    avi->a_next = avi->movi_first;
    avi->v_size = 0;
    avi->v_id = CC_ID_NONE;
    avi->a_size = 0;
    avi->a_id = CC_ID_NONE;
    avi->aud_off = 0;
    avi->aud_left = 0;
    avi->audio_frame = 0;
    avi_audio_ring_reset(avi);
    avi->played_base = 0;
    avi->play_ms_accum = 0;
    avi->next_frame = 0;
    avi->show_frame = AVI_POS_NONE;
    avi->frame_pending = 0;
}

/**
 * @brief override the frame rate read from the AVI header
 * @param obj AVI player object
 * @param fps playback rate in frames per second; clamped to 1..60
 * @return none
 */
void sgl_avi_set_fps(sgl_obj_t *obj, uint8_t fps)
{
    sgl_avi_t *avi;

    if (obj == NULL)
        return;
    avi = sgl_container_of(obj, sgl_avi_t, obj);
    if (fps < 1)
        fps = 1;
    if (fps > 60)
        fps = 60;

    avi->fps = fps;
    avi->period_ms = (uint16_t)(1000u / fps);
    if (avi->period_ms == 0) {
        avi->period_ms = 1;
    }
    avi->period_us = 1000000u / fps;
    if (avi->state == SGL_AVI_STATE_PLAYING) {
        avi_anim_restart(avi);
    }
}

/**
 * @brief set the minimum JPEG downscale factor
 * @param obj AVI player object
 * @param scale 0=full, 1=half, 2=quarter, or 3=eighth resolution
 * @return none
 * @note the decoder may downscale further to satisfy SGL_AVI_PIXMAP_MAX
 */
void sgl_avi_set_decode_scale(sgl_obj_t *obj, uint8_t scale)
{
    sgl_avi_t *avi;

    if (obj == NULL)
        return;
    avi = sgl_container_of(obj, sgl_avi_t, obj);
    avi->decode_scale = scale > 3 ? 3 : scale;
}

/**
 * @brief position the audio pump at an exact PCM byte position. Uses the
 *        (sparse) audio index as a starting point, then walks chunk
 *        headers forward until the chunk containing byte_pos is found.
 *        The first chunk is trimmed so feeding resumes exactly at byte_pos
 * @param avi pointer to the player
 * @param byte_pos absolute pcm byte position within the audio stream
 * @param skipp receives the number of leading bytes of the target chunk to
 *              discard before feeding (>= 0)
 * @return 0 on success, -1 when the position cannot be resolved
 */
static int avi_audio_locate(sgl_avi_t *avi, int64_t byte_pos, int32_t *skipp)
{
    int32_t start, pos;
    int64_t cstart;
    int32_t guard = 0;

    *skipp = 0;
    if (!avi->audio || byte_pos < 0) {
        return -1;
    }

    /* index shortcut: last indexed chunk starting at or before byte_pos.
     * The index is sparse, so byte_pos can still be past its last entry */
    start = avi->movi_first;
    cstart = 0;
    if (avi->audidx_count > 0) {
        int32_t lo = 0, hi = avi->audidx_count - 1, pick = -1;

        while (lo <= hi) {
            int32_t mid = (lo + hi) / 2;

            if ((int64_t)avi->audidx[mid].cstart <= byte_pos) {
                pick = mid;
                lo = mid + 1;
            } else {
                hi = mid - 1;
            }
        }
        if (pick >= 0) {
            start = avi->audidx[pick].offset - 8;
            cstart = avi->audidx[pick].cstart;
        }
    }

    /* forward walk over chunk headers to the chunk holding byte_pos */
    pos = start;
    while (pos >= 0 && pos + 8 <= avi->movi_end && guard++ < AVI_AUDSEEK_WALK_GUARD) {
        uint8_t hdr[8];
        uint32_t id;
        int32_t size;

        if (avi_pread(avi->fd, hdr, pos, 8) != 8) {
            return -1;
        }
        id = avi_rd32(hdr);
        size = (int32_t)avi_rd32(hdr + 4);
        if (size < 0 || size > avi->file_size || pos + 8 + size > avi->movi_end) {
            return -1;
        }
        if (id == avi->aflag) {
            if (cstart + (int64_t)size > byte_pos) {
                int32_t skip = (int32_t)(byte_pos - cstart);

                /* keep the feed sample-frame aligned */
                skip -= skip % (int32_t)avi->audio_align;
                avi->a_pos = pos;
                avi->a_next = pos + 8 + size + (size & 1);
                avi->aud_off = pos + 8 + skip;
                avi->aud_left = size - skip;
                avi->a_size = avi->aud_left;
                *skipp = skip;
                return 0;
            }
            cstart += size;
        }
        pos += 8 + size + (size & 1);
    }
    return -1;
}

/**
 * @brief seek to a video frame and align audio to the same playback position
 * @param obj AVI player object
 * @param frame_index zero-based frame index; values are clamped to the file
 * @return 0 on success, or -1 if no valid AVI file is open
 * @note MJPEG frames are independently decodable keyframes
 */
int sgl_avi_seek_frame(sgl_obj_t *obj, int32_t frame_index)
{
    sgl_avi_t *avi;
    int32_t frame = frame_index;
    int64_t byte_pos;
    uint32_t audio_frame;
    int32_t audio_skip = 0;

    if (obj == NULL)
        return -1;
    avi = sgl_container_of(obj, sgl_avi_t, obj);
    if (avi->fd < 0 || avi->movi_first < 0) {
        return -1;
    }
    if (frame < 0)
        frame = 0;
    if (frame >= avi_vidx_total(avi)) {
        frame = avi_vidx_total(avi) - 1;
    }

    if (avi->vidx_count > 0 && avi->vidx_step <= 1) {
        /* dense index: land exactly on the requested frame */
        avi->v_pos = avi->vidx[frame].offset - 8;
        avi->next_frame = frame;
    } else if (avi->vidx_count > 0) {
        /* sampled index: land on the nearest sampled frame at or before
         * the target, then walk the few chunks in between */
        int32_t stride = avi_vidx_stride(avi);
        int32_t entry = frame / stride;
        int32_t base;

        if (entry >= avi->vidx_count) {
            entry = avi->vidx_count - 1;
        }
        base = entry * stride;
        if (base >= avi->vidx_total) {
            base = avi->vidx_total - 1;
        }
        avi->v_pos = avi->vidx[entry].offset - 8;
        if (avi_walk_video_skip(avi, frame - base) != 0) {
            /* walk failed: settle on the sampled frame instead */
            avi->v_pos = avi->vidx[entry].offset - 8;
            frame = base;
        }
        avi->next_frame = frame;
    } else {
        /* no index: walkers resume from the top */
        avi->v_pos = avi->movi_first;
        avi->next_frame = 0;
    }

    /* audio: derive the byte position from the exact frame timestamp */
    audio_frame = (uint32_t)(((int64_t)frame * avi->period_us) * avi->audio_rate / 1000000);
    byte_pos = (int64_t)audio_frame * avi->audio_align;

    avi->v_size = 0;
    avi->v_id = CC_ID_NONE;
    avi->aud_off = 0;
    avi->aud_left = 0;
    avi->a_size = 0;
    avi->a_id = CC_ID_NONE;

    /* position the audio pump exactly on byte_pos (index + short walk);
     * a position past the audio stream end just drains the pump and the
     * clock keeps running silent instead of restarting from the top */
    if (avi->audio && avi->fd >= 0 && byte_pos > 0 &&
        avi_audio_locate(avi, byte_pos, &audio_skip) != 0) {
        audio_skip = 0;
        avi->a_pos = avi->movi_end;
        avi->a_next = avi->movi_end;
    }

    /* sample frames fed before the seek point keep the clock consistent */
    avi->audio_frame = audio_frame;
    avi_audio_ring_reset(avi);

    if (avi->state == SGL_AVI_STATE_PLAYING) {
        /* flush stale samples and rebase the clock without stopping */
        if (g_audio_port_ready) {
            g_audio_port.flush(g_audio_port.user_data);
        }
        avi->played_base =
            (int32_t)(audio_frame * avi->audio_align) - (int32_t)avi_audio_consumed_bytes();
        if (avi->audio) {
            avi_audio_pump(avi);
        }
    } else if (avi->audio && avi->fd >= 0) {
        /* prime a little audio so play() has data ready instantly */
        if (g_audio_port_ready) {
            g_audio_port.flush(g_audio_port.user_data);
        }
        avi->played_base = (int32_t)(audio_frame * avi->audio_align);
        avi_audio_pump(avi);
    } else {
        avi->played_base = (int32_t)(audio_frame * avi->audio_align);
    }

    /* the tick clock restarts at the seek position */
    avi->play_ms_accum = (uint32_t)avi_frame_slot_ms(avi, frame);
    avi->play_tick_base = sgl_tick_get();

    avi->has_frame = 0;
    avi->show_frame = frame - 1;
    avi->frame_pending = 0;
    return 0;
}

/**
 * @brief seek to a percentage of the AVI duration
 * @param obj AVI player object
 * @param percent playback position from 0 to 100; values are clamped
 * @return 0 on success, or -1 if no file or frame index is available
 */
int sgl_avi_seek_percent(sgl_obj_t *obj, int32_t percent)
{
    sgl_avi_t *avi;
    int32_t total, frame;

    if (obj == NULL)
        return -1;
    avi = sgl_container_of(obj, sgl_avi_t, obj);
    if (avi->fd < 0)
        return -1;
    if (percent < 0)
        percent = 0;
    if (percent > 100)
        percent = 100;

    total = avi_vidx_total(avi);
    if (total <= 0)
        return -1;

    frame = (int32_t)(((int64_t)total - 1) * percent / 100);
    return sgl_avi_seek_frame(obj, frame);
}

/**
 * @brief get the total number of video frames
 * @param obj AVI player object
 * @return indexed frame count, header frame count, or 0 if unknown
 */
int32_t sgl_avi_get_frame_total(sgl_obj_t *obj)
{
    sgl_avi_t *avi;
    if (obj == NULL)
        return 0;
 avi = sgl_container_of(obj, sgl_avi_t, obj);
    return avi_vidx_total(avi);
}

/**
 * @brief get the index of the most recently decoded frame
 * @param obj AVI player object
 * @return zero-based frame index; returns 0 before the first frame is decoded
 */
int32_t sgl_avi_tell_frame(sgl_obj_t *obj)
{
    sgl_avi_t *avi;
    if (obj == NULL)
        return 0;
    avi = sgl_container_of(obj, sgl_avi_t, obj);
    return avi->next_frame > 0 ? avi->next_frame - 1 : 0;
}

/**
 * @brief get the total AVI duration
 * @param obj AVI player object
 * @return duration in milliseconds, or 0 if it cannot be determined
 */
int32_t sgl_avi_get_duration(sgl_obj_t *obj)
{
    sgl_avi_t *avi;
    int32_t dur;

    if (obj == NULL)
        return 0;
    avi = sgl_container_of(obj, sgl_avi_t, obj);

    if (avi->audio && avi->audio_rate > 0 && avi->audio_align > 0) {
        dur = (int32_t)((int64_t)avi->aud_bytes_total * 1000 /
                        ((int64_t)avi->audio_rate * avi->audio_align));
        if (dur > 0)
            return dur;
    }
    return avi_frame_slot_ms(avi, avi_vidx_total(avi));
}

/**
 * @brief get the current playback position
 * @param obj AVI player object
 * @return position in milliseconds, using the audio clock when available or
 *         the decoded video frame otherwise
 */
int32_t sgl_avi_get_position(sgl_obj_t *obj)
{
    sgl_avi_t *avi;
    int32_t pos;

    if (obj == NULL)
        return 0;
    avi = sgl_container_of(obj, sgl_avi_t, obj);

    if (avi->state == SGL_AVI_STATE_PLAYING) {
        pos = avi_played_ms(avi);
        if (pos < 0)
            pos = 0;
        return pos;
    }
    pos = avi_frame_slot_ms(avi, avi->next_frame > 0 ? avi->next_frame - 1 : 0);
    return pos;
}

/**
 * @brief get the opened AVI file size
 * @param obj AVI player object
 * @return file size in bytes, or -1 if no file is open
 */
int32_t sgl_avi_get_file_size(sgl_obj_t *obj)
{
    sgl_avi_t *avi;
    if (obj == NULL)
        return -1;
    avi = sgl_container_of(obj, sgl_avi_t, obj);
    return avi->fd >= 0 ? avi->file_size : -1;
}

/**
 * @brief set whether playback restarts when the stream ends
 * @param obj AVI player object
 * @param loop true to repeat playback, false to stop at the end
 * @return none
 */
void sgl_avi_set_loop(sgl_obj_t *obj, bool loop)
{
    sgl_avi_t *avi;
    if (obj == NULL)
        return;
    avi = sgl_container_of(obj, sgl_avi_t, obj);
    avi->loop = loop ? 1 : 0;
}

/**
 * @brief get the current playback state
 * @param obj AVI player object
 * @return current state; returns SGL_AVI_STATE_STOPPED for NULL
 */
sgl_avi_state_t sgl_avi_get_state(sgl_obj_t *obj)
{
    sgl_avi_t *avi;
    if (obj == NULL)
        return SGL_AVI_STATE_STOPPED;
    avi = sgl_container_of(obj, sgl_avi_t, obj);
    return avi->state;
}

/**
 * @brief check whether the opened AVI contains a supported PCM audio stream
 * @param obj AVI player object
 * @return true if a supported PCM stream is present, otherwise false
 */
bool sgl_avi_has_audio(sgl_obj_t *obj)
{
    sgl_avi_t *avi;
    if (obj == NULL)
        return false;
    avi = sgl_container_of(obj, sgl_avi_t, obj);
    return avi->audio ? true : false;
}
