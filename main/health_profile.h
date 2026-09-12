// main/health_profile.h —— 健康信息卡：全部个人数据集中在这里，改完重新编译刷写即可。
//
// ⚠️  本文件是【个人信息配置文件】：所有占位内容（姓名、电话、病史等）均为示例，
//     请在使用前替换成你自己的真实信息。电话建议用 138-0000-0000 这种
//     “3-4-4” 分组格式，小屏幕上更易读。
//
// ⚠️  中文显示依赖 main/health_font_16.c / health_font_20.c 中的子集字体。
//     如果姓名或病史包含这些字体没有的汉字，屏幕上会显示为空白/方块。
//     解决：把缺的字加进 tools/gen_health_font.ps1 的字符集文件后重新生成字体。
//     常用姓氏、名字用字和医疗术语已内置。
//
// 字段约定：留空字符串 "" 表示“该栏目不显示”；填 "无" 表示“无此问题”。
#pragma once

typedef struct {
    const char *relation;   // 关系，例如 "爸爸"
    const char *phone;      // 电话，例如 "138-0000-0000"
} health_contact_t;

typedef struct {
    // ---- 基本信息 ----
    const char *name;       // 姓名
    const char *gender;     // 性别：男 / 女
    const char *age;        // 年龄，例如 "32"
    const char *blood_type; // 血型，例如 "A 型"（含 Rh 可写 "A 型 Rh+"）
    // ---- 紧急联系人（最多 3 位，第二位起可留空）----
    health_contact_t contacts[3];
    // ---- 医疗信息 ----
    const char *allergies;   // 过敏史，无则填 "无"
    const char *conditions;  // 慢性病，无则填 "无"
    const char *medications; // 常服药物，无则填 "无"
    const char *notes;       // 其他备注（手术史等），无则填 "无"
    // ---- 急救意愿 ----
    const char *donor;       // 器官捐献意愿：同意 / 拒绝
} health_profile_t;

// 档案内容：修改这里即可，页面会自动展示。
extern const health_profile_t g_health_profile;
