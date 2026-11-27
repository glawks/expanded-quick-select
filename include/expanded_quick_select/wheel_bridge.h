/* SPDX-License-Identifier: MIT */
/* Optional Windows test bridge: caller owns the wheel, EQS owns assignments.
   Game thread only. Resolve from the already loaded module. */
#ifndef OPENRAC_EQS_WHEEL_BRIDGE_H
#define OPENRAC_EQS_WHEEL_BRIDGE_H
#include <stdint.h>
enum { EQS_WHEEL_BEGIN = 1, EQS_WHEEL_UPDATE = 2, EQS_WHEEL_DRAW = 3, EQS_WHEEL_END = 4 };
typedef struct eqs_wheel_page_v2 {
    uint32_t size;
    uint32_t page_index;
    uint32_t page_count;
    int32_t native_ids[8];
} eqs_wheel_page_v2;
typedef int (*eqs_wheel_page_fn)(uint32_t event, uint32_t held_unmasked, eqs_wheel_page_v2* out);
#endif
