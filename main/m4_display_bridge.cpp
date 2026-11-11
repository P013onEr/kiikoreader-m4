#include "m4_display_bridge.h"

#include <FreeInkDisplay.h>
#include <InputManager.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <cstring>

#include "m4_app_backend.h"
#include "e0470_page_turn.h"
#include "m4_refresh_policy.h"

static freeink::FreeInkDisplay *s_display = nullptr;
/* InputManager is provided by the SDK in the global namespace. */
static InputManager *s_input = nullptr;
static uint8_t *s_logical = nullptr;
static uint8_t *s_lastLogical = nullptr;
static uint8_t *s_lsb = nullptr;
static uint8_t *s_msb = nullptr;
static uint8_t *s_projectFramebuffer = nullptr;
static bool s_forceFullRefresh;

static bool ensure_buffers() {
    if (s_logical && s_lastLogical && s_lsb && s_msb) return true;
    s_logical = static_cast<uint8_t *>(heap_caps_malloc(m4AppLogicalGray2Bytes(), MALLOC_CAP_SPIRAM));
    s_lastLogical = static_cast<uint8_t *>(heap_caps_malloc(m4AppLogicalGray2Bytes(), MALLOC_CAP_SPIRAM));
    const size_t planeBytes = m4AppPanelGray2Bytes() / 2u;
    const size_t plane = planeBytes * 2u;
    uint8_t *planes = static_cast<uint8_t *>(heap_caps_malloc(plane, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
    if (!s_logical || !s_lastLogical || !planes) {
        heap_caps_free(s_logical);
        heap_caps_free(s_lastLogical);
        heap_caps_free(planes);
        s_logical = nullptr;
        s_lastLogical = nullptr;
        return false;
    }
    s_lsb = planes;
    s_msb = planes + planeBytes;
    return true;
}

extern "C" void m4_display_bridge_bind(void *display, EpdiyHighlevelState *state) {
    s_display = static_cast<freeink::FreeInkDisplay *>(display);
    if (state) (void)m4_display_bridge_framebuffer(state);
}

extern "C" uint8_t *m4_display_bridge_framebuffer(EpdiyHighlevelState *state) {
    if (!s_projectFramebuffer) {
        s_projectFramebuffer = static_cast<uint8_t *>(
            heap_caps_malloc(m4AppGray4Bytes(), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        if (s_projectFramebuffer)
            std::memset(s_projectFramebuffer, 0xff, m4AppGray4Bytes());
    }
    if (state && s_projectFramebuffer) {
        state->framebuffer = s_projectFramebuffer;
        state->width = M4_PROJECT_FB_WIDTH;
        state->height = M4_PROJECT_FB_HEIGHT;
    }
    return s_projectFramebuffer;
}

extern "C" void m4_input_bridge_bind(void *input) {
    s_input = static_cast<InputManager *>(input);
    m4_input_bridge_reset();
}

static uint16_t s_lastTouchX, s_lastTouchY;
static int s_virtualKey = -1;
static bool s_virtualDown;
static constexpr const char *TAG = "m4_touch";

static void normalize_to_project(float nx, float ny, uint16_t *x, uint16_t *y) {
    /* InputManager reports normalized panel-native 800x480 coordinates. */
    int logicalX = static_cast<int>((1.0f - ny) * 480.0f);
    int logicalY = static_cast<int>(nx * 800.0f);
    if (logicalX < 0) logicalX = 0;
    if (logicalX >= 480) logicalX = 479;
    if (logicalY < 0) logicalY = 0;
    if (logicalY >= 800) logicalY = 799;
    *x = static_cast<uint16_t>(logicalX);
    *y = static_cast<uint16_t>(logicalY);
}

extern "C" bool m4_input_bridge_read(cst836u_touch_t *touch) {
    if (!touch || !s_input) return false;
    std::memset(touch, 0, sizeof(*touch));
    s_input->update();

    float nx = 0.0f, ny = 0.0f;
    if (s_input->wasTouchPressedAt(nx, ny)) {
        normalize_to_project(nx, ny, &s_lastTouchX, &s_lastTouchY);
    } else if (s_input->isTouchHeldAt(nx, ny)) {
        normalize_to_project(nx, ny, &s_lastTouchX, &s_lastTouchY);
    }

    if (s_input->isTouchPressed()) {
        touch->touched = true;
        touch->count = 1;
        touch->x = s_lastTouchX;
        touch->y = s_lastTouchY;
        touch->points[0] = {true, 0, 0, s_lastTouchX, s_lastTouchY};
        ESP_LOGI(TAG, "press raw=(%.3f,%.3f) logical=(%u,%u)", static_cast<double>(nx),
                 static_cast<double>(ny), static_cast<unsigned>(s_lastTouchX),
                 static_cast<unsigned>(s_lastTouchY));
        return true;
    }
    if (s_input->wasTouchReleased()) {
        touch->x = s_lastTouchX;
        touch->y = s_lastTouchY;
        return true;
    }

    if (s_virtualDown) {
        s_virtualDown = false;
        touch->x = s_lastTouchX;
        touch->y = s_lastTouchY;
        s_virtualKey = -1;
        return true;
    }
    const uint8_t buttonToKey[] = {0xff, 1, 0, 2, 0, 2, 0xff};
    for (uint8_t button = 0; button < 7; ++button) {
        if (!s_input->wasPressed(button) || buttonToKey[button] == 0xff) continue;
        s_virtualKey = buttonToKey[button];
        s_virtualDown = true;
        s_lastTouchX = static_cast<uint16_t>(80 + s_virtualKey * 160);
        s_lastTouchY = 760;
        touch->touched = true;
        touch->count = 1;
        touch->x = s_lastTouchX;
        touch->y = s_lastTouchY;
        touch->points[0] = {true, 0, 0, s_lastTouchX, s_lastTouchY};
        ESP_LOGI(TAG, "button=%u mapped_key=%d logical=(%u,%u)", static_cast<unsigned>(button),
                 s_virtualKey, static_cast<unsigned>(s_lastTouchX),
                 static_cast<unsigned>(s_lastTouchY));
        return true;
    }
    return true;
}

static freeink::FreeInkDisplay::RefreshMode m4_freeink_refresh_mode(enum EpdDrawMode mode) {
    switch (m4_refresh_policy_from_project_mode(mode)) {
        case M4_REFRESH_POLICY_FAST: return freeink::FreeInkDisplay::FAST_REFRESH;
        case M4_REFRESH_POLICY_FULL: return freeink::FreeInkDisplay::FULL_REFRESH;
        case M4_REFRESH_POLICY_HALF:
        default: return freeink::FreeInkDisplay::HALF_REFRESH;
    }
}

static m4_refresh_policy_t m4_policy_from_freeink_mode(freeink::FreeInkDisplay::RefreshMode mode) {
    switch (mode) {
        case freeink::FreeInkDisplay::FAST_REFRESH: return M4_REFRESH_POLICY_FAST;
        case freeink::FreeInkDisplay::FULL_REFRESH: return M4_REFRESH_POLICY_FULL;
        case freeink::FreeInkDisplay::HALF_REFRESH:
        default: return M4_REFRESH_POLICY_HALF;
    }
}

static enum EpdDrawError m4_display_bridge_present(EpdiyHighlevelState *state,
                                                   freeink::FreeInkDisplay::RefreshMode mode) {
    if (!s_display || !state || !state->framebuffer || !ensure_buffers()) return EPD_DRAW_ERROR;
    const size_t source = m4AppGray4Bytes();
    if (!m4AppProjectFrameToLogical(state->framebuffer, source, s_logical,
                                    m4AppLogicalGray2Bytes())) return EPD_DRAW_ERROR;
    const size_t logicalBytes = m4AppLogicalGray2Bytes();
    static bool haveLastFrame;
    static m4_refresh_policy_t lastPolicy = M4_REFRESH_POLICY_HALF;
    const bool sameFrame = haveLastFrame && std::memcmp(s_logical, s_lastLogical, logicalBytes) == 0;
    const m4_refresh_policy_t requestedPolicy = m4_policy_from_freeink_mode(mode);
    const m4_refresh_policy_t resolvedPolicy =
        m4_refresh_policy_resolve(requestedPolicy, s_forceFullRefresh);
    if (resolvedPolicy != requestedPolicy) mode = freeink::FreeInkDisplay::FULL_REFRESH;
    if (!m4_refresh_policy_should_present(lastPolicy, resolvedPolicy, haveLastFrame, sameFrame))
        return EPD_DRAW_SUCCESS;
    uint8_t *bw = s_display->getFrameBuffer();
    if (!bw || !m4AppGray2ToSsd1677(s_logical, m4AppLogicalGray2Bytes(), bw,
                                    s_display->getDisplayWidthBytes() * 480u,
                                    s_display->getDisplayWidthBytes(), s_lsb, s_msb,
                                    m4AppPanelGray2Bytes() / 2u)) return EPD_DRAW_ERROR;
    s_display->displayGrayscaleBase(mode, false);
    s_display->copyGrayscaleBuffers(s_lsb, s_msb);
    s_display->displayGrayBuffer(false);
    std::memcpy(s_lastLogical, s_logical, logicalBytes);
    lastPolicy = resolvedPolicy;
    haveLastFrame = true;
    s_forceFullRefresh = false;
    return EPD_DRAW_SUCCESS;
}

extern "C" enum EpdDrawError m4_display_bridge_full(EpdiyHighlevelState *state) {
    return m4_display_bridge_present(state, freeink::FreeInkDisplay::FULL_REFRESH);
}

extern "C" enum EpdDrawError m4_display_bridge_area(EpdiyHighlevelState *state, EpdRect area) {
    (void)area;
    /* SSD1677 grayscale windows are not supported by the SDK contract. */
    return m4_display_bridge_present(state, freeink::FreeInkDisplay::HALF_REFRESH);
}

extern "C" EpdRect epd_full_screen(void) { return (EpdRect){0, 0, 480, 800}; }
extern "C" void guard_draw_result(EpdiyHighlevelState *state, enum EpdDrawError result) {
    if (result != EPD_DRAW_SUCCESS) (void)m4_display_bridge_full(state);
}
extern "C" enum EpdDrawError update_display_full(EpdiyHighlevelState *state) {
    return m4_display_bridge_full(state);
}
extern "C" enum EpdDrawError update_display_white(EpdiyHighlevelState *state) {
    if (!state || !state->framebuffer) return EPD_DRAW_ERROR;
    std::memset(state->framebuffer, 0xff, m4AppGray4Bytes());
    return m4_display_bridge_full(state);
}
extern "C" enum EpdDrawError update_display_mode(EpdiyHighlevelState *state, enum EpdDrawMode mode) {
    return m4_display_bridge_present(state, m4_freeink_refresh_mode(mode));
}
extern "C" enum EpdDrawError update_display_mode_diff(EpdiyHighlevelState *state, enum EpdDrawMode mode) {
    return update_display_mode(state, mode);
}
extern "C" enum EpdDrawError update_display_area_with(EpdiyHighlevelState *state, const EpdWaveform *, enum EpdDrawMode mode, EpdRect area) {
    (void)area;
    return m4_display_bridge_present(state, m4_freeink_refresh_mode(mode));
}
extern "C" enum EpdDrawError update_display_area_full_with(EpdiyHighlevelState *state, const EpdWaveform *w, enum EpdDrawMode m, EpdRect a) { return update_display_area_with(state, w, m, a); }
extern "C" enum EpdDrawError update_display_area_diff_with(EpdiyHighlevelState *state, const EpdWaveform *w, enum EpdDrawMode m, EpdRect a) { return update_display_area_with(state, w, m, a); }
extern "C" enum EpdDrawError update_display_with(EpdiyHighlevelState *state, const EpdWaveform *, enum EpdDrawMode mode) { return update_display_mode(state, mode); }
extern "C" enum EpdDrawError update_display_from_white(EpdiyHighlevelState *state) { return update_display_white(state); }
extern "C" enum EpdDrawError update_display_from_white_with(EpdiyHighlevelState *state, const EpdWaveform *, enum EpdDrawMode mode) {
    if (!state || !state->framebuffer) return EPD_DRAW_ERROR;
    std::memset(state->framebuffer, 0xff, m4AppGray4Bytes());
    return update_display_mode(state, mode);
}
extern "C" enum EpdDrawError update_display_image_gray(EpdiyHighlevelState *state) { return m4_display_bridge_full(state); }
extern "C" enum EpdDrawError update_display_fast_page(EpdiyHighlevelState *state) {
    return m4_display_bridge_present(state, freeink::FreeInkDisplay::FAST_REFRESH);
}
extern "C" enum EpdDrawError update_display_main_turn(EpdiyHighlevelState *state, EpdRect a, e0470_turn_dir_t) {
    (void)a; return m4_display_bridge_present(state, freeink::FreeInkDisplay::FAST_REFRESH);
}
extern "C" enum EpdDrawError update_display_water_turn(EpdiyHighlevelState *state, EpdRect a, e0470_turn_dir_t) {
    (void)a; return m4_display_bridge_present(state, freeink::FreeInkDisplay::FAST_REFRESH);
}
extern "C" enum EpdDrawError update_display_text_turn(EpdiyHighlevelState *state, EpdRect a, e0470_turn_dir_t) {
    (void)a; return m4_display_bridge_present(state, freeink::FreeInkDisplay::FAST_REFRESH);
}
extern "C" enum EpdDrawError update_display_click_feedback(EpdiyHighlevelState *state, EpdRect a) {
    (void)a; return m4_display_bridge_present(state, freeink::FreeInkDisplay::FAST_REFRESH);
}
extern "C" enum EpdDrawError display_panel_repair(EpdiyHighlevelState *state) { return m4_display_bridge_full(state); }
extern "C" void display_set_bulk_io(bool) {}
extern "C" void display_request_full_refresh(void) { s_forceFullRefresh = true; }
extern "C" void display_hold_white_exit(bool) {}
extern "C" bool display_take_white_exit(void) { return false; }
extern "C" void rails_keepalive(void) {}
extern "C" void rails_idle_check(int64_t) {}
extern "C" int display_pclk_mhz(void) { return 0; }
extern "C" unsigned display_main_quality_set(unsigned value) { return value; }

static volatile uint32_t s_pressed, s_released;
static volatile bool s_touch_pressed, s_touch_released;
static volatile int s_touch_x, s_touch_y;

extern "C" void m4_input_bridge_reset(void) {
    s_pressed = 0; s_released = 0;
    s_touch_pressed = false; s_touch_released = false;
    s_touch_x = 0; s_touch_y = 0;
    s_lastTouchX = s_lastTouchY = 0;
    s_virtualKey = -1;
    s_virtualDown = false;
}
extern "C" void m4_input_bridge_set_touch(bool pressed, bool released, int x, int y) {
    s_touch_pressed = pressed; s_touch_released = released; s_touch_x = x; s_touch_y = y;
}
extern "C" void m4_input_bridge_set_keys(uint32_t pressed, uint32_t released) {
    s_pressed |= pressed; s_released |= released;
}
extern "C" bool m4_input_bridge_take_touch(bool *pressed, bool *released, int *x, int *y) {
    if (pressed) *pressed = s_touch_pressed;
    if (released) *released = s_touch_released;
    if (x) *x = s_touch_x;
    if (y) *y = s_touch_y;
    const bool any = s_touch_pressed || s_touch_released;
    s_touch_pressed = false; s_touch_released = false;
    return any;
}
extern "C" uint32_t m4_input_bridge_take_pressed(void) { uint32_t v = s_pressed; s_pressed = 0; return v; }
extern "C" uint32_t m4_input_bridge_take_released(void) { uint32_t v = s_released; s_released = 0; return v; }
