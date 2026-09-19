// main/fap_screenshot.h -- FAP_SCREENSHOT_V1 串口截屏协议 + FAP_KEY_V1 按键注入。
//
// 实现对齐官方提交 6acfd9d(社区发布助手截图协议)的约定:
//   主机 -> 设备: ASCII 行 "FAP_SCREENSHOT_V1\n"(115200 波特;
//                 USB-Serial-JTAG 是原生 USB-CDC,波特率仅为名义值)。
//   设备 -> 主机: 头行 "FAP_SCREENSHOT_V1 <宽> <高> ARGB2222 <字节数>\n"
//                 紧跟 <字节数> 字节 ARGB2222 行主序像素(每像素 1 字节,
//                 2bit/通道低精度颜色;本固件无 PSRAM、堆仅百余 KB,
//                 RGB565 的 153,600 字节整屏放不下,ARGB2222 为 76,800)。
// 按键注入(FAP_KEY_V1 <btn> <ev>):仅供自动化真机验收,btn 0=上 1=下
// 2=确定,ev 1=单击 3=长按;注入走正常按键回调路径,UI 无感知。
// 命令严格只读/注入:不重启、不刷机、不改设置、不暴露任何设备凭据。
#pragma once

#include "bsp_button.h"   // bsp_btn_cb_t

// 启动串口应答任务(内部只创建一次)。在 UI 就绪后调用。
void fap_screenshot_start(void);

// 注册按键注入回调(通常为 app 的统一按键分发 on_key)。
// 注入的按键事件从串口进入后走此回调,与实体按键同路径。
void fap_screenshot_set_key_cb(bsp_btn_cb_t cb);
