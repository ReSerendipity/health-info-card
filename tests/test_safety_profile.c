#include "safety_profile.h"

#include <assert.h>
#include <string.h>

int main(void)
{
    safety_profile_t profile;
    safety_profile_defaults(&profile);
    assert(profile.configured == 1);
    assert(profile.demo == 1);
    assert(profile.show_full_phone == 0);
    assert(!safety_profile_has_pin(&profile));
    assert(safety_profile_is_valid(&profile));
    assert(strcmp(profile.age, "68") == 0);
    assert(strcmp(profile.blood_type, "A型") == 0);

    strcpy(profile.phone, "13800138000");
    profile.show_full_phone = 0;
    safety_profile_seal(&profile);
    assert(safety_profile_is_valid(&profile));
    assert(profile.configured == 1);

    char masked[32];
    safety_profile_mask_phone(&profile, masked, sizeof(masked));
    assert(strcmp(masked, "138****8000") == 0);

    safety_profile_t tampered = profile;
    tampered.phone[0] = '9';
    assert(!safety_profile_is_valid(&tampered));

    // 掩码规则:11 位号码保留前 3 后 4,中间置 *;
    // show_full 时明文;7 位以下短号不掩码。
    // 表单内容校验:电话仅数字、年龄 1~150、血型白名单。
    assert(safety_profile_valid_phone("13800000000"));
    assert(safety_profile_valid_phone("+86 138-0000-0000"));
    assert(!safety_profile_valid_phone("abc"));
    assert(!safety_profile_valid_phone("123"));
    assert(safety_profile_valid_phone(""));
    assert(safety_profile_valid_age("68"));
    assert(safety_profile_valid_age("1"));
    assert(safety_profile_valid_age("150"));
    assert(!safety_profile_valid_age("0"));
    assert(!safety_profile_valid_age("151"));
    assert(!safety_profile_valid_age("abc"));
    assert(!safety_profile_valid_age("6a8"));
    assert(safety_profile_valid_age(""));
    assert(safety_profile_valid_blood_type("A"));
    assert(safety_profile_valid_blood_type("AB+"));
    assert(safety_profile_valid_blood_type("O-"));
    assert(safety_profile_valid_blood_type("B型"));
    assert(safety_profile_valid_blood_type("未知"));
    assert(!safety_profile_valid_blood_type("X"));
    assert(!safety_profile_valid_blood_type("AA"));
    assert(safety_profile_valid_blood_type(""));
    safety_profile_mask_number("13800000000", false, masked, sizeof(masked));
    assert(strcmp(masked, "138****0000") == 0);
    safety_profile_mask_number("13800000000", true, masked, sizeof(masked));
    assert(strcmp(masked, "13800000000") == 0);
    safety_profile_mask_number("123", false, masked, sizeof(masked));
    assert(strcmp(masked, "123") == 0);
    safety_profile_mask_number(NULL, false, masked, sizeof(masked));
    assert(masked[0] == '\0');

    // NVS 损坏/版本不符/长度不符的档案必须被判无效,存储层据此降级
    // 为"未配置"并引导重新配置,绝不展示半截档案。
    safety_profile_t bad_version = profile;
    bad_version.version++;
    assert(!safety_profile_is_valid(&bad_version));

    safety_profile_t bad_size = profile;
    bad_size.size++;
    assert(!safety_profile_is_valid(&bad_size));

    profile.show_full_phone = 1;
    safety_profile_mask_phone(&profile, masked, sizeof(masked));
    assert(strcmp(masked, "13800138000") == 0);

    profile.pin_hash[0] = 1;
    assert(safety_profile_has_pin(&profile));
    return 0;
}
