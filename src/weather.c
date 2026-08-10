#include "weather.h"

#include "compat.h"
#include "history.h"
#include "http.h"
#include "log.h"
#include "settings.h"

#include "../third_party/cJSON.h"

#include <stdio.h>
#include <string.h>

int convert_owm_to_wmo(int owm_code) {
    if (owm_code >= 200 && owm_code < 300) {
        return 95;
    }
    if (owm_code >= 300 && owm_code < 400) {
        return 61;
    }
    if (owm_code >= 500 && owm_code < 600) {
        if (owm_code == 511) {
            return 71;
        }
        return 61;
    }
    if (owm_code >= 600 && owm_code < 700) {
        return 71;
    }
    if (owm_code >= 700 && owm_code < 800) {
        return 45;
    }
    if (owm_code == 800) {
        return 0;
    }
    if (owm_code >= 801 && owm_code < 810) {
        if (owm_code == 801) {
            return 1;
        }
        if (owm_code == 802) {
            return 2;
        }
        return 3;
    }
    return 0;
}

const char *weather_emoji(int weathercode) {
    if (weathercode == 0) {
        return "☀️";
    }
    if (weathercode == 1) {
        return "🌤️";
    }
    if (weathercode == 2) {
        return "⛅";
    }
    if (weathercode == 3) {
        return "☁️";
    }
    if (weathercode >= 45 && weathercode <= 48) {
        return "🌫️";
    }
    if (weathercode >= 51 && weathercode <= 55) {
        return "🌦️";
    }
    if (weathercode >= 56 && weathercode <= 57) {
        return "🌨️";
    }
    if (weathercode >= 61 && weathercode <= 65) {
        return "🌧️";
    }
    if (weathercode >= 66 && weathercode <= 67) {
        return "🌨️";
    }
    if (weathercode >= 71 && weathercode <= 77) {
        return "❄️";
    }
    if (weathercode >= 80 && weathercode <= 82) {
        return "🌦️";
    }
    if (weathercode >= 85 && weathercode <= 86) {
        return "🌨️";
    }
    if (weathercode >= 95 && weathercode <= 99) {
        return "⛈️";
    }
    return "❓";
}

const char *weather_description(int weathercode) {
    if (weathercode == 0) {
        return "Ясно";
    }
    if (weathercode >= 1 && weathercode <= 3) {
        return "Облачно";
    }
    if (weathercode >= 45 && weathercode <= 48) {
        return "Туман";
    }
    if (weathercode >= 51 && weathercode <= 67) {
        return "Дождь";
    }
    if (weathercode >= 71 && weathercode <= 77) {
        return "Снег";
    }
    if (weathercode >= 80 && weathercode <= 99) {
        if (weathercode >= 95) {
            return "Гроза";
        }
        if (weathercode >= 85) {
            return "Снегопад";
        }
        return "Ливень";
    }
    return "Неизвестно";
}

static void record_http(const char *api, const char *url, const HttpResponse *resp,
                        bool ok_body) {
    history_add_request(api, url, "GET", resp->status_code, resp->duration_ms);
    if (!ok_body) {
        if (resp->error[0]) {
            history_add_error(api, resp->error, url, resp->status_code);
        } else {
            char msg[128];
            snprintf(msg, sizeof(msg), "HTTP error %d", resp->status_code);
            history_add_error(api, msg, url, resp->status_code);
        }
    }
}

bool fetch_coordinates_by_city(const char *city, const char *country,
                               LocationData *out) {
    char enc_city[MAX_STR * 3];
    char enc_country[MAX_STR * 3];
    if (!url_encode(city, enc_city, sizeof(enc_city)) ||
        !url_encode(country, enc_country, sizeof(enc_country))) {
        return false;
    }

    /* Open-Meteo: narrow with "name=City,Country" (not undocumented country=). */
    char url[MAX_URL];
    snprintf(url, sizeof(url),
             "https://geocoding-api.open-meteo.com/v1/search?name=%s,%s"
             "&count=10&language=en",
             enc_city, enc_country);

    HttpResponse resp;
    if (!http_get(url, NULL, &resp)) {
        record_http("Geocoding API", url, &resp, false);
        http_response_free(&resp);
        return false;
    }
    if (resp.status_code != 200 || !resp.data) {
        record_http("Geocoding API", url, &resp, false);
        http_response_free(&resp);
        return false;
    }
    history_add_request("Geocoding API", url, "GET", resp.status_code,
                        resp.duration_ms);

    cJSON *root = cJSON_Parse(resp.data);
    http_response_free(&resp);
    if (!root) {
        history_add_error("Geocoding API", "JSON parse error", url, -1);
        return false;
    }

    cJSON *results = cJSON_GetObjectItemCaseSensitive(root, "results");
    if (!cJSON_IsArray(results) || cJSON_GetArraySize(results) == 0) {
        cJSON_Delete(root);
        history_add_error("Geocoding API", "город не найден", url, 200);
        return false;
    }

    cJSON *chosen = NULL;
    cJSON *item = NULL;
    cJSON_ArrayForEach(item, results) {
        cJSON *name = cJSON_GetObjectItemCaseSensitive(item, "name");
        cJSON *ctry = cJSON_GetObjectItemCaseSensitive(item, "country");
        if (cJSON_IsString(name) && cJSON_IsString(ctry) &&
            strcasecmp(name->valuestring, city) == 0 &&
            strcasecmp(ctry->valuestring, country) == 0) {
            chosen = item;
            break;
        }
    }
    if (!chosen) {
        cJSON_ArrayForEach(item, results) {
            cJSON *name = cJSON_GetObjectItemCaseSensitive(item, "name");
            if (cJSON_IsString(name) &&
                strcasecmp(name->valuestring, city) == 0) {
                chosen = item;
                break;
            }
        }
    }
    if (!chosen) {
        chosen = cJSON_GetArrayItem(results, 0);
    }

    const cJSON *name = cJSON_GetObjectItemCaseSensitive(chosen, "name");
    const cJSON *ctry = cJSON_GetObjectItemCaseSensitive(chosen, "country");
    const cJSON *lat = cJSON_GetObjectItemCaseSensitive(chosen, "latitude");
    const cJSON *lon = cJSON_GetObjectItemCaseSensitive(chosen, "longitude");
    if (!cJSON_IsString(name) || !cJSON_IsNumber(lat) || !cJSON_IsNumber(lon)) {
        cJSON_Delete(root);
        return false;
    }

    memset(out, 0, sizeof(*out));
    snprintf(out->city_name, sizeof(out->city_name), "%s", name->valuestring);
    if (cJSON_IsString(ctry)) {
        snprintf(out->country_name, sizeof(out->country_name), "%s",
                 ctry->valuestring);
    }
    out->latitude = lat->valuedouble;
    out->longitude = lon->valuedouble;
    cJSON_Delete(root);
    return true;
}

bool fetch_location_by_coordinates(double lat, double lon, LocationData *out) {
    char url[MAX_URL];
    snprintf(url, sizeof(url),
             "https://nominatim.openstreetmap.org/reverse?format=json&lat=%.6f"
             "&lon=%.6f&addressdetails=1&accept-language=ru",
             lat, lon);

    HttpResponse resp;
    if (!http_get(url, "WeatherApp/1.0", &resp)) {
        record_http("Nominatim API", url, &resp, false);
        http_response_free(&resp);
        return false;
    }
    if (resp.status_code != 200 || !resp.data) {
        record_http("Nominatim API", url, &resp, false);
        http_response_free(&resp);
        return false;
    }
    history_add_request("Nominatim API", url, "GET", resp.status_code,
                        resp.duration_ms);

    cJSON *root = cJSON_Parse(resp.data);
    http_response_free(&resp);
    if (!root) {
        history_add_error("Nominatim API", "JSON parse error", url, -1);
        return false;
    }

    const cJSON *err = cJSON_GetObjectItemCaseSensitive(root, "error");
    if (cJSON_IsString(err)) {
        history_add_error("Nominatim API", err->valuestring, url, 200);
        cJSON_Delete(root);
        return false;
    }

    const cJSON *address = cJSON_GetObjectItemCaseSensitive(root, "address");
    const char *city = NULL;
    const char *country_name = NULL;
    if (cJSON_IsObject(address)) {
        const char *keys[] = {"city", "town", "village", "municipality",
                              "county", NULL};
        for (int i = 0; keys[i]; i++) {
            const cJSON *v =
                cJSON_GetObjectItemCaseSensitive(address, keys[i]);
            if (cJSON_IsString(v) && v->valuestring[0]) {
                city = v->valuestring;
                break;
            }
        }
        const cJSON *c = cJSON_GetObjectItemCaseSensitive(address, "country");
        if (cJSON_IsString(c)) {
            country_name = c->valuestring;
        }
    }

    memset(out, 0, sizeof(*out));
    if (city) {
        snprintf(out->city_name, sizeof(out->city_name), "%s", city);
    } else {
        const cJSON *display =
            cJSON_GetObjectItemCaseSensitive(root, "display_name");
        if (cJSON_IsString(display)) {
            snprintf(out->city_name, sizeof(out->city_name), "%s",
                     display->valuestring);
            char *comma = strchr(out->city_name, ',');
            if (comma) {
                *comma = '\0';
            }
        }
    }
    if (country_name) {
        snprintf(out->country_name, sizeof(out->country_name), "%s",
                 country_name);
    }
    out->latitude = lat;
    out->longitude = lon;
    cJSON_Delete(root);
    return true;
}

bool fetch_weather(WeatherData *out) {
    memset(out, 0, sizeof(*out));
    if (!g_app.has_coords) {
        return false;
    }

    char url[MAX_URL];
    bool use_owm =
        strcmp(g_app.settings.api_provider, "openweathermap") == 0 &&
        g_app.settings.has_api_key;

    if (use_owm) {
        snprintf(url, sizeof(url),
                 "https://api.openweathermap.org/data/2.5/weather?lat=%.6f&lon="
                 "%.6f&appid=%s&units=metric",
                 g_app.latitude, g_app.longitude, g_app.settings.api_key);
    } else {
        snprintf(url, sizeof(url),
                 "https://api.open-meteo.com/v1/forecast?latitude=%.6f&"
                 "longitude=%.6f&current_weather=true&windspeed_unit=ms",
                 g_app.latitude, g_app.longitude);
    }

    HttpResponse resp;
    if (!http_get(url, NULL, &resp)) {
        record_http("Weather API", url, &resp, false);
        http_response_free(&resp);
        return false;
    }
    if (resp.status_code != 200 || !resp.data) {
        record_http("Weather API", url, &resp, false);
        http_response_free(&resp);
        return false;
    }
    history_add_request("Weather API", url, "GET", resp.status_code,
                        resp.duration_ms);

    cJSON *root = cJSON_Parse(resp.data);
    http_response_free(&resp);
    if (!root) {
        history_add_error("Weather API", "JSON parse error", url, -1);
        return false;
    }

    if (use_owm) {
        const cJSON *main = cJSON_GetObjectItemCaseSensitive(root, "main");
        const cJSON *temp =
            main ? cJSON_GetObjectItemCaseSensitive(main, "temp") : NULL;
        const cJSON *weather = cJSON_GetObjectItemCaseSensitive(root, "weather");
        if (!cJSON_IsNumber(temp)) {
            cJSON_Delete(root);
            return false;
        }
        out->temperature = temp->valuedouble;
        out->weathercode = 0;
        if (cJSON_IsArray(weather) && cJSON_GetArraySize(weather) > 0) {
            const cJSON *w0 = cJSON_GetArrayItem(weather, 0);
            const cJSON *id = cJSON_GetObjectItemCaseSensitive(w0, "id");
            if (cJSON_IsNumber(id)) {
                out->weathercode = convert_owm_to_wmo((int)id->valuedouble);
            }
        }
    } else {
        const cJSON *cw =
            cJSON_GetObjectItemCaseSensitive(root, "current_weather");
        const cJSON *temp =
            cw ? cJSON_GetObjectItemCaseSensitive(cw, "temperature") : NULL;
        const cJSON *code =
            cw ? cJSON_GetObjectItemCaseSensitive(cw, "weathercode") : NULL;
        if (!cJSON_IsNumber(temp)) {
            cJSON_Delete(root);
            return false;
        }
        out->temperature = temp->valuedouble;
        out->weathercode = cJSON_IsNumber(code) ? (int)code->valuedouble : 0;
    }

    out->valid = true;
    cJSON_Delete(root);
    return true;
}

bool initialize_location(void) {
    Settings *s = &g_app.settings;

    /* City+country mode: resolve coords at runtime only — do not persist them. */
    if (s->has_city && s->has_country) {
        log_errf("Определение координат для %s, %s...\n", s->city,
                 s->country);
        LocationData loc;
        if (!fetch_coordinates_by_city(s->city, s->country, &loc)) {
            log_errf("Не удалось определить координаты\n");
            return false;
        }
        g_app.latitude = loc.latitude;
        g_app.longitude = loc.longitude;
        g_app.has_coords = true;
        snprintf(g_app.city_name, sizeof(g_app.city_name), "%s", loc.city_name);
        snprintf(g_app.country_name, sizeof(g_app.country_name), "%s",
                 loc.country_name);

        /* Drop any leftover coords from older builds so settings stay exclusive. */
        if (s->has_latitude || s->has_longitude) {
            s->has_latitude = false;
            s->has_longitude = false;
            settings_save();
        }

        log_errf("Координаты определены: %f, %f\n", g_app.latitude,
                 g_app.longitude);
        log_errf("Местоположение: %s, %s\n", g_app.city_name,
                 g_app.country_name);
        return true;
    }

    if (s->has_latitude && s->has_longitude) {
        g_app.latitude = s->latitude;
        g_app.longitude = s->longitude;
        g_app.has_coords = true;
        log_errf("Используются координаты: %f, %f\n", g_app.latitude,
                 g_app.longitude);

        if (s->has_city && s->has_country) {
            snprintf(g_app.city_name, sizeof(g_app.city_name), "%s", s->city);
            snprintf(g_app.country_name, sizeof(g_app.country_name), "%s",
                     s->country);
        } else {
            LocationData loc;
            if (fetch_location_by_coordinates(g_app.latitude, g_app.longitude,
                                              &loc)) {
                snprintf(g_app.city_name, sizeof(g_app.city_name), "%s",
                         loc.city_name);
                snprintf(g_app.country_name, sizeof(g_app.country_name), "%s",
                         loc.country_name);
                log_errf("Местоположение обновлено по координатам: %s, %s\n",
                         g_app.city_name, g_app.country_name);
            }
        }
        return true;
    }

    log_errf("Не указаны ни координаты, ни город и страна\n");
    return false;
}
