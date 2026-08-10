#include "tempfile.h"
#include "test.h"

#include "icon.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

static bool file_starts_with_png(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        return false;
    }
    unsigned char mag[8];
    size_t n = fread(mag, 1, sizeof(mag), f);
    fclose(f);
    if (n != 8) {
        return false;
    }
    static const unsigned char png[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a,
                                         '\n'};
    return memcmp(mag, png, 8) == 0;
}

void test_icon(void) {
    TEST_SUITE("icon");

    char path[512];
    int fd = test_mkstemp(path, sizeof(path), "c-weather-icon");
    EXPECT_TRUE(fd >= 0);
    if (fd < 0) {
        return;
    }
    close(fd);

    EXPECT_TRUE(icon_write_temp_png(path, "21°"));
    EXPECT_TRUE(file_starts_with_png(path));

    EXPECT_TRUE(icon_write_temp_png(path, "--"));
    EXPECT_TRUE(file_starts_with_png(path));

    EXPECT_TRUE(icon_write_weather_png(path, 0));
    EXPECT_TRUE(file_starts_with_png(path));

    EXPECT_TRUE(icon_write_weather_png(path, 95));
    EXPECT_TRUE(file_starts_with_png(path));

    EXPECT_TRUE(icon_write_weather_png(path, -1));
    EXPECT_TRUE(file_starts_with_png(path));

    EXPECT_TRUE(icon_write_loading_png(path));
    EXPECT_TRUE(file_starts_with_png(path));

    unlink(path);
}
