#include "settings.h"
#include "log.h"

#include "../third_party/cJSON.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void settings_apply_defaults(Settings *s) {
    memset(s, 0, sizeof(*s));
    snprintf(s->city, sizeof(s->city), "New York City");
    snprintf(s->country, sizeof(s->country), "United States");
    s->has_city = true;
    s->has_country = true;
    s->update_interval_seconds = 60;
    snprintf(s->api_provider, sizeof(s->api_provider), "open-meteo");
}

void settings_set_path(const char *path) {
    snprintf(g_app.settings_path, sizeof(g_app.settings_path), "%s", path);
}

static char *read_file(const char *path, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        return NULL;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    long n = ftell(f);
    if (n < 0) {
        fclose(f);
        return NULL;
    }
    rewind(f);
    char *buf = malloc((size_t)n + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    size_t got = fread(buf, 1, (size_t)n, f);
    fclose(f);
    buf[got] = '\0';
    if (out_len) {
        *out_len = got;
    }
    return buf;
}

static void json_copy_string(const cJSON *obj, const char *key, char *dst,
                             size_t n, bool *has) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (cJSON_IsString(item) && item->valuestring && item->valuestring[0]) {
        snprintf(dst, n, "%s", item->valuestring);
        if (has) {
            *has = true;
        }
    } else {
        dst[0] = '\0';
        if (has) {
            *has = false;
        }
    }
}

bool settings_load(void) {
    if (access(g_app.settings_path, F_OK) != 0) {
        settings_apply_defaults(&g_app.settings);
        if (!settings_save()) {
            log_errf("Не удалось создать %s\n", g_app.settings_path);
            return false;
        }
        log_errf("Создан файл settings.json с дефолтными значениями: %s\n",
                 g_app.settings_path);
        return true;
    }

    char *data = read_file(g_app.settings_path, NULL);
    if (!data) {
        log_errf("Ошибка чтения %s\n", g_app.settings_path);
        settings_apply_defaults(&g_app.settings);
        return false;
    }

    cJSON *root = cJSON_Parse(data);
    free(data);
    if (!root) {
        log_errf("Ошибка парсинга settings.json\n");
        settings_apply_defaults(&g_app.settings);
        return false;
    }

    Settings s;
    settings_apply_defaults(&s);

    json_copy_string(root, "city", s.city, sizeof(s.city), &s.has_city);
    json_copy_string(root, "country", s.country, sizeof(s.country),
                     &s.has_country);
    json_copy_string(root, "apiProvider", s.api_provider, sizeof(s.api_provider),
                     NULL);
    if (!s.api_provider[0]) {
        snprintf(s.api_provider, sizeof(s.api_provider), "open-meteo");
    }

    json_copy_string(root, "apiKey", s.api_key, sizeof(s.api_key),
                     &s.has_api_key);

    const cJSON *lat = cJSON_GetObjectItemCaseSensitive(root, "latitude");
    if (cJSON_IsNumber(lat)) {
        s.latitude = lat->valuedouble;
        s.has_latitude = true;
    } else {
        s.has_latitude = false;
    }

    const cJSON *lon = cJSON_GetObjectItemCaseSensitive(root, "longitude");
    if (cJSON_IsNumber(lon)) {
        s.longitude = lon->valuedouble;
        s.has_longitude = true;
    } else {
        s.has_longitude = false;
    }

    cJSON *interval =
        cJSON_GetObjectItemCaseSensitive(root, "updateIntervalInSeconds");
    if (cJSON_IsNumber(interval) && interval->valuedouble >= 1) {
        s.update_interval_seconds = (int)interval->valuedouble;
    } else {
        s.update_interval_seconds = 60;
    }

    cJSON_Delete(root);
    g_app.settings = s;
    return true;
}

bool settings_save(void) {
    const Settings *s = &g_app.settings;
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return false;
    }

    if (s->has_city) {
        cJSON_AddStringToObject(root, "city", s->city);
    } else {
        cJSON_AddNullToObject(root, "city");
    }
    if (s->has_country) {
        cJSON_AddStringToObject(root, "country", s->country);
    } else {
        cJSON_AddNullToObject(root, "country");
    }
    if (s->has_latitude) {
        cJSON_AddNumberToObject(root, "latitude", s->latitude);
    } else {
        cJSON_AddNullToObject(root, "latitude");
    }
    if (s->has_longitude) {
        cJSON_AddNumberToObject(root, "longitude", s->longitude);
    } else {
        cJSON_AddNullToObject(root, "longitude");
    }
    cJSON_AddNumberToObject(root, "updateIntervalInSeconds",
                            s->update_interval_seconds);
    cJSON_AddStringToObject(root, "apiProvider", s->api_provider);
    if (s->has_api_key) {
        cJSON_AddStringToObject(root, "apiKey", s->api_key);
    } else {
        cJSON_AddNullToObject(root, "apiKey");
    }

    char *printed = cJSON_Print(root);
    cJSON_Delete(root);
    if (!printed) {
        return false;
    }

    FILE *f = fopen(g_app.settings_path, "wb");
    if (!f) {
        free(printed);
        return false;
    }
    fputs(printed, f);
    fputc('\n', f);
    fclose(f);
    free(printed);
    return true;
}

bool settings_validate(const Settings *s, char *err, size_t errlen) {
    bool has_city_country = s->has_city && s->has_country;
    bool has_coords = s->has_latitude && s->has_longitude;
    bool partial_city = s->has_city != s->has_country;
    bool partial_coords = s->has_latitude != s->has_longitude;

    if (partial_city || partial_coords) {
        if (err) {
            snprintf(err, errlen,
                     "Укажите либо оба поля город+страна, либо оба "
                     "поля широта+долгота");
        }
        return false;
    }
    if (has_city_country && has_coords) {
        if (err) {
            snprintf(err, errlen,
                     "Нельзя одновременно указывать город/страну и координаты");
        }
        return false;
    }
    if (!has_city_country && !has_coords) {
        if (err) {
            snprintf(err, errlen,
                     "Необходимо указать либо город и страну, либо координаты");
        }
        return false;
    }
    if (s->update_interval_seconds < 1) {
        if (err) {
            snprintf(err, errlen, "Интервал обновления должен быть >= 1");
        }
        return false;
    }
    if (s->has_latitude && (s->latitude < -90.0 || s->latitude > 90.0)) {
        if (err) {
            snprintf(err, errlen, "Широта должна быть в диапазоне -90…90");
        }
        return false;
    }
    if (s->has_longitude && (s->longitude < -180.0 || s->longitude > 180.0)) {
        if (err) {
            snprintf(err, errlen, "Долгота должна быть в диапазоне -180…180");
        }
        return false;
    }
    if (strcmp(s->api_provider, "open-meteo") != 0 &&
        strcmp(s->api_provider, "openweathermap") != 0) {
        if (err) {
            snprintf(err, errlen, "Неизвестный apiProvider");
        }
        return false;
    }
    if (strcmp(s->api_provider, "openweathermap") == 0 && !s->has_api_key) {
        if (err) {
            snprintf(err, errlen, "Для OpenWeatherMap нужен apiKey");
        }
        return false;
    }
    return true;
}
