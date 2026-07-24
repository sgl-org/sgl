/* source/fs/fatfs/sgl_fatfs.c
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
#include "sgl_fatfs.h"
#include <string.h>
#include <sgl_mm.h>

#define FAT_FS_NAME         "fatfs"

#define FAT_MAX_OPEN_FILES  8
#define FAT_MAX_OPEN_DIRS   4
#define FAT_MAX_VOLS        4

#define FAT_TYPE_12         12
#define FAT_TYPE_16         16
#define FAT_TYPE_32         32

#define FAT_ATTR_READ_ONLY  0x01
#define FAT_ATTR_HIDDEN     0x02
#define FAT_ATTR_SYSTEM     0x04
#define FAT_ATTR_VOLUME_ID  0x08
#define FAT_ATTR_DIRECTORY  0x10
#define FAT_ATTR_ARCHIVE    0x20
#define FAT_ATTR_LONG_NAME  (FAT_ATTR_READ_ONLY|FAT_ATTR_HIDDEN|FAT_ATTR_SYSTEM|FAT_ATTR_VOLUME_ID)

#define FAT_ENTRY_FREE      0xE5
#define FAT_ENTRY_END       0x00
#define FAT_DIR_ENTRY_SIZE  32

#define FAT_LFN_LAST         0x40
#define FAT_LFN_ATTR        0x0F
#define FAT_LFN_NAME1_LEN   5
#define FAT_LFN_NAME2_LEN   6
#define FAT_LFN_NAME3_LEN   2
#define FAT_LFN_CHARS       (FAT_LFN_NAME1_LEN + FAT_LFN_NAME2_LEN + FAT_LFN_NAME3_LEN)

#define FAT12_EOC_MIN       0xFF8
#define FAT16_EOC_MIN       0xFFF8
#define FAT32_EOC_MIN       0x0FFFFFF8

enum {
    FAT_OK = SGL_FS_OK,
    FAT_ERR_IO = SGL_FS_IO_ERROR,
    FAT_ERR_NOT_FOUND = SGL_FS_NOT_FOUND,
    FAT_ERR_INVALID = SGL_FS_INVALID_ARGUMENT,
    FAT_ERR_DENIED = SGL_FS_PERMISSION_DENIED,
    FAT_ERR_EXIST = SGL_FS_EXISTS,
    FAT_ERR_NO_SPACE = SGL_FS_NO_SPACE,
    FAT_ERR_TOO_MANY_OPEN = SGL_FS_ERROR,
    FAT_ERR_NOT_DIR = SGL_FS_ERROR,
    FAT_ERR_NOT_EMPTY = SGL_FS_ERROR,
    FAT_ERR_NO_MEM = SGL_FS_NO_MEMORY,
};

/* 8.3 short name entry on disk (32 bytes) */
typedef struct {
    uint8_t  name[11];     /* 8.3 filename, space-padded */
    uint8_t  attr;
    uint8_t  nt_res;
    uint8_t  crt_time_tenth;
    uint16_t crt_time;
    uint16_t crt_date;
    uint16_t lst_acc_date;
    uint16_t fst_clus_hi;
    uint16_t wrt_time;
    uint16_t wrt_date;
    uint16_t fst_clus_lo;
    uint32_t file_size;
} sgl_packed fat_dir_entry_t;

typedef struct {
    uint8_t  used;
    uint32_t dir_start_cluster;  /* first cluster of parent dir (0=root) */
    uint32_t entry_sector;       /* sector containing the dir entry */
    uint16_t entry_offset;       /* byte offset within sector */
    uint32_t start_cluster;      /* first data cluster of file */
    uint32_t cur_cluster;        /* current cluster for r/w */
    uint32_t cur_pos;            /* current byte position */
    uint32_t file_size;
    uint8_t  flags;              /* SGL_O_xxx */
    uint8_t  dirty;              /* entry metadata needs flush */
} fat_file_t;

typedef struct {
    uint8_t  used;
    uint32_t start_cluster;      /* first cluster of the directory */
    uint32_t cur_cluster;        /* current cluster being read */
    uint32_t entry_index;        /* next entry index to read */
    uint8_t  lfn_valid;
    char     lfn_name[SGL_FATFS_MAX_LFN * 3 + 1]; /* reconstructed UTF-8 long file name */
    uint8_t  finished;           /* end of directory reached */
} fat_dir_t;

typedef struct {
    sgl_block_dev_t *dev;
    uint8_t  vol_id;

    /* Partition offset in sectors (0 for non-MBR) */
    uint32_t part_offset;

    /* BPB derived values */
    uint16_t sector_size;
    uint8_t  cluster_size;       /* sectors per cluster */
    uint8_t  fat_type;           /* 12, 16, or 32 */
    uint8_t  num_fats;
    uint32_t fat_start;          /* first sector of FAT */
    uint32_t root_dir_start;     /* first sector of root dir (FAT12/16) */
    uint16_t root_entry_count;   /* root dir entries (FAT12/16) */
    uint32_t data_start;         /* first sector of data region */
    uint32_t total_clusters;
    uint32_t total_sectors;
    uint32_t fat_sectors;        /* sectors per FAT */
    uint32_t root_dir_sectors;   /* root dir sectors (FAT12/16) */
    uint32_t fat32_root_cluster; /* root dir cluster (FAT32) */

    /* Sector cache (single buffer) */
    uint8_t *sec_buf;
    uint32_t sec_buf_num;        /* sector number in buffer, 0xFFFFFFFF=invalid */
    uint8_t  sec_buf_dirty;

    fat_file_t files[FAT_MAX_OPEN_FILES];
    fat_dir_t  dirs[FAT_MAX_OPEN_DIRS];
} fat_ctx_t;

/* Global volume -> block device mapping */
static sgl_block_dev_t *vol_dev_map[FAT_MAX_VOLS];


static uint16_t rd16(const uint8_t *p) { return (uint16_t)p[0] | ((uint16_t)p[1] << 8); }
static uint32_t rd32(const uint8_t *p) { return (uint32_t)p[0] | ((uint32_t)p[1]<<8) | ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24); }
static void wr16(uint8_t *p, uint16_t v) { p[0]=v&0xFF; p[1]=(v>>8)&0xFF; }
static void wr32(uint8_t *p, uint32_t v) { p[0]=v&0xFF; p[1]=(v>>8)&0xFF; p[2]=(v>>16)&0xFF; p[3]=(v>>24)&0xFF; }

static size_t fat_utf8_decode(const char *s, uint16_t *out)
{
    const unsigned char *p = (const unsigned char *)s;
    if (p[0] < 0x80) {
        *out = p[0];
        return 1;
    }
    if ((p[0] & 0xE0) == 0xC0 && (p[1] & 0xC0) == 0x80) {
        *out = (uint16_t)(((p[0] & 0x1F) << 6) | (p[1] & 0x3F));
        return 2;
    }
    if ((p[0] & 0xF0) == 0xE0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80) {
        *out = (uint16_t)(((p[0] & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F));
        return 3;
    }
    *out = '?';
    return 1;
}

static size_t fat_utf8_encode(uint16_t ch, char *out)
{
    if (ch < 0x80) {
        out[0] = (char)ch;
        return 1;
    }
    if (ch < 0x800) {
        out[0] = (char)(0xC0 | (ch >> 6));
        out[1] = (char)(0x80 | (ch & 0x3F));
        return 2;
    }
    out[0] = (char)(0xE0 | (ch >> 12));
    out[1] = (char)(0x80 | ((ch >> 6) & 0x3F));
    out[2] = (char)(0x80 | (ch & 0x3F));
    return 3;
}

static uint8_t fat_lfn_checksum(const uint8_t short_name[11])
{
    uint8_t sum = 0;
    for (int i = 0; i < 11; i++) {
        sum = (uint8_t)(((sum & 1) ? 0x80 : 0) + (sum >> 1) + short_name[i]);
    }
    return sum;
}

static int fat_utf8_to_ucs2(const char *name, uint16_t *out, size_t max_chars)
{
    size_t count = 0;
    while (*name) {
        if (count >= max_chars) return -1;
        uint16_t ch = 0;
        size_t used = fat_utf8_decode(name, &ch);
        out[count++] = ch;
        name += used;
    }
    return (int)count;
}

static int fat_ucs2_to_utf8(const uint16_t *name, size_t count, char *out, size_t out_size)
{
    size_t pos = 0;
    for (size_t i = 0; i < count; i++) {
        uint16_t ch = name[i];
        if (ch == 0x0000 || ch == 0xFFFF) break;
        if (pos + 4 >= out_size) break;
        pos += fat_utf8_encode(ch, &out[pos]);
    }
    if (out_size > 0) out[pos < out_size ? pos : out_size - 1] = '\0';
    return (int)pos;
}

static int fat_name_needs_lfn(const char *name)
{
    size_t base_len = 0;
    size_t ext_len = 0;
    int seen_dot = 0;

    if (!name || name[0] == '\0' || name[0] == '.') return 1;

    for (const unsigned char *p = (const unsigned char *)name; *p; p++) {
        unsigned char ch = *p;
        if (ch >= 0x80 || ch == ' ' || ch == '+' || ch == ',' || ch == ';' ||
            ch == '=' || ch == '[' || ch == ']') {
            return 1;
        }
        if (ch >= 'a' && ch <= 'z') return 1;
        if (ch == '.') {
            if (seen_dot || base_len == 0) return 1;
            seen_dot = 1;
            continue;
        }
        if (!seen_dot) {
            base_len++;
            if (base_len > 8) return 1;
        } else {
            ext_len++;
            if (ext_len > 3) return 1;
        }
    }

    return base_len == 0;
}

/**
 * @brief FatFS default configuration.
 * @note  This configuration is used when mounting the file system.
 */
static sgl_fatfs_config_t fatfs_default_cfg = {
    .vol_id = 0,
    .opt = 1,
};

static int fat_read_sector(fat_ctx_t *ctx, uint32_t sector, uint8_t *buf)
{
    return ctx->dev->read_sectors(ctx->dev, sector + ctx->part_offset, buf, 1);
}

static int fat_write_sector(fat_ctx_t *ctx, uint32_t sector, const uint8_t *buf)
{
    return ctx->dev->write_sectors(ctx->dev, sector + ctx->part_offset, buf, 1);
}

/* Cached sector read */
static int fat_cache_read(fat_ctx_t *ctx, uint32_t sector)
{
    if (ctx->sec_buf_num == sector) return FAT_OK;
    if (ctx->sec_buf_dirty && ctx->sec_buf_num != 0xFFFFFFFF) {
        if (fat_write_sector(ctx, ctx->sec_buf_num, ctx->sec_buf) != 0) {
            SGL_LOG_ERROR(FAT_FS_NAME " cache_read: flush dirty sec=%d failed", (int)ctx->sec_buf_num);
            return FAT_ERR_IO;
        }
    }
    if (fat_read_sector(ctx, sector, ctx->sec_buf) != 0) {
        SGL_LOG_ERROR(FAT_FS_NAME " cache_read: read sec=%d failed", (int)sector);
        return FAT_ERR_IO;
    }
    ctx->sec_buf_num = sector;
    ctx->sec_buf_dirty = 0;
    return FAT_OK;
}

/* Mark cache dirty */
static void fat_cache_dirty(fat_ctx_t *ctx)
{
    ctx->sec_buf_dirty = 1;
}

/* Flush cache */
static int fat_cache_flush(fat_ctx_t *ctx)
{
    if (ctx->sec_buf_dirty && ctx->sec_buf_num != 0xFFFFFFFF) {
        if (fat_write_sector(ctx, ctx->sec_buf_num, ctx->sec_buf) != 0) {
            SGL_LOG_ERROR(FAT_FS_NAME " cache_flush: write sec=%d failed", (int)ctx->sec_buf_num);
            return FAT_ERR_IO;
        }
        ctx->sec_buf_dirty = 0;
    }
    return FAT_OK;
}

/* Invalidate cache */
static void fat_cache_invalidate(fat_ctx_t *ctx)
{
    if (ctx->sec_buf_dirty && ctx->sec_buf_num != 0xFFFFFFFF) {
        fat_write_sector(ctx, ctx->sec_buf_num, ctx->sec_buf);
    }
    ctx->sec_buf_num = 0xFFFFFFFF;
    ctx->sec_buf_dirty = 0;
}

/* Convert cluster number to sector number */
static uint32_t cluster_to_sector(fat_ctx_t *ctx, uint32_t cluster)
{
    if (ctx->fat_type == FAT_TYPE_32 && cluster == 0) {
        cluster = ctx->fat32_root_cluster;
    }
    return ctx->data_start + (uint32_t)(cluster - 2) * ctx->cluster_size;
}

static int is_eoc(fat_ctx_t *ctx, uint32_t entry)
{
    if (ctx->fat_type == FAT_TYPE_12) return entry >= FAT12_EOC_MIN;
    if (ctx->fat_type == FAT_TYPE_16) return entry >= FAT16_EOC_MIN;
    return entry >= FAT32_EOC_MIN;
}

/* Read FAT entry for a given cluster */
static uint32_t fat_get_entry(fat_ctx_t *ctx, uint32_t cluster)
{
    uint32_t offset, ent_offset, sec;
    int ret;

    if (ctx->fat_type == FAT_TYPE_12) {
        offset = cluster + (cluster / 2);  /* 1.5 bytes per entry */
    } else if (ctx->fat_type == FAT_TYPE_16) {
        offset = cluster * 2;
    } else {
        offset = cluster * 4;
    }

    sec = ctx->fat_start + offset / ctx->sector_size;
    ent_offset = offset % ctx->sector_size;

    ret = fat_cache_read(ctx, sec);
    if (ret != FAT_OK) return 0xFFFFFFFF;

    if (ctx->fat_type == FAT_TYPE_12) {
        uint16_t val;
        if ((uint16_t)ent_offset == ctx->sector_size - 1) {
            /* Entry spans two sectors */
            uint8_t b0 = ctx->sec_buf[ent_offset];
            fat_cache_read(ctx, sec + 1);
            uint8_t b1 = ctx->sec_buf[0];
            val = (uint16_t)b0 | ((uint16_t)b1 << 8);
        } else {
            val = rd16(&ctx->sec_buf[ent_offset]);
        }
        if (cluster & 1) val >>= 4;
        else val &= 0x0FFF;
        return val;
    } else if (ctx->fat_type == FAT_TYPE_16) {
        return rd16(&ctx->sec_buf[ent_offset]);
    } else {
        return rd32(&ctx->sec_buf[ent_offset]) & 0x0FFFFFFF;
    }
}

/* Write FAT entry for a given cluster (writes to all FAT copies) */
static int fat_set_entry(fat_ctx_t *ctx, uint32_t cluster, uint32_t value)
{
    for (uint8_t f = 0; f < ctx->num_fats; f++) {
        uint32_t fat_base = ctx->fat_start + (uint32_t)f * ctx->fat_sectors;
        uint32_t offset, ent_offset, sec;

        if (ctx->fat_type == FAT_TYPE_12) {
            offset = cluster + (cluster / 2);
        } else if (ctx->fat_type == FAT_TYPE_16) {
            offset = cluster * 2;
        } else {
            offset = cluster * 4;
        }

        sec = fat_base + offset / ctx->sector_size;
        ent_offset = offset % ctx->sector_size;

        if (fat_cache_read(ctx, sec) != FAT_OK) {
            SGL_LOG_ERROR(FAT_FS_NAME " set_entry: cache_read sec=%d failed", (int)sec);
            return FAT_ERR_IO;
        }

        if (ctx->fat_type == FAT_TYPE_12) {
            uint16_t val = rd16(&ctx->sec_buf[ent_offset]);
            if (cluster & 1) {
                val = (val & 0x000F) | ((value & 0x0FFF) << 4);
            } else {
                val = (val & 0xF000) | (value & 0x0FFF);
            }
            wr16(&ctx->sec_buf[ent_offset], val);
            fat_cache_dirty(ctx);
            /* Handle sector-spanning case */
            if (ent_offset == (uint32_t)(ctx->sector_size - 1)) {
                fat_cache_flush(ctx);
                fat_cache_read(ctx, sec + 1);
                ctx->sec_buf[0] = (uint8_t)((val >> 8) & 0xFF);
                fat_cache_dirty(ctx);
            }
        } else if (ctx->fat_type == FAT_TYPE_16) {
            wr16(&ctx->sec_buf[ent_offset], (uint16_t)(value & 0xFFFF));
            fat_cache_dirty(ctx);
        } else {
            uint32_t old = rd32(&ctx->sec_buf[ent_offset]);
            wr32(&ctx->sec_buf[ent_offset], (old & 0xF0000000) | (value & 0x0FFFFFFF));
            fat_cache_dirty(ctx);
        }
        fat_cache_flush(ctx);
    }
    return FAT_OK;
}

/* Allocate a free cluster, returns cluster number or 0 on failure */
static uint32_t fat_alloc_cluster(fat_ctx_t *ctx)
{
    for (uint32_t c = 2; c < ctx->total_clusters + 2; c++) {
        uint32_t val = fat_get_entry(ctx, c);
        if (val == 0) {
            /* Mark as end of chain */
            uint32_t eoc = (ctx->fat_type == FAT_TYPE_12) ? 0xFFF :
                           (ctx->fat_type == FAT_TYPE_16) ? 0xFFFF : 0x0FFFFFFF;
            fat_set_entry(ctx, c, eoc);
            return c;
        }
    }
    return 0;
}

/* Free a cluster chain starting from 'start' */
static int fat_free_chain(fat_ctx_t *ctx, uint32_t start)
{
    if (start < 2) return FAT_OK;
    uint32_t cur = start;
    while (cur >= 2 && !is_eoc(ctx, cur)) {
        uint32_t next = fat_get_entry(ctx, cur);
        fat_set_entry(ctx, cur, 0);
        cur = next;
    }
    if (cur >= 2) fat_set_entry(ctx, cur, 0); /* free last cluster */
    return FAT_OK;
}

/* Get the Nth cluster in a chain (0-based). Returns 0 on failure */
static uint32_t fat_chain_get(fat_ctx_t *ctx, uint32_t start, uint32_t index)
{
    uint32_t cur = start;
    for (uint32_t i = 0; i < index; i++) {
        if (cur < 2 || is_eoc(ctx, cur)) return 0;
        cur = fat_get_entry(ctx, cur);
    }
    return (cur >= 2 && !is_eoc(ctx, cur)) ? cur : (index == 0 ? start : 0);
}

static int fat_read_dir_entry(fat_ctx_t *ctx, uint32_t dir_cluster,
                              uint32_t index, fat_dir_entry_t *entry,
                              uint32_t *out_sector, uint16_t *out_offset);
static int fat_write_dir_entry(fat_ctx_t *ctx, uint32_t sector, uint16_t offset,
                               const fat_dir_entry_t *entry);

static int fat_decode_lfn_name(const uint16_t *ucs2, size_t count, char *out, size_t out_size)
{
    return fat_ucs2_to_utf8(ucs2, count, out, out_size);
}

static int fat_collect_lfn_name(fat_ctx_t *ctx, uint32_t dir_cluster, uint32_t start_index,
                                char *out, size_t out_size, uint32_t *next_index)
{
    uint16_t parts[SGL_FATFS_MAX_LFN + FAT_LFN_CHARS];
    uint8_t expected_seq = 0;
    uint8_t checksum = 0;
    size_t max_chars = 0;
    uint32_t i = start_index;

    memset(parts, 0xFF, sizeof(parts));

    while (1) {
        fat_dir_entry_t e;
        uint32_t sec; uint16_t off;
        int ret = fat_read_dir_entry(ctx, dir_cluster, i, &e, &sec, &off);
        if (ret != FAT_OK) return ret;
        if (e.name[0] == FAT_ENTRY_END) return FAT_ERR_NOT_FOUND;
        if (e.name[0] == FAT_ENTRY_FREE) {
            i++;
            expected_seq = 0;
            continue;
        }
        if (!(e.attr & FAT_ATTR_LONG_NAME)) break;

        const uint8_t *raw = (const uint8_t *)&e;
        uint8_t seq = raw[0] & 0x1F;
        if (raw[0] & FAT_LFN_LAST) {
            expected_seq = seq;
            checksum = raw[13];
        }
        if (expected_seq == 0 || seq != expected_seq) {
            expected_seq = 0;
            i++;
            continue;
        }
        if (raw[13] != checksum) {
            expected_seq = 0;
            i++;
            continue;
        }

        size_t seq_index = (size_t)(seq - 1);
        size_t base = seq_index * FAT_LFN_CHARS;
        if (base + FAT_LFN_CHARS > SGL_FATFS_MAX_LFN + FAT_LFN_CHARS) {
            expected_seq = 0;
            i++;
            continue;
        }

        uint16_t seq_name[13] = {
            rd16(&raw[1]), rd16(&raw[3]), rd16(&raw[5]), rd16(&raw[7]), rd16(&raw[9]),
            rd16(&raw[14]), rd16(&raw[16]), rd16(&raw[18]), rd16(&raw[20]), rd16(&raw[22]),
            rd16(&raw[24]), rd16(&raw[28]), rd16(&raw[30])
        };
        for (size_t k = 0; k < 13; k++) {
            parts[base + k] = seq_name[k];
        }
        if (seq_index + 1 > max_chars) {
            max_chars = (seq_index + 1) * FAT_LFN_CHARS;
        }

        if (seq == 1) {
            size_t begin = 0;
            while (begin < max_chars && parts[begin] == 0xFFFF) begin++;
            size_t end = max_chars;
            while (end > begin && parts[end - 1] == 0xFFFF) end--;
            if (end > begin) {
                fat_decode_lfn_name(&parts[begin], end - begin, out, out_size);
            } else if (out_size > 0) {
                out[0] = '\0';
            }
            if (next_index) *next_index = i + 1;
            return FAT_OK;
        }
        expected_seq--;
        i++;
    }
    return FAT_ERR_NOT_FOUND;
}

/* Convert a filename component to 8.3 format (space-padded, uppercase) */
static void name_to_83(const char *name, uint8_t out[11])
{
    memset(out, ' ', 11);
    int i = 0, dot = -1;
    /* Find dot position */
    for (int j = 0; name[j]; j++) {
        if (name[j] == '.') { dot = j; break; }
    }

    /* Copy base name (up to 8 chars before dot or end) */
    int base_len = (dot >= 0) ? dot : (int)strlen(name);
    if (base_len > 8) base_len = 8;
    for (i = 0; i < base_len; i++) {
        char c = name[i];
        if (c >= 'a' && c <= 'z') c -= 32;
        out[i] = (uint8_t)c;
    }

    /* Copy extension (up to 3 chars after dot) */
    if (dot >= 0) {
        int ext_len = (int)strlen(name + dot + 1);
        if (ext_len > 3) ext_len = 3;
        for (i = 0; i < ext_len; i++) {
            char c = name[dot + 1 + i];
            if (c >= 'a' && c <= 'z') c -= 32;
            out[8 + i] = (uint8_t)c;
        }
    }
}

/* Convert 8.3 dir entry name to null-terminated string */
static void name_from_83(const uint8_t entry[11], char *out, uint32_t out_size)
{
    uint32_t pos = 0;
    /* Copy base name, trim trailing spaces */
    int base_end = 7;
    while (base_end >= 0 && entry[base_end] == ' ') base_end--;
    for (int i = 0; i <= base_end && pos < out_size - 1; i++) {
        out[pos++] = (char)entry[i];
    }
    /* Copy extension if present */
    int ext_end = 10;
    while (ext_end >= 8 && entry[ext_end] == ' ') ext_end--;
    if (ext_end >= 8 && pos < out_size - 2) {
        out[pos++] = '.';
        for (int i = 8; i <= ext_end && pos < out_size - 1; i++) {
            out[pos++] = (char)entry[i];
        }
    }
    out[pos] = '\0';
}

/* Get root directory start cluster */
static uint32_t root_cluster(fat_ctx_t *ctx)
{
    if (ctx->fat_type == FAT_TYPE_32) return ctx->fat32_root_cluster;
    return 0; /* FAT12/16: root is in fixed area */
}

/* Read a directory entry at a given index within a directory.
   dir_cluster=0 means root dir (FAT12/16 fixed area).
   Returns FAT_OK, or FAT_ERR_NOT_FOUND when end reached */
static int fat_read_dir_entry(fat_ctx_t *ctx, uint32_t dir_cluster,
                              uint32_t index, fat_dir_entry_t *entry,
                              uint32_t *out_sector, uint16_t *out_offset)
{
    uint32_t byte_offset = index * FAT_DIR_ENTRY_SIZE;
    uint32_t sector_in_dir = byte_offset / ctx->sector_size;
    uint16_t offset_in_sec = (uint16_t)(byte_offset % ctx->sector_size);
    uint32_t sector;

    if (dir_cluster == 0) {
        /* FAT12/16 root directory: fixed location, limited entries */
        uint32_t max_entries = (uint32_t)ctx->root_entry_count;
        if (index >= max_entries) {
            SGL_LOG_ERROR(FAT_FS_NAME " read_dir_entry: root index=%d >= max=%d", (int)index, (int)max_entries);
            return FAT_ERR_NOT_FOUND;
        }
        sector = ctx->root_dir_start + sector_in_dir;
        if (sector >= ctx->root_dir_start + ctx->root_dir_sectors) {
            SGL_LOG_ERROR(FAT_FS_NAME " read_dir_entry: root sector=%d out of range", (int)sector);
            return FAT_ERR_NOT_FOUND;
        }
    } else {
        /* Cluster-based directory */
        uint32_t cluster_index = sector_in_dir / ctx->cluster_size;
        uint32_t sec_in_cluster = sector_in_dir % ctx->cluster_size;

        uint32_t clust = fat_chain_get(ctx, dir_cluster, cluster_index);
        if (clust == 0) {
            SGL_LOG_ERROR(FAT_FS_NAME " read_dir_entry: chain_get dir=%d idx=%d failed", (int)dir_cluster, (int)cluster_index);
            return FAT_ERR_NOT_FOUND;
        }
        sector = cluster_to_sector(ctx, clust) + sec_in_cluster;
    }

    if (fat_cache_read(ctx, sector) != FAT_OK) {
        SGL_LOG_ERROR(FAT_FS_NAME " read_dir_entry: cache_read sec=%d failed", (int)sector);
        return FAT_ERR_IO;
    }
    memcpy(entry, &ctx->sec_buf[offset_in_sec], FAT_DIR_ENTRY_SIZE);

    if (out_sector) *out_sector = sector;
    if (out_offset) *out_offset = offset_in_sec;

    return FAT_OK;
}

/* Write a directory entry at a given sector/offset */
static int fat_write_dir_entry(fat_ctx_t *ctx, uint32_t sector, uint16_t offset,
                               const fat_dir_entry_t *entry)
{
    if (fat_cache_read(ctx, sector) != FAT_OK) {
        SGL_LOG_ERROR(FAT_FS_NAME " write_dir_entry: cache_read sec=%d failed", (int)sector);
        return FAT_ERR_IO;
    }
    memcpy(&ctx->sec_buf[offset], entry, FAT_DIR_ENTRY_SIZE);
    fat_cache_dirty(ctx);
    return fat_cache_flush(ctx);
}

/* Search a directory for a given name component.
   Returns FAT_OK if found, FAT_ERR_NOT_FOUND if not */
static int fat_find_in_dir(fat_ctx_t *ctx, uint32_t dir_cluster,
                           const char *name, fat_dir_entry_t *entry,
                           uint32_t *out_sector, uint16_t *out_offset)
{
    uint8_t target[11];
    name_to_83(name, target);

    for (uint32_t i = 0; ; ) {
        fat_dir_entry_t e;
        uint32_t sec; uint16_t off;
        int ret = fat_read_dir_entry(ctx, dir_cluster, i, &e, &sec, &off);
        if (ret != FAT_OK) return ret;
        if (e.name[0] == FAT_ENTRY_END) return FAT_ERR_NOT_FOUND;
        if (e.name[0] == FAT_ENTRY_FREE) {
            i++;
            continue;
        }
        if (e.attr & FAT_ATTR_LONG_NAME) {
            char lfn_name[SGL_FATFS_MAX_LFN * 3 + 1];
            uint32_t short_index = 0;
            if (fat_collect_lfn_name(ctx, dir_cluster, i, lfn_name, sizeof(lfn_name), &short_index) == FAT_OK) {
                if (fat_read_dir_entry(ctx, dir_cluster, short_index, &e, &sec, &off) != FAT_OK) {
                    return FAT_ERR_IO;
                }
                if (strcmp(lfn_name, name) == 0 || memcmp(e.name, target, 11) == 0) {
                    *entry = e;
                    if (out_sector) *out_sector = sec;
                    if (out_offset) *out_offset = off;
                    return FAT_OK;
                }
                i = short_index + 1;
                continue;
            }
            i++;
            continue;
        }
        i++;
        if (e.attr & FAT_ATTR_VOLUME_ID) continue;
        if (memcmp(e.name, target, 11) == 0) {
            *entry = e;
            if (out_sector) *out_sector = sec;
            if (out_offset) *out_offset = off;
            return FAT_OK;
        }
    }
}

static int fat_extend_directory(fat_ctx_t *ctx, uint32_t dir_cluster)
{
    if (dir_cluster == 0) return FAT_ERR_NO_SPACE;

    uint32_t new_clust = fat_alloc_cluster(ctx);
    if (new_clust == 0) return FAT_ERR_NO_SPACE;

    uint32_t base = cluster_to_sector(ctx, new_clust);
    memset(ctx->sec_buf, 0, ctx->sector_size);
    for (uint8_t s = 0; s < ctx->cluster_size; s++) {
        if (fat_write_sector(ctx, base + s, ctx->sec_buf) != 0) return FAT_ERR_IO;
    }

    uint32_t cur = dir_cluster;
    while (!is_eoc(ctx, fat_get_entry(ctx, cur))) {
        cur = fat_get_entry(ctx, cur);
    }
    uint32_t eoc = (ctx->fat_type == FAT_TYPE_12) ? 0xFFF :
                   (ctx->fat_type == FAT_TYPE_16) ? 0xFFFF : 0x0FFFFFFF;
    fat_set_entry(ctx, cur, new_clust);
    fat_set_entry(ctx, new_clust, eoc);
    return FAT_OK;
}

static int fat_find_free_entries(fat_ctx_t *ctx, uint32_t dir_cluster,
                                 uint32_t needed, uint32_t *out_index)
{
    uint32_t run_start = 0;
    uint32_t run_count = 0;

    for (uint32_t i = 0; ; i++) {
        fat_dir_entry_t entry;
        int ret = fat_read_dir_entry(ctx, dir_cluster, i, &entry, NULL, NULL);
        if (ret != FAT_OK) {
            if (dir_cluster == 0) return FAT_ERR_NO_SPACE;
            ret = fat_extend_directory(ctx, dir_cluster);
            if (ret != FAT_OK) return ret;
            i--;
            continue;
        }

        if (entry.name[0] == FAT_ENTRY_FREE || entry.name[0] == FAT_ENTRY_END) {
            if (run_count == 0) run_start = i;
            run_count++;
            if (run_count >= needed) {
                *out_index = run_start;
                return FAT_OK;
            }
            continue;
        }

        run_count = 0;
    }
}

static int fat_write_dir_entry_by_index(fat_ctx_t *ctx, uint32_t dir_cluster,
                                        uint32_t index, const fat_dir_entry_t *entry,
                                        uint32_t *out_sector, uint16_t *out_offset)
{
    fat_dir_entry_t old_entry;
    uint32_t sec;
    uint16_t off;
    int ret = fat_read_dir_entry(ctx, dir_cluster, index, &old_entry, &sec, &off);
    if (ret != FAT_OK) return ret;
    ret = fat_write_dir_entry(ctx, sec, off, entry);
    if (ret == FAT_OK) {
        if (out_sector) *out_sector = sec;
        if (out_offset) *out_offset = off;
    }
    return ret;
}

static void fat_lfn_write_char(uint8_t raw[32], int index, uint16_t value)
{
    static const uint8_t offsets[FAT_LFN_CHARS] = {
        1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30
    };
    wr16(&raw[offsets[index]], value);
}

/* Resolve a path to its parent directory cluster and the final name component.
   Returns FAT_OK on success. *parent_cluster = 0 for FAT12/16 root.
   final_name points into the original path string. */
static int fat_resolve_path(fat_ctx_t *ctx, const char *path,
                            uint32_t *parent_cluster, const char **final_name)
{
    while (*path == '/') path++;
    if (*path == '\0') {
        *parent_cluster = root_cluster(ctx);
        *final_name = NULL;
        return FAT_OK;
    }

    uint32_t cur_dir = root_cluster(ctx);
    char component[SGL_FATFS_MAX_LFN * 3 + 1];

    while (*path) {
        const char *component_start = path;
        size_t component_len = 0;
        while (path[component_len] && path[component_len] != '/') {
            component_len++;
        }
        if (component_len == 0 || component_len >= sizeof(component)) {
            return FAT_ERR_INVALID;
        }

        const char *separator = path + component_len;
        path = separator;
        while (*path == '/') path++;

        if (*path == '\0' && *separator != '/') {
            *parent_cluster = cur_dir;
            *final_name = component_start;
            return FAT_OK;
        }

        memcpy(component, component_start, component_len);
        component[component_len] = '\0';

        fat_dir_entry_t entry;
        int ret = fat_find_in_dir(ctx, cur_dir, component, &entry, NULL, NULL);
        if (ret != FAT_OK) return FAT_ERR_NOT_FOUND;
        if (!(entry.attr & FAT_ATTR_DIRECTORY)) return FAT_ERR_NOT_DIR;

        cur_dir = ((uint32_t)entry.fst_clus_hi << 16) | entry.fst_clus_lo;
        if (cur_dir == 0) cur_dir = root_cluster(ctx);

        if (*path == '\0') {
            *parent_cluster = cur_dir;
            *final_name = NULL;
            return FAT_OK;
        }
    }

    *parent_cluster = cur_dir;
    *final_name = NULL;
    return FAT_OK;
}

/* Check if a directory is empty (only . and .. or nothing) */
static int fat_dir_is_empty(fat_ctx_t *ctx, uint32_t dir_cluster)
{
    for (uint32_t i = 0; ; i++) {
        fat_dir_entry_t e;
        int ret = fat_read_dir_entry(ctx, dir_cluster, i, &e, NULL, NULL);
        if (ret != FAT_OK) return 1; /* end of dir = empty */
        if (e.name[0] == FAT_ENTRY_END) return 1; /* end of dir = empty */
        if (e.name[0] == FAT_ENTRY_FREE) continue;
        if (e.attr & FAT_ATTR_LONG_NAME) continue;
        /* Check for . and .. */
        if (e.name[0] == '.' && (e.name[1] == ' ' || (e.name[1] == '.' && e.name[2] == ' ')))
            continue;
        return 0; /* Found a real entry */
    }
}

/* Create a new directory entry in parent_dir. Returns sector/offset of new entry */
static int fat_create_entry(fat_ctx_t *ctx, uint32_t parent_dir,
                            const char *name, uint8_t attr,
                            uint32_t start_cluster, uint32_t file_size,
                            uint32_t *out_sector, uint16_t *out_offset)
{
    uint8_t short_name[11];
    uint32_t needed_entries = 1;
    uint32_t first_index = 0;
    int lfn_slots = 0;
    uint16_t name_u16[SGL_FATFS_MAX_LFN];
    int name_len = -1;

    name_to_83(name, short_name);

    if (fat_name_needs_lfn(name)) {
        name_len = fat_utf8_to_ucs2(name, name_u16, SGL_FATFS_MAX_LFN);
        if (name_len <= 0) return FAT_ERR_INVALID;
        lfn_slots = (name_len + FAT_LFN_CHARS - 1) / FAT_LFN_CHARS;
        needed_entries += (uint32_t)lfn_slots;
    }

    int ret = fat_find_free_entries(ctx, parent_dir, needed_entries, &first_index);
    if (ret != FAT_OK) return ret;

    if (lfn_slots > 0) {
        uint8_t checksum = fat_lfn_checksum(short_name);
        for (int slot = 0; slot < lfn_slots; slot++) {
            fat_dir_entry_t lfn_entry;
            uint8_t *raw = (uint8_t *)&lfn_entry;
            uint8_t seq = (uint8_t)(lfn_slots - slot);
            uint32_t part = (uint32_t)(seq - 1) * FAT_LFN_CHARS;

            memset(raw, 0xFF, FAT_DIR_ENTRY_SIZE);
            if (slot == 0) seq |= FAT_LFN_LAST;
            raw[0] = seq;
            raw[11] = FAT_LFN_ATTR;
            raw[12] = 0;
            raw[13] = checksum;
            raw[26] = 0;
            raw[27] = 0;

            for (int index = 0; index < FAT_LFN_CHARS; index++) {
                uint16_t value = 0xFFFF;
                uint32_t char_index = part + (uint32_t)index;
                if (char_index < (uint32_t)name_len) {
                    value = name_u16[char_index];
                } else if (char_index == (uint32_t)name_len) {
                    value = 0x0000;
                }
                fat_lfn_write_char(raw, index, value);
            }

            ret = fat_write_dir_entry_by_index(ctx, parent_dir, first_index + (uint32_t)slot,
                                               &lfn_entry, NULL, NULL);
            if (ret != FAT_OK) return ret;
        }
    }

    fat_dir_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    memcpy(entry.name, short_name, sizeof(entry.name));
    entry.attr = attr;
    entry.fst_clus_lo = (uint16_t)(start_cluster & 0xFFFF);
    entry.fst_clus_hi = (uint16_t)((start_cluster >> 16) & 0xFFFF);
    entry.file_size = file_size;

    return fat_write_dir_entry_by_index(ctx, parent_dir, first_index + (uint32_t)lfn_slots,
                                        &entry, out_sector, out_offset);
}

static int fatfs_mount(void **fs, sgl_block_dev_t *dev,
                       const char *mount_point, void *fs_config)
{
    (void)mount_point;
    sgl_fatfs_config_t *cfg = NULL;

    if (!fs_config) {
        cfg = &fatfs_default_cfg;
    }
    else {
        cfg = (sgl_fatfs_config_t *)fs_config;
    }

    if (cfg->vol_id >= FAT_MAX_VOLS) {
        SGL_LOG_ERROR(FAT_FS_NAME " mount: invalid vol_id=%d", cfg->vol_id);
        return FAT_ERR_INVALID;
    }

    if (dev->init) {
        int init_ret = dev->init(dev);
        if (init_ret != 0) {
            SGL_LOG_ERROR(FAT_FS_NAME " mount: dev init failed, ret=%d", init_ret);
            return FAT_ERR_IO;
        }
    }

    fat_ctx_t *ctx = (fat_ctx_t *)sgl_malloc(sizeof(fat_ctx_t));
    if (!ctx) {
        SGL_LOG_ERROR(FAT_FS_NAME " mount: malloc ctx failed");
        return FAT_ERR_NO_MEM;
    }
    memset(ctx, 0, sizeof(fat_ctx_t));

    ctx->dev = dev;
    ctx->vol_id = cfg->vol_id;
    ctx->sec_buf_num = 0xFFFFFFFF;
    vol_dev_map[cfg->vol_id] = dev;

    ctx->sec_buf = (uint8_t *)sgl_malloc(512);
    if (!ctx->sec_buf) {
        SGL_LOG_ERROR(FAT_FS_NAME " mount: malloc sec_buf failed");
        sgl_free(ctx);
        return FAT_ERR_NO_MEM;
    }

    if (fat_read_sector(ctx, 0, ctx->sec_buf) != 0) {
        SGL_LOG_ERROR(FAT_FS_NAME " read sector 0 failed");
        sgl_free(ctx->sec_buf); sgl_free(ctx); return FAT_ERR_IO;
    }

    uint8_t *bpb = ctx->sec_buf;

    /* Check if sector 0 is an MBR (partition table) instead of a FAT VBR.
     * MBR signature is also 0x55AA at offset 510, but the partition table
     * starts at offset 446. If offset 450 (partition type of 1st entry) is
     * a valid FAT type, we treat it as MBR and follow the first partition. */
    {
        uint8_t ptype = bpb[450]; /* partition type of 1st entry */
        int is_mbr = 0;

        /* Check for MBR signature AND a valid FAT partition type */
        if (bpb[510] == 0x55 && bpb[511] == 0xAA) {
            if (ptype == 0x01 || ptype == 0x04 || ptype == 0x06 ||
                ptype == 0x0B || ptype == 0x0C || ptype == 0x0E ||
                ptype == 0x1B || ptype == 0x1C || ptype == 0x0F) {
                is_mbr = 1;
            }
        }

        if (is_mbr) {
            /* Parse the first partition entry at offset 446 */
            uint32_t part_lba = rd32(&bpb[454]); /* LBA of first partition */
            SGL_LOG_INFO(FAT_FS_NAME " MBR detected, first partition at LBA 0x%x", (unsigned)part_lba);

            if (part_lba == 0) {
                SGL_LOG_ERROR(FAT_FS_NAME " mount: MBR partition LBA is 0");
                sgl_free(ctx->sec_buf); sgl_free(ctx);
                return FAT_ERR_INVALID;
            }

            /* Set partition offset so all subsequent sector reads are relative
             * to the partition start. Read VBR using global address first. */
            ctx->part_offset = part_lba;

            /* Read the VBR (volume boot record) from the partition start */
            if (fat_read_sector(ctx, 0, ctx->sec_buf) != 0) {
                SGL_LOG_ERROR(FAT_FS_NAME " mount: read VBR at LBA %u failed", (unsigned)part_lba);
                sgl_free(ctx->sec_buf); sgl_free(ctx);
                return FAT_ERR_IO;
            }
            bpb = ctx->sec_buf;
        }
    }

    if (bpb[510] != 0x55 || bpb[511] != 0xAA) {
        SGL_LOG_ERROR(FAT_FS_NAME " invalid boot signature");
        sgl_free(ctx->sec_buf); sgl_free(ctx); return FAT_ERR_INVALID;
    }
    if (bpb[0] != 0xEB && bpb[0] != 0xE9 && bpb[0] != 0xE8) {
        SGL_LOG_WARN(FAT_FS_NAME " non-standard jump byte: 0x%02x", bpb[0]);
    }

    ctx->sector_size = rd16(&bpb[11]);
    if (ctx->sector_size == 0 || (ctx->sector_size & (ctx->sector_size - 1)) != 0) {
        SGL_LOG_ERROR(FAT_FS_NAME " mount: invalid sector_size=%d (Device unformatted or read error)", ctx->sector_size);
        return FAT_ERR_INVALID;
    }

    ctx->cluster_size = bpb[13];
    ctx->num_fats = bpb[16];
    ctx->root_entry_count = rd16(&bpb[17]);
    ctx->total_sectors = rd16(&bpb[19]);
    if (ctx->total_sectors == 0) ctx->total_sectors = rd32(&bpb[32]);
    ctx->fat_sectors = rd16(&bpb[22]);
    if (ctx->fat_sectors == 0) ctx->fat_sectors = rd32(&bpb[36]);

    if (ctx->sector_size != 512) {
        sgl_free(ctx->sec_buf);
        ctx->sec_buf = (uint8_t *)sgl_malloc(ctx->sector_size);
        if (!ctx->sec_buf) {
            SGL_LOG_ERROR(FAT_FS_NAME " mount: malloc sec_buf size=%d failed", ctx->sector_size);
            sgl_free(ctx);
            return FAT_ERR_NO_MEM;
        }
        if (fat_read_sector(ctx, 0, ctx->sec_buf) != 0) {
            SGL_LOG_ERROR(FAT_FS_NAME " mount: read sector 0 with size=%d failed", ctx->sector_size);
            sgl_free(ctx->sec_buf);
            sgl_free(ctx);
            return FAT_ERR_IO;
        }
        bpb = ctx->sec_buf;
    }

    uint16_t rsvd_sectors = rd16(&bpb[14]);
    ctx->fat_start = rsvd_sectors;
    ctx->root_dir_sectors = ((uint32_t)ctx->root_entry_count * 32 + ctx->sector_size - 1) / ctx->sector_size;
    ctx->root_dir_start = ctx->fat_start + (uint32_t)ctx->num_fats * ctx->fat_sectors;
    ctx->data_start = ctx->root_dir_start + ctx->root_dir_sectors;
    uint32_t data_sectors = ctx->total_sectors - ctx->data_start;
    ctx->total_clusters = data_sectors / ctx->cluster_size;

    if (ctx->total_clusters < 4085)      ctx->fat_type = FAT_TYPE_12;
    else if (ctx->total_clusters < 65525) ctx->fat_type = FAT_TYPE_16;
    else { ctx->fat_type = FAT_TYPE_32; ctx->fat32_root_cluster = rd32(&bpb[44]); }

    *fs = ctx;
    return FAT_OK;
}

static int fatfs_unmount(void *fs, const char *mount_point)
{
    (void)mount_point;
    fat_ctx_t *ctx = (fat_ctx_t *)fs;
    if (!ctx) return FAT_ERR_INVALID;
    fat_cache_invalidate(ctx);
    vol_dev_map[ctx->vol_id] = NULL;
    sgl_free(ctx->sec_buf);
    sgl_free(ctx);
    return FAT_OK;
}

static int fatfs_open(void *fs, const char *path, uint32_t flags)
{
    fat_ctx_t *ctx = (fat_ctx_t *)fs;
    int slot = -1;
    for (int i = 0; i < FAT_MAX_OPEN_FILES; i++)
        if (!ctx->files[i].used) { slot = i; break; }
    if (slot < 0) {
        SGL_LOG_ERROR(FAT_FS_NAME " open: too many open files");
        return FAT_ERR_TOO_MANY_OPEN;
    }

    uint32_t parent_dir;
    const char *fname;
    int ret = fat_resolve_path(ctx, path, &parent_dir, &fname);
    if (ret != FAT_OK) {
        SGL_LOG_ERROR(FAT_FS_NAME " open: resolve_path failed, ret=%d, path=%s", ret, path);
        return ret;
    }
    if (fname == NULL) {
        SGL_LOG_ERROR(FAT_FS_NAME " open: fname is NULL, path=%s", path);
        return FAT_ERR_INVALID;
    }

    fat_dir_entry_t entry;
    uint32_t entry_sec; uint16_t entry_off;
    int found = fat_find_in_dir(ctx, parent_dir, fname, &entry, &entry_sec, &entry_off);

    if (found == FAT_OK) {
        if (entry.attr & FAT_ATTR_DIRECTORY) {
            SGL_LOG_ERROR(FAT_FS_NAME " open: path is directory, path=%s", path);
            return FAT_ERR_NOT_DIR;
        }
        uint32_t start = ((uint32_t)entry.fst_clus_hi << 16) | entry.fst_clus_lo;
        if (flags & SGL_O_TRUNC) {
            if (start >= 2) fat_free_chain(ctx, start);
            start = 0;
            entry.fst_clus_lo = 0; entry.fst_clus_hi = 0; entry.file_size = 0;
            fat_write_dir_entry(ctx, entry_sec, entry_off, &entry);
        }
        fat_file_t *f = &ctx->files[slot];
        memset(f, 0, sizeof(fat_file_t));
        f->used = 1; f->dir_start_cluster = parent_dir;
        f->entry_sector = entry_sec; f->entry_offset = entry_off;
        f->start_cluster = start; f->cur_cluster = start;
        f->file_size = entry.file_size; f->flags = (uint8_t)(flags & 0xFF);
        if (flags & SGL_O_APPEND) {
            f->cur_pos = f->file_size;
            if (f->file_size > 0 && start >= 2) {
                uint32_t cb = (uint32_t)ctx->cluster_size * ctx->sector_size;
                uint32_t idx = f->file_size / cb;
                f->cur_cluster = fat_chain_get(ctx, start, idx);
                if (f->cur_cluster == 0) f->cur_cluster = start;
            }
        }
        return slot;
    }
    if (!(flags & SGL_O_CREAT)) {
        SGL_LOG_ERROR(FAT_FS_NAME " open: file not found and not CREAT, path=%s", path);
        return FAT_ERR_NOT_FOUND;
    }

    uint32_t nc = 0;
    if (flags & (SGL_O_WRONLY | SGL_O_RDWR)) {
        nc = fat_alloc_cluster(ctx);
        if (nc == 0) {
            SGL_LOG_ERROR(FAT_FS_NAME " open: alloc cluster failed for new file");
            return FAT_ERR_NO_SPACE;
        }
        uint32_t base = cluster_to_sector(ctx, nc);
        memset(ctx->sec_buf, 0, ctx->sector_size);
        for (uint8_t s = 0; s < ctx->cluster_size; s++)
            fat_write_sector(ctx, base + s, ctx->sec_buf);
    }

    uint32_t esec = 0; uint16_t eoff = 0;
    ret = fat_create_entry(ctx, parent_dir, fname, FAT_ATTR_ARCHIVE, nc, 0, &esec, &eoff);
    if (ret != FAT_OK) {
        SGL_LOG_ERROR(FAT_FS_NAME " open: create_entry failed, ret=%d", ret);
        if (nc >= 2) fat_free_chain(ctx, nc);
        return ret;
    }

    fat_file_t *f = &ctx->files[slot];
    memset(f, 0, sizeof(fat_file_t));
    f->used = 1; f->dir_start_cluster = parent_dir;
    f->entry_sector = esec; f->entry_offset = eoff;
    f->start_cluster = nc; f->cur_cluster = nc;
    f->flags = (uint8_t)(flags & 0xFF);
    return slot;
}

static int fatfs_close(void *fs, int fd)
{
    fat_ctx_t *ctx = (fat_ctx_t *)fs;
    if (fd < 0 || fd >= FAT_MAX_OPEN_FILES || !ctx->files[fd].used) return FAT_ERR_INVALID;
    fat_file_t *f = &ctx->files[fd];
    if (f->dirty || (f->flags & (SGL_O_WRONLY | SGL_O_RDWR))) {
        fat_dir_entry_t entry;
        if (fat_cache_read(ctx, f->entry_sector) == FAT_OK) {
            memcpy(&entry, &ctx->sec_buf[f->entry_offset], FAT_DIR_ENTRY_SIZE);
            entry.fst_clus_lo = (uint16_t)(f->start_cluster & 0xFFFF);
            entry.fst_clus_hi = (uint16_t)((f->start_cluster >> 16) & 0xFFFF);
            entry.file_size = f->file_size;
            fat_write_dir_entry(ctx, f->entry_sector, f->entry_offset, &entry);
        }
    }
    f->used = 0;
    fat_cache_flush(ctx);
    return FAT_OK;
}

static int fatfs_read(void *fs, int fd, void *buffer, uint32_t count)
{
    fat_ctx_t *ctx = (fat_ctx_t *)fs;
    if (fd < 0 || fd >= FAT_MAX_OPEN_FILES || !ctx->files[fd].used) return FAT_ERR_INVALID;
    fat_file_t *f = &ctx->files[fd];
    /* SGL_O_RDONLY is 0, so check if NOT write-only */
    if ((f->flags & 0x03) == SGL_O_WRONLY) {
        SGL_LOG_ERROR(FAT_FS_NAME " read: file opened as write-only");
        return FAT_ERR_DENIED;
    }
    if (f->start_cluster < 2) return 0;
    if (f->cur_pos + count > f->file_size) {
        if (f->cur_pos >= f->file_size) return 0;
        count = f->file_size - f->cur_pos;
    }
    if (count == 0) return 0;
    uint32_t cb = (uint32_t)ctx->cluster_size * ctx->sector_size;
    uint8_t *dst = (uint8_t *)buffer;
    uint32_t br = 0;
    if (f->cur_cluster < 2) f->cur_cluster = f->start_cluster;
    while (br < count) {
        uint32_t oc = f->cur_pos % cb;
        uint32_t sector = cluster_to_sector(ctx, f->cur_cluster) + oc / ctx->sector_size;
        uint16_t os = (uint16_t)(oc % ctx->sector_size);
        if (fat_cache_read(ctx, sector) != FAT_OK) break;
        uint32_t avail = ctx->sector_size - os;
        uint32_t tc = (count - br < avail) ? count - br : avail;
        memcpy(dst + br, &ctx->sec_buf[os], tc);
        br += tc; f->cur_pos += tc;
        if (f->cur_pos % cb == 0 && br < count) {
            uint32_t next = fat_get_entry(ctx, f->cur_cluster);
            if (next < 2 || is_eoc(ctx, next)) break;
            f->cur_cluster = next;
        }
    }
    return (int)br;
}

static int fatfs_write(void *fs, int fd, const void *buffer, uint32_t count)
{
    fat_ctx_t *ctx = (fat_ctx_t *)fs;
    if (fd < 0 || fd >= FAT_MAX_OPEN_FILES || !ctx->files[fd].used) return FAT_ERR_INVALID;
    fat_file_t *f = &ctx->files[fd];
    /* Check if file has write permission (WRONLY or RDWR) */
    if ((f->flags & 0x03) == SGL_O_RDONLY) {
        SGL_LOG_ERROR(FAT_FS_NAME " write: file opened as read-only");
        return FAT_ERR_DENIED;
    }

    if (count == 0) {
        return 0;
    }

    uint32_t cb = (uint32_t)ctx->cluster_size * ctx->sector_size;
    const uint8_t *src = (const uint8_t *)buffer;
    uint32_t bw = 0;

    if (f->start_cluster < 2) {
        f->start_cluster = fat_alloc_cluster(ctx);
        if (f->start_cluster == 0) return FAT_ERR_NO_SPACE;
        f->cur_cluster = f->start_cluster;
        uint32_t base = cluster_to_sector(ctx, f->cur_cluster);
        memset(ctx->sec_buf, 0, ctx->sector_size);
        for (uint8_t s = 0; s < ctx->cluster_size; s++)
            fat_write_sector(ctx, base + s, ctx->sec_buf);
    }

    if (f->cur_cluster < 2) {
        f->cur_cluster = f->start_cluster;
    }

    while (bw < count) {
        uint32_t oc = f->cur_pos % cb;
        uint32_t sector = cluster_to_sector(ctx, f->cur_cluster) + oc / ctx->sector_size;
        uint16_t os = (uint16_t)(oc % ctx->sector_size);
        uint32_t avail = ctx->sector_size - os;
        uint32_t tw = (count - bw < avail) ? count - bw : avail;
        if (tw < ctx->sector_size) { if (fat_cache_read(ctx, sector) != FAT_OK) break; }
        memcpy(&ctx->sec_buf[os], src + bw, tw);
        fat_cache_dirty(ctx); fat_cache_flush(ctx);
        bw += tw; f->cur_pos += tw;

        if (f->cur_pos > f->file_size) {
            f->file_size = f->cur_pos;
            f->dirty = 1; 
        }

        if (f->cur_pos % cb == 0 && bw < count) {
            uint32_t next = fat_get_entry(ctx, f->cur_cluster);
            if (next < 2 || is_eoc(ctx, next)) {
                next = fat_alloc_cluster(ctx);
                if (next == 0) break;
                fat_set_entry(ctx, f->cur_cluster, next);
                uint32_t base = cluster_to_sector(ctx, next);
                memset(ctx->sec_buf, 0, ctx->sector_size);
                for (uint8_t s = 0; s < ctx->cluster_size; s++)
                    fat_write_sector(ctx, base + s, ctx->sec_buf);
            }
            f->cur_cluster = next;
        }
    }
    return (int)bw;
}

static int fatfs_opendir(void *fs, const char *path, int *dd)
{
    fat_ctx_t *ctx = (fat_ctx_t *)fs;
    int slot = -1;
    for (int i = 0; i < FAT_MAX_OPEN_DIRS; i++)
        if (!ctx->dirs[i].used) { slot = i; break; }
    if (slot < 0) {
        SGL_LOG_ERROR(FAT_FS_NAME " opendir: too many open dirs");
        return FAT_ERR_TOO_MANY_OPEN;
    }

    uint32_t parent_dir; const char *fname;
    int ret = fat_resolve_path(ctx, path, &parent_dir, &fname);
    if (ret != FAT_OK) {
        SGL_LOG_ERROR(FAT_FS_NAME " opendir: resolve_path failed, ret=%d", ret);
        return ret;
    }

    uint32_t dir_cluster;
    if (fname == NULL) { dir_cluster = parent_dir; }
    else {
        fat_dir_entry_t entry;
        ret = fat_find_in_dir(ctx, parent_dir, fname, &entry, NULL, NULL);
        if (ret != FAT_OK) {
            SGL_LOG_ERROR(FAT_FS_NAME " opendir: find_in_dir failed, ret=%d", ret);
            return ret;
        }
        if (!(entry.attr & FAT_ATTR_DIRECTORY)) {
            SGL_LOG_ERROR(FAT_FS_NAME " opendir: not a directory");
            return FAT_ERR_NOT_DIR;
        }
        dir_cluster = ((uint32_t)entry.fst_clus_hi << 16) | entry.fst_clus_lo;
        if (dir_cluster == 0) dir_cluster = root_cluster(ctx);
    }
    fat_dir_t *d = &ctx->dirs[slot];
    d->used = 1; d->start_cluster = dir_cluster;
    d->cur_cluster = dir_cluster; d->entry_index = 0; d->lfn_valid = 0; d->lfn_name[0] = '\0'; d->finished = 0;
    *dd = slot;
    return FAT_OK;
}

static int fatfs_readdir(void *fs, int dd, char *name, uint32_t name_size, uint32_t *type)
{
    fat_ctx_t *ctx = (fat_ctx_t *)fs;
    if (dd < 0 || dd >= FAT_MAX_OPEN_DIRS || !ctx->dirs[dd].used) return FAT_ERR_INVALID;
    fat_dir_t *d = &ctx->dirs[dd];
    if (d->finished) return 0;
    uint32_t dc = d->start_cluster;
    uint8_t is_root = (ctx->fat_type != FAT_TYPE_32 && dc == 0);

    while (1) {
        fat_dir_entry_t entry;
        int ret = fat_read_dir_entry(ctx, is_root ? 0 : dc, d->entry_index, &entry, NULL, NULL);
        if (ret != FAT_OK) { d->finished = 1; return 0; }
        if (entry.name[0] == FAT_ENTRY_END) { d->finished = 1; return 0; }
        if (entry.name[0] == FAT_ENTRY_FREE) {
            d->entry_index++;
            d->lfn_valid = 0;
            continue;
        }
        if (entry.attr & FAT_ATTR_LONG_NAME) {
            uint32_t next_index = 0;
            if (fat_collect_lfn_name(ctx, is_root ? 0 : dc, d->entry_index, d->lfn_name, sizeof(d->lfn_name), &next_index) == FAT_OK) {
                d->lfn_valid = 1;
                d->entry_index = next_index;
                continue;
            }
            d->entry_index++;
            d->lfn_valid = 0;
            continue;
        }
        d->entry_index++;
        if (entry.attr & FAT_ATTR_VOLUME_ID) continue;
        if (name && name_size > 0) {
            if (d->lfn_valid) {
                strncpy(name, d->lfn_name, name_size - 1);
                name[name_size - 1] = '\0';
                d->lfn_valid = 0;
            } else
            {
                name_from_83(entry.name, name, name_size);
            }
        }
        if (type) *type = (entry.attr & FAT_ATTR_DIRECTORY) ? SGL_S_IFDIR : SGL_S_IFREG;
        return 1;
    }
}

static int fatfs_closedir(void *fs, int dd)
{
    fat_ctx_t *ctx = (fat_ctx_t *)fs;
    if (dd < 0 || dd >= FAT_MAX_OPEN_DIRS || !ctx->dirs[dd].used) return FAT_ERR_INVALID;
    ctx->dirs[dd].used = 0;
    return FAT_OK;
}

static int fatfs_sync(void *fs)
{
    fat_ctx_t *ctx = (fat_ctx_t *)fs;
    fat_cache_flush(ctx);
    for (int i = 0; i < FAT_MAX_OPEN_FILES; i++) {
        fat_file_t *f = &ctx->files[i];
        if (f->used && (f->dirty || (f->flags & (SGL_O_WRONLY | SGL_O_RDWR)))) {
            fat_dir_entry_t entry;
            if (fat_cache_read(ctx, f->entry_sector) == FAT_OK) {
                memcpy(&entry, &ctx->sec_buf[f->entry_offset], FAT_DIR_ENTRY_SIZE);
                entry.fst_clus_lo = (uint16_t)(f->start_cluster & 0xFFFF);
                entry.fst_clus_hi = (uint16_t)((f->start_cluster >> 16) & 0xFFFF);
                entry.file_size = f->file_size;
                fat_write_dir_entry(ctx, f->entry_sector, f->entry_offset, &entry);
            }
            f->dirty = 0;
        }
    }
    sgl_block_dev_ioctl(ctx->dev, SGL_BLK_CTRL_SYNC, NULL);
    return FAT_OK;
}

static int fatfs_format(void *fs)
{
    fat_ctx_t *ctx = (fat_ctx_t *)fs;
    if (!ctx) return FAT_ERR_INVALID;
    uint32_t total_sec = 0;
    if (sgl_block_dev_ioctl(ctx->dev, SGL_BLK_GET_SECTOR_COUNT, &total_sec) != 0 || total_sec == 0) {
        total_sec = ctx->total_sectors;
    }

    uint8_t *buf = ctx->sec_buf;
    memset(buf, 0, ctx->sector_size);
    buf[0] = 0xEB; buf[1] = 0x3C; buf[2] = 0x90;
    memcpy(&buf[3], "SGLFAT  ", 8);
    wr16(&buf[11], ctx->sector_size);
    uint8_t spc = 1;
    if (total_sec > 32768) spc = 8; else if (total_sec > 8192) spc = 4;
    buf[13] = spc;
    uint16_t rsvd = 1;
    wr16(&buf[14], rsvd); buf[16] = 2;
    uint16_t root_ent = 512; wr16(&buf[17], root_ent);
    if (total_sec <= 0xFFFF) wr16(&buf[19], (uint16_t)total_sec);
    else { wr16(&buf[19], 0); wr32(&buf[32], total_sec); }
    buf[21] = 0xF8;
    uint32_t root_sec = ((uint32_t)root_ent * 32 + ctx->sector_size - 1) / ctx->sector_size;
    uint32_t data_sec = total_sec - rsvd - 2 * 1 - root_sec;  /* total - rsvd - 2*fat - root */
    /* Recalculate fat_sec: each cluster needs 2 bytes in FAT16, 2 FAT copies */
    uint32_t fat_sec = ((data_sec / spc) * 2 + ctx->sector_size - 1) / ctx->sector_size;
    if (fat_sec == 0) fat_sec = 1;
    data_sec = total_sec - rsvd - 2 * fat_sec - root_sec;  /* actual data sectors */
    wr16(&buf[22], (uint16_t)fat_sec);
    wr16(&buf[24], 63); wr16(&buf[26], 255);
    buf[510] = 0x55; buf[511] = 0xAA;

    fat_cache_invalidate(ctx);
    if (fat_write_sector(ctx, 0, buf) != 0) return FAT_ERR_IO;

    memset(buf, 0, ctx->sector_size);
    wr16(&buf[0], 0xFFF8); wr16(&buf[2], 0xFFFF);
    for (uint8_t f = 0; f < 2; f++) {
        uint32_t base = rsvd + (uint32_t)f * fat_sec;
        fat_write_sector(ctx, base, buf);
        memset(buf, 0, ctx->sector_size);
        for (uint32_t s = 1; s < fat_sec; s++) fat_write_sector(ctx, base + s, buf);
    }
    uint32_t root_start = rsvd + 2 * fat_sec;
    memset(buf, 0, ctx->sector_size);
    for (uint32_t s = 0; s < root_sec; s++) fat_write_sector(ctx, root_start + s, buf);

    /* Update in-memory context to match the newly formatted filesystem */
    ctx->cluster_size = spc;
    ctx->num_fats = 2;
    ctx->root_entry_count = root_ent;
    ctx->total_sectors = total_sec;
    ctx->fat_sectors = fat_sec;
    ctx->fat_start = rsvd;
    ctx->root_dir_sectors = root_sec;
    ctx->root_dir_start = root_start;
    ctx->data_start = root_start + root_sec;
    ctx->total_clusters = data_sec / spc;
    ctx->fat_type = (ctx->total_clusters < 4085) ? FAT_TYPE_12 :
                    (ctx->total_clusters < 65525) ? FAT_TYPE_16 : FAT_TYPE_32;
    ctx->fat32_root_cluster = 0;

    return FAT_OK;
}

static int fatfs_remove(void *fs, const char *path)
{
    fat_ctx_t *ctx = (fat_ctx_t *)fs;
    uint32_t parent_dir; const char *fname;
    int ret = fat_resolve_path(ctx, path, &parent_dir, &fname);
    if (ret != FAT_OK) {
        SGL_LOG_ERROR(FAT_FS_NAME " remove: resolve_path failed, ret=%d", ret);
        return ret;
    }
    if (fname == NULL) {
        SGL_LOG_ERROR(FAT_FS_NAME " remove: fname is NULL");
        return FAT_ERR_INVALID;
    }

    fat_dir_entry_t entry; uint32_t esec; uint16_t eoff;
    ret = fat_find_in_dir(ctx, parent_dir, fname, &entry, &esec, &eoff);
    if (ret != FAT_OK) {
        SGL_LOG_ERROR(FAT_FS_NAME " remove: find_in_dir failed, ret=%d", ret);
        return ret;
    }

    if (entry.attr & FAT_ATTR_DIRECTORY) {
        uint32_t dc = ((uint32_t)entry.fst_clus_hi << 16) | entry.fst_clus_lo;
        if (dc >= 2 && !fat_dir_is_empty(ctx, dc)) {
            SGL_LOG_ERROR(FAT_FS_NAME " remove: directory not empty");
            return FAT_ERR_NOT_EMPTY;
        }
    }
    uint32_t start = ((uint32_t)entry.fst_clus_hi << 16) | entry.fst_clus_lo;
    if (start >= 2) fat_free_chain(ctx, start);
    entry.name[0] = FAT_ENTRY_FREE;
    fat_write_dir_entry(ctx, esec, eoff, &entry);
    return FAT_OK;
}

static int fatfs_mkdir(void *fs, const char *path)
{
    fat_ctx_t *ctx = (fat_ctx_t *)fs;
    uint32_t parent_dir; const char *fname;
    int ret = fat_resolve_path(ctx, path, &parent_dir, &fname);
    if (ret != FAT_OK) {
        SGL_LOG_ERROR(FAT_FS_NAME " mkdir: resolve_path failed, ret=%d", ret);
        return ret;
    }
    if (fname == NULL) {
        SGL_LOG_ERROR(FAT_FS_NAME " mkdir: fname is NULL");
        return FAT_ERR_INVALID;
    }

    fat_dir_entry_t existing;
    if (fat_find_in_dir(ctx, parent_dir, fname, &existing, NULL, NULL) == FAT_OK) {
        SGL_LOG_ERROR(FAT_FS_NAME " mkdir: file already exists");
        return FAT_ERR_EXIST;
    }

    uint32_t nc = fat_alloc_cluster(ctx);
    if (nc == 0) {
        SGL_LOG_ERROR(FAT_FS_NAME " mkdir: alloc cluster failed");
        return FAT_ERR_NO_SPACE;
    }
    uint32_t base = cluster_to_sector(ctx, nc);
    memset(ctx->sec_buf, 0, ctx->sector_size);
    for (uint8_t s = 0; s < ctx->cluster_size; s++)
        fat_write_sector(ctx, base + s, ctx->sec_buf);

    fat_dir_entry_t dot;
    memset(&dot, 0, sizeof(dot));
    memset(dot.name, ' ', 11); dot.name[0] = '.';
    dot.attr = FAT_ATTR_DIRECTORY;
    dot.fst_clus_lo = (uint16_t)(nc & 0xFFFF);
    dot.fst_clus_hi = (uint16_t)((nc >> 16) & 0xFFFF);
    fat_write_dir_entry(ctx, base, 0, &dot);

    memset(&dot, 0, sizeof(dot));
    memset(dot.name, ' ', 11); dot.name[0] = '.'; dot.name[1] = '.';
    dot.attr = FAT_ATTR_DIRECTORY;
    uint32_t pc = parent_dir;
    if (ctx->fat_type != FAT_TYPE_32 && pc == root_cluster(ctx)) pc = 0;
    dot.fst_clus_lo = (uint16_t)(pc & 0xFFFF);
    dot.fst_clus_hi = (uint16_t)((pc >> 16) & 0xFFFF);
    fat_write_dir_entry(ctx, base, FAT_DIR_ENTRY_SIZE, &dot);

    uint32_t esec = 0; uint16_t eoff = 0;
    ret = fat_create_entry(ctx, parent_dir, fname, FAT_ATTR_DIRECTORY, nc, 0, &esec, &eoff);
    if (ret != FAT_OK) {
        SGL_LOG_ERROR(FAT_FS_NAME " mkdir: create_entry failed, ret=%d", ret);
        fat_free_chain(ctx, nc);
        return ret;
    }
    return FAT_OK;
}

static int fatfs_stat(void *fs, const char *path, sgl_stat_t *st)
{
    fat_ctx_t *ctx = (fat_ctx_t *)fs;
    if (!st) {
        SGL_LOG_ERROR(FAT_FS_NAME " stat: st is NULL");
        return FAT_ERR_INVALID;
    }
    uint32_t parent_dir; const char *fname;
    int ret = fat_resolve_path(ctx, path, &parent_dir, &fname);
    if (ret != FAT_OK) {
        SGL_LOG_ERROR(FAT_FS_NAME " stat: resolve_path failed, ret=%d", ret);
        return ret;
    }
    if (fname == NULL) {
        memset(st, 0, sizeof(sgl_stat_t));
        st->st_mode = SGL_S_IFDIR | SGL_S_IRWXU | SGL_S_IRWXG | SGL_S_IRWXO;
        return FAT_OK;
    }
    fat_dir_entry_t entry;
    ret = fat_find_in_dir(ctx, parent_dir, fname, &entry, NULL, NULL);
    if (ret != FAT_OK) {
        SGL_LOG_ERROR(FAT_FS_NAME " stat: find_in_dir failed, ret=%d", ret);
        return ret;
    }
    st->st_size = entry.file_size;
    st->st_mode = (entry.attr & FAT_ATTR_DIRECTORY) ? SGL_S_IFDIR : SGL_S_IFREG;
    st->st_mode |= SGL_S_IRWXU | SGL_S_IRWXG | SGL_S_IRWXO;
    st->st_mtime = 0;
    return FAT_OK;
}

static int fatfs_rename(void *fs, const char *old_path, const char *new_path)
{
    fat_ctx_t *ctx = (fat_ctx_t *)fs;
    uint32_t old_parent; const char *old_name;
    int ret = fat_resolve_path(ctx, old_path, &old_parent, &old_name);
    if (ret != FAT_OK) {
        SGL_LOG_ERROR(FAT_FS_NAME " rename: resolve old_path failed, ret=%d", ret);
        return ret;
    }
    if (old_name == NULL) {
        SGL_LOG_ERROR(FAT_FS_NAME " rename: old_name is NULL");
        return FAT_ERR_INVALID;
    }

    fat_dir_entry_t entry; uint32_t old_sec; uint16_t old_off;
    ret = fat_find_in_dir(ctx, old_parent, old_name, &entry, &old_sec, &old_off);
    if (ret != FAT_OK) {
        SGL_LOG_ERROR(FAT_FS_NAME " rename: find old entry failed, ret=%d", ret);
        return ret;
    }

    uint32_t new_parent; const char *new_name;
    ret = fat_resolve_path(ctx, new_path, &new_parent, &new_name);
    if (ret != FAT_OK) {
        SGL_LOG_ERROR(FAT_FS_NAME " rename: resolve new_path failed, ret=%d", ret);
        return ret;
    }
    if (new_name == NULL) {
        SGL_LOG_ERROR(FAT_FS_NAME " rename: new_name is NULL");
        return FAT_ERR_INVALID;
    }

    fat_dir_entry_t existing; uint32_t ex_sec; uint16_t ex_off;
    if (fat_find_in_dir(ctx, new_parent, new_name, &existing, &ex_sec, &ex_off) == FAT_OK) {
        uint32_t ec = ((uint32_t)existing.fst_clus_hi << 16) | existing.fst_clus_lo;
        if (ec >= 2) fat_free_chain(ctx, ec);
        existing.name[0] = FAT_ENTRY_FREE;
        fat_write_dir_entry(ctx, ex_sec, ex_off, &existing);
    }
    uint32_t start = ((uint32_t)entry.fst_clus_hi << 16) | entry.fst_clus_lo;
    uint32_t ns; uint16_t no;
    ret = fat_create_entry(ctx, new_parent, new_name, entry.attr, start, entry.file_size, &ns, &no);
    if (ret != FAT_OK) {
        SGL_LOG_ERROR(FAT_FS_NAME " rename: create_entry failed, ret=%d", ret);
        return ret;
    }
    entry.name[0] = FAT_ENTRY_FREE;
    fat_write_dir_entry(ctx, old_sec, old_off, &entry);
    return FAT_OK;
}

static int fatfs_statvfs(void *fs, sgl_statvfs_t *info)
{
    fat_ctx_t *ctx = (fat_ctx_t *)fs;
    if (!ctx || !info) return FAT_ERR_INVALID;

    memset(info, 0, sizeof(*info));

    /* Block size = cluster size in bytes */
    info->f_bsize = (uint32_t)ctx->cluster_size * ctx->sector_size;
    info->f_blocks = ctx->total_clusters;

    /* Count free clusters by scanning the FAT */
    uint32_t free_count = 0;
    uint32_t bad_count = 0;
    for (uint32_t c = 2; c < ctx->total_clusters + 2; c++) {
        uint32_t entry = fat_get_entry(ctx, c);
        if (entry == 0) {
            free_count++;
        } else if (entry == 0xFFFFFFF7 || entry == 0xFFF7) {
            bad_count++;
        }
    }

    info->f_bfree = free_count;
    info->f_bavail = free_count;
    /* FAT doesn't track inode counts */
    info->f_files = 0;
    info->f_ffree = 0;

    return FAT_OK;
}

static sgl_fs_ops_t fatfs_ops = {
    .mount    = fatfs_mount,
    .unmount  = fatfs_unmount,
    .open     = fatfs_open,
    .close    = fatfs_close,
    .read     = fatfs_read,
    .write    = fatfs_write,
    .opendir  = fatfs_opendir,
    .readdir  = fatfs_readdir,
    .closedir = fatfs_closedir,
    .sync     = fatfs_sync,
    .format   = fatfs_format,
    .remove   = fatfs_remove,
    .mkdir    = fatfs_mkdir,
    .stat     = fatfs_stat,
    .rename   = fatfs_rename,
    .statvfs  = fatfs_statvfs,
};

static sgl_fs_type_t fatfs_type = {
    .name = FAT_FS_NAME,
    .ops  = &fatfs_ops,
};

/**
 * @brief Register FatFS file system type with sgl_fs VFS.
 *        Call this once before sgl_fs_mount("fatfs", ...).
 * @return 0 on success, -1 on failure
 */
int sgl_fatfs_register(void)
{
    memset(vol_dev_map, 0, sizeof(vol_dev_map));
    return sgl_fs_register(&fatfs_type);
}
