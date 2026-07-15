# Arc 圆弧控件 API 教程

`arc` 控件用于绘制圆弧、圆环以及带背景轨道的环形进度。控件支持设置内外半径、起止角度、颜色、透明度和端点模式，也支持通过按下或拖动直接改变结束角度。

## 1. 引入头文件

使用 SGL 聚合头文件：

```c
#include "sgl.h"
```

也可以只引入 arc 控件：

```c
#include "widgets/arc/sgl_arc.h"
```

## 2. 创建一个圆弧

下面的示例创建一个半径为 50 像素、宽度为 8 像素的蓝色圆弧。圆弧从 0° 开始，顺时针绘制到 270°。

```c
sgl_obj_t *arc = sgl_arc_create(parent);
if (arc == NULL) {
    return;
}

/* 控件大小通常设置为外径，圆心位于控件中心。 */
sgl_obj_set_size(arc, 100, 100);
sgl_obj_set_pos(arc, 20, 20);

/* 内半径 42，外半径 50，因此圆弧宽度为 8。 */
sgl_arc_set_radius(arc, 42, 50);
sgl_arc_set_color(arc, SGL_COLOR_BLUE);
sgl_arc_set_start_angle(arc, 0);
sgl_arc_set_end_angle(arc, 270);
```

如果需要让控件在父对象中居中，应先设置尺寸，再调用对齐函数：

```c
sgl_obj_set_size(arc, 100, 100);
sgl_obj_set_pos_align(arc, SGL_ALIGN_CENTER);
```

## 3. 坐标与角度规则

- `0°` 位于圆的正上方。
- 角度沿顺时针方向增大：`90°` 在右侧，`180°` 在下方，`270°` 在左侧。
- 角度有效范围为 `0~360`。
- 起始角大于结束角时会跨越 `0°`。例如 `315°~45°` 表示一段 90° 的圆弧。
- `start_angle == end_angle` 时不绘制圆弧。
- `0°~360°` 会绘制完整圆环。

## 4. 绘制环形进度条

`SGL_ARC_MODE_RING` 会用背景色绘制未完成部分，适合制作环形进度条。

```c
static void arc_set_progress(sgl_obj_t *arc, uint8_t percent)
{
    if (percent > 100) {
        percent = 100;
    }

    sgl_arc_set_start_angle(arc, 0);
    sgl_arc_set_end_angle(arc, (int16_t)(percent * 360 / 100));
}

sgl_obj_t *progress = sgl_arc_create(parent);
if (progress == NULL) {
    return;
}

sgl_obj_set_size(progress, 120, 120);
sgl_obj_set_pos(progress, 10, 10);
sgl_arc_set_radius(progress, 50, 60);
sgl_arc_set_mode(progress, SGL_ARC_MODE_RING);
sgl_arc_set_color(progress, SGL_COLOR_GREEN);
sgl_arc_set_bg_color(progress, SGL_COLOR_DARK_GRAY);
arc_set_progress(progress, 75);
```

更新结束角度后，控件会自动标记为需要重绘，不需要手动调用 `sgl_obj_set_dirty()`。

## 5. 圆弧模式

通过 `sgl_arc_set_mode()` 设置绘制模式：

| 模式 | 说明 |
| --- | --- |
| `SGL_ARC_MODE_NORMAL` | 只绘制指定角度范围内的圆弧，其余部分保留原背景。 |
| `SGL_ARC_MODE_RING` | 绘制完整圆环，指定角度范围使用前景色，其余部分使用背景色。 |
| `SGL_ARC_MODE_NORMAL_SMOOTH` | 只绘制圆弧，并对圆弧端点进行平滑处理。 |
| `SGL_ARC_MODE_RING_SMOOTH` | 带背景圆环，并对前景圆弧端点进行平滑处理。 |

平滑模式通常更适合较粗的圆弧或进度条。

```c
sgl_arc_set_mode(arc, SGL_ARC_MODE_RING_SMOOTH);
```

## 6. 透明度

透明度范围为 `0~255`：

- `0`：完全透明。
- `255`：完全不透明。
- 也可以使用 `SGL_ALPHA_PRCNT()` 按百分比设置。

```c
sgl_arc_set_alpha(arc, SGL_ALPHA_PRCNT(60));
```

## 7. 交互行为

arc 控件创建后默认具有点击和移动属性。收到按下或移动事件时，控件会根据指针相对圆心的位置自动更新结束角度，因此可以直接用作可拖动的圆形调节器。

如果业务只需要静态显示，可根据项目的对象属性 API 关闭控件的点击或移动能力，或者确保事件不会被派发给该对象。

## 8. API 参考

### `sgl_arc_create`

```c
sgl_obj_t *sgl_arc_create(sgl_obj_t *parent);
```

创建 arc 控件并挂载到 `parent`。创建失败时返回 `NULL`。

默认值：

- 起始角度：`0°`
- 结束角度：`360°`
- 模式：`SGL_ARC_MODE_NORMAL`
- 透明度：`SGL_THEME_ALPHA`
- 内外半径：首次绘制时根据控件尺寸自动计算

### `sgl_arc_set_color`

```c
void sgl_arc_set_color(sgl_obj_t *obj, sgl_color_t color);
```

设置圆弧前景色。

### `sgl_arc_set_bg_color`

```c
void sgl_arc_set_bg_color(sgl_obj_t *obj, sgl_color_t color);
```

设置圆环背景色，主要用于 `RING` 和 `RING_SMOOTH` 模式。

### `sgl_arc_set_alpha`

```c
void sgl_arc_set_alpha(sgl_obj_t *obj, uint8_t alpha);
```

设置控件整体透明度，范围为 `0~255`。

### `sgl_arc_set_radius`

```c
void sgl_arc_set_radius(sgl_obj_t *obj,
                        int16_t radius_in,
                        int16_t radius_out);
```

设置内半径和外半径。圆弧宽度为：

```text
radius_out - radius_in
```

应保证 `0 <= radius_in < radius_out`，并为控件预留至少 `2 * radius_out` 的宽度和高度。

### `sgl_arc_set_mode`

```c
void sgl_arc_set_mode(sgl_obj_t *obj, uint8_t mode);
```

设置圆弧绘制模式。应传入 `SGL_ARC_MODE_*` 宏之一。

### `sgl_arc_set_start_angle`

```c
void sgl_arc_set_start_angle(sgl_obj_t *obj, int16_t angle);
```

设置起始角度，建议范围为 `0~360`。

### `sgl_arc_set_end_angle`

```c
void sgl_arc_set_end_angle(sgl_obj_t *obj, int16_t angle);
```

设置结束角度，建议范围为 `0~360`。

## 9. 使用注意事项

1. 当前 setter 不会自动限制角度和半径参数，调用方应自行保证参数有效。
2. 控件圆心由对象区域中心计算，建议使用宽高相同的正方形区域。
3. 未显式设置半径时，外半径默认为控件宽度的一半，内半径默认为外半径减 2 像素。
4. 调用 `sgl_arc_set_radius()` 可能同步缩放对象区域；通常应先设置对象尺寸和位置，再设置半径。
5. `0°~360°` 的完整圆环走专用绘制路径，此时不会显示 `RING` 模式的背景分段。
