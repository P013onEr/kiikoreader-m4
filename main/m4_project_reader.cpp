/* SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0 */
#include "m4_project_reader.h"
#include "m4_reader.h"

extern "C" {
#include "m4_gray2.h"
#include "m4_image_draw.h"
#include "book_cover.h"
#include "book_epub.h"
#include "book_source.h"
#include "book_title.h"
#include "book_layout.h"
#include "book_progress.h"
#include "book_store.h"
#include "html_text.h"
#include "m4_reader_settings.h"
#include "ttf_font.h"
}

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>

namespace {

constexpr char kTag[] = "m4_project_reader";
constexpr int kWidth = 480;
constexpr int kHeight = 800;
constexpr size_t kFrame4Bytes = static_cast<size_t>(kWidth) * kHeight / 2u;
constexpr size_t kFrame2Bytes = static_cast<size_t>(kWidth) * kHeight / 4u;
// M4 uses a dedicated portrait text column.  Keep a clear footer/navigation
// reserve instead of scaling the 684x1216 body into the smaller panel.
constexpr EpdRect kBodyRect = {28, 112, 424, 596};

struct Reader {
    html_text_t chapterText{};
    uint8_t *frame4 = nullptr;
    uint8_t *frame2 = nullptr;
    char path[BOOK_STORE_PATH_MAX]{};
    char title[160]{};
    uint32_t fileSize = 0;
    uint8_t fontPx = 32;
    size_t chapter = 0;
    size_t page = 0;
    size_t pageCount = 0;
    size_t chapterTextOffset = 0;
    bool ready = false;
};

Reader s_reader;

static void clearChapter(Reader &reader) {
    book_layout_set_image_dims(nullptr, nullptr);
    book_layout_free();
    html_text_free(&reader.chapterText);
    reader.pageCount = 0;
    reader.page = 0;
}

static int scaledFontPx() {
    const int projectPx = m4ReaderSettingsFontSize();
    // Settings are expressed in reader pixels for M4.  Do not apply the old
    // Pico height ratio: it made the body illegibly small on the 480x800 panel.
    return projectPx < 18 ? 18 : projectPx > 60 ? 60 : projectPx;
}

static void applyLayoutSettings(Reader &reader) {
    book_layout_set_spacing(m4ReaderSettingsLineSpacing(), 50);
    book_layout_set_images_visible(!m4ReaderSettingsHideImages());
    book_layout_set_vertical(m4ReaderSettingsVertical());
    reader.fontPx = static_cast<uint8_t>(scaledFontPx());
}

static bool ensureBuffers(Reader &reader) {
    if (reader.frame4 && reader.frame2) return true;
    auto *frame4 = static_cast<uint8_t *>(heap_caps_malloc(kFrame4Bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    auto *frame2 = static_cast<uint8_t *>(heap_caps_malloc(kFrame2Bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!frame4 || !frame2) {
        heap_caps_free(frame4);
        heap_caps_free(frame2);
        return false;
    }
    reader.frame4 = frame4;
    reader.frame2 = frame2;
    return true;
}

static bool loadChapter(Reader &reader, const size_t chapter, const char *anchor = nullptr,
                        const size_t sourceOffset = SIZE_MAX, size_t *targetTextOffset = nullptr) {
    clearChapter(reader);
    if (chapter >= book_chapter_count()) return false;
    size_t parsedTargetOffset = 0;
    const esp_err_t loadErr = anchor || sourceOffset != SIZE_MAX
        ? book_chapter_load_blocks_target(chapter, anchor, sourceOffset,
                                          &parsedTargetOffset, &reader.chapterText)
        : book_chapter_load_blocks(chapter, &reader.chapterText);
    if (loadErr != ESP_OK) return false;
    reader.chapterTextOffset = parsedTargetOffset;
    if (targetTextOffset) *targetTextOffset = parsedTargetOffset;
    reader.chapter = chapter;
    book_layout_set_image_dims([](void *ctx, const int image, int *width, int *height) {
        auto *current = static_cast<Reader *>(ctx);
        if (!current || image < 0 || static_cast<size_t>(image) >= current->chapterText.image_count) return false;
        unsigned sourceWidth = 0;
        unsigned sourceHeight = 0;
        if (book_chapter_image_dimensions(current->chapter,
                                       current->chapterText.images[image],
                                       &sourceWidth, &sourceHeight) != ESP_OK ||
            sourceWidth == 0 || sourceHeight == 0 || sourceWidth > INT_MAX || sourceHeight > INT_MAX) {
            return false;
        }
        for (size_t note = 0; note < current->chapterText.note_count; ++note) {
            if (current->chapterText.notes[note].image != image) continue;
            const unsigned box = current->fontPx > 0 ? current->fontPx : 20u;
            const unsigned noteHeight = static_cast<unsigned>((static_cast<uint64_t>(sourceHeight) * box) / sourceWidth);
            if (width) *width = static_cast<int>(box);
            if (height) *height = static_cast<int>(noteHeight ? noteHeight : 1u);
            return true;
        }
        if (width) *width = static_cast<int>(sourceWidth);
        if (height) *height = static_cast<int>(sourceHeight);
        return true;
    }, &reader);
    book_layout_set_runs(reader.chapterText.runs, reader.chapterText.run_count);
    applyLayoutSettings(reader);
    const bool built = reader.chapterText.blocks
        ? book_layout_build_blocks(reader.chapterText.utf8, reader.chapterText.len,
                                   reader.chapterText.blocks, reader.chapterText.count,
                                   kBodyRect, reader.fontPx)
        : book_layout_build(reader.chapterText.utf8, reader.chapterText.len,
                            kBodyRect, reader.fontPx);
    if (!built) {
        clearChapter(reader);
        return false;
    }
    reader.pageCount = book_layout_page_count();
    reader.page = 0;
    return reader.pageCount > 0;
}

static bool renderPage(Reader &reader) {
    if (!reader.ready || !reader.frame4 || !reader.frame2 || reader.page >= reader.pageCount) return false;
    std::memset(reader.frame4, 0xff, kFrame4Bytes);
    book_layout_draw_page(reader.frame4, reader.page, kBodyRect, reader.fontPx);
    const EpdRect imageArea = book_layout_image_rect();
    const int visibleImages = book_layout_page_image_count(reader.page);
    const int imageSlots = visibleImages ? visibleImages : (book_layout_page_image(reader.page) >= 0 ? 1 : 0);
    for (int slot = 0; slot < imageSlots; ++slot) {
        int image = -1;
        int x = -1;
        int y = 0;
        int width = imageArea.width;
        int height = imageArea.height;
        if (visibleImages) {
            if (!book_layout_page_image_rect(reader.page, slot, &image, &x, &y, &width, &height)) continue;
        } else {
            image = book_layout_page_image(reader.page);
        }
        if (image < 0 || static_cast<size_t>(image) >= reader.chapterText.image_count ||
            width <= 0 || height <= 0) continue;

        uint8_t *encoded = nullptr;
        size_t encodedBytes = 0;
        bool png = false;
        esp_err_t err = book_chapter_image(reader.chapter, reader.chapterText.images[image],
                                        &encoded, &encodedBytes, &png);
        if (err != ESP_OK) {
            ESP_LOGW(kTag, "page image %d read failed: %s", image, esp_err_to_name(err));
            free(encoded);
            continue;
        }

        if (!visibleImages) {
            unsigned sourceWidth = 0;
            unsigned sourceHeight = 0;
            if (!book_image_dimensions(encoded, encodedBytes, png, &sourceWidth, &sourceHeight) ||
                sourceWidth == 0 || sourceHeight == 0) {
                free(encoded);
                continue;
            }
            const uint64_t widthLimitedHeight = static_cast<uint64_t>(sourceHeight) * width / sourceWidth;
            if (sourceWidth > static_cast<unsigned>(width)) {
                height = static_cast<int>(widthLimitedHeight ? widthLimitedHeight : 1u);
            } else {
                width = static_cast<int>(sourceWidth);
                height = static_cast<int>(sourceHeight);
            }
            if (height > imageArea.height) {
                width = static_cast<int>(static_cast<uint64_t>(width) * imageArea.height / height);
                height = imageArea.height;
            }
            if (width < 1 || height < 1) {
                free(encoded);
                continue;
            }
        }

        const size_t pixels = static_cast<size_t>(width) * static_cast<size_t>(height);
        auto *gray = static_cast<uint8_t *>(heap_caps_malloc(pixels, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        const bool decoded = gray && book_image_grayscale(encoded, encodedBytes, png,
                                                           static_cast<unsigned>(width),
                                                           static_cast<unsigned>(height), gray);
        free(encoded);
        if (!decoded) {
            ESP_LOGW(kTag, "page image %d grayscale decode failed (%dx%d)", image, width, height);
            heap_caps_free(gray);
            continue;
        }
        if (x < 0) x = imageArea.x + (imageArea.width - width) / 2;
        const int imageY = imageArea.y + y;
        const bool drawn = m4ProjectBlitGray8ToGray4(reader.frame4, kFrame4Bytes, kWidth, kHeight,
                                                      gray, pixels, static_cast<uint16_t>(width),
                                                      static_cast<uint16_t>(height), x, imageY);
        heap_caps_free(gray);
        if (!drawn) ESP_LOGW(kTag, "page image %d could not be composed", image);
        vTaskDelay(1);
    }
    return m4ProjectGray4ToGray2(reader.frame4, kFrame4Bytes,
                                 reader.frame2, kFrame2Bytes, kWidth, kHeight);
}

static void saveProgress(const Reader &reader) {
    if (!reader.ready || reader.chapter > UINT16_MAX || reader.fileSize == 0) return;
    const size_t chapterBase = book_chapter_byte_offset(reader.chapter);
    const size_t localPageOffset = book_layout_page_start_offset(reader.page);
    const size_t pageOffset = reader.chapterTextOffset + localPageOffset;
    const size_t offset = chapterBase + pageOffset;
    const size_t total = book_total_bytes();
    book_progress_t progress{};
    progress.file_size = reader.fileSize;
    progress.chapter = static_cast<uint16_t>(reader.chapter);
    progress.byte_off = static_cast<uint32_t>(pageOffset > UINT32_MAX ? UINT32_MAX : pageOffset);
    progress.px = reader.fontPx;
    progress.pct = total ? static_cast<uint8_t>((offset * 100u) / total) : 0;
    if (progress.pct > 100) progress.pct = 100;
    const esp_err_t err = book_progress_save(reader.path, &progress);
    if (err != ESP_OK) ESP_LOGW(kTag, "progress save failed: %s", esp_err_to_name(err));
}

static bool moveChapter(Reader &reader, const int direction) {
    const size_t count = book_chapter_count();
    size_t candidate = reader.chapter;
    while (direction > 0 ? ++candidate < count : candidate-- > 0) {
        if (!loadChapter(reader, candidate)) continue;
        reader.page = direction > 0 ? 0 : reader.pageCount - 1;
        if (renderPage(reader)) {
            saveProgress(reader);
            return true;
        }
    }
    return false;
}

} // namespace

bool murphyM4ProjectReaderOpen(const char *path) {
    Reader &reader = s_reader;
    if (!path || std::strlen(path) >= sizeof(reader.path)) {
        ESP_LOGE(kTag, "EPUB path is missing or too long");
        return false;
    }
    if (!ensureBuffers(reader)) {
        ESP_LOGE(kTag, "reader frame allocation failed: free_psram=%u largest=%u",
                 static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)),
                 static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)));
        return false;
    }
    struct stat st{};
    if (stat(path, &st) != 0 || st.st_size <= 0 || static_cast<uint64_t>(st.st_size) > UINT32_MAX) {
        ESP_LOGE(kTag, "EPUB stat/size rejected: %s", path);
        return false;
    }
    book_close();
    clearChapter(reader);
    reader.ready = false;
    reader.fileSize = static_cast<uint32_t>(st.st_size);
    std::snprintf(reader.path, sizeof(reader.path), "%s", path);
    const esp_err_t fontErr = ttf_font_init();
    const esp_err_t sourceErr = fontErr == ESP_OK ? book_open(path) : fontErr;
    if (sourceErr != ESP_OK) {
        ESP_LOGE(kTag, "project book/font initialization failed: %s (%s)", path,
                 esp_err_to_name(sourceErr));
        return false;
    }
    book_set_embedded_fonts(m4ReaderSettingsBookFonts());
    const size_t chapters = book_chapter_count();
    const bool titleOk = book_title_from_path(path, reader.title, sizeof(reader.title)) ||
                         book_chapter_title(0, reader.title, sizeof(reader.title)) == ESP_OK;
    if (chapters == 0 || !titleOk) {
        ESP_LOGE(kTag, "book source rejected: chapters=%u path=%s",
                 static_cast<unsigned>(chapters), path);
        return false;
    }
    reader.ready = true;
    book_progress_t progress{};
    bool restored = false;
    if (book_progress_load(path, reader.fileSize, &progress) &&
        progress.chapter < book_chapter_count() && loadChapter(reader, progress.chapter)) {
        reader.page = book_layout_page_for_offset(progress.byte_off);
        if (reader.page >= reader.pageCount) reader.page = reader.pageCount - 1;
        restored = true;
        ESP_LOGI(kTag, "restored project progress: chapter=%u offset=%u page=%u/%u",
                 static_cast<unsigned>(reader.chapter), static_cast<unsigned>(progress.byte_off),
                 static_cast<unsigned>(reader.page + 1), static_cast<unsigned>(reader.pageCount));
    }
    if (!restored) {
        uint16_t legacyChapter = 0;
        uint32_t legacyPage = 0;
        if (murphyM4ReaderStoredProgress(path, reader.fileSize, &legacyChapter, &legacyPage) &&
            legacyChapter < book_chapter_count() && loadChapter(reader, legacyChapter)) {
            reader.page = legacyPage < reader.pageCount ? legacyPage : reader.pageCount - 1;
            restored = true;
            ESP_LOGI(kTag, "migrating legacy FreeInk page position: chapter=%u page=%u/%u",
                     static_cast<unsigned>(reader.chapter), static_cast<unsigned>(reader.page + 1),
                     static_cast<unsigned>(reader.pageCount));
        }
    }
    if (!restored && !loadChapter(reader, 0)) {
        reader.ready = false;
        ESP_LOGE(kTag, "project chapter layout failed: chapter=0 bytes=%u path=%s",
                 static_cast<unsigned>(reader.chapterText.len), path);
        return false;
    }
    if (!renderPage(reader)) {
        reader.ready = false;
        ESP_LOGE(kTag, "project first page render failed: chapter=%u page=%u/%u path=%s",
                 static_cast<unsigned>(reader.chapter), static_cast<unsigned>(reader.page + 1),
                 static_cast<unsigned>(reader.pageCount), path);
        return false;
    }
    saveProgress(reader);
    ESP_LOGI(kTag, "opened with project parser/layout: chapters=%u pages=%u",
             static_cast<unsigned>(book_chapter_count()),
             static_cast<unsigned>(reader.pageCount));
    return true;
}

bool murphyM4ProjectReaderNext(void) {
    Reader &reader = s_reader;
    if (!reader.ready) return false;
    if (reader.page + 1 < reader.pageCount) {
        ++reader.page;
        if (!renderPage(reader)) return false;
        saveProgress(reader);
        return true;
    }
    return moveChapter(reader, 1);
}

bool murphyM4ProjectReaderPrevious(void) {
    Reader &reader = s_reader;
    if (!reader.ready) return false;
    if (reader.page > 0) {
        --reader.page;
        if (!renderPage(reader)) return false;
        saveProgress(reader);
        return true;
    }
    return moveChapter(reader, -1);
}

size_t murphyM4ProjectReaderTocCount(void) {
    return s_reader.ready ? book_navigation_count() : 0;
}

bool murphyM4ProjectReaderTocTitle(const size_t index, char *title, const size_t capacity) {
    if (!s_reader.ready || !title || !capacity) return false;
    return book_navigation_title(index, title, capacity) == ESP_OK;
}

bool murphyM4ProjectReaderJumpToToc(const size_t index) {
    Reader &reader = s_reader;
    if (!reader.ready || index >= book_navigation_count()) return false;
    const size_t chapter = book_navigation_chapter(index);
    if (chapter == SIZE_MAX) return false;
    const char *anchor = book_navigation_anchor(index);
    const size_t sourceOffset = book_navigation_source_offset(index);
    size_t targetTextOffset = 0;
    if (!loadChapter(reader, chapter, anchor, sourceOffset, &targetTextOffset)) return false;
    reader.page = book_layout_page_for_offset(targetTextOffset);
    if (reader.page >= reader.pageCount) reader.page = reader.pageCount - 1;
    if (!renderPage(reader)) return false;
    saveProgress(reader);
    return true;
}

static bool reflowCurrentPage() {
    Reader &reader = s_reader;
    if (!reader.ready) return false;
    const size_t chapter = reader.chapter;
    const size_t offset = reader.chapterTextOffset + book_layout_page_start_offset(reader.page);
    if (!loadChapter(reader, chapter)) return false;
    reader.page = book_layout_page_for_offset(offset);
    if (reader.page >= reader.pageCount) reader.page = reader.pageCount - 1;
    if (!renderPage(reader)) return false;
    saveProgress(reader);
    return true;
}

bool murphyM4ProjectReaderAdjustFont(const int direction) {
    if (!s_reader.ready || (direction != -1 && direction != 1)) return false;
    const int current = m4ReaderSettingsFontSize();
    const int next = current + direction * 4;
    if (next < 18 || next > 90 || next == current ||
        !m4ReaderSettingsSetFontSize(static_cast<uint8_t>(next))) return false;
    if (reflowCurrentPage()) return true;
    (void)m4ReaderSettingsSetFontSize(static_cast<uint8_t>(current));
    (void)reflowCurrentPage();
    return false;
}

bool murphyM4ProjectReaderToggleVertical(void) {
    if (!s_reader.ready) return false;
    const bool previous = m4ReaderSettingsVertical();
    if (!m4ReaderSettingsSetVertical(!previous)) return false;
    if (reflowCurrentPage()) return true;
    (void)m4ReaderSettingsSetVertical(previous);
    (void)reflowCurrentPage();
    return false;
}

bool murphyM4ProjectReaderToggleImages(void) {
    if (!s_reader.ready) return false;
    const bool previous = m4ReaderSettingsHideImages();
    if (!m4ReaderSettingsSetHideImages(!previous)) return false;
    if (reflowCurrentPage()) return true;
    (void)m4ReaderSettingsSetHideImages(previous);
    (void)reflowCurrentPage();
    return false;
}

bool murphyM4ProjectReaderToggleBookFonts(void) {
    Reader &reader = s_reader;
    if (!reader.ready) return false;
    const bool previous = m4ReaderSettingsBookFonts();
    if (!m4ReaderSettingsSetBookFonts(!previous)) return false;
    book_set_embedded_fonts(!previous);
    if (reflowCurrentPage()) return true;
    (void)m4ReaderSettingsSetBookFonts(previous);
    book_set_embedded_fonts(previous);
    (void)reflowCurrentPage();
    return false;
}

bool murphyM4ProjectReaderAdjustLineSpacing(const int direction) {
    if (!s_reader.ready || (direction != -1 && direction != 1)) return false;
    const int current = m4ReaderSettingsLineSpacing();
    const int next = current + direction * 10;
    if (next < 110 || next > 150 || next == current ||
        !m4ReaderSettingsSetLineSpacing(static_cast<uint8_t>(next))) return false;
    if (reflowCurrentPage()) return true;
    (void)m4ReaderSettingsSetLineSpacing(static_cast<uint8_t>(current));
    (void)reflowCurrentPage();
    return false;
}

uint8_t murphyM4ProjectReaderFontPx(void) { return s_reader.fontPx; }

bool murphyM4ProjectReaderRefresh(void) {
    Reader &reader = s_reader;
    if (!reader.ready || !renderPage(reader)) return false;
    saveProgress(reader);
    return true;
}

const uint8_t *murphyM4ProjectReaderFrame(size_t *byteCount) {
    if (byteCount) *byteCount = s_reader.frame2 ? kFrame2Bytes : 0;
    return s_reader.frame2;
}

const char *murphyM4ProjectReaderTitle(void) { return s_reader.title; }
uint32_t murphyM4ProjectReaderPage(void) { return static_cast<uint32_t>(s_reader.page + 1); }
uint32_t murphyM4ProjectReaderPageCount(void) { return static_cast<uint32_t>(s_reader.pageCount); }
