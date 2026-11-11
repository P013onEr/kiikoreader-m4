#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#if KIIKO_BOARD_MURPHY_M4
#include "m4_project_platform.h"
#endif
typedef struct {
    bool time_synced;
    int64_t unix_sec;
    int battery_percent;
    bool charging;
    bool present;
    bool status_ok;
    uint8_t wake_reason;
    uint8_t alarm_mode;
    struct { bool wake_on_charge; } config;
} pmu_snapshot_t;
typedef enum { READ_PICO_PMU_KEY_NONE=0, READ_PICO_PMU_KEY_SHORT, READ_PICO_PMU_KEY_LONG } read_pico_pmu_key_action_t;
static inline bool read_pico_pmu_ready(void) { return true; }
static inline esp_err_t read_pico_pmu_poll(void) { return ESP_OK; }
static inline void read_pico_pmu_drain_events(void) {}
static inline read_pico_pmu_key_action_t read_pico_pmu_take_key_action(void) { return READ_PICO_PMU_KEY_NONE; }
static inline const pmu_snapshot_t *read_pico_pmu_get(void) {
    static pmu_snapshot_t s={.time_synced=false,.battery_percent=-1};
#if KIIKO_BOARD_MURPHY_M4
    s.battery_percent=m4_project_platform_battery_percent();
    s.charging=m4_project_platform_charging();
    s.present=s.battery_percent>=0;
    s.status_ok=s.present;
    s.time_synced=m4_project_platform_time_read(&s.unix_sec);
#endif
    return &s;
}
static inline int pmu_battery_percent(const pmu_snapshot_t *p) { return p ? p->battery_percent : -1; }
static inline bool pmu_battery_charging(const pmu_snapshot_t *p) { return p && p->charging; }
static inline esp_err_t read_pico_pmu_uid_get(uint8_t *uid) { if(uid) for(int i=0;i<16;i++) uid[i]=0; return ESP_OK; }
static inline esp_err_t read_pico_pmu_refresh(void) { return ESP_OK; }
static inline esp_err_t read_pico_pmu_cmd(int cmd, const void *p, size_t n) { (void)cmd;(void)p;(void)n; return ESP_OK; }
