#include "pin_throttle.h"

#include <assert.h>

int main(void)
{
    pin_throttle_t t = {0};
    uint32_t now = 100000u;

    // 初始状态允许尝试。
    assert(pin_throttle_allowed(&t, now));

    // 前 4 次失败未锁定。
    for (int i = 0; i < 4; ++i) {
        pin_throttle_report_failure(&t, now, 5, 60000);
    }
    assert(pin_throttle_allowed(&t, now));

    // 第 5 次失败锁定 60 秒。
    pin_throttle_report_failure(&t, now, 5, 60000);
    assert(!pin_throttle_allowed(&t, now));
    assert(!pin_throttle_allowed(&t, now + 59000u));

    // 锁定到期后允许,重新计数。
    assert(pin_throttle_allowed(&t, now + 60000u));
    pin_throttle_report_failure(&t, now + 60000u, 5, 60000);
    assert(pin_throttle_allowed(&t, now + 61000u));

    // 成功后 reset 清零。
    pin_throttle_reset(&t);
    assert(pin_throttle_allowed(&t, now + 62000u));

    // 32 位毫秒计数器回绕边界:锁定到期后解锁。
    pin_throttle_t wrap = {0};
    uint32_t start = 0xFFFFFFF0u;
    pin_throttle_report_failure(&wrap, start, 1, 60000);
    assert(!pin_throttle_allowed(&wrap, 0xFFFFFFF1u));
    assert(pin_throttle_allowed(&wrap, start + 60000u));

    // 空指针安全。
    pin_throttle_allowed(NULL, now);
    pin_throttle_report_failure(NULL, now, 5, 60000);
    pin_throttle_reset(NULL);
    return 0;
}
