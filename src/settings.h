#ifndef C_WEATHER_SETTINGS_H
#define C_WEATHER_SETTINGS_H

#include "app.h"

#include <stddef.h>

void settings_set_path(const char *path);
bool settings_load(void);
bool settings_save(void);
void settings_apply_defaults(Settings *s);

/* Validate XOR location modes and interval/provider rules.
 * On failure writes message into err (may be NULL). */
bool settings_validate(const Settings *s, char *err, size_t errlen);

#endif
