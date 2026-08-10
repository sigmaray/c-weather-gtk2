#ifndef C_WEATHER_WEATHER_H
#define C_WEATHER_WEATHER_H

#include "app.h"

int convert_owm_to_wmo(int owm_code);
const char *weather_emoji(int weathercode);
const char *weather_description(int weathercode);

bool fetch_coordinates_by_city(const char *city, const char *country,
                               LocationData *out);
bool fetch_location_by_coordinates(double lat, double lon, LocationData *out);
bool fetch_weather(WeatherData *out);

/* Resolve g_app location from settings. Returns false on failure. */
bool initialize_location(void);

#endif
