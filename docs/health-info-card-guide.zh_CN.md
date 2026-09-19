<p align="right">
  <a href="health-info-card-guide.md">English</a> · <strong>简体中文</strong>
</p>

# 健康信息卡（HEALTH INFO CARD）· FoloToy AI Passport 交付说明

基于官方 `feat/senior-safety-card`（老人安全卡）分支改造，在 FoloToy AI Passport
（ESP32-C3 / 240×320 ST7789 / 无 PSRAM）上实现"事故/走失时他人可快速了解的健康信息卡"。

## 一、功能：5 页卡片（上下键翻页）

| 页 | 内容 | 说明 |
| --- | --- | --- |
| 1/5 | 姓名 · 年龄 · 血型 · 求助语 | 核心信息页：`李小明 / 68岁 / A型`，"您好，我可能迷路了，请帮我联系家人" |
| 2/5 | 联系家人 | 家属姓名/关系、主号码、备用号码（官方支持掩码显示） |
| 3/5 | 帮我回家 | 常住地址（江西省南昌市） |
| 4/5 | 健康提醒 | 过敏史、慢性病、日常用药（青霉素过敏；高血压，日常服药） |
| 5/5 | 微信联系 | 微信二维码（未上传时显示提示） |

- 长按确认键 = 重新进入配置门户（官方行为）。
- 60 秒无操作自动深睡（官方行为，省电）；按任意键唤醒。
- 数据当前为**占位示例**，正式使用前必须替换为真实信息（见第二节）。

## 二、数据占位与替换（必须）

以下档案当前固化在固件里（`main/safety_profile.c` 的 `safety_profile_defaults()`），
全部为示例，**请替换为真实信息后重新编译刷写**：

| 字段 | 当前占位 | 建议替换为 |
| --- | --- | --- |
| 姓名 | 李小明 | 真实姓名 |
| 年龄 | 68 | 真实年龄 |
| 血型 | A型 | 真实血型（A/B/AB/O ± RH） |
| 住址 | 江西省南昌市（红谷滩区示例路1号） | 真实常住地址 |
| 家属 | 李建国（儿子） | 真实家属及关系 |
| 家属电话 | 13800000000 | 真实号码 |
| 备用电话 | 13900000000 | 真实备用号码 |
| 医疗 | 青霉素过敏；高血压，日常服药 | 真实过敏史/慢病/用药（重要！急救场景） |

字段定义在 `main/safety_profile.h`（档案结构 version=2，含 `age[]`/`blood_type[]`）。
替换后请用第 3 节命令重新构建并刷写；刷写会清除旧 NVS，新固件直接使用新占位档案。

## 三、构建与刷写

```powershell
# 激活 ESP-IDF 5.5.3（IDF_PATH 必须用正斜杠，官方 recovery_boot_hook 的反斜杠会触发 CMake 转义报错）
& C:\Users\Doro\esp\esp-idf-v5.5.3\export.ps1
$env:IDF_PATH = "C:/Users/Doro/esp/esp-idf-v5.5.3"
$env:IDF_COMPONENT_STORAGE_URL = "https://components-file.espressif.cn"

# 编译（在仓库根目录）
idf.py build

# 合并整包并刷写（仓库强制只发整包 0x0；串口按实际调整，示例 COM3）
cd build
python -m esptool --chip esp32c3 merge_bin -o FoloToy-AI-Passport-full.bin "@..\build\flash_args"
python -m esptool --chip esp32c3 -p COM3 -b 460800 write_flash 0x0 FoloToy-AI-Passport-full.bin
```

Python 运行库：`C:\Users\Doro\.espressif\python_env\idf5.5_py3.14_env\Scripts\python.exe`

## 四、串口真机验收（截图 + 按键注入）

设备内置只读截屏/按键注入协议（FAP_SCREENSHOT_V1 / FAP_KEY_V1），无需手动按键即可自动验收：

```powershell
# 单张截图（L8 灰度，240×320 → 24 位 BMP）
python tools\fap_device.py -p COM3 shot _shot.bmp

# 按键注入（btn: 0=上 1=下 2=确定；ev: 1=单击 3=长按）
python tools\fap_device.py -p COM3 key 1 1

# 5 页全自动验收（截图→翻页→截图，输出到指定目录）
python tools\fap_verify.py COM3 _fap_shots
python tools\fap_verify2.py COM3 _fap_shots   # 备用独立脚本（不经 fap_device.py）
```

要点（踩坑记录）：
- **不要用 DTR/RTS 复位设备**：USB CDC 线路状态变化会让设备 USB 短暂重枚举，
  干扰截图协议（实测复位后截图全部超时）。唤醒设备用 esptool 刷写后的 hard reset，
  或按任意键；验收需在设备唤醒后的 60 秒窗口内完成（或临时把
  `main/main.c` 的 `NORMAL_IDLE_MS` 调大，验收后恢复 60s）。
- 设备侧回传使用**寄存器级直写 TX FIFO**（`usb_serial_jtag_ll_*`）：driver 的
  write_bytes 走 ring+ISR，在本固件（只装 driver 不装 VFS）下数据不落地。
- 快照格式为 **L8 灰度**（1B/px，76,800B）：SW 渲染器不支持 ARGB2222 目标
  （快照输出为噪点）；RGB565 需 153,600B 连续堆，本机空闲堆不足。
- 主机端读取要**直接读原始字节再定位头行**：逐行 `readline()` 在短超时下会漏读
  （此前脚本化截图全部超时的根因）。

## 五、真机验收结果（2026-09-19）

| 检查项 | 结果 |
| --- | --- |
| 启动稳定性 | 通过（日志干净，无 panic / 反复重启） |
| 页 1/5 姓名·年龄·血型 | 通过（李小明 / 68 / A型） |
| 页 2/5 联系家人 | 通过（李建国/儿子，主/备号码） |
| 页 3/5 帮我回家 | 通过（江西省南昌市） |
| 页 4/5 健康提醒 | 通过（青霉素过敏/高血压/日常服药） |
| 页 5/5 微信联系 | 通过（未上传二维码提示） |
| 按键注入翻页 | 通过（1→2→3→4→5 全链路） |
| 60s 深睡恢复 | 通过（恢复官方 60s 后重刷，窗口内 5 页验收成功） |

验收截图见 `_fap_shots/fap_page_0X_*.bmp`（5 张，本目录同批交付）。

## 六、主要改动文件（相对官方 `feat/senior-safety-card`）

| 文件 | 改动 |
| --- | --- |
| `main/safety_profile.h` | 档案 v2：新增 `age[12]`、`blood_type[12]` |
| `main/safety_profile.c` | defaults 填充占位档案并 seal（configured=1） |
| `main/safety_store.c` | 无 NVS 时直接返回占位档案（不强制进配置门户） |
| `main/ui_safety.c` | 第 0 页改为姓名+年龄+血型+求助语 |
| `main/main.c` | 接入 fap 协议；`NORMAL_IDLE_MS` 保持官方 60s |
| `main/fap_screenshot.{c,h}` | 新增：FAP_SCREENSHOT_V1（L8 快照，寄存器级 TX）+ FAP_KEY_V1 |
| `main/CMakeLists.txt` | 加入 `fap_screenshot.c` |
| `sdkconfig.defaults` | LV_USE_CLIB_MALLOC / LV_USE_SNAPSHOT / 主任务与定时器栈 8192 |
| `tools/fap_device.py` | 主机端 shot/key 工具（L8/ARGB2222/RGB565LE → BMP） |
| `tools/fap_verify.py` / `tools/fap_verify2.py` | 5 页自动验收脚本 |

> 说明：改动尚未 git 提交（按仓库约定，仅在用户要求时提交）；旧自研方案
> （`health-info-card` 分支工作区，stash "fap-work-before-official-branch"）已废弃可弃。
