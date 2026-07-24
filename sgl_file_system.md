# SGL 文件系统使用说明

SGL 内置了三种文件系统实现，通过统一的 VFS（虚拟文件系统）层提供 POSIX 风格的文件操作接口。本文以 `附件` 为示例，分三部分说明 FATFS、LittleFS 和 RAMFS 的使用方法。

---

## 目录

1. [FATFS — 兼容 SD 卡 / 硬盘的 FAT 文件系统](#1-fatfs--兼容-sd-卡--硬盘的-fat-文件系统)
2. [LittleFS — 面向 Flash 的轻量日志文件系统](#2-littlefs--面向-flash-的轻量日志文件系统)
3. [RAMFS — 基于内存的临时文件系统](#3-ramfs--基于内存的临时文件系统)
4. [附录：VFS API 速查](#4-附录vfs-api-速查)

---

## 1. FATFS — 兼容 SD 卡 / 硬盘的 FAT 文件系统

FATFS 是 SGL 自带的 FAT12/16/32 文件系统实现，兼容 Windows/macOS/Linux 格式化的 SD 卡和硬盘，支持长文件名（LFN）。

### 1.1 底层接口对接

FATFS 通过 `sgl_block_dev_t` 结构体与底层存储设备交互。用户需要实现以下接口：

```c
// 定义在 sgl/include/sgl_block.h

typedef struct sgl_block_dev {
    int (*init)(struct sgl_block_dev *dev);                    // 设备初始化（可选，可设为 NULL）
    int (*read_sectors)(struct sgl_block_dev *dev, uint32_t sector, uint8_t *buf, uint32_t count);   // 读扇区
    int (*write_sectors)(struct sgl_block_dev *dev, uint32_t sector, const uint8_t *buf, uint32_t count); // 写扇区
    int (*ioctl)(struct sgl_block_dev *dev, uint8_t cmd, void *param);  // 控制命令（可选）
    int (*status)(struct sgl_block_dev *dev);                  // 状态查询（可选）
    const sgl_block_dev_info_t *info;                          // 设备信息
} sgl_block_dev_t;
```

`sgl_block_dev_info_t` 结构体：

```c
typedef struct sgl_block_dev_info {
    uint32_t sector_count;      // 总扇区数
    uint32_t sector_size;       // 每扇区字节数（通常 512）
    uint32_t block_size;        // 擦除块大小（以扇区为单位）
    uint32_t erase_block_size;  // 擦除块大小（以字节为单位）
    uint32_t page_size;         // 编程页大小（以字节为单位）
} sgl_block_dev_info_t;
```

### 1.2 用户需要实现的接口（以 SD 卡为例）

参考 `附件` 中的 SD 卡对接示例：

```c
#include <sgl_fs.h>

/* 1. 设备初始化函数 */
int sdcard_init(struct sgl_block_dev *dev)
{
    return (sdio_sd_init() == SD_OK) ? 0 : -1;
}

/* 2. 读扇区函数 */
int sdio_sd_read_sectors(struct sgl_block_dev *dev, uint32_t sector, uint8_t *buf, uint32_t count)
{
    return (sd_read_disk(buf, sector, count) == SD_OK) ? 0 : -1;
}

/* 3. 写扇区函数 */
int sdio_sd_write_sectors(struct sgl_block_dev *dev, uint32_t sector, const uint8_t *buf, uint32_t count)
{
    return (sd_write_disk((uint8_t *)buf, sector, count) == SD_OK) ? 0 : -1;
}

/* 4. 设备信息结构体 */
const static sgl_block_dev_info_t sdcard_info = {
    .sector_size = 512,         // 每扇区 512 字节
    .block_size = 0,            // 无擦除块限制
    .erase_block_size = 0,
    .page_size = 512,
};

/* 5. 块设备结构体实例 */
const sgl_block_dev_t sdcard_block_dev = {
    .init = sdcard_init,
    .read_sectors = sdio_sd_read_sectors,
    .write_sectors = sdio_sd_write_sectors,
    .info = &sdcard_info,
};
```

### 1.3 注册与挂载

```c
#include "../sgl/fs/fatfs/sgl_fatfs.h"

/* 1. 注册 FATFS 文件系统类型（在 main 初始化时调用一次） */
sgl_fatfs_register();

/* 2. 挂载 FATFS 到挂载点 */
sgl_fs_mount("/", "fatfs", &sdcard_block_dev, NULL);
```

挂载配置参数（可选）：

```c
typedef struct {
    uint8_t vol_id;     // 卷 ID（0-9），默认 0
    uint8_t opt;        // 挂载选项（0=自动，1=强制挂载）
} sgl_fatfs_config_t;

// 示例：指定卷 ID
sgl_fatfs_config_t fatfs_cfg = {
    .vol_id = 0,
    .opt = 1,
};
sgl_fs_mount("/", "fatfs", &sdcard_block_dev, &fatfs_cfg);
```

### 1.4 文件读写示例

```c
/* 写文件 */
int fd = sgl_fs_open("/test.txt", SGL_O_WRONLY | SGL_O_CREAT | SGL_O_TRUNC);
if (fd >= 0) {
    const char *data = "Hello, SGL FATFS!";
    sgl_fs_write(fd, data, strlen(data));
    sgl_fs_close(fd);
}

/* 读文件 */
fd = sgl_fs_open("/test.txt", SGL_O_RDONLY);
if (fd >= 0) {
    char buf[128] = {0};
    sgl_fs_read(fd, buf, sizeof(buf) - 1);
    printf("Read: %s\n", buf);
    sgl_fs_close(fd);
}
```

### 1.5 目录操作示例

```c
/* 创建目录 */
sgl_fs_mkdir("/mydir");

/* 遍历目录 */
int dd;
if (sgl_fs_opendir("/", &dd) == 0) {
    char name[256];
    uint32_t type;
    while (sgl_fs_readdir(dd, name, sizeof(name), &type) > 0) {
        if (type & SGL_S_IFDIR) {
            printf("[DIR]  %s\n", name);
        } else {
            printf("[FILE] %s\n", name);
        }
    }
    sgl_fs_closedir(dd);
}
```

### 1.6 其他操作

```c
/* 获取文件状态 */
sgl_stat_t st;
sgl_fs_stat("/test.txt", &st);
printf("Size: %u\n", st.st_size);

/* 删除文件 */
sgl_fs_remove("/test.txt");

/* 重命名 */
sgl_fs_rename("/old.txt", "/new.txt");

/* 获取文件系统空间信息 */
sgl_statvfs_t vfs;
sgl_fs_statvfs("/", &vfs);
printf("Total: %u blocks, Free: %u blocks\n", vfs.f_blocks, vfs.f_bfree);

/* 卸载文件系统 */
sgl_fs_unmount("/");
```

### 1.7 注意事项

- FATFS 支持 MBR 分区表，会自动检测并跳转到第一个 FAT 分区
- 支持 FAT12/FAT16/FAT32，自动识别
- 支持长文件名（LFN），最大 32 个 Unicode 字符
- 扇区大小必须是 2 的幂，通常为 512 字节
- 格式化使用 `sgl_fs_format("/", NULL)`，会创建 FAT16 文件系统

---

## 2. LittleFS — 面向 Flash 的轻量日志文件系统

LittleFS 是 SGL 自带的轻量级日志文件系统（兼容 littlefs v2 设计），专为 Flash 存储设计，具有掉电安全和磨损均衡特性。

### 2.1 底层接口对接

LittleFS 同样通过 `sgl_block_dev_t` 与底层设备交互，接口与 FATFS 相同。但 LittleFS 需要设备提供**擦除块大小**信息。

参考 `附件` 中的 NOR Flash 对接示例：

```c
#include "../bsp/norflash/norflash.h"

/* 1. 设备初始化 */
int __norflash_init(struct sgl_block_dev *dev)
{
    norflash_init();
    return 0;
}

/* 2. 读扇区（NOR Flash 扇区大小为 4096 字节） */
int __norflash_read_sectors(struct sgl_block_dev *dev, uint32_t sector, uint8_t *buf, uint32_t count)
{
    uint32_t i;
    for (i = 0; i < count; i++) {
        norflash_read(buf + (i * 4096), sector * 4096 + (i * 4096), 4096);
    }
    return 0;
}

/* 3. 写扇区 */
int __norflash_write_sectors(struct sgl_block_dev *dev, uint32_t sector, const uint8_t *buf, uint32_t count)
{
    uint32_t i;
    for (i = 0; i < count; i++) {
        norflash_write((uint8_t *)buf + (i * 4096), sector * 4096 + (i * 4096), 4096);
    }
    return 0;
}

/* 4. 设备信息（必须提供 erase_block_size 或 block_size） */
const sgl_block_dev_info_t norflash_info = {
    .sector_count = 4096,           // 总扇区数
    .sector_size = 4096,            // 每扇区 4096 字节
    .block_size = 1,                // 擦除块 = 1 个扇区
    .erase_block_size = 4096,       // 擦除块大小 4096 字节
    .page_size = 256,               // 编程页大小
};

/* 5. 块设备实例 */
const sgl_block_dev_t norflash_block_dev = {
    .init = __norflash_init,
    .read_sectors = __norflash_read_sectors,
    .write_sectors = __norflash_write_sectors,
    .info = &norflash_info,
};
```

### 2.2 设备信息要求

LittleFS 在挂载时会自动检测设备信息，按以下优先级确定**文件系统块大小**：

1. `ioctl(SGL_BLK_GET_BLOCK_SIZE)` 返回的块大小（扇区数）× 扇区大小
2. `dev->info->erase_block_size`（擦除块大小，字节）
3. `dev->info->block_size × sector_size`
4. 如果以上都不可用，默认使用 `sector_size`（512 字节）

> **注意**：对于 NOR Flash，建议在 `info` 中正确设置 `erase_block_size` 或 `block_size`，否则 LittleFS 会使用 512 字节作为块大小，可能导致性能下降。

### 2.3 注册、格式化与挂载

LittleFS 在挂载时会自动检测设备上的超级块。对于全新的 Flash 设备，需要先格式化才能挂载。格式化有两种方式：

**方式一：挂载时自动格式化（推荐）**

```c
#include "../sgl/fs/littlefs/sgl_littlefs.h"

/* 1. 注册 LittleFS 文件系统类型 */
sgl_littlefs_register();

/* 2. 挂载（首次挂载未格式化的设备会自动格式化） */
sgl_fs_mount("/flash", "littlefs", &norflash_block_dev, NULL);
```

> 挂载时如果检测到无效的超级块（magic 不正确），会自动调用 `sgl_fs_format()` 格式化设备。这是最简便的方式。

**方式二：先手动格式化，再挂载**

```c
/* 1. 注册 LittleFS */
sgl_littlefs_register();

/* 2. 先挂载（自动格式化失败时挂载会返回错误，但 ctx 已创建）*/
/*    对于全新设备，直接用方式一即可自动格式化                    */
/*    如果需要强制重建，先挂载再单独格式化：                      */
sgl_fs_mount("/flash", "littlefs", &norflash_block_dev, NULL);

/* 3. 单独格式化（会调用已挂载文件系统的 format 函数） */
sgl_fs_format("/flash", NULL);

/* 4. 格式化后文件系统已重建，可直接使用 */
```

> 如果自动格式化失败，通常是因为设备信息（`sector_count`、`erase_block_size` 等）未正确配置。请检查 `sgl_block_dev_info_t` 中的字段是否填写完整。

### 2.4 文件读写示例

```c
/* 写文件 */
int fd = sgl_fs_open("/flash/config.txt", SGL_O_WRONLY | SGL_O_CREAT | SGL_O_TRUNC);
if (fd >= 0) {
    const char *data = "{\"wifi\":\"SSID\",\"password\":\"123456\"}";
    sgl_fs_write(fd, data, strlen(data));
    sgl_fs_close(fd);
}

/* 读文件 */
fd = sgl_fs_open("/flash/config.txt", SGL_O_RDONLY);
if (fd >= 0) {
    char buf[256] = {0};
    sgl_fs_read(fd, buf, sizeof(buf) - 1);
    printf("Config: %s\n", buf);
    sgl_fs_close(fd);
}
```

### 2.5 目录操作

```c
/* 创建目录 */
sgl_fs_mkdir("/flash/data");

/* 遍历目录 */
int dd;
if (sgl_fs_opendir("/flash", &dd) == 0) {
    char name[256];
    uint32_t type;
    while (sgl_fs_readdir(dd, name, sizeof(name), &type) > 0) {
        printf("%s: %s\n", (type & SGL_S_IFDIR) ? "[DIR]" : "[FILE]", name);
    }
    sgl_fs_closedir(dd);
}
```

### 2.6 注意事项

- LittleFS 是日志结构文件系统，写入不需要先擦除，直接覆盖即可
- 具有掉电安全特性，异常断电不会损坏文件系统
- 文件系统块大小由设备信息自动确定，建议与 Flash 的擦除块大小一致
- 文件名最大长度 255 字节
- 支持目录层级，路径分隔符为 `/`
- 首次挂载未格式化的设备时会自动格式化；也可先调用 `sgl_fs_format()` 手动格式化后再挂载
- 如果自动格式化失败，请检查设备 `info` 中的 `sector_count`、`erase_block_size` 等字段是否正确

---

## 3. RAMFS — 基于内存的临时文件系统

RAMFS 是完全基于 RAM 的文件系统，所有数据存储在堆内存中，掉电即丢失。适用于临时文件、配置文件缓存、运行时数据等场景。

### 3.1 特点

- **无需底层设备对接**：RAMFS 不需要 `sgl_block_dev_t`，挂载时传入 `NULL`
- **动态内存分配**：文件数据通过 `sgl_malloc` / `sgl_free` 动态管理
- **速度快**：纯内存操作，读写速度极快
- **容量限制**：受可用堆内存限制

### 3.2 注册与挂载

```c
#include "../sgl/fs/ramfs/sgl_ramfs.h"

/* 1. 注册 RAMFS 文件系统类型 */
sgl_ramfs_register();

/* 2. 挂载 RAMFS（dev 参数传 NULL） */
sgl_fs_mount("/tmp", "ramfs", NULL, NULL);
```

挂载配置参数（可选）：

```c
typedef struct {
    uint32_t max_size;   // 最大文件数据字节数（0 = 不限制，直到堆耗尽）
} sgl_ramfs_config_t;

// 限制 RAMFS 最大使用 64KB
sgl_ramfs_config_t ramfs_cfg = {
    .max_size = 64 * 1024,
};
sgl_fs_mount("/tmp", "ramfs", NULL, &ramfs_cfg);
```

### 3.3 文件读写示例

```c
/* 写文件 */
int fd = sgl_fs_open("/tmp/hello.txt", SGL_O_WRONLY | SGL_O_CREAT | SGL_O_TRUNC);
if (fd >= 0) {
    const char *data = "Hello, RAMFS!";
    sgl_fs_write(fd, data, strlen(data));
    sgl_fs_close(fd);
}

/* 读文件 */
fd = sgl_fs_open("/tmp/hello.txt", SGL_O_RDONLY);
if (fd >= 0) {
    char buf[64] = {0};
    sgl_fs_read(fd, buf, sizeof(buf) - 1);
    printf("Read: %s\n", buf);
    sgl_fs_close(fd);
}

/* 追加写入 */
fd = sgl_fs_open("/tmp/hello.txt", SGL_O_WRONLY | SGL_O_APPEND);
if (fd >= 0) {
    sgl_fs_write(fd, "\nAppended line.", 15);
    sgl_fs_close(fd);
}
```

### 3.4 目录操作

```c
/* 创建目录结构 */
sgl_fs_mkdir("/tmp/cache");
sgl_fs_mkdir("/tmp/session");

/* 遍历根目录 */
int dd;
if (sgl_fs_opendir("/tmp", &dd) == 0) {
    char name[256];
    uint32_t type;
    while (sgl_fs_readdir(dd, name, sizeof(name), &type) > 0) {
        printf("%s: %s\n", (type & SGL_S_IFDIR) ? "[DIR]" : "[FILE]", name);
    }
    sgl_fs_closedir(dd);
}
```

### 3.5 典型应用场景

```c
/* 场景 1：缓存从 SD 卡读取的配置文件 */
int src = sgl_fs_open("/config.ini", SGL_O_RDONLY);      // 从 FATFS 读取
if (src >= 0) {
    int dst = sgl_fs_open("/tmp/config.ini", SGL_O_WRONLY | SGL_O_CREAT | SGL_O_TRUNC);  // 缓存到 RAMFS
    if (dst >= 0) {
        char buf[512];
        int n;
        while ((n = sgl_fs_read(src, buf, sizeof(buf))) > 0) {
            sgl_fs_write(dst, buf, n);
        }
        sgl_fs_close(dst);
    }
    sgl_fs_close(src);
}

/* 场景 2：运行时生成临时数据 */
int fd = sgl_fs_open("/tmp/sensor_log.txt", SGL_O_WRONLY | SGL_O_CREAT | SGL_O_APPEND);
if (fd >= 0) {
    char line[64];
    snprintf(line, sizeof(line), "temp=%.1f humi=%.1f\n", 25.5, 60.0);
    sgl_fs_write(fd, line, strlen(line));
    sgl_fs_close(fd);
}
```

### 3.6 注意事项

- RAMFS 不需要块设备，挂载时 `dev` 参数传 `NULL`
- 所有数据存储在堆内存中，系统重启后数据丢失
- 文件名最大长度 31 字节（由 `SGL_RAMFS_MAX_NAME_LEN` 定义）
- 可通过 `max_size` 配置限制最大内存使用量，防止内存耗尽
- 删除文件会自动释放内存

---

## 4. 附录：VFS API 速查

### 4.1 文件系统注册与挂载

| 函数 | 说明 |
|------|------|
| `sgl_fatfs_register()` | 注册 FATFS 文件系统类型 |
| `sgl_littlefs_register()` | 注册 LittleFS 文件系统类型 |
| `sgl_ramfs_register()` | 注册 RAMFS 文件系统类型 |
| `sgl_fs_mount(path, name, dev, cfg)` | 挂载文件系统到指定路径 |
| `sgl_fs_unmount(path)` | 卸载文件系统 |

### 4.2 文件操作

| 函数 | 说明 |
|------|------|
| `sgl_fs_open(path, flags)` | 打开文件，返回文件描述符 |
| `sgl_fs_close(fd)` | 关闭文件 |
| `sgl_fs_read(fd, buf, count)` | 读取文件数据 |
| `sgl_fs_write(fd, buf, count)` | 写入文件数据 |
| `sgl_fs_remove(path)` | 删除文件或空目录 |
| `sgl_fs_rename(old, new)` | 重命名文件或目录 |
| `sgl_fs_stat(path, st)` | 获取文件状态信息 |

**打开模式标志**：

| 标志 | 说明 |
|------|------|
| `SGL_O_RDONLY` | 只读打开 |
| `SGL_O_WRONLY` | 只写打开 |
| `SGL_O_RDWR` | 读写打开 |
| `SGL_O_CREAT` | 文件不存在时创建 |
| `SGL_O_TRUNC` | 打开时截断文件为 0 长度 |
| `SGL_O_APPEND` | 追加模式写入 |

### 4.3 目录操作

| 函数 | 说明 |
|------|------|
| `sgl_fs_opendir(path, &dd)` | 打开目录，返回目录描述符 |
| `sgl_fs_readdir(dd, name, size, &type)` | 读取目录项（>0 成功，0 结束） |
| `sgl_fs_closedir(dd)` | 关闭目录 |
| `sgl_fs_mkdir(path)` | 创建目录 |

### 4.4 其他操作

| 函数 | 说明 |
|------|------|
| `sgl_fs_sync(path)` | 同步文件系统（刷新缓存） |
| `sgl_fs_format(path, cfg)` | 格式化文件系统 |
| `sgl_fs_statvfs(path, &info)` | 获取文件系统空间信息 |

### 4.5 完整示例：同时使用三种文件系统

```c
#include <sgl_fs.h>
#include "../sgl/fs/fatfs/sgl_fatfs.h"
#include "../sgl/fs/littlefs/sgl_littlefs.h"
#include "../sgl/fs/ramfs/sgl_ramfs.h"

// 外部声明的块设备
extern const sgl_block_dev_t sdcard_block_dev;
extern const sgl_block_dev_t norflash_block_dev;

void fs_demo(void)
{
    /* 注册所有文件系统类型 */
    sgl_fatfs_register();
    sgl_littlefs_register();
    sgl_ramfs_register();

    /* 挂载三个文件系统到不同路径 */
    sgl_fs_mount("/sd",   "fatfs",    &sdcard_block_dev,  NULL);     // SD 卡 -> FATFS
    sgl_fs_mount("/flash","littlefs", &norflash_block_dev, NULL);    // NOR Flash -> LittleFS
    sgl_fs_mount("/tmp",  "ramfs",    NULL,               NULL);     // 内存 -> RAMFS

    /* 从 SD 卡读取配置文件，缓存到 RAMFS */
    int src = sgl_fs_open("/sd/config.ini", SGL_O_RDONLY);
    if (src >= 0) {
        int dst = sgl_fs_open("/tmp/config.ini", SGL_O_WRONLY | SGL_O_CREAT | SGL_O_TRUNC);
        if (dst >= 0) {
            char buf[256];
            int n;
            while ((n = sgl_fs_read(src, buf, sizeof(buf))) > 0) {
                sgl_fs_write(dst, buf, n);
            }
            sgl_fs_close(dst);
        }
        sgl_fs_close(src);
    }

    /* 列出 SD 卡根目录 */
    int dd;
    if (sgl_fs_opendir("/sd", &dd) == 0) {
        char name[256];
        uint32_t type;
        printf("SD Card files:\n");
        while (sgl_fs_readdir(dd, name, sizeof(name), &type) > 0) {
            printf("  %s %s\n", (type & SGL_S_IFDIR) ? "[DIR]" : "[FILE]", name);
        }
        sgl_fs_closedir(dd);
    }

    /* 卸载 */
    sgl_fs_unmount("/tmp");
    sgl_fs_unmount("/flash");
    sgl_fs_unmount("/sd");
}
```

---

> **参考文件**：
```c
#include <sgl_fs.h>
#include <stdint.h>

int sdcard_init(struct sgl_block_dev *dev)
{
    return (sdio_sd_init() == SD_OK) ? 0 : -1;
}

int sdio_sd_read_sectors(struct sgl_block_dev *dev, uint32_t sector, uint8_t *buf, uint32_t count)
{
    return (sd_read_disk(buf, sector, count) == SD_OK) ? 0 : -1;
}

int sdio_sd_write_sectors(struct sgl_block_dev *dev, uint32_t sector, const uint8_t *buf, uint32_t count)
{
    return (sd_write_disk((uint8_t *)buf, sector, count) == SD_OK) ? 0 : -1;
}

const static sgl_block_dev_info_t sdcard_info = {
    .sector_size = 512,
    .block_size = 0,
    .erase_block_size = 0,
    .page_size = 512,
};

const sgl_block_dev_t sdcard_block_dev = {
    .init = sdcard_init,
    .read_sectors = sdio_sd_read_sectors,
    .write_sectors = sdio_sd_write_sectors,
    .info = &sdcard_info,
};

int __norflash_init(struct sgl_block_dev *dev)
{
    norflash_init();
    return 0;
}

int __norflash_read_sectors(struct sgl_block_dev *dev, uint32_t sector, uint8_t *buf, uint32_t count)
{
    uint32_t i;
    
    /* NOR flash sector size is 4096 bytes */
    for (i = 0; i < count; i++)
    {
        norflash_read(buf + (i * 4096), sector * 4096 + (i * 4096), 4096);
    }
    
    return 0;
}

int __norflash_write_sectors(struct sgl_block_dev *dev, uint32_t sector, const uint8_t *buf, uint32_t count)
{
    uint32_t i;
    
    /* NOR flash sector size is 4096 bytes */
    for (i = 0; i < count; i++)
    {
        norflash_write((uint8_t *)buf + (i * 4096), sector * 4096 + (i * 4096), 4096);
    }
    
    return 0;
}

const sgl_block_dev_t norflash_block_dev = {
    .init = __norflash_init,
    .read_sectors = __norflash_read_sectors,
    .write_sectors = __norflash_write_sectors,
    .info = &sgl_w25q128_info,
};
```
