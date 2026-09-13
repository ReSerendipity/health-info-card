<p align="right">
  <strong>简体中文</strong> · <a href="README.md">English</a>
</p>

# 健康信息卡

为 FoloToy AI Passport 设计的四页式 ICE（紧急情况下）医疗信息展示应用。当机主遇到事故、或他人捡到设备时，姓名、年龄、血型、家属电话、过敏史、慢性病等关键信息一键即达。

## 各页内容

| 页 | 标题 | 内容 |
| --- | --- | --- |
| 0 | 健康信息 | 姓名（大字）、血型徽章、年龄/性别、"遇事故请出示此卡"提醒 |
| 1 | 紧急联系 | 最多 3 位家属电话（关系 + 号码）、"请先联系家属"提醒 |
| 2 | 医疗信息 | 过敏史、慢性病、常服药物、备注 |
| 3 | 急救须知 | 按档案自动生成的施救提示：拨打 120、联系家属、过敏警示、捐献意愿 |

## 按键说明

- **上 / 下键** —— 切换页面（可循环）
- **确定（短按）** —— 下一页（方便单手翻页）
- **确定（长按）** —— 返回主菜单（由 `main.c` 统一拦截）

## 修改自己的信息

所有个人数据集中在一处：

- `main/health_profile.h` —— 字段说明与填写约定
- `main/health_profile.c` —— 实际数据 `g_health_profile`

把占位内容（姓名、电话、过敏史等）替换成真实信息后重新编译刷写即可。注意：

- 电话建议用 `138-0000-0000` 的 3-4-4 分组，小屏上更易读。
- 用 `""` 表示"不显示该栏目"，用 `"无"` 表示"没有此问题"。
- `blood_type` 保持简短（如 `A 型`、`AB 型 Rh-`），徽章宽度有限。
- 中文显示依赖子集字体。如果某字（如姓名中的生僻字）显示为空白，把它加进 `tools/health_font_chars.txt`，运行 `tools/gen_health_font.ps1` 重新生成字体后再编译。

## 构建与刷写

激活 ESP-IDF 5.5.3 后，在仓库根目录执行：

```bash
./tools/validate.sh --firmware
```

在偏移 `0x0` 刷写校验通过的整包镜像：

```bash
python -m esptool --chip esp32c3 -p <端口> -b 460800 \
    write-flash 0x0 build/FoloToy-AI-Passport-full.bin
```

开机进入菜单后选择 **Health** 即可。构建同时会运行档案数据主机测试（`tests/test_health_profile.c`）。

## 相关文件

| 文件 | 作用 |
| --- | --- |
| `main/demo_health.c` | 页面 UI、按键处理、电量显示 |
| `main/health_profile.h` / `.c` | 可编辑的个人数据 |
| `main/health_font_16.c` / `_20.c` | Noto Sans CJK SC 子集字体（生成产物） |
| `main/health_font.h` | 字体声明 |
| `tools/health_font_chars.txt` | 字体字符集（缺字时在这里补充） |
| `tools/gen_health_font.ps1` | 字体再生成脚本 |
| `tests/test_health_profile.c` | 档案数据主机侧校验 |

## 真机验收清单

- [ ] 概览页姓名、血型、年龄、性别显示完整无溢出
- [ ] 联系页每位家属电话显示正确
- [ ] 上 / 下 / 确定可翻页；确定长按返回菜单
- [ ] 各页右上角显示电量百分比
- [ ] 过敏史、慢性病等行均正常显示（无空白方块）
