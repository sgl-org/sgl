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
#include <sgl_mm.h>

#define LFS_FS_NAME         "littlefs"

#define SLFS_MAX_OPEN_FILES  8
#define SLFS_MAX_OPEN_DIRS   4
#define SLFS_MAX_NAME_LEN    255

#define SLFS_MAGIC           0x534C4653  /* "SLFS" */
#define SLFS_VERSION         1

/* Inode types */
#define SLFS_TYPE_FILE       1
#define SLFS_TYPE_DIR        2

/* On-disk block type tags */
#define SLFS_BLK_SUPER       0x01
#define SLFS_BLK_INODE       0x02
#define SLFS_BLK_DATA        0x03
#define SLFS_BLK_FREE        0xFF

enum {
    SLFS_OK = SGL_FS_OK,
    SLFS_ERR_IO = SGL_FS_IO_ERROR,
    SLFS_ERR_NOT_FOUND = SGL_FS_NOT_FOUND,
    SLFS_ERR_INVALID = SGL_FS_INVALID_ARGUMENT,
    SLFS_ERR_DENIED = SGL_FS_PERMISSION_DENIED,
    SLFS_ERR_EXIST = SGL_FS_EXISTS,
    SLFS_ERR_NO_SPACE = SGL_FS_NO_SPACE,
    SLFS_ERR_TOO_MANY_OPEN = SGL_FS_ERROR,
    SLFS_ERR_NOT_DIR = SGL_FS_ERROR,
    SLFS_ERR_NOT_EMPTY = SGL_FS_ERROR,
    SLFS_ERR_NO_MEM = SGL_FS_NO_MEMORY,
    SLFS_ERR_BADF = SGL_FS_INVALID_ARGUMENT,
    SLFS_ERR_INVAL = SGL_FS_INVALID_ARGUMENT,
};

/* Superblock layout (block 0, raw byte offsets):
 *   0: magic (uint32_t)
 *   4: version (uint32_t)
 *   8: block_size (uint32_t)
 *  12: block_count (uint32_t)
 *  16: inode_count (uint32_t)
 *  20: inode_table_start (uint32_t)
 *  24: free_head (uint32_t)
 *  28: free_count (uint32_t)
 *  32: root_inode (uint32_t)
 *  36: inodes_per_block (uint32_t)
 */

/* Inode - stored in inode table blocks */
typedef struct {
    uint32_t type;             /* 0=free, 1=file, 2=dir */
    uint32_t size;             /* file size in bytes */
    uint32_t data_head;        /* first data block (0=none) */
    uint32_t data_count;       /* number of data blocks */
    uint32_t parent_inode;     /* parent directory inode index */
} sgl_packed slfs_inode_t;

/* Data block header - first bytes of each data block */
typedef struct {
    uint32_t next_block;       /* next data block in chain (0=end) */
    uint32_t used_bytes;       /* bytes used in this block's data area */
} sgl_packed slfs_data_hdr_t;

/* Directory entry - stored sequentially in directory data blocks */
typedef struct {
    uint8_t  name_len;         /* length of name (0=deleted entry) */
    uint8_t  type;             /* 1=file, 2=dir */
    uint32_t inode;            /* inode index */
    /* followed by name_len bytes of name (no null terminator on disk) */
} sgl_packed slfs_dirent_t;

typedef struct {
    uint8_t  used;
    uint32_t inode_idx;        /* inode index */
    uint32_t cur_pos;          /* current read/write position */
    uint32_t cur_block;        /* current data block for r/w */
    uint32_t cur_block_off;    /* offset within current block's data area */
    uint8_t  flags;
    uint8_t  dirty;
} slfs_file_t;

typedef struct {
    uint8_t  used;
    uint32_t inode_idx;        /* directory inode index */
    uint32_t entry_pos;        /* byte position in dir data stream */
    uint8_t  finished;
} slfs_dir_t;

typedef struct {
    sgl_block_dev_t *dev;
    uint32_t block_size;
    uint32_t sector_size;
    uint32_t block_count;
    uint32_t inode_count;
    uint32_t inode_table_start;
    uint32_t free_head;
    uint32_t free_count;
    uint32_t root_inode;
    uint32_t inodes_per_block;
    uint32_t data_area_size;   /* usable bytes per data block */

    uint8_t *blk_buf;          /* block-sized buffer */
    uint32_t blk_buf_num;      /* block number in buffer */
    uint8_t  blk_buf_dirty;

    slfs_file_t files[SLFS_MAX_OPEN_FILES];
    slfs_dir_t  dirs[SLFS_MAX_OPEN_DIRS];
} slfs_ctx_t;

static int slfs_read_block(slfs_ctx_t *ctx, uint32_t block)
{
    uint32_t sectors_per_block = ctx->block_size / ctx->sector_size;
    if (sectors_per_block == 0) sectors_per_block = 1;
    uint32_t start_sector = block * sectors_per_block;
    for (uint32_t i = 0; i < sectors_per_block; i++) {
        if (ctx->dev->read_sectors(ctx->dev, start_sector + i,
            ctx->blk_buf + i * ctx->sector_size, 1) != 0) {
            SGL_LOG_ERROR(LFS_FS_NAME " read_block: block=%d sector=%d failed", block, start_sector + i);
            return SLFS_ERR_IO;
        }
    }
    ctx->blk_buf_num = block;
    ctx->blk_buf_dirty = 0;
    return SLFS_OK;
}

static int slfs_write_block(slfs_ctx_t *ctx, uint32_t block)
{
    uint32_t sectors_per_block = ctx->block_size / ctx->sector_size;
    if (sectors_per_block == 0) sectors_per_block = 1;
    uint32_t start_sector = block * sectors_per_block;
    for (uint32_t i = 0; i < sectors_per_block; i++) {
        if (ctx->dev->write_sectors(ctx->dev, start_sector + i,
            ctx->blk_buf + i * ctx->sector_size, 1) != 0) {
            SGL_LOG_ERROR(LFS_FS_NAME " write_block: block=%d sector=%d failed", block, start_sector + i);
            return SLFS_ERR_IO;
        }
    }
    ctx->blk_buf_num = block;
    ctx->blk_buf_dirty = 0;
    return SLFS_OK;
}

static int slfs_cache_read(slfs_ctx_t *ctx, uint32_t block)
{
    if (ctx->blk_buf_num == block) return SLFS_OK;
    if (ctx->blk_buf_dirty) {
        int ret = slfs_write_block(ctx, ctx->blk_buf_num);
        if (ret != SLFS_OK) {
            SGL_LOG_ERROR(LFS_FS_NAME " cache_read: flush block=%d failed", ctx->blk_buf_num);
            return ret;
        }
    }
    return slfs_read_block(ctx, block);
}

static void slfs_cache_dirty(slfs_ctx_t *ctx) { ctx->blk_buf_dirty = 1; }

static int slfs_cache_flush(slfs_ctx_t *ctx)
{
    if (ctx->blk_buf_dirty) {
        int ret = slfs_write_block(ctx, ctx->blk_buf_num);
        if (ret != SLFS_OK) {
            SGL_LOG_ERROR(LFS_FS_NAME " cache_flush: write block=%d failed", ctx->blk_buf_num);
            return ret;
        }
    }
    return SLFS_OK;
}

static uint32_t slfs_alloc_block(slfs_ctx_t *ctx)
{
    if (ctx->free_head == 0) return 0;
    uint32_t block = ctx->free_head;

    /* Read the free block to get next pointer */
    if (slfs_read_block(ctx, block) != SLFS_OK) return 0;
    uint32_t next;
    memcpy(&next, ctx->blk_buf, sizeof(uint32_t));
    ctx->free_head = next;
    ctx->free_count--;

    /* Zero the allocated block */
    memset(ctx->blk_buf, 0, ctx->block_size);
    slfs_write_block(ctx, block);

    /* Update superblock */
    slfs_read_block(ctx, 0);
    memcpy(&ctx->blk_buf[24], &ctx->free_head, 4);
    memcpy(&ctx->blk_buf[28], &ctx->free_count, 4);
    slfs_write_block(ctx, 0);

    return block;
}

static int slfs_free_block(slfs_ctx_t *ctx, uint32_t block)
{
    if (block == 0) return SLFS_OK;
    memset(ctx->blk_buf, 0, ctx->block_size);
    memcpy(ctx->blk_buf, &ctx->free_head, 4);
    slfs_write_block(ctx, block);
    ctx->free_head = block;
    ctx->free_count++;

    /* Update superblock */
    slfs_read_block(ctx, 0);
    memcpy(&ctx->blk_buf[24], &ctx->free_head, 4);
    memcpy(&ctx->blk_buf[28], &ctx->free_count, 4);
    slfs_write_block(ctx, 0);
    return SLFS_OK;
}

static int slfs_free_chain(slfs_ctx_t *ctx, uint32_t head)
{
    uint32_t cur = head;
    while (cur != 0) {
        if (slfs_read_block(ctx, cur) != SLFS_OK) return SLFS_ERR_IO;
        uint32_t next;
        memcpy(&next, ctx->blk_buf, 4);
        slfs_free_block(ctx, cur);
        cur = next;
    }
    return SLFS_OK;
}

static int slfs_read_inode(slfs_ctx_t *ctx, uint32_t idx, slfs_inode_t *inode)
{
    uint32_t block = ctx->inode_table_start + idx / ctx->inodes_per_block;
    uint32_t offset = (idx % ctx->inodes_per_block) * sizeof(slfs_inode_t);

    if (slfs_cache_read(ctx, block) != SLFS_OK) return SLFS_ERR_IO;
    memcpy(inode, &ctx->blk_buf[offset], sizeof(slfs_inode_t));
    return SLFS_OK;
}

static int slfs_write_inode(slfs_ctx_t *ctx, uint32_t idx, const slfs_inode_t *inode)
{
    uint32_t block = ctx->inode_table_start + idx / ctx->inodes_per_block;
    uint32_t offset = (idx % ctx->inodes_per_block) * sizeof(slfs_inode_t);

    if (slfs_cache_read(ctx, block) != SLFS_OK) return SLFS_ERR_IO;
    memcpy(&ctx->blk_buf[offset], inode, sizeof(slfs_inode_t));
    slfs_cache_dirty(ctx);
    return slfs_cache_flush(ctx);
}

static uint32_t slfs_alloc_inode(slfs_ctx_t *ctx)
{
    slfs_inode_t inode;
    for (uint32_t i = 1; i < ctx->inode_count; i++) {
        if (slfs_read_inode(ctx, i, &inode) != SLFS_OK) {
            SGL_LOG_ERROR(LFS_FS_NAME " alloc_inode: read inode %d failed", i);
            continue;
        }
        if (inode.type == 0) {
            inode.type = SLFS_TYPE_FILE; /* caller sets correct type */
            inode.size = 0;
            inode.data_head = 0;
            inode.data_count = 0;
            inode.parent_inode = 0;
            slfs_write_inode(ctx, i, &inode);
            return i;
        }
    }
    SGL_LOG_ERROR(LFS_FS_NAME " alloc_inode: no free inodes (count=%d)", ctx->inode_count);
    return 0;
}

static int slfs_free_inode(slfs_ctx_t *ctx, uint32_t idx)
{
    if (idx == 0) return SLFS_ERR_INVALID;
    slfs_inode_t inode;
    if (slfs_read_inode(ctx, idx, &inode) != SLFS_OK) return SLFS_ERR_IO;

    /* Free all data blocks */
    if (inode.data_head != 0)
        slfs_free_chain(ctx, inode.data_head);

    /* Clear inode */
    memset(&inode, 0, sizeof(inode));
    return slfs_write_inode(ctx, idx, &inode);
}

/* Get Nth block in a data chain (0-based) */
static uint32_t slfs_chain_get(slfs_ctx_t *ctx, uint32_t head, uint32_t index)
{
    uint32_t cur = head;
    for (uint32_t i = 0; i < index && cur != 0; i++) {
        if (slfs_cache_read(ctx, cur) != SLFS_OK) return 0;
        memcpy(&cur, ctx->blk_buf, 4);
    }
    return cur;
}

/* Append a new block to the end of a chain, returns new block number */
static uint32_t slfs_chain_append(slfs_ctx_t *ctx, uint32_t head)
{
    uint32_t new_block = slfs_alloc_block(ctx);
    if (new_block == 0) return 0;

    if (head == 0) return new_block;

    /* Find last block in chain */
    uint32_t cur = head;
    while (1) {
        if (slfs_cache_read(ctx, cur) != SLFS_OK) {
            slfs_free_block(ctx, new_block);
            return 0;
        }
        uint32_t next;
        memcpy(&next, ctx->blk_buf, 4);
        if (next == 0) break;
        cur = next;
    }

    /* Link last block to new block */
    memcpy(ctx->blk_buf, &new_block, 4);
    slfs_cache_dirty(ctx);
    slfs_cache_flush(ctx);

    return new_block;
}

static int slfs_read_dir_stream(slfs_ctx_t *ctx, const slfs_inode_t *dir_inode,
                                uint32_t byte_pos, uint8_t *buf, uint32_t len)
{
    uint32_t data_area = ctx->data_area_size;
    uint32_t bytes_read = 0;

    if (byte_pos >= dir_inode->size || len > dir_inode->size - byte_pos)
        return SLFS_ERR_NOT_FOUND;

    while (bytes_read < len) {
        uint32_t stream_pos = byte_pos + bytes_read;
        uint32_t block_idx = stream_pos / data_area;
        uint32_t offset_in_block = stream_pos % data_area;
        uint32_t block = slfs_chain_get(ctx, dir_inode->data_head, block_idx);
        if (block == 0) return SLFS_ERR_NOT_FOUND;
        if (slfs_cache_read(ctx, block) != SLFS_OK) return SLFS_ERR_IO;

        uint32_t avail = data_area - offset_in_block;
        uint32_t chunk = (len - bytes_read < avail) ? len - bytes_read : avail;
        memcpy(buf + bytes_read,
               ctx->blk_buf + sizeof(slfs_data_hdr_t) + offset_in_block,
               chunk);
        bytes_read += chunk;
    }

    return SLFS_OK;
}

/* Read a directory entry at a given byte position in the directory's data stream.
   Returns SLFS_OK if found, SLFS_ERR_NOT_FOUND at end. */
static int slfs_read_dirent(slfs_ctx_t *ctx, uint32_t inode_idx,
                            uint32_t byte_pos, slfs_dirent_t *dirent,
                            char *name_buf, uint32_t name_buf_size)
{
    slfs_inode_t dir_inode;
    if (slfs_read_inode(ctx, inode_idx, &dir_inode) != SLFS_OK)
        return SLFS_ERR_IO;
    if (dir_inode.type != SLFS_TYPE_DIR || dir_inode.data_head == 0)
        return SLFS_ERR_NOT_FOUND;
    if (byte_pos >= dir_inode.size)
        return SLFS_ERR_NOT_FOUND;

    int ret = slfs_read_dir_stream(ctx, &dir_inode, byte_pos,
                                   (uint8_t *)dirent, sizeof(slfs_dirent_t));
    if (ret != SLFS_OK) return ret;
    if (dirent->name_len == 0) return SLFS_ERR_NOT_FOUND; /* deleted */

    if (byte_pos + sizeof(slfs_dirent_t) + dirent->name_len > dir_inode.size)
        return SLFS_ERR_NOT_FOUND;

    if (name_buf && name_buf_size > 0) {
        uint32_t copy_len = dirent->name_len;
        if (copy_len >= name_buf_size) copy_len = name_buf_size - 1;

        ret = slfs_read_dir_stream(ctx, &dir_inode,
                                   byte_pos + sizeof(slfs_dirent_t),
                                   (uint8_t *)name_buf, copy_len);
        if (ret != SLFS_OK) return ret;
        name_buf[copy_len] = '\0';
    }
    return SLFS_OK;
}

/* Total bytes a directory entry occupies */
static uint32_t slfs_dirent_total_size(uint8_t name_len)
{
    return (uint32_t)sizeof(slfs_dirent_t) + name_len;
}

/* Search a directory for a name. Returns inode index or 0 if not found. */
static uint32_t slfs_dir_find(slfs_ctx_t *ctx, uint32_t dir_inode,
                              const char *name, uint8_t *out_type)
{
    uint32_t name_len = (uint32_t)strlen(name);
    if (name_len == 0 || name_len > SLFS_MAX_NAME_LEN) return 0;

    slfs_inode_t dir;
    if (slfs_read_inode(ctx, dir_inode, &dir) != SLFS_OK) return 0;
    if (dir.type != SLFS_TYPE_DIR) return 0;

    uint32_t pos = 0;
    while (pos < dir.size) {
        slfs_dirent_t dirent;
        char entry_name[SLFS_MAX_NAME_LEN + 1];
        int ret = slfs_read_dirent(ctx, dir_inode, pos, &dirent,
                                   entry_name, sizeof(entry_name));
        if (ret != SLFS_OK) {
            pos += 1;
            continue;
        }
        if (dirent.name_len == name_len &&
            memcmp(entry_name, name, name_len) == 0) {
            if (out_type) *out_type = dirent.type;
            return dirent.inode;
        }
        pos += slfs_dirent_total_size(dirent.name_len);
    }
    return 0;
}

/* Write data into a file inode's data chain at the end, extending as needed.
   Used for appending directory entries. */
static int slfs_append_data(slfs_ctx_t *ctx, uint32_t inode_idx,
                            const uint8_t *data, uint32_t len)
{
    slfs_inode_t inode;
    if (slfs_read_inode(ctx, inode_idx, &inode) != SLFS_OK) return SLFS_ERR_IO;

    uint32_t data_area = ctx->data_area_size;
    uint32_t written = 0;

    while (written < len) {
        uint32_t file_pos = inode.size + written;
        uint32_t block_idx = file_pos / data_area;
        uint32_t offset_in_data = file_pos % data_area;

        uint32_t block;
        if (inode.data_head == 0 || block_idx >= inode.data_count) {
            /* Need new block */
            block = slfs_chain_append(ctx, inode.data_head);
            if (block == 0) return SLFS_ERR_NO_SPACE;
            if (inode.data_head == 0) inode.data_head = block;
            inode.data_count++;

            /* Initialize block header */
            memset(ctx->blk_buf, 0, ctx->block_size);
            uint32_t zero = 0;
            memcpy(ctx->blk_buf, &zero, 4);  /* next = 0 */
            memcpy(ctx->blk_buf + 4, &zero, 4); /* used_bytes = 0 */
            slfs_cache_dirty(ctx);
            if (slfs_cache_flush(ctx) != SLFS_OK) return SLFS_ERR_IO;
        } else {
            block = slfs_chain_get(ctx, inode.data_head, block_idx);
            if (block == 0) return SLFS_ERR_IO;
        }

        if (slfs_cache_read(ctx, block) != SLFS_OK) return SLFS_ERR_IO;

        uint32_t avail = data_area - offset_in_data;
        uint32_t chunk = (len - written < avail) ? len - written : avail;
        memcpy(ctx->blk_buf + sizeof(slfs_data_hdr_t) + offset_in_data,
               data + written, chunk);

        /* Update used_bytes */
        uint32_t used = offset_in_data + chunk;
        uint32_t old_used;
        memcpy(&old_used, ctx->blk_buf + 4, 4);
        if (used > old_used) memcpy(ctx->blk_buf + 4, &used, 4);

        slfs_cache_dirty(ctx);
        if (slfs_cache_flush(ctx) != SLFS_OK) return SLFS_ERR_IO;

        written += chunk;
    }

    inode.size += len;
    return slfs_write_inode(ctx, inode_idx, &inode);
}

/* Write a directory entry into a directory inode. */
static int slfs_resolve_path(slfs_ctx_t *ctx, const char *path,
                             uint32_t *parent_inode, char *final_name,
                             uint32_t final_name_size, uint8_t *has_final_name)
{
    if (!path || !parent_inode || !final_name || final_name_size == 0 || !has_final_name)
        return SLFS_ERR_INVALID;

    while (*path == '/') path++;
    if (*path == '\0') {
        *parent_inode = ctx->root_inode;
        final_name[0] = '\0';
        *has_final_name = 0;
        return SLFS_OK;
    }

    uint32_t cur = ctx->root_inode;
    char component[SLFS_MAX_NAME_LEN + 1];

    *has_final_name = 0;
    final_name[0] = '\0';

    while (*path) {
        uint32_t component_len = 0;
        while (path[component_len] && path[component_len] != '/') {
            if (component_len >= SLFS_MAX_NAME_LEN)
                return SLFS_ERR_INVALID;
            component[component_len] = path[component_len];
            component_len++;
        }
        component[component_len] = '\0';

        path += component_len;
        while (*path == '/') path++;

        if (*path == '\0') {
            if (component_len == 0 || component_len >= final_name_size)
                return SLFS_ERR_INVALID;
            memcpy(final_name, component, component_len + 1);
            *parent_inode = cur;
            *has_final_name = 1;
            return SLFS_OK;
        }

        uint8_t type = 0;
        uint32_t found = slfs_dir_find(ctx, cur, component, &type);
        if (found == 0) return SLFS_ERR_NOT_FOUND;
        if (type != SLFS_TYPE_DIR) return SLFS_ERR_NOT_DIR;
        cur = found;
    }

    *parent_inode = cur;
    final_name[0] = '\0';
    *has_final_name = 0;
    return SLFS_OK;
}

/* Add a directory entry to a directory inode */
static int slfs_dir_add_entry(slfs_ctx_t *ctx, uint32_t dir_inode,
                              const char *name, uint8_t type, uint32_t child_inode)
{
    uint32_t name_len = (uint32_t)strlen(name);
    if (name_len == 0 || name_len > SLFS_MAX_NAME_LEN)
        return SLFS_ERR_INVALID;

    uint32_t entry_size = sizeof(slfs_dirent_t) + name_len;
    uint8_t buf[sizeof(slfs_dirent_t) + SLFS_MAX_NAME_LEN];
    if (entry_size > sizeof(buf)) return SLFS_ERR_INVALID;

    slfs_dirent_t dirent;
    dirent.name_len = (uint8_t)name_len;
    dirent.type = type;
    dirent.inode = child_inode;
    memcpy(buf, &dirent, sizeof(slfs_dirent_t));
    memcpy(buf + sizeof(slfs_dirent_t), name, name_len);

    return slfs_append_data(ctx, dir_inode, buf, entry_size);
}

/* Mark a directory entry as deleted (set name_len=0) */
static int slfs_dir_remove_entry(slfs_ctx_t *ctx, uint32_t dir_inode,
                                 const char *name)
{
    uint32_t name_len = (uint32_t)strlen(name);
    slfs_inode_t dir;
    if (slfs_read_inode(ctx, dir_inode, &dir) != SLFS_OK) return SLFS_ERR_IO;

    uint32_t pos = 0;
    while (pos < dir.size) {
        slfs_dirent_t dirent;
        char entry_name[SLFS_MAX_NAME_LEN + 1];
        int ret = slfs_read_dirent(ctx, dir_inode, pos, &dirent,
                                   entry_name, sizeof(entry_name));
        if (ret == SLFS_OK && dirent.name_len == name_len &&
            memcmp(entry_name, name, name_len) == 0) {
            /* Found - zero out name_len to mark deleted */
            uint32_t data_area = ctx->data_area_size;
            uint32_t block_idx = pos / data_area;
            uint32_t offset_in_data = pos % data_area;
            uint32_t block = slfs_chain_get(ctx, dir.data_head, block_idx);
            if (block == 0) return SLFS_ERR_IO;

            if (slfs_cache_read(ctx, block) != SLFS_OK) {
                return SLFS_ERR_IO;
            }

            /* Tombstone the whole record so later scans do not read stale name bytes. */
            slfs_dirent_t tombstone;
            memset(&tombstone, 0, sizeof(tombstone));
            memcpy(ctx->blk_buf + sizeof(slfs_data_hdr_t) + offset_in_data, &tombstone, sizeof(tombstone));
            slfs_cache_dirty(ctx);

            if (slfs_cache_flush(ctx) != SLFS_OK) {
                return SLFS_ERR_IO;
            }
            return SLFS_OK;
        }
        if (ret == SLFS_OK) {
            pos += slfs_dirent_total_size(dirent.name_len);
        } else {
            pos++;
        }
    }
    return SLFS_ERR_NOT_FOUND;
}

/* Check if directory is empty (no live entries) */
static int slfs_dir_is_empty(slfs_ctx_t *ctx, uint32_t dir_inode)
{
    slfs_inode_t dir;
    if (slfs_read_inode(ctx, dir_inode, &dir) != SLFS_OK) return 1;
    if (dir.size == 0) return 1;

    uint32_t pos = 0;
    while (pos < dir.size) {
        slfs_dirent_t dirent;
        int ret = slfs_read_dirent(ctx, dir_inode, pos, &dirent, NULL, 0);
        if (ret == SLFS_OK && dirent.name_len > 0) return 0;
        if (ret == SLFS_OK) {
            pos += slfs_dirent_total_size(dirent.name_len);
        } else {
            pos++;
        }
    }
    return 1;
}

/* Read file data from an inode's data chain */
static int slfs_read_file_data(slfs_ctx_t *ctx, uint32_t inode_idx,
                               uint32_t offset, uint8_t *buf, uint32_t len)
{
    slfs_inode_t inode;
    if (slfs_read_inode(ctx, inode_idx, &inode) != SLFS_OK) return SLFS_ERR_IO;
    if (offset >= inode.size) return 0;
    if (offset + len > inode.size) len = inode.size - offset;
    if (len == 0) return 0;

    uint32_t data_area = ctx->data_area_size;
    uint32_t bytes_read = 0;

    while (bytes_read < len) {
        uint32_t file_pos = offset + bytes_read;
        uint32_t block_idx = file_pos / data_area;
        uint32_t off_in_data = file_pos % data_area;

        uint32_t block = slfs_chain_get(ctx, inode.data_head, block_idx);
        if (block == 0) break;

        if (slfs_cache_read(ctx, block) != SLFS_OK) break;

        uint32_t avail = data_area - off_in_data;
        uint32_t chunk = (len - bytes_read < avail) ? len - bytes_read : avail;
        memcpy(buf + bytes_read, ctx->blk_buf + sizeof(slfs_data_hdr_t) + off_in_data, chunk);
        bytes_read += chunk;
    }
    return (int)bytes_read;
}

/* Write file data into an inode's data chain at a given offset */
static int slfs_write_file_data(slfs_ctx_t *ctx, uint32_t inode_idx,
                                uint32_t offset, const uint8_t *buf, uint32_t len)
{
    slfs_inode_t inode;
    if (slfs_read_inode(ctx, inode_idx, &inode) != SLFS_OK) return SLFS_ERR_IO;

    uint32_t data_area = ctx->data_area_size;
    uint32_t bytes_written = 0;

    while (bytes_written < len) {
        uint32_t file_pos = offset + bytes_written;
        uint32_t block_idx = file_pos / data_area;
        uint32_t off_in_data = file_pos % data_area;

        uint32_t block;
        if (inode.data_head == 0 || block_idx >= inode.data_count) {
            block = slfs_chain_append(ctx, inode.data_head);
            if (block == 0) return bytes_written > 0 ? (int)bytes_written : SLFS_ERR_NO_SPACE;
            if (inode.data_head == 0) inode.data_head = block;
            inode.data_count++;
            if (slfs_cache_read(ctx, block) != SLFS_OK) return bytes_written > 0 ? (int)bytes_written : SLFS_ERR_IO;
            memset(ctx->blk_buf, 0, ctx->block_size);
            uint32_t zero = 0;
            memcpy(ctx->blk_buf, &zero, 4);
            memcpy(ctx->blk_buf + 4, &zero, 4);
            slfs_cache_dirty(ctx);
            if (slfs_cache_flush(ctx) != SLFS_OK) return bytes_written > 0 ? (int)bytes_written : SLFS_ERR_IO;
        } else {
            block = slfs_chain_get(ctx, inode.data_head, block_idx);
            if (block == 0) break;
        }

        if (slfs_cache_read(ctx, block) != SLFS_OK) break;

        uint32_t avail = data_area - off_in_data;
        uint32_t chunk = (len - bytes_written < avail) ? len - bytes_written : avail;
        memcpy(ctx->blk_buf + sizeof(slfs_data_hdr_t) + off_in_data,
               buf + bytes_written, chunk);

        uint32_t used = off_in_data + chunk;
        uint32_t old_used;
        memcpy(&old_used, ctx->blk_buf + 4, 4);
        if (used > old_used) memcpy(ctx->blk_buf + 4, &used, 4);
        slfs_cache_dirty(ctx);
        if (slfs_cache_flush(ctx) != SLFS_OK) return bytes_written > 0 ? (int)bytes_written : SLFS_ERR_IO;

        bytes_written += chunk;
        uint32_t new_end = offset + bytes_written;
        if (new_end > inode.size) inode.size = new_end;
    }

    if (slfs_write_inode(ctx, inode_idx, &inode) != SLFS_OK) return SLFS_ERR_IO;
    return (int)bytes_written;
}

static int littlefs_format(void *fs);

static int littlefs_mount(void **fs, sgl_block_dev_t *dev,
                          const char *mount_point, void *fs_config)
{
    (void)mount_point;
    (void)fs_config;

    if (!dev) {
        SGL_LOG_ERROR(LFS_FS_NAME " mount: invalid dev");
        return SLFS_ERR_INVAL;
    }
    uint32_t sector_size = 512;
    if (sgl_block_dev_ioctl(dev, SGL_BLK_GET_SECTOR_SIZE, &sector_size) != 0 || sector_size == 0) {
        sector_size = 512;
    }

    uint32_t fs_block_size = 0;
    if (fs_block_size == 0) {
        uint32_t block_size_sectors = 0;
        if (sgl_block_dev_ioctl(dev, SGL_BLK_GET_BLOCK_SIZE, &block_size_sectors) == 0 && block_size_sectors != 0) {
            fs_block_size = block_size_sectors * sector_size;
        } else if (dev->info && dev->info->erase_block_size != 0) {
            fs_block_size = dev->info->erase_block_size;
        } else if (dev->info && dev->info->block_size != 0) {
            fs_block_size = dev->info->block_size * sector_size;
        } else {
            fs_block_size = sector_size;
        }
    }

    if (fs_block_size < sector_size || (fs_block_size % sector_size) != 0) {
        SGL_LOG_ERROR(LFS_FS_NAME " mount: invalid block_size=%d for sector_size=%d", fs_block_size, sector_size);
        return SLFS_ERR_INVAL;
    }

    if (dev->init && dev->init(dev) != 0) {
        SGL_LOG_ERROR(LFS_FS_NAME " mount: dev init failed");
        return SLFS_ERR_IO;
    }

    slfs_ctx_t *ctx = (slfs_ctx_t *)sgl_malloc(sizeof(slfs_ctx_t));
    if (!ctx) {
        SGL_LOG_ERROR(LFS_FS_NAME " mount: malloc ctx failed");
        return SLFS_ERR_NO_MEM;
    }
    memset(ctx, 0, sizeof(slfs_ctx_t));

    ctx->dev = dev;
    ctx->block_size = fs_block_size;
    ctx->sector_size = sector_size;
    ctx->blk_buf_num = 0xFFFFFFFF;

    /* Auto-detect block count */
    uint32_t sector_count = 0;
    if (sgl_block_dev_ioctl(dev, SGL_BLK_GET_SECTOR_COUNT, &sector_count) != 0 || sector_count == 0) {
        if (dev->info && dev->info->sector_count != 0) {
            sector_count = dev->info->sector_count;
        } else {
            SGL_LOG_ERROR(LFS_FS_NAME " mount: failed to get sector count");
            sgl_free(ctx);
            return SLFS_ERR_IO;
        }
    }

    uint32_t sectors_per_block = fs_block_size / sector_size;
    ctx->block_count = sector_count / sectors_per_block;
    if (ctx->block_count == 0) {
        SGL_LOG_ERROR(LFS_FS_NAME " mount: no available fs blocks");
        sgl_free(ctx);
        return SLFS_ERR_INVAL;
    }

    ctx->blk_buf = (uint8_t *)sgl_malloc(fs_block_size);
    if (!ctx->blk_buf) {
        SGL_LOG_ERROR(LFS_FS_NAME " mount: malloc blk_buf failed");
        sgl_free(ctx);
        return SLFS_ERR_NO_MEM;
    }

    /* Read superblock */
    if (slfs_read_block(ctx, 0) != SLFS_OK) {
        SGL_LOG_ERROR(LFS_FS_NAME " mount: read superblock failed");
        sgl_free(ctx->blk_buf);
        sgl_free(ctx);
        return SLFS_ERR_IO;
    }

    uint32_t magic;
    memcpy(&magic, ctx->blk_buf, 4);
    if (magic != SLFS_MAGIC) {
        SGL_LOG_WARN(LFS_FS_NAME " mount: invalid magic=0x%x, formatting...", magic);
        
        /* Auto-format the device */
        int ret = littlefs_format(ctx);
        if (ret != SLFS_OK) {
            SGL_LOG_ERROR(LFS_FS_NAME " mount: auto-format failed");
            sgl_free(ctx->blk_buf);
            sgl_free(ctx);
            return SLFS_ERR_IO;
        }
        
        /* Re-read superblock after format */
        if (slfs_read_block(ctx, 0) != SLFS_OK) {
            SGL_LOG_ERROR(LFS_FS_NAME " mount: read superblock after format failed");
            sgl_free(ctx->blk_buf);
            sgl_free(ctx);
            return SLFS_ERR_IO;
        }
        
        memcpy(&magic, ctx->blk_buf, 4);
        if (magic != SLFS_MAGIC) {
            SGL_LOG_ERROR(LFS_FS_NAME " mount: still invalid magic=0x%x after format", magic);
            sgl_free(ctx->blk_buf);
            sgl_free(ctx);
            return SLFS_ERR_INVALID;
        }
    }

    memcpy(&ctx->inode_count, ctx->blk_buf + 16, 4);
    memcpy(&ctx->inode_table_start, ctx->blk_buf + 20, 4);
    memcpy(&ctx->free_head, ctx->blk_buf + 24, 4);
    memcpy(&ctx->free_count, ctx->blk_buf + 28, 4);
    memcpy(&ctx->root_inode, ctx->blk_buf + 32, 4);
    memcpy(&ctx->inodes_per_block, ctx->blk_buf + 36, 4);
    ctx->data_area_size = ctx->block_size - sizeof(slfs_data_hdr_t);

    slfs_inode_t verify_inode;
    if (slfs_read_inode(ctx, 1, &verify_inode) != SLFS_OK) {
        SGL_LOG_ERROR(LFS_FS_NAME " mount: failed to read inode[1]");
    }

    *fs = ctx;
    return SLFS_OK;
}

static int littlefs_unmount(void *fs, const char *mount_point)
{
    (void)mount_point;
    slfs_ctx_t *ctx = (slfs_ctx_t *)fs;
    if (!ctx) return SLFS_ERR_INVALID;
    slfs_cache_flush(ctx);
    sgl_free(ctx->blk_buf);
    sgl_free(ctx);
    return SLFS_OK;
}

static int littlefs_open(void *fs, const char *path, uint32_t flags)
{
    slfs_ctx_t *ctx = (slfs_ctx_t *)fs;
    int slot = -1;
    for (int i = 0; i < SLFS_MAX_OPEN_FILES; i++)
        if (!ctx->files[i].used) { slot = i; break; }
    if (slot < 0) {
        SGL_LOG_ERROR(LFS_FS_NAME " open: too many open files");
        return SLFS_ERR_TOO_MANY_OPEN;
    }

    uint32_t parent_inode;
    char fname[SLFS_MAX_NAME_LEN + 1];
    uint8_t has_fname;
    int ret = slfs_resolve_path(ctx, path, &parent_inode, fname,
                                sizeof(fname), &has_fname);
    if (ret != SLFS_OK) {
        SGL_LOG_ERROR(LFS_FS_NAME " open: resolve_path failed, ret=%d", ret);
        return ret;
    }
    if (!has_fname) {
        SGL_LOG_ERROR(LFS_FS_NAME " open: fname is NULL");
        return SLFS_ERR_INVALID;
    }

    uint8_t type = 0;
    uint32_t found = slfs_dir_find(ctx, parent_inode, fname, &type);

    if (found != 0) {
        if (type != SLFS_TYPE_FILE) {
            SGL_LOG_ERROR(LFS_FS_NAME " open: not a file");
            return SLFS_ERR_NOT_DIR;
        }
        if (flags & SGL_O_TRUNC) {
            slfs_inode_t inode;
            memset(&inode, 0, sizeof(inode));
            if (slfs_read_inode(ctx, found, &inode) != SLFS_OK) {
                SGL_LOG_ERROR(LFS_FS_NAME " open: read inode failed");
                return SLFS_ERR_IO;
            }
            if (inode.data_head) slfs_free_chain(ctx, inode.data_head);
            inode.data_head = 0; inode.data_count = 0; inode.size = 0;
            slfs_write_inode(ctx, found, &inode);
        }
    } else {
        if (!(flags & SGL_O_CREAT)) {
            SGL_LOG_ERROR(LFS_FS_NAME " open: file not found and not CREAT");
            return SLFS_ERR_NOT_FOUND;
        }
        found = slfs_alloc_inode(ctx);
        if (found == 0) {
            SGL_LOG_ERROR(LFS_FS_NAME " open: alloc inode failed");
            return SLFS_ERR_NO_SPACE;
        }
        slfs_inode_t inode;
        slfs_read_inode(ctx, found, &inode);
        inode.type = SLFS_TYPE_FILE;
        inode.parent_inode = parent_inode;
        slfs_write_inode(ctx, found, &inode);
        ret = slfs_dir_add_entry(ctx, parent_inode, fname, SLFS_TYPE_FILE, found);
        if (ret != SLFS_OK) {
            slfs_free_inode(ctx, found);
            return ret;
        }
    }

    slfs_file_t *f = &ctx->files[slot];
    memset(f, 0, sizeof(slfs_file_t));
    f->used = 1;
    f->inode_idx = found;
    f->flags = (uint8_t)(flags & 0xFF);
    f->cur_pos = 0;
    if (flags & SGL_O_APPEND) {
        slfs_inode_t inode;
        memset(&inode, 0, sizeof(inode));
        if (slfs_read_inode(ctx, found, &inode) != SLFS_OK) {
            SGL_LOG_ERROR(LFS_FS_NAME " open: read inode failed");
            return SLFS_ERR_IO;
        }
        f->cur_pos = inode.size;
    }
    return slot;
}

static int littlefs_close(void *fs, int fd)
{
    slfs_ctx_t *ctx = (slfs_ctx_t *)fs;
    if (fd < 0 || fd >= SLFS_MAX_OPEN_FILES || !ctx->files[fd].used) {
        SGL_LOG_ERROR(LFS_FS_NAME " close: bad fd=%d", fd);
        return SLFS_ERR_BADF;
    }
    slfs_cache_flush(ctx);
    ctx->files[fd].used = 0;
    return SLFS_OK;
}

static int littlefs_read(void *fs, int fd, void *buffer, uint32_t count)
{
    slfs_ctx_t *ctx = (slfs_ctx_t *)fs;
    if (fd < 0 || fd >= SLFS_MAX_OPEN_FILES || !ctx->files[fd].used) {
        SGL_LOG_ERROR(LFS_FS_NAME " read: bad fd=%d", fd);
        return SLFS_ERR_BADF;
    }
    slfs_file_t *f = &ctx->files[fd];
    /* Check read permission */
    if ((f->flags & 0x03) == SGL_O_WRONLY) {
        SGL_LOG_ERROR(LFS_FS_NAME " read: file opened as write-only");
        return SLFS_ERR_DENIED;
    }

    int n = slfs_read_file_data(ctx, f->inode_idx, f->cur_pos,
                                (uint8_t *)buffer, count);
    if (n > 0) f->cur_pos += (uint32_t)n;
    return n;
}

static int littlefs_write(void *fs, int fd, const void *buffer, uint32_t count)
{
    slfs_ctx_t *ctx = (slfs_ctx_t *)fs;
    if (fd < 0 || fd >= SLFS_MAX_OPEN_FILES || !ctx->files[fd].used) {
        SGL_LOG_ERROR(LFS_FS_NAME " write: bad fd=%d", fd);
        return SLFS_ERR_BADF;
    }
    slfs_file_t *f = &ctx->files[fd];
    /* Check write permission */
    if ((f->flags & 0x03) == SGL_O_RDONLY) {
        SGL_LOG_ERROR(LFS_FS_NAME " write: file opened as read-only");
        return SLFS_ERR_DENIED;
    }

    int n = slfs_write_file_data(ctx, f->inode_idx, f->cur_pos,
                                 (const uint8_t *)buffer, count);
    if (n > 0) f->cur_pos += (uint32_t)n;
    return n;
}

static int littlefs_opendir(void *fs, const char *path, int *dd)
{
    slfs_ctx_t *ctx = (slfs_ctx_t *)fs;
    int slot = -1;
    for (int i = 0; i < SLFS_MAX_OPEN_DIRS; i++)
        if (!ctx->dirs[i].used) { slot = i; break; }
    if (slot < 0) {
        SGL_LOG_ERROR(LFS_FS_NAME " opendir: too many open dirs");
        return SLFS_ERR_TOO_MANY_OPEN;
    }

    uint32_t parent_inode;
    char fname[SLFS_MAX_NAME_LEN + 1];
    uint8_t has_fname;
    int ret = slfs_resolve_path(ctx, path, &parent_inode, fname,
                                sizeof(fname), &has_fname);
    if (ret != SLFS_OK) {
        SGL_LOG_ERROR(LFS_FS_NAME " opendir: resolve_path failed, ret=%d", ret);
        return ret;
    }

    uint32_t dir_inode;
    if (!has_fname) {
        dir_inode = parent_inode;
    } else {
        uint8_t type = 0;
        dir_inode = slfs_dir_find(ctx, parent_inode, fname, &type);
        if (dir_inode == 0) {
            SGL_LOG_ERROR(LFS_FS_NAME " opendir: find dir failed");
            return SLFS_ERR_NOT_FOUND;
        }
        if (type != SLFS_TYPE_DIR) {
            SGL_LOG_ERROR(LFS_FS_NAME " opendir: not a directory");
            return SLFS_ERR_NOT_DIR;
        }
    }

    slfs_dir_t *d = &ctx->dirs[slot];
    d->used = 1;
    d->inode_idx = dir_inode;
    d->entry_pos = 0;
    d->finished = 0;
    *dd = slot;
    return SLFS_OK;
}

static int littlefs_readdir(void *fs, int dd, char *name,
                            uint32_t name_size, uint32_t *type)
{
    slfs_ctx_t *ctx = (slfs_ctx_t *)fs;
    if (dd < 0 || dd >= SLFS_MAX_OPEN_DIRS || !ctx->dirs[dd].used)
        return SLFS_ERR_BADF;
    slfs_dir_t *d = &ctx->dirs[dd];
    if (d->finished) return 0;

    slfs_inode_t dir;
    memset(&dir, 0, sizeof(dir));
    if (slfs_read_inode(ctx, d->inode_idx, &dir) != SLFS_OK) {
        SGL_LOG_ERROR(LFS_FS_NAME " readdir: read inode failed");
        return SLFS_ERR_IO;
    }

    while (d->entry_pos < dir.size) {
        slfs_dirent_t dirent;
        char entry_name[SLFS_MAX_NAME_LEN + 1];
        int ret = slfs_read_dirent(ctx, d->inode_idx, d->entry_pos,
                                   &dirent, entry_name, sizeof(entry_name));
        if (ret == SLFS_OK && dirent.name_len > 0) {
            d->entry_pos += slfs_dirent_total_size(dirent.name_len);
            if (name && name_size > 0) {
                uint32_t copy = dirent.name_len < name_size - 1 ? dirent.name_len : name_size - 1;
                memcpy(name, entry_name, copy);
                name[copy] = '\0';
            }
            if (type) *type = (dirent.type == SLFS_TYPE_DIR) ? SGL_S_IFDIR : SGL_S_IFREG;
            return 1;
        }
        if (ret == SLFS_OK) {
            d->entry_pos += slfs_dirent_total_size(dirent.name_len);
        } else {
            d->entry_pos++;
        }
    }
    d->finished = 1;
    return 0;
}

static int littlefs_closedir(void *fs, int dd)
{
    slfs_ctx_t *ctx = (slfs_ctx_t *)fs;
    if (dd < 0 || dd >= SLFS_MAX_OPEN_DIRS || !ctx->dirs[dd].used) {
        SGL_LOG_ERROR(LFS_FS_NAME " closedir: bad dd=%d", dd);
        return SLFS_ERR_BADF;
    }
    ctx->dirs[dd].used = 0;
    return SLFS_OK;
}

static int littlefs_sync(void *fs)
{
    slfs_ctx_t *ctx = (slfs_ctx_t *)fs;
    slfs_cache_flush(ctx);
    if (ctx->dev->ioctl) ctx->dev->ioctl(ctx->dev, SGL_BLK_CTRL_SYNC, NULL);
    return SLFS_OK;
}

static int littlefs_format(void *fs)
{
    slfs_ctx_t *ctx = (slfs_ctx_t *)fs;
    if (!ctx) return SLFS_ERR_INVALID;

    SGL_LOG_INFO(LFS_FS_NAME " format: Formatting");
    SGL_LOG_INFO(LFS_FS_NAME " format: ..");

    uint32_t bs = ctx->block_size;
    uint32_t bc = ctx->block_count;
    uint32_t ipb = bs / sizeof(slfs_inode_t);
    if (ipb == 0) ipb = 1;

    /* Decide inode table size: enough for ~block_count/4 inodes */
    uint32_t desired_inodes = bc / 4;
    if (desired_inodes < 16) desired_inodes = 16;
    uint32_t inode_blocks = (desired_inodes + ipb - 1) / ipb;
    uint32_t inode_table_start = 1; /* block 0 = superblock */
    uint32_t data_start = inode_table_start + inode_blocks;

    /* Write superblock */
    memset(ctx->blk_buf, 0, bs);
    uint32_t magic = SLFS_MAGIC;
    uint32_t ver = SLFS_VERSION;
    uint32_t root = 1;
    memcpy(ctx->blk_buf, &magic, 4);
    memcpy(ctx->blk_buf + 4, &ver, 4);
    memcpy(ctx->blk_buf + 8, &bs, 4);
    memcpy(ctx->blk_buf + 12, &bc, 4);
    memcpy(ctx->blk_buf + 16, &desired_inodes, 4);
    memcpy(ctx->blk_buf + 20, &inode_table_start, 4);
    /* free_head and free_count set below */
    memcpy(ctx->blk_buf + 32, &root, 4);
    memcpy(ctx->blk_buf + 36, &ipb, 4);
    slfs_write_block(ctx, 0);

    SGL_LOG_INFO(LFS_FS_NAME " format: ....");

    /* Initialize layout metadata before any inode operations */
    ctx->inode_count = desired_inodes;
    ctx->inode_table_start = inode_table_start;
    ctx->inodes_per_block = ipb;
    ctx->root_inode = 1;
    ctx->data_area_size = bs - sizeof(slfs_data_hdr_t);

    /* Zero inode table */
    memset(ctx->blk_buf, 0, bs);
    for (uint32_t i = 0; i < inode_blocks; i++) {
        slfs_write_block(ctx, inode_table_start + i);
    }

    SGL_LOG_INFO(LFS_FS_NAME " format: ......");

    /* Build free list */
    uint32_t free_head = 0;
    uint32_t free_count = 0;

    for (uint32_t b = bc - 1; b >= data_start; b--) {
        memset(ctx->blk_buf, 0, bs);
        memcpy(ctx->blk_buf, &free_head, 4);
        slfs_write_block(ctx, b);
        free_head = b;
        free_count++;
    }

    SGL_LOG_INFO(LFS_FS_NAME " format: ........");

    /* Update superblock with free list */
    slfs_read_block(ctx, 0);
    memcpy(ctx->blk_buf + 24, &free_head, 4);
    memcpy(ctx->blk_buf + 28, &free_count, 4);
    slfs_write_block(ctx, 0);

    /* Create root directory inode */
    slfs_inode_t root_inode;
    memset(&root_inode, 0, sizeof(root_inode));
    root_inode.type = SLFS_TYPE_DIR;
    root_inode.parent_inode = 1; /* root's parent is itself */
    slfs_write_inode(ctx, 1, &root_inode);

    /* Force flush all caches to NOR flash */
    slfs_cache_flush(ctx);
    sgl_block_dev_ioctl(ctx->dev, SGL_BLK_CTRL_SYNC, NULL);

    SGL_LOG_INFO(LFS_FS_NAME " format: .......... done!");

    /* Reload context from new superblock */
    ctx->inode_count = desired_inodes;
    ctx->inode_table_start = inode_table_start;
    ctx->free_head = free_head;
    ctx->free_count = free_count;
    ctx->root_inode = root;
    ctx->inodes_per_block = ipb;
    ctx->data_area_size = bs - sizeof(slfs_data_hdr_t);

    return SLFS_OK;
}

static int littlefs_remove(void *fs, const char *path)
{
    slfs_ctx_t *ctx = (slfs_ctx_t *)fs;
    uint32_t parent_inode;
    char fname[SLFS_MAX_NAME_LEN + 1];
    uint8_t has_fname;
    int ret = slfs_resolve_path(ctx, path, &parent_inode, fname,
                                sizeof(fname), &has_fname);
    if (ret != SLFS_OK) return ret;
    if (!has_fname) return SLFS_ERR_INVALID;

    uint8_t type = 0;
    uint32_t found = slfs_dir_find(ctx, parent_inode, fname, &type);
    if (found == 0) return SLFS_ERR_NOT_FOUND;

    if (type == SLFS_TYPE_DIR) {
        if (!slfs_dir_is_empty(ctx, found)) return SLFS_ERR_NOT_EMPTY;
    }

    if (slfs_dir_remove_entry(ctx, parent_inode, fname) != SLFS_OK) {
        return SLFS_ERR_IO;
    }
    if (slfs_free_inode(ctx, found) != SLFS_OK) {
        return SLFS_ERR_IO;
    }
    return SLFS_OK;
}

static int littlefs_mkdir(void *fs, const char *path)
{
    slfs_ctx_t *ctx = (slfs_ctx_t *)fs;
    uint32_t parent_inode;
    char fname[SLFS_MAX_NAME_LEN + 1];
    uint8_t has_fname;
    int ret = slfs_resolve_path(ctx, path, &parent_inode, fname,
                                sizeof(fname), &has_fname);
    if (ret != SLFS_OK) return ret;
    if (!has_fname) return SLFS_ERR_INVALID;

    uint8_t type = 0;
    if (slfs_dir_find(ctx, parent_inode, fname, &type) != 0)
        return SLFS_ERR_EXIST;

    uint32_t new_inode = slfs_alloc_inode(ctx);
    if (new_inode == 0) return SLFS_ERR_NO_SPACE;

    slfs_inode_t inode;
    slfs_read_inode(ctx, new_inode, &inode);
    inode.type = SLFS_TYPE_DIR;
    inode.parent_inode = parent_inode;
    if (slfs_write_inode(ctx, new_inode, &inode) != SLFS_OK) {
        slfs_free_inode(ctx, new_inode);
        return SLFS_ERR_IO;
    }

    ret = slfs_dir_add_entry(ctx, parent_inode, fname, SLFS_TYPE_DIR, new_inode);
    if (ret != SLFS_OK) {
        slfs_free_inode(ctx, new_inode);
        return ret;
    }
    return SLFS_OK;
}

static int littlefs_stat(void *fs, const char *path, sgl_stat_t *st)
{
    slfs_ctx_t *ctx = (slfs_ctx_t *)fs;
    if (!st) return SLFS_ERR_INVAL;

    uint32_t parent_inode;
    char fname[SLFS_MAX_NAME_LEN + 1];
    uint8_t has_fname;
    int ret = slfs_resolve_path(ctx, path, &parent_inode, fname,
                                sizeof(fname), &has_fname);
    if (ret != SLFS_OK) return ret;

    if (!has_fname) {
        memset(st, 0, sizeof(sgl_stat_t));
        st->st_mode = SGL_S_IFDIR | SGL_S_IRWXU | SGL_S_IRWXG | SGL_S_IRWXO;
        return SLFS_OK;
    }

    uint8_t type = 0;
    uint32_t found = slfs_dir_find(ctx, parent_inode, fname, &type);
    if (found == 0) return SLFS_ERR_NOT_FOUND;

    slfs_inode_t inode;
    memset(&inode, 0, sizeof(inode));
    if (slfs_read_inode(ctx, found, &inode) != SLFS_OK) {
        SGL_LOG_ERROR(LFS_FS_NAME " stat: read inode failed");
        return SLFS_ERR_IO;
    }
    st->st_size = inode.size;
    st->st_mode = (type == SLFS_TYPE_DIR) ? SGL_S_IFDIR : SGL_S_IFREG;
    st->st_mode |= SGL_S_IRWXU | SGL_S_IRWXG | SGL_S_IRWXO;
    st->st_mtime = 0;
    return SLFS_OK;
}

static int littlefs_rename(void *fs, const char *old_path, const char *new_path)
{
    slfs_ctx_t *ctx = (slfs_ctx_t *)fs;
    uint32_t old_parent, new_parent;
    char old_name[SLFS_MAX_NAME_LEN + 1];
    char new_name[SLFS_MAX_NAME_LEN + 1];
    uint8_t has_old_name;
    uint8_t has_new_name;

    int ret = slfs_resolve_path(ctx, old_path, &old_parent, old_name,
                                sizeof(old_name), &has_old_name);
    if (ret != SLFS_OK) return ret;
    if (!has_old_name) return SLFS_ERR_INVALID;

    ret = slfs_resolve_path(ctx, new_path, &new_parent, new_name,
                            sizeof(new_name), &has_new_name);
    if (ret != SLFS_OK) return ret;
    if (!has_new_name) return SLFS_ERR_INVALID;

    uint8_t type = 0;
    uint32_t found = slfs_dir_find(ctx, old_parent, old_name, &type);
    if (found == 0) return SLFS_ERR_NOT_FOUND;

    /* Remove existing target if present */
    uint8_t new_type = 0;
    uint32_t existing = slfs_dir_find(ctx, new_parent, new_name, &new_type);
    if (existing != 0) {
        if (slfs_dir_remove_entry(ctx, new_parent, new_name) != SLFS_OK) {
            return SLFS_ERR_IO;
        }
        if (slfs_free_inode(ctx, existing) != SLFS_OK) {
            return SLFS_ERR_IO;
        }
    }

    /* Add new entry */
    if (slfs_dir_add_entry(ctx, new_parent, new_name, type, found) != SLFS_OK) {
        return SLFS_ERR_IO;
    }
    /* Remove old entry */
    if (slfs_dir_remove_entry(ctx, old_parent, old_name) != SLFS_OK) {
        return SLFS_ERR_IO;
    }
    return SLFS_OK;
}

static int littlefs_statvfs(void *fs, sgl_statvfs_t *info)
{
    slfs_ctx_t *ctx = (slfs_ctx_t *)fs;
    if (!ctx || !info) return SLFS_ERR_INVALID;

    memset(info, 0, sizeof(*info));
    info->f_bsize = ctx->block_size;
    info->f_blocks = ctx->block_count;
    info->f_bfree = ctx->free_count;
    info->f_bavail = ctx->free_count;
    info->f_files = 0;
    info->f_ffree = 0;

    return SLFS_OK;
}

static sgl_fs_ops_t littlefs_ops = {
    .mount    = littlefs_mount,
    .unmount  = littlefs_unmount,
    .open     = littlefs_open,
    .close    = littlefs_close,
    .read     = littlefs_read,
    .write    = littlefs_write,
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

static sgl_fs_type_t littlefs_type = {
    .name = LFS_FS_NAME,
    .ops  = &littlefs_ops,
};

int sgl_littlefs_register(void)
{
    return sgl_fs_register(&littlefs_type);
}
