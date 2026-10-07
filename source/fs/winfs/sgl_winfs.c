/* source/fs/winfs/sgl_winfs.c
 *
 * Minimal host-file filesystem for local Windows builds. It exposes the host
 * filesystem through the generic SGL VFS API so AVI files can be opened using
 * the normal sgl_fs_* functions.
 */

#include <sgl_fs.h>
#include <sgl_mm.h>
#include "sgl_winfs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#include <direct.h>
#include <sys/stat.h>
#define WINFS_OPEN(path, flags) _open(path, flags | _O_BINARY)
#define WINFS_CLOSE(fd) _close(fd)
#define WINFS_READ(fd, buf, count) _read(fd, buf, count)
#define WINFS_WRITE(fd, buf, count) _write(fd, buf, count)
#define WINFS_LSEEK(fd, offset, whence) _lseek(fd, offset, whence)
#define WINFS_STAT(path, st) _stat(path, st)
#define WINFS_MKDIR(path) _mkdir(path)
#else
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#define WINFS_OPEN(path, flags) open(path, flags)
#define WINFS_CLOSE(fd) close(fd)
#define WINFS_READ(fd, buf, count) read(fd, buf, count)
#define WINFS_WRITE(fd, buf, count) write(fd, buf, count)
#define WINFS_LSEEK(fd, offset, whence) lseek(fd, offset, whence)
#define WINFS_STAT(path, st) stat(path, st)
#define WINFS_MKDIR(path) mkdir(path, 0777)
#endif

typedef struct {
    char base_dir[512];
} sgl_winfs_ctx_t;

static sgl_winfs_config_t sgl_winfs_default_cfg = { "." };

static void winfs_normalize_path(const char *path, const char *base_dir, char *out, size_t out_size)
{
    const char *src = path;
    char tmp[1024];

    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';
    if (src == NULL) {
        return;
    }

    if (src[0] == '\0') {
        snprintf(out, out_size, "%s", base_dir ? base_dir : ".");
        return;
    }

    if (src[1] == ':') {
        snprintf(out, out_size, "%s", src);
    } else {
        if (src[0] == '/' || src[0] == '\\') {
            snprintf(tmp, sizeof(tmp), "%s%s", base_dir ? base_dir : ".", src);
        } else {
            snprintf(tmp, sizeof(tmp), "%s/%s", base_dir ? base_dir : ".", src);
        }
        snprintf(out, out_size, "%s", tmp);
    }

    for (char *p = out; *p; ++p) {
        if (*p == '/') {
            *p = '\\';
        }
    }
}

static int winfs_mount(void **fs, sgl_block_dev_t *dev, const char *mount_point, void *fs_config)
{
    sgl_winfs_ctx_t *ctx;
    sgl_winfs_config_t *cfg = (sgl_winfs_config_t *)fs_config;

    (void)dev;
    (void)mount_point;

    ctx = (sgl_winfs_ctx_t *)sgl_malloc(sizeof(sgl_winfs_ctx_t));
    if (ctx == NULL) {
        return SGL_FS_NO_MEMORY;
    }

    memset(ctx, 0, sizeof(*ctx));
    snprintf(ctx->base_dir, sizeof(ctx->base_dir), "%s",
             cfg && cfg->base_dir ? cfg->base_dir : sgl_winfs_default_cfg.base_dir);

    *fs = ctx;
    return SGL_FS_OK;
}

static int winfs_unmount(void *fs, const char *mount_point)
{
    (void)mount_point;
    if (fs != NULL) {
        sgl_free(fs);
    }
    return SGL_FS_OK;
}

static int winfs_open(void *fs, const char *path, uint32_t flags)
{
    sgl_winfs_ctx_t *ctx = (sgl_winfs_ctx_t *)fs;
    char host_path[1024];
    int mode = 0;

    if (ctx == NULL || path == NULL) {
        return -1;
    }

    winfs_normalize_path(path, ctx->base_dir, host_path, sizeof(host_path));

    if ((flags & SGL_O_RDWR) == SGL_O_RDWR) {
        mode = _O_RDWR;
    } else if (flags & SGL_O_WRONLY) {
        mode = _O_WRONLY;
    } else {
        mode = _O_RDONLY;
    }

    if (flags & SGL_O_CREAT) {
        mode |= _O_CREAT;
    }
    if (flags & SGL_O_TRUNC) {
        mode |= _O_TRUNC;
    }
    if (flags & SGL_O_APPEND) {
        mode |= _O_APPEND;
    }

    return WINFS_OPEN(host_path, mode);
}

static int winfs_close(void *fs, int fd)
{
    (void)fs;
    return WINFS_CLOSE(fd);
}

static int winfs_read(void *fs, int fd, void *buffer, uint32_t count)
{
    (void)fs;
    if (buffer == NULL || count == 0) {
        return 0;
    }
    return WINFS_READ(fd, buffer, count);
}

static int winfs_write(void *fs, int fd, const void *buffer, uint32_t count)
{
    (void)fs;
    if (buffer == NULL || count == 0) {
        return 0;
    }
    return WINFS_WRITE(fd, buffer, count);
}

static int winfs_seek(void *fs, int fd, int32_t offset, uint8_t whence)
{
    (void)fs;
    return WINFS_LSEEK(fd, offset, (int)whence);
}

static int winfs_opendir(void *fs, const char *path, int *dd)
{
    (void)fs;
    (void)path;
    if (dd == NULL) {
        return SGL_FS_INVALID_ARGUMENT;
    }
    *dd = -1;
    return SGL_FS_ERROR;
}

static int winfs_readdir(void *fs, int dd, char *name, uint32_t name_size, uint32_t *type)
{
    (void)fs;
    (void)dd;
    (void)name;
    (void)name_size;
    (void)type;
    return SGL_FS_ERROR;
}

static int winfs_closedir(void *fs, int dd)
{
    (void)fs;
    (void)dd;
    return SGL_FS_OK;
}

static int winfs_sync(void *fs)
{
    (void)fs;
    return SGL_FS_OK;
}

static int winfs_format(void *fs)
{
    (void)fs;
    return SGL_FS_OK;
}

static int winfs_remove(void *fs, const char *path)
{
    sgl_winfs_ctx_t *ctx = (sgl_winfs_ctx_t *)fs;
    char host_path[1024];

    if (ctx == NULL || path == NULL) {
        return SGL_FS_INVALID_ARGUMENT;
    }
    winfs_normalize_path(path, ctx->base_dir, host_path, sizeof(host_path));
#ifdef _WIN32
    return _unlink(host_path);
#else
    return unlink(host_path);
#endif
}

static int winfs_mkdir(void *fs, const char *path)
{
    sgl_winfs_ctx_t *ctx = (sgl_winfs_ctx_t *)fs;
    char host_path[1024];

    if (ctx == NULL || path == NULL) {
        return SGL_FS_INVALID_ARGUMENT;
    }
    winfs_normalize_path(path, ctx->base_dir, host_path, sizeof(host_path));
    return WINFS_MKDIR(host_path);
}

static int winfs_stat(void *fs, const char *path, sgl_stat_t *st)
{
    sgl_winfs_ctx_t *ctx = (sgl_winfs_ctx_t *)fs;
    char host_path[1024];
#ifdef _WIN32
    struct _stat sb;
#else
    struct stat sb;
#endif

    if (ctx == NULL || path == NULL || st == NULL) {
        return SGL_FS_INVALID_ARGUMENT;
    }
    winfs_normalize_path(path, ctx->base_dir, host_path, sizeof(host_path));
    if (WINFS_STAT(host_path, &sb) != 0) {
        return SGL_FS_NOT_FOUND;
    }

    memset(st, 0, sizeof(*st));
    st->st_mode = (sb.st_mode & S_IFDIR) ? SGL_S_IFDIR : SGL_S_IFREG;
    st->st_size = (uint32_t)sb.st_size;
    st->st_mtime = (uint32_t)sb.st_mtime;
    return SGL_FS_OK;
}

static int winfs_rename(void *fs, const char *old_path, const char *new_path)
{
    sgl_winfs_ctx_t *ctx = (sgl_winfs_ctx_t *)fs;
    char old_host[1024];
    char new_host[1024];

    if (ctx == NULL || old_path == NULL || new_path == NULL) {
        return SGL_FS_INVALID_ARGUMENT;
    }
    winfs_normalize_path(old_path, ctx->base_dir, old_host, sizeof(old_host));
    winfs_normalize_path(new_path, ctx->base_dir, new_host, sizeof(new_host));
    return rename(old_host, new_host);
}

static int winfs_statvfs(void *fs, sgl_statvfs_t *info)
{
    (void)fs;
    if (info == NULL) {
        return SGL_FS_INVALID_ARGUMENT;
    }
    memset(info, 0, sizeof(*info));
    info->f_bsize = 4096;
    info->f_blocks = 0;
    info->f_bfree = 0;
    info->f_bavail = 0;
    info->f_files = 0;
    info->f_ffree = 0;
    return SGL_FS_OK;
}

static sgl_fs_ops_t winfs_ops = {
    winfs_mount,
    winfs_unmount,
    winfs_open,
    winfs_close,
    winfs_read,
    winfs_write,
    winfs_seek,
    winfs_opendir,
    winfs_readdir,
    winfs_closedir,
    winfs_sync,
    winfs_format,
    winfs_remove,
    winfs_mkdir,
    winfs_stat,
    winfs_rename,
    winfs_statvfs,
};

static sgl_fs_type_t winfs_type = {
    {0, 0},
    "winfs",
    &winfs_ops,
};

int sgl_winfs_register(void)
{
    return sgl_fs_register(&winfs_type);
}
