#include <FreeInkDisplay.h>
#include <InputManager.h>
#include <BatteryMonitor.h>
#include <FrontlightManager.h>
#include <Rtc.h>
#include "BoardConfig.h"
#include "m4_ui.h"
#include "m4_display_bridge.h"
#include "m4_project_platform.h"
extern "C" {
#include "read_pico_sd.h"
#include "app/app_loop.h"
#include "app/app_registry.h"
#include "settings.h"
#include "setup_state.h"
#include "boot_state.h"
#include "app_font_context.h"
}
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "sdmmc_cmd.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <strings.h>
#include <cstdio>
#include <dirent.h>
#include <sys/stat.h>
#include <FreeInkBook.h>
#include <BookArena.h>
#include <layout/ChapterLayout.h>
#include <render/TtfFont.h>
#include <render/PageRenderer.h>

extern const uint8_t builtin_ttf_start[] asm("_binary_builtin_ttf_start");
extern const uint8_t builtin_ttf_end[] asm("_binary_builtin_ttf_end");

static BatteryMonitor *s_projectBattery;
static Rtc *s_projectRtc;
static int s_projectBatteryPercent = -1;
static bool s_projectCharging;
static bool s_projectTimeValid;
static int64_t s_projectUnixTime;
static int64_t s_projectBatteryReadAt;
static int64_t s_projectRtcReadAt;

extern "C" void m4_project_platform_poll(void) {
    const int64_t now = esp_timer_get_time();
    if (s_projectBattery && now - s_projectBatteryReadAt >= 5000000) {
        s_projectBatteryReadAt = now;
        const auto status = s_projectBattery->readStatus();
        if (status.supported && status.percentageKnown && status.percentage <= 100)
            s_projectBatteryPercent = static_cast<int>(status.percentage);
        if (status.supported && status.chargingKnown)
            s_projectCharging = status.charging;
    }
    if (s_projectRtc && now - s_projectRtcReadAt >= 30000000) {
        s_projectRtcReadAt = now;
        Rtc::DateTime date{};
        s_projectTimeValid = false;
        if (s_projectRtc->now(date) && date.year >= 2024 && date.year <= 2099 &&
            date.month >= 1 && date.month <= 12 && date.day >= 1 && date.day <= 31 &&
            date.hour < 24 && date.minute < 60 && date.second < 60) {
            static constexpr unsigned monthDays[] = {31,28,31,30,31,30,31,31,30,31,30,31};
            int64_t days = 0;
            for (uint16_t y = 1970; y < date.year; ++y)
                days += (y % 400 == 0 || (y % 4 == 0 && y % 100 != 0)) ? 366 : 365;
            for (uint8_t m = 1; m < date.month; ++m) {
                days += monthDays[m - 1];
                if (m == 2 && (date.year % 400 == 0 || (date.year % 4 == 0 && date.year % 100 != 0))) ++days;
            }
            days += date.day - 1;
            /* Shared status formatting applies UTC+8; express local RTC as UTC here. */
            s_projectUnixTime = days * 86400LL + static_cast<int64_t>(date.hour) * 3600LL +
                                static_cast<int64_t>(date.minute) * 60LL + date.second - 8 * 3600LL;
            s_projectTimeValid = true;
        }
    }
}

extern "C" int m4_project_platform_battery_percent(void) { return s_projectBatteryPercent; }
extern "C" bool m4_project_platform_charging(void) { return s_projectCharging; }
extern "C" bool m4_project_platform_time_read(int64_t *unix_sec) {
    if (!unix_sec || !s_projectTimeValid) return false;
    *unix_sec = s_projectUnixTime;
    return true;
}

static const char *TAG = "murphy_m4";

static sdmmc_card_t *s_card = nullptr;

class PageCountSink final : public freeink::book::PageSink {
 public:
    bool onPage(const freeink::book::Page &page) override { pages = page.pageIndex + 1; return true; }
    uint32_t pages = 0;
};

class FileBookSource final : public freeink::book::BookSource {
 public:
    explicit FileBookSource(const char *path) : file_(std::fopen(path, "rb")) {
        if (file_) {
            std::fseek(file_, 0, SEEK_END);
            size_ = static_cast<uint64_t>(std::ftell(file_));
            std::fseek(file_, 0, SEEK_SET);
        }
    }
    ~FileBookSource() override { if (file_) std::fclose(file_); }
    bool good() const { return file_ != nullptr; }
    int32_t readAt(uint64_t offset, void *dst, uint32_t len) override {
        if (!file_ || std::fseek(file_, static_cast<long>(offset), SEEK_SET) != 0) return -1;
        return static_cast<int32_t>(std::fread(dst, 1, len, file_));
    }
    uint64_t size() const override { return size_; }
 private:
    FILE *file_ = nullptr;
    uint64_t size_ = 0;
};

class FirstPageSink final : public freeink::book::PageSink {
 public:
    FirstPageSink(freeink::book::FontChain &fontChain, freeink::book::BookSource &bookSource,
                  const freeink::book::ZipCatalog &catalog, freeink::book::Arena &renderScratch,
                  freeink::FreeInkDisplay &panel, uint8_t *grayTarget,
                  uint8_t *grayLsb, uint8_t *grayMsb)
        : fonts_(fontChain), source_(bookSource), zip_(catalog), scratch_(renderScratch),
          display_(panel), gray_(grayTarget), lsb_(grayLsb), msb_(grayMsb) {}

    bool onPage(const freeink::book::Page &page) override {
        if (page.pageIndex != 0) return false;
        pages_ = 1;
        std::memset(gray_, 0xff, kPixels);
        const freeink::book::FrameTarget target{gray_, kWidth, kHeight, kWidth,
            freeink::book::FrameFormat::Gray8, freeink::book::FrameRotation::None};
        renderStatus_ = freeink::book::PageRenderer::render(page, fonts_, source_, zip_, scratch_, target);
        if (renderStatus_ != freeink::book::BookStatus::Ok) return false;

        uint8_t *frame = display_.getFrameBuffer();
        if (!frame) {
            renderStatus_ = freeink::book::BookStatus::OutOfMemory;
            return false;
        }
        const size_t frameBytes = static_cast<size_t>(display_.getDisplayWidthBytes()) * kHeight;
        std::memset(frame, 0xff, frameBytes);
        std::memset(lsb_, 0, kPlaneBytes);
        std::memset(msb_, 0, kPlaneBytes);
        for (uint16_t y = 0; y < kHeight; ++y) {
            for (uint16_t x = 0; x < kWidth; ++x) {
                const uint8_t value = gray_[static_cast<size_t>(y) * kWidth + x];
                const uint8_t level = quantize(value, x, y);
                const size_t packedIndex = static_cast<size_t>(y) * kWidth + x;
                const size_t planeIndex = packedIndex / 8u;
                const uint8_t mask = static_cast<uint8_t>(0x80u >> (x & 7u));
                const size_t frameIndex = static_cast<size_t>(y) * display_.getDisplayWidthBytes() + x / 8u;
                if (level == 0) {
                    frame[frameIndex] &= static_cast<uint8_t>(~mask);
                } else if (level == 1) {
                    lsb_[planeIndex] |= mask;
                    msb_[planeIndex] |= mask;
                } else if (level == 2) {
                    lsb_[planeIndex] |= mask;
                }
            }
        }
        display_.displayGrayscaleBase(freeink::FreeInkDisplay::HALF_REFRESH, false);
        display_.copyGrayscaleBuffers(lsb_, msb_);
        display_.displayGrayBuffer(false);
        displayed_ = true;
        return false;
    }

    uint32_t pages() const { return pages_; }
    bool displayed() const { return displayed_; }
    freeink::book::BookStatus renderStatus() const { return renderStatus_; }

 private:
    static constexpr uint16_t kWidth = 800;
    static constexpr uint16_t kHeight = 480;
    static constexpr size_t kPixels = static_cast<size_t>(kWidth) * kHeight;
    static constexpr size_t kPlaneBytes = kPixels / 8u;

    static uint8_t quantize(uint8_t value, uint16_t x, uint16_t y) {
        (void)x; (void)y;
        return static_cast<uint8_t>(value >> 6);
    }

    freeink::book::FontChain &fonts_;
    freeink::book::BookSource &source_;
    const freeink::book::ZipCatalog &zip_;
    freeink::book::Arena &scratch_;
    freeink::FreeInkDisplay &display_;
    uint8_t *gray_;
    uint8_t *lsb_;
    uint8_t *msb_;
    uint32_t pages_ = 0;
    bool displayed_ = false;
    freeink::book::BookStatus renderStatus_ = freeink::book::BookStatus::Ok;
};

[[maybe_unused]] static void probeFirstEpub(freeink::FreeInkDisplay &display) {
    DIR *dir = opendir("/sdcard");
    if (!dir) {
        ESP_LOGW(TAG, "EPUB probe skipped: /sdcard unavailable");
        return;
    }
    char path[256] = {};
    struct dirent *entry = nullptr;
    while ((entry = readdir(dir)) != nullptr) {
        const char *name = entry->d_name;
        const size_t n = std::strlen(name);
        constexpr char prefix[] = "/sdcard/";
        if (n > 5 && sizeof(prefix) - 1 + n < sizeof(path) &&
            strcasecmp(name + n - 5, ".epub") == 0) {
            std::memcpy(path, prefix, sizeof(prefix) - 1);
            std::memcpy(path + sizeof(prefix) - 1, name, n + 1);
            break;
        }
    }
    closedir(dir);
    if (path[0] == '\0') {
        ESP_LOGI(TAG, "EPUB probe: no .epub at /sdcard root");
        return;
    }
    FileBookSource source(path);
    if (!source.good()) {
        ESP_LOGE(TAG, "EPUB probe: cannot open %s", path);
        return;
    }
    constexpr size_t kBookArenaBytes = 384 * 1024;
    constexpr size_t kScratchBytes = 256 * 1024;
    constexpr size_t kFontArenaBytes = 128 * 1024;
    constexpr size_t kPixels = 800u * 480u;
    constexpr size_t kPlaneBytes = kPixels / 8u;
    auto *bookMem = static_cast<uint8_t *>(heap_caps_malloc(kBookArenaBytes, MALLOC_CAP_SPIRAM));
    auto *scratchMem = static_cast<uint8_t *>(heap_caps_malloc(kScratchBytes, MALLOC_CAP_SPIRAM));
    auto *fontMem = static_cast<uint8_t *>(heap_caps_malloc(kFontArenaBytes, MALLOC_CAP_SPIRAM));
    auto *gray = static_cast<uint8_t *>(heap_caps_malloc(kPixels, MALLOC_CAP_SPIRAM));
    auto *lsb = static_cast<uint8_t *>(heap_caps_calloc(1, kPlaneBytes, MALLOC_CAP_SPIRAM));
    auto *msb = static_cast<uint8_t *>(heap_caps_calloc(1, kPlaneBytes, MALLOC_CAP_SPIRAM));
    if (!bookMem || !scratchMem || !fontMem || !gray || !lsb || !msb) {
        ESP_LOGE(TAG, "EPUB probe: arena allocation failed");
        heap_caps_free(bookMem);
        heap_caps_free(scratchMem);
        heap_caps_free(fontMem);
        heap_caps_free(gray);
        heap_caps_free(lsb);
        heap_caps_free(msb);
        return;
    }
    {
        freeink::book::Arena bookArena(bookMem, kBookArenaBytes);
        freeink::book::Arena scratch(scratchMem, kScratchBytes);
        freeink::book::Book book;
        const auto status = book.open(source, bookArena, scratch);
        ESP_LOGI(TAG, "EPUB probe: %s status=%s size=%llu spine=%u toc=%u title=%s author=%s",
                 path, freeink::book::bookStatusName(status),
                 static_cast<unsigned long long>(source.size()),
                 static_cast<unsigned>(book.spineCount()), static_cast<unsigned>(book.tocCount()),
                 book.metadata().title, book.metadata().author);
        if (status == freeink::book::BookStatus::Ok && book.spineCount() > 0) {
            const auto *spine = book.spineItem(0);
            const auto *entry = spine ? book.zip().find(spine->href) : nullptr;
            freeink::book::Arena fontArena(fontMem, kFontArenaBytes);
            freeink::book::TtfFont font;
            freeink::book::FontChain fonts;
            const bool fontReady = font.init(builtin_ttf_start,
                                             static_cast<uint32_t>(builtin_ttf_end - builtin_ttf_start),
                                             fontArena);
            if (fontReady) fonts.add(&font);
            FirstPageSink sink(fonts, source, book.zip(), scratch, display, gray, lsb, msb);
            freeink::book::LayoutParams params;
            params.pageWidth = 800;
            params.pageHeight = 480;
            params.baseSizePx = 18;
            params.language = book.metadata().language;
            params.font = fontReady ? &fonts : nullptr;
            uint32_t pageCount = 0;
            auto layoutStatus = (entry && fontReady)
                ? freeink::book::ChapterLayout::layout(source, book.zip(), *entry, spine->href,
                                                       params, scratch, sink, &pageCount)
                : freeink::book::BookStatus::Unsupported;
            ESP_LOGI(TAG, "EPUB first chapter: font=%s layout=%s pages=%u href=%s",
                     fontReady ? "ready" : "unavailable",
                     freeink::book::bookStatusName(layoutStatus),
                     static_cast<unsigned>(pageCount), spine ? spine->href : "");
            ESP_LOGI(TAG, "EPUB first page: render=%s displayed=%s",
                     freeink::book::bookStatusName(sink.renderStatus()),
                     sink.displayed() ? "yes" : "no");
        }
    }
    heap_caps_free(bookMem);
    heap_caps_free(scratchMem);
    heap_caps_free(fontMem);
    heap_caps_free(gray);
    heap_caps_free(lsb);
    heap_caps_free(msb);
}

static bool mountSdCard() {
    const auto &pins = BoardConfig::ACTIVE.sdmmc;
    if (pins.busWidth != 4) {
        ESP_LOGE(TAG, "M4 SDMMC profile is not 4-bit");
        return false;
    }
    const int power = BoardConfig::ACTIVE.sd.powerEnable;
    if (power >= 0) {
        gpio_hold_dis(static_cast<gpio_num_t>(power));
        gpio_set_direction(static_cast<gpio_num_t>(power), GPIO_MODE_OUTPUT);
        const auto &sd = BoardConfig::ACTIVE.sd;
        if (sd.powerCycleOnMount) {
            gpio_set_level(static_cast<gpio_num_t>(power), sd.powerActiveHigh ? 0 : 1);
            vTaskDelay(pdMS_TO_TICKS(80));
            gpio_set_level(static_cast<gpio_num_t>(power), sd.powerActiveHigh ? 1 : 0);
            vTaskDelay(pdMS_TO_TICKS(120));
        } else {
            gpio_set_level(static_cast<gpio_num_t>(power), sd.powerActiveHigh ? 1 : 0);
        }
    }

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.max_freq_khz = SDMMC_FREQ_DEFAULT;
    host.unaligned_multi_block_rw_max_chunk_size = 8;
    sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
    slot.width = 4;
    slot.clk = static_cast<gpio_num_t>(pins.clk);
    slot.cmd = static_cast<gpio_num_t>(pins.cmd);
    slot.d0 = static_cast<gpio_num_t>(pins.d0);
    slot.d1 = static_cast<gpio_num_t>(pins.d1);
    slot.d2 = static_cast<gpio_num_t>(pins.d2);
    slot.d3 = static_cast<gpio_num_t>(pins.d3);
    slot.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;
    esp_vfs_fat_sdmmc_mount_config_t mountConfig = {};
    mountConfig.format_if_mount_failed = false;
    mountConfig.max_files = 8;
    mountConfig.allocation_unit_size = 16 * 1024;
    const esp_err_t err = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot,
                                                  &mountConfig, &s_card);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SDMMC mount failed: %s", esp_err_to_name(err));
        s_card = nullptr;
        return false;
    }
    ESP_LOGI(TAG, "SD card mounted at /sdcard: %llu MB, %u-bit",
             static_cast<unsigned long long>(s_card->csd.capacity / 2048ULL),
             static_cast<unsigned>(slot.width));
    return true;
}

extern "C" void app_main(void) {
    using namespace freeink;
    ESP_LOGI(TAG, "Murphy M4 display and grayscale bring-up");
    const esp_partition_t *running = esp_ota_get_running_partition();
    if (running) {
        ESP_LOGI(TAG, "running app partition: %s offset=0x%lx size=0x%lx",
                 running->label, static_cast<unsigned long>(running->address),
                 static_cast<unsigned long>(running->size));
    } else {
        ESP_LOGE(TAG, "running app partition unavailable");
    }
    if (!BoardConfig::selectDevice(BoardConfig::Board::MurphyM4)) {
        ESP_LOGE(TAG, "Murphy M4 profile is missing from this build");
        return;
    }
    FreeInkDisplay display(0, 0, 0, 0, 0, 0);
    display.setMurphyM4Batch(MurphyM4Batch::Second);
    InputManager input;
    input.setMurphyM4Batch(MurphyM4Batch::Second);
    BatteryMonitor battery;
    FrontlightManager frontlight;
    Rtc rtc;
    BoardConfig::holdPowerRails();
    const bool sdReady = mountSdCard();
    m4_project_sd_set_mounted(sdReady);
    ESP_LOGI(TAG, "storage status: %s", sdReady ? "ready" : "unavailable");
    input.begin();
    display.begin();
    EpdiyHighlevelState projectDisplayState = {};
    m4_display_bridge_bind(&display, &projectDisplayState);
    ESP_LOGI(TAG, "shared project display bridge: framebuffer=%s logical=%dx%d",
             projectDisplayState.framebuffer ? "ready" : "unavailable",
             projectDisplayState.width, projectDisplayState.height);
    ESP_LOGI(TAG, "display initialized: controller=%s geometry=%ux%u",
             display.getControllerName(), display.getDisplayWidth(), display.getDisplayHeight());
    if (!input.reinitializeTouchAfterSharedReset()) {
        ESP_LOGE(TAG, "FT6336U initialization after shared panel reset failed");
    } else {
        ESP_LOGI(TAG, "FT6336U initialized after shared panel reset");
    }
    m4_input_bridge_bind(&input);
    frontlight.begin();
    frontlight.setBrightness(0);
    const bool rtcReady = rtc.begin();
    const auto batteryStatus = battery.readStatus();
    s_projectBattery = &battery;
    s_projectRtc = &rtc;
    const int64_t statusNow = esp_timer_get_time();
    s_projectBatteryReadAt = statusNow - 5000000;
    s_projectRtcReadAt = statusNow - 30000000;
    m4_project_platform_poll();
    ESP_LOGI(TAG, "RTC=%s battery=%s %u%% %umV charging=%d frontlight=%s",
             rtcReady ? "ready" : "unavailable", batteryStatus.supported ? "ready" : "unavailable",
             static_cast<unsigned>(batteryStatus.percentage),
             static_cast<unsigned>(batteryStatus.millivolts), batteryStatus.charging ? 1 : 0,
             frontlight.present() ? "ready" : "unavailable");

    /* Run the maintained project event loop on M4. The native FreeInk UI
       remains available as a diagnostic fallback, but is no longer the
       product entry point. */
    app_settings_init();
    app_setup_init();
    pico_boot_init(false);
    (void)app_font_activate_system();
    const app_loop_config_t projectLoopConfig = {
        .hl = &projectDisplayState,
        .fb = projectDisplayState.framebuffer,
        .acc = nullptr,
        .tp = nullptr,
        .sensor_ready = false,
        .first_app = app_home_page(),
    };
    app_loop_run(&projectLoopConfig);
}
