// main/fap_screenshot.c -- FAP_SCREENSHOT_V1 串口截屏协议 + FAP_KEY_V1 按键注入。
//
// 实现对齐官方提交 6acfd9d(社区发布助手截图协议)的坑位结论:
//   1) 直接使用 usb_serial_jtag_* 驱动 API,不安装 VFS
//      (usb_serial_jtag_vfs_use_driver 会接管控制台/printf 路径,
//      实测在 main 任务引发 Stack protection fault -> 开机反复重启);
//   2) 不做启动期大块预分配:快照时刻用 lv_snapshot_take 动态分配,
//      堆不足时只记日志不应答,绝不影响系统启动;
//   3) 帧缓冲格式用 L8 灰度(1B/px):SW 渲染器不支持 ARGB2222 目标(实测快照全是噪点),
//       RGB565 需 153,600B 连续堆(空闲约 110KB 不够),L8 仅 76,800B,
//      无余量、空闲堆约 110KB,装不下 RGB565 的 153,600 字节;
//      布局/文字清晰可辨识(灰度);
//   4) 回传期间临时关闭日志输出,避免日志字节混入二进制流;
//   5) 只读/注入都保持静默失败:任何失败不应答,主机得到干净超时。
#include "fap_screenshot.h"
#include "bsp_display.h"       // bsp_lvgl_lock / bsp_lvgl_unlock
#include "bsp_button.h"        // bsp_btn_cb_t / bsp_btn_ev_t
#include "esp_log.h"
#include "driver/usb_serial_jtag.h"
#include "hal/usb_serial_jtag_ll.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "fap_shot";

#define FAP_CMD        "FAP_SCREENSHOT_V1"
#define FAP_CMD_LEN    (sizeof(FAP_CMD) - 1)
#define FAP_KEY_CMD    "FAP_KEY_V1"
#define FAP_KEY_LEN    (sizeof(FAP_KEY_CMD) - 1)
#define FAP_LINE_MAX   40    // 命令行缓冲:略大于最长命令+参数,溢出即丢弃重来
#define FAP_TASK_STACK 8192  // lv_snapshot 在本任务内做整屏软件渲染,栈要余量
#define FAP_TASK_PRIO  4     // 低于 UI/采集(官方值):截屏是偶发操作,不抢交互

static bsp_btn_cb_t s_key_cb;

// 寄存器级直写发送(与启动日志同一路径,已验证稳定):
// driver write_bytes 走 ring+ISR,本固件只装 driver 不装 VFS 时 TX 实际
// 不落地(实测设备侧打完"已回传"日志,主机侧收不到像素)。
// 这里直接轮询 TX FIFO 直写,绕开 driver TX 与 VFS 换行转换。
// 注意:64 字节整包会被 USB 视为未完成事务,结尾必须 flush 补 ZLP。
static void tx_raw(const void *data, size_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    size_t left = len;
    uint32_t guard = 0;
    while (left > 0) {
        if (usb_serial_jtag_ll_txfifo_writable()) {
            int n = usb_serial_jtag_ll_write_txfifo(p, (uint32_t)left);
            if (n > 0) { p += n; left -= (size_t)n; guard = 0; continue; }
        }
        if (++guard > 2000000) return;   // 主机断开等:约 2s 兜底,放弃本次
    }
    usb_serial_jtag_ll_txfifo_flush();    // 收尾:最后一包(可能 64 字节整)必须终结
}



// 渲染当前屏幕并以协议格式回传。快照失败(堆不足)只记日志不应答,
// 主机会以超时给出明确错误,不破坏流格式。
static void dump_screen(void) {
    lv_draw_buf_t *snap = NULL;
    if (bsp_lvgl_lock(2000)) {
        snap = lv_snapshot_take(lv_screen_active(), LV_COLOR_FORMAT_L8);
        bsp_lvgl_unlock();
    }
    if (!snap) {
        ESP_LOGE(TAG, "快照失败:堆不足或 LVGL 忙,空闲堆 %lu 字节",
                 (unsigned long)esp_get_free_heap_size());
        return;
    }
    // L8 行距无填充(240B/行),防御性核对,不符则宁可不应答。
    uint32_t len = (uint32_t)snap->data_size;
    if (len != (uint32_t)snap->header.w * snap->header.h * 1) {
        ESP_LOGE(TAG, "行距含填充(%lu),不符合 L8 紧排约定", (unsigned long)len);
        lv_draw_buf_destroy(snap);
        return;
    }

    char header[48];
    int n = snprintf(header, sizeof(header), "%s %d %d L8 %lu\n",
                     FAP_CMD, snap->header.w, snap->header.h, (unsigned long)len);
    int w = snap->header.w, h = snap->header.h;
    esp_log_level_set("*", ESP_LOG_NONE);  // 传输窗口内禁一切日志
    tx_raw(header, (size_t)n);
    tx_raw(snap->data, len);
    esp_log_level_set("*", ESP_LOG_INFO);  // 恢复 sdkconfig 默认日志级别
    lv_draw_buf_destroy(snap);
    ESP_LOGI(TAG, "已回传截屏 %dx%d(%lu 字节)", w, h, (unsigned long)len);
}

// 按键注入:FAP_KEY_V1 <btn> <ev>。回调运行在 fap 任务上下文,
// 与 bsp 按键任务回调一致,操作 LVGL 由回调自行加锁。
static void do_key(int btn, int ev) {
    if (s_key_cb == NULL) return;
    if (btn < (int)BSP_BTN_UP || btn > (int)BSP_BTN_OK) return;
    bsp_btn_ev_t e;
    switch (ev) {
    case 1: e = BSP_BTN_CLICK; break;       // 单击
    case 3: e = BSP_BTN_LONG;  break;       // 长按
    default: return;
    }
    s_key_cb((bsp_btn_t)btn, e, NULL);
}

// 串口应答任务:攒行匹配命令,其余输入(日志回显、换行等)一律忽略。
static void fap_task(void *arg) {
    (void)arg;
    // 普通固件(不启动 REPL)默认没有 USB-Serial-JTAG 驱动对象:日志走 VFS
    // 的 no_driver 直写寄存器路径,而 read_bytes/write_bytes 依赖
    // driver_install 创建的对象,不装会 Load access fault(实测崩溃)。
    // 这里只装 driver、不动 VFS/printf 路径(避免 main 任务 printf 爆栈)。
    // 截图窗口内 esp_log_level_set 全局静默日志,no_driver TX 停写,
    // 与 driver 的 TX FIFO 写入在时间上不重叠,无外设竞争。
    usb_serial_jtag_driver_config_t cfg = {
        .tx_buffer_size = 1024,
        .rx_buffer_size = 512,
    };
    if (usb_serial_jtag_driver_install(&cfg) != ESP_OK) {
        ESP_LOGE(TAG, "USB-Serial-JTAG 驱动安装失败,截图/按键注入不可用");
        vTaskDelete(NULL);
        return;
    }

    char line[FAP_LINE_MAX];
    size_t used = 0;
    uint8_t buf[16];
    for (;;) {
        int n = usb_serial_jtag_read_bytes(buf, sizeof(buf), pdMS_TO_TICKS(500));
        if (n <= 0) continue;  // 无输入:超时后继续等,任务常驻不退出
        for (int i = 0; i < n; i++) {
            char c = (char)buf[i];
            if (c == '\r') continue;
            if (c != '\n') {
                if (used < sizeof(line) - 1) {
                    line[used++] = c;
                } else {
                    used = 0;  // 超长行:丢弃,防止半截内容被误判成命令
                }
                continue;
            }
            line[used] = '\0';
            if (used == FAP_CMD_LEN && memcmp(line, FAP_CMD, FAP_CMD_LEN) == 0) {
                dump_screen();
            } else if (used > FAP_KEY_LEN &&
                       memcmp(line, FAP_KEY_CMD, FAP_KEY_LEN) == 0) {
                int btn = -1, ev = -1;
                if (sscanf(line + FAP_KEY_LEN, "%d %d", &btn, &ev) == 2) {
                    do_key(btn, ev);
                }
            }
            used = 0;
        }
    }
}

void fap_screenshot_start(void) {
    xTaskCreate(fap_task, "fap_shot", FAP_TASK_STACK, NULL, FAP_TASK_PRIO, NULL);
}

void fap_screenshot_set_key_cb(bsp_btn_cb_t cb) {
    s_key_cb = cb;
}
