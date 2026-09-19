# health-display —— 老人健康信息展示（我们的代码）

本目录是**我们自研**的部分，与从 GitHub 克隆的 FoloToy AI-Passport 框架/工具**完全分离**，
位于工程根目录 `ESP32-C3/health-display/`，不属于 `main/`，也不在 `components/bsp` 等框架目录里。

## 包含内容

| 文件 | 作用 |
|------|------|
| `demo_health.c` | 健康信息卡演示页（概览 / 紧急联系 / 医疗信息 / 急救须知），由 `main.c` 的菜单进入 |
| `health_profile.c` / `health_profile.h` | 健康档案数据（姓名、血型、家属电话、病史等）。**改这里填真实信息** |
| `health_font.h` / `health_font_16.c` / `health_font_20.c` | 健康卡专用的中文字体子集（由 `tools/gen_health_font.ps1` 生成，勿手改） |
| `ui_pixel.c` / `ui_pixel.h` / `ui_pixel_math.c` / `ui_pixel_math.h` | 共享像素 UI 库（菜单与各个 demo 都用） |
| `demo.h` | 演示页统一接口（被 `main/` 与各 demo 共用，随本组件一起提供） |
| `tools/` | 字体生成脚本 `gen_health_font.ps1` 与字符集 `health_font_chars.txt` |
| `tests/` | 单元测试 `test_health_profile.c`、`test_ui_pixel_math.c` |
| `docs/` | 健康信息卡参考文档 |

## 如何接入工程

作为 ESP-IDF **外部组件**接入，不污染 `main/`：

- 根目录 `CMakeLists.txt` 通过
  `set(EXTRA_COMPONENT_DIRS "${CMAKE_SOURCE_DIR}/health-display")` 把本目录加入组件搜索路径；
- `main/CMakeLists.txt` 的 `REQUIRES` 里加了 `health_display`，因此 `main` 能调用本组件的
  `demo_health_enter/exit/key`、`ui_pixel_*`、`health_profile` 等接口。

依赖方向是单向的（`main` → `health_display`），无循环依赖。

## 编译 / 验证

```bash
idf.py reconfigure      # 让 EXTRA_COMPONENT_DIRS 生效
idf.py build
```

> 注意：重构后 `build/` 是旧缓存（263M，已被 git 忽略），建议先 `idf.py reconfigure`
> 或删除 `build/` 再重新编译。

## 修改健康信息

编辑 `health_profile.c` 里的 `g_health_profile`（示例占位数据），重新编译刷写即可。
若姓名/病史包含字体里没有的汉字导致显示空白，把缺的字加进
`tools/health_font_chars.txt` 后重跑 `gen_health_font.ps1` 重新生成字体。
