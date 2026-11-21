/* SPDX-License-Identifier: MIT */
/* Optional, read-only inter-mod discovery API. Native slot values are NOT custom weapon keys. */
#ifndef EXPANDED_QUICK_SELECT_COMPAT_API_H
#define EXPANDED_QUICK_SELECT_COMPAT_API_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define EQS_COMPAT_API_VERSION_1 1u
#define EQS_COMPAT_NO_PAGE UINT32_MAX
#define EQS_COMPAT_GAMEPLAY_OPEN 1u
#define EQS_COMPAT_EDITOR_OPEN 2u

/* Every result uses zero-based page indices. EQS_COMPAT_NO_PAGE means inactive. */
typedef struct eqs_compat_state_v1 {
    uint32_t size;
    uint32_t page_count;
    uint32_t gameplay_page;
    uint32_t editor_page;
    uint32_t flags;
} eqs_compat_state_v1;

/* These are the game's native quick-select IDs (0..39), not stable custom keys.
   A carrier such as Walloper 18 does not identify which custom weapon owns it. */
typedef struct eqs_compat_slots_v1 {
    uint32_t size;
    uint32_t count;
    int32_t native_ids[8];
} eqs_compat_slots_v1;

/* Copy-only, game-thread-only v1 API; no OpenRAC or C++ types cross the boundary.
   Consumers must size the output before calling and check all function pointers.
   Do not cache the API table past the exporting mod's lifetime. */
typedef struct eqs_compat_api_v1 {
    uint32_t size;
    uint32_t version;
    int (*get_state)(eqs_compat_state_v1* out);
    int (*get_slots)(uint32_t zero_based_page, eqs_compat_slots_v1* out);
} eqs_compat_api_v1;

/* Resolve the symbol "openrac_expanded_quick_select_query" from the ALREADY
   loaded Expanded Quick Select module. Never LoadLibrary a second mod instance.
   Query version 1; null means this optional provider is unavailable. */
typedef const eqs_compat_api_v1* (*eqs_compat_query_fn)(uint32_t version);
#ifdef __cplusplus
}
#endif
#endif
