# SGL 配置指南 (sgl_config.h)

## 概述

sgl_config.h是 SGL (Simple Graphics Library) 框架中的**功能配置**。

## 目录

- [SGL 配置指南 (sgl\_config.h)](#sgl-配置指南-sgl_configh)
  - [概述](#概述)
  - [目录](#目录)
  - [显示与帧缓冲设置](#显示与帧缓冲设置)
  - [系统与定时](#系统与定时)
  - [输入事件](#输入事件)
  - [渲染与性能](#渲染与性能)
  - [调试与监控](#调试与监控)
  - [内存管理](#内存管理)
  - [字体系统](#字体系统)
  - [主题与 UI](#主题与-ui)
  - [启动序列](#启动序列)

---

## 显示与帧缓冲设置

这些宏控制与显示硬件 (FBDEV) 的底层交互。

| 宏定义 | 默认值 | 描述 |
| :--- | :---: | :--- |
| [CONFIG_SGL_FBDEV_PIXEL_DEPTH]| `16` | 定义帧缓冲区的颜色深度。支持的值通常为 `8`、`16`、`24` 或 `32` 位每像素。 |
| [CONFIG_SGL_FBDEV_ROTATION]| `0` | 初始化时的静态屏幕旋转角度。选项：`0`, `90`, `180`, `270` 度。 |
| [CONFIG_SGL_FBDEV_RUNTIME_ROTATION] | `0` | 启用运行时屏幕旋转。如果设置为 `1`，应用程序可以在启动后动态旋转屏幕。 |
| [CONFIG_SGL_IMG_BUFFER_LINES] | `0` | 该宏用来设置Flash外部图片时缓冲行大小，默认不使用，设置该宏可以加快外部Flash图片显示速度。|
| [CONFIG_SGL_FBDEV_EVEN_COORDS]| `0` | 如果启用 (`1`)，SGL的每次设置的坐标为偶数，这个主要在QSPI屏幕下使用。|
| [CONFIG_SGL_USE_FBDEV_VRAM]| `0` | 如果启用 (`1`)，SGL 直接写入帧缓冲 VRAM。如果禁用 (`0`)，SGL 可能根据驱动实现使用内部缓冲区。 |
| [CONFIG_SGL_COLOR16_SWAP]| `0` | 交换 16 位颜色的字节顺序（例如 RGB565 与 BGR565）。如果硬件上颜色显示不正确，请启用此项。 |

---

## 系统与定时

控制 GUI 引擎的核心计时机制。

| 宏定义 | 默认值 | 描述 |
| :--- | :---: | :--- |
| [CONFIG_SGL_SYSTICK_MS]| `10` | 系统滴答间隔（毫秒）。这决定了动画、超时和事件处理的粒度。较低的值会增加流畅度，但也会增加 CPU 负载。 |

---

## 输入事件

输入处理的配置，如触摸屏和物理键盘。

| 宏定义 | 默认值 | 描述 |
| :--- | :---: | :--- |
| [CONFIG_SGL_EVENT_QUEUE_SIZE] | `16` | 可以队列化的最大输入事件数（触摸、按键、鼠标）。如果队列已满，新事件可能会被丢弃。对于高频输入设备，请增加此值。 |
| [CONFIG_SGL_FOCUSED_COLOR]| `sgl_rgb(0, 0xff, 0)` | 按键事件聚焦颜色，默认是绿色。 |
| [CONFIG_SGL_FOCUSED_WIDTH]| `1` | 按键事件聚焦宽度，默认是 1。|

---

## 渲染与性能

与绘图操作相关的优化和功能。

| 宏定义 | 默认值 | 描述 |
| :--- | :---: | :--- |
| [CONFIG_SGL_DIRTY_AREA_NUM_MAX]| `16` | 用于部分屏幕刷新的“脏”区域跟踪的最大数量。较高的值允许更复杂的部分更新，但会消耗更多 RAM。 |
| [CONFIG_SGL_PIXMAP_BILINEAR_INTERP]| `0` | 启用 pixmap 缩放的双线性插值。这在缩放图像时提高图像质量，但会显著增加 CPU 使用率。 |
| [CONFIG_SGL_ANIMATION]| `0` | 动画支持的全局开关。如果禁用 (`0`)，所有动画相关代码将被排除以节省空间和 CPU。 |

---

## 调试与监控

开发人员用于跟踪执行过程和可视化内部状态的工具。

| 宏定义 | 默认值 | 描述 |
| :--- | :---: | :--- |
| [CONFIG_SGL_DEBUG]| `0` | 全局调试日志启用。设置为 `1` 以启用日志记录。 |
| [CONFIG_SGL_LOG_COLOR]| `0` | *(仅当 `DEBUG=1` 时有效)* 在终端日志中启用彩色输出以提高可读性。 |
| [CONFIG_SGL_LOG_LEVEL]| `1` | *(仅当 `DEBUG=1` 时有效)* 设置 verbosity 级别。通常：`0`=错误, `1`=警告, `2`=信息, `3`=调试。 |
| [CONFIG_SGL_DIRTY_AREA_TRACE]| `0` | 在屏幕上可视化脏区域。用于优化部分刷新逻辑。 |
| [CONFIG_SGL_DIRTY_AREA_TRACE_COLOR]| [sgl_rgb(0,0,0)]| 用于绘制脏区域调试覆盖层的颜色。 |
| [CONFIG_SGL_MONITOR_TRACE]| `0` | 启用一般监控跟踪（如 FPS、内存使用情况），如果在监控模块中实现的话。 |

---

## 内存管理

配置 SGL 用于动态对象创建的堆分配器。

| 宏定义 | 默认值 | 描述 |
| :--- | :---: | :--- |
| [CONFIG_SGL_HEAP_ALGO]| `lwmem` | 指定堆分配算法。`lwmem` 指的是适用于嵌入式系统的轻量级内存管理器。其他选项可能取决于端口实现。 |
| [CONFIG_SGL_HEAP_MEMORY_SIZE] | `10240` | GUI 堆的总大小（字节）。此内存用于创建小部件、缓冲区和其他动态结构。根据可用 RAM 进行调整。 |
| [CONFIG_SGL_TLSF_INDEX_MAX]| `13` |  **(仅当使用 `tlsf` 算法时有效)** TLSF 分配器中空闲链表（free lists）的最大索引数。该值决定了内存块大小的分类粒度。<br>**推荐设置参考：**<br>- 1KB 内存 -> 设为 10<br>- 2KB 内存 -> 设为 11<br>- 4KB 内存 -> 设为 12<br>- 以此类推，每增加一倍内存，指数加 1。默认值 13 适用于约 8KB-16KB 左右的堆空间。|

---

## 字体系统

控制哪些字体被编译到二进制文件中。仅启用所需的字体可以节省大量的 Flash 空间。

| 宏定义 | 默认值 | 描述 |
| :--- | :---: | :--- |
| [CONFIG_SGL_FONT_COMPRESSED]| `0` | 启用压缩字体格式的支持。 |
| [CONFIG_SGL_FONT_SMALL_TABLE]| `0` | 使用优化/小型的字体查找表。可能会略微降低性能，但节省 RAM。 |
| [CONFIG_SGL_FONT_SONG23]| `0` | 包含 "Song 23px" 字体（通常用于中文 CJK 支持）。 |
| [CONFIG_SGL_FONT_CONSOLAS14]| `0` | 包含 Consolas 14px 字体。 |
| [CONFIG_SGL_FONT_CONSOLAS23]| `0` | 包含 Consolas 23px 字体。 |
| [CONFIG_SGL_FONT_CONSOLAS24]| `0` | 包含 Consolas 24px 字体。 |
| [CONFIG_SGL_FONT_CONSOLAS32]| `0` | 包含 Consolas 32px 字体。 |

> **注意**：如果您打算显示文本，必须至少启用一种字体。

---

## 主题与 UI

默认视觉样式和对象属性。

| 宏定义 | 默认值 | 描述 |
| :--- | :---: | :--- |
| [CONFIG_SGL_THEME_DEFAULT]| `1` | 如果未选择特定主题，则应用的默认主题 ID。 |
| [CONFIG_SGL_OBJ_USE_NAME]| `0` | 如果启用，每个 GUI 对象存储一个字符串名称。便于调试和按名称查找对象，但会增加每个对象的 RAM 使用量。 |

---

## 启动序列

系统启动期间显示的视觉效果。

| 宏定义 | 默认值 | 描述 |
| :--- | :---: | :--- |
| [CONFIG_SGL_BOOT_LOGO]| `1` | 在初始化期间显示启动 Logo。 |

---
