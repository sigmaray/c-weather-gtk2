#ifndef C_WEATHER_HISTORY_H
#define C_WEATHER_HISTORY_H

#include "app.h"

#include <stdbool.h>

void history_add_error(const char *api, const char *error, const char *url,
                       int status_code);
void history_add_request(const char *api, const char *url, const char *method,
                         int response_status, long duration_ms);

int history_error_count(void);
int history_request_count(void);

/* index 0 = oldest among stored. Returns false if out of range. */
bool history_get_error(int index, ApiError *out);
bool history_get_request(int index, ApiRequest *out);

#endif
