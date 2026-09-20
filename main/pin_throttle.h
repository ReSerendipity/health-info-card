#pragma once

#include <stdbool.h>
#include <stdint.h>

// Pure-logic PIN attempt throttle (no ESP-IDF dependency, host-testable):
// after max_failures consecutive failures the throttle locks for lock_ms,
// rejecting every attempt until the window expires. A successful attempt
// (caller calls pin_throttle_reset) clears the counter.
typedef struct {
    uint32_t failures;
    uint32_t locked_until_ms;
} pin_throttle_t;

void pin_throttle_reset(pin_throttle_t *throttle);

// Returns true when an attempt may proceed. Expired locks are reported as
// allowed; the caller then decides whether to report a new failure.
bool pin_throttle_allowed(const pin_throttle_t *throttle, uint32_t now_ms);

// Records one failed attempt. When the failure count reaches max_failures
// the lock is set to now_ms + lock_ms. A lock that already expired is reset
// before counting, so the counter restarts after an unlock.
void pin_throttle_report_failure(pin_throttle_t *throttle, uint32_t now_ms,
                                 uint32_t max_failures, uint32_t lock_ms);
