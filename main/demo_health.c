// main/demo_health.c —— 健康信息卡（紧急医疗信息展示）
//
// 功能：在 AI Passport 上展示姓名、年龄、血型、家属电话、过敏史/慢性病等
//       遇到事故时他人需要的信息。数据见 health_profile.h / health_profile.c。
//
// 按键：
//   上键 短按   上一页
//   下键 短按   下一页
//   确定 短按   下一页（单手翻页更方便）
//   确定 长按   返回菜单（由 main.c 统一拦截）
//
// 页面：
//   0 概览    姓名 / 血型 / 年龄 / 性别
//   1 紧急联系  家属电话（最多 3 位）
//   2 医疗信息  过敏史 / 慢性病 / 常服药物 / 备注
//   3 急救须知  给施救者/医护人员的提示
#include "demo.h"
#include "health_profile.h"
#include "health_font.h"
#include "ui_pixel.h"
#include "bsp_battery.h"
#include "lvgl.h"
#include <string.h>

#define PAGE_COUNT 4

static lv_obj_t *s_screens[PAGE_COUNT]; // 每页一个屏,进入时一次建好
static lv_obj_t *s_battery[PAGE_COUNT]; // 各页右上角电量
static lv_timer_t *s_timer;             // 电量刷新定时器
static int s_page = 0;                  // 当前页

// ---------------------------------------------------------------- 工具函数

// 在 panel 里放一行 "标签(色块) + 值":返回值 label,便于后续设色。
static lv_obj_t *health_row(lv_obj_t *parent, const char *label, int chip_w,
                            const char *value, int x, int y, int value_w)
{
    lv_obj_t *chip = ui_pixel_panel_create(parent, x, y, chip_w, 26, UI_MUTED);
    lv_obj_t *chip_lbl = lv_label_create(chip);
    lv_obj_set_style_text_font(chip_lbl, &lv_font_health_16, 0);
    lv_obj_set_style_text_color(chip_lbl, lv_color_hex(UI_INK), 0);
    lv_label_set_text(chip_lbl, label);
    lv_obj_center(chip_lbl);

    lv_obj_t *val = lv_label_create(parent);
    lv_obj_set_style_text_font(val, &lv_font_health_16, 0);
    lv_obj_set_style_text_color(val, lv_color_hex(UI_INK), 0);
    lv_label_set_long_mode(val, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(val, value_w);
    lv_obj_set_pos(val, x + chip_w + 8, y + 4);
    lv_label_set_text(val, value);
    return val;
}

// 页面底部草地区:翻页提示 + 页码。每个屏一个,切换时更新。
static void health_footer(lv_obj_t *scr, int page)
{
    lv_obj_t *hint = lv_label_create(scr);
    lv_obj_set_style_text_font(hint, &lv_font_health_16, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_text(hint, "上下键翻页");
    lv_obj_set_pos(hint, 12, 292);

    lv_obj_t *ind = lv_label_create(scr);
    lv_obj_set_style_text_font(ind, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(ind, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_text_fmt(ind, "%d/%d", page + 1, PAGE_COUNT);
    lv_obj_set_pos(ind, 196, 293);
}

// 每页右上角电量(天空空白处,不压云朵)。读不到时显示 "--%"。
static void health_battery(lv_obj_t *scr, lv_obj_t **slot)
{
    lv_obj_t *b = lv_label_create(scr);
    lv_obj_set_style_text_font(b, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(b, lv_color_hex(UI_INK), 0);
    lv_obj_set_pos(b, 198, 32);
    *slot = b;
}

static void health_refresh_battery(lv_timer_t *t)
{
    (void)t;
    int soc = bsp_battery_soc();
    for (int i = 0; i < PAGE_COUNT; i++) {
        if (!s_battery[i]) continue;
        if (soc < 0) lv_label_set_text(s_battery[i], "--%");
        else         lv_label_set_text_fmt(s_battery[i], "%d%%", soc);
    }
}

// ------------------------------------------------------------ 各页构建

// 页 0 概览:姓名 / 血型 / 年龄 / 性别,一眼可读。
static lv_obj_t *build_overview(void)
{
    const health_profile_t *p = &g_health_profile;
    lv_obj_t *scr = ui_pixel_screen_create("健康信息");
    lv_obj_t *panel = ui_pixel_panel_create(scr, 14, 52, 212, 176, UI_PAPER);

    // 姓名(大字)
    lv_obj_t *name = lv_label_create(panel);
    lv_obj_set_style_text_font(name, &lv_font_health_20, 0);
    lv_obj_set_style_text_color(name, lv_color_hex(UI_INK), 0);
    lv_label_set_text(name, p->name);
    lv_obj_align(name, LV_ALIGN_TOP_MID, 0, 8);

    // 血型徽章
    lv_obj_t *badge = ui_pixel_panel_create(panel, 20, 60, 150, 36, UI_YELLOW);
    lv_obj_t *blood = lv_label_create(badge);
    lv_obj_set_style_text_font(blood, &lv_font_health_20, 0);
    lv_obj_set_style_text_color(blood, lv_color_hex(UI_INK), 0);
    lv_label_set_text_fmt(blood, "血型 %s", p->blood_type);
    lv_obj_center(blood);

    // 年龄 / 性别
    lv_obj_t *info = lv_label_create(panel);
    lv_obj_set_style_text_font(info, &lv_font_health_16, 0);
    lv_obj_set_style_text_color(info, lv_color_hex(UI_INK), 0);
    lv_label_set_text_fmt(info, "%s  /  %s 岁", p->gender, p->age);
    lv_obj_align(info, LV_ALIGN_TOP_MID, 0, 102);

    // 提示
    lv_obj_t *note = lv_label_create(panel);
    lv_obj_set_style_text_font(note, &lv_font_health_16, 0);
    lv_obj_set_style_text_color(note, lv_color_hex(UI_RED), 0);
    lv_label_set_text(note, "遇事故请出示此卡");
    lv_obj_set_pos(note, 31, 130);

    ui_pixel_mascot_create(scr, 101, 238);
    return scr;
}

// 页 1 紧急联系:家属电话。
static lv_obj_t *build_contacts(void)
{
    const health_profile_t *p = &g_health_profile;
    lv_obj_t *scr = ui_pixel_screen_create("紧急联系");
    lv_obj_t *panel = ui_pixel_panel_create(scr, 14, 52, 212, 176, UI_PAPER);

    lv_obj_t *head = lv_label_create(panel);
    lv_obj_set_style_text_font(head, &lv_font_health_16, 0);
    lv_obj_set_style_text_color(head, lv_color_hex(UI_INK), 0);
    lv_label_set_text(head, "家属电话");
    lv_obj_set_pos(head, 10, 6);

    int y = 36;
    for (int i = 0; i < 3; i++) {
        if (p->contacts[i].relation[0] == '\0' &&
            p->contacts[i].phone[0] == '\0') continue;
        lv_obj_t *chip = ui_pixel_panel_create(panel, 12, y, 48, 26, UI_MUTED);
        lv_obj_t *rel = lv_label_create(chip);
        lv_obj_set_style_text_font(rel, &lv_font_health_16, 0);
        lv_obj_set_style_text_color(rel, lv_color_hex(UI_INK), 0);
        lv_label_set_text(rel, p->contacts[i].relation);
        lv_obj_center(rel);

        lv_obj_t *phone = lv_label_create(panel);
        lv_obj_set_style_text_font(phone, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(phone, lv_color_hex(UI_INK), 0);
        lv_label_set_text(phone, p->contacts[i].phone);
        lv_obj_set_pos(phone, 66, y + 5);
        y += 32;
    }

    lv_obj_t *note = lv_label_create(panel);
    lv_obj_set_style_text_font(note, &lv_font_health_16, 0);
    lv_obj_set_style_text_color(note, lv_color_hex(UI_RED), 0);
    lv_label_set_text(note, "遇事故请先联系家属");
    lv_obj_set_pos(note, 10, 130);

    return scr;
}

// 页 2 医疗信息:过敏史 / 慢性病 / 常服药物 / 备注。
static lv_obj_t *build_medical(void)
{
    const health_profile_t *p = &g_health_profile;
    lv_obj_t *scr = ui_pixel_screen_create("医疗信息");
    lv_obj_t *panel = ui_pixel_panel_create(scr, 14, 52, 212, 224, UI_PAPER);

    lv_obj_t *a = health_row(panel, "过敏史", 70, p->allergies, 12, 12, 96);
    lv_obj_t *c = health_row(panel, "慢性病", 70, p->conditions, 12, 56, 96);
    lv_obj_t *m = health_row(panel, "常服药物", 84, p->medications, 12, 100, 82);
    lv_obj_t *n = health_row(panel, "备注", 70, p->notes, 12, 144, 96);

    // 有过敏史时标红提醒
    if (p->allergies[0] != '\0' && strcmp(p->allergies, "无") != 0)
        lv_obj_set_style_text_color(a, lv_color_hex(UI_RED), 0);
    (void)c; (void)m; (void)n;
    return scr;
}

// 页 3 急救须知:给施救者/医护人员的行动提示,按档案自动生成。
static lv_obj_t *build_guide(void)
{
    const health_profile_t *p = &g_health_profile;
    lv_obj_t *scr = ui_pixel_screen_create("急救须知");
    lv_obj_t *panel = ui_pixel_panel_create(scr, 14, 52, 212, 224, UI_PAPER);

    // 一行提示文本(超宽自动换行),返回用于后续变色的 label。
    int y = 8;
    const int line_h = 24;
#define GUIDE_LINE(var, color)                                             \
    lv_obj_t *var = lv_label_create(panel);                                \
    lv_obj_set_style_text_font(var, &lv_font_health_16, 0);                \
    lv_obj_set_style_text_color(var, lv_color_hex(color), 0);              \
    lv_label_set_long_mode(var, LV_LABEL_LONG_WRAP);                       \
    lv_obj_set_width(var, 176);                                            \
    lv_obj_set_pos(var, 14, y); y += line_h

    GUIDE_LINE(l1, UI_INK); lv_label_set_text(l1, "- 遇事故请拨打 120");
    GUIDE_LINE(l2, UI_INK); lv_label_set_text(l2, "- 并第一时间联系家属");

    if (p->allergies[0] != '\0' && strcmp(p->allergies, "无") != 0) {
        GUIDE_LINE(la, UI_RED); lv_label_set_text_fmt(la, "- 过敏: %s 勿使用", p->allergies);
    }
    if (p->conditions[0] != '\0' && strcmp(p->conditions, "无") != 0) {
        GUIDE_LINE(lc, UI_INK); lv_label_set_text_fmt(lc, "- 慢性病: %s", p->conditions);
    }
    if (p->medications[0] != '\0' && strcmp(p->medications, "无") != 0) {
        GUIDE_LINE(lm, UI_INK); lv_label_set_text_fmt(lm, "- 常服药物: %s", p->medications);
    }
    if (p->notes[0] != '\0' && strcmp(p->notes, "无") != 0) {
        GUIDE_LINE(ln, UI_INK); lv_label_set_text_fmt(ln, "- 备注: %s", p->notes);
    }

    GUIDE_LINE(ld, UI_INK); lv_label_set_text_fmt(ld, "- 器官捐献: %s", p->donor);
#undef GUIDE_LINE
    return scr;
}

// ------------------------------------------------------------ demo 接口

static void health_load(int page)
{
    s_page = page;
    lv_screen_load(s_screens[page]);
}

void demo_health_enter(void)
{
    s_screens[0] = build_overview();
    s_screens[1] = build_contacts();
    s_screens[2] = build_medical();
    s_screens[3] = build_guide();

    for (int i = 0; i < PAGE_COUNT; i++) {
        health_battery(s_screens[i], &s_battery[i]);
        health_footer(s_screens[i], i);
    }
    health_refresh_battery(NULL);
    s_timer = lv_timer_create(health_refresh_battery, 3000, NULL);

    health_load(0);
}

void demo_health_exit(void)
{
    if (s_timer) { lv_timer_delete(s_timer); s_timer = NULL; }
    for (int i = 0; i < PAGE_COUNT; i++) {
        if (s_screens[i]) { lv_obj_delete(s_screens[i]); s_screens[i] = NULL; }
        s_battery[i] = NULL;
    }
    s_page = 0;
}

void demo_health_key(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    if (ev != BSP_BTN_CLICK && ev != BSP_BTN_DOUBLE) return;
    int next = s_page;
    if (btn == BSP_BTN_UP)   next = (s_page + PAGE_COUNT - 1) % PAGE_COUNT;
    if (btn == BSP_BTN_DOWN || btn == BSP_BTN_OK) next = (s_page + 1) % PAGE_COUNT;
    if (next != s_page) health_load(next);
}
