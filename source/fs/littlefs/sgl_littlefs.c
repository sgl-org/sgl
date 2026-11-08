/* source/fs/littlefs/sgl_littlefs.c
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

#include "sgl_littlefs.h"
#include <string.h>
#include <assert.h>
#include <sgl_mm.h>

#define LFS_FS_NAME         "littlefs"

#define LFS_MAX_OPEN_FILES 8
#define LFS_MAX_OPEN_DIRS  4
#define LFS_MAX_NAME_LEN   255
#define LFS_DISK_VERSION   0x00020001
#define LFS_NAME_MAX       255
#define LFS_FILE_MAX       2147483647
#define LFS_ATTR_MAX       1022

enum {
    LFS_ERR_OK          = 0,
    LFS_ERR_IO          = -5,
    LFS_ERR_CORRUPT     = -84,
    LFS_ERR_NOENT       = -2,
    LFS_ERR_EXIST       = -17,
    LFS_ERR_NOTDIR      = -20,
    LFS_ERR_ISDIR       = -21,
    LFS_ERR_NOTEMPTY    = -39,
    LFS_ERR_BADF        = -9,
    LFS_ERR_FBIG        = -27,
    LFS_ERR_INVAL       = -22,
    LFS_ERR_NOSPC       = -28,
    LFS_ERR_NOMEM       = -12,
    LFS_ERR_NOATTR      = -61,
    LFS_ERR_NAMETOOLONG = -36,
};

enum {
    LFS_TYPE_REG          = 0x001,
    LFS_TYPE_DIR          = 0x002,

    /* internally used types */
    LFS_TYPE_SPLICE       = 0x400,
    LFS_TYPE_NAME         = 0x000,
    LFS_TYPE_STRUCT       = 0x200,
    LFS_TYPE_USERATTR     = 0x300,
    LFS_TYPE_FROM         = 0x100,
    LFS_TYPE_TAIL         = 0x600,
    LFS_TYPE_GLOBALS      = 0x700,
    LFS_TYPE_CRC          = 0x500,

    LFS_TYPE_CREATE       = 0x401,
    LFS_TYPE_DELETE       = 0x4ff,
    LFS_TYPE_SUPERBLOCK   = 0x0ff,
    LFS_TYPE_DIRSTRUCT    = 0x200,
    LFS_TYPE_CTZSTRUCT    = 0x202,
    LFS_TYPE_INLINESTRUCT = 0x201,
    LFS_TYPE_SOFTTAIL     = 0x600,
    LFS_TYPE_HARDTAIL     = 0x601,
    LFS_TYPE_MOVESTATE    = 0x7ff,
    LFS_TYPE_CCRC         = 0x500,
    LFS_TYPE_FCRC         = 0x5ff,

    /* internal chip sources */
    LFS_FROM_NOOP         = 0x000,
    LFS_FROM_MOVE         = 0x101,
    LFS_FROM_USERATTRS    = 0x102,
};

enum {
    LFS_O_RDONLY = 1,
    LFS_O_WRONLY = 2,
    LFS_O_RDWR   = 3,
    LFS_O_CREAT  = 0x0100,
    LFS_O_EXCL   = 0x0200,
    LFS_O_TRUNC  = 0x0400,
    LFS_O_APPEND = 0x0800,

    /* internally used flags */
    LFS_F_DIRTY   = 0x00010000,
    LFS_F_DUSTY   = 0x00020000,
    LFS_F_WRITING = 0x00040000,
    LFS_F_READING = 0x00080000,
    LFS_F_ERRED   = 0x00100000,
    LFS_F_INLINE  = 0x01000000,
};

enum {
    LFS_SEEK_SET = 0,
    LFS_SEEK_CUR = 1,
    LFS_SEEK_END = 2,
};

enum {
    LFS_OK_RELOCATED = 1,
    LFS_OK_DROPPED   = 2,
    LFS_OK_ORPHANED  = 3,
};

enum {
    LFS_CMP_EQ = 0,
    LFS_CMP_LT = 1,
    LFS_CMP_GT = 2,
};

#define LFS_BLOCK_NULL   ((lfs_block_t)-1)
#define LFS_BLOCK_INLINE ((lfs_block_t)-2)

typedef uint32_t lfs_size_t;
typedef uint32_t lfs_off_t;
typedef int32_t  lfs_ssize_t;
typedef int32_t  lfs_soff_t;
typedef uint32_t lfs_block_t;
typedef uint32_t lfs_tag_t;
typedef int32_t  lfs_stag_t;

#define LFS_ASSERT(cond) assert(cond)

static inline lfs_size_t lfs_min(lfs_size_t a, lfs_size_t b)
{
    return (a < b) ? a : b;
}

static inline lfs_size_t lfs_max(lfs_size_t a, lfs_size_t b)
{
    return (a > b) ? a : b;
}

static inline lfs_size_t lfs_aligndown(lfs_size_t a, lfs_size_t alignment)
{
    return a & ~(alignment - 1);
}

static inline lfs_size_t lfs_alignup(lfs_size_t a, lfs_size_t alignment)
{
    return lfs_aligndown(a + alignment - 1, alignment);
}

static inline int lfs_scmp(lfs_soff_t a, lfs_soff_t b)
{
    return (a > b) - (a < b);
}

static inline uint8_t lfs_ctz(uint32_t i)
{
    uint8_t n = 0;
    while (!(i & 0x1)) {
        i >>= 1;
        n += 1;
    }
    return n;
}

static inline uint8_t lfs_popc(uint32_t v)
{
    uint8_t n = 0;
    while (v) {
        v &= v - 1;
        n += 1;
    }
    return n;
}

static inline uint32_t lfs_npw2(uint32_t n)
{
    return (n > 1) ? 32U - (uint32_t)__builtin_clz(n - 1U) : 0U;
}

static uint32_t lfs_crc(uint32_t crc, const void *buffer, size_t size)
{
    const uint8_t *data = (const uint8_t *)buffer;
    while (size--) {
        crc ^= *data++;
        for (int i = 0; i < 8; i++) {
            uint32_t mask = (uint32_t)(-(int32_t)(crc & 1));
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return crc;
}

#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
static inline uint32_t lfs_fromle32(uint32_t a)
{
    return ((a & 0x000000FF) << 24) | ((a & 0x0000FF00) << 8) |
           ((a & 0x00FF0000) >> 8)  | ((a & 0xFF000000) >> 24);
}
#define lfs_tole32 lfs_fromle32
static inline uint32_t lfs_frombe32(uint32_t a)
{
    return a;
}
#define lfs_tobe32(a) (a)
#else
static inline uint32_t lfs_fromle32(uint32_t a)
{
    (void)a;
    return a;
}
#define lfs_tole32 lfs_fromle32
static inline uint32_t lfs_frombe32(uint32_t a)
{
    return ((a & 0x000000FF) << 24) | ((a & 0x0000FF00) << 8) |
           ((a & 0x00FF0000) >> 8)  | ((a & 0xFF000000) >> 24);
}
#define lfs_tobe32 lfs_frombe32
#endif

static inline lfs_size_t lfs_path_namelen(const char *path)
{
    return (lfs_size_t)strcspn(path, "/");
}

static inline bool lfs_path_islast(const char *path)
{
    lfs_size_t namelen = lfs_path_namelen(path);
    return path[namelen + strspn(path + namelen, "/")] == '\0';
}

static inline bool lfs_path_isdir(const char *path)
{
    return path[lfs_path_namelen(path)] != '\0';
}

static inline void lfs_pair_swap(lfs_block_t pair[2])
{
    lfs_block_t t = pair[0];
    pair[0] = pair[1];
    pair[1] = t;
}

static inline bool lfs_pair_isnull(const lfs_block_t pair[2])
{
    return pair[0] == LFS_BLOCK_NULL || pair[1] == LFS_BLOCK_NULL;
}

static inline int lfs_pair_cmp(
        const lfs_block_t paira[2],
        const lfs_block_t pairb[2])
{
    return !(paira[0] == pairb[0] || paira[1] == pairb[1] ||
             paira[0] == pairb[1] || paira[1] == pairb[0]);
}

static inline bool lfs_pair_issync(
        const lfs_block_t paira[2],
        const lfs_block_t pairb[2])
{
    return (paira[0] == pairb[0] && paira[1] == pairb[1]) ||
           (paira[0] == pairb[1] && paira[1] == pairb[0]);
}

static inline void lfs_pair_fromle32(lfs_block_t pair[2])
{
    pair[0] = lfs_fromle32(pair[0]);
    pair[1] = lfs_fromle32(pair[1]);
}

static inline void lfs_pair_tole32(lfs_block_t pair[2])
{
    pair[0] = lfs_tole32(pair[0]);
    pair[1] = lfs_tole32(pair[1]);
}

#define LFS_MKTAG(type, id, size) \
    (((lfs_tag_t)(type) << 20) | ((lfs_tag_t)(id) << 10) | (lfs_tag_t)(size))

#define LFS_MKTAG_IF(cond, type, id, size) \
    ((cond) ? LFS_MKTAG(type, id, size) : LFS_MKTAG(LFS_FROM_NOOP, 0, 0))

#define LFS_MKTAG_IF_ELSE(cond, type1, id1, size1, type2, id2, size2) \
    ((cond) ? LFS_MKTAG(type1, id1, size1) : LFS_MKTAG(type2, id2, size2))

static inline bool lfs_tag_isvalid(lfs_tag_t tag)
{
    return !(tag & 0x80000000);
}

static inline bool lfs_tag_isdelete(lfs_tag_t tag)
{
    return ((int32_t)(tag << 22) >> 22) == -1;
}

static inline uint16_t lfs_tag_type1(lfs_tag_t tag)
{
    return (uint16_t)((tag & 0x70000000) >> 20);
}

static inline uint16_t lfs_tag_type2(lfs_tag_t tag)
{
    return (uint16_t)((tag & 0x78000000) >> 20);
}

static inline uint16_t lfs_tag_type3(lfs_tag_t tag)
{
    return (uint16_t)((tag & 0x7ff00000) >> 20);
}

static inline uint8_t lfs_tag_chunk(lfs_tag_t tag)
{
    return (uint8_t)((tag & 0x0ff00000) >> 20);
}

static inline int8_t lfs_tag_splice(lfs_tag_t tag)
{
    return (int8_t)lfs_tag_chunk(tag);
}

static inline uint16_t lfs_tag_id(lfs_tag_t tag)
{
    return (uint16_t)((tag & 0x000ffc00) >> 10);
}

static inline lfs_size_t lfs_tag_size(lfs_tag_t tag)
{
    return tag & 0x000003ff;
}

static inline lfs_size_t lfs_tag_dsize(lfs_tag_t tag)
{
    return sizeof(tag) + lfs_tag_size(tag + lfs_tag_isdelete(tag));
}

struct lfs_mattr {
    lfs_tag_t tag;
    const void *buffer;
};

struct lfs_diskoff {
    lfs_block_t block;
    lfs_off_t off;
};

#define LFS_MKATTRS(...) \
    (struct lfs_mattr[]){__VA_ARGS__}, \
    sizeof((struct lfs_mattr[]){__VA_ARGS__}) / sizeof(struct lfs_mattr)

typedef struct {
    sgl_block_dev_t *dev;
    uint32_t sector_size;
    uint32_t sector_count;
    uint32_t sectors_per_block;
    uint8_t *sec_buf;          // one sector, RMW staging
} lfs_bdev_t;

struct lfs_config {
    void *context;

    int (*read)(const struct lfs_config *c, lfs_block_t block,
            lfs_off_t off, void *buffer, lfs_size_t size);
    int (*prog)(const struct lfs_config *c, lfs_block_t block,
            lfs_off_t off, const void *buffer, lfs_size_t size);
    int (*erase)(const struct lfs_config *c, lfs_block_t block);
    int (*sync)(const struct lfs_config *c);

    lfs_size_t read_size;
    lfs_size_t prog_size;
    lfs_size_t block_size;
    lfs_size_t block_count;
    int32_t block_cycles;
    lfs_size_t cache_size;
    lfs_size_t lookahead_size;
    lfs_size_t compact_thresh;
    void *read_buffer;
    void *prog_buffer;
    void *lookahead_buffer;
    lfs_size_t name_max;
    lfs_size_t file_max;
    lfs_size_t attr_max;
    lfs_size_t metadata_max;
    lfs_size_t inline_max;
};

static int lfs_bdev_read(const struct lfs_config *c, lfs_block_t block,
        lfs_off_t off, void *buffer, lfs_size_t size)
{
    lfs_bdev_t *bd = (lfs_bdev_t *)c->context;
    uint8_t *dst = (uint8_t *)buffer;
    uint32_t ssz = bd->sector_size;

    while (size > 0) {
        uint32_t sector = (block * bd->sectors_per_block) + (off / ssz);
        uint32_t soff = off % ssz;
        uint32_t diff = ssz - soff;
        if (diff > size) diff = size;
        if (bd->dev->read_sectors(bd->dev, sector, bd->sec_buf, 1) != 0) {
            SGL_LOG_ERROR(LFS_FS_NAME " bdev read: block=%u sector=%u failed",
                          (unsigned)block, (unsigned)sector);
            return LFS_ERR_IO;
        }
        memcpy(dst, bd->sec_buf + soff, diff);
        dst += diff;
        off += diff;
        size -= diff;
    }
    return 0;
}

static int lfs_bdev_prog(const struct lfs_config *c, lfs_block_t block,
        lfs_off_t off, const void *buffer, lfs_size_t size)
{
    lfs_bdev_t *bd = (lfs_bdev_t *)c->context;
    const uint8_t *src = (const uint8_t *)buffer;
    uint32_t ssz = bd->sector_size;

    while (size > 0) {
        uint32_t sector = (block * bd->sectors_per_block) + (off / ssz);
        uint32_t soff = off % ssz;
        uint32_t diff = ssz - soff;
        if (diff > size) diff = size;
        if (soff != 0 || diff != ssz) {
            // partial sector: read-modify-write
            if (bd->dev->read_sectors(bd->dev, sector, bd->sec_buf, 1) != 0) {
                SGL_LOG_ERROR(LFS_FS_NAME " bdev prog rmw: sector=%u failed",
                              (unsigned)sector);
                return LFS_ERR_IO;
            }
        }
        memcpy(bd->sec_buf + soff, src, diff);
        if (bd->dev->write_sectors(bd->dev, sector, bd->sec_buf, 1) != 0) {
            SGL_LOG_ERROR(LFS_FS_NAME " bdev prog: sector=%u failed",
                          (unsigned)sector);
            return LFS_ERR_IO;
        }
        src += diff;
        off += diff;
        size -= diff;
    }
    return 0;
}

static int lfs_bdev_erase(const struct lfs_config *c, lfs_block_t block)
{
    lfs_bdev_t *bd = (lfs_bdev_t *)c->context;
    uint32_t ssz = bd->sector_size;
    memset(bd->sec_buf, 0xff, ssz);
    uint32_t base = block * bd->sectors_per_block;
    for (uint32_t i = 0; i < bd->sectors_per_block; i++) {
        if (bd->dev->write_sectors(bd->dev, base + i, bd->sec_buf, 1) != 0) {
            SGL_LOG_ERROR(LFS_FS_NAME " bdev erase: sector=%u failed",
                          (unsigned)(base + i));
            return LFS_ERR_IO;
        }
    }
    return 0;
}

static int lfs_bdev_sync(const struct lfs_config *c)
{
    lfs_bdev_t *bd = (lfs_bdev_t *)c->context;
    if (bd->dev->ioctl &&
            bd->dev->ioctl(bd->dev, SGL_BLK_CTRL_SYNC, NULL) != 0) {
        return LFS_ERR_IO;
    }
    return 0;
}

struct lfs_info {
    uint8_t type;
    lfs_size_t size;
    char name[LFS_MAX_NAME_LEN + 1];
};

struct lfs_file_config {
    void *buffer;
    const struct lfs_attr *attrs;
    lfs_size_t attr_count;
};

struct lfs_attr {
    uint8_t type;
    void *buffer;
    lfs_size_t size;
};

typedef struct lfs_cache {
    lfs_block_t block;
    lfs_off_t off;
    lfs_size_t size;
    uint8_t *buffer;
} lfs_cache_t;

typedef struct lfs_mdir {
    lfs_block_t pair[2];
    uint32_t rev;
    lfs_off_t off;
    uint32_t etag;
    uint16_t count;
    bool erased;
    bool split;
    lfs_block_t tail[2];
} lfs_mdir_t;

typedef struct lfs_dir {
    struct lfs_dir *next;
    uint16_t id;
    uint8_t type;
    lfs_mdir_t m;

    lfs_off_t pos;
    lfs_block_t head[2];
} lfs_dir_t;

typedef struct lfs_ctz {
    lfs_block_t head;
    lfs_size_t size;
} lfs_ctz_t;

typedef struct lfs_file {
    struct lfs_file *next;
    uint16_t id;
    uint8_t type;
    lfs_mdir_t m;

    lfs_ctz_t ctz;

    uint32_t flags;
    lfs_off_t pos;
    lfs_block_t block;
    lfs_off_t off;
    lfs_cache_t cache;

    const struct lfs_file_config *cfg;
} lfs_file_t;

typedef struct lfs_superblock {
    uint32_t version;
    lfs_size_t block_size;
    lfs_size_t block_count;
    lfs_size_t name_max;
    lfs_size_t file_max;
    lfs_size_t attr_max;
} lfs_superblock_t;

typedef struct lfs_gstate {
    uint32_t tag;
    lfs_block_t pair[2];
} lfs_gstate_t;

typedef struct lfs_fs {
    lfs_cache_t rcache;
    lfs_cache_t pcache;

    lfs_block_t root[2];
    struct lfs_mlist {
        struct lfs_mlist *next;
        uint16_t id;
        uint8_t type;
        lfs_mdir_t m;
    } *mlist;
    uint32_t seed;

    lfs_gstate_t gstate;
    lfs_gstate_t gdisk;
    lfs_gstate_t gdelta;

    struct lfs_lookahead {
        lfs_block_t start;
        lfs_block_t size;
        lfs_block_t next;
        lfs_block_t ckpoint;
        uint8_t *buffer;
    } lookahead;

    const struct lfs_config *cfg;
    lfs_size_t block_count;
    lfs_size_t name_max;
    lfs_size_t file_max;
    lfs_size_t attr_max;
    lfs_size_t inline_max;
} lfs_fs_t;

// operations on global state
static inline void lfs_gstate_xor(lfs_gstate_t *a, const lfs_gstate_t *b)
{
    a->tag ^= b->tag;
    a->pair[0] ^= b->pair[0];
    a->pair[1] ^= b->pair[1];
}

static inline bool lfs_gstate_iszero(const lfs_gstate_t *a)
{
    return a->tag == 0 && a->pair[0] == 0 && a->pair[1] == 0;
}

static inline bool lfs_gstate_hasorphans(const lfs_gstate_t *a)
{
    return lfs_tag_size(a->tag) != 0;
}

static inline uint8_t lfs_gstate_getorphans(const lfs_gstate_t *a)
{
    return (uint8_t)(lfs_tag_size(a->tag) & 0x1ff);
}

static inline bool lfs_gstate_hasmove(const lfs_gstate_t *a)
{
    return lfs_tag_type1(a->tag) != 0;
}

static inline bool lfs_gstate_needssuperblock(const lfs_gstate_t *a)
{
    return lfs_tag_size(a->tag) >> 9;
}

static inline bool lfs_gstate_hasmovehere(const lfs_gstate_t *a,
        const lfs_block_t *pair)
{
    return lfs_tag_type1(a->tag) && lfs_pair_cmp(a->pair, pair) == 0;
}

static inline void lfs_gstate_fromle32(lfs_gstate_t *a)
{
    a->tag     = lfs_fromle32(a->tag);
    a->pair[0] = lfs_fromle32(a->pair[0]);
    a->pair[1] = lfs_fromle32(a->pair[1]);
}

static inline void lfs_gstate_tole32(lfs_gstate_t *a)
{
    a->tag     = lfs_tole32(a->tag);
    a->pair[0] = lfs_tole32(a->pair[0]);
    a->pair[1] = lfs_tole32(a->pair[1]);
}

// operations on forward-CRCs used to track erased state
struct lfs_fcrc {
    lfs_size_t size;
    uint32_t crc;
};

static void lfs_fcrc_fromle32(struct lfs_fcrc *fcrc)
{
    fcrc->size = lfs_fromle32(fcrc->size);
    fcrc->crc = lfs_fromle32(fcrc->crc);
}

static void lfs_fcrc_tole32(struct lfs_fcrc *fcrc)
{
    fcrc->size = lfs_tole32(fcrc->size);
    fcrc->crc = lfs_tole32(fcrc->crc);
}

// other endianness operations
static void lfs_ctz_fromle32(lfs_ctz_t *ctz)
{
    ctz->head = lfs_fromle32(ctz->head);
    ctz->size = lfs_fromle32(ctz->size);
}

static void lfs_ctz_tole32(lfs_ctz_t *ctz)
{
    ctz->head = lfs_tole32(ctz->head);
    ctz->size = lfs_tole32(ctz->size);
}

static inline void lfs_superblock_fromle32(lfs_superblock_t *superblock)
{
    superblock->version     = lfs_fromle32(superblock->version);
    superblock->block_size  = lfs_fromle32(superblock->block_size);
    superblock->block_count = lfs_fromle32(superblock->block_count);
    superblock->name_max    = lfs_fromle32(superblock->name_max);
    superblock->file_max    = lfs_fromle32(superblock->file_max);
    superblock->attr_max    = lfs_fromle32(superblock->attr_max);
}

static inline void lfs_superblock_tole32(lfs_superblock_t *superblock)
{
    superblock->version     = lfs_tole32(superblock->version);
    superblock->block_size  = lfs_tole32(superblock->block_size);
    superblock->block_count = lfs_tole32(superblock->block_count);
    superblock->name_max    = lfs_tole32(superblock->name_max);
    superblock->file_max    = lfs_tole32(superblock->file_max);
    superblock->attr_max    = lfs_tole32(superblock->attr_max);
}

static void lfs_mlist_remove(lfs_fs_t *fs, struct lfs_mlist *mlist)
{
    for (struct lfs_mlist **p = &fs->mlist; *p; p = &(*p)->next) {
        if (*p == mlist) {
            *p = (*p)->next;
            break;
        }
    }
}

static void lfs_mlist_append(lfs_fs_t *fs, struct lfs_mlist *mlist)
{
    mlist->next = fs->mlist;
    fs->mlist = mlist;
}

static uint32_t lfs_fs_disk_version(lfs_fs_t *fs)
{
    (void)fs;
    return LFS_DISK_VERSION;
}

static uint16_t lfs_fs_disk_version_major(lfs_fs_t *fs)
{
    return 0xffff & (lfs_fs_disk_version(fs) >> 16);
}

static uint16_t lfs_fs_disk_version_minor(lfs_fs_t *fs)
{
    return 0xffff & (lfs_fs_disk_version(fs) >> 0);
}

/// Internal operations predeclared here ///
static int lfs_dir_commit(lfs_fs_t *fs, lfs_mdir_t *dir,
        const struct lfs_mattr *attrs, int attrcount);
static int lfs_dir_compact(lfs_fs_t *fs, lfs_mdir_t *dir,
        const struct lfs_mattr *attrs, int attrcount,
        lfs_mdir_t *source, uint16_t begin, uint16_t end);
static lfs_ssize_t lfs_file_flushedwrite(lfs_fs_t *fs, lfs_file_t *file,
        const void *buffer, lfs_size_t size);
static lfs_ssize_t lfs_file_write_(lfs_fs_t *fs, lfs_file_t *file,
        const void *buffer, lfs_size_t size);
static int lfs_file_sync_(lfs_fs_t *fs, lfs_file_t *file);
static int lfs_file_outline(lfs_fs_t *fs, lfs_file_t *file);
static int lfs_file_flush(lfs_fs_t *fs, lfs_file_t *file);

static int lfs_fs_deorphan(lfs_fs_t *fs, bool powerloss);
static int lfs_fs_preporphans(lfs_fs_t *fs, int8_t orphans);
static void lfs_fs_prepmove(lfs_fs_t *fs,
        uint16_t id, const lfs_block_t pair[2]);
static int lfs_fs_pred(lfs_fs_t *fs, const lfs_block_t dir[2],
        lfs_mdir_t *pdir);
static lfs_stag_t lfs_fs_parent(lfs_fs_t *fs, const lfs_block_t dir[2],
        lfs_mdir_t *parent);
static int lfs_fs_forceconsistency(lfs_fs_t *fs);

static void lfs_fs_prepsuperblock(lfs_fs_t *fs, bool needssuperblock);

static lfs_ssize_t lfs_file_flushedread(lfs_fs_t *fs, lfs_file_t *file,
        void *buffer, lfs_size_t size);
static lfs_ssize_t lfs_file_read_(lfs_fs_t *fs, lfs_file_t *file,
        void *buffer, lfs_size_t size);
static int lfs_file_close_(lfs_fs_t *fs, lfs_file_t *file);
static lfs_soff_t lfs_file_size_(lfs_fs_t *fs, lfs_file_t *file);

static lfs_ssize_t lfs_fs_size_(lfs_fs_t *fs);
static int lfs_fs_traverse_(lfs_fs_t *fs,
        int (*cb)(void *data, lfs_block_t block), void *data,
        bool includeorphans);

static int lfs_deinit(lfs_fs_t *fs);
static int lfs_unmount_(lfs_fs_t *fs);
static lfs_stag_t lfs_dir_fetchmatch(lfs_fs_t *fs, lfs_mdir_t *dir,
        const lfs_block_t pair[2], lfs_tag_t fmask, lfs_tag_t ftag,
        uint16_t *id,
        int (*cb)(void *data, lfs_tag_t tag, const void *buffer), void *data);
static int lfs_dir_traverse(lfs_fs_t *fs,
        const lfs_mdir_t *dir, lfs_off_t off, lfs_tag_t ptag,
        const struct lfs_mattr *attrs, int attrcount,
        lfs_tag_t tmask, lfs_tag_t ttag,
        uint16_t begin, uint16_t end, int16_t diff,
        int (*cb)(void *data, lfs_tag_t tag, const void *buffer), void *data);
static int lfs_dir_getinfo(lfs_fs_t *fs, const lfs_mdir_t *dir,
        uint16_t id, struct lfs_info *info);
static int lfs_ctz_traverse(lfs_fs_t *fs,
        const lfs_cache_t *pcache, lfs_cache_t *rcache,
        lfs_block_t head, lfs_size_t size,
        int (*cb)(void *, lfs_block_t), void *data);
static lfs_stag_t lfs_dir_find(lfs_fs_t *fs, lfs_mdir_t *dir,
        const char **path, uint16_t *id);

static inline void lfs_cache_drop(lfs_fs_t *fs, lfs_cache_t *rcache)
{
    (void)fs;
    rcache->block = LFS_BLOCK_NULL;
}

static inline void lfs_cache_zero(lfs_fs_t *fs, lfs_cache_t *pcache)
{
    // zero to avoid information leak
    (void)fs;
    memset(pcache->buffer, 0xff, fs->cfg->cache_size);
    pcache->block = LFS_BLOCK_NULL;
}

static int lfs_bd_read(lfs_fs_t *fs,
        const lfs_cache_t *pcache, lfs_cache_t *rcache, lfs_size_t hint,
        lfs_block_t block, lfs_off_t off,
        void *buffer, lfs_size_t size)
{
    uint8_t *data = (uint8_t *)buffer;
    if (off + size > fs->cfg->block_size
            || (fs->block_count && block >= fs->block_count)) {
        return LFS_ERR_CORRUPT;
    }

    while (size > 0) {
        lfs_size_t diff = size;

        if (pcache && block == pcache->block &&
                off < pcache->off + pcache->size) {
            if (off >= pcache->off) {
                // is already in pcache?
                diff = lfs_min(diff, pcache->size - (off - pcache->off));
                memcpy(data, &pcache->buffer[off - pcache->off], diff);

                data += diff;
                off += diff;
                size -= diff;
                continue;
            }

            // pcache takes priority
            diff = lfs_min(diff, pcache->off - off);
        }

        if (block == rcache->block &&
                off < rcache->off + rcache->size) {
            if (off >= rcache->off) {
                // is already in rcache?
                diff = lfs_min(diff, rcache->size - (off - rcache->off));
                memcpy(data, &rcache->buffer[off - rcache->off], diff);

                data += diff;
                off += diff;
                size -= diff;
                continue;
            }

            // rcache takes priority
            diff = lfs_min(diff, rcache->off - off);
        }

        if (size >= hint && off % fs->cfg->read_size == 0 &&
                size >= fs->cfg->read_size) {
            // bypass cache?
            diff = lfs_aligndown(diff, fs->cfg->read_size);
            int err = fs->cfg->read(fs->cfg, block, off, data, diff);
            LFS_ASSERT(err <= 0);
            if (err) {
                return err;
            }

            data += diff;
            off += diff;
            size -= diff;
            continue;
        }

        // load to cache, first condition can no longer fail
        LFS_ASSERT(!fs->block_count || block < fs->block_count);
        rcache->block = block;
        rcache->off = lfs_aligndown(off, fs->cfg->read_size);
        rcache->size = lfs_min(
                lfs_min(
                    lfs_alignup(off + hint, fs->cfg->read_size),
                    fs->cfg->block_size)
                - rcache->off,
                fs->cfg->cache_size);
        int err = fs->cfg->read(fs->cfg, rcache->block,
                rcache->off, rcache->buffer, rcache->size);
        LFS_ASSERT(err <= 0);
        if (err) {
            return err;
        }
    }

    return 0;
}

static int lfs_bd_cmp(lfs_fs_t *fs,
        const lfs_cache_t *pcache, lfs_cache_t *rcache, lfs_size_t hint,
        lfs_block_t block, lfs_off_t off,
        const void *buffer, lfs_size_t size)
{
    const uint8_t *data = (const uint8_t *)buffer;
    lfs_size_t diff = 0;

    for (lfs_off_t i = 0; i < size; i += diff) {
        uint8_t dat[8];

        diff = lfs_min(size - i, sizeof(dat));
        int err = lfs_bd_read(fs,
                pcache, rcache, hint - i,
                block, off + i, &dat, diff);
        if (err) {
            return err;
        }

        int res = memcmp(dat, data + i, diff);
        if (res) {
            return res < 0 ? LFS_CMP_LT : LFS_CMP_GT;
        }
    }

    return LFS_CMP_EQ;
}

static int lfs_bd_crc(lfs_fs_t *fs,
        const lfs_cache_t *pcache, lfs_cache_t *rcache, lfs_size_t hint,
        lfs_block_t block, lfs_off_t off, lfs_size_t size, uint32_t *crc)
{
    lfs_size_t diff = 0;

    for (lfs_off_t i = 0; i < size; i += diff) {
        uint8_t dat[8];
        diff = lfs_min(size - i, sizeof(dat));
        int err = lfs_bd_read(fs,
                pcache, rcache, hint - i,
                block, off + i, &dat, diff);
        if (err) {
            return err;
        }

        *crc = lfs_crc(*crc, &dat, diff);
    }

    return 0;
}

static int lfs_bd_flush(lfs_fs_t *fs,
        lfs_cache_t *pcache, lfs_cache_t *rcache, bool validate)
{
    if (pcache->block != LFS_BLOCK_NULL && pcache->block != LFS_BLOCK_INLINE) {
        LFS_ASSERT(pcache->block < fs->block_count);
        lfs_size_t diff = lfs_alignup(pcache->size, fs->cfg->prog_size);
        int err = fs->cfg->prog(fs->cfg, pcache->block,
                pcache->off, pcache->buffer, diff);
        LFS_ASSERT(err <= 0);
        if (err) {
            return err;
        }

        if (validate) {
            // check data on disk
            lfs_cache_drop(fs, rcache);
            int res = lfs_bd_cmp(fs,
                    NULL, rcache, diff,
                    pcache->block, pcache->off, pcache->buffer, diff);
            if (res < 0) {
                return res;
            }

            if (res != LFS_CMP_EQ) {
                return LFS_ERR_CORRUPT;
            }
        }

        lfs_cache_zero(fs, pcache);
    }

    return 0;
}

static int lfs_bd_sync(lfs_fs_t *fs,
        lfs_cache_t *pcache, lfs_cache_t *rcache, bool validate)
{
    lfs_cache_drop(fs, rcache);

    int err = lfs_bd_flush(fs, pcache, rcache, validate);
    if (err) {
        return err;
    }

    err = fs->cfg->sync(fs->cfg);
    LFS_ASSERT(err <= 0);
    return err;
}

static int lfs_bd_prog(lfs_fs_t *fs,
        lfs_cache_t *pcache, lfs_cache_t *rcache, bool validate,
        lfs_block_t block, lfs_off_t off,
        const void *buffer, lfs_size_t size)
{
    const uint8_t *data = (const uint8_t *)buffer;
    LFS_ASSERT(block == LFS_BLOCK_INLINE || block < fs->block_count);
    LFS_ASSERT(off + size <= fs->cfg->block_size);

    while (size > 0) {
        if (block == pcache->block &&
                off >= pcache->off &&
                off < pcache->off + fs->cfg->cache_size) {
            // already fits in pcache?
            lfs_size_t diff = lfs_min(size,
                    fs->cfg->cache_size - (off - pcache->off));
            memcpy(&pcache->buffer[off - pcache->off], data, diff);

            data += diff;
            off += diff;
            size -= diff;

            pcache->size = lfs_max(pcache->size, off - pcache->off);
            if (pcache->size == fs->cfg->cache_size) {
                // eagerly flush out pcache if we fill up
                int err = lfs_bd_flush(fs, pcache, rcache, validate);
                if (err) {
                    return err;
                }
            }

            continue;
        }

        if (pcache->block != LFS_BLOCK_NULL) {
            lfs_cache_drop(fs, pcache);
        }

        // prepare pcache, first condition can no longer fail
        pcache->block = block;
        pcache->off = lfs_aligndown(off, fs->cfg->prog_size);
        pcache->size = 0;
    }

    return 0;
}

static int lfs_bd_erase(lfs_fs_t *fs, lfs_block_t block)
{
    LFS_ASSERT(block < fs->block_count);
    int err = fs->cfg->erase(fs->cfg, block);
    LFS_ASSERT(err <= 0);
    return err;
}

static void lfs_alloc_ckpoint(lfs_fs_t *fs)
{
    fs->lookahead.ckpoint = fs->block_count;
}

static void lfs_alloc_drop(lfs_fs_t *fs)
{
    fs->lookahead.size = 0;
    fs->lookahead.next = 0;
    lfs_alloc_ckpoint(fs);
}

static int lfs_alloc_lookahead(void *p, lfs_block_t block)
{
    lfs_fs_t *fs = (lfs_fs_t *)p;
    lfs_block_t off = ((block - fs->lookahead.start)
            + fs->block_count) % fs->block_count;

    if (off < fs->lookahead.size) {
        fs->lookahead.buffer[off / 8] |= 1U << (off % 8);
    }

    return 0;
}

static int lfs_alloc_scan(lfs_fs_t *fs)
{
    fs->lookahead.start = (fs->lookahead.start + fs->lookahead.next)
            % fs->block_count;
    fs->lookahead.next = 0;
    fs->lookahead.size = lfs_min(8 * fs->cfg->lookahead_size, fs->lookahead.ckpoint);

    memset(fs->lookahead.buffer, 0, fs->cfg->lookahead_size);
    int err = lfs_fs_traverse_(fs, lfs_alloc_lookahead, fs, true);
    if (err) {
        lfs_alloc_drop(fs);
        return err;
    }

    return 0;
}

static int lfs_alloc(lfs_fs_t *fs, lfs_block_t *block)
{
    while (true) {
        // scan our lookahead buffer for free blocks
        while (fs->lookahead.next < fs->lookahead.size) {
            if (!(fs->lookahead.buffer[fs->lookahead.next / 8]
                    & (1U << (fs->lookahead.next % 8)))) {
                // found a free block
                *block = (fs->lookahead.start + fs->lookahead.next) % fs->block_count;

                while (true) {
                    fs->lookahead.next += 1;
                    fs->lookahead.ckpoint -= 1;

                    if (fs->lookahead.next >= fs->lookahead.size
                            || !(fs->lookahead.buffer[fs->lookahead.next / 8]
                                & (1U << (fs->lookahead.next % 8)))) {
                        return 0;
                    }
                }
            }

            fs->lookahead.next += 1;
            fs->lookahead.ckpoint -= 1;
        }

        if (fs->lookahead.ckpoint <= 0) {
            SGL_LOG_TRACE(LFS_FS_NAME " alloc: no more free space");
            return LFS_ERR_NOSPC;
        }

        int err = lfs_alloc_scan(fs);
        if (err) {
            return err;
        }
    }
}

static lfs_stag_t lfs_dir_getslice(lfs_fs_t *fs, const lfs_mdir_t *dir,
        lfs_tag_t gmask, lfs_tag_t gtag,
        lfs_off_t goff, void *gbuffer, lfs_size_t gsize)
{
    lfs_off_t off = dir->off;
    lfs_tag_t ntag = dir->etag;
    lfs_stag_t gdiff = 0;

    // synthetic moves
    if (lfs_gstate_hasmovehere(&fs->gdisk, dir->pair) &&
            lfs_tag_id(gmask) != 0) {
        if (lfs_tag_id(fs->gdisk.tag) == lfs_tag_id(gtag)) {
            return LFS_ERR_NOENT;
        } else if (lfs_tag_id(fs->gdisk.tag) < lfs_tag_id(gtag)) {
            gdiff -= LFS_MKTAG(0, 1, 0);
        }
    }

    while (off >= sizeof(lfs_tag_t) + lfs_tag_dsize(ntag)) {
        off -= lfs_tag_dsize(ntag);
        lfs_tag_t tag = ntag;
        int err = lfs_bd_read(fs,
                NULL, &fs->rcache, sizeof(ntag),
                dir->pair[0], off, &ntag, sizeof(ntag));
        LFS_ASSERT(err <= 0);
        if (err) {
            return err;
        }

        ntag = (lfs_frombe32(ntag) ^ tag) & 0x7fffffff;

        if (lfs_tag_id(gmask) != 0 &&
                lfs_tag_type1(tag) == LFS_TYPE_SPLICE &&
                lfs_tag_id(tag) <= lfs_tag_id(gtag - gdiff)) {
            if (tag == (LFS_MKTAG(LFS_TYPE_CREATE, 0, 0) | (LFS_MKTAG(0, 0x3ff, 0) & (gtag - gdiff)))) {
                return LFS_ERR_NOENT;
            }

            gdiff += LFS_MKTAG(0, lfs_tag_splice(tag), 0);
        }

        if ((gmask & tag) == (gmask & (gtag - gdiff))) {
            if (lfs_tag_isdelete(tag)) {
                return LFS_ERR_NOENT;
            }

            lfs_size_t diff = lfs_min(lfs_tag_size(tag), gsize);
            err = lfs_bd_read(fs, NULL, &fs->rcache, diff, dir->pair[0], off + sizeof(tag) + goff, gbuffer, diff);
            LFS_ASSERT(err <= 0);
            if (err) {
                return err;
            }
            memset((uint8_t *)gbuffer + diff, 0, gsize - diff);
            return tag + gdiff;
        }
    }

    return LFS_ERR_NOENT;
}

static lfs_stag_t lfs_dir_get(lfs_fs_t *fs, const lfs_mdir_t *dir,
        lfs_tag_t gmask, lfs_tag_t gtag, void *buffer)
{
    return lfs_dir_getslice(fs, dir,
            gmask, gtag,
            0, buffer, lfs_tag_size(gtag));
}

static int lfs_dir_getread(lfs_fs_t *fs, const lfs_mdir_t *dir,
        const lfs_cache_t *pcache, lfs_cache_t *rcache, lfs_size_t hint,
        lfs_tag_t gmask, lfs_tag_t gtag,
        lfs_off_t off, void *buffer, lfs_size_t size)
{
    uint8_t *data = (uint8_t *)buffer;
    if (off + size > fs->cfg->block_size) {
        return LFS_ERR_CORRUPT;
    }

    while (size > 0) {
        lfs_size_t diff = size;

        if (pcache && pcache->block == LFS_BLOCK_INLINE &&
                off < pcache->off + pcache->size) {
            if (off >= pcache->off) {
                // is already in pcache?
                diff = lfs_min(diff, pcache->size - (off - pcache->off));
                memcpy(data, &pcache->buffer[off - pcache->off], diff);

                data += diff;
                off += diff;
                size -= diff;
                continue;
            }

            // pcache takes priority
            diff = lfs_min(diff, pcache->off - off);
        }

        if (rcache->block == LFS_BLOCK_INLINE &&
                off < rcache->off + rcache->size) {
            if (off >= rcache->off) {
                // is already in rcache?
                diff = lfs_min(diff, rcache->size - (off - rcache->off));
                memcpy(data, &rcache->buffer[off - rcache->off], diff);

                data += diff;
                off += diff;
                size -= diff;
                continue;
            }

            // rcache takes priority
            diff = lfs_min(diff, rcache->off - off);
        }

        // load to cache, first condition can no longer fail
        rcache->block = LFS_BLOCK_INLINE;
        rcache->off = lfs_aligndown(off, fs->cfg->read_size);
        rcache->size = lfs_min(lfs_alignup(off + hint, fs->cfg->read_size),
                fs->cfg->cache_size);
        int err = lfs_dir_getslice(fs, dir, gmask, gtag,
                rcache->off, rcache->buffer, rcache->size);
        if (err < 0) {
            return err;
        }
    }

    return 0;
}

static int lfs_dir_traverse_filter(void *p,
        lfs_tag_t tag, const void *buffer)
{
    lfs_tag_t *filtertag = (lfs_tag_t *)p;
    (void)buffer;

    // which mask depends on unique bit in tag structure
    uint32_t mask = (tag & LFS_MKTAG(0x100, 0, 0))
            ? LFS_MKTAG(0x7ff, 0x3ff, 0)
            : LFS_MKTAG(0x700, 0x3ff, 0);

    // check for redundancy
    if ((mask & tag) == (mask & *filtertag) ||
            lfs_tag_isdelete(*filtertag) ||
            (LFS_MKTAG(0x7ff, 0x3ff, 0) & tag) == (
                LFS_MKTAG(LFS_TYPE_DELETE, 0, 0) |
                    (LFS_MKTAG(0, 0x3ff, 0) & *filtertag))) {
        *filtertag = LFS_MKTAG(LFS_FROM_NOOP, 0, 0);
        return true;
    }

    // check if we need to adjust for created/deleted tags
    if (lfs_tag_type1(tag) == LFS_TYPE_SPLICE &&
            lfs_tag_id(tag) <= lfs_tag_id(*filtertag)) {
        *filtertag += LFS_MKTAG(0, lfs_tag_splice(tag), 0);
    }

    return false;
}

#define LFS_DIR_TRAVERSE_DEPTH 3

struct lfs_traverse_frame {
    const lfs_mdir_t *dir;
    lfs_off_t off;
    lfs_tag_t ptag;
    const struct lfs_mattr *attrs;
    int attrcount;

    lfs_tag_t tmask;
    lfs_tag_t ttag;
    uint16_t begin;
    uint16_t end;
    int16_t diff;

    int (*cb)(void *data, lfs_tag_t tag, const void *buffer);
    void *data;

    lfs_tag_t tag;
    const void *buffer;
    struct lfs_diskoff disk;
};

static int lfs_dir_traverse(lfs_fs_t *fs,
        const lfs_mdir_t *dir, lfs_off_t off, lfs_tag_t ptag,
        const struct lfs_mattr *attrs, int attrcount,
        lfs_tag_t tmask, lfs_tag_t ttag,
        uint16_t begin, uint16_t end, int16_t diff,
        int (*cb)(void *data, lfs_tag_t tag, const void *buffer), void *data)
{
    struct lfs_traverse_frame stack[LFS_DIR_TRAVERSE_DEPTH - 1];
    unsigned sp = 0;
    int res;
    lfs_tag_t tag;
    const void *buffer;
    struct lfs_diskoff disk = {0};

    while (true) {
        {
            if (off + lfs_tag_dsize(ptag) < dir->off) {
                off += lfs_tag_dsize(ptag);
                int err = lfs_bd_read(fs,
                        NULL, &fs->rcache, sizeof(tag),
                        dir->pair[0], off, &tag, sizeof(tag));
                if (err) {
                    return err;
                }

                tag = (lfs_frombe32(tag) ^ ptag) | 0x80000000;
                disk.block = dir->pair[0];
                disk.off = off + sizeof(lfs_tag_t);
                buffer = &disk;
                ptag = tag;
            } else if (attrcount > 0) {
                tag = attrs[0].tag;
                buffer = attrs[0].buffer;
                attrs += 1;
                attrcount -= 1;
            } else {
                // finished traversal, pop from stack?
                res = 0;
                break;
            }

            // do we need to filter?
            lfs_tag_t mask = LFS_MKTAG(0x7ff, 0, 0);
            if ((mask & tmask & tag) != (mask & tmask & ttag)) {
                continue;
            }

            if (lfs_tag_id(tmask) != 0) {
                LFS_ASSERT(sp < LFS_DIR_TRAVERSE_DEPTH);
                stack[sp] = (struct lfs_traverse_frame){
                    .dir        = dir,
                    .off        = off,
                    .ptag       = ptag,
                    .attrs      = attrs,
                    .attrcount  = attrcount,
                    .tmask      = tmask,
                    .ttag       = ttag,
                    .begin      = begin,
                    .end        = end,
                    .diff       = diff,
                    .cb         = cb,
                    .data       = data,
                    .tag        = tag,
                    .buffer     = buffer,
                    .disk       = disk,
                };
                sp += 1;

                tmask = 0;
                ttag = 0;
                begin = 0;
                end = 0;
                diff = 0;
                cb = lfs_dir_traverse_filter;
                data = &stack[sp - 1].tag;
                continue;
            }
        }

popped:
        // in filter range?
        if (lfs_tag_id(tmask) != 0 &&
                !(lfs_tag_id(tag) >= begin && lfs_tag_id(tag) < end)) {
            continue;
        }

        // handle special cases for mcu-side operations
        if (lfs_tag_type3(tag) == LFS_FROM_NOOP) {
            // do nothing
        } else if (lfs_tag_type3(tag) == LFS_FROM_MOVE) {
            if (cb == lfs_dir_traverse_filter) {
                continue;
            }

            // recurse into move
            stack[sp] = (struct lfs_traverse_frame){
                .dir        = dir,
                .off        = off,
                .ptag       = ptag,
                .attrs      = attrs,
                .attrcount  = attrcount,
                .tmask      = tmask,
                .ttag       = ttag,
                .begin      = begin,
                .end        = end,
                .diff       = diff,
                .cb         = cb,
                .data       = data,
                .tag        = LFS_MKTAG(LFS_FROM_NOOP, 0, 0),
            };
            sp += 1;

            uint16_t fromid = lfs_tag_size(tag);
            uint16_t toid = lfs_tag_id(tag);
            dir = (const lfs_mdir_t *)buffer;
            off = 0;
            ptag = 0xffffffff;
            attrs = NULL;
            attrcount = 0;
            tmask = LFS_MKTAG(0x600, 0x3ff, 0);
            ttag = LFS_MKTAG(LFS_TYPE_STRUCT, 0, 0);
            begin = fromid;
            end = fromid + 1;
            diff = toid - fromid + diff;
        } else if (lfs_tag_type3(tag) == LFS_FROM_USERATTRS) {
            for (unsigned i = 0; i < lfs_tag_size(tag); i++) {
                const struct lfs_attr *a = (const struct lfs_attr *)buffer;
                res = cb(data, LFS_MKTAG(LFS_TYPE_USERATTR + a[i].type,
                        lfs_tag_id(tag) + diff, a[i].size), a[i].buffer);
                if (res < 0) {
                    return res;
                }

                if (res) {
                    break;
                }
            }
        } else {
            res = cb(data, tag + LFS_MKTAG(0, diff, 0), buffer);
            if (res < 0) {
                return res;
            }

            if (res) {
                break;
            }
        }
    }

    if (sp > 0) {
        dir         = stack[sp - 1].dir;
        off         = stack[sp - 1].off;
        ptag        = stack[sp - 1].ptag;
        attrs       = stack[sp - 1].attrs;
        attrcount   = stack[sp - 1].attrcount;
        tmask       = stack[sp - 1].tmask;
        ttag        = stack[sp - 1].ttag;
        begin       = stack[sp - 1].begin;
        end         = stack[sp - 1].end;
        diff        = stack[sp - 1].diff;
        cb          = stack[sp - 1].cb;
        data        = stack[sp - 1].data;
        tag         = stack[sp - 1].tag;
        buffer      = stack[sp - 1].buffer;
        disk        = stack[sp - 1].disk;
        sp -= 1;
        goto popped;
    }

    return res;
}

struct lfs_tortoise_t {
    lfs_block_t pair[2];
    lfs_size_t i;
    lfs_size_t period;
};

static int lfs_tortoise_detectcycles(const lfs_mdir_t *dir, struct lfs_tortoise_t *tortoise)
{
    if (lfs_pair_issync(dir->tail, tortoise->pair)) {
        SGL_LOG_WARN(LFS_FS_NAME " tortoise: cycle detected in tail list");
        return LFS_ERR_CORRUPT;
    }

    if (tortoise->i == tortoise->period) {
        tortoise->pair[0] = dir->tail[0];
        tortoise->pair[1] = dir->tail[1];
        tortoise->i = 0;
        tortoise->period *= 2;
    }

    tortoise->i += 1;
    return LFS_ERR_OK;
}

static lfs_stag_t lfs_dir_fetchmatch(lfs_fs_t *fs, lfs_mdir_t *dir,
        const lfs_block_t pair[2], lfs_tag_t fmask, lfs_tag_t ftag,
        uint16_t *id,
        int (*cb)(void *data, lfs_tag_t tag, const void *buffer), void *data)
{
    // we can find tag very efficiently during a fetch, since we're already
    // scanning the entire directory
    lfs_stag_t besttag = -1;

    // if either block address is invalid we return LFS_ERR_CORRUPT here,
    // otherwise later writes to the pair could fail
    if (fs->block_count
            && (pair[0] >= fs->block_count || pair[1] >= fs->block_count)) {
        return LFS_ERR_CORRUPT;
    }

    // find the block with the most recent revision
    uint32_t revs[2] = {0, 0};
    int r = 0;
    for (int i = 0; i < 2; i++) {
        int err = lfs_bd_read(fs,
                NULL, &fs->rcache, sizeof(revs[i]),
                pair[i], 0, &revs[i], sizeof(revs[i]));
        revs[i] = lfs_fromle32(revs[i]);
        if (err && err != LFS_ERR_CORRUPT) {
            return err;
        }

        if (err != LFS_ERR_CORRUPT &&
                lfs_scmp(revs[i], revs[(i+1)%2]) > 0) {
            r = i;
        }
    }

    dir->pair[0] = pair[(r+0)%2];
    dir->pair[1] = pair[(r+1)%2];
    dir->rev = revs[(r+0)%2];
    dir->off = 0; // nonzero = found some commits

    // now scan tags to fetch the actual dir and find possible match
    for (int i = 0; i < 2; i++) {
        lfs_off_t off = 0;
        lfs_tag_t ptag = 0xffffffff;

        uint16_t tempcount = 0;
        lfs_block_t temptail[2] = {LFS_BLOCK_NULL, LFS_BLOCK_NULL};
        bool tempsplit = false;
        lfs_stag_t tempbesttag = besttag;

        // assume not erased until proven otherwise
        bool maybeerased = false;
        bool hasfcrc = false;
        struct lfs_fcrc fcrc;

        dir->rev = lfs_tole32(dir->rev);
        uint32_t crc = lfs_crc(0xffffffff, &dir->rev, sizeof(dir->rev));
        dir->rev = lfs_fromle32(dir->rev);

        while (true) {
            // extract next tag
            lfs_tag_t tag;
            off += lfs_tag_dsize(ptag);
            int err = lfs_bd_read(fs,
                    NULL, &fs->rcache, fs->cfg->block_size,
                    dir->pair[0], off, &tag, sizeof(tag));
            if (err) {
                if (err == LFS_ERR_CORRUPT) {
                    // can't continue?
                    break;
                }
                return err;
            }

            crc = lfs_crc(crc, &tag, sizeof(tag));
            tag = lfs_frombe32(tag) ^ ptag;

            // next commit not yet programmed?
            if (!lfs_tag_isvalid(tag)) {
                // we only might be erased if the last tag was a crc
                maybeerased = (lfs_tag_type2(ptag) == LFS_TYPE_CCRC);
                break;
            // out of range?
            } else if (off + lfs_tag_dsize(tag) > fs->cfg->block_size) {
                break;
            }

            ptag = tag;

            if (lfs_tag_type2(tag) == LFS_TYPE_CCRC) {
                // check the crc attr
                uint32_t dcrc;
                err = lfs_bd_read(fs,
                        NULL, &fs->rcache, fs->cfg->block_size,
                        dir->pair[0], off+sizeof(tag), &dcrc, sizeof(dcrc));
                if (err) {
                    if (err == LFS_ERR_CORRUPT) {
                        break;
                    }
                    return err;
                }
                dcrc = lfs_fromle32(dcrc);

                if (crc != dcrc) {
                    break;
                }

                // reset the next bit if we need to
                ptag ^= (lfs_tag_t)(lfs_tag_chunk(tag) & 1U) << 31;

                // toss our crc into the filesystem seed for
                // pseudorandom numbers
                fs->seed = lfs_crc(fs->seed, &crc, sizeof(crc));

                // update with what's found so far
                besttag = tempbesttag;
                dir->off = off + lfs_tag_dsize(tag);
                dir->etag = ptag;
                dir->count = tempcount;
                dir->tail[0] = temptail[0];
                dir->tail[1] = temptail[1];
                dir->split = tempsplit;

                // reset crc, hasfcrc
                crc = 0xffffffff;
                continue;
            }

            // crc the entry first, hopefully leaving it in the cache
            err = lfs_bd_crc(fs,
                    NULL, &fs->rcache, fs->cfg->block_size,
                    dir->pair[0], off+sizeof(tag),
                    lfs_tag_dsize(tag)-sizeof(tag), &crc);
            if (err) {
                if (err == LFS_ERR_CORRUPT) {
                    break;
                }
                return err;
            }

            // directory modification tags?
            if (lfs_tag_type1(tag) == LFS_TYPE_NAME) {
                // increase count of files if necessary
                if (lfs_tag_id(tag) >= tempcount) {
                    tempcount = lfs_tag_id(tag) + 1;
                }
            } else if (lfs_tag_type1(tag) == LFS_TYPE_SPLICE) {
                tempcount += lfs_tag_splice(tag);

                if (tag == (LFS_MKTAG(LFS_TYPE_DELETE, 0, 0) |
                        (LFS_MKTAG(0, 0x3ff, 0) & tempbesttag))) {
                    tempbesttag |= 0x80000000;
                } else if (tempbesttag != -1 &&
                        lfs_tag_id(tag) <= lfs_tag_id(tempbesttag)) {
                    tempbesttag += LFS_MKTAG(0, lfs_tag_splice(tag), 0);
                }
            } else if (lfs_tag_type1(tag) == LFS_TYPE_TAIL) {
                tempsplit = (lfs_tag_chunk(tag) & 1);

                err = lfs_bd_read(fs,
                        NULL, &fs->rcache, fs->cfg->block_size,
                        dir->pair[0], off+sizeof(tag), &temptail, 8);
                if (err) {
                    if (err == LFS_ERR_CORRUPT) {
                        break;
                    }
                    return err;
                }
                lfs_pair_fromle32(temptail);
            } else if (lfs_tag_type3(tag) == LFS_TYPE_FCRC) {
                err = lfs_bd_read(fs,
                        NULL, &fs->rcache, fs->cfg->block_size,
                        dir->pair[0], off+sizeof(tag),
                        &fcrc, sizeof(fcrc));
                if (err) {
                    if (err == LFS_ERR_CORRUPT) {
                        break;
                    }
                    return err;
                }

                lfs_fcrc_fromle32(&fcrc);
                hasfcrc = true;
            }

            // found a match for our fetcher?
            if ((fmask & tag) == (fmask & ftag)) {
                LFS_ASSERT(cb != NULL);
                int res = cb(data, tag, &(struct lfs_diskoff){
                        dir->pair[0], off+sizeof(tag)});
                if (res < 0) {
                    if (res == LFS_ERR_CORRUPT) {
                        break;
                    }
                    return res;
                }

                if (res == LFS_CMP_EQ) {
                    // found a match
                    tempbesttag = tag;
                } else if ((LFS_MKTAG(0x7ff, 0x3ff, 0) & tag) ==
                        (LFS_MKTAG(0x7ff, 0x3ff, 0) & tempbesttag)) {
                    // found an identical tag, but contents didn't match
                    // this must mean that our besttag has been overwritten
                    tempbesttag = -1;
                } else if (res == LFS_CMP_GT &&
                        lfs_tag_id(tag) <= lfs_tag_id(tempbesttag)) {
                    // found a greater match, keep track to keep things sorted
                    tempbesttag = tag | 0x80000000;
                }
            }
        }

        // found no valid commits?
        if (dir->off == 0) {
            // try the other block?
            lfs_pair_swap(dir->pair);
            dir->rev = revs[(r+1)%2];
            continue;
        }

        // did we end on a valid commit? we may have an erased block
        dir->erased = false;
        if (maybeerased && dir->off % fs->cfg->prog_size == 0 && hasfcrc) {
            // check for an fcrc matching the next prog's erased state, if
            // this failed most likely a previous prog was interrupted, we
            // need a new erase
            uint32_t fcrc_ = 0xffffffff;
            int err = lfs_bd_crc(fs,
                    NULL, &fs->rcache, fs->cfg->block_size,
                    dir->pair[0], dir->off, fcrc.size, &fcrc_);
            if (err && err != LFS_ERR_CORRUPT) {
                return err;
            }

            // found beginning of erased part?
            dir->erased = (fcrc_ == fcrc.crc);
        }

        // synthetic move
        if (lfs_gstate_hasmovehere(&fs->gdisk, dir->pair)) {
            if (lfs_tag_id(fs->gdisk.tag) == lfs_tag_id(besttag)) {
                besttag |= 0x80000000;
            } else if (besttag != -1 &&
                    lfs_tag_id(fs->gdisk.tag) < lfs_tag_id(besttag)) {
                besttag -= LFS_MKTAG(0, 1, 0);
            }
        }

        if (id) {
            *id = lfs_min(lfs_tag_id(besttag), dir->count);
        }

        if (lfs_tag_isvalid(besttag)) {
            return besttag;
        } else if (lfs_tag_id(besttag) < dir->count) {
            return LFS_ERR_NOENT;
        } else {
            return 0;
        }
    }

    SGL_LOG_ERROR(LFS_FS_NAME " fetch: corrupted dir pair at {%x, %x}",
            (unsigned)dir->pair[0], (unsigned)dir->pair[1]);
    return LFS_ERR_CORRUPT;
}

static int lfs_dir_fetch(lfs_fs_t *fs,
        lfs_mdir_t *dir, const lfs_block_t pair[2])
{
    return (int)lfs_dir_fetchmatch(fs, dir, pair,
            (lfs_tag_t)-1, (lfs_tag_t)-1, NULL, NULL, NULL);
}

static int lfs_dir_getgstate(lfs_fs_t *fs, const lfs_mdir_t *dir,
        lfs_gstate_t *gstate)
{
    lfs_gstate_t temp;
    lfs_stag_t res = lfs_dir_get(fs, dir, LFS_MKTAG(0x7ff, 0, 0),
            LFS_MKTAG(LFS_TYPE_MOVESTATE, 0, sizeof(temp)), &temp);
    if (res < 0 && res != LFS_ERR_NOENT) {
        return res;
    }

    if (res != LFS_ERR_NOENT) {
        // xor together to find resulting gstate
        lfs_gstate_fromle32(&temp);
        lfs_gstate_xor(gstate, &temp);
    }

    return 0;
}

static int lfs_dir_getinfo(lfs_fs_t *fs, const lfs_mdir_t *dir,
        uint16_t id, struct lfs_info *info)
{
    if (id == 0x3ff) {
        // special case for root
        strcpy(info->name, "/");
        info->type = LFS_TYPE_DIR;
        return 0;
    }

    lfs_stag_t tag = lfs_dir_get(fs, dir, LFS_MKTAG(0x780, 0x3ff, 0),
            LFS_MKTAG(LFS_TYPE_NAME, id, fs->name_max+1), info->name);
    if (tag < 0) {
        return (int)tag;
    }

    info->type = lfs_tag_type3(tag);

    struct lfs_ctz ctz;
    tag = lfs_dir_get(fs, dir, LFS_MKTAG(0x700, 0x3ff, 0),
            LFS_MKTAG(LFS_TYPE_STRUCT, id, sizeof(ctz)), &ctz);
    if (tag < 0) {
        return (int)tag;
    }
    lfs_ctz_fromle32(&ctz);

    if (lfs_tag_type3(tag) == LFS_TYPE_CTZSTRUCT) {
        info->size = ctz.size;
    } else if (lfs_tag_type3(tag) == LFS_TYPE_INLINESTRUCT) {
        info->size = lfs_tag_size(tag);
    }

    return 0;
}

struct lfs_dir_find_match {
    lfs_fs_t *fs;
    const void *name;
    lfs_size_t size;
};

static int lfs_dir_find_match(void *data,
        lfs_tag_t tag, const void *buffer)
{
    struct lfs_dir_find_match *name = data;
    lfs_fs_t *fs = name->fs;
    const struct lfs_diskoff *disk = buffer;

    // compare with disk
    lfs_size_t diff = lfs_min(name->size, lfs_tag_size(tag));
    int res = lfs_bd_cmp(fs,
            NULL, &fs->rcache, diff,
            disk->block, disk->off, name->name, diff);
    if (res != LFS_CMP_EQ) {
        return res;
    }

    // only equal if our size is still the same
    if (name->size != lfs_tag_size(tag)) {
        return (name->size < lfs_tag_size(tag))
                ? LFS_CMP_LT : LFS_CMP_GT;
    }

    // found a match!
    return LFS_CMP_EQ;
}

// lfs_dir_find tries to set dir and id even if file is not found
//
// returns:
// - 0                  if file is found
// - LFS_ERR_NOENT     if file or parent is not found
// - LFS_ERR_NOTDIR    if parent is not a dir
//
// on success the returned tag describes the found entry (type in
// type3, id in id10) and dir holds the pair containing it
static lfs_stag_t lfs_dir_find(lfs_fs_t *fs, lfs_mdir_t *dir,
        const char **path, uint16_t *id)
{
    // we reduce path to a single name if we can find it
    const char *name = *path;

    // default to root dir
    lfs_stag_t tag = LFS_MKTAG(LFS_TYPE_DIR, 0x3ff, 0);
    dir->tail[0] = fs->root[0];
    dir->tail[1] = fs->root[1];

    // empty paths are not allowed
    if (*name == '\0') {
        return LFS_ERR_INVAL;
    }

    while (true) {
nextname:
        // skip slashes if we're a directory
        if (lfs_tag_type3(tag) == LFS_TYPE_DIR) {
            name += strspn(name, "/");
        }
        lfs_size_t namelen = (lfs_size_t)strcspn(name, "/");

        // skip '.'
        if (namelen == 1 && memcmp(name, ".", 1) == 0) {
            name += namelen;
            goto nextname;
        }

        // error on unmatched '..', trying to go above root?
        if (namelen == 2 && memcmp(name, "..", 2) == 0) {
            return LFS_ERR_INVAL;
        }

        // skip if matched by '..' in name
        const char *suffix = name + namelen;
        lfs_size_t sufflen;
        int depth = 1;
        while (true) {
            suffix += strspn(suffix, "/");
            sufflen = (lfs_size_t)strcspn(suffix, "/");
            if (sufflen == 0) {
                break;
            }

            if (sufflen == 1 && memcmp(suffix, ".", 1) == 0) {
                // noop
            } else if (sufflen == 2 && memcmp(suffix, "..", 2) == 0) {
                depth -= 1;
                if (depth == 0) {
                    name = suffix + sufflen;
                    goto nextname;
                }
            } else {
                depth += 1;
            }

            suffix += sufflen;
        }

        // found path
        if (*name == '\0') {
            return tag;
        }

        // update what we've found so far
        *path = name;

        // only continue if we're a directory
        if (lfs_tag_type3(tag) != LFS_TYPE_DIR) {
            return LFS_ERR_NOTDIR;
        }

        // grab the entry data
        if (lfs_tag_id(tag) != 0x3ff) {
            lfs_stag_t res = lfs_dir_get(fs, dir,
                    LFS_MKTAG(0x700, 0x3ff, 0),
                    LFS_MKTAG(LFS_TYPE_STRUCT, lfs_tag_id(tag), 8),
                    dir->tail);
            if (res < 0) {
                return res;
            }
            lfs_pair_fromle32(dir->tail);
        }

        // find entry matching name
        while (true) {
            tag = lfs_dir_fetchmatch(fs, dir, dir->tail,
                    LFS_MKTAG(0x780, 0, 0),
                    LFS_MKTAG(LFS_TYPE_NAME, 0, namelen),
                    id,
                    lfs_dir_find_match, &(struct lfs_dir_find_match){
                        fs, name, namelen});
            if (tag < 0) {
                return tag;
            }

            if (tag) {
                break;
            }

            if (!dir->split) {
                return LFS_ERR_NOENT;
            }
        }

        // to next name
        name += namelen;
    }
}

/// Metadata pair commit logic ///
struct lfs_commit {
    lfs_block_t block;
    lfs_off_t off;
    lfs_tag_t ptag;
    uint32_t crc;

    lfs_off_t begin;
    lfs_off_t end;
};

static int lfs_dir_commitprog(lfs_fs_t *fs, struct lfs_commit *commit,
        const void *buffer, lfs_size_t size)
{
    int err = lfs_bd_prog(fs,
            &fs->pcache, &fs->rcache, false,
            commit->block, commit->off ,
            (const uint8_t*)buffer, size);
    if (err) {
        return err;
    }

    commit->crc = lfs_crc(commit->crc, buffer, size);
    commit->off += size;
    return 0;
}

static int lfs_dir_commitattr(lfs_fs_t *fs, struct lfs_commit *commit,
        lfs_tag_t tag, const void *buffer)
{
    // check if we fit
    lfs_size_t dsize = lfs_tag_dsize(tag);
    if (commit->off + dsize > commit->end) {
        return LFS_ERR_NOSPC;
    }

    // write out tag
    lfs_tag_t ntag = lfs_tobe32((tag & 0x7fffffff) ^ commit->ptag);
    int err = lfs_dir_commitprog(fs, commit, &ntag, sizeof(ntag));
    if (err) {
        return err;
    }

    if (!(tag & 0x80000000)) {
        // from memory
        err = lfs_dir_commitprog(fs, commit, buffer, dsize-sizeof(tag));
        if (err) {
            return err;
        }
    } else {
        // from disk
        const struct lfs_diskoff *disk = buffer;
        for (lfs_off_t i = 0; i < dsize-sizeof(tag); i++) {
            // rely on caching to make this efficient
            uint8_t dat;
            err = lfs_bd_read(fs,
                    NULL, &fs->rcache, dsize-sizeof(tag)-i,
                    disk->block, disk->off+i, &dat, 1);
            if (err) {
                return err;
            }

            err = lfs_dir_commitprog(fs, commit, &dat, 1);
            if (err) {
                return err;
            }
        }
    }

    commit->ptag = tag & 0x7fffffff;
    return 0;
}

// finish a commit with checksum tags, spilling into any remaining space.
//
// two shapes are possible:
// - 5-word crc with a trailing fcrc describing the following prog
//   (middle of a block), letting future fetches distinguish erased
//   space from a broken commit
// - 2-word crc with no following prog (end of block)
static int lfs_dir_commitcrc(lfs_fs_t *fs, struct lfs_commit *commit)
{
    // align to program units
    const lfs_off_t end = lfs_alignup(
            lfs_min(commit->off + 5*sizeof(uint32_t), fs->cfg->block_size),
            fs->cfg->prog_size);

    lfs_off_t off1 = 0;
    uint32_t crc1 = 0;

    // create crc tags to fill up remainder of commit, note that
    // padding is not crced, which lets fetches skip padding but
    // makes committing a bit more complicated
    while (commit->off < end) {
        lfs_off_t noff = (
                lfs_min(end - (commit->off+sizeof(lfs_tag_t)), 0x3fe)
                + (commit->off+sizeof(lfs_tag_t)));
        // too large for crc tag? need padding commits
        if (noff < end) {
            noff = lfs_min(noff, end - 5*sizeof(uint32_t));
        }

        // space for fcrc?
        uint8_t eperturb = (uint8_t)-1;
        if (noff >= end && noff <= fs->cfg->block_size - fs->cfg->prog_size) {
            // first read the leading byte, this always contains a bit
            // we can perturb to avoid writes that don't change the fcrc
            int err = lfs_bd_read(fs,
                    NULL, &fs->rcache, fs->cfg->prog_size,
                    commit->block, noff, &eperturb, 1);
            if (err && err != LFS_ERR_CORRUPT) {
                return err;
            }

            // find the expected fcrc, don't bother avoiding a reread
            // of the eperturb, it should still be in our cache
            struct lfs_fcrc fcrc = {
                .size = fs->cfg->prog_size,
                .crc = 0xffffffff
            };
            err = lfs_bd_crc(fs,
                    NULL, &fs->rcache, fs->cfg->prog_size,
                    commit->block, noff, fcrc.size, &fcrc.crc);
            if (err && err != LFS_ERR_CORRUPT) {
                return err;
            }

            lfs_fcrc_tole32(&fcrc);
            err = lfs_dir_commitattr(fs, commit,
                    LFS_MKTAG(LFS_TYPE_FCRC, 0x3ff, sizeof(struct lfs_fcrc)),
                    &fcrc);
            if (err) {
                return err;
            }
        }

        // build commit crc
        struct {
            lfs_tag_t tag;
            uint32_t crc;
        } ccrc;
        lfs_tag_t ntag = LFS_MKTAG(
                LFS_TYPE_CCRC + (((uint8_t)~eperturb) >> 7), 0x3ff,
                noff - (commit->off+sizeof(lfs_tag_t)));
        ccrc.tag = lfs_tobe32(ntag ^ commit->ptag);
        commit->crc = lfs_crc(commit->crc, &ccrc.tag, sizeof(lfs_tag_t));
        ccrc.crc = lfs_tole32(commit->crc);

        int err = lfs_bd_prog(fs,
                &fs->pcache, &fs->rcache, false,
                commit->block, commit->off, &ccrc, sizeof(ccrc));
        if (err) {
            return err;
        }

        // keep track of non-padding checksum to verify
        if (off1 == 0) {
            off1 = commit->off + sizeof(lfs_tag_t);
            crc1 = commit->crc;
        }

        commit->off = noff;
        // perturb valid bit?
        commit->ptag = ntag ^ ((lfs_tag_t)(0x80 & ~eperturb) << 24);
        // reset crc for next commit
        commit->crc = 0xffffffff;

        // manually flush here since we don't prog the padding, this confuses
        // the caching layer
        if (noff >= end || noff >= fs->pcache.off + fs->cfg->cache_size) {
            // flush buffers
            int err = lfs_bd_sync(fs, &fs->pcache, &fs->rcache, false);
            if (err) {
                return err;
            }
        }
    }

    // successful commit, check checksums to make sure
    //
    // note that we don't need to check padding commits, worst
    // case if they are corrupted we would have had to compact anyways
    lfs_off_t off = commit->begin;
    uint32_t crc = 0xffffffff;
    int err = lfs_bd_crc(fs,
            NULL, &fs->rcache, off1+sizeof(uint32_t),
            commit->block, off, off1-off, &crc);
    if (err) {
        return err;
    }

    // check non-padding commits against known crc
    if (crc != crc1) {
        return LFS_ERR_CORRUPT;
    }

    // make sure to check crc in case we happen to pick
    // up an unrelated crc (frozen block?)
    err = lfs_bd_crc(fs,
            NULL, &fs->rcache, sizeof(uint32_t),
            commit->block, off1, sizeof(uint32_t), &crc);
    if (err) {
        return err;
    }

    if (crc != 0) {
        return LFS_ERR_CORRUPT;
    }

    return 0;
}

// allocate a fresh metadata pair, the caller commits the initial state
static int lfs_dir_alloc(lfs_fs_t *fs, lfs_mdir_t *dir)
{
    // allocate pair of dir blocks (backwards, so we write block 1 first)
    for (int i = 0; i < 2; i++) {
        int err = lfs_alloc(fs, &dir->pair[(i+1)%2]);
        if (err) {
            return err;
        }
    }

    // zero for reproducibility in case initial block is unreadable
    dir->rev = 0;

    // rather than clobbering one of the blocks we just pretend
    // the revision may be valid
    int err = lfs_bd_read(fs,
            NULL, &fs->rcache, sizeof(dir->rev),
            dir->pair[0], 0, &dir->rev, sizeof(dir->rev));
    dir->rev = lfs_fromle32(dir->rev);
    if (err && err != LFS_ERR_CORRUPT) {
        return err;
    }

    if (fs->cfg->block_cycles > 0) {
        dir->rev = lfs_alignup(dir->rev, ((fs->cfg->block_cycles+1)|1));
    }

    // set defaults
    dir->off = sizeof(dir->rev);
    dir->etag = 0xffffffff;
    dir->count = 0;
    dir->tail[0] = LFS_BLOCK_NULL;
    dir->tail[1] = LFS_BLOCK_NULL;
    dir->erased = false;
    dir->split = false;

    return 0;
}

// drop a split-off tail, merging its state back into dir
static int lfs_dir_drop(lfs_fs_t *fs, lfs_mdir_t *dir, lfs_mdir_t *tail)
{
    // steal state
    int err = lfs_dir_getgstate(fs, tail, &fs->gdelta);
    if (err) {
        return err;
    }

    // steal tail
    lfs_pair_tole32(tail->tail);
    err = lfs_dir_commit(fs, dir, LFS_MKATTRS(
            {LFS_MKTAG(LFS_TYPE_TAIL + tail->split, 0x3ff, 8), tail->tail}));
    lfs_pair_fromle32(tail->tail);
    if (err) {
        return err;
    }

    return 0;
}

// split off [split, end) of source into a new tail metadata pair
static int lfs_dir_split(lfs_fs_t *fs, lfs_mdir_t *dir,
        const struct lfs_mattr *attrs, int attrcount,
        lfs_mdir_t *source, uint16_t split, uint16_t end)
{
    // create tail metadata pair
    lfs_mdir_t tail;
    int err = lfs_dir_alloc(fs, &tail);
    if (err) {
        return err;
    }

    tail.split = dir->split;
    tail.tail[0] = dir->tail[0];
    tail.tail[1] = dir->tail[1];

    // note we don't care about LFS_OK_RELOCATED
    int res = lfs_dir_compact(fs, &tail, attrs, attrcount, source, split, end);
    if (res < 0) {
        return res;
    }

    dir->tail[0] = tail.pair[0];
    dir->tail[1] = tail.pair[1];
    dir->split = true;

    // update root if needed
    if (lfs_pair_cmp(dir->pair, fs->root) == 0 && split == 0) {
        fs->root[0] = tail.pair[0];
        fs->root[1] = tail.pair[1];
    }

    return 0;
}

// size callback, used to probe how big a split would be
static int lfs_dir_commit_size(void *p, lfs_tag_t tag, const void *buffer)
{
    lfs_size_t *size = p;
    (void)buffer;

    *size += lfs_tag_dsize(tag);
    return 0;
}

struct lfs_dir_commit_commit {
    lfs_fs_t *fs;
    struct lfs_commit *commit;
};

static int lfs_dir_commit_commit(void *p, lfs_tag_t tag, const void *buffer)
{
    struct lfs_dir_commit_commit *commit = p;
    return lfs_dir_commitattr(commit->fs, commit->commit, tag, buffer);
}

// If our revision count == n * block_cycles, we should force a relocation,
// this is how littlefs wear-levels at the metadata-pair level. Note that we
// actually use (block_cycles+1)|1, this is to avoid two corner cases:
// 1. block_cycles = 1, which would prevent relocations from terminating
// 2. block_cycles = 2n, which, due to aliasing, would only ever relocate
//    one metadata block in the pair, effectively making this useless
static bool lfs_dir_needsrelocation(lfs_fs_t *fs, const lfs_mdir_t *dir)
{
    return (fs->cfg->block_cycles > 0
            && ((dir->rev + 1) % ((fs->cfg->block_cycles+1)|1) == 0));
}

/// Metadata pair compaction ///

// rewrite the [begin, end) slice of source, plus attrs, into the second
// block of dir's pair and swap, bumping the revision. Relocates to a new
// block on corruption.
static int lfs_dir_compact(lfs_fs_t *fs,
        lfs_mdir_t *dir, const struct lfs_mattr *attrs, int attrcount,
        lfs_mdir_t *source, uint16_t begin, uint16_t end)
{
    // save some state in case block is bad
    bool relocated = false;
    bool tired = lfs_dir_needsrelocation(fs, dir);

    // increment revision count
    dir->rev += 1;

    if (tired && lfs_pair_cmp(dir->pair, (const lfs_block_t[2]){0, 1}) != 0) {
        // we're writing too much, time to relocate
        goto relocate;
    }

    // begin loop to commit compaction to blocks until a compact sticks
    while (true) {
        {
            // setup commit state
            struct lfs_commit commit = {
                .block = dir->pair[1],
                .off = 0,
                .ptag = 0xffffffff,
                .crc = 0xffffffff,

                .begin = 0,
                .end = (fs->cfg->metadata_max ?
                    fs->cfg->metadata_max : fs->cfg->block_size) - 8,
            };

            // erase block to write to
            int err = lfs_bd_erase(fs, dir->pair[1]);
            if (err) {
                if (err == LFS_ERR_CORRUPT) {
                    goto relocate;
                }
                return err;
            }

            // write out header
            dir->rev = lfs_tole32(dir->rev);
            err = lfs_dir_commitprog(fs, &commit,
                    &dir->rev, sizeof(dir->rev));
            dir->rev = lfs_fromle32(dir->rev);
            if (err) {
                if (err == LFS_ERR_CORRUPT) {
                    goto relocate;
                }
                return err;
            }

            // traverse the directory, this time writing out all unique tags
            err = lfs_dir_traverse(fs,
                    source, 0, 0xffffffff, attrs, attrcount,
                    LFS_MKTAG(0x400, 0x3ff, 0),
                    LFS_MKTAG(LFS_TYPE_NAME, 0, 0),
                    begin, end, -begin,
                    lfs_dir_commit_commit, &(struct lfs_dir_commit_commit){
                        fs, &commit});
            if (err) {
                if (err == LFS_ERR_CORRUPT) {
                    goto relocate;
                }
                return err;
            }

            // commit tail, which may be new after last size check
            if (!lfs_pair_isnull(dir->tail)) {
                lfs_pair_tole32(dir->tail);
                err = lfs_dir_commitattr(fs, &commit,
                        LFS_MKTAG(LFS_TYPE_TAIL + dir->split, 0x3ff, 8),
                        dir->tail);
                lfs_pair_fromle32(dir->tail);
                if (err) {
                    if (err == LFS_ERR_CORRUPT) {
                        goto relocate;
                    }
                    return err;
                }
            }

            // bring over gstate?
            lfs_gstate_t delta = {0};
            if (!relocated) {
                lfs_gstate_xor(&delta, &fs->gdisk);
                lfs_gstate_xor(&delta, &fs->gstate);
            }
            lfs_gstate_xor(&delta, &fs->gdelta);
            delta.tag &= ~LFS_MKTAG(0, 0, 0x3ff);

            err = lfs_dir_getgstate(fs, dir, &delta);
            if (err) {
                return err;
            }

            if (!lfs_gstate_iszero(&delta)) {
                lfs_gstate_tole32(&delta);
                err = lfs_dir_commitattr(fs, &commit,
                        LFS_MKTAG(LFS_TYPE_MOVESTATE, 0x3ff,
                            sizeof(delta)), &delta);
                if (err) {
                    if (err == LFS_ERR_CORRUPT) {
                        goto relocate;
                    }
                    return err;
                }
            }

            // complete commit with crc
            err = lfs_dir_commitcrc(fs, &commit);
            if (err) {
                if (err == LFS_ERR_CORRUPT) {
                    goto relocate;
                }
                return err;
            }

            // successful compaction, swap dir pair to indicate most recent
            LFS_ASSERT(commit.off % fs->cfg->prog_size == 0);
            lfs_pair_swap(dir->pair);
            dir->count = end - begin;
            dir->off = commit.off;
            dir->etag = commit.ptag;
            // update gstate
            fs->gdelta = (lfs_gstate_t){0};
            if (!relocated) {
                fs->gdisk = fs->gstate;
            }
        }
        break;

relocate:
        // commit was corrupted, drop caches and prepare to relocate block
        relocated = true;
        lfs_cache_drop(fs, &fs->pcache);
        if (!tired) {
            SGL_LOG_WARN(LFS_FS_NAME " compact: bad block at %x",
                    (unsigned)dir->pair[1]);
        }

        // can't relocate superblock, filesystem is now frozen
        if (lfs_pair_cmp(dir->pair, (const lfs_block_t[2]){0, 1}) == 0) {
            SGL_LOG_WARN(LFS_FS_NAME " compact: superblock %x has become "
                    "unwritable", (unsigned)dir->pair[1]);
            return LFS_ERR_NOSPC;
        }

        // relocate half of pair
        int err = lfs_alloc(fs, &dir->pair[1]);
        if (err && (err != LFS_ERR_NOSPC || !tired)) {
            return err;
        }

        tired = false;
        continue;
    }

    return relocated ? LFS_OK_RELOCATED : 0;
}

// compact with an optional pre-split of the tag range, halving the range
// until it provably fits in one metadata block
static int lfs_dir_splittingcompact(lfs_fs_t *fs, lfs_mdir_t *dir,
        const struct lfs_mattr *attrs, int attrcount,
        lfs_mdir_t *source, uint16_t begin, uint16_t end)
{
    while (true) {
        // find size of first split, we do this by halving the split until
        // the metadata is guaranteed to fit
        //
        // Note that this isn't a true binary search, we never increase the
        // split size. This may result in poorly distributed metadata but
        // isn't worth the extra code size or performance hit to fix.
        lfs_size_t split = begin;
        while (end - split > 1) {
            lfs_size_t size = 0;
            int err = lfs_dir_traverse(fs,
                    source, 0, 0xffffffff, attrs, attrcount,
                    LFS_MKTAG(0x400, 0x3ff, 0),
                    LFS_MKTAG(LFS_TYPE_NAME, 0, 0),
                    split, end, -split,
                    lfs_dir_commit_size, &size);
            if (err) {
                return err;
            }

            // space is complicated, we need room for:
            //
            // - tail:         4+2*4 = 12 bytes
            // - gstate:       4+3*4 = 16 bytes
            // - move delete:  4     = 4 bytes
            // - crc:          4+4   = 8 bytes
            //                 total = 40 bytes
            //
            // And we cap at half a block to avoid degenerate cases with
            // nearly-full metadata blocks.
            //
            lfs_size_t metadata_max = (fs->cfg->metadata_max)
                    ? fs->cfg->metadata_max
                    : fs->cfg->block_size;
            if (end - split < 0xff
                    && size <= lfs_min(
                        metadata_max - 40,
                        lfs_alignup(
                            metadata_max/2,
                            fs->cfg->prog_size))) {
                break;
            }

            split = split + ((end - split) / 2);
        }

        if (split == begin) {
            // no split needed
            break;
        }

        // split into two metadata pairs and continue
        int err = lfs_dir_split(fs, dir, attrs, attrcount,
                source, split, end);
        if (err && err != LFS_ERR_NOSPC) {
            return err;
        }

        if (err) {
            // we can't allocate a new block, try to compact with degraded
            // performance
            SGL_LOG_WARN(LFS_FS_NAME " splittingcompact: unable to split "
                    "pair {%x, %x}",
                    (unsigned)dir->pair[0], (unsigned)dir->pair[1]);
            break;
        } else {
            end = split;
        }
    }

    if (lfs_dir_needsrelocation(fs, dir)
            && lfs_pair_cmp(dir->pair, (const lfs_block_t[2]){0, 1}) == 0) {
        // oh no! we're writing too much to the superblock,
        // should we expand?
        lfs_ssize_t size = lfs_fs_size_(fs);
        if (size < 0) {
            return size;
        }

        // littlefs cannot reclaim expanded superblocks, so expand cautiously
        //
        // if our filesystem is more than ~88% full, don't expand, this is
        // somewhat arbitrary
        if (fs->block_count - size > fs->block_count/8) {
            SGL_LOG_TRACE(LFS_FS_NAME " splittingcompact: expanding "
                    "superblock at rev %u", (unsigned)dir->rev);
            int err = lfs_dir_split(fs, dir, attrs, attrcount,
                    source, begin, end);
            if (err && err != LFS_ERR_NOSPC) {
                return err;
            }

            if (err) {
                // welp, we tried, if we ran out of space there's not much
                // we can do, we'll error later if we've become frozen
                SGL_LOG_WARN(LFS_FS_NAME " splittingcompact: unable to "
                        "expand superblock");
            } else {
                // duplicate the superblock entry into the new superblock
                end = 1;
            }
        }
    }

    return lfs_dir_compact(fs, dir, attrs, attrcount, source, begin, end);
}

/// Metadata pair committing ///

// attempt an in-place (erased-append) commit first, fall back to a full
// compaction, and fix up any open metadata-pairs affected by splits
static int lfs_dir_relocatingcommit(lfs_fs_t *fs, lfs_mdir_t *dir,
        const lfs_block_t pair[2],
        const struct lfs_mattr *attrs, int attrcount,
        lfs_mdir_t *pdir)
{
    int state = 0;

    // calculate changes to the directory
    bool hasdelete = false;
    for (int i = 0; i < attrcount; i++) {
        if (lfs_tag_type3(attrs[i].tag) == LFS_TYPE_CREATE) {
            dir->count += 1;
        } else if (lfs_tag_type3(attrs[i].tag) == LFS_TYPE_DELETE) {
            LFS_ASSERT(dir->count > 0);
            dir->count -= 1;
            hasdelete = true;
        } else if (lfs_tag_type1(attrs[i].tag) == LFS_TYPE_TAIL) {
            dir->tail[0] = ((lfs_block_t*)attrs[i].buffer)[0];
            dir->tail[1] = ((lfs_block_t*)attrs[i].buffer)[1];
            dir->split = (lfs_tag_chunk(attrs[i].tag) & 1);
            lfs_pair_fromle32(dir->tail);
        }
    }

    // should we actually drop the directory block?
    if (hasdelete && dir->count == 0) {
        LFS_ASSERT(pdir);
        int err = lfs_fs_pred(fs, dir->pair, pdir);
        if (err && err != LFS_ERR_NOENT) {
            return err;
        }

        if (err != LFS_ERR_NOENT && pdir->split) {
            state = LFS_OK_DROPPED;
            goto fixmlist;
        }
    }

    if (dir->erased && dir->count < 0xff) {
        // try to commit
        struct lfs_commit commit = {
            .block = dir->pair[0],
            .off = dir->off,
            .ptag = dir->etag,
            .crc = 0xffffffff,

            .begin = dir->off,
            .end = (fs->cfg->metadata_max ?
                fs->cfg->metadata_max : fs->cfg->block_size) - 8,
        };

        // traverse attrs that need to be written out
        lfs_pair_tole32(dir->tail);
        int err = lfs_dir_traverse(fs,
                dir, dir->off, dir->etag, attrs, attrcount,
                0, 0, 0, 0, 0,
                lfs_dir_commit_commit, &(struct lfs_dir_commit_commit){
                    fs, &commit});
        lfs_pair_fromle32(dir->tail);
        if (err) {
            if (err == LFS_ERR_NOSPC || err == LFS_ERR_CORRUPT) {
                goto compact;
            }
            return err;
        }

        // commit any global diffs if we have any
        lfs_gstate_t delta = {0};
        lfs_gstate_xor(&delta, &fs->gstate);
        lfs_gstate_xor(&delta, &fs->gdisk);
        lfs_gstate_xor(&delta, &fs->gdelta);
        delta.tag &= ~LFS_MKTAG(0, 0, 0x3ff);
        if (!lfs_gstate_iszero(&delta)) {
            err = lfs_dir_getgstate(fs, dir, &delta);
            if (err) {
                return err;
            }

            lfs_gstate_tole32(&delta);
            err = lfs_dir_commitattr(fs, &commit,
                    LFS_MKTAG(LFS_TYPE_MOVESTATE, 0x3ff,
                        sizeof(delta)), &delta);
            if (err) {
                if (err == LFS_ERR_NOSPC || err == LFS_ERR_CORRUPT) {
                    goto compact;
                }
                return err;
            }
        }

        // finalize commit with the crc
        err = lfs_dir_commitcrc(fs, &commit);
        if (err) {
            if (err == LFS_ERR_NOSPC || err == LFS_ERR_CORRUPT) {
                goto compact;
            }
            return err;
        }

        // successful commit, update dir
        LFS_ASSERT(commit.off % fs->cfg->prog_size == 0);
        dir->off = commit.off;
        dir->etag = commit.ptag;
        // and update gstate
        fs->gdisk = fs->gstate;
        fs->gdelta = (lfs_gstate_t){0};

        goto fixmlist;
    }

compact:
    // fall back to compaction
    lfs_cache_drop(fs, &fs->pcache);

    state = lfs_dir_splittingcompact(fs, dir, attrs, attrcount,
            dir, 0, dir->count);
    if (state < 0) {
        return state;
    }

    goto fixmlist;

fixmlist:;
    lfs_block_t oldpair[2] = {pair[0], pair[1]};
    for (struct lfs_mlist *d = fs->mlist; d; d = d->next) {
        if (lfs_pair_cmp(d->m.pair, oldpair) == 0) {
            d->m = *dir;
            if (d->m.pair != pair) {
                for (int i = 0; i < attrcount; i++) {
                    if (lfs_tag_type3(attrs[i].tag) == LFS_TYPE_DELETE &&
                            d->id == lfs_tag_id(attrs[i].tag) &&
                            d->type != LFS_TYPE_DIR) {
                        d->m.pair[0] = LFS_BLOCK_NULL;
                        d->m.pair[1] = LFS_BLOCK_NULL;
                    } else if (lfs_tag_type3(attrs[i].tag) == LFS_TYPE_DELETE
                            && d->id > lfs_tag_id(attrs[i].tag)) {
                        d->id -= 1;
                        if (d->type == LFS_TYPE_DIR) {
                            ((lfs_dir_t*)d)->pos -= 1;
                        }
                    } else if (lfs_tag_type3(attrs[i].tag) == LFS_TYPE_CREATE
                            && d->id >= lfs_tag_id(attrs[i].tag)) {
                        d->id += 1;
                        if (d->type == LFS_TYPE_DIR) {
                            ((lfs_dir_t*)d)->pos += 1;
                        }
                    }
                }
            }

            while (d->id >= d->m.count && d->m.split) {
                // we split and id is on tail now
                if (lfs_pair_cmp(d->m.tail, fs->root) != 0) {
                    d->id -= d->m.count;
                }
                int err = lfs_dir_fetch(fs, &d->m, d->m.tail);
                if (err) {
                    return err;
                }
            }
        }
    }

    return state;
}

// the orphan-aware commit: after a relocating commit succeeds we may still
// owe the filesystem a parent/pred desync fixup (an "orphan"); these are
// flattened here to avoid unbounded recursion
static int lfs_dir_orphaningcommit(lfs_fs_t *fs, lfs_mdir_t *dir,
        const struct lfs_mattr *attrs, int attrcount)
{
    for (lfs_file_t *f = (lfs_file_t*)fs->mlist; f; f = f->next) {
        if (dir != &f->m && lfs_pair_cmp(f->m.pair, dir->pair) == 0 &&
                f->type == LFS_TYPE_REG && (f->flags & LFS_F_INLINE) &&
                f->ctz.size > fs->cfg->cache_size) {
            int err = lfs_file_outline(fs, f);
            if (err) {
                return err;
            }

            err = lfs_file_flush(fs, f);
            if (err) {
                return err;
            }
        }
    }

    lfs_block_t lpair[2] = {dir->pair[0], dir->pair[1]};
    lfs_mdir_t ldir = *dir;
    lfs_mdir_t pdir;
    int state = lfs_dir_relocatingcommit(fs, &ldir, dir->pair,
            attrs, attrcount, &pdir);
    if (state < 0) {
        return state;
    }

    // update if we're not in mlist, note we may have already been
    // updated if we are in mlist
    if (lfs_pair_cmp(dir->pair, lpair) == 0) {
        *dir = ldir;
    }

    if (state == LFS_OK_DROPPED) {
        // steal state
        int err = lfs_dir_getgstate(fs, dir, &fs->gdelta);
        if (err) {
            return err;
        }

        // steal tail, note that this can't create a recursive drop
        lpair[0] = pdir.pair[0];
        lpair[1] = pdir.pair[1];
        lfs_pair_tole32(dir->tail);
        state = lfs_dir_relocatingcommit(fs, &pdir, lpair, LFS_MKATTRS(
                    {LFS_MKTAG(LFS_TYPE_TAIL + dir->split, 0x3ff, 8),
                        dir->tail}),
                NULL);
        lfs_pair_fromle32(dir->tail);
        if (state < 0) {
            return state;
        }

        ldir = pdir;
    }

    // need to relocate?
    bool orphans = false;
    while (state == LFS_OK_RELOCATED) {
        SGL_LOG_TRACE(LFS_FS_NAME " orphaningcommit: relocating {%x, %x} -> "
                "{%x, %x}",
                (unsigned)lpair[0], (unsigned)lpair[1],
                (unsigned)ldir.pair[0], (unsigned)ldir.pair[1]);
        state = 0;

        // update internal root
        if (lfs_pair_cmp(lpair, fs->root) == 0) {
            fs->root[0] = ldir.pair[0];
            fs->root[1] = ldir.pair[1];
        }

        // update internally tracked dirs
        for (struct lfs_mlist *d = fs->mlist; d; d = d->next) {
            if (lfs_pair_cmp(lpair, d->m.pair) == 0) {
                d->m.pair[0] = ldir.pair[0];
                d->m.pair[1] = ldir.pair[1];
            }

            if (d->type == LFS_TYPE_DIR &&
                    lfs_pair_cmp(lpair, ((lfs_dir_t*)d)->head) == 0) {
                ((lfs_dir_t*)d)->head[0] = ldir.pair[0];
                ((lfs_dir_t*)d)->head[1] = ldir.pair[1];
            }
        }

        // find parent
        lfs_stag_t tag = lfs_fs_parent(fs, lpair, &pdir);
        if (tag < 0 && tag != LFS_ERR_NOENT) {
            return tag;
        }

        bool hasparent = (tag != LFS_ERR_NOENT);
        if (tag != LFS_ERR_NOENT) {
            // note that if we have a parent, we must have a pred, so this will
            // always create an orphan
            int err = lfs_fs_preporphans(fs, +1);
            if (err) {
                return err;
            }

            // fix pending move in this pair? this looks like an optimization
            // but is in fact _required_ since relocating may outdate the move.
            uint16_t moveid = 0x3ff;
            if (lfs_gstate_hasmovehere(&fs->gstate, pdir.pair)) {
                moveid = lfs_tag_id(fs->gstate.tag);
                SGL_LOG_TRACE(LFS_FS_NAME " orphaningcommit: fixing move "
                        "while relocating {%x, %x} id %x",
                        (unsigned)pdir.pair[0], (unsigned)pdir.pair[1],
                        (unsigned)moveid);
                lfs_fs_prepmove(fs, 0x3ff, NULL);
                if (moveid < lfs_tag_id(tag)) {
                    tag -= LFS_MKTAG(0, 1, 0);
                }
            }

            lfs_block_t ppair[2] = {pdir.pair[0], pdir.pair[1]};
            lfs_pair_tole32(ldir.pair);
            state = lfs_dir_relocatingcommit(fs, &pdir, ppair, LFS_MKATTRS(
                        {LFS_MKTAG_IF(moveid != 0x3ff,
                            LFS_TYPE_DELETE, moveid, 0), NULL},
                        {tag, ldir.pair}),
                    NULL);
            lfs_pair_fromle32(ldir.pair);
            if (state < 0) {
                return state;
            }

            if (state == LFS_OK_RELOCATED) {
                lpair[0] = ppair[0];
                lpair[1] = ppair[1];
                ldir = pdir;
                orphans = true;
                continue;
            }
        }

        // find pred
        int err = lfs_fs_pred(fs, lpair, &pdir);
        if (err && err != LFS_ERR_NOENT) {
            return err;
        }
        LFS_ASSERT(!(hasparent && err == LFS_ERR_NOENT));

        // if we can't find dir, it must be new
        if (err != LFS_ERR_NOENT) {
            if (lfs_gstate_hasorphans(&fs->gstate)) {
                // next step, clean up orphans
                err = lfs_fs_preporphans(fs, -(int8_t)hasparent);
                if (err) {
                    return err;
                }
            }

            // fix pending move in this pair? this looks like an optimization
            // but is in fact _required_ since relocating may outdate the move.
            uint16_t moveid = 0x3ff;
            if (lfs_gstate_hasmovehere(&fs->gstate, pdir.pair)) {
                moveid = lfs_tag_id(fs->gstate.tag);
                SGL_LOG_TRACE(LFS_FS_NAME " orphaningcommit: fixing move "
                        "while relocating {%x, %x} id %x",
                        (unsigned)pdir.pair[0], (unsigned)pdir.pair[1],
                        (unsigned)moveid);
                lfs_fs_prepmove(fs, 0x3ff, NULL);
            }

            // replace bad pair, either we clean up desync, or no desync
            // occured
            lpair[0] = pdir.pair[0];
            lpair[1] = pdir.pair[1];
            lfs_pair_tole32(ldir.pair);
            state = lfs_dir_relocatingcommit(fs, &pdir, lpair, LFS_MKATTRS(
                        {LFS_MKTAG_IF(moveid != 0x3ff,
                            LFS_TYPE_DELETE, moveid, 0), NULL},
                        {LFS_MKTAG(LFS_TYPE_TAIL + pdir.split, 0x3ff, 8),
                            ldir.pair}),
                    NULL);
            lfs_pair_fromle32(ldir.pair);
            if (state < 0) {
                return state;
            }

            ldir = pdir;
        }
    }

    return orphans ? LFS_OK_ORPHANED : 0;
}

// public entry point for all metadata commits
static int lfs_dir_commit(lfs_fs_t *fs, lfs_mdir_t *dir,
        const struct lfs_mattr *attrs, int attrcount)
{
    int orphans = lfs_dir_orphaningcommit(fs, dir, attrs, attrcount);
    if (orphans < 0) {
        return orphans;
    }

    if (orphans) {
        // make sure we've removed all orphans, this is a noop if there
        // are none, but if we had nested blocks failures we may have
        // created some
        int err = lfs_fs_deorphan(fs, false);
        if (err) {
            return err;
        }
    }

    return 0;
}

/// Top level directory operations ///
static int lfs_mkdir_(lfs_fs_t *fs, const char *path)
{
    // deorphan if we haven't yet, needed at most once after poweron
    int err = lfs_fs_forceconsistency(fs);
    if (err) {
        return err;
    }

    struct lfs_mlist cwd;
    cwd.next = fs->mlist;
    uint16_t id;
    err = lfs_dir_find(fs, &cwd.m, &path, &id);
    if (!(err == LFS_ERR_NOENT && lfs_path_islast(path))) {
        return (err < 0) ? err : LFS_ERR_EXIST;
    }

    // check that name fits
    lfs_size_t nlen = lfs_path_namelen(path);
    if (nlen > fs->name_max) {
        return LFS_ERR_NAMETOOLONG;
    }

    // build up new directory
    lfs_alloc_ckpoint(fs);
    lfs_mdir_t dir;
    err = lfs_dir_alloc(fs, &dir);
    if (err) {
        return err;
    }

    // find end of list
    lfs_mdir_t pred = cwd.m;
    while (pred.split) {
        err = lfs_dir_fetch(fs, &pred, pred.tail);
        if (err) {
            return err;
        }
    }

    // setup dir
    lfs_pair_tole32(pred.tail);
    err = lfs_dir_commit(fs, &dir, LFS_MKATTRS(
            {LFS_MKTAG(LFS_TYPE_SOFTTAIL, 0x3ff, 8), pred.tail}));
    lfs_pair_fromle32(pred.tail);
    if (err) {
        return err;
    }

    // current block not end of list?
    if (cwd.m.split) {
        // update tails, this creates a desync
        err = lfs_fs_preporphans(fs, +1);
        if (err) {
            return err;
        }

        // it's possible our predecessor has to be relocated, and if
        // our parent is our predecessor's predecessor, this could have
        // caused our parent to go out of date, fortunately we can hook
        // ourselves into the filesystem to catch this
        cwd.type = 0;
        cwd.id = 0;
        fs->mlist = &cwd;

        lfs_pair_tole32(dir.pair);
        err = lfs_dir_commit(fs, &pred, LFS_MKATTRS(
                {LFS_MKTAG(LFS_TYPE_SOFTTAIL, 0x3ff, 8), dir.pair}));
        lfs_pair_fromle32(dir.pair);
        if (err) {
            fs->mlist = cwd.next;
            return err;
        }

        fs->mlist = cwd.next;
        err = lfs_fs_preporphans(fs, -1);
        if (err) {
            return err;
        }
    }

    // now insert into our parent block
    lfs_pair_tole32(dir.pair);
    err = lfs_dir_commit(fs, &cwd.m, LFS_MKATTRS(
            {LFS_MKTAG(LFS_TYPE_CREATE, id, 0), NULL},
            {LFS_MKTAG(LFS_TYPE_DIR, id, nlen), path},
            {LFS_MKTAG(LFS_TYPE_DIRSTRUCT, id, 8), dir.pair},
            {LFS_MKTAG_IF(!cwd.m.split,
                LFS_TYPE_SOFTTAIL, 0x3ff, 8), dir.pair}));
    lfs_pair_fromle32(dir.pair);
    if (err) {
        return err;
    }

    return 0;
}

static int lfs_dir_open_(lfs_fs_t *fs, lfs_dir_t *dir, const char *path)
{
    lfs_stag_t tag = lfs_dir_find(fs, &dir->m, &path, NULL);
    if (tag < 0) {
        return tag;
    }

    if (lfs_tag_type3(tag) != LFS_TYPE_DIR) {
        return LFS_ERR_NOTDIR;
    }

    lfs_block_t pair[2];
    if (lfs_tag_id(tag) == 0x3ff) {
        // handle root dir separately
        pair[0] = fs->root[0];
        pair[1] = fs->root[1];
    } else {
        // get dir pair from parent
        lfs_stag_t res = lfs_dir_get(fs, &dir->m,
                LFS_MKTAG(0x700, 0x3ff, 0),
                LFS_MKTAG(LFS_TYPE_STRUCT, lfs_tag_id(tag), 8), pair);
        if (res < 0) {
            return res;
        }
        lfs_pair_fromle32(pair);
    }

    // fetch first pair
    int err = lfs_dir_fetch(fs, &dir->m, pair);
    if (err) {
        return err;
    }

    // setup entry
    dir->head[0] = dir->m.pair[0];
    dir->head[1] = dir->m.pair[1];
    dir->id = 0;
    dir->pos = 0;

    // add to list of mdirs
    dir->type = LFS_TYPE_DIR;
    lfs_mlist_append(fs, (struct lfs_mlist *)dir);

    return 0;
}

static int lfs_dir_close_(lfs_fs_t *fs, lfs_dir_t *dir)
{
    // remove from list of mdirs
    lfs_mlist_remove(fs, (struct lfs_mlist *)dir);

    return 0;
}

static int lfs_dir_read_(lfs_fs_t *fs, lfs_dir_t *dir,
        struct lfs_info *info)
{
    memset(info, 0, sizeof(*info));

    // special offset for '.' and '..'
    if (dir->pos == 0) {
        info->type = LFS_TYPE_DIR;
        strcpy(info->name, ".");
        dir->pos += 1;
        return true;
    } else if (dir->pos == 1) {
        info->type = LFS_TYPE_DIR;
        strcpy(info->name, "..");
        dir->pos += 1;
        return true;
    }

    while (true) {
        if (dir->id == dir->m.count) {
            if (!dir->m.split) {
                return false;
            }

            int err = lfs_dir_fetch(fs, &dir->m, dir->m.tail);
            if (err) {
                return err;
            }

            dir->id = 0;
        }

        int err = lfs_dir_getinfo(fs, &dir->m, dir->id, info);
        if (err && err != LFS_ERR_NOENT) {
            return err;
        }

        dir->id += 1;
        if (err != LFS_ERR_NOENT) {
            break;
        }
    }

    dir->pos += 1;
    return true;
}

/// File index list operations ///
//
// files are CTZ skip-lists: block i stores pointers to blocks
// i - 2**j for each trailing zero j of i, at the block head, with the
// remaining space holding data
static int lfs_ctz_index(lfs_fs_t *fs, lfs_off_t *off)
{
    lfs_off_t size = *off;
    lfs_off_t b = fs->cfg->block_size - 2*4;
    lfs_off_t i = size / b;
    if (i == 0) {
        return 0;
    }

    i = (size - 4*(lfs_popc(i-1)+2)) / b;
    *off = size - b*i - 4*lfs_popc(i);
    return i;
}

static int lfs_ctz_find(lfs_fs_t *fs,
        const lfs_cache_t *pcache, lfs_cache_t *rcache,
        lfs_block_t head, lfs_size_t size,
        lfs_size_t pos, lfs_block_t *block, lfs_off_t *off)
{
    if (size == 0) {
        *block = LFS_BLOCK_NULL;
        *off = 0;
        return 0;
    }

    lfs_off_t current = lfs_ctz_index(fs, &(lfs_off_t){size-1});
    lfs_off_t target = lfs_ctz_index(fs, &pos);

    while (current > target) {
        lfs_size_t skip = lfs_min(
                lfs_npw2(current-target+1) - 1,
                lfs_ctz(current));

        int err = lfs_bd_read(fs,
                pcache, rcache, sizeof(head),
                head, 4*skip, &head, sizeof(head));
        head = lfs_fromle32(head);
        if (err) {
            return err;
        }

        current -= 1 << skip;
    }

    *block = head;
    *off = pos;
    return 0;
}

static int lfs_ctz_extend(lfs_fs_t *fs,
        lfs_cache_t *pcache, lfs_cache_t *rcache,
        lfs_block_t head, lfs_size_t size,
        lfs_block_t *block, lfs_off_t *off)
{
    while (true) {
        // go ahead and grab a block
        lfs_block_t nblock;
        int err = lfs_alloc(fs, &nblock);
        if (err) {
            return err;
        }

        {
            err = lfs_bd_erase(fs, nblock);
            if (err) {
                if (err == LFS_ERR_CORRUPT) {
                    goto relocate;
                }
                return err;
            }

            if (size == 0) {
                *block = nblock;
                *off = 0;
                return 0;
            }

            lfs_size_t noff = size - 1;
            lfs_off_t index = lfs_ctz_index(fs, &noff);
            noff = noff + 1;

            // just copy out the last block if it is incomplete
            if (noff != fs->cfg->block_size) {
                for (lfs_off_t i = 0; i < noff; i++) {
                    uint8_t data;
                    err = lfs_bd_read(fs,
                            NULL, rcache, noff-i,
                            head, i, &data, 1);
                    if (err) {
                        return err;
                    }

                    err = lfs_bd_prog(fs,
                            pcache, rcache, true,
                            nblock, i, &data, 1);
                    if (err) {
                        if (err == LFS_ERR_CORRUPT) {
                            goto relocate;
                        }
                        return err;
                    }
                }

                *block = nblock;
                *off = noff;
                return 0;
            }

            // append block
            index += 1;
            lfs_size_t skips = lfs_ctz(index) + 1;
            lfs_block_t nhead = head;
            for (lfs_off_t i = 0; i < skips; i++) {
                nhead = lfs_tole32(nhead);
                err = lfs_bd_prog(fs, pcache, rcache, true,
                        nblock, 4*i, &nhead, 4);
                nhead = lfs_fromle32(nhead);
                if (err) {
                    if (err == LFS_ERR_CORRUPT) {
                        goto relocate;
                    }
                    return err;
                }

                if (i != skips-1) {
                    err = lfs_bd_read(fs,
                            NULL, rcache, sizeof(nhead),
                            nhead, 4*i, &nhead, sizeof(nhead));
                    nhead = lfs_fromle32(nhead);
                    if (err) {
                        return err;
                    }
                }
            }

            *block = nblock;
            *off = 4*skips;
            return 0;
        }

relocate:
        SGL_LOG_TRACE(LFS_FS_NAME " ctz_extend: bad block at %x",
                (unsigned)nblock);

        // just clear cache and try a new block
        lfs_cache_drop(fs, pcache);
    }
}

static int lfs_ctz_traverse(lfs_fs_t *fs,
        const lfs_cache_t *pcache, lfs_cache_t *rcache,
        lfs_block_t head, lfs_size_t size,
        int (*cb)(void*, lfs_block_t), void *data)
{
    if (size == 0) {
        return 0;
    }

    lfs_off_t index = lfs_ctz_index(fs, &(lfs_off_t){size-1});

    while (true) {
        int err = cb(data, head);
        if (err) {
            return err;
        }

        if (index == 0) {
            return 0;
        }

        lfs_block_t heads[2];
        int count = 2 - (index & 1);
        err = lfs_bd_read(fs,
                pcache, rcache, count*sizeof(head),
                head, 0, &heads, count*sizeof(head));
        heads[0] = lfs_fromle32(heads[0]);
        heads[1] = lfs_fromle32(heads[1]);
        if (err) {
            return err;
        }

        for (int i = 0; i < count-1; i++) {
            err = cb(data, heads[i]);
            if (err) {
                return err;
            }
        }

        head = heads[count-1];
        index -= count;
    }
}

/// Top level file operations ///
static int lfs_file_opencfg_(lfs_fs_t *fs, lfs_file_t *file,
        const char *path, int flags,
        const struct lfs_file_config *cfg)
{
    // deorphan if we haven't yet, needed at most once when opening
    // for write
    if ((flags & LFS_O_WRONLY) == LFS_O_WRONLY) {
        int err = lfs_fs_forceconsistency(fs);
        if (err) {
            return err;
        }
    }

    // setup simple file details
    int err;
    file->cfg = cfg;
    file->flags = flags;
    file->pos = 0;
    file->off = 0;
    file->cache.buffer = NULL;

    // allocate entry for file if it doesn't exist
    lfs_stag_t tag = lfs_dir_find(fs, &file->m, &path, &file->id);
    if (tag < 0 && !(tag == LFS_ERR_NOENT && lfs_path_islast(path))) {
        err = tag;
        goto cleanup;
    }

    // get id, add to list of mdirs to catch update changes
    file->type = LFS_TYPE_REG;
    lfs_mlist_append(fs, (struct lfs_mlist *)file);

    if (tag == LFS_ERR_NOENT) {
        if (!(flags & LFS_O_CREAT)) {
            err = LFS_ERR_NOENT;
            goto cleanup;
        }

        // don't allow trailing slashes
        if (lfs_path_isdir(path)) {
            err = LFS_ERR_NOTDIR;
            goto cleanup;
        }

        // check that name fits
        lfs_size_t nlen = lfs_path_namelen(path);
        if (nlen > fs->name_max) {
            err = LFS_ERR_NAMETOOLONG;
            goto cleanup;
        }

        // get next slot and create entry to remember name
        err = lfs_dir_commit(fs, &file->m, LFS_MKATTRS(
                {LFS_MKTAG(LFS_TYPE_CREATE, file->id, 0), NULL},
                {LFS_MKTAG(LFS_TYPE_REG, file->id, nlen), path},
                {LFS_MKTAG(LFS_TYPE_INLINESTRUCT, file->id, 0), NULL}));

        // it may happen that the file name doesn't fit in the metadata
        // blocks, e.g., a 256 byte file name will not fit in a 128 byte
        // block.
        err = (err == LFS_ERR_NOSPC) ? LFS_ERR_NAMETOOLONG : err;
        if (err) {
            goto cleanup;
        }

        tag = LFS_MKTAG(LFS_TYPE_INLINESTRUCT, 0, 0);
    } else if (flags & LFS_O_EXCL) {
        err = LFS_ERR_EXIST;
        goto cleanup;
    } else if (lfs_tag_type3(tag) != LFS_TYPE_REG) {
        err = LFS_ERR_ISDIR;
        goto cleanup;
    } else if (flags & LFS_O_TRUNC) {
        // truncate if requested
        tag = LFS_MKTAG(LFS_TYPE_INLINESTRUCT, file->id, 0);
        file->flags |= LFS_F_DIRTY;
    } else {
        // try to load what's on disk, if it's inlined we'll fix it later
        tag = lfs_dir_get(fs, &file->m, LFS_MKTAG(0x700, 0x3ff, 0),
                LFS_MKTAG(LFS_TYPE_STRUCT, file->id, 8), &file->ctz);
        if (tag < 0) {
            err = tag;
            goto cleanup;
        }
        lfs_ctz_fromle32(&file->ctz);
    }

    // fetch attrs
    for (unsigned i = 0; i < file->cfg->attr_count; i++) {
        // if opened for read / read-write operations
        if ((file->flags & 0x3) != LFS_O_WRONLY) {
            lfs_stag_t res = lfs_dir_get(fs, &file->m,
                    LFS_MKTAG(0x7ff, 0x3ff, 0),
                    LFS_MKTAG(LFS_TYPE_USERATTR + file->cfg->attrs[i].type,
                        file->id, file->cfg->attrs[i].size),
                        file->cfg->attrs[i].buffer);
            if (res < 0 && res != LFS_ERR_NOENT) {
                err = res;
                goto cleanup;
            }
        }

        // if opened for write / read-write operations
        if ((file->flags & 0x3) != LFS_O_RDONLY) {
            if (file->cfg->attrs[i].size > fs->attr_max) {
                err = LFS_ERR_NOSPC;
                goto cleanup;
            }

            file->flags |= LFS_F_DIRTY;
        }
    }

    // allocate buffer if needed
    if (file->cfg->buffer) {
        file->cache.buffer = file->cfg->buffer;
    } else {
        file->cache.buffer = sgl_malloc(fs->cfg->cache_size);
        if (!file->cache.buffer) {
            err = LFS_ERR_NOMEM;
            goto cleanup;
        }
    }

    // zero to avoid information leak
    lfs_cache_zero(fs, &file->cache);

    if (lfs_tag_type3(tag) == LFS_TYPE_INLINESTRUCT) {
        // load inline files
        file->ctz.head = LFS_BLOCK_INLINE;
        file->ctz.size = lfs_tag_size(tag);
        file->flags |= LFS_F_INLINE;
        file->cache.block = file->ctz.head;
        file->cache.off = 0;
        file->cache.size = fs->cfg->cache_size;

        // don't always read (may be new/trunc file)
        if (file->ctz.size > 0) {
            lfs_stag_t res = lfs_dir_get(fs, &file->m,
                    LFS_MKTAG(0x700, 0x3ff, 0),
                    LFS_MKTAG(LFS_TYPE_STRUCT, file->id,
                        lfs_min(file->cache.size, 0x3fe)),
                    file->cache.buffer);
            if (res < 0) {
                err = res;
                goto cleanup;
            }
        }
    }

    return 0;

cleanup:
    // clean up lingering resources
    file->flags |= LFS_F_ERRED;
    lfs_file_close_(fs, file);
    return err;
}

static int lfs_file_open_(lfs_fs_t *fs, lfs_file_t *file,
        const char *path, int flags)
{
    static const struct lfs_file_config defaults = {0};
    int err = lfs_file_opencfg_(fs, file, path, flags, &defaults);
    return err;
}

static int lfs_file_close_(lfs_fs_t *fs, lfs_file_t *file)
{
    int err = 0;
    // it's not safe to do anything if our file errored
    if (!(file->flags & LFS_F_ERRED)) {
        err = lfs_file_sync_(fs, file);
    }

    // remove from list of mdirs
    lfs_mlist_remove(fs, (struct lfs_mlist*)file);

    // clean up memory
    if (!file->cfg->buffer) {
        sgl_free(file->cache.buffer);
    }

    return err;
}

static int lfs_file_relocate(lfs_fs_t *fs, lfs_file_t *file)
{
    while (true) {
        // just relocate what exists into new block
        lfs_block_t nblock;
        int err = lfs_alloc(fs, &nblock);
        if (err) {
            return err;
        }

        err = lfs_bd_erase(fs, nblock);
        if (err) {
            if (err == LFS_ERR_CORRUPT) {
                goto relocate;
            }
            return err;
        }

        // either read from dirty cache or disk
        for (lfs_off_t i = 0; i < file->off; i++) {
            uint8_t data;
            if (file->flags & LFS_F_INLINE) {
                err = lfs_dir_getread(fs, &file->m,
                        // note we evict inline files before they can be dirty
                        NULL, &file->cache, file->off-i,
                        LFS_MKTAG(0xfff, 0x1ff, 0),
                        LFS_MKTAG(LFS_TYPE_INLINESTRUCT, file->id, 0),
                        i, &data, 1);
                if (err) {
                    return err;
                }
            } else {
                err = lfs_bd_read(fs,
                        &file->cache, &fs->rcache, file->off-i,
                        file->block, i, &data, 1);
                if (err) {
                    return err;
                }
            }

            err = lfs_bd_prog(fs,
                    &fs->pcache, &fs->rcache, true,
                    nblock, i, &data, 1);
            if (err) {
                if (err == LFS_ERR_CORRUPT) {
                    goto relocate;
                }
                return err;
            }
        }

        // copy over new state of file
        memcpy(file->cache.buffer, fs->pcache.buffer, fs->cfg->cache_size);
        file->cache.block = fs->pcache.block;
        file->cache.off = fs->pcache.off;
        file->cache.size = fs->pcache.size;
        lfs_cache_zero(fs, &fs->pcache);

        file->block = nblock;
        file->flags |= LFS_F_WRITING;
        return 0;

relocate:
        SGL_LOG_TRACE(LFS_FS_NAME " file_relocate: bad block at %x",
                (unsigned)nblock);

        // just clear cache and try a new block
        lfs_cache_drop(fs, &fs->pcache);
    }
}

static int lfs_file_outline(lfs_fs_t *fs, lfs_file_t *file)
{
    file->off = file->pos;
    lfs_alloc_ckpoint(fs);
    int err = lfs_file_relocate(fs, file);
    if (err) {
        return err;
    }

    file->flags &= ~LFS_F_INLINE;
    return 0;
}

static int lfs_file_flush(lfs_fs_t *fs, lfs_file_t *file)
{
    if (file->flags & LFS_F_READING) {
        if (!(file->flags & LFS_F_INLINE)) {
            lfs_cache_drop(fs, &file->cache);
        }
        file->flags &= ~LFS_F_READING;
    }

    if (file->flags & LFS_F_WRITING) {
        lfs_off_t pos = file->pos;

        if (!(file->flags & LFS_F_INLINE)) {
            // copy over anything after current branch
            lfs_file_t orig = {
                .ctz.head = file->ctz.head,
                .ctz.size = file->ctz.size,
                .flags = LFS_O_RDONLY,
                .pos = file->pos,
                .cache = fs->rcache,
            };
            lfs_cache_drop(fs, &fs->rcache);

            while (file->pos < file->ctz.size) {
                // copy over a byte at a time, leave it up to caching
                // to make this efficient
                uint8_t data;
                lfs_ssize_t res = lfs_file_flushedread(fs, &orig, &data, 1);
                if (res < 0) {
                    return res;
                }

                res = lfs_file_flushedwrite(fs, file, &data, 1);
                if (res < 0) {
                    return res;
                }

                // keep our reference to the rcache in sync
                if (fs->rcache.block != LFS_BLOCK_NULL) {
                    lfs_cache_drop(fs, &orig.cache);
                    lfs_cache_drop(fs, &fs->rcache);
                }
            }

            // write out what we have
            while (true) {
                int err = lfs_bd_flush(fs, &file->cache, &fs->rcache, true);
                if (err) {
                    if (err == LFS_ERR_CORRUPT) {
                        goto relocate;
                    }
                    return err;
                }

                break;

relocate:
                SGL_LOG_TRACE(LFS_FS_NAME " file_flush: bad block at %x",
                        (unsigned)file->block);
                err = lfs_file_relocate(fs, file);
                if (err) {
                    return err;
                }
            }
        } else {
            file->pos = lfs_max(file->pos, file->ctz.size);
        }

        // actual file updates
        file->ctz.head = file->block;
        file->ctz.size = file->pos;
        file->flags &= ~LFS_F_WRITING;
        file->flags |= LFS_F_DIRTY;

        file->pos = pos;
    }

    return 0;
}

static int lfs_file_sync_(lfs_fs_t *fs, lfs_file_t *file)
{
    int err = lfs_file_flush(fs, file);
    if (err) {
        file->flags |= LFS_F_ERRED;
        return err;
    }

    if ((file->flags & LFS_F_DIRTY) &&
            !lfs_pair_isnull(file->m.pair)) {
        // before we commit metadata, we need sync the disk to make sure
        // data writes don't complete after metadata writes
        if (!(file->flags & LFS_F_INLINE)) {
            err = lfs_bd_sync(fs, &fs->pcache, &fs->rcache, false);
            if (err) {
                return err;
            }
        }

        // update dir entry
        uint16_t type;
        const void *buffer;
        lfs_size_t size;
        struct lfs_ctz ctz;
        if (file->flags & LFS_F_INLINE) {
            // inline the whole file
            type = LFS_TYPE_INLINESTRUCT;
            buffer = file->cache.buffer;
            size = file->ctz.size;
        } else {
            // update the ctz reference
            type = LFS_TYPE_CTZSTRUCT;
            // copy ctz so alloc will work during a relocate
            ctz = file->ctz;
            lfs_ctz_tole32(&ctz);
            buffer = &ctz;
            size = sizeof(ctz);
        }

        // commit file data and attributes
        err = lfs_dir_commit(fs, &file->m, LFS_MKATTRS(
                {LFS_MKTAG(type, file->id, size), buffer},
                {LFS_MKTAG(LFS_FROM_USERATTRS, file->id,
                    file->cfg->attr_count), file->cfg->attrs}));
        if (err) {
            file->flags |= LFS_F_ERRED;
            return err;
        }

        file->flags &= ~LFS_F_DIRTY;
    }

    // mark any other file handles as dirty + desync
    for (lfs_file_t *f = (lfs_file_t*)fs->mlist; f; f = f->next) {
        if (file != f
                && f->type == LFS_TYPE_REG
                && lfs_pair_cmp(f->m.pair, file->m.pair) == 0
                && f->id == file->id) {
            f->flags |= LFS_F_DUSTY;
        }
    }

    file->flags &= ~LFS_F_ERRED & ~LFS_F_DUSTY;
    return 0;
}

static lfs_ssize_t lfs_file_flushedread(lfs_fs_t *fs, lfs_file_t *file,
        void *buffer, lfs_size_t size)
{
    uint8_t *data = (uint8_t *)buffer;
    lfs_size_t nsize = size;

    if (file->pos >= file->ctz.size) {
        // eof if past end
        return 0;
    }

    size = lfs_min(size, file->ctz.size - file->pos);
    nsize = size;

    while (nsize > 0) {
        // check if we need a new block
        if (!(file->flags & LFS_F_READING) ||
                file->off == fs->cfg->block_size) {
            if (!(file->flags & LFS_F_INLINE)) {
                int err = lfs_ctz_find(fs, NULL, &file->cache,
                        file->ctz.head, file->ctz.size,
                        file->pos, &file->block, &file->off);
                if (err) {
                    return err;
                }
            } else {
                file->block = LFS_BLOCK_INLINE;
                file->off = file->pos;
            }

            file->flags |= LFS_F_READING;
        }

        // read as much as we can in current block
        lfs_size_t diff = lfs_min(nsize, fs->cfg->block_size - file->off);
        if (file->flags & LFS_F_INLINE) {
            int err = lfs_dir_getread(fs, &file->m,
                    NULL, &file->cache, fs->cfg->block_size,
                    LFS_MKTAG(0xfff, 0x1ff, 0),
                    LFS_MKTAG(LFS_TYPE_INLINESTRUCT, file->id, 0),
                    file->off, data, diff);
            if (err) {
                return err;
            }
        } else {
            int err = lfs_bd_read(fs,
                    NULL, &file->cache, fs->cfg->block_size,
                    file->block, file->off, data, diff);
            if (err) {
                return err;
            }
        }

        file->pos += diff;
        file->off += diff;
        data += diff;
        nsize -= diff;
    }

    return size;
}

static lfs_ssize_t lfs_file_read_(lfs_fs_t *fs, lfs_file_t *file,
        void *buffer, lfs_size_t size)
{
    LFS_ASSERT((file->flags & 0x3) != LFS_O_WRONLY);

    if (file->flags & LFS_F_WRITING) {
        // flush out any writes
        int err = lfs_file_flush(fs, file);
        if (err) {
            return err;
        }
    }

    return lfs_file_flushedread(fs, file, buffer, size);
}

static lfs_ssize_t lfs_file_flushedwrite(lfs_fs_t *fs, lfs_file_t *file,
        const void *buffer, lfs_size_t size)
{
    const uint8_t *data = (const uint8_t *)buffer;
    lfs_size_t nsize = size;

    if ((file->flags & LFS_F_INLINE) &&
            lfs_max(file->pos+nsize, file->ctz.size) > fs->inline_max) {
        // inline file doesn't fit anymore
        int err = lfs_file_outline(fs, file);
        if (err) {
            file->flags |= LFS_F_ERRED;
            return err;
        }
    }

    while (nsize > 0) {
        // check if we need a new block
        if (!(file->flags & LFS_F_WRITING) ||
                file->off == fs->cfg->block_size) {
            if (!(file->flags & LFS_F_INLINE)) {
                if (!(file->flags & LFS_F_WRITING) && file->pos > 0) {
                    // find out which block we're extending from
                    int err = lfs_ctz_find(fs, NULL, &file->cache,
                            file->ctz.head, file->ctz.size,
                            file->pos-1, &file->block, &(lfs_off_t){0});
                    if (err) {
                        file->flags |= LFS_F_ERRED;
                        return err;
                    }

                    // mark cache as dirty since we may have read data into it
                    lfs_cache_zero(fs, &file->cache);
                }

                // extend file with new blocks
                lfs_alloc_ckpoint(fs);
                int err = lfs_ctz_extend(fs, &file->cache, &fs->rcache,
                        file->block, file->pos,
                        &file->block, &file->off);
                if (err) {
                    file->flags |= LFS_F_ERRED;
                    return err;
                }
            } else {
                file->block = LFS_BLOCK_INLINE;
                file->off = file->pos;
            }

            file->flags |= LFS_F_WRITING;
        }

        // program as much as we can in current block
        lfs_size_t diff = lfs_min(nsize, fs->cfg->block_size - file->off);
        while (true) {
            int err = lfs_bd_prog(fs, &file->cache, &fs->rcache, true,
                    file->block, file->off, data, diff);
            if (err) {
                if (err == LFS_ERR_CORRUPT) {
                    goto relocate;
                }
                file->flags |= LFS_F_ERRED;
                return err;
            }

            break;
relocate:
            err = lfs_file_relocate(fs, file);
            if (err) {
                file->flags |= LFS_F_ERRED;
                return err;
            }
        }

        file->pos += diff;
        file->off += diff;
        data += diff;
        nsize -= diff;

        lfs_alloc_ckpoint(fs);
    }

    return size;
}

static lfs_ssize_t lfs_file_write_(lfs_fs_t *fs, lfs_file_t *file,
        const void *buffer, lfs_size_t size)
{
    LFS_ASSERT((file->flags & 0x3) != LFS_O_RDONLY);

    if (file->flags & LFS_F_READING) {
        // drop any reads
        int err = lfs_file_flush(fs, file);
        if (err) {
            return err;
        }
    }

    if ((file->flags & LFS_O_APPEND) && file->pos < file->ctz.size) {
        file->pos = file->ctz.size;
    }

    if (file->pos + size > fs->file_max) {
        // Larger than file limit?
        return LFS_ERR_FBIG;
    }

    if (!(file->flags & LFS_F_WRITING) && file->pos > file->ctz.size) {
        // fill with zeros
        lfs_off_t pos = file->pos;
        file->pos = file->ctz.size;

        while (file->pos < pos) {
            lfs_ssize_t res = lfs_file_flushedwrite(fs, file,
                    &(uint8_t){0}, 1);
            if (res < 0) {
                return res;
            }
        }
    }

    lfs_ssize_t nsize = lfs_file_flushedwrite(fs, file, buffer, size);
    if (nsize < 0) {
        return nsize;
    }

    file->flags &= ~LFS_F_ERRED & ~LFS_F_DUSTY;
    return nsize;
}

static lfs_soff_t lfs_file_seek_(lfs_fs_t *fs, lfs_file_t *file,
        lfs_soff_t off, int whence)
{
    // find new pos
    //
    // fortunately for us, littlefs is limited to 31-bit file sizes, so we
    // don't have to worry too much about integer overflow
    lfs_off_t npos = file->pos;
    if (whence == LFS_SEEK_SET) {
        npos = off;
    } else if (whence == LFS_SEEK_CUR) {
        npos = file->pos + (lfs_off_t)off;
    } else if (whence == LFS_SEEK_END) {
        npos = (lfs_off_t)lfs_file_size_(fs, file) + (lfs_off_t)off;
    }

    if (npos > fs->file_max) {
        // file position out of range
        return LFS_ERR_INVAL;
    }

    if (file->pos == npos) {
        // noop - position has not changed
        return npos;
    }

    // if we're only reading and our new offset is still in the file's cache
    // we can avoid flushing and needing to reread the data
    if ((file->flags & LFS_F_READING)
            && file->off != fs->cfg->block_size) {
        int oindex = lfs_ctz_index(fs, &(lfs_off_t){file->pos});
        lfs_off_t noff = npos;
        int nindex = lfs_ctz_index(fs, &noff);
        if (oindex == nindex
                && noff >= file->cache.off
                && noff < file->cache.off + file->cache.size) {
            file->pos = npos;
            file->off = noff;
            return npos;
        }
    }

    // write out everything beforehand, may be noop if rdonly
    int err = lfs_file_flush(fs, file);
    if (err) {
        return err;
    }

    // update pos
    file->pos = npos;
    return npos;
}

static lfs_soff_t lfs_file_size_(lfs_fs_t *fs, lfs_file_t *file)
{
    (void)fs;

    if (file->flags & LFS_F_WRITING) {
        return lfs_max(file->pos, file->ctz.size);
    }

    return file->ctz.size;
}

/// General fs operations ///
static int lfs_stat_(lfs_fs_t *fs, const char *path, struct lfs_info *info)
{
    lfs_mdir_t cwd;
    lfs_stag_t tag = lfs_dir_find(fs, &cwd, &path, NULL);
    if (tag < 0) {
        return (int)tag;
    }

    // only allow trailing slashes on dirs
    if (strchr(path, '/') != NULL
            && lfs_tag_type3(tag) != LFS_TYPE_DIR) {
        return LFS_ERR_NOTDIR;
    }

    return lfs_dir_getinfo(fs, &cwd, lfs_tag_id(tag), info);
}

static int lfs_remove_(lfs_fs_t *fs, const char *path)
{
    // deorphan if we haven't yet, needed at most once after poweron
    int err = lfs_fs_forceconsistency(fs);
    if (err) {
        return err;
    }

    lfs_mdir_t cwd;
    lfs_stag_t tag = lfs_dir_find(fs, &cwd, &path, NULL);
    if (tag < 0 || lfs_tag_id(tag) == 0x3ff) {
        return (tag < 0) ? (int)tag : LFS_ERR_INVAL;
    }

    struct lfs_mlist dir;
    dir.next = fs->mlist;
    if (lfs_tag_type3(tag) == LFS_TYPE_DIR) {
        // must be empty before removal
        lfs_block_t pair[2];
        lfs_stag_t res = lfs_dir_get(fs, &cwd, LFS_MKTAG(0x700, 0x3ff, 0),
                LFS_MKTAG(LFS_TYPE_STRUCT, lfs_tag_id(tag), 8), pair);
        if (res < 0) {
            return (int)res;
        }
        lfs_pair_fromle32(pair);

        err = lfs_dir_fetch(fs, &dir.m, pair);
        if (err) {
            return err;
        }

        if (dir.m.count > 0 || dir.m.split) {
            return LFS_ERR_NOTEMPTY;
        }

        // mark fs as orphaned
        err = lfs_fs_preporphans(fs, +1);
        if (err) {
            return err;
        }

        // I know it's crazy but yes, dir can be changed by our parent's
        // commit (if predecessor is child)
        dir.type = 0;
        dir.id = 0;
        fs->mlist = &dir;
    }

    // delete the entry
    err = lfs_dir_commit(fs, &cwd, LFS_MKATTRS(
            {LFS_MKTAG(LFS_TYPE_DELETE, lfs_tag_id(tag), 0), NULL}));
    if (err) {
        fs->mlist = dir.next;
        return err;
    }

    fs->mlist = dir.next;
    if (lfs_gstate_hasorphans(&fs->gstate)) {
        LFS_ASSERT(lfs_tag_type3(tag) == LFS_TYPE_DIR);

        // fix orphan
        err = lfs_fs_preporphans(fs, -1);
        if (err) {
            return err;
        }

        err = lfs_fs_pred(fs, dir.m.pair, &cwd);
        if (err) {
            return err;
        }

        err = lfs_dir_drop(fs, &cwd, &dir.m);
        if (err) {
            return err;
        }
    }

    return 0;
}

static int lfs_rename_(lfs_fs_t *fs, const char *oldpath,
        const char *newpath)
{
    // deorphan if we haven't yet, needed at most once after poweron
    int err = lfs_fs_forceconsistency(fs);
    if (err) {
        return err;
    }

    // find old entry
    lfs_mdir_t oldcwd;
    lfs_stag_t oldtag = lfs_dir_find(fs, &oldcwd, &oldpath, NULL);
    if (oldtag < 0 || lfs_tag_id(oldtag) == 0x3ff) {
        return (oldtag < 0) ? (int)oldtag : LFS_ERR_INVAL;
    }

    // find new entry
    lfs_mdir_t newcwd;
    uint16_t newid;
    lfs_stag_t prevtag = lfs_dir_find(fs, &newcwd, &newpath, &newid);
    if ((prevtag < 0 || lfs_tag_id(prevtag) == 0x3ff) &&
            !(prevtag == LFS_ERR_NOENT && lfs_path_islast(newpath))) {
        return (prevtag < 0) ? (int)prevtag : LFS_ERR_INVAL;
    }

    // if we're in the same pair there's a few special cases...
    bool samepair = (lfs_pair_cmp(oldcwd.pair, newcwd.pair) == 0);
    uint16_t newoldid = lfs_tag_id(oldtag);

    struct lfs_mlist prevdir;
    prevdir.next = fs->mlist;
    if (prevtag == LFS_ERR_NOENT) {
        // if we're a file, don't allow trailing slashes
        if (lfs_path_isdir(newpath)
                && lfs_tag_type3(oldtag) != LFS_TYPE_DIR) {
            return LFS_ERR_NOTDIR;
        }

        // check that name fits
        lfs_size_t nlen = lfs_path_namelen(newpath);
        if (nlen > fs->name_max) {
            return LFS_ERR_NAMETOOLONG;
        }

        // there is a small chance we are being renamed in the same
        // directory/ to an id less than our old id, the global update
        // to handle this is a bit messy
        if (samepair && newid <= newoldid) {
            newoldid += 1;
        }
    } else if (lfs_tag_type3(prevtag) != lfs_tag_type3(oldtag)) {
        return (lfs_tag_type3(prevtag) == LFS_TYPE_DIR)
                ? LFS_ERR_ISDIR
                : LFS_ERR_NOTDIR;
    } else if (samepair && newid == newoldid) {
        // we're renaming to ourselves??
        return 0;
    } else if (lfs_tag_type3(prevtag) == LFS_TYPE_DIR) {
        // must be empty before removal
        lfs_block_t prevpair[2];
        lfs_stag_t res = lfs_dir_get(fs, &newcwd, LFS_MKTAG(0x700, 0x3ff, 0),
                LFS_MKTAG(LFS_TYPE_STRUCT, newid, 8), prevpair);
        if (res < 0) {
            return (int)res;
        }
        lfs_pair_fromle32(prevpair);

        err = lfs_dir_fetch(fs, &prevdir.m, prevpair);
        if (err) {
            return err;
        }

        if (prevdir.m.count > 0 || prevdir.m.split) {
            return LFS_ERR_NOTEMPTY;
        }

        // mark fs as orphaned
        err = lfs_fs_preporphans(fs, +1);
        if (err) {
            return err;
        }

        // I know it's crazy but yes, dir can be changed by our parent's
        // commit (if predecessor is child)
        prevdir.type = 0;
        prevdir.id = 0;
        fs->mlist = &prevdir;
    }

    if (!samepair) {
        lfs_fs_prepmove(fs, newoldid, oldcwd.pair);
    }

    // move over all attributes
    err = lfs_dir_commit(fs, &newcwd, LFS_MKATTRS(
            {LFS_MKTAG_IF(prevtag != LFS_ERR_NOENT,
                LFS_TYPE_DELETE, newid, 0), NULL},
            {LFS_MKTAG(LFS_TYPE_CREATE, newid, 0), NULL},
            {LFS_MKTAG(lfs_tag_type3(oldtag),
                newid, lfs_path_namelen(newpath)), newpath},
            {LFS_MKTAG(LFS_FROM_MOVE, newid, lfs_tag_id(oldtag)), &oldcwd},
            {LFS_MKTAG_IF(samepair,
                LFS_TYPE_DELETE, newoldid, 0), NULL}));
    if (err) {
        fs->mlist = prevdir.next;
        return err;
    }

    // let commit clean up after move (if we're different! otherwise move
    // logic already fixed it for us)
    if (!samepair && lfs_gstate_hasmove(&fs->gstate)) {
        // prep gstate and delete move id
        lfs_fs_prepmove(fs, 0x3ff, NULL);
        err = lfs_dir_commit(fs, &oldcwd, LFS_MKATTRS(
                {LFS_MKTAG(LFS_TYPE_DELETE, lfs_tag_id(oldtag), 0),
                    NULL}));
        if (err) {
            fs->mlist = prevdir.next;
            return err;
        }
    }

    fs->mlist = prevdir.next;
    if (lfs_gstate_hasorphans(&fs->gstate)) {
        LFS_ASSERT(prevtag != LFS_ERR_NOENT
                && lfs_tag_type3(prevtag) == LFS_TYPE_DIR);

        // fix orphan
        err = lfs_fs_preporphans(fs, -1);
        if (err) {
            return err;
        }

        err = lfs_fs_pred(fs, prevdir.m.pair, &newcwd);
        if (err) {
            return err;
        }

        err = lfs_dir_drop(fs, &newcwd, &prevdir.m);
        if (err) {
            return err;
        }
    }

    return 0;
}

static int lfs_fs_pred(lfs_fs_t *fs,
        const lfs_block_t pair[2], lfs_mdir_t *pdir)
{
    // iterate over all directory directory entries
    pdir->tail[0] = 0;
    pdir->tail[1] = 1;
    struct lfs_tortoise_t tortoise = {
        .pair = {LFS_BLOCK_NULL, LFS_BLOCK_NULL},
        .i = 1,
        .period = 1,
    };
    int err = LFS_ERR_OK;
    while (!lfs_pair_isnull(pdir->tail)) {
        err = lfs_tortoise_detectcycles(pdir, &tortoise);
        if (err < 0) {
            return LFS_ERR_CORRUPT;
        }

        if (lfs_pair_cmp(pdir->tail, pair) == 0) {
            return 0;
        }

        err = lfs_dir_fetch(fs, pdir, pdir->tail);
        if (err) {
            return err;
        }
    }

    return LFS_ERR_NOENT;
}

struct lfs_fs_parent_match {
    lfs_fs_t *fs;
    const lfs_block_t pair[2];
};

static int lfs_fs_parent_match(void *data,
        lfs_tag_t tag, const void *buffer)
{
    struct lfs_fs_parent_match *find = data;
    lfs_fs_t *fs = find->fs;
    const struct lfs_diskoff *disk = buffer;
    (void)tag;

    lfs_block_t child[2];
    int err = lfs_bd_read(fs,
            &fs->pcache, &fs->rcache, fs->cfg->block_size,
            disk->block, disk->off, &child, sizeof(child));
    if (err) {
        return err;
    }

    lfs_pair_fromle32(child);
    return (lfs_pair_cmp(child, find->pair) == 0) ? LFS_CMP_EQ : LFS_CMP_LT;
}

static lfs_stag_t lfs_fs_parent(lfs_fs_t *fs, const lfs_block_t pair[2],
        lfs_mdir_t *parent)
{
    // use fetchmatch with callback to find pairs
    parent->tail[0] = 0;
    parent->tail[1] = 1;
    struct lfs_tortoise_t tortoise = {
        .pair = {LFS_BLOCK_NULL, LFS_BLOCK_NULL},
        .i = 1,
        .period = 1,
    };
    int err = LFS_ERR_OK;
    while (!lfs_pair_isnull(parent->tail)) {
        err = lfs_tortoise_detectcycles(parent, &tortoise);
        if (err < 0) {
            return err;
        }

        lfs_stag_t tag = lfs_dir_fetchmatch(fs, parent, parent->tail,
                LFS_MKTAG(0x7ff, 0, 0x3ff),
                LFS_MKTAG(LFS_TYPE_DIRSTRUCT, 0, 8),
                NULL,
                lfs_fs_parent_match, &(struct lfs_fs_parent_match){
                    fs, {pair[0], pair[1]}});
        if (tag && tag != LFS_ERR_NOENT) {
            return tag;
        }
    }

    return LFS_ERR_NOENT;
}

static void lfs_fs_prepsuperblock(lfs_fs_t *fs, bool needssuperblock)
{
    fs->gstate.tag = (fs->gstate.tag & ~LFS_MKTAG(0, 0, 0x200))
            | (uint32_t)needssuperblock << 9;
}

static int lfs_fs_preporphans(lfs_fs_t *fs, int8_t orphans)
{
    LFS_ASSERT(lfs_tag_size(fs->gstate.tag) > 0x000 || orphans >= 0);
    LFS_ASSERT(lfs_tag_size(fs->gstate.tag) < 0x1ff || orphans <= 0);
    fs->gstate.tag += orphans;
    fs->gstate.tag = ((fs->gstate.tag & ~LFS_MKTAG(0x800, 0, 0)) |
            ((uint32_t)lfs_gstate_hasorphans(&fs->gstate) << 31));

    return 0;
}

static void lfs_fs_prepmove(lfs_fs_t *fs,
        uint16_t id, const lfs_block_t pair[2])
{
    fs->gstate.tag = ((fs->gstate.tag & ~LFS_MKTAG(0x7ff, 0x3ff, 0)) |
            ((id != 0x3ff) ? LFS_MKTAG(LFS_TYPE_DELETE, id, 0) : 0));
    fs->gstate.pair[0] = (id != 0x3ff) ? pair[0] : 0;
    fs->gstate.pair[1] = (id != 0x3ff) ? pair[1] : 0;
}

static int lfs_fs_desuperblock(lfs_fs_t *fs)
{
    if (!lfs_gstate_needssuperblock(&fs->gstate)) {
        return 0;
    }

    SGL_LOG_TRACE(LFS_FS_NAME " rewriting superblock {%x, %x}",
                  (unsigned)fs->root[0], (unsigned)fs->root[1]);

    lfs_mdir_t root;
    int err = lfs_dir_fetch(fs, &root, fs->root);
    if (err) {
        return err;
    }

    // write a new superblock
    lfs_superblock_t superblock = {
        .version     = lfs_fs_disk_version(fs),
        .block_size  = fs->cfg->block_size,
        .block_count = fs->block_count,
        .name_max    = fs->name_max,
        .file_max    = fs->file_max,
        .attr_max    = fs->attr_max,
    };

    lfs_superblock_tole32(&superblock);
    err = lfs_dir_commit(fs, &root, LFS_MKATTRS(
            {LFS_MKTAG(LFS_TYPE_INLINESTRUCT, 0, sizeof(superblock)),
                &superblock}));
    if (err) {
        return err;
    }

    lfs_fs_prepsuperblock(fs, false);
    return 0;
}

static int lfs_fs_demove(lfs_fs_t *fs)
{
    if (!lfs_gstate_hasmove(&fs->gdisk)) {
        return 0;
    }

    // Fix bad moves
    SGL_LOG_TRACE(LFS_FS_NAME " fixing move {%x, %x} id=%x",
                  (unsigned)fs->gdisk.pair[0], (unsigned)fs->gdisk.pair[1],
                  (unsigned)lfs_tag_id(fs->gdisk.tag));

    // no other gstate is supported at this time, so if we found something
    // else something most likely went wrong in gstate calculation
    LFS_ASSERT(lfs_tag_type3(fs->gdisk.tag) == LFS_TYPE_DELETE);

    // fetch and delete the moved entry
    lfs_mdir_t movedir;
    int err = lfs_dir_fetch(fs, &movedir, fs->gdisk.pair);
    if (err) {
        return err;
    }

    // prep gstate and delete move id
    uint16_t moveid = lfs_tag_id(fs->gdisk.tag);
    lfs_fs_prepmove(fs, 0x3ff, NULL);
    err = lfs_dir_commit(fs, &movedir, LFS_MKATTRS(
            {LFS_MKTAG(LFS_TYPE_DELETE, moveid, 0), NULL}));
    if (err) {
        return err;
    }

    return 0;
}

static int lfs_fs_deorphan(lfs_fs_t *fs, bool powerloss)
{
    if (!lfs_gstate_hasorphans(&fs->gstate)) {
        return 0;
    }

    int pass = 0;
    while (pass < 2) {
        // Fix any orphans
        lfs_mdir_t pdir = {.split = true, .tail = {0, 1}};
        lfs_mdir_t dir;
        bool moreorphans = false;

        // iterate over all directory directory entries
        while (!lfs_pair_isnull(pdir.tail)) {
            int err = lfs_dir_fetch(fs, &dir, pdir.tail);
            if (err) {
                return err;
            }

            // check head blocks for orphans
            if (!pdir.split) {
                // check if we have a parent
                lfs_mdir_t parent;
                lfs_stag_t tag = lfs_fs_parent(fs, pdir.tail, &parent);
                if (tag < 0 && tag != LFS_ERR_NOENT) {
                    return tag;
                }

                if (pass == 0 && tag != LFS_ERR_NOENT) {
                    lfs_block_t pair[2];
                    lfs_stag_t state = lfs_dir_get(fs, &parent,
                            LFS_MKTAG(0x7ff, 0x3ff, 0), tag, pair);
                    if (state < 0) {
                        return state;
                    }
                    lfs_pair_fromle32(pair);

                    if (!lfs_pair_issync(pair, pdir.tail)) {
                        // we have desynced
                        SGL_LOG_TRACE(LFS_FS_NAME " fixing half-orphan "
                                      "{%x, %x} -> {%x, %x}",
                                      (unsigned)pdir.tail[0],
                                      (unsigned)pdir.tail[1],
                                      (unsigned)pair[0], (unsigned)pair[1]);

                        // fix pending move in this pair? this looks like an
                        // optimization but is in fact _required_ since
                        // relocating may outdate the move.
                        uint16_t moveid = 0x3ff;
                        if (lfs_gstate_hasmovehere(&fs->gstate, pdir.pair)) {
                            moveid = lfs_tag_id(fs->gstate.tag);
                            SGL_LOG_TRACE(LFS_FS_NAME " fixing move while "
                                          "fixing orphans {%x, %x} id=%x",
                                          (unsigned)pdir.pair[0],
                                          (unsigned)pdir.pair[1],
                                          (unsigned)moveid);
                            lfs_fs_prepmove(fs, 0x3ff, NULL);
                        }

                        lfs_pair_tole32(pair);
                        state = lfs_dir_orphaningcommit(fs, &pdir,
                                LFS_MKATTRS(
                                {LFS_MKTAG_IF(moveid != 0x3ff,
                                    LFS_TYPE_DELETE, moveid, 0), NULL},
                                {LFS_MKTAG(LFS_TYPE_SOFTTAIL, 0x3ff, 8),
                                    pair}));
                        lfs_pair_fromle32(pair);
                        if (state < 0) {
                            return state;
                        }

                        // did our commit create more orphans?
                        if (state == LFS_OK_ORPHANED) {
                            moreorphans = true;
                        }

                        // refetch tail
                        continue;
                    }
                }

                // note we only check for full orphans if we may have had a
                // power-loss, otherwise orphans are created intentionally
                // during operations such as lfs_mkdir_
                if (pass == 1 && tag == LFS_ERR_NOENT && powerloss) {
                    // we are an orphan
                    SGL_LOG_TRACE(LFS_FS_NAME " fixing orphan {%x, %x}",
                                  (unsigned)pdir.tail[0],
                                  (unsigned)pdir.tail[1]);

                    // steal state
                    err = lfs_dir_getgstate(fs, &dir, &fs->gdelta);
                    if (err) {
                        return err;
                    }

                    // steal tail
                    lfs_pair_tole32(dir.tail);
                    int state = lfs_dir_orphaningcommit(fs, &pdir,
                            LFS_MKATTRS(
                            {LFS_MKTAG(LFS_TYPE_TAIL + dir.split, 0x3ff, 8),
                                dir.tail}));
                    lfs_pair_fromle32(dir.tail);
                    if (state < 0) {
                        return state;
                    }

                    // did our commit create more orphans?
                    if (state == LFS_OK_ORPHANED) {
                        moreorphans = true;
                    }

                    // refetch tail
                    continue;
                }
            }

            pdir = dir;
        }

        pass = moreorphans ? 0 : pass+1;
    }

    // mark orphans as fixed
    return lfs_fs_preporphans(fs, (int8_t)-lfs_gstate_getorphans(&fs->gstate));
}

static int lfs_fs_forceconsistency(lfs_fs_t *fs)
{
    int err = lfs_fs_desuperblock(fs);
    if (err) {
        return err;
    }

    err = lfs_fs_demove(fs);
    if (err) {
        return err;
    }

    err = lfs_fs_deorphan(fs, true);
    if (err) {
        return err;
    }

    return 0;
}

static int lfs_fs_mkconsistent_(lfs_fs_t *fs)
{
    // lfs_fs_forceconsistency does most of the work here
    int err = lfs_fs_forceconsistency(fs);
    if (err) {
        return err;
    }

    // do we have any pending gstate?
    lfs_gstate_t delta = {0};
    lfs_gstate_xor(&delta, &fs->gdisk);
    lfs_gstate_xor(&delta, &fs->gstate);
    if (!lfs_gstate_iszero(&delta)) {
        // lfs_dir_commit will implicitly write out any pending gstate
        lfs_mdir_t root;
        err = lfs_dir_fetch(fs, &root, fs->root);
        if (err) {
            return err;
        }

        err = lfs_dir_commit(fs, &root, NULL, 0);
        if (err) {
            return err;
        }
    }

    return 0;
}

static int lfs_fs_size_count(void *p, lfs_block_t block)
{
    (void)block;
    lfs_size_t *size = p;
    *size += 1;
    return 0;
}

static lfs_ssize_t lfs_fs_size_(lfs_fs_t *fs)
{
    lfs_size_t size = 0;
    int err = lfs_fs_traverse_(fs, lfs_fs_size_count, &size, false);
    if (err) {
        return err;
    }

    return (lfs_ssize_t)size;
}

static int lfs_fs_traverse_(lfs_fs_t *fs,
        int (*cb)(void *data, lfs_block_t block), void *data,
        bool includeorphans)
{
    // iterate over metadata pairs
    lfs_mdir_t dir = {.tail = {0, 1}};

    struct lfs_tortoise_t tortoise = {
        .pair = {LFS_BLOCK_NULL, LFS_BLOCK_NULL},
        .i = 1,
        .period = 1,
    };
    int err = LFS_ERR_OK;
    while (!lfs_pair_isnull(dir.tail)) {
        err = lfs_tortoise_detectcycles(&dir, &tortoise);
        if (err < 0) {
            return LFS_ERR_CORRUPT;
        }

        for (int i = 0; i < 2; i++) {
            int err = cb(data, dir.tail[i]);
            if (err) {
                return err;
            }
        }

        // iterate through ids in directory
        int err = lfs_dir_fetch(fs, &dir, dir.tail);
        if (err) {
            return err;
        }

        for (uint16_t id = 0; id < dir.count; id++) {
            lfs_ctz_t ctz;
            lfs_stag_t tag = lfs_dir_get(fs, &dir,
                    LFS_MKTAG(0x700, 0x3ff, 0),
                    LFS_MKTAG(LFS_TYPE_STRUCT, id, sizeof(ctz)), &ctz);
            if (tag < 0) {
                if (tag == LFS_ERR_NOENT) {
                    continue;
                }
                return tag;
            }
            lfs_ctz_fromle32(&ctz);

            if (lfs_tag_type3(tag) == LFS_TYPE_CTZSTRUCT) {
                err = lfs_ctz_traverse(fs, NULL, &fs->rcache,
                        ctz.head, ctz.size, cb, data);
                if (err) {
                    return err;
                }
            } else if (includeorphans &&
                    lfs_tag_type3(tag) == LFS_TYPE_DIRSTRUCT) {
                for (int i = 0; i < 2; i++) {
                    err = cb(data, (&ctz.head)[i]);
                    if (err) {
                        return err;
                    }
                }
            }
        }
    }

    // iterate over any open files
    for (lfs_file_t *f = (lfs_file_t *)fs->mlist; f; f = f->next) {
        if (f->type != LFS_TYPE_REG) {
            continue;
        }

        if (((f->flags & LFS_F_DIRTY) || (f->flags & LFS_F_DUSTY))
                && !(f->flags & LFS_F_INLINE)) {
            int err = lfs_ctz_traverse(fs, &f->cache, &fs->rcache,
                    f->ctz.head, f->ctz.size, cb, data);
            if (err) {
                return err;
            }
        }

        if ((f->flags & LFS_F_WRITING) && !(f->flags & LFS_F_INLINE)) {
            int err = lfs_ctz_traverse(fs, &f->cache, &fs->rcache,
                    f->block, (lfs_size_t)f->pos, cb, data);
            if (err) {
                return err;
            }
        }
    }

    return 0;
}

static int lfs_init(lfs_fs_t *fs, const struct lfs_config *cfg)
{
    fs->cfg = cfg;
    fs->block_count = cfg->block_count;  // May be 0
    int err = 0;

    // check that bool is a truthy-preserving type
    //
    // note the most common reason for this failure is a before-c99 compiler,
    // which littlefs currently does not support
    LFS_ASSERT((bool)0x80000000);

    // check that the required io functions are provided
    LFS_ASSERT(fs->cfg->read != NULL);
    LFS_ASSERT(fs->cfg->prog != NULL);
    LFS_ASSERT(fs->cfg->erase != NULL);
    LFS_ASSERT(fs->cfg->sync != NULL);

    // validate that the config sizes were initiated properly before
    // performing any arithmetic logics with them
    LFS_ASSERT(fs->cfg->read_size != 0);
    LFS_ASSERT(fs->cfg->prog_size != 0);
    LFS_ASSERT(fs->cfg->cache_size != 0);

    // check that block size is a multiple of cache size is a multiple
    // of prog and read sizes
    LFS_ASSERT(fs->cfg->cache_size % fs->cfg->read_size == 0);
    LFS_ASSERT(fs->cfg->cache_size % fs->cfg->prog_size == 0);
    LFS_ASSERT(fs->cfg->block_size % fs->cfg->cache_size == 0);

    // check that the block size is large enough to fit all ctz pointers
    LFS_ASSERT(fs->cfg->block_size >= 128);
    LFS_ASSERT(4*lfs_npw2(0xffffffff / (fs->cfg->block_size-2*4))
            <= fs->cfg->block_size);

    LFS_ASSERT(fs->cfg->block_cycles != 0);

    // metadata can't be compacted below block_size/2, and metadata can't
    // exceed a block_size
    LFS_ASSERT(fs->cfg->compact_thresh == 0
            || fs->cfg->compact_thresh >= fs->cfg->block_size/2);
    LFS_ASSERT(fs->cfg->compact_thresh == (lfs_size_t)-1
            || fs->cfg->compact_thresh <= fs->cfg->block_size);

    LFS_ASSERT(!fs->cfg->metadata_max
            || fs->cfg->metadata_max % fs->cfg->read_size == 0);
    LFS_ASSERT(!fs->cfg->metadata_max
            || fs->cfg->metadata_max % fs->cfg->prog_size == 0);
    LFS_ASSERT(!fs->cfg->metadata_max
            || fs->cfg->block_size % fs->cfg->metadata_max == 0);

    // setup read cache
    if (fs->cfg->read_buffer) {
        fs->rcache.buffer = fs->cfg->read_buffer;
    } else {
        fs->rcache.buffer = sgl_malloc(fs->cfg->cache_size);
        if (!fs->rcache.buffer) {
            err = LFS_ERR_NOMEM;
            goto cleanup;
        }
    }

    // setup program cache
    if (fs->cfg->prog_buffer) {
        fs->pcache.buffer = fs->cfg->prog_buffer;
    } else {
        fs->pcache.buffer = sgl_malloc(fs->cfg->cache_size);
        if (!fs->pcache.buffer) {
            err = LFS_ERR_NOMEM;
            goto cleanup;
        }
    }

    // zero to avoid information leaks
    lfs_cache_zero(fs, &fs->rcache);
    lfs_cache_zero(fs, &fs->pcache);

    LFS_ASSERT(fs->cfg->lookahead_size > 0);
    if (fs->cfg->lookahead_buffer) {
        fs->lookahead.buffer = fs->cfg->lookahead_buffer;
    } else {
        fs->lookahead.buffer = sgl_malloc(fs->cfg->lookahead_size);
        if (!fs->lookahead.buffer) {
            err = LFS_ERR_NOMEM;
            goto cleanup;
        }
    }

    // check that the size limits are sane
    LFS_ASSERT(fs->cfg->name_max <= LFS_NAME_MAX);
    fs->name_max = fs->cfg->name_max;
    if (!fs->name_max) {
        fs->name_max = LFS_NAME_MAX;
    }

    LFS_ASSERT(fs->cfg->file_max <= LFS_FILE_MAX);
    fs->file_max = fs->cfg->file_max;
    if (!fs->file_max) {
        fs->file_max = LFS_FILE_MAX;
    }

    LFS_ASSERT(fs->cfg->attr_max <= LFS_ATTR_MAX);
    fs->attr_max = fs->cfg->attr_max;
    if (!fs->attr_max) {
        fs->attr_max = LFS_ATTR_MAX;
    }

    LFS_ASSERT(fs->cfg->metadata_max <= fs->cfg->block_size);

    LFS_ASSERT(fs->cfg->inline_max == (lfs_size_t)-1
            || fs->cfg->inline_max <= fs->cfg->cache_size);
    LFS_ASSERT(fs->cfg->inline_max == (lfs_size_t)-1
            || fs->cfg->inline_max <= fs->attr_max);
    LFS_ASSERT(fs->cfg->inline_max == (lfs_size_t)-1
            || fs->cfg->inline_max <= ((fs->cfg->metadata_max)
                ? fs->cfg->metadata_max
                : fs->cfg->block_size)/8);
    fs->inline_max = fs->cfg->inline_max;
    if (fs->inline_max == (lfs_size_t)-1) {
        fs->inline_max = 0;
    } else if (fs->inline_max == 0) {
        fs->inline_max = lfs_min(
                            fs->cfg->cache_size,
                            lfs_min(fs->attr_max, ((fs->cfg->metadata_max) ? fs->cfg->metadata_max : fs->cfg->block_size)/8)
                            );
    }

    // setup default state
    fs->root[0] = LFS_BLOCK_NULL;
    fs->root[1] = LFS_BLOCK_NULL;
    fs->mlist = NULL;
    fs->seed = 0;
    fs->gdisk = (lfs_gstate_t){0};
    fs->gstate = (lfs_gstate_t){0};
    fs->gdelta = (lfs_gstate_t){0};

    return 0;

cleanup:
    lfs_deinit(fs);
    return err;
}

static int lfs_deinit(lfs_fs_t *fs)
{
    // free allocated memory
    if (!fs->cfg->read_buffer) {
        sgl_free(fs->rcache.buffer);
    }

    if (!fs->cfg->prog_buffer) {
        sgl_free(fs->pcache.buffer);
    }

    if (!fs->cfg->lookahead_buffer) {
        sgl_free(fs->lookahead.buffer);
    }

    return 0;
}

static int lfs_format_(lfs_fs_t *fs, const struct lfs_config *cfg)
{
    int err = 0;
    {
        err = lfs_init(fs, cfg);
        if (err) {
            return err;
        }

        LFS_ASSERT(cfg->block_count != 0);

        // create free lookahead
        memset(fs->lookahead.buffer, 0, fs->cfg->lookahead_size);
        fs->lookahead.start = 0;
        fs->lookahead.size = lfs_min(8*fs->cfg->lookahead_size,
                fs->block_count);
        fs->lookahead.next = 0;
        lfs_alloc_ckpoint(fs);

        // create root dir
        lfs_mdir_t root;
        err = lfs_dir_alloc(fs, &root);
        if (err) {
            goto cleanup;
        }

        // write one superblock
        lfs_superblock_t superblock = {
            .version     = lfs_fs_disk_version(fs),
            .block_size  = fs->cfg->block_size,
            .block_count = fs->block_count,
            .name_max    = fs->name_max,
            .file_max    = fs->file_max,
            .attr_max    = fs->attr_max,
        };

        lfs_superblock_tole32(&superblock);
        err = lfs_dir_commit(fs, &root, LFS_MKATTRS(
                {LFS_MKTAG(LFS_TYPE_CREATE, 0, 0), NULL},
                {LFS_MKTAG(LFS_TYPE_SUPERBLOCK, 0, 8), "littlefs"},
                {LFS_MKTAG(LFS_TYPE_INLINESTRUCT, 0, sizeof(superblock)),
                    &superblock}));
        if (err) {
            goto cleanup;
        }

        // force compaction to prevent accidentally mounting any
        // older version of littlefs that may live on disk
        root.erased = false;
        err = lfs_dir_commit(fs, &root, NULL, 0);
        if (err) {
            goto cleanup;
        }

        // sanity check that fetch works
        err = lfs_dir_fetch(fs, &root, (const lfs_block_t[2]){0, 1});
        if (err) {
            goto cleanup;
        }
    }

cleanup:
    lfs_deinit(fs);
    return err;
}

static int lfs_mount_(lfs_fs_t *fs, const struct lfs_config *cfg)
{
    int err = lfs_init(fs, cfg);
    if (err) {
        return err;
    }

    // scan directory blocks for superblock and any global updates
    lfs_mdir_t dir = {.tail = {0, 1}};
    struct lfs_tortoise_t tortoise = {
        .pair = {LFS_BLOCK_NULL, LFS_BLOCK_NULL},
        .i = 1,
        .period = 1,
    };
    while (!lfs_pair_isnull(dir.tail)) {
        err = lfs_tortoise_detectcycles(&dir, &tortoise);
        if (err < 0) {
            goto cleanup;
        }

        // fetch next block in tail list
        lfs_stag_t tag = lfs_dir_fetchmatch(fs, &dir, dir.tail,
                LFS_MKTAG(0x7ff, 0x3ff, 0),
                LFS_MKTAG(LFS_TYPE_SUPERBLOCK, 0, 8),
                NULL,
                lfs_dir_find_match, &(struct lfs_dir_find_match){
                    fs, "littlefs", 8});
        if (tag < 0) {
            err = tag;
            goto cleanup;
        }

        // has superblock?
        if (tag && !lfs_tag_isdelete(tag)) {
            // update root
            fs->root[0] = dir.pair[0];
            fs->root[1] = dir.pair[1];

            // grab superblock
            lfs_superblock_t superblock;
            tag = lfs_dir_get(fs, &dir, LFS_MKTAG(0x7ff, 0x3ff, 0),
                    LFS_MKTAG(LFS_TYPE_INLINESTRUCT, 0, sizeof(superblock)),
                    &superblock);
            if (tag < 0) {
                err = tag;
                goto cleanup;
            }
            lfs_superblock_fromle32(&superblock);

            // check version
            uint16_t major_version = (0xffff & (uint16_t)(superblock.version >> 16));
            uint16_t minor_version = (0xffff & (uint16_t)(superblock.version >> 0));
            if (major_version != lfs_fs_disk_version_major(fs)
                    || minor_version > lfs_fs_disk_version_minor(fs)) {
                SGL_LOG_ERROR(LFS_FS_NAME " invalid version %u.%u != %u.%u",
                              (unsigned)major_version, (unsigned)minor_version,
                              (unsigned)lfs_fs_disk_version_major(fs),
                              (unsigned)lfs_fs_disk_version_minor(fs));
                err = LFS_ERR_INVAL;
                goto cleanup;
            }

            // found older minor version? set an in-device only bit in the
            // gstate so we know we need to rewrite the superblock before
            // the first write
            bool needssuperblock = false;
            if (minor_version < lfs_fs_disk_version_minor(fs)) {
                SGL_LOG_TRACE(LFS_FS_NAME " found older minor version "
                              "%u.%u < %u.%u",
                              (unsigned)major_version, (unsigned)minor_version,
                              (unsigned)lfs_fs_disk_version_major(fs),
                              (unsigned)lfs_fs_disk_version_minor(fs));
                needssuperblock = true;
            }
            // note this bit is reserved on disk, so fetching more gstate
            // will not interfere here
            lfs_fs_prepsuperblock(fs, needssuperblock);

            // check superblock configuration
            if (superblock.name_max) {
                if (superblock.name_max > fs->name_max) {
                    SGL_LOG_ERROR(LFS_FS_NAME " unsupported name_max %u > %u",
                                  (unsigned)superblock.name_max,
                                  (unsigned)fs->name_max);
                    err = LFS_ERR_INVAL;
                    goto cleanup;
                }

                fs->name_max = superblock.name_max;
            }

            if (superblock.file_max) {
                if (superblock.file_max > fs->file_max) {
                    SGL_LOG_ERROR(LFS_FS_NAME " unsupported file_max %u > %u",
                                  (unsigned)superblock.file_max,
                                  (unsigned)fs->file_max);
                    err = LFS_ERR_INVAL;
                    goto cleanup;
                }

                fs->file_max = superblock.file_max;
            }

            if (superblock.attr_max) {
                if (superblock.attr_max > fs->attr_max) {
                    SGL_LOG_ERROR(LFS_FS_NAME " unsupported attr_max %u > %u",
                                  (unsigned)superblock.attr_max,
                                  (unsigned)fs->attr_max);
                    err = LFS_ERR_INVAL;
                    goto cleanup;
                }

                fs->attr_max = superblock.attr_max;

                // we also need to update inline_max in case attr_max changed
                fs->inline_max = lfs_min(fs->inline_max, fs->attr_max);
            }

            // this is where we get the block_count from disk if
            // block_count=0
            if (fs->cfg->block_count
                    && superblock.block_count != fs->cfg->block_count) {
                SGL_LOG_ERROR(LFS_FS_NAME " invalid block count %u != %u",
                              (unsigned)superblock.block_count,
                              (unsigned)fs->cfg->block_count);
                err = LFS_ERR_INVAL;
                goto cleanup;
            }

            fs->block_count = superblock.block_count;

            if (superblock.block_size != fs->cfg->block_size) {
                SGL_LOG_ERROR(LFS_FS_NAME " invalid block size %u != %u",
                              (unsigned)superblock.block_size,
                              (unsigned)fs->cfg->block_size);
                err = LFS_ERR_INVAL;
                goto cleanup;
            }
        }

        // has gstate?
        err = lfs_dir_getgstate(fs, &dir, &fs->gstate);
        if (err) {
            goto cleanup;
        }
    }

    // update littlefs with gstate
    if (!lfs_gstate_iszero(&fs->gstate)) {
        SGL_LOG_TRACE(LFS_FS_NAME " found pending gstate %x %x %x",
                      (unsigned)fs->gstate.tag,
                      (unsigned)fs->gstate.pair[0],
                      (unsigned)fs->gstate.pair[1]);
    }
    fs->gstate.tag += !lfs_tag_isvalid(fs->gstate.tag);
    fs->gdisk = fs->gstate;

    fs->lookahead.start = fs->seed % fs->block_count;
    lfs_alloc_drop(fs);

    return 0;

cleanup:
    lfs_unmount_(fs);
    return err;
}

static int lfs_unmount_(lfs_fs_t *fs)
{
    return lfs_deinit(fs);
}

typedef struct {
    lfs_fs_t fs;
    struct lfs_config cfg;
    lfs_bdev_t bdev;
    lfs_file_t files[LFS_MAX_OPEN_FILES];
    uint8_t file_used[LFS_MAX_OPEN_FILES];
    lfs_dir_t dirs[LFS_MAX_OPEN_DIRS];
    uint8_t dir_used[LFS_MAX_OPEN_DIRS];
} lfs_ctx_t;

static int lfs_dev_query(sgl_block_dev_t *dev, uint32_t *sector_size,
        uint32_t *sector_count)
{
    *sector_size = 0;
    *sector_count = 0;
    if (dev->ioctl) {
        sgl_block_dev_ioctl(dev, SGL_BLK_GET_SECTOR_SIZE, sector_size);
        sgl_block_dev_ioctl(dev, SGL_BLK_GET_SECTOR_COUNT, sector_count);
    }
    if ((!*sector_size || !*sector_count) && dev->info) {
        if (!*sector_size) *sector_size = dev->info->sector_size;
        if (!*sector_count) *sector_count = dev->info->sector_count;
    }
    return (*sector_size && *sector_count) ? 0 : -1;
}

static void lfs_dev_config(lfs_ctx_t *ctx, sgl_block_dev_t *dev)
{
    struct lfs_config *cfg = &ctx->cfg;
    SGL_UNUSED(dev);
    memset(cfg, 0, sizeof(*cfg));
    cfg->context = &ctx->bdev;
    cfg->read = lfs_bdev_read;
    cfg->prog = lfs_bdev_prog;
    cfg->erase = lfs_bdev_erase;
    cfg->sync = lfs_bdev_sync;
    // the sgl block device moves whole sectors; run the engine at
    // byte granularity on top of it with a one-sector cache
    cfg->read_size = 1;
    cfg->prog_size = 1;
    cfg->block_size = ctx->bdev.sector_size;
    cfg->block_count = ctx->bdev.sector_count;
    cfg->block_cycles = -1;       // disable metadata wear-leveling shuffling
    cfg->cache_size = ctx->bdev.sector_size;
    cfg->lookahead_size = 128;    // 1024-block allocation window
    cfg->compact_thresh = 0;      // littlefs default
}

static int lfs_translate_flags(uint32_t flags)
{
    uint32_t acc = flags & 0x3;
    if (acc == 3) {
        return -1;
    }

    int out = (acc == SGL_O_WRONLY) ? LFS_O_WRONLY :
              (acc == SGL_O_RDWR)   ? LFS_O_RDWR   : LFS_O_RDONLY;
    if (flags & SGL_O_CREAT)  out |= LFS_O_CREAT;
    if (flags & SGL_O_TRUNC)  out |= LFS_O_TRUNC;
    if (flags & SGL_O_APPEND) out |= LFS_O_APPEND;
    return out;
}

static int littlefs_mount(void **rfs, sgl_block_dev_t *dev,
        const char *mount_point, void *fs_config)
{
    (void)mount_point;
    (void)fs_config;
    if (!rfs || !dev) {
        return LFS_ERR_INVAL;
    }

    if (dev->init && dev->init(dev) != 0) {
        SGL_LOG_ERROR(LFS_FS_NAME " mount: device init failed");
        return LFS_ERR_IO;
    }

    lfs_ctx_t *ctx = (lfs_ctx_t *)sgl_malloc(sizeof(*ctx));
    if (!ctx) {
        return LFS_ERR_NOMEM;
    }
    memset(ctx, 0, sizeof(*ctx));

    uint32_t sector_size, sector_count;
    if (lfs_dev_query(dev, &sector_size, &sector_count) != 0) {
        SGL_LOG_ERROR(LFS_FS_NAME " mount: device geometry unknown");
        sgl_free(ctx);
        return LFS_ERR_INVAL;
    }

    ctx->bdev.dev = dev;
    ctx->bdev.sector_size = sector_size;
    ctx->bdev.sectors_per_block = 1;
    ctx->bdev.sector_count = sector_count;
    ctx->bdev.sec_buf = (uint8_t *)sgl_malloc(sector_size);
    if (!ctx->bdev.sec_buf) {
        sgl_free(ctx);
        return LFS_ERR_NOMEM;
    }
    lfs_dev_config(ctx, dev);

    int err = lfs_mount_(&ctx->fs, &ctx->cfg);
    if (err) {
        SGL_LOG_INFO(LFS_FS_NAME " mount failed (%d), formatting", err);
        err = lfs_format_(&ctx->fs, &ctx->cfg);
        if (!err) {
            err = lfs_mount_(&ctx->fs, &ctx->cfg);
        }
    }
    if (err) {
        sgl_free(ctx->bdev.sec_buf);
        sgl_free(ctx);
        return err;
    }

    *rfs = ctx;
    return 0;
}

static int littlefs_unmount(void *fs, const char *mount_point)
{
    (void)mount_point;
    lfs_ctx_t *ctx = (lfs_ctx_t *)fs;
    if (!ctx) {
        return LFS_ERR_INVAL;
    }

    for (int i = 0; i < LFS_MAX_OPEN_DIRS; i++) {
        if (ctx->dir_used[i]) {
            lfs_dir_close_(&ctx->fs, &ctx->dirs[i]);
            ctx->dir_used[i] = 0;
        }
    }
    int err = lfs_fs_mkconsistent_(&ctx->fs);

    lfs_unmount_(&ctx->fs);
    sgl_free(ctx->bdev.sec_buf);
    sgl_free(ctx);
    return err;
}

static int littlefs_open(void *fs, const char *path, uint32_t flags)
{
    lfs_ctx_t *ctx = (lfs_ctx_t *)fs;
    if (!ctx || !path) {
        return LFS_ERR_BADF;
    }

    int sflags = lfs_translate_flags(flags);
    if (sflags < 0) {
        return LFS_ERR_INVAL;
    }

    for (int i = 0; i < LFS_MAX_OPEN_FILES; i++) {
        if (!ctx->file_used[i]) {
            int err = lfs_file_open_(&ctx->fs, &ctx->files[i], path, sflags);
            if (err) {
                return err;
            }
            if ((sflags & LFS_O_APPEND) &&
                    ctx->files[i].pos < (lfs_off_t)ctx->files[i].ctz.size) {
                ctx->files[i].pos = (lfs_off_t)ctx->files[i].ctz.size;
            }
            if (ctx->files[i].flags & LFS_F_DIRTY) {
                int serr = lfs_file_sync_(&ctx->fs, &ctx->files[i]);
                if (serr) {
                    ctx->files[i].flags |= LFS_F_ERRED;
                    lfs_file_close_(&ctx->fs, &ctx->files[i]);
                    return serr;
                }
            }
            ctx->file_used[i] = 1;
            return i;
        }
    }
    return LFS_ERR_NOMEM;
}

static int littlefs_close(void *fs, int fd)
{
    lfs_ctx_t *ctx = (lfs_ctx_t *)fs;
    if (!ctx || fd < 0 || fd >= LFS_MAX_OPEN_FILES ||
            !ctx->file_used[fd]) {
        return LFS_ERR_BADF;
    }

    int err = lfs_file_close_(&ctx->fs, &ctx->files[fd]);
    ctx->file_used[fd] = 0;
    return err;
}

static int littlefs_read(void *fs, int fd, void *buffer, uint32_t count)
{
    lfs_ctx_t *ctx = (lfs_ctx_t *)fs;
    if (!ctx || fd < 0 || fd >= LFS_MAX_OPEN_FILES ||
            !ctx->file_used[fd]) {
        return LFS_ERR_BADF;
    }

    lfs_file_t *file = &ctx->files[fd];
    if ((file->flags & 0x3) == LFS_O_WRONLY) {
        return LFS_ERR_BADF;
    }
    if (file->flags & LFS_F_ERRED) {
        return LFS_ERR_IO;
    }
    return (int)lfs_file_read_(&ctx->fs, file, buffer, count);
}

static int littlefs_write(void *fs, int fd, const void *buffer, uint32_t count)
{
    lfs_ctx_t *ctx = (lfs_ctx_t *)fs;
    if (!ctx || fd < 0 || fd >= LFS_MAX_OPEN_FILES ||
            !ctx->file_used[fd]) {
        return LFS_ERR_BADF;
    }

    lfs_file_t *file = &ctx->files[fd];
    if ((file->flags & 0x3) == LFS_O_RDONLY) {
        return LFS_ERR_BADF;
    }
    if (file->flags & LFS_F_ERRED) {
        return LFS_ERR_IO;
    }

    lfs_size_t committed = file->ctz.size;
    lfs_ssize_t n = lfs_file_write_(&ctx->fs, file, buffer, count);
    if (n > 0) {
        if (lfs_file_sync_(&ctx->fs, file) != 0) {
            file->flags |= LFS_F_ERRED;
            file->pos = (lfs_off_t)committed;
            file->ctz.size = committed;
            lfs_cache_drop(&ctx->fs, &file->cache);
            file->flags &= ~LFS_F_WRITING;
            file->block = LFS_BLOCK_NULL;
            file->off = 0;
            return LFS_ERR_IO;
        }
    }
    if (n < 0 && file->pos > (lfs_off_t)committed) {
        file->pos = (lfs_off_t)committed;
        file->ctz.size = committed;
        lfs_cache_drop(&ctx->fs, &file->cache);
        file->flags &= ~LFS_F_WRITING;
        file->block = LFS_BLOCK_NULL;
        file->off = 0;
    }
    return (int)n;
}

static int littlefs_seek(void *fs, int fd, int32_t offset, uint8_t whence)
{
    lfs_ctx_t *ctx = (lfs_ctx_t *)fs;
    if (!ctx || fd < 0 || fd >= LFS_MAX_OPEN_FILES ||
            !ctx->file_used[fd]) {
        return LFS_ERR_BADF;
    }

    if (whence > LFS_SEEK_END) {
        return LFS_ERR_INVAL;
    }
    if (ctx->files[fd].flags & LFS_F_ERRED) {
        return LFS_ERR_IO;
    }
    return (int)lfs_file_seek_(&ctx->fs, &ctx->files[fd], offset, whence);
}

static int littlefs_opendir(void *fs, const char *path, int *dd)
{
    lfs_ctx_t *ctx = (lfs_ctx_t *)fs;
    if (!ctx || !path || !dd) {
        return LFS_ERR_BADF;
    }

    for (int i = 0; i < LFS_MAX_OPEN_DIRS; i++) {
        if (!ctx->dir_used[i]) {
            int err = lfs_dir_open_(&ctx->fs, &ctx->dirs[i], path);
            if (err) {
                return err;
            }
            ctx->dir_used[i] = 1;
            *dd = i;
            return 0;
        }
    }
    return LFS_ERR_NOMEM;
}

static int littlefs_readdir(void *fs, int dd, char *name,
        uint32_t name_size, uint32_t *type)
{
    lfs_ctx_t *ctx = (lfs_ctx_t *)fs;
    if (!ctx || dd < 0 || dd >= LFS_MAX_OPEN_DIRS ||
            !ctx->dir_used[dd]) {
        return LFS_ERR_BADF;
    }

    struct lfs_info info;
    int res;
    do {
        res = lfs_dir_read_(&ctx->fs, &ctx->dirs[dd], &info);
    } while (res > 0 &&
            (!strcmp(info.name, ".") || !strcmp(info.name, "..")));
    if (res <= 0) {
        return res;
    }

    if (type) {
        *type = (info.type == LFS_TYPE_DIR) ? SGL_S_IFDIR : SGL_S_IFREG;
    }
    if (name && name_size > 0) {
        size_t len = strlen(info.name);
        if (len > name_size - 1) {
            len = name_size - 1;
        }
        memcpy(name, info.name, len);
        name[len] = '\0';
    }
    return 1;
}

static int littlefs_closedir(void *fs, int dd)
{
    lfs_ctx_t *ctx = (lfs_ctx_t *)fs;
    if (!ctx || dd < 0 || dd >= LFS_MAX_OPEN_DIRS ||
            !ctx->dir_used[dd]) {
        return LFS_ERR_BADF;
    }

    int err = lfs_dir_close_(&ctx->fs, &ctx->dirs[dd]);
    ctx->dir_used[dd] = 0;
    return err;
}

static int littlefs_sync(void *fs)
{
    lfs_ctx_t *ctx = (lfs_ctx_t *)fs;
    if (!ctx) {
        return LFS_ERR_BADF;
    }

    int err = 0;
    for (int i = 0; i < LFS_MAX_OPEN_FILES; i++) {
        if (ctx->file_used[i] && !(ctx->files[i].flags & LFS_F_ERRED)) {
            int e = lfs_file_sync_(&ctx->fs, &ctx->files[i]);
            if (e && !err) {
                err = e;
            }
        }
    }
    return err;
}

static int littlefs_format(void *fs)
{
    lfs_ctx_t *ctx = (lfs_ctx_t *)fs;
    if (!ctx) {
        return LFS_ERR_BADF;
    }

    for (int i = 0; i < LFS_MAX_OPEN_FILES; i++) {
        if (ctx->file_used[i]) {
            return LFS_ERR_INVAL;
        }
    }
    for (int i = 0; i < LFS_MAX_OPEN_DIRS; i++) {
        if (ctx->dir_used[i]) {
            return LFS_ERR_INVAL;
        }
    }

    // tear the engine down, reformat, then bring the instance back up so
    // the mount point stays usable
    lfs_unmount_(&ctx->fs);
    int err = lfs_format_(&ctx->fs, &ctx->cfg);
    if (!err) {
        err = lfs_mount_(&ctx->fs, &ctx->cfg);
    }
    return err;
}

static int littlefs_remove(void *fs, const char *path)
{
    lfs_ctx_t *ctx = (lfs_ctx_t *)fs;
    if (!ctx || !path) {
        return LFS_ERR_BADF;
    }
    return lfs_remove_(&ctx->fs, path);
}

static int littlefs_mkdir(void *fs, const char *path)
{
    lfs_ctx_t *ctx = (lfs_ctx_t *)fs;
    if (!ctx || !path) {
        return LFS_ERR_BADF;
    }
    return lfs_mkdir_(&ctx->fs, path);
}

static int littlefs_stat(void *fs, const char *path, sgl_stat_t *st)
{
    lfs_ctx_t *ctx = (lfs_ctx_t *)fs;
    if (!ctx || !path || !st) {
        return LFS_ERR_INVAL;
    }

    struct lfs_info info;
    int err = lfs_stat_(&ctx->fs, path, &info);
    if (err) {
        return err;
    }

    memset(st, 0, sizeof(*st));
    st->st_mode = (info.type == LFS_TYPE_DIR)
            ? (SGL_S_IFDIR | 0755) : (SGL_S_IFREG | 0644);
    st->st_size = (uint32_t)info.size;
    return 0;
}

static int littlefs_rename(void *fs, const char *old_path, const char *new_path)
{
    lfs_ctx_t *ctx = (lfs_ctx_t *)fs;
    if (!ctx || !old_path || !new_path) {
        return LFS_ERR_INVAL;
    }
    return lfs_rename_(&ctx->fs, old_path, new_path);
}

struct lfs_count_data {
    lfs_size_t used;
};

static int lfs_count_cb(void *p, lfs_block_t block)
{
    (void)block;
    ((struct lfs_count_data *)p)->used += 1;
    return 0;
}

static int littlefs_statvfs(void *fs, sgl_statvfs_t *info)
{
    lfs_ctx_t *ctx = (lfs_ctx_t *)fs;
    if (!ctx || !info) {
        return LFS_ERR_INVAL;
    }

    struct lfs_count_data count = {0};
    int err = lfs_fs_traverse_(&ctx->fs, lfs_count_cb, &count, true);
    if (err) {
        return err;
    }

    memset(info, 0, sizeof(*info));
    info->f_bsize = (uint32_t)ctx->cfg.block_size;
    info->f_blocks = (uint32_t)ctx->fs.block_count;
    info->f_bfree = (ctx->fs.block_count > count.used)
            ? (uint32_t)(ctx->fs.block_count - count.used) : 0;
    info->f_bavail = info->f_bfree;

    info->f_files = 0;
    info->f_ffree = 0;
    return 0;
}

static sgl_fs_ops_t g_littlefs_ops = {
    .mount    = littlefs_mount,
    .unmount  = littlefs_unmount,
    .open     = littlefs_open,
    .close    = littlefs_close,
    .read     = littlefs_read,
    .write    = littlefs_write,
    .seek     = littlefs_seek,
    .opendir  = littlefs_opendir,
    .readdir  = littlefs_readdir,
    .closedir = littlefs_closedir,
    .sync     = littlefs_sync,
    .format   = littlefs_format,
    .remove   = littlefs_remove,
    .mkdir    = littlefs_mkdir,
    .stat     = littlefs_stat,
    .rename   = littlefs_rename,
    .statvfs  = littlefs_statvfs,
};

static sgl_fs_type_t g_littlefs_type = {
    .name = LFS_FS_NAME,
    .ops  = &g_littlefs_ops,
};

int sgl_littlefs_register(void)
{
    return sgl_fs_register(&g_littlefs_type);
}
