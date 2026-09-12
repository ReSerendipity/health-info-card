// main/health_font.h —— 健康信息卡子集中文字体声明。
// 字体文件由 tools/gen_health_font.ps1 从开源字体 Noto Sans CJK SC（OFL 许可）生成，
// 只包含健康信息卡用到的汉字/数字/标点，节省 Flash。请勿手改 health_font_16.c / _20.c。
#pragma once

#include "lvgl.h"

LV_FONT_DECLARE(lv_font_health_16);   // 正文 16px
LV_FONT_DECLARE(lv_font_health_20);   // 标题/姓名 20px
