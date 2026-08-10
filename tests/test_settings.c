#include "tempfile.h"
#include "test.h"

#include "app.h"
#include "settings.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

static Settings valid_city_settings(void) {
    Settings s;
    settings_apply_defaults(&s);
    return s;
}

static Settings valid_coord_settings(void) {
    Settings s;
    memset(&s, 0, sizeof(s));
    s.has_latitude = true;
    s.has_longitude = true;
    s.latitude = 40.7;
    s.longitude = -74.0;
    s.update_interval_seconds = 30;
    snprintf(s.api_provider, sizeof(s.api_provider), "open-meteo");
    return s;
}

void test_settings(void) {
    TEST_SUITE("settings");

    Settings def;
    settings_apply_defaults(&def);
    EXPECT_STREQ(def.city, "New York City");
    EXPECT_STREQ(def.country, "United States");
    EXPECT_TRUE(def.has_city);
    EXPECT_TRUE(def.has_country);
    EXPECT_EQ_INT(def.update_interval_seconds, 60);
    EXPECT_STREQ(def.api_provider, "open-meteo");

    char err[256];
    Settings s = valid_city_settings();
    EXPECT_TRUE(settings_validate(&s, err, sizeof(err)));

    s = valid_coord_settings();
    EXPECT_TRUE(settings_validate(&s, err, sizeof(err)));

    s = valid_city_settings();
    s.has_country = false;
    EXPECT_FALSE(settings_validate(&s, err, sizeof(err)));

    s = valid_city_settings();
    s.has_latitude = true;
    s.has_longitude = true;
    s.latitude = 1.0;
    s.longitude = 2.0;
    EXPECT_FALSE(settings_validate(&s, err, sizeof(err)));

    s = valid_city_settings();
    s.has_city = false;
    s.has_country = false;
    EXPECT_FALSE(settings_validate(&s, err, sizeof(err)));

    s = valid_city_settings();
    s.update_interval_seconds = 0;
    EXPECT_FALSE(settings_validate(&s, err, sizeof(err)));

    s = valid_coord_settings();
    s.latitude = 91.0;
    EXPECT_FALSE(settings_validate(&s, err, sizeof(err)));

    s = valid_coord_settings();
    s.longitude = -181.0;
    EXPECT_FALSE(settings_validate(&s, err, sizeof(err)));

    s = valid_city_settings();
    snprintf(s.api_provider, sizeof(s.api_provider), "nope");
    EXPECT_FALSE(settings_validate(&s, err, sizeof(err)));

    s = valid_city_settings();
    snprintf(s.api_provider, sizeof(s.api_provider), "openweathermap");
    s.has_api_key = false;
    EXPECT_FALSE(settings_validate(&s, err, sizeof(err)));

    s = valid_city_settings();
    snprintf(s.api_provider, sizeof(s.api_provider), "openweathermap");
    snprintf(s.api_key, sizeof(s.api_key), "secret");
    s.has_api_key = true;
    EXPECT_TRUE(settings_validate(&s, err, sizeof(err)));

    /* Round-trip save/load via a temp file. */
    char path[512];
    int fd = test_mkstemp(path, sizeof(path), "c-weather-settings");
    EXPECT_TRUE(fd >= 0);
    if (fd >= 0) {
        close(fd);
        unlink(path);

        memset(&g_app, 0, sizeof(g_app));
        settings_set_path(path);
        g_app.settings = valid_coord_settings();
        EXPECT_TRUE(settings_save());

        memset(&g_app.settings, 0, sizeof(g_app.settings));
        EXPECT_TRUE(settings_load());
        EXPECT_TRUE(g_app.settings.has_latitude);
        EXPECT_TRUE(g_app.settings.has_longitude);
        EXPECT_FALSE(g_app.settings.has_city);
        EXPECT_EQ_INT(g_app.settings.update_interval_seconds, 30);
        EXPECT_STREQ(g_app.settings.api_provider, "open-meteo");
        EXPECT_TRUE(g_app.settings.latitude > 40.0 &&
                    g_app.settings.latitude < 41.0);
        EXPECT_TRUE(g_app.settings.longitude < -73.0 &&
                    g_app.settings.longitude > -75.0);

        unlink(path);
    }

    /* Missing file: load creates defaults. */
    char path2[512];
    fd = test_mkstemp(path2, sizeof(path2), "c-weather-settings-missing");
    EXPECT_TRUE(fd >= 0);
    if (fd >= 0) {
        close(fd);
        unlink(path2);
        memset(&g_app, 0, sizeof(g_app));
        settings_set_path(path2);
        EXPECT_TRUE(settings_load());
        EXPECT_STREQ(g_app.settings.city, "New York City");
        EXPECT_TRUE(access(path2, F_OK) == 0);
        unlink(path2);
    }
}
