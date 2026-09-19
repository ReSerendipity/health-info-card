<p align="right">
  <a href="esp32c3-usb-serial-jtag-pitfalls.md">English</a> · <strong>简体中文</strong>
</p>

# ESP32-C3 USB-Serial-JTAG：串口截屏与按键注入踩坑记录

在 AI Passport（ESP32-C3、无 PSRAM、USB-Serial-JTAG 控制台）上添加只读串口截屏
协议（`FAP_SCREENSHOT_V1`）与按键注入（`FAP_KEY_V1`）过程中实测沉淀的坑。
每条格式：现象 → 根因 → 正确做法。凡涉及 FAP 协议、控制台驱动、LVGL 快照或
主机端串口采集脚本，先读本文。

参考实现：官方提交
`6acfd9db3b9e7b937571827bb78c40a5d414bf5c`（"feat: add read-only
FAP_SCREENSHOT_V1 serial screenshot protocol"）；本分支接线见
`main/fap_screenshot.{c,h}` 与 `tools/fap_device.py`、`tools/fap_verify*.py`。

## 1. DTR/RTS 复位会破坏协议；深睡不响应 DTR

- **现象**：主机先通过 DTR 复位设备再截图，所有截图全部超时（"no screenshot
  header"），而不做复位的纯手动抓包却能成功。
- **根因**：USB-Serial-JTAG 上 CDC 线路状态变化会让 USB 端点短暂重枚举，
  干扰运行中固件的驱动 RX；此外深睡状态下只有 GPIO 唤醒源生效——DTR/RTS
  无法唤醒设备（已实测单独拉高 DTR 无效）。
- **正确做法**：
  - 协议交互期间绝不拨动 DTR/RTS。串口用
    `serial.Serial(..., dsrdtr=False, rtscts=False)` 并保持低电平。
  - 需要从主机侧唤醒/重启设备时，用 `esptool` 的 `--after hard_reset`
    （走 ROM 下载/启动时序），或按任意键；重新刷写也是一种可靠的硬复位。
  - 主机抓包须在唤醒窗口内完成（默认 60 秒无操作深睡）。长时间自动验收时，
    可临时把 `main/main.c` 的 `NORMAL_IDLE_MS` 调大，验收后恢复 60 秒并重刷。

## 2. 不装 VFS 时 `usb_serial_jtag_write_bytes` 的 TX 不会真正发出

- **现象**：设备侧打出"已回传截屏"（自认为发送完成），但主机只收到头行与
  日志字节，几乎收不到像素。
- **根因**：只调用 `usb_serial_jtag_driver_install()`（未调用
  `usb_serial_jtag_vfs_use_driver()`）时，驱动 `write_bytes` 走 TX ring/ISR
  路径，在该配置下实际没有排到 FIFO——写入调用"本地成功"，数据却到不了主机。
- **正确做法**：用寄存器级轮询发送：`usb_serial_jtag_ll_txfifo_writable()` +
  `usb_serial_jtag_ll_write_txfifo(p, n)`，结尾补
  `usb_serial_jtag_ll_txfifo_flush()`。这与启动日志同一发送路径，已验证稳定。
  RX 可继续用驱动 `usb_serial_jtag_read_bytes`（工作正常）。

## 3. SW 渲染器不支持 ARGB2222 快照目标

- **现象**：`lv_snapshot_take(..., LV_COLOR_FORMAT_ARGB2222)` 返回的尺寸合理
  （76,800 字节），但解码后图像全是噪点。
- **根因**：软件渲染器支持的**渲染目标**格式为：`RGB565`、`RGB888`、
  `XRGB8888`、`ARGB8888`、`L8`。`ARGB2222` 不是合法快照目标，输出为垃圾数据。
- **正确做法**：本板无 PSRAM、启动时空闲堆约 113 KB，`RGB565` 需 153,600
  字节连续内存，放不下。改用 **`L8` 灰度**（76,800 字节、1 字节/像素、
  文字清晰）。主机端按灰度 BMP 解码。协议头自带格式名，主机端按其分支即可。

## 4. 主机 `readline()` 读不到协议头

- **现象**：脚本化抓包持续超时，而手动"读原始字节"抓包成功。
- **根因**：设备把头行与像素块紧连输出；`readline()` 在短超时下可能截断或
  漏掉边界，导致始终看不到头行。
- **正确做法**：直接读原始字节（循环 `ser.read(65536)` 直到缓冲里出现头标记），
  用 `buf.find(b"FAP_SCREENSHOT_V1")` 定位，从头行后第一个 `\n` 起取像素。
  发送前不要 `reset_input_buffer()`。

## 5. `IDF_PATH` 用反斜杠会编译报错（CMake 转义）

- **现象**：`idf.py build` 在
  `bootloader_components/recovery_boot_hook/CMakeLists.txt` 报
  `Invalid character escape '\U'`。
- **根因**：该 CMake 文件插值 `$ENV{IDF_PATH}`；Windows 反斜杠路径在 CMake
  字符串里成为非法转义。
- **正确做法**：构建前把 `IDF_PATH` 导出为正斜杠，
  如 `$env:IDF_PATH = "C:/Users/Doro/esp/esp-idf-v5.5.3"`。

## 6. 不要装控制台 VFS + 启动期大块分配 + printf（会崩溃）

- **现象**：早期自研 FAP 用了 `usb_serial_jtag_vfs_use_driver()` + 启动期大块
  帧缓冲分配 + main 任务内 `printf`，设备循环崩溃：main 任务 Stack
  protection fault，地址均在 `_vfprintf_r`，表现为开机闪屏反复重启。
- **根因**：把控制台切到驱动路径会显著抬升 main 任务里 `printf` 的栈耗，
  叠加启动期大块堆分配，栈/堆预算超限。
- **正确做法**：保持默认（no-driver）控制台；只装 USB-Serial-JTAG 驱动用于
  RX；快照工作放在独立任务（自带 8 KB 栈）；绝不在启动期分配整帧缓冲；
  失败路径保持静默（只记日志不应答），让主机得到干净超时而不是被污染的流。

## 7. 64 字节整包的最后一包需要 ZLP 终结

- **现象**：76,800 = 64 × 1200 恰好整除；不做终结时最后 64 字节可能到不了
  主机。
- **根因**：64 字节整包（FIFO 写满）会被主机 CDC-ACM 端口视为未完成的 USB
  事务。
- **正确做法**：最后一字节之后调用 `usb_serial_jtag_ll_txfifo_flush()`；
  待 FIFO 再次可写时再 flush 一次以发出零长包（见 HAL 头文件注释）。

## 验收速查

- 构建：激活 ESP-IDF，`IDF_PATH` 用正斜杠，`idf.py build`。
- 刷写（仓库强制整包写 `0x0`）：
  `python -m esptool --chip esp32c3 merge_bin -o FoloToy-AI-Passport-full.bin "@build\flash_args"`
  再 `python -m esptool --chip esp32c3 -p COM3 -b 460800 write_flash 0x0 FoloToy-AI-Passport-full.bin`。
  `--after hard_reset` 会重启设备；在唤醒窗口内抓包。
- 单张抓图：`python tools/fap_device.py -p COM3 shot out.bmp`。
- 5 页全自动验收：`python tools/fap_verify.py COM3 <输出目录>`
  （或自包含的 `tools/fap_verify2.py`）。
- 协议摘要：头行 `FAP_SCREENSHOT_V1 240 320 <FMT> <N>\n` 后接 N 字节
  （L8 / ARGB2222 为 1 字节/像素，RGB565LE 为 2 字节/像素，行主序从上到下）；
  按键行 `FAP_KEY_V1 <btn> <ev>`（btn 0=上 1=下 2=确定；ev 1=单击 3=长按）。
