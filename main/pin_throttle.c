#include "pin_throttle.h"

void pin_throttle_reset(pin_throttle_t *throttle)
{
    if (!throttle) return;
    throttle->failures = 0;
    throttle->locked_until_ms = 0;
}

bool pin_throttle_allowed(const pin_throttle_t *throttle, uint32_t now_ms)
{
    if (!throttle || throttle->locked_until_ms == 0) return true;
    // Unsigned subtraction stays correct across the 32-bit ms counter wrap;
    // a positive signed difference means now_ms is at/after the unlock time.
    return (int32_t)(now_ms - throttle->locked_until_ms) >= 0;
}

void pin_throttle_report_failure(pin_throttle_t *throttle, uint32_t now_ms,
                                 uint32_t max_failures, uint32_t lock_ms)
{
    if (!throttle || max_failures == 0) return;
    if (throttle->locked_until_ms != 0 &&
        (int32_t)(now_ms - throttle->locked_until_ms) >= 0) {
        // Lock expired: start a fresh window.
        throttle->locked_until_ms = 0;
        throttle->failures = 0;
    }
    if (++throttle->failures >= max_failures) {
        throttle->locked_until_ms = now_ms + lock_ms;
    }
}
