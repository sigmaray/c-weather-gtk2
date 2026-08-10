#ifndef C_WEATHER_ICON_H
#define C_WEATHER_ICON_H

#include <stdbool.h>

/* Write a 32x32 PNG with temperature text (e.g. "21°" or "--"). */
bool icon_write_temp_png(const char *path, const char *label);

/* Write a 32x32 PNG with a simple weather glyph for WMO code. */
bool icon_write_weather_png(const char *path, int weathercode);

/* Write a 32x32 PNG spinner used while weather is loading. */
bool icon_write_loading_png(const char *path);

#endif
