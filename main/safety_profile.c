#include "safety_profile.h"

#include <stdlib.h>
#include <string.h>

#define SAFETY_MAGIC 0x31434653u /* SFC1 */

static uint32_t checksum_bytes(const uint8_t *data, size_t length)
{
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < length; ++i) {
        hash ^= data[i];
        hash *= 16777619u;
    }
    return hash;
}

static uint32_t profile_checksum(const safety_profile_t *profile)
{
    safety_profile_t copy = *profile;
    copy.checksum = 0;
    return checksum_bytes((const uint8_t *)&copy, sizeof(copy));
}

void safety_profile_defaults(safety_profile_t *profile)
{
    if (!profile) return;
    memset(profile, 0, sizeof(*profile));
    profile->magic = SAFETY_MAGIC;
    profile->version = SAFETY_PROFILE_VERSION;
    profile->size = sizeof(*profile);
    // 隐私默认:展示页电话一律掩码,配置门户可显式开启完整显示。
    profile->show_full_phone = 0;
    strncpy(profile->help_text, "您好，我可能迷路了，请帮我联系家人",
            sizeof(profile->help_text) - 1);
    strncpy(profile->wechat_note, "请添加我的家人，备注安心牌",
            sizeof(profile->wechat_note) - 1);

    // ---- 健康信息卡占位档案(示例数据) ----
    // 无 NVS 档案时开机直接展示以下占位内容;使用前请替换为真实信息:
    // 方式一: 长按确认键进入本地设置门户,用网页表单配置后保存到 NVS;
    // 方式二: 直接改本函数中的字段,重新编译刷写。
    // demo 标志置位:UI 会显示"示例数据"警示,保存真实档案后自动清除。
    profile->demo = 1;
    strncpy(profile->name, "李小明", sizeof(profile->name) - 1);
    strncpy(profile->age, "68", sizeof(profile->age) - 1);
    strncpy(profile->blood_type, "A型", sizeof(profile->blood_type) - 1);
    strncpy(profile->home_area, "江西省南昌市",
            sizeof(profile->home_area) - 1);
    strncpy(profile->home_address, "江西省南昌市红谷滩区示例路 1 号",
            sizeof(profile->home_address) - 1);
    strncpy(profile->contact_name, "李建国", sizeof(profile->contact_name) - 1);
    strncpy(profile->relation, "儿子", sizeof(profile->relation) - 1);
    strncpy(profile->phone, "13800000000", sizeof(profile->phone) - 1);
    strncpy(profile->backup_phone, "13900000000",
            sizeof(profile->backup_phone) - 1);
    strncpy(profile->medical,
            "青霉素过敏；高血压，日常服药。\n（示例占位，请替换为真实信息）",
            sizeof(profile->medical) - 1);
    // 标记为已配置,开机即可直接展示占位档案;demo 标志由 UI 用于警示
    safety_profile_seal(profile);
}

void safety_profile_seal(safety_profile_t *profile)
{
    if (!profile) return;
    profile->magic = SAFETY_MAGIC;
    profile->version = SAFETY_PROFILE_VERSION;
    profile->size = sizeof(*profile);
    profile->configured = 1;
    profile->checksum = profile_checksum(profile);
}

bool safety_profile_is_valid(const safety_profile_t *profile)
{
    if (!profile || profile->magic != SAFETY_MAGIC ||
        profile->version != SAFETY_PROFILE_VERSION ||
        profile->size != sizeof(*profile)) {
        return false;
    }
    return profile->checksum == profile_checksum(profile);
}

bool safety_profile_has_pin(const safety_profile_t *profile)
{
    if (!profile) return false;
    uint8_t value = 0;
    for (size_t i = 0; i < sizeof(profile->pin_hash); ++i) {
        value |= profile->pin_hash[i];
    }
    return value != 0;
}

void safety_profile_mask_number(const char *src, bool show_full,
                                char *output, size_t capacity)
{
    if (!output || capacity == 0) return;
    output[0] = '\0';
    if (!src) return;

    size_t length = strnlen(src, 32);
    if (show_full || length < 7) {
        strncpy(output, src, capacity - 1);
        output[capacity - 1] = '\0';
        return;
    }

    size_t used = 0;
    for (size_t i = 0; i < length && used + 1 < capacity; ++i) {
        bool hidden = i >= 3 && i + 4 < length;
        output[used++] = hidden ? '*' : src[i];
    }
    output[used] = '\0';
}

void safety_profile_mask_phone(const safety_profile_t *profile,
                               char *output, size_t capacity)
{
    if (!profile) {
        if (output && capacity) output[0] = '\0';
        return;
    }
    safety_profile_mask_number(profile->phone, profile->show_full_phone,
                               output, capacity);
}

// 门户表单字段的内容校验(纯逻辑,可 host 测试):
// 电话仅允许数字与 + - ( ) 空格,且 7~15 位数字;
// 年龄为 1~150 的纯数字(允许空);血型限 A/B/AB/O 可带 +/- 或“未知”,兼容“X型”写法。
bool safety_profile_valid_phone(const char *text)
{
    if (!text || text[0] == '\0') return true;
    size_t digits = 0;
    for (const unsigned char *p = (const unsigned char *)text; *p; ++p) {
        if (*p >= '0' && *p <= '9') {
            ++digits;
            continue;
        }
        if (*p == ' ' || *p == '-' || *p == '+' ||
            *p == '(' || *p == ')') {
            continue;
        }
        return false;
    }
    return digits >= 7 && digits <= 15;
}

bool safety_profile_valid_age(const char *text)
{
    if (!text || text[0] == '\0') return true;
    char *end = NULL;
    long value = strtol(text, &end, 10);
    if (end == text || *end != '\0') return false;
    return value >= 1 && value <= 150;
}

bool safety_profile_valid_blood_type(const char *text)
{
    if (!text || text[0] == '\0') return true;
    char buffer[16];
    size_t length = strnlen(text, sizeof(buffer) - 1);
    memcpy(buffer, text, length);
    buffer[length] = '\0';
    // 兼容“A型”写法:去掉尾缀“型”(UTF-8: E5 9E 8B)
    // “未知”(UTF-8: E6 9C AA E7 9F A5)
    if (length >= 3 &&
        (unsigned char)buffer[length - 3] == 0xE5 &&
        (unsigned char)buffer[length - 2] == 0x9E &&
        (unsigned char)buffer[length - 1] == 0x8B) {
        length -= 3;
        buffer[length] = '\0';
    }
    if (length == 0) return true;
    if (length == 6 &&
        memcmp(buffer, "\xE6\x9C\xAA\xE7\x9F\xA5", 6) == 0) {
        return true;
    }
    size_t index = 0;
    if (length >= 2 && buffer[0] == 'A' && buffer[1] == 'B') {
        index = 2;
    } else if (buffer[0] == 'A' || buffer[0] == 'B' ||
               buffer[0] == 'O') {
        index = 1;
    } else {
        return false;
    }
    if (index == length) return true;
    if (index + 1 == length &&
        (buffer[index] == '+' || buffer[index] == '-')) {
        return true;
    }
    if (index + 2 == length &&
        (unsigned char)buffer[index] == 0xC2 &&
        (unsigned char)buffer[index + 1] == 0xB1) {
        return true;  /* ± */
    }
    return false;
}
