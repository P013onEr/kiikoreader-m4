#include "m4_ui.h"
#include "m4_app_backend.h"
#include "m4_reader.h"
#include "m4_project_reader.h"
#include "m4_display_bridge.h"

#include <FreeInkApp.h>
#include <FreeInkUI.h>
#include <FreeInkUIDisplayTarget.h>
#include <FreeInkUIBookFont.h>
#include "assets/ui_icons.h"
#include <BookArena.h>
#include <render/TtfFont.h>
extern "C" {
#include "settings.h"
#include "m4_reader_settings.h"
#include "ttf_font.h"
#include "book_epub.h"
#include "book_title.h"
#include "book_progress.h"
#include "book_added.h"
#include "book_store.h"
#include "book_cover.h"
#include "m4_gray2.h"
#include "m4_image_draw.h"
#include "m4_shelf_sort.h"
}


#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "driver/gpio.h"
#include "BoardConfig.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <cstring>
#include <cstdlib>
#include <dirent.h>
#include <vector>
#include <strings.h>
#include <sys/stat.h>

extern const uint8_t builtin_ttf_start[] asm("_binary_builtin_ttf_start");
extern const uint8_t builtin_ttf_end[] asm("_binary_builtin_ttf_end");

namespace {

using namespace freeink;
using namespace freeink::ui;

constexpr char TAG[] = "m4_ui";
constexpr int16_t PANEL_WIDTH = 800;
constexpr int16_t PANEL_HEIGHT = 480;
constexpr int16_t UI_WIDTH = 480;
constexpr int16_t UI_HEIGHT = 800;
constexpr size_t GRAY2_BYTES = static_cast<size_t>(PANEL_WIDTH) * PANEL_HEIGHT / 4u;
constexpr size_t UI_GRAY4_BYTES = static_cast<size_t>(UI_WIDTH) * UI_HEIGHT / 2u;
constexpr size_t UI_GRAY2_BYTES = static_cast<size_t>(UI_WIDTH) * UI_HEIGHT / 4u;
constexpr size_t PLANE_BYTES = static_cast<size_t>(PANEL_WIDTH) * PANEL_HEIGHT / 8u;
constexpr size_t FONT_ARENA_BYTES = 128u * 1024u;
constexpr size_t MAX_ENTRIES = 96;
constexpr uint16_t COVER_WIDTH = 120;
constexpr uint16_t COVER_HEIGHT = 156;
constexpr size_t COVER_GRAY_BYTES = static_cast<size_t>(COVER_WIDTH) * COVER_HEIGHT;
constexpr size_t COVER_GRAY2_BYTES = COVER_GRAY_BYTES / 4u;
constexpr ActionId ACTION_TAB = 1;
constexpr ActionId ACTION_HOME_SHELF = 2;
constexpr ActionId ACTION_HOME_FILES = 3;
constexpr ActionId ACTION_HOME_SETTINGS = 4;
constexpr ActionId ACTION_ENTRY = 5;
constexpr ActionId ACTION_BACK = 6;
constexpr ActionId ACTION_LIGHT_DOWN = 7;
constexpr ActionId ACTION_LIGHT_UP = 8;
constexpr ActionId ACTION_LIGHT_TOGGLE = 9;
constexpr ActionId ACTION_READER_PREVIOUS = 10;
constexpr ActionId ACTION_READER_NEXT = 11;
constexpr ActionId ACTION_SHELF_PREVIOUS = 12;
constexpr ActionId ACTION_SHELF_NEXT = 13;
constexpr ActionId ACTION_FONT_NEXT = 14;
constexpr ActionId ACTION_IMAGE_PREVIOUS = 15;
constexpr ActionId ACTION_IMAGE_NEXT = 16;
constexpr ActionId ACTION_READER_TOC = 17;
constexpr ActionId ACTION_TOC_SELECT = 18;
constexpr ActionId ACTION_TOC_PREVIOUS = 19;
constexpr ActionId ACTION_TOC_NEXT = 20;
constexpr ActionId ACTION_READER_SETTINGS = 21;
constexpr ActionId ACTION_FONT_DOWN = 22;
constexpr ActionId ACTION_FONT_UP = 23;
constexpr ActionId ACTION_READER_VERTICAL = 24;
constexpr ActionId ACTION_READER_IMAGES = 25;
constexpr ActionId ACTION_LINE_DOWN = 26;
constexpr ActionId ACTION_LINE_UP = 27;
constexpr ActionId ACTION_READER_BOOK_FONTS = 28;
constexpr ActionId ACTION_SHELF_SORT = 29;
constexpr ActionId ACTION_READER_FAVORITE = 30;
constexpr ActionId ACTION_SETTINGS_ROW = 32;
constexpr ActionId ACTION_TIME_FIELD_UP = 33;
constexpr ActionId ACTION_TIME_FIELD_DOWN = 34;
constexpr ActionId ACTION_TIME_SAVE = 35;
constexpr ActionId ACTION_TIME_CANCEL = 36;
constexpr ActionId ACTION_OPEN_READER = 37;
constexpr size_t SHELF_PAGE_SIZE = 4;
constexpr size_t TOC_PAGE_SIZE = 8;
constexpr uint8_t SHELF_SORT_NAME_ASC = M4_SHELF_SORT_NAME_ASC;
constexpr uint8_t SHELF_SORT_NAME_DESC = M4_SHELF_SORT_NAME_DESC;
constexpr uint8_t SHELF_SORT_RECENT = M4_SHELF_SORT_RECENT;
constexpr uint8_t SHELF_SORT_ADDED = M4_SHELF_SORT_ADDED;

enum class Page : uint8_t { Home, Shelf, Files, Settings, BookDetail, Reader, ImageViewer, ReaderToc, ReaderSettings, TimeSettings };

struct Entry {
    char path[192]{};
    char title[96]{};
    char author[64]{};
    char subtitle[48]{};
    bool directory = false;
    bool metadataReady = false;
    bool hasProgress = false;
    uint8_t progress = 0;
    uint32_t size = 0;
    uint32_t lastRead = 0;
    uint64_t addedOrder = 0;
    bool favorite = false;
};

struct UiState {
    FreeInkDisplay* display = nullptr;
    InputManager* input = nullptr;
    FrontlightManager* frontlight = nullptr;
    BatteryMonitor* battery = nullptr;
    Rtc* rtc = nullptr;
    uint8_t* gray = nullptr;
    uint8_t* imageGray4 = nullptr;
    uint8_t* imageGray2 = nullptr;
    uint8_t* lsb = nullptr;
    uint8_t* msb = nullptr;
    uint8_t* fontMemory = nullptr;
    freeink::book::Arena fontArena;
    freeink::book::TtfFont font;
    TtfGlyphSource glyphSource;
    std::unique_ptr<FreeInkApp<32>> app;
    Entry entries[MAX_ENTRIES]{};
    ListItem listItems[MAX_ENTRIES]{};
    ListItem settingsItems[10]{};
    uint8_t* coverGray = nullptr;
    uint8_t* coverGray2 = nullptr;
    char coverPaths[SHELF_PAGE_SIZE][192]{};
    bool coverValid[SHELF_PAGE_SIZE]{};
    SemaphoreHandle_t coverMutex = nullptr;
    std::atomic<uint32_t> coverGeneration{0};
    std::atomic<uint32_t> coverFirst{0};
    std::atomic<bool> coverChanged{false};
    std::atomic<bool> shelfEntriesDirty{true};
    std::atomic<bool> fileEntriesDirty{true};
    std::atomic<bool> shelfScanRunning{false};
    std::atomic<bool> shelfScanWorker{false};
    std::atomic<bool> shelfScanDone{false};
    TaskHandle_t shelfScanTask = nullptr;
    TaskHandle_t coverTask = nullptr;
    size_t entryCount = 0;
    bool entriesTruncated = false;
    size_t shelfPage = 0;
    size_t tocPage = 0;
    bool storageReady = false;
    Page page = Page::Home;
    Page detailReturnPage = Page::Shelf;
    char currentDirectory[192] = "/sdcard";
    char selectedPath[192]{};
    char selectedTitle[96]{};
    char readerError[128]{};
    uint32_t selectedSize = 0;
    size_t imageEntryIndex = 0;
    uint16_t imageWidth = 0;
    uint16_t imageHeight = 0;
    uint8_t brightness = 0;
    uint8_t shelfSort = SHELF_SORT_NAME_ASC;
    bool frontlightOn = false;
    BatteryMonitor::Status batteryStatus{};
    Rtc::DateTime clockTime{};
    Rtc::DateTime timeEdit{};
    bool clockValid = false;
    uint32_t deviceStatusUpdatedAt = 0;
    nvs_handle_t settings = 0;
    bool settingsReady = false;
    bool shelfCacheAttempted = false;
};

static UiState s_ui;

static void favoriteKey(const char* path, char key[11]) {
    uint32_t hash = 2166136261u;
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(path); p && *p; ++p)
        hash = (hash ^ *p) * 16777619u;
    std::snprintf(key, 11, "f_%08lx", static_cast<unsigned long>(hash));
}

static bool favoriteRead(const char* path) {
    if (!path || !path[0]) return false;
    nvs_handle_t handle = 0;
    if (nvs_open("rp_favs", NVS_READONLY, &handle) != ESP_OK) return false;
    char key[11]; favoriteKey(path, key);
    uint8_t value = 0;
    const bool result = nvs_get_u8(handle, key, &value) == ESP_OK && value == 1;
    nvs_close(handle);
    return result;
}

static bool shelfHiddenRead(const char* path) {
    if (!path || !path[0]) return false;
    nvs_handle_t handle = 0;
    if (nvs_open("rp_shelf", NVS_READONLY, &handle) != ESP_OK) return false;
    uint32_t hash = 2166136261u;
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(path); *p; ++p)
        hash = (hash ^ *p) * 16777619u;
    char key[11];
    std::snprintf(key, sizeof(key), "h_%08lx", static_cast<unsigned long>(hash));
    char stored[288]{};
    size_t length = sizeof(stored);
    const bool hidden = nvs_get_str(handle, key, stored, &length) == ESP_OK &&
                        std::strcmp(stored, path) == 0;
    nvs_close(handle);
    return hidden;
}

static bool favoriteWrite(const char* path, const bool value) {
    if (!path || !path[0]) return false;
    nvs_handle_t handle = 0;
    if (nvs_open("rp_favs", NVS_READWRITE, &handle) != ESP_OK) return false;
    char key[11]; favoriteKey(path, key);
    esp_err_t error = value ? nvs_set_u8(handle, key, 1) : nvs_erase_key(handle, key);
    if (error == ESP_ERR_NVS_NOT_FOUND && !value) error = ESP_OK;
    if (error == ESP_OK) error = nvs_commit(handle);
    nvs_close(handle);
    return error == ESP_OK;
}

static bool endsWithEpub(const char* name) {
    const size_t n = std::strlen(name);
    return n > 5 && strcasecmp(name + n - 5, ".epub") == 0;
}

static bool endsWithBook(const char* name) {
    if (!name) return false;
    const size_t n = std::strlen(name);
    return (n > 5 && strcasecmp(name + n - 5, ".epub") == 0) ||
           (n > 4 && strcasecmp(name + n - 4, ".txt") == 0);
}

static bool imageFormat(const char* path, bool* png) {
    const char* dot = path ? std::strrchr(path, '.') : nullptr;
    if (!dot) return false;
    if (!strcasecmp(dot, ".png")) {
        if (png) *png = true;
        return true;
    }
    if (!strcasecmp(dot, ".jpg") || !strcasecmp(dot, ".jpeg") || !strcasecmp(dot, ".webp")) {
        if (png) *png = false;
        return true;
    }
    return false;
}

static bool loadImageAt(const size_t index) {
    if (index >= s_ui.entryCount || s_ui.entries[index].directory ||
        !s_ui.imageGray4 || !s_ui.imageGray2) return false;
    const Entry entry = s_ui.entries[index];
    bool png = false;
    if (!imageFormat(entry.path, &png)) return false;
    unsigned sourceWidth = 0, sourceHeight = 0;
    uint16_t width = 0, height = 0;
    constexpr uint16_t kImageAreaHeight = 680;
    if (!book_image_file_dimensions(entry.path, png, &sourceWidth, &sourceHeight) ||
        !m4ProjectFitImage(sourceWidth, sourceHeight, UI_WIDTH, kImageAreaHeight,
                           &width, &height)) return false;
    const size_t imageBytes = static_cast<size_t>(width) * height;
    auto* gray = static_cast<uint8_t*>(heap_caps_malloc(imageBytes, MALLOC_CAP_SPIRAM));
    if (!gray) return false;
    const bool decoded = book_image_file_grayscale(entry.path, png, width, height, gray);
    if (!decoded) {
        heap_caps_free(gray);
        return false;
    }
    std::memset(s_ui.imageGray4, 0xff, UI_GRAY4_BYTES);
    const int32_t x = (UI_WIDTH - width) / 2;
    const int32_t y = 48 + (kImageAreaHeight - height) / 2;
    const bool drawn = m4ProjectBlitGray8ToGray4(s_ui.imageGray4, UI_GRAY4_BYTES,
        UI_WIDTH, UI_HEIGHT, gray, imageBytes, width, height, x, y);
    heap_caps_free(gray);
    if (!drawn || !m4ProjectGray4ToGray2(s_ui.imageGray4, UI_GRAY4_BYTES,
            s_ui.imageGray2, UI_GRAY2_BYTES, UI_WIDTH, UI_HEIGHT)) return false;
    s_ui.imageEntryIndex = index;
    s_ui.imageWidth = width;
    s_ui.imageHeight = height;
    std::snprintf(s_ui.selectedPath, sizeof(s_ui.selectedPath), "%s", entry.path);
    std::snprintf(s_ui.selectedTitle, sizeof(s_ui.selectedTitle), "%s", entry.title);
    s_ui.selectedSize = entry.size;
    return true;
}

static bool moveImage(const int direction) {
    if (!s_ui.entryCount || (direction != -1 && direction != 1)) return false;
    size_t candidate = s_ui.imageEntryIndex;
    for (size_t checked = 0; checked < s_ui.entryCount; ++checked) {
        if (direction < 0) {
            if (candidate == 0) return false;
            --candidate;
        } else {
            if (++candidate >= s_ui.entryCount) return false;
        }
        bool png = false;
        if (imageFormat(s_ui.entries[candidate].path, &png) &&
            !s_ui.entries[candidate].directory && loadImageAt(candidate)) return true;
    }
    return false;
}

static bool skippedDirectory(const char* name) {
    return name[0] == '.' || !strcasecmp(name, "System Volume Information") ||
           !strcasecmp(name, "$RECYCLE.BIN");
}

static void titleFromPath(const char* path, char* out, size_t capacity) {
    const char* name = std::strrchr(path, '/');
    name = name ? name + 1 : path;
    const size_t n = std::strlen(name);
    size_t length = n;
    if (endsWithEpub(name)) length -= 5;
    else if (n > 4 && !strcasecmp(name + n - 4, ".txt")) length -= 4;
    if (length >= capacity) length = capacity - 1;
    std::memcpy(out, name, length);
    out[length] = '\0';
}

static void appendEntry(const char* path, const char* name, bool directory, uint32_t size) {
    if (s_ui.entryCount >= MAX_ENTRIES) {
        s_ui.entriesTruncated = true;
        return;
    }
    Entry& entry = s_ui.entries[s_ui.entryCount++];
    std::snprintf(entry.path, sizeof(entry.path), "%s", path);
    titleFromPath(name, entry.title, sizeof(entry.title));
    entry.directory = directory;
    entry.size = size;
    if (directory) {
        std::snprintf(entry.subtitle, sizeof(entry.subtitle), "文件夹");
    } else {
        const char* dot = std::strrchr(name, '.');
        std::snprintf(entry.subtitle, sizeof(entry.subtitle), "%s  %.1f MB",
                      dot ? dot + 1 : "文件", static_cast<double>(size) / (1024.0 * 1024.0));
    }
}

struct ShelfCacheHeader { uint32_t magic, version, count, checksum; };
struct ShelfCacheEntry {
    char path[288];
    char name[256];
    char author[128];
    uint32_t size, recent;
    uint64_t added;
    uint16_t chapter;
    uint8_t pct, flags;
};

struct ShelfBackfillContext { size_t added = 0; };

static bool shelfBackfillVisit(const char* path, const book_progress_t* progress, void* context) {
    if (!path || !progress || !endsWithBook(path)) return true;
    ShelfBackfillContext* state = static_cast<ShelfBackfillContext*>(context);
    for (size_t i = 0; i < s_ui.entryCount; ++i)
        if (std::strcmp(s_ui.entries[i].path, path) == 0) return true;
    struct stat st{};
    if (::stat(path, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size < 0 ||
        static_cast<uint64_t>(st.st_size) > UINT32_MAX ||
        static_cast<uint32_t>(st.st_size) != progress->file_size || shelfHiddenRead(path)) return true;
    if (s_ui.entryCount >= MAX_ENTRIES) {
        s_ui.entriesTruncated = true;
        return false;
    }
    const char* base = std::strrchr(path, '/');
    base = base ? base + 1 : path;
    appendEntry(path, base, false, static_cast<uint32_t>(st.st_size));
    Entry& entry = s_ui.entries[s_ui.entryCount - 1];
    char title[sizeof(entry.title)]{};
    char author[sizeof(entry.author)]{};
    if (book_name_from_path(path, title, sizeof(title), author, sizeof(author)) && title[0]) {
        std::snprintf(entry.title, sizeof(entry.title), "%s", title);
        std::snprintf(entry.author, sizeof(entry.author), "%s", author);
        entry.metadataReady = true;
    }
    entry.favorite = favoriteRead(path);
    entry.hasProgress = true;
    entry.progress = progress->pct;
    entry.lastRead = progress->last_open_s;
    ++state->added;
    return true;
}

static uint32_t shelfCacheHash(const void* data, size_t size) {
    const auto* bytes = static_cast<const uint8_t*>(data);
    uint32_t hash = UINT32_C(2166136261);
    while (size--) hash = (hash ^ *bytes++) * UINT32_C(16777619);
    return hash;
}

static bool loadProjectShelfCache() {
    FILE* file = std::fopen("/sdcard/.kiikoread/shelf.idx", "rb");
    if (!file) return false;
    ShelfCacheHeader header{};
    bool ok = std::fread(&header, 1, sizeof(header), file) == sizeof(header) &&
              header.magic == UINT32_C(0x4B534853) && header.version == 1 &&
              header.count <= 4096;
    uint32_t checksum = 0;
    s_ui.entryCount = 0;
    for (uint32_t i = 0; ok && i < header.count; ++i) {
        ShelfCacheEntry cached{};
        if (std::fread(&cached, 1, sizeof(cached), file) != sizeof(cached)) {
            ok = false;
            break;
        }
        checksum = (checksum ^ shelfCacheHash(&cached, sizeof(cached))) * UINT32_C(16777619);
        struct stat st{};
        if (!cached.path[0] || std::strlen(cached.path) >= sizeof(s_ui.entries[0].path) ||
            !endsWithBook(cached.path) ||
            stat(cached.path, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size < 0 ||
            static_cast<uint64_t>(st.st_size) != cached.size ||
            s_ui.entryCount >= MAX_ENTRIES) continue;
        Entry& entry = s_ui.entries[s_ui.entryCount++];
        const auto copyCached = [](char* dst, size_t cap, const char* src, size_t srcCap) {
            size_t n = 0;
            while (n + 1 < cap && n < srcCap && src[n]) ++n;
            std::memcpy(dst, src, n);
            dst[n] = '\0';
        };
        copyCached(entry.path, sizeof(entry.path), cached.path, sizeof(cached.path));
        copyCached(entry.title, sizeof(entry.title), cached.name, sizeof(cached.name));
        copyCached(entry.author, sizeof(entry.author), cached.author, sizeof(cached.author));
        entry.size = cached.size;
        entry.lastRead = cached.recent;
        entry.addedOrder = cached.added;
        entry.progress = cached.pct;
        entry.hasProgress = (cached.flags & 2u) != 0;
        entry.favorite = (cached.flags & 4u) != 0;
        entry.metadataReady = true;
    }
    std::fclose(file);
    if (!ok || (header.count && checksum != header.checksum) || !s_ui.entryCount) {
        s_ui.entryCount = 0;
        return false;
    }
    return true;
}

static void sortShelfEntries() {
    std::sort(s_ui.entries, s_ui.entries + s_ui.entryCount,
              [](const Entry& a, const Entry& b) {
                  const m4_shelf_sort_key_t left{a.title, a.path, a.lastRead,
                                                 a.addedOrder, a.directory, a.favorite};
                  const m4_shelf_sort_key_t right{b.title, b.path, b.lastRead,
                                                  b.addedOrder, b.directory, b.favorite};
                  return m4_shelf_sort_compare(&left, &right,
                      static_cast<m4_shelf_sort_t>(s_ui.shelfSort)) < 0;
              });
    for (size_t i = 0; i < s_ui.entryCount; ++i) {
        s_ui.listItems[i].label = s_ui.entries[i].title;
        s_ui.listItems[i].subtitle = s_ui.entries[i].author[0] ? s_ui.entries[i].author : s_ui.entries[i].subtitle;
        s_ui.listItems[i].actionValue = static_cast<int16_t>(i);
        s_ui.listItems[i].enabled = true;
    }
}

static void rebuildEntries(const bool files);

static void shelfRebuildWorker(void*) {
    s_ui.shelfScanWorker.store(true, std::memory_order_release);
    rebuildEntries(false);
    s_ui.shelfScanWorker.store(false, std::memory_order_release);
    s_ui.shelfScanRunning.store(false, std::memory_order_release);
    s_ui.shelfScanDone.store(true, std::memory_order_release);
    s_ui.shelfScanTask = nullptr;
    vTaskDelete(nullptr);
}

static void scanEpubs(const char* path, unsigned depth) {
    // Keep the maintained shelf traversal depth and path rules; this function
    // only converts discovered records into the FreeInk Entry representation.
    if (depth > 12 || s_ui.entryCount >= MAX_ENTRIES) return;
    DIR* dir = opendir(path);
    if (!dir) return;
    struct dirent* item;
    while ((item = readdir(dir)) != nullptr) {
        if (!std::strcmp(item->d_name, ".") || !std::strcmp(item->d_name, "..") ||
            skippedDirectory(item->d_name)) continue;
        char child[256];
        const int written = std::snprintf(child, sizeof(child), "%s/%s", path, item->d_name);
        if (written <= 0 || static_cast<size_t>(written) >= sizeof(child)) continue;
        struct stat st{};
        if (stat(child, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            scanEpubs(child, depth + 1);
        } else if (S_ISREG(st.st_mode) && endsWithBook(item->d_name)) {
            if (shelfHiddenRead(child)) continue;
            appendEntry(child, item->d_name, false,
                        st.st_size > 0 ? static_cast<uint32_t>(st.st_size) : 0);
        }
        if (s_ui.entryCount >= MAX_ENTRIES) {
            s_ui.entriesTruncated = true;
            break;
        }
    }
    closedir(dir);
}

static void scanFiles(const char* path, unsigned depth) {
    if (depth > 4 || s_ui.entryCount >= MAX_ENTRIES) return;
    DIR* dir = opendir(path);
    if (!dir) return;
    struct dirent* item;
    while ((item = readdir(dir)) != nullptr) {
        if (!std::strcmp(item->d_name, ".") || !std::strcmp(item->d_name, "..") ||
            skippedDirectory(item->d_name)) continue;
        char child[256];
        const int written = std::snprintf(child, sizeof(child), "%s/%s", path, item->d_name);
        if (written <= 0 || static_cast<size_t>(written) >= sizeof(child)) continue;
        struct stat st{};
        if (stat(child, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            appendEntry(child, item->d_name, true, 0);
        } else if (S_ISREG(st.st_mode)) {
            appendEntry(child, item->d_name, false,
                        st.st_size > 0 ? static_cast<uint32_t>(st.st_size) : 0);
        }
        if (s_ui.entryCount >= MAX_ENTRIES) {
            s_ui.entriesTruncated = true;
            break;
        }
    }
    closedir(dir);
}

static void rebuildEntries(const bool files) {
    std::atomic<bool>& dirty = files ? s_ui.fileEntriesDirty : s_ui.shelfEntriesDirty;
    if (!dirty.load(std::memory_order_acquire)) return;
    if (!files && !s_ui.shelfScanWorker.load(std::memory_order_acquire)) {
        bool expected = false;
        if (!s_ui.shelfScanRunning.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) return;
        if (xTaskCreate(shelfRebuildWorker, "m4_shelf_scan", 8192, nullptr, 1,
                        &s_ui.shelfScanTask) != pdPASS) {
            s_ui.shelfScanRunning.store(false, std::memory_order_release);
            ESP_LOGE(TAG, "cannot start shelf scan worker");
        }
        return;
    }
    s_ui.entryCount = 0;
    s_ui.entriesTruncated = false;
    if (!s_ui.storageReady) {
        dirty.store(false, std::memory_order_release);
        return;
    }
    if (!files && !s_ui.shelfCacheAttempted) {
        s_ui.shelfCacheAttempted = true;
        if (loadProjectShelfCache()) {
            sortShelfEntries();
            dirty.store(false, std::memory_order_release);
            ESP_LOGI(TAG, "shelf index restored: %u books",
                     static_cast<unsigned>(s_ui.entryCount));
            return;
        }
    }
    if (files) scanFiles(s_ui.currentDirectory, 0);
    else {
        book_store_root_t roots[BOOK_STORE_ROOT_MAX]{};
        int rootCount = 0;
        const esp_err_t rootsError = book_store_roots(roots, &rootCount);
        if (rootsError != ESP_OK || rootCount == 0) {
            ESP_LOGW(TAG, "book roots unavailable: %s", esp_err_to_name(rootsError));
        } else {
            for (int i = 0; i < rootCount && s_ui.entryCount < MAX_ENTRIES; ++i)
                scanEpubs(roots[i].path, 0);
        }
    }
    if (!files) {
        for (size_t i = 0; i < s_ui.entryCount; ++i) {
            Entry& entry = s_ui.entries[i];
            char title[sizeof(entry.title)]{};
            char author[sizeof(entry.author)]{};
            if (endsWithEpub(entry.path) &&
                book_epub_metadata_cached(entry.path, title, sizeof(title),
                                          author, sizeof(author)) == ESP_OK && title[0]) {
                std::snprintf(entry.title, sizeof(entry.title), "%s", title);
                std::snprintf(entry.author, sizeof(entry.author), "%s", author);
                entry.metadataReady = true;
            } else if (endsWithBook(entry.path)) {
                (void)book_name_from_path(entry.path, title, sizeof(title), author, sizeof(author));
                if (title[0]) {
                    std::snprintf(entry.title, sizeof(entry.title), "%s", title);
                    std::snprintf(entry.author, sizeof(entry.author), "%s", author);
                    entry.metadataReady = true;
                }
            }
            entry.favorite = favoriteRead(entry.path);
        }
        std::vector<book_progress_query_t> queries(s_ui.entryCount);
        std::vector<book_added_item_t> added(s_ui.entryCount);
        for (size_t i = 0; i < s_ui.entryCount; ++i) {
            queries[i] = {s_ui.entries[i].path, s_ui.entries[i].size, {}, false};
            added[i] = {s_ui.entries[i].path, &s_ui.entries[i].addedOrder};
        }
        if (!queries.empty() && book_progress_load_batch(queries.data(), queries.size())) {
            for (size_t i = 0; i < queries.size(); ++i) {
                if (queries[i].found) {
                    s_ui.entries[i].lastRead = queries[i].progress.last_open_s;
                    s_ui.entries[i].hasProgress = true;
                    s_ui.entries[i].progress = queries[i].progress.pct;
                }
            }
        }
        if (!added.empty()) book_added_load(added.data(), added.size());
        ShelfBackfillContext backfill{};
        (void)book_progress_list(shelfBackfillVisit, &backfill);
        if (backfill.added) {
            std::vector<book_added_item_t> backfilled;
            backfilled.reserve(backfill.added);
            for (size_t i = 0; i < s_ui.entryCount; ++i)
                if (s_ui.entries[i].addedOrder == 0)
                    backfilled.push_back({s_ui.entries[i].path, &s_ui.entries[i].addedOrder});
            if (!backfilled.empty()) book_added_load(backfilled.data(), backfilled.size());
        }
    }
    sortShelfEntries();
    dirty.store(false, std::memory_order_release);
}

static void packCoverGray2(const uint8_t* gray, uint8_t* packed) {
    std::memset(packed, 0, COVER_GRAY2_BYTES);
    for (uint16_t y = 0; y < COVER_HEIGHT; ++y) {
        for (uint16_t x = 0; x < COVER_WIDTH; ++x) {
            const size_t pixel = static_cast<size_t>(y) * COVER_WIDTH + x;
            const uint8_t level = static_cast<uint8_t>(gray[pixel] >> 6);
            packed[pixel / 4u] |= static_cast<uint8_t>(level << (6u - (pixel & 3u) * 2u));
        }
    }
}

static void shelfCoverWorker(void*) {
    uint8_t* gray = nullptr;
    uint8_t* projectGray = nullptr;
    uint8_t* packed = nullptr;
    for (;;) {
        (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (gray == nullptr || projectGray == nullptr || packed == nullptr) {
            gray = static_cast<uint8_t*>(heap_caps_malloc(COVER_GRAY_BYTES, MALLOC_CAP_SPIRAM));
            projectGray = static_cast<uint8_t*>(heap_caps_malloc(BOOK_COVER_W * BOOK_COVER_H, MALLOC_CAP_SPIRAM));
            packed = static_cast<uint8_t*>(heap_caps_malloc(COVER_GRAY2_BYTES, MALLOC_CAP_SPIRAM));
            if (gray == nullptr || projectGray == nullptr || packed == nullptr) {
                heap_caps_free(gray);
                heap_caps_free(projectGray);
                heap_caps_free(packed);
                gray = projectGray = packed = nullptr;
                ESP_LOGE(TAG, "shelf cover worker allocation failed");
                continue;
            }
        }
        const uint32_t generation = s_ui.coverGeneration.load(std::memory_order_acquire);
        const size_t first = s_ui.coverFirst.load(std::memory_order_acquire);
        const size_t count = s_ui.entryCount;
        for (size_t slot = 0; slot < SHELF_PAGE_SIZE && first + slot < count; ++slot) {
            if (generation != s_ui.coverGeneration.load(std::memory_order_acquire)) break;
            const Entry entry = s_ui.entries[first + slot];
            // Cached shelf indexes from older builds may contain arbitrary files.
            // Only EPUB entries are valid inputs to the cover extractor.
            if (!endsWithEpub(entry.path)) continue;
            char title[96]{};
            char author[64]{};
            book_progress_t progress{};
            const bool metadataReady = book_epub_metadata(entry.path, title, sizeof(title),
                                                          author, sizeof(author)) == ESP_OK;
            const bool hasProgress = book_progress_load(entry.path, entry.size, &progress);
            if (metadataReady && xSemaphoreTake(s_ui.coverMutex, portMAX_DELAY) == pdTRUE) {
                if (generation == s_ui.coverGeneration.load(std::memory_order_acquire) &&
                    first + slot < s_ui.entryCount &&
                    std::strcmp(s_ui.entries[first + slot].path, entry.path) == 0) {
                    Entry& current = s_ui.entries[first + slot];
                    std::snprintf(current.title, sizeof(current.title), "%s", title);
                    std::snprintf(current.author, sizeof(current.author), "%s", author);
                    current.metadataReady = true;
                    current.hasProgress = hasProgress;
                    current.progress = hasProgress ? progress.pct : 0;
                    s_ui.coverChanged.store(true, std::memory_order_release);
                }
                xSemaphoreGive(s_ui.coverMutex);
            }
            bool ready = false;
            bool pending = false;
            unsigned sourceWidth = BOOK_COVER_W, sourceHeight = BOOK_COVER_H;
            if (book_cover_load_list_gray(entry.path, title, author, projectGray, true,
                                          &pending, &sourceWidth, &sourceHeight) &&
                sourceWidth > 0 && sourceHeight > 0 && sourceWidth <= BOOK_COVER_W &&
                sourceHeight <= BOOK_COVER_H) {
                const book_crop_t crop = book_cover_crop(sourceWidth, sourceHeight,
                                                          COVER_WIDTH, COVER_HEIGHT);
                for (uint16_t y = 0; y < COVER_HEIGHT; ++y) {
                    const unsigned sy = crop.y + static_cast<uint64_t>(y) * crop.height / COVER_HEIGHT;
                    for (uint16_t x = 0; x < COVER_WIDTH; ++x) {
                        const unsigned sx = crop.x + static_cast<uint64_t>(x) * crop.width / COVER_WIDTH;
                        gray[static_cast<size_t>(y) * COVER_WIDTH + x] =
                            projectGray[static_cast<size_t>(sy) * sourceWidth + sx];
                    }
                }
                ready = true;
            }
            if (!ready) continue;
            packCoverGray2(gray, packed);
            if (generation != s_ui.coverGeneration.load(std::memory_order_acquire)) break;
            if (xSemaphoreTake(s_ui.coverMutex, portMAX_DELAY) == pdTRUE) {
                if (generation == s_ui.coverGeneration.load(std::memory_order_acquire) &&
                    slot < SHELF_PAGE_SIZE && first + slot < s_ui.entryCount &&
                    std::strcmp(s_ui.entries[first + slot].path, entry.path) == 0) {
                    std::memcpy(s_ui.coverGray + slot * COVER_GRAY_BYTES, gray, COVER_GRAY_BYTES);
                    std::memcpy(s_ui.coverGray2 + slot * COVER_GRAY2_BYTES, packed, COVER_GRAY2_BYTES);
                    std::snprintf(s_ui.coverPaths[slot], sizeof(s_ui.coverPaths[slot]), "%s", entry.path);
                    s_ui.coverValid[slot] = true;
                    s_ui.coverChanged.store(true, std::memory_order_release);
                }
                xSemaphoreGive(s_ui.coverMutex);
            }
        }
    }
}

static void scheduleShelfCovers() {
    if (s_ui.page != Page::Shelf || s_ui.shelfScanRunning.load(std::memory_order_acquire) ||
        !s_ui.coverGray || !s_ui.coverGray2 || !s_ui.coverMutex) return;
    s_ui.coverFirst.store(static_cast<uint32_t>(s_ui.shelfPage * SHELF_PAGE_SIZE), std::memory_order_release);
    s_ui.coverGeneration.fetch_add(1, std::memory_order_acq_rel);
    if (xSemaphoreTake(s_ui.coverMutex, portMAX_DELAY) == pdTRUE) {
        std::memset(s_ui.coverValid, 0, sizeof(s_ui.coverValid));
        std::memset(s_ui.coverPaths, 0, sizeof(s_ui.coverPaths));
        xSemaphoreGive(s_ui.coverMutex);
    }
    if (s_ui.coverTask != nullptr) {
        xTaskNotifyGive(s_ui.coverTask);
    } else if (xTaskCreate(shelfCoverWorker, "m4_covers", 12288, nullptr, 1,
                           &s_ui.coverTask) != pdPASS) {
        s_ui.coverTask = nullptr;
        ESP_LOGE(TAG, "cannot start shelf cover worker");
    }
}

static bool enterDirectory(const char* path) {
    const int written = std::snprintf(s_ui.currentDirectory, sizeof(s_ui.currentDirectory), "%s", path);
    if (written > 0 && static_cast<size_t>(written) < sizeof(s_ui.currentDirectory))
        s_ui.fileEntriesDirty.store(true, std::memory_order_release);
    return written > 0 && static_cast<size_t>(written) < sizeof(s_ui.currentDirectory);
}

static bool enterParentDirectory() {
    char parent[sizeof(s_ui.currentDirectory)];
    std::snprintf(parent, sizeof(parent), "%s", s_ui.currentDirectory);
    char* slash = std::strrchr(parent, '/');
    if (!slash || slash == parent) return enterDirectory("/sdcard");
    *slash = '\0';
    if (std::strcmp(parent, "/sdcard") != 0 && std::strncmp(parent, "/sdcard/", 8) != 0)
        return enterDirectory("/sdcard");
    return enterDirectory(parent);
}

static void drawTabBar(Screen<32>& screen) {
    static constexpr TabItem tabs[] = {
        {"首页", {}, {}, 0, false, true}, {"书架", {}, {}, 1, false, true},
        {"文件管理", {}, {}, 2, false, true}, {"设置", {}, {}, 3, false, true},
    };
    TabItem current[4];
    std::copy(std::begin(tabs), std::end(tabs), current);
    const int selected = s_ui.page == Page::Home ? 0 : s_ui.page == Page::Shelf ? 1 :
                         s_ui.page == Page::Files || s_ui.page == Page::ImageViewer ? 2 : 3;
    for (int i = 0; i < 4; ++i) current[i].selected = selected == i;
    TabBarProps props;
    props.tabs = current;
    props.count = 4;
    props.action = ACTION_TAB;
    props.text = screen.theme().smallText;
    props.selectedText = screen.theme().smallText;
    props.selectedText.bold = true;
    props.tabInset = Insets{3, 3, 3, 3};
    props.minTouchSize = 48;
    // The project nav uses a 44 px native icon in a 120 px footer. Preserve
    // that visual ratio after mapping its 684x1216 layout onto M4's 480x800.
    props.iconSize = 32;
    props.selectedUnderline = 4;
    props.layout = TabBarLayout::EqualWidth;
    props.selectedUnderlinePaint = Paint::solid(Color::Black);
    props.tabStyles.normal.background = Paint::none();
    props.tabStyles.normal.border = Paint::none();
    props.tabStyles.selected.background = Paint::none();
    props.tabStyles.selected.border = Paint::none();
    props.iconPainter = [](DrawTarget& target, Rect rect, const TabItem& tab, uint8_t, void*) {
        static constexpr ui_icon_t icons[] = {
            UI_ICON_HOUSE, UI_ICON_LIBRARY_BIG, UI_ICON_FOLDER_OPEN,
            UI_ICON_SLIDERS_HORIZONTAL};
        if (tab.value < 0 || tab.value >= 4) return false;
        const uint8_t *mask = ui_icon_masks[icons[tab.value]];
        const int size = std::min<int>(rect.width, rect.height);
        const int left = rect.x + (rect.width - size) / 2;
        const int top = rect.y + (rect.height - size) / 2;
        const Paint dark = Paint::solid(tab.selected ? Color::Black : Color::DarkGray);
        for (int y = 0; y < size; ++y) {
            const int sy = std::min(71, (y * UI_ICON_MASK_SIZE) / size);
            for (int x = 0; x < size; ++x) {
                const int sx = std::min(71, (x * UI_ICON_MASK_SIZE) / size);
                const uint8_t packed = mask[(sy * UI_ICON_MASK_SIZE + sx) / 2];
                const uint8_t alpha = (sx & 1) ? (packed & 0x0f) : (packed >> 4);
                if (alpha >= 8)
                    target.fill(Rect{static_cast<int16_t>(left + x), static_cast<int16_t>(top + y), 1, 1}, dark);
            }
        }
        return true;
    };
    const Rect bar = screen.takeBottom(78);
    screen.target().fill(bar, Paint::solid(Color::LightGray));
    screen.target().line(Point{bar.x, bar.y}, Point{bar.right(), bar.y}, 1,
                         Paint::solid(Color::DarkGray));
    ui::tabBar(screen.frame(), bar, props);
    const int16_t active = static_cast<int16_t>(selected);
    const int16_t slotW = static_cast<int16_t>(bar.width / 4);
    const int16_t activeX = static_cast<int16_t>(bar.x + active * slotW + (slotW - 36) / 2);
    screen.target().fill(Rect{activeX, static_cast<int16_t>(bar.y + 1), 36, 3},
                         Paint::solid(Color::Black));
}

static void drawTopStatus(Screen<32>& screen) {
    const Rect status = screen.takeTop(28);
    screen.target().fill(status, Paint::solid(Color::White));
    TextStyle style = screen.theme().smallText;
    style.color = Color::DarkGray;
    screen.target().text(Rect{12, status.y, 92, status.height}, "kiikoread", style);
    char timeLabel[20];
    if (s_ui.clockValid) {
        std::snprintf(timeLabel, sizeof(timeLabel), "%02u:%02u",
                      static_cast<unsigned>(s_ui.clockTime.hour),
                      static_cast<unsigned>(s_ui.clockTime.minute));
    } else {
        std::snprintf(timeLabel, sizeof(timeLabel), "时间未设");
    }
    screen.target().text(Rect{108, status.y, 72, status.height}, timeLabel, style);
    char batteryLabel[28];
    if (s_ui.batteryStatus.supported && s_ui.batteryStatus.percentageKnown) {
        std::snprintf(batteryLabel, sizeof(batteryLabel), "%u%%%s",
                      static_cast<unsigned>(s_ui.batteryStatus.percentage),
                      s_ui.batteryStatus.chargingKnown && s_ui.batteryStatus.charging ? " 充电" : "");
    } else {
        std::snprintf(batteryLabel, sizeof(batteryLabel), "电量未知");
    }
    screen.target().text(Rect{184, status.y, 112, status.height}, batteryLabel, style);
    style.align = TextAlign::Right;
    screen.target().text(Rect{300, status.y, 168, status.height},
                         s_ui.storageReady ? "TF 已连接" : "TF 未连接", style);
    screen.target().line(Point{12, static_cast<int16_t>(status.bottom() - 1)},
                         Point{468, static_cast<int16_t>(status.bottom() - 1)}, 1,
                         Paint::solid(Color::LightGray));
}

static void drawHome(Screen<32>& screen, void*) {
    // The project root uses a light gray canvas with white content modules.
    screen.target().fill(screen.contentRect(), Paint::solid(Color::LightGray));
    drawTopStatus(screen);
    HeaderProps header;
    header.title = "概览";
    header.subtitle = "kiikoread · Murphy M4";
    screen.header(header);
    drawTabBar(screen);

    Rect body = screen.body().inset(Insets{12, 16, 12, 16});
    TextStyle section = screen.theme().titleText;
    section.bold = true;
    Rect statusCard{body.x, body.y, body.width, 308};
    screen.target().fill(statusCard, Paint::solid(Color::White));
    screen.target().stroke(statusCard, Paint::solid(Color::DarkGray), 1);
    screen.target().text(Rect{static_cast<int16_t>(statusCard.x + 16),
                              static_cast<int16_t>(statusCard.y + 14),
                              static_cast<int16_t>(statusCard.width - 32), 42},
                         "设备状态", section);
    screen.target().line(Point{static_cast<int16_t>(statusCard.x + 16),
                               static_cast<int16_t>(statusCard.y + 62)},
                         Point{static_cast<int16_t>(statusCard.right() - 16),
                               static_cast<int16_t>(statusCard.y + 62)}, 1,
                         Paint::solid(Color::LightGray));
    const char* labels[] = {"型号", "屏幕", "存储卡", "阅读器"};
    const char* values[] = {"Murphy M4 Batch 2", "SSD1677 · 480 × 800 · 4 级灰阶",
                            s_ui.storageReady ? "已就绪" : "未就绪",
                            "TXT / EPUB · 项目解析与排版"};
    TextStyle labelStyle = screen.theme().smallText;
    TextStyle valueStyle = screen.theme().bodyText;
    valueStyle.bold = true;
    const int16_t rowTop = static_cast<int16_t>(statusCard.y + 72);
    const int16_t rowHeight = 54;
    for (int i = 0; i < 4; ++i) {
        const int16_t y = static_cast<int16_t>(rowTop + i * rowHeight);
        screen.target().text(Rect{static_cast<int16_t>(statusCard.x + 16), y,
                                  static_cast<int16_t>(statusCard.width - 32), 24}, labels[i], labelStyle);
        screen.target().text(Rect{static_cast<int16_t>(statusCard.x + 16), static_cast<int16_t>(y + 24),
                                  static_cast<int16_t>(statusCard.width - 32), 34},
                             values[i], valueStyle);
        if (i != 3) screen.target().line(Point{static_cast<int16_t>(statusCard.x + 16), static_cast<int16_t>(y + 52)},
                                         Point{static_cast<int16_t>(statusCard.right() - 16), static_cast<int16_t>(y + 52)}, 1,
                                         Paint::solid(Color::LightGray));
    }
    const Color levels[] = {Color::Black, Color::DarkGray, Color::LightGray, Color::White};
    const int16_t swatchY = static_cast<int16_t>(statusCard.bottom() - 28);
    const int16_t swatchW = static_cast<int16_t>(body.width / 4);
    for (int i = 0; i < 4; ++i) {
        Rect swatch{static_cast<int16_t>(body.x + i * swatchW + 3), swatchY,
                    static_cast<int16_t>(swatchW - 6), 24};
        screen.target().fill(swatch, Paint::solid(levels[i]));
        screen.target().stroke(swatch, Paint::solid(Color::DarkGray), 1);
    }
}

static void drawEntries(Screen<32>& screen, const bool files) {
    rebuildEntries(files);
    drawTopStatus(screen);
    HeaderProps header;
    header.title = files ? "文件管理" : "书架";
    char subtitle[240];
    if (files) {
        std::snprintf(subtitle, sizeof(subtitle), "%s  ·  %u 项%s", s_ui.currentDirectory,
                      static_cast<unsigned>(s_ui.entryCount), s_ui.entriesTruncated ? " · 已达上限" : "");
    } else {
        std::snprintf(subtitle, sizeof(subtitle), "%u 项%s", static_cast<unsigned>(s_ui.entryCount),
                      s_ui.entriesTruncated ? " · 已达扫描上限" : "");
    }
    header.subtitle = subtitle;
    screen.header(header);
    drawTabBar(screen);
    if (!files && s_ui.shelfScanRunning.load(std::memory_order_acquire)) {
        screen.target().text(screen.body(), "正在读取书架…", screen.theme().bodyText);
        return;
    }
    if (!s_ui.storageReady) {
        screen.target().text(screen.body(), "请检查 TF 卡", screen.theme().bodyText);
        return;
    }
    if (files && std::strcmp(s_ui.currentDirectory, "/sdcard") != 0)
        screen.button("返回上级目录", ACTION_BACK);
    if (s_ui.entryCount == 0) {
        screen.target().text(screen.body(), files ? "此目录没有文件" : "TF 卡中没有找到 TXT 或 EPUB",
                             screen.theme().bodyText);
        return;
    }
    ListProps props;
    props.items = s_ui.listItems;
    props.count = static_cast<uint16_t>(s_ui.entryCount);
    props.action = ACTION_ENTRY;
    props.rowHeight = 54;
    props.rowGap = 2;
    props.separator = SeparatorStyle::Solid;
    props.selectionMarker = SelectionMarker::Underline;
    props.labelText = screen.theme().bodyText;
    props.subtitleText = screen.theme().smallText;
    screen.list(props);
}

static void drawShelf(Screen<32>& screen, void*) {
    rebuildEntries(false);
    const size_t pageCount = s_ui.entryCount == 0 ? 1 : (s_ui.entryCount + SHELF_PAGE_SIZE - 1) / SHELF_PAGE_SIZE;
    if (s_ui.shelfPage >= pageCount) s_ui.shelfPage = pageCount - 1;
    drawTopStatus(screen);
    const Rect shelfControls = screen.takeTop(46);
    TextStyle category = screen.theme().bodyText;
    category.bold = true;
    const char* sortLabel = s_ui.shelfSort == SHELF_SORT_NAME_DESC ? "书名 ↓" :
                            s_ui.shelfSort == SHELF_SORT_RECENT ? "最近阅读" :
                            s_ui.shelfSort == SHELF_SORT_ADDED ? "首次收录" : "书名 ↑";
    ButtonProps sort;
    sort.text = category;
    sort.label = sortLabel;
    sort.action = ACTION_SHELF_SORT;
    sort.styles = defaultButtonStyles();
    sort.styles.normal.background = Paint::none();
    sort.styles.normal.border = Paint::none();
    ui::button(screen.frame(), Rect{12, shelfControls.y, 190, shelfControls.height}, sort);
    TextStyle countStyle = screen.theme().smallText;
    countStyle.align = TextAlign::Right;
    char subtitle[64];
    std::snprintf(subtitle, sizeof(subtitle), "%u 本  ·  %u/%u",
                  static_cast<unsigned>(s_ui.entryCount), static_cast<unsigned>(s_ui.shelfPage + 1),
                  static_cast<unsigned>(pageCount));
    screen.target().text(Rect{250, shelfControls.y, 214, shelfControls.height}, subtitle, countStyle);
    screen.target().line(Point{12, static_cast<int16_t>(shelfControls.bottom() - 1)},
                         Point{468, static_cast<int16_t>(shelfControls.bottom() - 1)}, 1,
                         Paint::solid(Color::LightGray));
    drawTabBar(screen);
    if (s_ui.shelfScanRunning.load(std::memory_order_acquire)) {
        screen.target().text(screen.body(), "正在读取书架…", screen.theme().bodyText);
        return;
    }
    if (!s_ui.storageReady) {
        screen.target().text(screen.body(), "请检查 TF 卡", screen.theme().bodyText);
        return;
    }
    if (s_ui.readerError[0] != '\0') {
        TextStyle error = screen.theme().smallText;
        error.color = Color::DarkGray;
        screen.target().text(Rect{16, 102, 448, 38}, s_ui.readerError, error);
    }
    if (s_ui.entryCount == 0) {
        screen.target().text(screen.body(), "TF 卡中没有找到 TXT 或 EPUB", screen.theme().bodyText);
        return;
    }

    // Match the project's shelf hierarchy: a cover-first book grid with metadata
    // and progress kept inside each card, reduced to two columns for M4's portrait UI.
    Rect body = screen.body().inset(Insets{12, 14, 48, 14});
    constexpr int16_t gapX = 10;
    constexpr int16_t gapY = 10;
    const int16_t cellW = static_cast<int16_t>((body.width - gapX) / 2);
    const int16_t cellH = static_cast<int16_t>((body.height - gapY) / 2);
    const int16_t coverW = 108;
    const int16_t coverH = 140;
    const size_t first = s_ui.shelfPage * SHELF_PAGE_SIZE;
    for (size_t slot = 0; slot < SHELF_PAGE_SIZE && first + slot < s_ui.entryCount; ++slot) {
        const int col = static_cast<int>(slot % 2);
        const int row = static_cast<int>(slot / 2);
        const int16_t x = static_cast<int16_t>(body.x + col * (cellW + gapX));
        const int16_t y = static_cast<int16_t>(body.y + row * (cellH + gapY));
        const int16_t index = static_cast<int16_t>(first + slot);
        Entry entry{};
        if (s_ui.coverMutex != nullptr && xSemaphoreTake(s_ui.coverMutex, portMAX_DELAY) == pdTRUE) {
            entry = s_ui.entries[index];
            xSemaphoreGive(s_ui.coverMutex);
        } else entry = s_ui.entries[index];
        ButtonProps hit;
        hit.action = ACTION_ENTRY;
        hit.value = index;
        hit.styles = defaultButtonStyles();
        hit.styles.normal.background = Paint::solid(Color::White);
        hit.styles.normal.border = Paint::none();
        hit.styles.normal.borderWidth = 0;
        ui::button(screen.frame(), Rect{x, y, cellW, cellH}, hit);

        Rect cover{static_cast<int16_t>(x + (cellW - coverW) / 2), static_cast<int16_t>(y + 12), coverW, coverH};
        screen.target().fill(cover, Paint::solid(Color::White));
        const size_t cacheSlot = slot;
        bool hasCover = false;
        if (s_ui.coverMutex != nullptr && xSemaphoreTake(s_ui.coverMutex, portMAX_DELAY) == pdTRUE) {
            hasCover = s_ui.coverValid[cacheSlot] &&
                       std::strcmp(s_ui.coverPaths[cacheSlot], entry.path) == 0;
            if (hasCover) {
                BitmapRef bitmap{s_ui.coverGray2 + cacheSlot * COVER_GRAY2_BYTES,
                                 COVER_WIDTH, COVER_HEIGHT, BitmapFormat::Gray2, false};
                screen.target().bitmap(cover, bitmap, BitmapMode::Cover);
            }
            xSemaphoreGive(s_ui.coverMutex);
        }
        if (!hasCover) {
            TextStyle coverTitle = screen.theme().smallText;
            coverTitle.bold = true;
            coverTitle.align = TextAlign::Center;
            coverTitle.maxLines = 4;
            screen.target().text(cover.inset(Insets{8, 8, 8, 12}), entry.title, coverTitle);
        }
        screen.target().stroke(cover, Paint::solid(Color::Black), 1);
        screen.target().fill(Rect{static_cast<int16_t>(cover.x + 3), cover.y, 3, cover.height},
                             Paint::solid(Color::DarkGray));

        const int16_t textX = static_cast<int16_t>(x + 10);
        const int16_t textW = static_cast<int16_t>(cellW - 20);
        TextStyle titleStyle = screen.theme().bodyText;
        titleStyle.bold = true;
        titleStyle.maxLines = 2;
        screen.target().text(Rect{textX, static_cast<int16_t>(cover.bottom() + 10), textW, 38},
                             entry.title, titleStyle);
        TextStyle authorStyle = screen.theme().smallText;
        authorStyle.color = Color::DarkGray;
        authorStyle.maxLines = 1;
        screen.target().text(Rect{textX, static_cast<int16_t>(cover.bottom() + 50), textW, 18},
                             entry.metadataReady && entry.author[0] ? entry.author : "本地 EPUB", authorStyle);
        char progressLabel[24];
        std::snprintf(progressLabel, sizeof(progressLabel), entry.hasProgress ? "已读 %u%%" : "未开始",
                      static_cast<unsigned>(entry.progress));
        screen.target().text(Rect{textX, static_cast<int16_t>(y + cellH - 34), textW, 18},
                             progressLabel, screen.theme().smallText);
        {
            const int16_t progressY = static_cast<int16_t>(y + cellH - 10);
            Rect track{textX, progressY, textW, 4};
            screen.target().fill(track, Paint::solid(Color::LightGray));
            if (entry.hasProgress && entry.progress > 0) {
                track.width = std::max<int16_t>(1, static_cast<int16_t>(textW * entry.progress / 100));
                screen.target().fill(track, Paint::solid(Color::DarkGray));
            }
        }
    }

    if (pageCount > 1) {
        const int16_t pagerY = static_cast<int16_t>(body.bottom() + 8);
        ButtonProps pager;
        pager.text = screen.theme().smallText;
        pager.styles = defaultButtonStyles();
        pager.styles.normal.border = Paint::solid(Color::DarkGray);
        pager.styles.normal.borderWidth = 1;
        pager.label = "上一页";
        pager.action = ACTION_SHELF_PREVIOUS;
        pager.enabled = s_ui.shelfPage > 0;
        ui::button(screen.frame(), Rect{body.x, pagerY, 112, 42}, pager);
        pager.label = "下一页";
        pager.action = ACTION_SHELF_NEXT;
        pager.enabled = s_ui.shelfPage + 1 < pageCount;
        ui::button(screen.frame(), Rect{static_cast<int16_t>(body.right() - 112), pagerY, 112, 42}, pager);
    }
}
static void drawFiles(Screen<32>& screen, void*) { drawEntries(screen, true); }

static void drawSettings(Screen<32>& screen, void*) {
    screen.target().fill(screen.contentRect(), Paint::solid(Color::LightGray));
    drawTopStatus(screen);
    HeaderProps header;
    header.title = "设置";
    // Keep the subtitle within the 480 px portrait header band; the settings
    // rows below carry the full section names from the project UI.
    header.subtitle = "连接 · 显示 · 个性化 · 升级";
    screen.header(header);
    drawTabBar(screen);
    screen.insetContent(Insets{10, 10, 8, 10});
    screen.target().fill(screen.body(), Paint::solid(Color::White));
    screen.target().stroke(screen.body(), Paint::solid(Color::LightGray), 1);
    ListItem* items = s_ui.settingsItems;
    std::memset(items, 0, sizeof(s_ui.settingsItems));
    items[0].label = "连接与时间"; items[0].isHeader = true; items[0].enabled = false;
    items[1].label = "日期与时间";
    items[1].value = s_ui.clockValid ? "设置时间  ›" : "校准 RTC  ›";
    items[1].enabled = true;
    items[1].actionValue = 4;
    items[2].label = "显示"; items[2].isHeader = true; items[2].enabled = false;
    items[3].label = "前光"; items[3].toggle = true;
    items[3].toggleChecked = s_ui.frontlightOn;
    items[3].value = s_ui.frontlightOn ? "开启" : "关闭";
    items[3].actionValue = 1;
    items[4].label = "前光亮度";
    char brightness[16];
    std::snprintf(brightness, sizeof(brightness), "%u%%", s_ui.brightness);
    items[4].value = brightness;
    items[4].actionValue = 2;
    items[5].label = "屏幕"; items[5].subtitle = "SSD1677 · 480 × 800 · 4 级灰阶";
    items[5].enabled = false;
    items[6].label = "个性化功能"; items[6].isHeader = true; items[6].enabled = false;
    const char* sortLabel = s_ui.shelfSort == SHELF_SORT_NAME_DESC ? "书名降序" :
                            s_ui.shelfSort == SHELF_SORT_RECENT ? "最近阅读" :
                            s_ui.shelfSort == SHELF_SORT_ADDED ? "首次收录" : "书名升序";
    items[7].label = "书架排序"; items[7].value = sortLabel; items[7].actionValue = 3;
    items[8].label = "升级和恢复"; items[8].isHeader = true; items[8].enabled = false;
    items[9].label = "存储卡"; items[9].value = s_ui.storageReady ? "已连接" : "未连接";
    items[9].enabled = false;
    ListProps props;
    props.items = items;
    props.count = 10;
    props.action = ACTION_SETTINGS_ROW;
    // Match the project's settings cards: rows need a full touch target and
    // breathing room on the 480x800 panel instead of the compressed list used
    // by the first M4 prototype.
    props.rowHeight = 56;
    props.rowGap = 0;
    props.headerRowHeight = 34;
    props.sectionGap = 8;
    props.separator = SeparatorStyle::Solid;
    props.labelText = screen.theme().bodyText;
    props.subtitleText = screen.theme().smallText;
    props.valueText = screen.theme().smallText;
    props.headerText = screen.theme().smallText;
    props.selectionMarker = SelectionMarker::None;
    screen.list(props);
}

static void drawTimeSettings(Screen<32>& screen, void*) {
    drawTopStatus(screen);
    HeaderProps header;
    header.title = "日期与时间";
    header.subtitle = "调整后保存到 M4 RTC";
    screen.header(header);
    const char *labels[] = {"年", "月", "日", "时", "分"};
    const uint16_t values[] = {s_ui.timeEdit.year, s_ui.timeEdit.month, s_ui.timeEdit.day,
                               s_ui.timeEdit.hour, s_ui.timeEdit.minute};
    for (int i = 0; i < 5; ++i) {
        const int16_t x = static_cast<int16_t>(12 + i * 92);
        screen.target().text(Rect{x, 142, 84, 28}, labels[i], screen.theme().smallText);
        char value[8];
        std::snprintf(value, sizeof(value), "%02u", static_cast<unsigned>(values[i]));
        TextStyle centered = screen.theme().bodyText;
        centered.align = TextAlign::Center;
        centered.bold = true;
        screen.target().text(Rect{x, 178, 84, 42}, value, centered);
        ButtonProps control;
        control.text = screen.theme().bodyText;
        control.styles = defaultButtonStyles();
        control.styles.normal.border = Paint::solid(Color::DarkGray);
        control.styles.normal.borderWidth = 1;
        control.label = "+";
        control.action = ACTION_TIME_FIELD_UP;
        control.value = static_cast<int16_t>(i);
        ui::button(screen.frame(), Rect{x, 228, 84, 44}, control);
        control.label = "-";
        control.action = ACTION_TIME_FIELD_DOWN;
        ui::button(screen.frame(), Rect{x, 278, 84, 44}, control);
    }
    ButtonProps action;
    action.text = screen.theme().bodyText;
    action.styles = defaultButtonStyles();
    action.styles.normal.border = Paint::solid(Color::DarkGray);
    action.styles.normal.borderWidth = 1;
    action.label = "保存时间";
    action.action = ACTION_TIME_SAVE;
    ui::button(screen.frame(), Rect{24, 370, 204, 52}, action);
    action.label = "取消";
    action.action = ACTION_TIME_CANCEL;
    ui::button(screen.frame(), Rect{252, 370, 204, 52}, action);
}

static void drawBookDetail(Screen<32>& screen, void*) {
    HeaderProps header;
    header.title = "书籍信息";
    header.subtitle = s_ui.selectedTitle;
    screen.header(header);
    drawTabBar(screen);
    screen.target().text(Rect{24, 120, 432, 56}, "文件位置", screen.theme().smallText);
    TextStyle pathStyle = screen.theme().bodyText;
    pathStyle.maxLines = 3;
    screen.target().text(Rect{24, 154, 432, 98}, s_ui.selectedPath, pathStyle);
    char sizeText[48];
    std::snprintf(sizeText, sizeof(sizeText), "文件大小：%.1f MB",
                  static_cast<double>(s_ui.selectedSize) / (1024.0 * 1024.0));
    screen.target().text(Rect{24, 266, 432, 44}, sizeText, screen.theme().bodyText);
    if (s_ui.readerError[0] != '\0') {
        TextStyle error = screen.theme().bodyText;
        error.color = Color::DarkGray;
        error.maxLines = 4;
        screen.target().text(Rect{24, 326, 432, 112}, s_ui.readerError, error);
    }
    if (endsWithBook(s_ui.selectedPath)) {
        screen.button("打开阅读器", ACTION_OPEN_READER);
        screen.button("返回列表", ACTION_BACK);
    } else {
        screen.button("返回列表", ACTION_BACK);
    }
}

static void selectPage(Page page);

static bool openSelectedBook() {
    if (!endsWithBook(s_ui.selectedPath)) return false;
    s_ui.readerError[0] = '\0';
    if (murphyM4ProjectReaderOpen(s_ui.selectedPath)) {
        ESP_LOGI(TAG, "opened book from detail: %s", s_ui.selectedPath);
        selectPage(Page::Reader);
        return true;
    }
    std::snprintf(s_ui.readerError, sizeof(s_ui.readerError),
                  "无法打开图书；请检查串口中的书源、字体或排版错误后重试。");
    ESP_LOGW(TAG, "project book reader rejected: %s", s_ui.selectedPath);
    s_ui.app->invalidate(RefreshHint::Fast);
    return false;
}

static void drawReader(Screen<32>& screen, void*) {
    size_t bytes = 0;
    const uint8_t* pixels = murphyM4ProjectReaderFrame(&bytes);
    if (pixels && bytes >= GRAY2_BYTES) {
        BitmapRef page{pixels, static_cast<uint16_t>(UI_WIDTH), static_cast<uint16_t>(UI_HEIGHT),
                       BitmapFormat::Gray2, false};
        screen.target().bitmap(Rect{0, 0, UI_WIDTH, UI_HEIGHT}, page, BitmapMode::Stretch);
    }
    screen.target().fill(Rect{0, 0, UI_WIDTH, 48}, Paint::solid(Color::White));
    screen.target().fill(Rect{0, 736, UI_WIDTH, 64}, Paint::solid(Color::White));
    TextStyle titleStyle = screen.theme().bodyText;
    titleStyle.bold = true;
    screen.target().text(Rect{12, 4, 300, 40}, murphyM4ProjectReaderTitle(), titleStyle);
    char pageLabel[48];
    std::snprintf(pageLabel, sizeof(pageLabel), "%u / %u",
                  static_cast<unsigned>(murphyM4ProjectReaderPage()),
                  static_cast<unsigned>(murphyM4ProjectReaderPageCount()));
    TextStyle pageStyle = screen.theme().smallText;
    pageStyle.align = TextAlign::Right;
    screen.target().text(Rect{320, 4, 148, 40}, pageLabel, pageStyle);

    ButtonProps button;
    button.text = screen.theme().bodyText;
    button.styles = defaultButtonStyles();
    button.styles.normal.border = Paint::solid(Color::DarkGray);
    button.styles.normal.borderWidth = 1;
    constexpr int16_t toolY = 748;
    constexpr int16_t toolW = 78;
    button.label = "返回";
    button.action = ACTION_BACK;
    ui::button(screen.frame(), Rect{6, toolY, toolW, 40}, button);
    button.label = "目录";
    button.action = ACTION_READER_TOC;
    ui::button(screen.frame(), Rect{84, toolY, toolW, 40}, button);
    button.label = favoriteRead(s_ui.selectedPath) ? "书签✓" : "书签";
    button.action = ACTION_READER_FAVORITE;
    ui::button(screen.frame(), Rect{162, toolY, toolW, 40}, button);
    button.label = "设置";
    button.action = ACTION_READER_SETTINGS;
    ui::button(screen.frame(), Rect{240, toolY, toolW, 40}, button);
    button.label = "上一页";
    button.action = ACTION_READER_PREVIOUS;
    ui::button(screen.frame(), Rect{318, toolY, toolW, 40}, button);
    button.label = "下一页";
    button.action = ACTION_READER_NEXT;
    ui::button(screen.frame(), Rect{396, toolY, toolW, 40}, button);
}

static void drawReaderToc(Screen<32>& screen, void*) {
    const size_t count = murphyM4ProjectReaderTocCount();
    const size_t pageCount = count ? (count + TOC_PAGE_SIZE - 1) / TOC_PAGE_SIZE : 1;
    if (s_ui.tocPage >= pageCount) s_ui.tocPage = pageCount - 1;
    drawTopStatus(screen);
    TextStyle heading = screen.theme().bodyText;
    heading.bold = true;
    screen.target().text(Rect{16, 64, 448, 42}, "本书目录", heading);
    if (!count) {
        screen.target().text(Rect{20, 128, 440, 48}, "此书没有可用目录", screen.theme().bodyText);
    } else {
        const size_t first = s_ui.tocPage * TOC_PAGE_SIZE;
        for (size_t row = 0; row < TOC_PAGE_SIZE && first + row < count; ++row) {
            char title[160]{};
            if (!murphyM4ProjectReaderTocTitle(first + row, title, sizeof(title)))
                std::snprintf(title, sizeof(title), "目录项 %u", static_cast<unsigned>(first + row + 1));
            ButtonProps item;
            item.text = screen.theme().bodyText;
            item.label = title;
            item.action = ACTION_TOC_SELECT;
            item.value = static_cast<int16_t>(first + row);
            item.styles = defaultButtonStyles();
            item.styles.normal.border = Paint::solid(Color::LightGray);
            item.styles.normal.borderWidth = 1;
            ui::button(screen.frame(), Rect{16, static_cast<int16_t>(116 + row * 70), 448, 62}, item);
        }
    }
    ButtonProps footer;
    footer.text = screen.theme().smallText;
    footer.styles = defaultButtonStyles();
    footer.styles.normal.border = Paint::solid(Color::DarkGray);
    footer.styles.normal.borderWidth = 1;
    footer.label = "返回正文";
    footer.action = ACTION_BACK;
    ui::button(screen.frame(), Rect{12, 748, 142, 40}, footer);
    char pageLabel[32];
    std::snprintf(pageLabel, sizeof(pageLabel), "%u / %u", static_cast<unsigned>(s_ui.tocPage + 1),
                  static_cast<unsigned>(pageCount));
    TextStyle pageStyle = screen.theme().smallText;
    pageStyle.align = TextAlign::Center;
    screen.target().text(Rect{170, 748, 140, 40}, pageLabel, pageStyle);
    footer.label = "上一组";
    footer.action = ACTION_TOC_PREVIOUS;
    ui::button(screen.frame(), Rect{318, 748, 72, 40}, footer);
    footer.label = "下一组";
    footer.action = ACTION_TOC_NEXT;
    ui::button(screen.frame(), Rect{398, 748, 72, 40}, footer);
}

static void drawReaderSettings(Screen<32>& screen, void*) {
    drawTopStatus(screen);
    TextStyle heading = screen.theme().bodyText;
    heading.bold = true;
    screen.target().text(Rect{16, 64, 448, 42}, "阅读设置", heading);
    char sizeLabel[48];
    std::snprintf(sizeLabel, sizeof(sizeLabel), "字号  %u px", murphyM4ProjectReaderFontPx());
    screen.target().text(Rect{20, 118, 440, 36}, sizeLabel, screen.theme().bodyText);
    ButtonProps control;
    control.text = screen.theme().bodyText;
    control.styles = defaultButtonStyles();
    control.styles.normal.border = Paint::solid(Color::DarkGray);
    control.styles.normal.borderWidth = 1;
    control.label = "A −";
    control.action = ACTION_FONT_DOWN;
    ui::button(screen.frame(), Rect{20, 158, 208, 54}, control);
    control.label = "A +";
    control.action = ACTION_FONT_UP;
    ui::button(screen.frame(), Rect{252, 158, 208, 54}, control);

    const char *fontPath = app_settings_font_path();
    const char *fontName = fontPath[0] ? std::strrchr(fontPath, '/') : nullptr;
    fontName = fontName ? fontName + 1 : (fontPath[0] ? fontPath : "内建字体");
    screen.target().text(Rect{20, 226, 260, 36}, fontName, screen.theme().smallText);
    control.label = "切换系统字体";
    control.action = ACTION_FONT_NEXT;
    ui::button(screen.frame(), Rect{286, 220, 174, 48}, control);

    control.label = m4ReaderSettingsBookFonts() ? "关闭 EPUB 内嵌字体" : "启用 EPUB 内嵌字体";
    control.action = ACTION_READER_BOOK_FONTS;
    ui::button(screen.frame(), Rect{20, 280, 440, 50}, control);

    control.label = m4ReaderSettingsVertical() ? "当前：竖排" : "当前：横排";
    control.action = ACTION_READER_VERTICAL;
    ui::button(screen.frame(), Rect{20, 340, 440, 50}, control);
    control.label = m4ReaderSettingsHideImages() ? "显示正文插图" : "隐藏正文插图";
    control.action = ACTION_READER_IMAGES;
    ui::button(screen.frame(), Rect{20, 400, 440, 50}, control);

    char spacingLabel[48];
    std::snprintf(spacingLabel, sizeof(spacingLabel), "行距  %u%%", m4ReaderSettingsLineSpacing());
    screen.target().text(Rect{20, 468, 440, 36}, spacingLabel, screen.theme().bodyText);
    control.label = "行距 −";
    control.action = ACTION_LINE_DOWN;
    ui::button(screen.frame(), Rect{20, 510, 208, 54}, control);
    control.label = "行距 +";
    control.action = ACTION_LINE_UP;
    ui::button(screen.frame(), Rect{252, 510, 208, 54}, control);
    control.label = "返回正文";
    control.action = ACTION_BACK;
    ui::button(screen.frame(), Rect{20, 748, 440, 40}, control);
}

static void drawImageViewer(Screen<32>& screen, void*) {
    if (s_ui.imageGray2) {
        BitmapRef image{s_ui.imageGray2, static_cast<uint16_t>(UI_WIDTH),
                        static_cast<uint16_t>(UI_HEIGHT), BitmapFormat::Gray2, false};
        screen.target().bitmap(Rect{0, 0, UI_WIDTH, UI_HEIGHT}, image, BitmapMode::Stretch);
    }
    screen.target().fill(Rect{0, 0, UI_WIDTH, 48}, Paint::solid(Color::White));
    screen.target().fill(Rect{0, 736, UI_WIDTH, 64}, Paint::solid(Color::White));
    TextStyle title = screen.theme().bodyText;
    title.bold = true;
    title.maxLines = 1;
    screen.target().text(Rect{12, 4, 340, 40}, s_ui.selectedTitle, title);
    TextStyle dimensions = screen.theme().smallText;
    dimensions.align = TextAlign::Right;
    char label[64];
    std::snprintf(label, sizeof(label), "%u x %u", s_ui.imageWidth, s_ui.imageHeight);
    screen.target().text(Rect{350, 4, 118, 40}, label, dimensions);

    ButtonProps button;
    button.text = screen.theme().smallText;
    button.styles = defaultButtonStyles();
    button.styles.normal.border = Paint::solid(Color::DarkGray);
    button.styles.normal.borderWidth = 1;
    button.label = "返回";
    button.action = ACTION_BACK;
    ui::button(screen.frame(), Rect{8, 748, 104, 40}, button);
    button.label = "上一张";
    button.action = ACTION_IMAGE_PREVIOUS;
    ui::button(screen.frame(), Rect{122, 748, 156, 40}, button);
    button.label = "下一张";
    button.action = ACTION_IMAGE_NEXT;
    ui::button(screen.frame(), Rect{288, 748, 184, 40}, button);
}

static void present() {
    auto* bw = s_ui.display->getFrameBuffer();
    if (!bw || !s_ui.lsb || !s_ui.msb) return;
    const size_t stride = s_ui.display->getDisplayWidthBytes();
    const size_t bwBytes = stride * PANEL_HEIGHT;
    if (!m4Gray2ToSsd1677Planes(s_ui.gray, GRAY2_BYTES, PANEL_WIDTH, PANEL_HEIGHT,
                                bw, bwBytes, stride, s_ui.lsb, s_ui.msb,
                                PLANE_BYTES)) {
        ESP_LOGE(TAG, "Gray2 to SSD1677 plane conversion rejected display geometry");
        return;
    }
    s_ui.display->displayGrayscaleBase(FreeInkDisplay::HALF_REFRESH, false);
    s_ui.display->copyGrayscaleBuffers(s_ui.lsb, s_ui.msb);
    s_ui.display->displayGrayBuffer(false);
}

static void drawTimeSettings(Screen<32>& screen, void*);

static void selectPage(const Page page) {
    if (s_ui.page == Page::Shelf && page != Page::Shelf)
        s_ui.coverGeneration.fetch_add(1, std::memory_order_acq_rel);
    s_ui.page = page;
    if (page == Page::Shelf) rebuildEntries(false);
    if (page == Page::Files) rebuildEntries(true);
    switch (page) {
        case Page::Home: s_ui.app->setScreen(drawHome, &s_ui, RefreshHint::Fast); break;
        case Page::Shelf: s_ui.app->setScreen(drawShelf, &s_ui, RefreshHint::Fast); break;
        case Page::Files: s_ui.app->setScreen(drawFiles, &s_ui, RefreshHint::Fast); break;
        case Page::Settings: s_ui.app->setScreen(drawSettings, &s_ui, RefreshHint::Fast); break;
        case Page::BookDetail: s_ui.app->setScreen(drawBookDetail, &s_ui, RefreshHint::Fast); break;
        case Page::Reader: s_ui.app->setScreen(drawReader, &s_ui, RefreshHint::Fast); break;
        case Page::ImageViewer: s_ui.app->setScreen(drawImageViewer, &s_ui, RefreshHint::Fast); break;
        case Page::ReaderToc: s_ui.app->setScreen(drawReaderToc, &s_ui, RefreshHint::Fast); break;
        case Page::ReaderSettings: s_ui.app->setScreen(drawReaderSettings, &s_ui, RefreshHint::Fast); break;
        case Page::TimeSettings: s_ui.app->setScreen(drawTimeSettings, &s_ui, RefreshHint::Fast); break;
    }
    if (page == Page::Shelf) scheduleShelfCovers();
}

static void updateLight() {
    if (s_ui.frontlightOn) s_ui.frontlight->setBrightness(s_ui.brightness);
    else s_ui.frontlight->setBrightness(0);
    if (s_ui.settingsReady) {
        (void)nvs_set_u8(s_ui.settings, "light", s_ui.brightness);
        (void)nvs_set_u8(s_ui.settings, "light_on", s_ui.frontlightOn ? 1 : 0);
        (void)nvs_commit(s_ui.settings);
    }
}

static void refreshDeviceStatus() {
    if (s_ui.rtc != nullptr) {
        Rtc::DateTime current{};
        if (s_ui.rtc->now(current)) {
            s_ui.clockTime = current;
            s_ui.clockValid = true;
        } else {
            s_ui.clockValid = false;
        }
    }
    if (s_ui.battery != nullptr) {
        const BatteryMonitor::Status current = s_ui.battery->readStatus();
        if (current.supported) {
            s_ui.batteryStatus.supported = true;
            if (current.percentageKnown) {
                s_ui.batteryStatus.percentageKnown = true;
                s_ui.batteryStatus.percentage = current.percentage;
            }
            if (current.millivoltsKnown) {
                s_ui.batteryStatus.millivoltsKnown = true;
                s_ui.batteryStatus.millivolts = current.millivolts;
            }
            if (current.chargingKnown) {
                s_ui.batteryStatus.chargingKnown = true;
                s_ui.batteryStatus.charging = current.charging;
            }
            if (current.externalPowerKnown) {
                s_ui.batteryStatus.externalPowerKnown = true;
                s_ui.batteryStatus.externalPower = current.externalPower;
            }
        }
    }
    s_ui.deviceStatusUpdatedAt = millis();
}

static uint8_t daysInMonth(const uint16_t year, const uint8_t month) {
    static constexpr uint8_t days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month < 1 || month > 12) return 31;
    if (month == 2 && (year % 400 == 0 || (year % 4 == 0 && year % 100 != 0))) return 29;
    return days[month - 1];
}

static uint8_t weekdayFor(const uint16_t year, const uint8_t month, const uint8_t day) {
    static constexpr uint8_t offsets[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    int y = year;
    const int m = month;
    if (m < 1 || m > 12) return 0;
    if (m < 3) --y;
    return static_cast<uint8_t>((y + y / 4 - y / 100 + y / 400 + offsets[m - 1] + day) % 7);
}

static void adjustTimeField(const int field, const int direction) {
    Rtc::DateTime& value = s_ui.timeEdit;
    switch (field) {
        case 0:
            value.year = value.year >= 2099 && direction > 0 ? 2024 :
                         value.year <= 2024 && direction < 0 ? 2099 :
                         static_cast<uint16_t>(value.year + direction);
            if (value.day > daysInMonth(value.year, value.month)) value.day = daysInMonth(value.year, value.month);
            break;
        case 1:
            value.month = value.month >= 12 && direction > 0 ? 1 :
                          value.month <= 1 && direction < 0 ? 12 :
                          static_cast<uint8_t>(value.month + direction);
            if (value.day > daysInMonth(value.year, value.month)) value.day = daysInMonth(value.year, value.month);
            break;
        case 2: {
            const uint8_t limit = daysInMonth(value.year, value.month);
            value.day = value.day >= limit && direction > 0 ? 1 :
                        value.day <= 1 && direction < 0 ? limit :
                        static_cast<uint8_t>(value.day + direction);
            break;
        }
        case 3:
            value.hour = static_cast<uint8_t>((value.hour + 24 + direction) % 24);
            break;
        case 4:
            value.minute = static_cast<uint8_t>((value.minute + 60 + direction) % 60);
            break;
        default: return;
    }
    value.weekday = weekdayFor(value.year, value.month, value.day);
}

static void beginTimeEdit() {
    Rtc::DateTime value{};
    if (s_ui.rtc != nullptr && s_ui.rtc->now(value) && value.year >= 2024 && value.year <= 2099) {
        s_ui.timeEdit = value;
    } else {
        char month[4]{};
        int day = 1;
        int year = 2026;
        int hour = 0, minute = 0, second = 0;
        if (std::sscanf(__DATE__, "%3s %d %d", month, &day, &year) == 3 &&
            std::sscanf(__TIME__, "%d:%d:%d", &hour, &minute, &second) == 3) {
            static constexpr const char* names[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                                     "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
            for (int i = 0; i < 12; ++i) if (std::strcmp(month, names[i]) == 0) value.month = static_cast<uint8_t>(i + 1);
            value.year = static_cast<uint16_t>(std::max(2024, std::min(year, 2099)));
            value.day = static_cast<uint8_t>(std::max(1, std::min(day, static_cast<int>(daysInMonth(value.year, value.month)))));
            value.hour = static_cast<uint8_t>(hour);
            value.minute = static_cast<uint8_t>(minute);
            value.second = static_cast<uint8_t>(second);
        } else {
            value.year = 2026;
            value.month = 1;
            value.day = 1;
        }
        value.weekday = weekdayFor(value.year, value.month, value.day);
        s_ui.timeEdit = value;
    }
    s_ui.timeEdit.second = 0;
}

static void handleAction(const ActionEvent& event, void*) {
    switch (event.action) {
        case ACTION_TAB:
            if (event.value >= 0 && event.value <= 3) selectPage(static_cast<Page>(event.value));
            break;
        case ACTION_HOME_SHELF: selectPage(Page::Shelf); break;
        case ACTION_HOME_FILES:
            (void)enterDirectory("/sdcard");
            selectPage(Page::Files);
            break;
        case ACTION_HOME_SETTINGS: selectPage(Page::Settings); break;
        case ACTION_SETTINGS_ROW:
            if (event.value == 4) {
                beginTimeEdit();
                selectPage(Page::TimeSettings);
            } else if (event.value == 1) {
                s_ui.frontlightOn = !s_ui.frontlightOn;
                if (s_ui.frontlightOn && s_ui.brightness == 0) s_ui.brightness = 30;
                updateLight();
                s_ui.app->invalidate(RefreshHint::Fast);
            } else if (event.value == 2) {
                s_ui.brightness = s_ui.brightness <= 90 ? static_cast<uint8_t>(s_ui.brightness + 10) : 0;
                s_ui.frontlightOn = s_ui.brightness > 0;
                updateLight();
                s_ui.app->invalidate(RefreshHint::Fast);
            } else if (event.value == 3) {
                s_ui.shelfSort = static_cast<uint8_t>((s_ui.shelfSort + 1u) % 4u);
                if (s_ui.settingsReady) {
                    (void)nvs_set_u8(s_ui.settings, "shelf_sort", s_ui.shelfSort);
                    (void)nvs_commit(s_ui.settings);
                }
                sortShelfEntries();
                s_ui.shelfPage = 0;
                s_ui.app->invalidate(RefreshHint::Fast);
            }
            break;
        case ACTION_TIME_FIELD_UP:
            adjustTimeField(event.value, 1);
            s_ui.app->invalidate(RefreshHint::Fast);
            break;
        case ACTION_TIME_FIELD_DOWN:
            adjustTimeField(event.value, -1);
            s_ui.app->invalidate(RefreshHint::Fast);
            break;
        case ACTION_TIME_SAVE:
            if (s_ui.rtc != nullptr && s_ui.rtc->set(s_ui.timeEdit)) {
                s_ui.clockTime = s_ui.timeEdit;
                s_ui.clockValid = true;
                ESP_LOGI(TAG, "M4 RTC set: %04u-%02u-%02u %02u:%02u",
                         static_cast<unsigned>(s_ui.timeEdit.year),
                         static_cast<unsigned>(s_ui.timeEdit.month),
                         static_cast<unsigned>(s_ui.timeEdit.day),
                         static_cast<unsigned>(s_ui.timeEdit.hour),
                         static_cast<unsigned>(s_ui.timeEdit.minute));
                selectPage(Page::Settings);
            } else {
                ESP_LOGE(TAG, "M4 RTC update failed");
                s_ui.app->invalidate(RefreshHint::Fast);
            }
            break;
        case ACTION_TIME_CANCEL:
            selectPage(Page::Settings);
            break;
        case ACTION_SHELF_PREVIOUS:
            if (s_ui.shelfPage > 0) {
                --s_ui.shelfPage;
                scheduleShelfCovers();
                s_ui.app->invalidate(RefreshHint::Fast);
            }
            break;
        case ACTION_SHELF_NEXT: {
            const size_t pages = s_ui.entryCount == 0 ? 1 : (s_ui.entryCount + SHELF_PAGE_SIZE - 1) / SHELF_PAGE_SIZE;
            if (s_ui.shelfPage + 1 < pages) {
                ++s_ui.shelfPage;
                scheduleShelfCovers();
                s_ui.app->invalidate(RefreshHint::Fast);
            }
            break;
        }
        case ACTION_SHELF_SORT:
            s_ui.shelfSort = static_cast<uint8_t>((s_ui.shelfSort + 1u) % 4u);
            if (s_ui.settingsReady) {
                (void)nvs_set_u8(s_ui.settings, "shelf_sort", s_ui.shelfSort);
                (void)nvs_commit(s_ui.settings);
            }
            sortShelfEntries();
            s_ui.shelfPage = 0;
            scheduleShelfCovers();
            s_ui.app->invalidate(RefreshHint::Fast);
            break;
        case ACTION_READER_PREVIOUS:
            if (murphyM4ProjectReaderPrevious()) s_ui.app->invalidate(RefreshHint::Fast);
            break;
        case ACTION_OPEN_READER:
            (void)openSelectedBook();
            break;
        case ACTION_READER_NEXT:
            if (murphyM4ProjectReaderNext()) s_ui.app->invalidate(RefreshHint::Fast);
            break;
        case ACTION_READER_TOC:
            s_ui.tocPage = 0;
            selectPage(Page::ReaderToc);
            break;
        case ACTION_READER_SETTINGS:
            selectPage(Page::ReaderSettings);
            break;
        case ACTION_FONT_DOWN:
            if (murphyM4ProjectReaderAdjustFont(-1)) s_ui.app->invalidate(RefreshHint::Fast);
            break;
        case ACTION_FONT_UP:
            if (murphyM4ProjectReaderAdjustFont(1)) s_ui.app->invalidate(RefreshHint::Fast);
            break;
        case ACTION_READER_VERTICAL:
            if (murphyM4ProjectReaderToggleVertical()) s_ui.app->invalidate(RefreshHint::Fast);
            break;
        case ACTION_READER_IMAGES:
            if (murphyM4ProjectReaderToggleImages()) s_ui.app->invalidate(RefreshHint::Fast);
            break;
        case ACTION_READER_BOOK_FONTS:
            if (murphyM4ProjectReaderToggleBookFonts()) s_ui.app->invalidate(RefreshHint::Fast);
            break;
        case ACTION_READER_FAVORITE: {
            const bool next = !favoriteRead(s_ui.selectedPath);
            if (favoriteWrite(s_ui.selectedPath, next)) {
                for (size_t i = 0; i < s_ui.entryCount; ++i)
                    if (std::strcmp(s_ui.entries[i].path, s_ui.selectedPath) == 0)
                        s_ui.entries[i].favorite = next;
                sortShelfEntries();
                s_ui.app->invalidate(RefreshHint::Fast);
            }
            break;
        }
        case ACTION_LINE_DOWN:
            if (murphyM4ProjectReaderAdjustLineSpacing(-1)) s_ui.app->invalidate(RefreshHint::Fast);
            break;
        case ACTION_LINE_UP:
            if (murphyM4ProjectReaderAdjustLineSpacing(1)) s_ui.app->invalidate(RefreshHint::Fast);
            break;
        case ACTION_TOC_SELECT:
            if (event.value >= 0 && murphyM4ProjectReaderJumpToToc(static_cast<size_t>(event.value)))
                selectPage(Page::Reader);
            break;
        case ACTION_TOC_PREVIOUS:
            if (s_ui.tocPage > 0) {
                --s_ui.tocPage;
                s_ui.app->invalidate(RefreshHint::Fast);
            }
            break;
        case ACTION_TOC_NEXT: {
            const size_t count = murphyM4ProjectReaderTocCount();
            const size_t pages = count ? (count + TOC_PAGE_SIZE - 1) / TOC_PAGE_SIZE : 1;
            if (s_ui.tocPage + 1 < pages) {
                ++s_ui.tocPage;
                s_ui.app->invalidate(RefreshHint::Fast);
            }
            break;
        }
        case ACTION_IMAGE_PREVIOUS:
            if (moveImage(-1)) s_ui.app->invalidate(RefreshHint::Fast);
            break;
        case ACTION_IMAGE_NEXT:
            if (moveImage(1)) s_ui.app->invalidate(RefreshHint::Fast);
            break;
        case ACTION_ENTRY:
            if (event.value >= 0 && static_cast<size_t>(event.value) < s_ui.entryCount) {
                const Entry& entry = s_ui.entries[event.value];
                if (s_ui.page == Page::Files && entry.directory) {
                    if (enterDirectory(entry.path)) selectPage(Page::Files);
                    break;
                }
                std::snprintf(s_ui.selectedPath, sizeof(s_ui.selectedPath), "%s", entry.path);
                std::snprintf(s_ui.selectedTitle, sizeof(s_ui.selectedTitle), "%s", entry.title);
                s_ui.selectedSize = entry.size;
                s_ui.readerError[0] = '\0';
                if (!entry.directory) {
                    s_ui.detailReturnPage = s_ui.page;
                    s_ui.readerError[0] = '\0';
                    if (endsWithBook(entry.path)) {
                        if (murphyM4ProjectReaderOpen(entry.path)) {
                            ESP_LOGI(TAG, "opened book from UI: %s", entry.path);
                            selectPage(Page::Reader);
                            break;
                        }
                        std::snprintf(s_ui.readerError, sizeof(s_ui.readerError),
                                      "无法打开图书；请检查串口中的书源、字体或排版错误后重试。");
                        ESP_LOGW(TAG, "project book reader rejected: %s", entry.path);
                        selectPage(Page::BookDetail);
                        break;
                    } else if (s_ui.page == Page::Files && imageFormat(entry.path, nullptr)) {
                        if (loadImageAt(static_cast<size_t>(event.value))) {
                            s_ui.detailReturnPage = Page::Files;
                            selectPage(Page::ImageViewer);
                            break;
                        }
                        std::snprintf(s_ui.readerError, sizeof(s_ui.readerError),
                                      "图片无法解码或超出设备支持范围。");
                    }
                    selectPage(Page::BookDetail);
                }
            }
            break;
        case ACTION_BACK:
            if (s_ui.page == Page::ReaderToc) {
                selectPage(Page::Reader);
                break;
            }
            if (s_ui.page == Page::ReaderSettings) {
                selectPage(Page::Reader);
                break;
            }
            if (s_ui.page == Page::TimeSettings) {
                selectPage(Page::Settings);
                break;
            }
            if (s_ui.page == Page::BookDetail || s_ui.page == Page::Reader ||
                s_ui.page == Page::ImageViewer) {
                selectPage(s_ui.detailReturnPage);
            } else if (s_ui.page == Page::Files && std::strcmp(s_ui.currentDirectory, "/sdcard") != 0) {
                if (enterParentDirectory()) selectPage(Page::Files);
            } else {
                selectPage(Page::Home);
            }
            break;
        case ACTION_LIGHT_DOWN:
            s_ui.brightness = s_ui.brightness >= 10 ? static_cast<uint8_t>(s_ui.brightness - 10) : 0;
            s_ui.frontlightOn = s_ui.brightness > 0;
            updateLight();
            s_ui.app->invalidate(RefreshHint::Fast);
            break;
        case ACTION_LIGHT_UP:
            s_ui.brightness = s_ui.brightness <= 90 ? static_cast<uint8_t>(s_ui.brightness + 10) : 100;
            s_ui.frontlightOn = true;
            updateLight();
            s_ui.app->invalidate(RefreshHint::Fast);
            break;
        case ACTION_LIGHT_TOGGLE:
            s_ui.frontlightOn = !s_ui.frontlightOn;
            if (s_ui.frontlightOn && s_ui.brightness == 0) s_ui.brightness = 30;
            updateLight();
            s_ui.app->invalidate(RefreshHint::Fast);
            break;
        case ACTION_FONT_NEXT: {
            const char *current = app_settings_font_path();
            const int count = ttf_font_scan();
            int next = -1;
            if (current[0] == '\0') {
                next = count > 0 ? 0 : -2;
            } else {
                for (int i = 0; i < count; ++i) {
                    const ttf_font_item_t *item = ttf_font_item(i);
                    if (item && !std::strcmp(item->path, current)) {
                        next = i + 1 < count ? i + 1 : -2;
                        break;
                    }
                }
                if (next == -1) next = count > 0 ? 0 : -2;
            }
            const ttf_font_item_t *item = next >= 0 ? ttf_font_item(next) : nullptr;
            const char *path = next == -2 ? "" : item ? item->path : nullptr;
            const esp_err_t opened = path && (path[0] ? ttf_font_open(path) : ttf_font_open_builtin()) == ESP_OK
                                         ? ESP_OK : ESP_FAIL;
            if (opened == ESP_OK) {
                app_settings_set_font_path(path);
                (void)murphyM4ProjectReaderRefresh();
            } else ESP_LOGW(TAG, "cannot switch M4 reader font");
            s_ui.app->invalidate(RefreshHint::Fast);
            break;
        }
        default: break;
    }
}

static InputSnapshot readInput(const DeviceContext& device) {
    InputSnapshot snapshot;
    float nx = 0;
    float ny = 0;
    if (s_ui.input->wasTouchTap(nx, ny)) {
        const Point point = touchToLogical(device, nx, ny);
        snapshot.touchReleased = true;
        snapshot.touchX = point.x;
        snapshot.touchY = point.y;
    }
    float sx = 0, sy = 0, ex = 0, ey = 0;
    if (s_ui.input->wasSwipe(sx, sy, ex, ey)) {
        const Point start = touchToLogical(device, sx, sy);
        const Point end = touchToLogical(device, ex, ey);
        const int dx = static_cast<int>(end.x) - start.x;
        const int dy = static_cast<int>(end.y) - start.y;
        if (std::abs(dy) > std::abs(dx)) {
            snapshot.swipeLeft = dy < 0;
            snapshot.swipeRight = dy > 0;
        } else {
            snapshot.swipeLeft = dx < 0;
            snapshot.swipeRight = dx > 0;
        }
        snapshot.touchX = end.x;
        snapshot.touchY = end.y;
    }
    snapshot.focusPrev = s_ui.input->wasPressed(InputManager::BTN_UP);
    snapshot.focusNext = s_ui.input->wasPressed(InputManager::BTN_DOWN);
    snapshot.prev = s_ui.input->wasPressed(InputManager::BTN_LEFT);
    snapshot.confirm = s_ui.input->wasPressed(InputManager::BTN_CONFIRM);
    snapshot.next = s_ui.input->wasPressed(InputManager::BTN_RIGHT);
    snapshot.back = s_ui.input->wasPressed(InputManager::BTN_BACK);
    return snapshot;
}

static bool armButtonWake(const int8_t pin, const gpio_int_type_t level) {
    if (pin < 0) return false;
    const esp_err_t error = gpio_wakeup_enable(static_cast<gpio_num_t>(pin), level);
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "cannot enable sleep wake on GPIO%d: %s", pin, esp_err_to_name(error));
        return false;
    }
    return true;
}

static bool buttonIsPressed(const int8_t pin, const bool activeHigh = false) {
    return pin >= 0 && gpio_get_level(static_cast<gpio_num_t>(pin)) == (activeHigh ? 1 : 0);
}

static void disarmButtonWake(const int8_t pin) {
    if (pin >= 0) (void)gpio_wakeup_disable(static_cast<gpio_num_t>(pin));
}

static void enterLightSleep(InputManager& input) {
    const auto& pins = BoardConfig::ACTIVE.input;
    while (buttonIsPressed(pins.power, pins.powerActiveHigh) ||
           buttonIsPressed(pins.up) || buttonIsPressed(pins.down)) {
        input.update();
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    bool armed = armButtonWake(pins.power, pins.powerActiveHigh ? GPIO_INTR_HIGH_LEVEL : GPIO_INTR_LOW_LEVEL);
    armed = armButtonWake(pins.up, GPIO_INTR_LOW_LEVEL) && armed;
    armed = armButtonWake(pins.down, GPIO_INTR_LOW_LEVEL) && armed;
    if (!armed || esp_sleep_enable_gpio_wakeup() != ESP_OK) {
        ESP_LOGE(TAG, "M4 light sleep not entered: button wake setup failed");
        disarmButtonWake(pins.power);
        disarmButtonWake(pins.up);
        disarmButtonWake(pins.down);
        return;
    }

    const uint8_t restoreBrightness = s_ui.frontlightOn ? s_ui.brightness : 0;
    s_ui.frontlight->setBrightness(0);
    ESP_LOGI(TAG, "entering light sleep; wake GPIOs power=%d up=%d down=%d",
             pins.power, pins.up, pins.down);
    const esp_err_t error = esp_light_sleep_start();
    const esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    ESP_LOGI(TAG, "left light sleep: result=%s wake_cause=%d",
             esp_err_to_name(error), static_cast<int>(cause));

    s_ui.frontlight->setBrightness(restoreBrightness);
    refreshDeviceStatus();
    s_ui.app->invalidate(RefreshHint::Fast);
    input.update();
    while (buttonIsPressed(pins.power, pins.powerActiveHigh) ||
           buttonIsPressed(pins.up) || buttonIsPressed(pins.down)) {
        input.update();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    disarmButtonWake(pins.power);
    disarmButtonWake(pins.up);
    disarmButtonWake(pins.down);
}

}  // namespace

void murphyM4UiRun(FreeInkDisplay& display, InputManager& input, FrontlightManager& frontlight,
                   BatteryMonitor& battery, Rtc& rtc, const bool storageReady) {
    s_ui.display = &display;
    s_ui.input = &input;
    s_ui.frontlight = &frontlight;
    s_ui.battery = &battery;
    s_ui.rtc = &rtc;
    s_ui.storageReady = storageReady;

    const esp_err_t nvsInit = nvs_flash_init();
    if (nvsInit == ESP_OK) {
        m4ReaderSettingsInit();
        if (nvs_open("m4_ui", NVS_READWRITE, &s_ui.settings) == ESP_OK) {
            s_ui.settingsReady = true;
            (void)nvs_get_u8(s_ui.settings, "light", &s_ui.brightness);
            uint8_t lightOn = 0;
            (void)nvs_get_u8(s_ui.settings, "light_on", &lightOn);
            s_ui.frontlightOn = lightOn != 0;
            uint8_t shelfSort = SHELF_SORT_NAME_ASC;
            if (nvs_get_u8(s_ui.settings, "shelf_sort", &shelfSort) == ESP_OK &&
                shelfSort <= SHELF_SORT_ADDED) s_ui.shelfSort = shelfSort;
        }
    } else {
        ESP_LOGW(TAG, "NVS unavailable; UI setting changes are not persisted: %s", esp_err_to_name(nvsInit));
    }
    frontlight.setBrightness(s_ui.frontlightOn ? s_ui.brightness : 0);

    s_ui.shelfCacheAttempted = true;
    if (storageReady && loadProjectShelfCache()) {
        sortShelfEntries();
        s_ui.shelfEntriesDirty.store(false, std::memory_order_release);
        ESP_LOGI(TAG, "shelf first frame uses project index: %u books",
                 static_cast<unsigned>(s_ui.entryCount));
    }

    s_ui.gray = static_cast<uint8_t*>(heap_caps_malloc(GRAY2_BYTES, MALLOC_CAP_SPIRAM));
    s_ui.imageGray4 = static_cast<uint8_t*>(heap_caps_malloc(UI_GRAY4_BYTES, MALLOC_CAP_SPIRAM));
    s_ui.imageGray2 = static_cast<uint8_t*>(heap_caps_malloc(UI_GRAY2_BYTES, MALLOC_CAP_SPIRAM));
    s_ui.coverGray = static_cast<uint8_t*>(heap_caps_malloc(COVER_GRAY_BYTES * SHELF_PAGE_SIZE, MALLOC_CAP_SPIRAM));
    s_ui.coverGray2 = static_cast<uint8_t*>(heap_caps_malloc(COVER_GRAY2_BYTES * SHELF_PAGE_SIZE, MALLOC_CAP_SPIRAM));
    s_ui.coverMutex = xSemaphoreCreateMutex();
    s_ui.lsb = static_cast<uint8_t*>(heap_caps_calloc(1, PLANE_BYTES, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
    s_ui.msb = static_cast<uint8_t*>(heap_caps_calloc(1, PLANE_BYTES, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
    s_ui.fontMemory = static_cast<uint8_t*>(heap_caps_malloc(FONT_ARENA_BYTES, MALLOC_CAP_SPIRAM));
    if (!s_ui.gray || !s_ui.imageGray4 || !s_ui.imageGray2 || !s_ui.lsb || !s_ui.msb || !s_ui.fontMemory) {
        ESP_LOGE(TAG, "UI framebuffer allocation failed");
        return;
    }
    std::memset(s_ui.imageGray4, 0xff, UI_GRAY4_BYTES);
    std::memset(s_ui.imageGray2, 0xff, UI_GRAY2_BYTES);
    if (!s_ui.coverGray || !s_ui.coverGray2 || !s_ui.coverMutex) {
        heap_caps_free(s_ui.coverGray);
        heap_caps_free(s_ui.coverGray2);
        if (s_ui.coverMutex != nullptr) vSemaphoreDelete(s_ui.coverMutex);
        s_ui.coverGray = nullptr;
        s_ui.coverGray2 = nullptr;
        s_ui.coverMutex = nullptr;
        ESP_LOGW(TAG, "shelf cover cache unavailable; showing title placeholders");
    }
    s_ui.fontArena.init(s_ui.fontMemory, FONT_ARENA_BYTES);
    if (s_ui.font.init(builtin_ttf_start, static_cast<uint32_t>(builtin_ttf_end - builtin_ttf_start),
                       s_ui.fontArena)) {
        s_ui.glyphSource.setFont(&s_ui.font);
    } else {
        ESP_LOGW(TAG, "CJK UI font unavailable; bitmap fallback remains active");
    }

    std::memset(s_ui.gray, 0xff, GRAY2_BYTES);
    DisplayTarget target(s_ui.gray, PANEL_WIDTH, PANEL_HEIGHT, PANEL_WIDTH / 4,
                         Orientation::Portrait, DisplayPixelFormat::Gray2);
    target.setGlyphFallback(&s_ui.glyphSource);
    DeviceContext device = target.deviceContext();
    device.hasTouch = true;
    device.hasButtons = true;
    device.minTouchSize = 48;
    s_ui.app.reset(new (std::nothrow) FreeInkApp<32>(target, device));
    if (!s_ui.app) {
        ESP_LOGE(TAG, "UI app allocation failed");
        return;
    }
    ThemeTokens theme = themeTokensForLineHeight(target.lineHeight(0));
    theme.rowHeight = 56;
    // The project header has two explicit lines; reserve both on the 480x800
    // portrait canvas so title and subtitle never share a baseline.
    // Native project content starts at y=176 and its navigation occupies the
    // final 120 px. The M4 portrait canvas keeps those proportions instead of
    // using a compact generic FreeInk header/footer.
    theme.headerHeight = 92;
    theme.footerHeight = 78;
    theme.minTouchSize = 48;
    theme.spaceSm = 8;
    theme.spaceMd = 12;
    s_ui.app->setTheme(theme);
    s_ui.app->setClearColor(Color::White);
    s_ui.app->setTransitionFullEvery(4);
    s_ui.app->on(ACTION_TAB, handleAction, nullptr);
    s_ui.app->on(ACTION_HOME_SHELF, handleAction, nullptr);
    s_ui.app->on(ACTION_HOME_FILES, handleAction, nullptr);
    s_ui.app->on(ACTION_HOME_SETTINGS, handleAction, nullptr);
    s_ui.app->on(ACTION_SETTINGS_ROW, handleAction, nullptr);
    s_ui.app->on(ACTION_TIME_FIELD_UP, handleAction, nullptr);
    s_ui.app->on(ACTION_TIME_FIELD_DOWN, handleAction, nullptr);
    s_ui.app->on(ACTION_TIME_SAVE, handleAction, nullptr);
    s_ui.app->on(ACTION_TIME_CANCEL, handleAction, nullptr);
    s_ui.app->on(ACTION_ENTRY, handleAction, nullptr);
    s_ui.app->on(ACTION_OPEN_READER, handleAction, nullptr);
    s_ui.app->on(ACTION_BACK, handleAction, nullptr);
    s_ui.app->on(ACTION_LIGHT_DOWN, handleAction, nullptr);
    s_ui.app->on(ACTION_LIGHT_UP, handleAction, nullptr);
    s_ui.app->on(ACTION_LIGHT_TOGGLE, handleAction, nullptr);
    s_ui.app->on(ACTION_READER_PREVIOUS, handleAction, nullptr);
    s_ui.app->on(ACTION_READER_NEXT, handleAction, nullptr);
    s_ui.app->on(ACTION_SHELF_PREVIOUS, handleAction, nullptr);
    s_ui.app->on(ACTION_SHELF_NEXT, handleAction, nullptr);
    s_ui.app->on(ACTION_SHELF_SORT, handleAction, nullptr);
    s_ui.app->on(ACTION_FONT_NEXT, handleAction, nullptr);
    s_ui.app->on(ACTION_IMAGE_PREVIOUS, handleAction, nullptr);
    s_ui.app->on(ACTION_IMAGE_NEXT, handleAction, nullptr);
    s_ui.app->on(ACTION_READER_TOC, handleAction, nullptr);
    s_ui.app->on(ACTION_TOC_SELECT, handleAction, nullptr);
    s_ui.app->on(ACTION_TOC_PREVIOUS, handleAction, nullptr);
    s_ui.app->on(ACTION_TOC_NEXT, handleAction, nullptr);
    s_ui.app->on(ACTION_READER_SETTINGS, handleAction, nullptr);
    s_ui.app->on(ACTION_FONT_DOWN, handleAction, nullptr);
    s_ui.app->on(ACTION_FONT_UP, handleAction, nullptr);
    s_ui.app->on(ACTION_READER_VERTICAL, handleAction, nullptr);
    s_ui.app->on(ACTION_READER_IMAGES, handleAction, nullptr);
    s_ui.app->on(ACTION_LINE_DOWN, handleAction, nullptr);
    s_ui.app->on(ACTION_LINE_UP, handleAction, nullptr);
    s_ui.app->on(ACTION_READER_BOOK_FONTS, handleAction, nullptr);
    s_ui.app->on(ACTION_READER_FAVORITE, handleAction, nullptr);
    refreshDeviceStatus();
    selectPage(Page::Home);
    s_ui.app->render();
    present();
    ESP_LOGI(TAG, "responsive UI ready: %ux%u portrait on %ux%u panel, 4 gray levels",
             UI_WIDTH, UI_HEIGHT, PANEL_WIDTH, PANEL_HEIGHT);

    for (;;) {
        input.update();
        if (millis() - s_ui.deviceStatusUpdatedAt >= 60000u) {
            refreshDeviceStatus();
            s_ui.app->invalidate(RefreshHint::Fast);
        }
        if (input.wasPressed(InputManager::BTN_POWER)) {
            enterLightSleep(input);
            continue;
        }
        const InputSnapshot event = readInput(device);
        if (s_ui.page == Page::Reader && (event.next || event.swipeLeft)) {
            if (murphyM4ProjectReaderNext()) s_ui.app->invalidate(RefreshHint::Fast);
        } else if (s_ui.page == Page::Reader && (event.prev || event.swipeRight)) {
            if (murphyM4ProjectReaderPrevious()) s_ui.app->invalidate(RefreshHint::Fast);
        } else if (s_ui.page == Page::Reader && event.touchReleased && event.touchY < 736) {
            // Match the project's reading gesture on the narrow M4 canvas: edge taps turn
            // pages, while the center remains available for future reader controls.
            if (event.touchX < UI_WIDTH / 3) {
                const bool changed = murphyM4ProjectReaderPrevious();
                ESP_LOGI(TAG, "reader tap: left x=%d y=%d changed=%d page=%u/%u",
                         event.touchX, event.touchY, changed ? 1 : 0,
                         static_cast<unsigned>(murphyM4ProjectReaderPage()),
                         static_cast<unsigned>(murphyM4ProjectReaderPageCount()));
                if (changed) s_ui.app->invalidate(RefreshHint::Fast);
            } else if (event.touchX >= (UI_WIDTH * 2) / 3) {
                const bool changed = murphyM4ProjectReaderNext();
                ESP_LOGI(TAG, "reader tap: right x=%d y=%d changed=%d page=%u/%u",
                         event.touchX, event.touchY, changed ? 1 : 0,
                         static_cast<unsigned>(murphyM4ProjectReaderPage()),
                         static_cast<unsigned>(murphyM4ProjectReaderPageCount()));
                if (changed) s_ui.app->invalidate(RefreshHint::Fast);
            }
        } else if (s_ui.page == Page::Reader && event.back) {
            selectPage(s_ui.detailReturnPage);
        } else if ((s_ui.page == Page::ReaderToc || s_ui.page == Page::ReaderSettings) && event.back) {
            selectPage(Page::Reader);
        } else if (s_ui.page == Page::ImageViewer && (event.next || event.swipeLeft)) {
            if (moveImage(1)) s_ui.app->invalidate(RefreshHint::Fast);
        } else if (s_ui.page == Page::ImageViewer && (event.prev || event.swipeRight)) {
            if (moveImage(-1)) s_ui.app->invalidate(RefreshHint::Fast);
        } else if (s_ui.page == Page::ImageViewer && event.back) {
            selectPage(s_ui.detailReturnPage);
        } else if (event.touchReleased || event.touchPressed || event.confirm || event.back || event.prev || event.next ||
                   event.swipeLeft || event.swipeRight || event.focusNext || event.focusPrev) {
            (void)s_ui.app->route(event);
        }
        if (s_ui.app->invalidated()) {
            s_ui.app->render();
            present();
        }
        if (s_ui.coverChanged.exchange(false, std::memory_order_acq_rel) && s_ui.page == Page::Shelf)
            s_ui.app->invalidate(RefreshHint::Fast);
        if (s_ui.shelfScanDone.exchange(false, std::memory_order_acq_rel) && s_ui.page == Page::Shelf) {
            scheduleShelfCovers();
            s_ui.app->invalidate(RefreshHint::Fast);
        }
        vTaskDelay(pdMS_TO_TICKS(8));
    }
}

extern "C" bool m4_project_platform_storage_ready(void) {
    return s_ui.storageReady;
}

extern "C" bool m4_project_platform_touch_read(void *touch) {
    if (!touch) return false;
    /* Shared project pages use the CST836U-shaped ABI. M4's FT6336U is
       sampled by the FreeInk InputManager and translated by this bridge. */
    return m4_input_bridge_read(static_cast<cst836u_touch_t *>(touch));
}
