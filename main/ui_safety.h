#pragma once

#include <stdbool.h>

#include "safety_profile.h"

// Base profile pages (name / contacts / address / health). QR pages are appended
// after page 3 at runtime, one per populated image slot.
#define UI_SAFETY_BASE_PAGES 4
#define UI_SAFETY_PAGE_COUNT 5  // deprecated: kept for callers that hard-coded it

void ui_safety_show_profile(const safety_profile_t *profile, int page,
                            bool has_wechat_qr, int total_pages,
                            int battery_percent);
void ui_safety_show_setup(const char *ssid, const char *password,
                          bool first_setup, int battery_percent);
void ui_safety_show_reset_confirm(int battery_percent);
void ui_safety_show_saved(int battery_percent);
void ui_safety_show_qr_loading(int battery_percent);
void ui_safety_show_qr_error(int battery_percent);
