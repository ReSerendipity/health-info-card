#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// v3: 新增 demo 标志——defaults() 生成的占位示例档案置位,经配置门户
// 保存真实档案后清除;UI 在 demo 置位时显示"示例数据"警示,避免假信息
// 在急救/走失场景误导救援。注意:version 变化会使旧版已配置档案因
// size 不匹配而失效(视为未配置);占位档案无此问题。
#define SAFETY_PROFILE_VERSION 3u

// Keep this layout compatible with the original Senior Safety Card release so
// upgrading the application does not discard an already configured profile.
typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    uint32_t checksum;
    uint8_t configured;
    uint8_t show_full_address;
    uint8_t show_full_phone;
    uint8_t reserved;
    uint8_t demo;          // v3: 1=内置占位示例档案(非真实数据)
    char name[40];
    char help_text[120];
    char home_area[72];
    char home_address[152];
    char contact_name[40];
    char relation[28];
    char phone[32];
    char backup_phone[32];
    char medical[224];
    char wechat_note[80];
    uint8_t pin_salt[16];
    uint8_t pin_hash[32];
    char age[12];        // v2: 年龄(自由文本,如 "68")
    char blood_type[12]; // v2: 血型(如 "A型")
} safety_profile_t;

void safety_profile_defaults(safety_profile_t *profile);
void safety_profile_seal(safety_profile_t *profile);
bool safety_profile_is_valid(const safety_profile_t *profile);
bool safety_profile_has_pin(const safety_profile_t *profile);
void safety_profile_mask_number(const char *src, bool show_full,
                                char *output, size_t capacity);
bool safety_profile_valid_phone(const char *text);
bool safety_profile_valid_age(const char *text);
bool safety_profile_valid_blood_type(const char *text);
void safety_profile_mask_phone(const safety_profile_t *profile,
                               char *output, size_t capacity);
