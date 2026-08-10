#ifndef C_WEATHER_APP_H
#define C_WEATHER_APP_H

#include <stdbool.h>
#include <time.h>

#define MAX_STR 512
#define MAX_URL 4096
#define MAX_API_ERRORS 20
#define MAX_API_REQUESTS 20

typedef struct {
    char city[MAX_STR];
    char country[MAX_STR];
    double latitude;
    double longitude;
    bool has_latitude;
    bool has_longitude;
    int update_interval_seconds;
    char api_provider[64];
    char api_key[MAX_STR];
    bool has_city;
    bool has_country;
    bool has_api_key;
} Settings;

typedef struct {
    double temperature;
    int weathercode;
    bool valid;
} WeatherData;

typedef struct {
    char city_name[MAX_STR];
    char country_name[MAX_STR];
    double latitude;
    double longitude;
} LocationData;

typedef struct {
    time_t timestamp;
    char api[64];
    char error[512];
    char url[MAX_URL];
    int status_code; /* -1 if none */
} ApiError;

typedef struct {
    time_t timestamp;
    char api[64];
    char url[MAX_URL];
    char method[16];
    int response_status; /* -1 if none */
    long duration_ms;
} ApiRequest;

typedef struct {
    Settings settings;
    char settings_path[MAX_STR];

    char city_name[MAX_STR];
    char country_name[MAX_STR];
    double latitude;
    double longitude;
    bool has_coords;

    WeatherData weather;
    time_t last_update_time;
    bool has_last_update;

    ApiError api_errors[MAX_API_ERRORS];
    int api_error_count;
    int api_error_start;

    ApiRequest api_requests[MAX_API_REQUESTS];
    int api_request_count;
    int api_request_start;

    unsigned int update_timer_id;
} AppState;

extern AppState g_app;

#endif
