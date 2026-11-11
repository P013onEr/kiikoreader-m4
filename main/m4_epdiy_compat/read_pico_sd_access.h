#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef struct { bool allowed, active; } read_pico_sd_access_scope_t;
static inline bool read_pico_sd_access_begin(void) { return true; }
static inline void read_pico_sd_access_end(void) {}
static inline bool read_pico_sd_access_transition_begin(uint32_t timeout_ms) { (void)timeout_ms; return true; }
static inline void read_pico_sd_access_transition_end(void) {}
static inline void read_pico_sd_access_set_mounted(bool mounted) { (void)mounted; }
static inline read_pico_sd_access_scope_t read_pico_sd_access_scope(const char *path) {
    (void)path; return (read_pico_sd_access_scope_t){true, false};
}
static inline void read_pico_sd_access_scope_end(read_pico_sd_access_scope_t *scope) {
    (void)scope;
}
