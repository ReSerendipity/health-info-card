#include "safety_profile.h"

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
    profile->show_full_phone = 1;
    strncpy(profile->help_text, "您好，我可能迷路了，请帮我联系家人",
            sizeof(profile->help_text) - 1);
    strncpy(profile->wechat_note, "请添加我的家人，备注安心牌",
            sizeof(profile->wechat_note) - 1);

    // ---- 健康信息卡占位档案(示例数据) ----
    // 无 NVS 档案时开机直接展示以下占位内容;使用前请替换为真实信息:
    // 方式一: 直接改本函数中的字段,重新编译刷写;
    // 方式二: 长按确认键进入本地设置门户,用网页表单配置后保存到 NVS。
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
    // 标记为已配置,开机即可直接展示占位档案
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

void safety_profile_mask_phone(const safety_profile_t *profile,
                               char *output, size_t capacity)
{
    if (!output || capacity == 0) return;
    output[0] = '\0';
    if (!profile) return;

    size_t length = strnlen(profile->phone, sizeof(profile->phone));
    if (profile->show_full_phone || length < 7) {
        strncpy(output, profile->phone, capacity - 1);
        output[capacity - 1] = '\0';
        return;
    }

    size_t used = 0;
    for (size_t i = 0; i < length && used + 1 < capacity; ++i) {
        bool hidden = i >= 3 && i + 4 < length;
        output[used++] = hidden ? '*' : profile->phone[i];
    }
    output[used] = '\0';
}
