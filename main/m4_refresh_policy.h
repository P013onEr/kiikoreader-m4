#pragma once

#include <stdbool.h>
#include "m4_epdiy_compat/epd_highlevel.h"

typedef enum {
    M4_REFRESH_POLICY_FAST = 0,
    M4_REFRESH_POLICY_HALF,
    M4_REFRESH_POLICY_FULL,
} m4_refresh_policy_t;

static inline m4_refresh_policy_t m4_refresh_policy_from_project_mode(enum EpdDrawMode mode) {
    switch (mode) {
        /* Murphy M4 Batch 2 SDK uses FAST 0xFC, HALF 0xD4 (temperature 0x50), FULL 0xF7. */
        case MODE_DU: return M4_REFRESH_POLICY_FAST;
        case MODE_GL16: return M4_REFRESH_POLICY_HALF;
        case MODE_GC16: return M4_REFRESH_POLICY_FULL;
        default: return M4_REFRESH_POLICY_HALF;
    }
}

static inline bool m4_refresh_policy_should_present(m4_refresh_policy_t previous,
                                                     m4_refresh_policy_t requested,
                                                     bool have_previous, bool same_frame) {
    if (!same_frame || !have_previous || requested == M4_REFRESH_POLICY_FULL) return true;
    return previous < requested;
}

static inline m4_refresh_policy_t m4_refresh_policy_resolve(m4_refresh_policy_t requested,
                                                            bool force_full) {
    return force_full ? M4_REFRESH_POLICY_FULL : requested;
}
