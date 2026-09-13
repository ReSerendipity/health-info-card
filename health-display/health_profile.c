// main/health_profile.c —— 健康信息卡档案数据（示例占位，使用前请修改）。
#include "health_profile.h"

const health_profile_t g_health_profile = {
    // ---- 基本信息 ----
    .name       = "李小明",        // 姓名
    .gender     = "男",            // 性别
    .age        = "32",            // 年龄
    .blood_type = "A 型",          // 血型
    // ---- 紧急联系人（最多 3 位；不需要的留 ""）----
    .contacts = {
        { .relation = "爸爸", .phone = "138-0000-0000" },
        { .relation = "妈妈", .phone = "139-0000-0000" },
        { .relation = "",     .phone = "" },
    },
    // ---- 医疗信息 ----
    .allergies   = "青霉素",       // 过敏史
    .conditions  = "无",           // 慢性病
    .medications = "无",           // 常服药物
    .notes       = "无",           // 其他备注（手术史等）
    // ---- 急救意愿 ----
    .donor       = "同意",         // 器官捐献意愿：同意 / 拒绝
};
