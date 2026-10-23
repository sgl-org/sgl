/* source/fs/kvfs/sgl_kvfs.c
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

#include "sgl_kvfs.h"
#include <string.h>
#include <sgl_mm.h>

#define KV_OK                   SGL_FS_OK
#define KV_ERR                  SGL_FS_ERROR
#define KV_ERR_NOT_FOUND        SGL_FS_NOT_FOUND
#define KV_ERR_DENIED           SGL_FS_PERMISSION_DENIED
#define KV_ERR_IO               SGL_FS_IO_ERROR
#define KV_ERR_NO_MEM           SGL_FS_NO_MEMORY
#define KV_ERR_NO_SPACE         SGL_FS_NO_SPACE
#define KV_ERR_EXIST            SGL_FS_EXISTS
#define KV_ERR_INVALID          SGL_FS_INVALID_ARGUMENT
#define KV_ERR_BADF             SGL_FS_INVALID_ARGUMENT

#define KV_FS_NAME              "kvfs"
#define KV_LOG_TAG              "kvfs: "

#define KV_MAGIC                0x4B564631u /* "1KVF" little-endian */
#define KV_VERSION              1u
#define KV_SB_SIZE              12          /* magic + version + crc */
#define KV_REC_HDR_SIZE         12
#define KV_REC_DATA             1u
#define KV_REC_DEL              2u

#define KV_MAX_OPEN_FILES       8
#define KV_MAX_OPEN_DIRS        4

#define KV_ALIGN4(x)            (((x) + 3u) & ~3u)

typedef struct kv_entry {
    struct kv_entry *next;
    char key[SGL_KVFS_MAX_KEY_LEN + 1];
    uint32_t off;       /* byte offset of the record inside the log */
    uint32_t vlen;      /* live value length */
} kv_entry_t;

typedef struct {
    uint8_t used;
    uint32_t pos;
    uint32_t flags;
    char key[SGL_KVFS_MAX_KEY_LEN + 1];
    uint8_t *wbuf;      /* whole-value staging buffer, committed on close */
    uint32_t wlen;
    uint32_t wcap;
    uint8_t dirty;
} kv_fd_t;

typedef struct {
    uint8_t used;
    uint32_t index;
} kv_dd_t;

/* Private instance type: a kvfs volume is only reachable through the
 * sgl_fs VFS mount table, never handed out to the caller. */
typedef struct sgl_kvfs sgl_kvfs_t;

struct sgl_kvfs {
    sgl_block_dev_t *dev;
    uint32_t sector_size;
    uint32_t sb_sector;           /* superblock sector */
    uint32_t vol_sectors;         /* sectors owned, 0 = till device end */
    uint32_t data_start_sector;
    uint32_t data_sectors;
    uint32_t log_bytes;           /* data_sectors * sector_size */
    uint32_t wr_off;              /* append cursor, always 4-byte aligned */
    kv_entry_t *entries;          /* live index, insertion order */
    uint32_t entry_count;
    uint8_t *sec_buf;             /* one sector, RMW staging */
    kv_fd_t files[KV_MAX_OPEN_FILES];
    kv_dd_t dirs[KV_MAX_OPEN_DIRS];
};

static void kv_put16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
}

static void kv_put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
    p[2] = (uint8_t)((v >> 16) & 0xFF);
    p[3] = (uint8_t)((v >> 24) & 0xFF);
}

static uint16_t kv_get16(const uint8_t *p)
{
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t kv_get32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* Resumable IEEE CRC32: start with crc = 0, feed parts, final value as-is */
static uint32_t kv_crc32(uint32_t crc, const uint8_t *data, uint32_t len)
{
    crc = ~crc;
    while (len--) {
        crc ^= *data++;
        for (int i = 0; i < 8; i++) {
            crc = (crc >> 1) ^ (0xEDB88320u & (uint32_t)(-(int32_t)(crc & 1)));
        }
    }
    return ~crc;
}

/* Read bytes from the record log (byte-addressed, sector-at-a-time) */
static int kv_pread(sgl_kvfs_t *kv, uint32_t boff, void *buf, uint32_t len)
{
    uint8_t *dst = (uint8_t *)buf;
    uint32_t ssz = kv->sector_size;

    if (boff + len > kv->log_bytes) return KV_ERR_INVALID;
    while (len > 0) {
        uint32_t sector = kv->data_start_sector + boff / ssz;
        uint32_t soff = boff % ssz;
        uint32_t n = ssz - soff;
        if (n > len) n = len;
        if (kv->dev->read_sectors(kv->dev, sector, kv->sec_buf, 1) != 0) return KV_ERR_IO;
        memcpy(dst, kv->sec_buf + soff, n);
        dst += n;
        boff += n;
        len -= n;
    }
    return KV_OK;
}

/* Write bytes to the record log with per-sector read-modify-write */
static int kv_pwrite(sgl_kvfs_t *kv, uint32_t boff, const void *buf, uint32_t len)
{
    const uint8_t *src = (const uint8_t *)buf;
    uint32_t ssz = kv->sector_size;

    if (boff + len > kv->log_bytes) return KV_ERR_NO_SPACE;
    while (len > 0) {
        uint32_t sector = kv->data_start_sector + boff / ssz;
        uint32_t soff = boff % ssz;
        uint32_t n = ssz - soff;
        if (n > len) n = len;
        if (kv->dev->read_sectors(kv->dev, sector, kv->sec_buf, 1) != 0) return KV_ERR_IO;
        memcpy(kv->sec_buf + soff, src, n);
        if (kv->dev->write_sectors(kv->dev, sector, kv->sec_buf, 1) != 0) return KV_ERR_IO;
        src += n;
        boff += n;
        len -= n;
    }
    return KV_OK;
}

static int kv_erase_log(sgl_kvfs_t *kv)
{
    memset(kv->sec_buf, 0xFF, kv->sector_size);
    for (uint32_t i = 0; i < kv->data_sectors; i++) {
        if (kv->dev->write_sectors(kv->dev, kv->data_start_sector + i, kv->sec_buf, 1) != 0)
            return KV_ERR_IO;
    }
    return KV_OK;
}

static int kv_key_valid(const char *key)
{
    if (!key || key[0] == '\0') return 0;
    size_t len = strlen(key);
    if (len > SGL_KVFS_MAX_KEY_LEN) return 0;
    for (size_t i = 0; i < len; i++) {
        uint8_t c = (uint8_t)key[i];
        if (c <= 0x20 || c == '/') return 0;   /* no '/' and no control/space */
    }
    return 1;
}

static kv_entry_t *kv_find(sgl_kvfs_t *kv, const char *key)
{
    for (kv_entry_t *e = kv->entries; e; e = e->next) {
        if (strcmp(e->key, key) == 0) return e;
    }
    return NULL;
}

static void kv_clear_index(sgl_kvfs_t *kv)
{
    kv_entry_t *e = kv->entries;
    while (e) {
        kv_entry_t *next = e->next;
        sgl_free(e);
        e = next;
    }
    kv->entries = NULL;
    kv->entry_count = 0;
}

/* Append or refresh an index node; keeps insertion order for existing keys */
static kv_entry_t *kv_touch_entry(sgl_kvfs_t *kv, const char *key, uint32_t off, uint32_t vlen)
{
    kv_entry_t *e = kv_find(kv, key);
    if (e) {
        e->off = off;
        e->vlen = vlen;
        return e;
    }
    e = (kv_entry_t *)sgl_malloc(sizeof(kv_entry_t));
    if (!e) return NULL;
    strcpy(e->key, key);
    e->off = off;
    e->vlen = vlen;
    e->next = NULL;
    if (!kv->entries) {
        kv->entries = e;
    }
    else {
        kv_entry_t *tail = kv->entries;
        while (tail->next) tail = tail->next;
        tail->next = e;
    }
    kv->entry_count++;
    return e;
}

static void kv_remove_entry(sgl_kvfs_t *kv, const char *key)
{
    kv_entry_t **link = &kv->entries;
    while (*link) {
        if (strcmp((*link)->key, key) == 0) {
            kv_entry_t *dead = *link;
            *link = dead->next;
            sgl_free(dead);
            kv->entry_count--;
            return;
        }
        link = &(*link)->next;
    }
}

static int kv_scan_log(sgl_kvfs_t *kv)
{
    uint8_t hdr[KV_REC_HDR_SIZE];
    uint32_t pos = 0;

    kv_clear_index(kv);
    while (pos + KV_REC_HDR_SIZE <= kv->log_bytes) {
        if (kv_pread(kv, pos, hdr, sizeof(hdr)) != KV_OK) return KV_ERR_IO;
        uint8_t type = hdr[0];
        if (type == 0xFF) break;             /* first unwritten byte: end of log */
        if (type != KV_REC_DATA && type != KV_REC_DEL) break;  /* torn record */

        uint32_t klen = hdr[1];
        uint32_t vlen = kv_get16(hdr + 2);
        uint32_t crc = kv_get32(hdr + 4);
        if (klen == 0 || klen > SGL_KVFS_MAX_KEY_LEN) break;
        uint32_t body = klen + vlen;
        if (pos + KV_REC_HDR_SIZE + body > kv->log_bytes) break;

        uint8_t *buf = (uint8_t *)sgl_malloc(body ? body : 1);
        if (!buf) return KV_ERR_NO_MEM;
        int err = kv_pread(kv, pos + KV_REC_HDR_SIZE, buf, body);
        if (err != KV_OK) { sgl_free(buf); return err; }

        uint32_t calc = kv_crc32(0, buf, klen);
        calc = kv_crc32(calc, buf + klen, vlen);
        if (calc != crc) {                   /* bad tail from an interrupted write */
            sgl_free(buf);
            break;
        }

        char key[SGL_KVFS_MAX_KEY_LEN + 1];
        memcpy(key, buf, klen);
        key[klen] = '\0';
        if (type == KV_REC_DATA) {
            if (!kv_touch_entry(kv, key, pos, vlen)) { sgl_free(buf); return KV_ERR_NO_MEM; }
        }
        else {
            kv_remove_entry(kv, key);
        }
        sgl_free(buf);
        pos += KV_ALIGN4(KV_REC_HDR_SIZE + body);
    }
    kv->wr_off = pos;
    return KV_OK;
}

/* Total bytes the log would hold after compaction */
static uint32_t kv_live_bytes(sgl_kvfs_t *kv)
{
    uint32_t total = 0;
    for (kv_entry_t *e = kv->entries; e; e = e->next) {
        size_t klen = strlen(e->key);
        total += KV_ALIGN4(KV_REC_HDR_SIZE + (uint32_t)klen + e->vlen);
    }
    return total;
}

/*
 * Compact the log: copy every live record verbatim into a RAM blob, erase
 * the data area, then rewrite the blob back to back. Called when a new
 * record does not fit anymore.
 */
static int kv_gc(sgl_kvfs_t *kv)
{
    uint32_t live = kv_live_bytes(kv);
    if (live >= kv->log_bytes) return KV_ERR_NO_SPACE;

    uint8_t *blob = (uint8_t *)sgl_malloc(live ? live : 1);
    if (!blob) return KV_ERR_NO_MEM;

    /* Pass 1: snapshot live records (with their original crc/header bytes) */
    uint32_t boff = 0;
    for (kv_entry_t *e = kv->entries; e; e = e->next) {
        uint32_t klen = (uint32_t)strlen(e->key);
        uint32_t rsize = KV_REC_HDR_SIZE + klen + e->vlen;
        if (kv_pread(kv, e->off, blob + boff, rsize) != KV_OK) {
            sgl_free(blob);
            /* Earlier entries already point into the (freed) blob layout: */
            kv_scan_log(kv);
            return KV_ERR_IO;
        }
        e->off = boff;   /* provisional: position inside the blob */
        boff += rsize;
    }

    /* Erase and pass 2: rewrite with fixed-up offsets */
    int err = kv_erase_log(kv);
    if (err == KV_OK) {
        uint32_t src = 0;
        uint32_t dst = 0;
        for (kv_entry_t *e = kv->entries; e; e = e->next) {
            uint32_t klen = (uint32_t)strlen(e->key);
            uint32_t rsize = KV_REC_HDR_SIZE + klen + e->vlen;
            /* fix the crc field? crc covers key+value only, header stays valid */
            if (kv_pwrite(kv, dst, blob + src, rsize) != KV_OK) { err = KV_ERR_IO; break; }
            e->off = dst;
            src += rsize;
            dst += KV_ALIGN4(rsize);
        }
        if (err == KV_OK) kv->wr_off = dst;
    }

    if (err != KV_OK) {
        /* Keep the volume consistent: index offsets now point into the
         * blob layout; a failed rewrite is unrecoverable on disk, so scan
         * whatever made it out and rebuild. */
        SGL_LOG_ERROR(KV_LOG_TAG"gc rewrite failed, rebuilding index from log");
        if (kv_scan_log(kv) != KV_OK) return err;
    }
    sgl_free(blob);
    SGL_LOG_INFO(KV_LOG_TAG"gc done: %u bytes live, log at %u", live, kv->wr_off);
    return err;
}

/* Append one record; may trigger GC first. Returns log offset of the record */
static int64_t kv_append(sgl_kvfs_t *kv, uint8_t type, const char *key,
                         const void *value, uint32_t vlen)
{
    uint32_t klen = (uint32_t)strlen(key);
    uint32_t rsize = KV_REC_HDR_SIZE + klen + vlen;
    uint32_t aligned = KV_ALIGN4(rsize);

    if (kv->wr_off + aligned > kv->log_bytes) {
        int err = kv_gc(kv);
        if (err != KV_OK) return err;
        if (kv->wr_off + aligned > kv->log_bytes) return KV_ERR_NO_SPACE;
    }

    uint32_t crc = kv_crc32(0, (const uint8_t *)key, klen);
    if (vlen) crc = kv_crc32(crc, (const uint8_t *)value, vlen);

    uint8_t hdr[KV_REC_HDR_SIZE];
    memset(hdr, 0xFF, sizeof(hdr));
    hdr[0] = type;
    hdr[1] = (uint8_t)klen;
    kv_put16(hdr + 2, (uint16_t)vlen);
    kv_put32(hdr + 4, crc);

    uint32_t off = kv->wr_off;
    if (kv_pwrite(kv, off, hdr, KV_REC_HDR_SIZE) != KV_OK) return KV_ERR_IO;
    if (kv_pwrite(kv, off + KV_REC_HDR_SIZE, key, klen) != KV_OK) return KV_ERR_IO;
    if (vlen && kv_pwrite(kv, off + KV_REC_HDR_SIZE + klen, value, vlen) != KV_OK)
        return KV_ERR_IO;
    kv->wr_off = off + aligned;
    return (int64_t)off;
}

static int kv_dev_query(sgl_kvfs_t *kv)
{
    sgl_block_dev_t *dev = kv->dev;
    uint32_t ssz = 0, cnt = 0;
    if (dev->ioctl) {
        sgl_block_dev_ioctl(dev, SGL_BLK_GET_SECTOR_SIZE, &ssz);
        sgl_block_dev_ioctl(dev, SGL_BLK_GET_SECTOR_COUNT, &cnt);
    }
    if ((!ssz || !cnt) && dev->info) {
        if (!ssz) ssz = dev->info->sector_size;
        if (!cnt) cnt = dev->info->sector_count;
    }
    if (!ssz || !cnt) return KV_ERR_INVALID;
    kv->sector_size = ssz;

    uint32_t avail = cnt - kv->sb_sector;
    if (kv->vol_sectors == 0 || kv->vol_sectors > avail) kv->vol_sectors = avail;
    if (kv->vol_sectors < 2) return KV_ERR_INVALID;   /* need sb + at least 1 */
    kv->data_start_sector = kv->sb_sector + 1;
    kv->data_sectors = kv->vol_sectors - 1;
    kv->log_bytes = kv->data_sectors * kv->sector_size;
    return KV_OK;
}

static int kv_write_superblock(sgl_kvfs_t *kv)
{
    memset(kv->sec_buf, 0xFF, kv->sector_size);
    kv_put32(kv->sec_buf + 0, KV_MAGIC);
    kv_put32(kv->sec_buf + 4, KV_VERSION);
    kv_put32(kv->sec_buf + 8, kv_crc32(0, kv->sec_buf, 8));
    return (kv->dev->write_sectors(kv->dev, kv->sb_sector, kv->sec_buf, 1) == 0)
           ? KV_OK : KV_ERR_IO;
}

static int kv_read_superblock(sgl_kvfs_t *kv)
{
    if (kv->dev->read_sectors(kv->dev, kv->sb_sector, kv->sec_buf, 1) != 0)
        return KV_ERR_IO;
    if (kv_get32(kv->sec_buf) != KV_MAGIC) return KV_ERR;
    if (kv_get32(kv->sec_buf + 4) != KV_VERSION) return KV_ERR;
    if (kv_get32(kv->sec_buf + 8) != kv_crc32(0, kv->sec_buf, 8)) return KV_ERR;
    return KV_OK;
}

/* Open (or create) an instance on a block device.  A volume with a
 * missing/invalid superblock is formatted automatically. */
static sgl_kvfs_t *kv_open(sgl_block_dev_t *dev, const sgl_kvfs_config_t *cfg)
{
    if (!dev || !dev->read_sectors || !dev->write_sectors) {
        SGL_LOG_ERROR(KV_LOG_TAG"invalid block device");
        return NULL;
    }

    sgl_kvfs_t *kv = (sgl_kvfs_t *)sgl_malloc(sizeof(sgl_kvfs_t));
    if (!kv) return NULL;
    memset(kv, 0, sizeof(*kv));
    kv->dev = dev;
    kv->sb_sector = cfg ? cfg->start_sector : 0;
    kv->vol_sectors = cfg ? cfg->sector_count : 0;

    if (kv_dev_query(kv) != KV_OK) {
        SGL_LOG_ERROR(KV_LOG_TAG"device geometry unavailable");
        sgl_free(kv);
        return NULL;
    }

    kv->sec_buf = (uint8_t *)sgl_malloc(kv->sector_size);
    if (!kv->sec_buf) {
        sgl_free(kv);
        return NULL;
    }

    int err = kv_read_superblock(kv);
    if (err != KV_OK) {
        /* Unformatted (or corrupted) volume: create it on first mount */
        SGL_LOG_INFO(KV_LOG_TAG"no valid superblock, formatting");
        err = kv_write_superblock(kv);
        if (err == KV_OK) err = kv_erase_log(kv);
        if (err != KV_OK) {
            sgl_free(kv->sec_buf);
            sgl_free(kv);
            return NULL;
        }
    }

    if (kv_scan_log(kv) != KV_OK) {
        SGL_LOG_ERROR(KV_LOG_TAG"log scan failed");
        sgl_free(kv->sec_buf);
        sgl_free(kv);
        return NULL;
    }

    SGL_LOG_INFO(KV_LOG_TAG"mounted: %u keys, log %u/%u bytes",
                 (unsigned)kv->entry_count, kv->wr_off, kv->log_bytes);
    return kv;
}

static void kv_close(sgl_kvfs_t *kv)
{
    if (!kv) return;
    for (int i = 0; i < KV_MAX_OPEN_FILES; i++) {
        if (kv->files[i].wbuf) sgl_free(kv->files[i].wbuf);
    }
    kv_clear_index(kv);
    sgl_free(kv->sec_buf);
    sgl_free(kv);
}

/* Store a key/value pair (creates or fully replaces the value): the new
 * record is appended to the log immediately. */
static int kv_set(sgl_kvfs_t *kv, const char *key, const void *value, uint32_t len)
{
    if (!kv || !kv_key_valid(key) || len > SGL_KVFS_MAX_VAL_LEN ||
        (len && !value)) return KV_ERR_INVALID;

    int64_t off = kv_append(kv, KV_REC_DATA, key, value, len);
    if (off < 0) return (int)off;

    if (!kv_touch_entry(kv, key, (uint32_t)off, len)) {
        /* The record is already on disk: rebuild the RAM index from the log */
        kv_scan_log(kv);
        return KV_ERR_NO_MEM;
    }
    return KV_OK;
}

/* Read a value; returns the number of bytes copied, or with buffer == NULL
 * the value size only. */
static int kv_get(sgl_kvfs_t *kv, const char *key, void *buffer, uint32_t bufsize)
{
    if (!kv || !key) return KV_ERR_INVALID;
    kv_entry_t *e = kv_find(kv, key);
    if (!e) return KV_ERR_NOT_FOUND;
    if (!buffer || bufsize == 0) return (int)e->vlen;

    uint32_t n = (bufsize < e->vlen) ? bufsize : e->vlen;
    uint32_t klen = (uint32_t)strlen(key);
    int err = kv_pread(kv, e->off + KV_REC_HDR_SIZE + klen, buffer, n);
    if (err != KV_OK) return err;
    return (int)n;
}

static int kv_size(sgl_kvfs_t *kv, const char *key)
{
    if (!kv || !key) return KV_ERR_INVALID;
    kv_entry_t *e = kv_find(kv, key);
    return e ? (int)e->vlen : KV_ERR_NOT_FOUND;
}

/* Delete a key (a tombstone record is appended to the log) */
static int kv_del(sgl_kvfs_t *kv, const char *key)
{
    if (!kv || !key) return KV_ERR_INVALID;
    if (!kv_find(kv, key)) return KV_ERR_NOT_FOUND;

    int64_t off = kv_append(kv, KV_REC_DEL, key, NULL, 0);
    if (off < 0) return (int)off;
    kv_remove_entry(kv, key);
    return KV_OK;
}

/* Get the index-th live key (insertion order, stable across updates) */
static const char *kv_key_at(sgl_kvfs_t *kv, uint32_t index)
{
    if (!kv) return NULL;
    uint32_t i = 0;
    for (kv_entry_t *e = kv->entries; e; e = e->next, i++) {
        if (i == index) return e->key;
    }
    return NULL;
}

/* Strip the leading '/' and validate the single-level path; ""/"/" -> root */
static int kvfs_path_to_key(const char *path, const char **key)
{
    if (!path) return KV_ERR_INVALID;
    if (path[0] == '/') path++;
    while (*path == '/') path++;
    if (*path == '\0') {
        if (key) *key = NULL;
        return KV_OK;   /* root */
    }
    if (!kv_key_valid(path)) return KV_ERR_INVALID;
    if (key) *key = path;
    return KV_OK;
}

static int kvfs_mount(void **fs, sgl_block_dev_t *dev, const char *mount_point, void *fs_config)
{
    (void)mount_point;
    if (!fs) return KV_ERR_INVALID;

    sgl_kvfs_config_t *cfg = (sgl_kvfs_config_t *)fs_config;
    sgl_kvfs_t *kv = kv_open(dev, cfg);
    if (!kv) return KV_ERR_IO;
    *fs = kv;
    return KV_OK;
}

static int kvfs_unmount(void *fs, const char *mount_point)
{
    (void)mount_point;
    if (!fs) return KV_ERR_INVALID;
    kv_close((sgl_kvfs_t *)fs);
    return KV_OK;
}

static int kvfs_open(void *fs, const char *path, uint32_t flags)
{
    sgl_kvfs_t *kv = (sgl_kvfs_t *)fs;
    if (!kv) return KV_ERR_INVALID;

    const char *key;
    int err = kvfs_path_to_key(path, &key);
    if (err != KV_OK) return err;
    if (!key) return KV_ERR_INVALID;   /* cannot open the root as a file */

    kv_entry_t *e = kv_find(kv, key);
    if (!e && !(flags & SGL_O_CREAT)) return KV_ERR_NOT_FOUND;

    int slot = -1;
    for (int i = 0; i < KV_MAX_OPEN_FILES; i++) {
        if (!kv->files[i].used) { slot = i; break; }
    }
    if (slot < 0) return KV_ERR_BADF;

    kv_fd_t *f = &kv->files[slot];
    memset(f, 0, sizeof(*f));
    f->used = 1;
    strcpy(f->key, key);
    f->flags = flags;
    f->pos = (flags & SGL_O_APPEND) && e ? e->vlen : 0;
    return slot;
}

static int kvfs_close(void *fs, int fd)
{
    sgl_kvfs_t *kv = (sgl_kvfs_t *)fs;
    if (!kv || fd < 0 || fd >= KV_MAX_OPEN_FILES || !kv->files[fd].used) return KV_ERR_BADF;

    kv_fd_t *f = &kv->files[fd];
    int err = KV_OK;
    if (f->dirty) err = kv_set(kv, f->key, f->wbuf, f->wlen);
    if (f->wbuf) sgl_free(f->wbuf);
    memset(f, 0, sizeof(*f));
    return err;
}

/* Grow the write staging buffer, loading the old value on first use unless
 * the file was opened with O_TRUNC */
static int kvfs_fd_buffered(sgl_kvfs_t *kv, kv_fd_t *f, uint32_t need)
{
    if (!f->dirty) {
        uint32_t base = 0;
        if (f->flags & SGL_O_APPEND) {
            int len = kv_size(kv, f->key);
            if (len > 0) base = (uint32_t)len;
        }
        if (need < base) need = base;
        if (need > SGL_KVFS_MAX_VAL_LEN) return KV_ERR_NO_SPACE;

        uint32_t cap = f->wcap ? f->wcap : 32;
        while (cap < need) cap *= 2;
        uint8_t *buf = (uint8_t *)sgl_realloc(f->wbuf, cap);
        if (!buf) return KV_ERR_NO_MEM;
        f->wbuf = buf;
        f->wcap = cap;
        f->wlen = base;
        if (base && kv_get(kv, f->key, f->wbuf, base) != (int)base)
            return KV_ERR_IO;
        f->dirty = 1;
    }
    else if (need > f->wcap) {
        uint32_t cap = f->wcap;
        while (cap < need) cap *= 2;
        if (cap > SGL_KVFS_MAX_VAL_LEN) return KV_ERR_NO_SPACE;
        uint8_t *buf = (uint8_t *)sgl_realloc(f->wbuf, cap);
        if (!buf) return KV_ERR_NO_MEM;
        f->wbuf = buf;
        f->wcap = cap;
    }
    return KV_OK;
}

static int kvfs_read(void *fs, int fd, void *buffer, uint32_t count)
{
    sgl_kvfs_t *kv = (sgl_kvfs_t *)fs;
    if (!kv || fd < 0 || fd >= KV_MAX_OPEN_FILES || !kv->files[fd].used) return KV_ERR_BADF;
    if (!buffer || count == 0) return 0;

    kv_fd_t *f = &kv->files[fd];
    if (f->dirty) {   /* read-your-own-writes from the staging buffer */
        if (f->pos >= f->wlen) return 0;
        uint32_t n = f->wlen - f->pos;
        if (n > count) n = count;
        memcpy(buffer, f->wbuf + f->pos, n);
        f->pos += n;
        return (int)n;
    }

    kv_entry_t *e = kv_find(kv, f->key);
    if (!e) return KV_ERR_NOT_FOUND;
    if (f->pos >= e->vlen) return 0;
    uint32_t n = e->vlen - f->pos;
    if (n > count) n = count;
    uint32_t klen = (uint32_t)strlen(e->key);
    int err = kv_pread(kv, e->off + KV_REC_HDR_SIZE + klen + f->pos, buffer, n);
    if (err != KV_OK) return err;
    f->pos += n;
    return (int)n;
}

static int kvfs_write(void *fs, int fd, const void *buffer, uint32_t count)
{
    sgl_kvfs_t *kv = (sgl_kvfs_t *)fs;
    if (!kv || fd < 0 || fd >= KV_MAX_OPEN_FILES || !kv->files[fd].used) return KV_ERR_BADF;
    if (!buffer || count == 0) return 0;

    kv_fd_t *f = &kv->files[fd];
    if ((f->flags & 0x03) == SGL_O_RDONLY) return KV_ERR_DENIED;
    if (f->flags & SGL_O_APPEND) {
        if (f->dirty) {
            f->pos = f->wlen;
        }
        else {
            int len = kv_size(kv, f->key);
            f->pos = len > 0 ? (uint32_t)len : 0;
        }
    }
    if (f->pos > 0xFFFFFFFFu - count) return KV_ERR_NO_SPACE;

    int err = kvfs_fd_buffered(kv, f, f->pos + count);
    if (err != KV_OK) return err;
    if (f->pos > f->wlen) {
        memset(f->wbuf + f->wlen, 0, f->pos - f->wlen);   /* zero the gap */
        f->wlen = f->pos;
    }
    memcpy(f->wbuf + f->pos, buffer, count);
    f->pos += count;
    if (f->pos > f->wlen) f->wlen = f->pos;
    return (int)count;
}

static int kvfs_seek(void *fs, int fd, int32_t offset, uint8_t whence)
{
    sgl_kvfs_t *kv = (sgl_kvfs_t *)fs;
    if (!kv || fd < 0 || fd >= KV_MAX_OPEN_FILES || !kv->files[fd].used) return KV_ERR_BADF;

    kv_fd_t *f = &kv->files[fd];
    uint32_t size = f->dirty ? f->wlen : 0;
    if (!f->dirty) {
        int len = kv_size(kv, f->key);
        if (len > 0) size = (uint32_t)len;
    }

    int64_t new_pos;
    switch (whence) {
    case SGL_SEEK_SET:  new_pos = offset; break;
    case SGL_SEEK_CUR:  new_pos = (int64_t)f->pos + offset; break;
    case SGL_SEEK_END:  new_pos = (int64_t)size + offset; break;
    default: return KV_ERR_INVALID;
    }
    if (new_pos < 0) return KV_ERR_INVALID;
    f->pos = (uint32_t)new_pos;
    return (int)f->pos;
}

static int kvfs_opendir(void *fs, const char *path, int *dd)
{
    sgl_kvfs_t *kv = (sgl_kvfs_t *)fs;
    if (!kv || !path || !dd) return KV_ERR_INVALID;

    const char *key;
    int err = kvfs_path_to_key(path, &key);
    if (err != KV_OK) return err;
    if (key) return KV_ERR_INVALID;   /* only the root exists as a directory */

    for (int i = 0; i < KV_MAX_OPEN_DIRS; i++) {
        if (!kv->dirs[i].used) {
            kv->dirs[i].used = 1;
            kv->dirs[i].index = 0;
            *dd = i;
            return KV_OK;
        }
    }
    return KV_ERR_BADF;
}

static int kvfs_readdir(void *fs, int dd, char *name, uint32_t name_size, uint32_t *type)
{
    sgl_kvfs_t *kv = (sgl_kvfs_t *)fs;
    if (!kv || dd < 0 || dd >= KV_MAX_OPEN_DIRS || !kv->dirs[dd].used) return KV_ERR_BADF;

    const char *key = kv_key_at(kv, kv->dirs[dd].index);
    if (!key) return 0;
    if (name && name_size > 0) {
        uint32_t len = (uint32_t)strlen(key);
        uint32_t copy = len < name_size - 1 ? len : name_size - 1;
        memcpy(name, key, copy);
        name[copy] = '\0';
    }
    if (type) *type = SGL_S_IFREG;
    kv->dirs[dd].index++;
    return 1;
}

static int kvfs_closedir(void *fs, int dd)
{
    sgl_kvfs_t *kv = (sgl_kvfs_t *)fs;
    if (!kv || dd < 0 || dd >= KV_MAX_OPEN_DIRS || !kv->dirs[dd].used) return KV_ERR_BADF;
    kv->dirs[dd].used = 0;
    kv->dirs[dd].index = 0;
    return KV_OK;
}

static int kvfs_sync(void *fs)
{
    sgl_kvfs_t *kv = (sgl_kvfs_t *)fs;
    if (!kv) return KV_ERR_INVALID;
    /* All records reach the device immediately; just forward the flush hint */
    if (kv->dev->ioctl) sgl_block_dev_ioctl(kv->dev, SGL_BLK_CTRL_SYNC, NULL);
    return KV_OK;
}

static int kvfs_format(void *fs)
{
    sgl_kvfs_t *kv = (sgl_kvfs_t *)fs;
    if (!kv) return KV_ERR_INVALID;

    for (int i = 0; i < KV_MAX_OPEN_FILES; i++) {
        if (kv->files[i].used) return KV_ERR_DENIED;
    }
    int err = kv_write_superblock(kv);
    if (err == KV_OK) err = kv_erase_log(kv);
    if (err == KV_OK) {
        kv_clear_index(kv);
        kv->wr_off = 0;
    }
    return err;
}

static int kvfs_remove(void *fs, const char *path)
{
    sgl_kvfs_t *kv = (sgl_kvfs_t *)fs;
    if (!kv) return KV_ERR_INVALID;

    const char *key;
    int err = kvfs_path_to_key(path, &key);
    if (err != KV_OK) return err;
    if (!key) return KV_ERR_DENIED;   /* the root cannot be removed */

    for (int i = 0; i < KV_MAX_OPEN_FILES; i++) {
        if (kv->files[i].used && strcmp(kv->files[i].key, key) == 0) return KV_ERR_DENIED;
    }
    return kv_del(kv, key);
}

static int kvfs_mkdir(void *fs, const char *path)
{
    (void)fs;
    (void)path;
    /* Flat namespace: directories do not exist */
    return KV_ERR_DENIED;
}

static int kvfs_stat(void *fs, const char *path, sgl_stat_t *st)
{
    sgl_kvfs_t *kv = (sgl_kvfs_t *)fs;
    if (!kv || !path || !st) return KV_ERR_INVALID;

    const char *key;
    int err = kvfs_path_to_key(path, &key);
    if (err != KV_OK) return err;

    memset(st, 0, sizeof(*st));
    if (!key) {
        st->st_mode = SGL_S_IFDIR;
        return KV_OK;
    }
    kv_entry_t *e = kv_find(kv, key);
    if (!e) return KV_ERR_NOT_FOUND;
    st->st_mode = SGL_S_IFREG;
    st->st_size = e->vlen;
    return KV_OK;
}

static int kvfs_rename(void *fs, const char *old_path, const char *new_path)
{
    sgl_kvfs_t *kv = (sgl_kvfs_t *)fs;
    if (!kv) return KV_ERR_INVALID;

    const char *old_key, *new_key;
    int err = kvfs_path_to_key(old_path, &old_key);
    if (err != KV_OK) return err;
    err = kvfs_path_to_key(new_path, &new_key);
    if (err != KV_OK) return err;
    if (!old_key || !new_key) return KV_ERR_DENIED;

    kv_entry_t *e = kv_find(kv, old_key);
    if (!e) return KV_ERR_NOT_FOUND;
    if (kv_find(kv, new_key)) return KV_ERR_EXIST;

    uint8_t *buf = (uint8_t *)sgl_malloc(e->vlen ? e->vlen : 1);
    if (!buf) return KV_ERR_NO_MEM;
    int n = kv_get(kv, old_key, buf, e->vlen);
    if (n != (int)e->vlen) {
        sgl_free(buf);
        return KV_ERR_IO;
    }
    err = kv_set(kv, new_key, buf, e->vlen);
    sgl_free(buf);
    if (err != KV_OK) return err;
    return kv_del(kv, old_key);
}

static int kvfs_statvfs(void *fs, sgl_statvfs_t *info)
{
    sgl_kvfs_t *kv = (sgl_kvfs_t *)fs;
    if (!kv || !info) return KV_ERR_INVALID;

    memset(info, 0, sizeof(*info));
    /* 1-byte blocks so the numbers match the byte-oriented log */
    info->f_bsize = 1;
    info->f_blocks = kv->log_bytes;
    /* Free bytes before the next compaction, worst case (no GC headroom) */
    uint32_t used = kv_live_bytes(kv);
    info->f_bfree = used < kv->log_bytes ? kv->log_bytes - used : 0;
    info->f_bavail = info->f_bfree;
    return KV_OK;
}

static sgl_fs_ops_t kvfs_ops = {
    .mount = kvfs_mount,
    .unmount = kvfs_unmount,
    .open = kvfs_open,
    .close = kvfs_close,
    .read = kvfs_read,
    .write = kvfs_write,
    .seek = kvfs_seek,
    .opendir = kvfs_opendir,
    .readdir = kvfs_readdir,
    .closedir = kvfs_closedir,
    .sync = kvfs_sync,
    .format = kvfs_format,
    .remove = kvfs_remove,
    .mkdir = kvfs_mkdir,
    .stat = kvfs_stat,
    .rename = kvfs_rename,
    .statvfs = kvfs_statvfs,
};

static sgl_fs_type_t kvfs_type = {
    .name = KV_FS_NAME,
    .ops = &kvfs_ops,
};

/**
 * @brief Register KVFS file system type with sgl_fs VFS.
 *        Call this once before sgl_fs_mount("/kv", "kvfs", dev, cfg).
 * @return 0 on success, -1 on failure
 */
int sgl_kvfs_register(void)
{
    return sgl_fs_register(&kvfs_type);
}
