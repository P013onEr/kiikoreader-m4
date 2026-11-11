#include <assert.h>
#include <string.h>
#include "storage_scan_filter.h"

int main(void) {
    const char *metadata[] = {
        ".DS_Store", "._cover.epub", "Thumbs.db", "ehthumbs.db",
        "desktop.ini", "folder.htt", "IconCache.db", "IndexerVolumeGuid",
        "WPSettings.dat", "$RECYCLE.BIN", "RECYCLER",
        "System Volume Information", "__MACOSX", "Temporary Items",
    };
    for (unsigned i = 0; i < sizeof(metadata) / sizeof(metadata[0]); ++i)
        assert(storage_name_is_os_metadata(metadata[i]));
    assert(storage_name_is_os_directory("System Volume Information"));
    assert(storage_name_is_os_directory("$RECYCLE.BIN"));
    assert(storage_name_is_os_directory(".Spotlight-V100"));
    assert(storage_name_is_os_directory(".Trashes"));
    assert(storage_name_is_os_directory(".fseventsd"));
    assert(!storage_name_is_os_metadata("my-book.epub"));
    assert(!storage_name_is_os_metadata("User Fonts"));

    const char *temporary[] = {
        "~$locked.epub", "partial.epub.part", "font.ttf.tmp",
        "download.crdownload", "download.download", "scratch.temp",
    };
    for (unsigned i = 0; i < sizeof(temporary) / sizeof(temporary[0]); ++i)
        assert(storage_file_is_temporary(temporary[i]));
    assert(!storage_file_is_temporary("archive.epub"));
    assert(!storage_file_is_temporary("notes.backup"));
    return 0;
}
