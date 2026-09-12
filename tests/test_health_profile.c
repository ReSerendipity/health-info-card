// tests/test_health_profile.c —— 健康信息卡档案数据的纯逻辑校验。
// 在提交/刷写前确保占位数据或用户数据不会导致界面越界/空白。
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "health_profile.h"

// UTF-8 字符串的字符数(中文按 1 个字符计,与屏幕宽度约束对应)。
static int utf8_len(const char *s)
{
    int n = 0;
    for (; *s; s++) {
        if ((*s & 0xC0) != 0x80) n++;   // 仅统计每个字符的首字节
    }
    return n;
}

static int count_digits(const char *s)
{
    int n = 0;
    for (; *s; s++)
        if (*s >= '0' && *s <= '9') n++;
    return n;
}

static int phone_ok(const char *phone)
{
    if (phone[0] == '\0') return 0;                 // 空电话不算有效
    size_t len = strlen(phone);
    if (len < 5 || len > 16) return 0;              // 长度过短/过长
    for (size_t i = 0; i < len; i++) {
        char c = phone[i];
        int ok = (c >= '0' && c <= '9') || c == '+' || c == '-' ||
                 c == '(' || c == ')' || c == ' ';
        if (!ok) return 0;
    }
    int digits = count_digits(phone);
    return digits >= 5 && digits <= 13;
}

int main(void)
{
    const health_profile_t *p = &g_health_profile;

    /* 基本信息:必填且长度受屏幕约束 */
    assert(p->name && p->name[0] != '\0');
    assert(utf8_len(p->name) <= 8);                 // 20px 大字一行放得下
    assert(strcmp(p->gender, "男") == 0 || strcmp(p->gender, "女") == 0);
    assert(p->age && p->age[0] != '\0');
    assert(strlen(p->age) <= 3);
    for (const char *c = p->age; *c; c++)
        assert(*c >= '0' && *c <= '9');
    assert(p->blood_type && p->blood_type[0] != '\0');
    assert(utf8_len(p->blood_type) <= 7);           // 血型徽章 170px 放得下

    /* 紧急联系人:至少一位有效,号码字符合法 */
    int valid_contacts = 0;
    for (int i = 0; i < 3; i++) {
        const char *rel = p->contacts[i].relation;
        const char *ph  = p->contacts[i].phone;
        if (rel[0] == '\0' && ph[0] == '\0') continue;      // 整行留空合法
        assert(rel[0] != '\0');                             // 有电话必须有关系
        assert(utf8_len(rel) <= 2);                         // 关系色块 48px 放得下
        assert(phone_ok(ph));
        valid_contacts++;
    }
    assert(valid_contacts >= 1);

    /* 医疗信息:字段非空(用 "无" 表示没有) */
    assert(p->allergies[0]   != '\0');
    assert(p->conditions[0]  != '\0');
    assert(p->medications[0] != '\0');
    assert(p->notes[0]       != '\0');

    /* 急救意愿:仅允许两种取值 */
    assert(strcmp(p->donor, "同意") == 0 || strcmp(p->donor, "拒绝") == 0);

    printf("Health profile checks: PASS (name=%s, contacts=%d)\n",
           p->name, valid_contacts);
    return 0;
}
