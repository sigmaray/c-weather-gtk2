#ifndef C_WEATHER_HTTP_H
#define C_WEATHER_HTTP_H

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    char *data;
    size_t size;
    int status_code;
    long duration_ms;
    char error[256];
} HttpResponse;

void http_global_init(void);
void http_global_cleanup(void);

/* GET request. user_agent may be NULL. Caller must http_response_free(). */
bool http_get(const char *url, const char *user_agent, HttpResponse *out);
void http_response_free(HttpResponse *resp);

/* Percent-encode into buf (null-terminated). Returns false if too small. */
bool url_encode(const char *src, char *buf, size_t bufsize);

#endif
