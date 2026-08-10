#include "history.h"

#include <stdio.h>
#include <string.h>

static void copy_str(char *dst, size_t n, const char *src) {
    if (!src) {
        src = "";
    }
    snprintf(dst, n, "%s", src);
}

void history_add_error(const char *api, const char *error, const char *url,
                       int status_code) {
    int idx;
    if (g_app.api_error_count < MAX_API_ERRORS) {
        idx = (g_app.api_error_start + g_app.api_error_count) % MAX_API_ERRORS;
        g_app.api_error_count++;
    } else {
        idx = g_app.api_error_start;
        g_app.api_error_start = (g_app.api_error_start + 1) % MAX_API_ERRORS;
    }

    ApiError *e = &g_app.api_errors[idx];
    memset(e, 0, sizeof(*e));
    e->timestamp = time(NULL);
    copy_str(e->api, sizeof(e->api), api);
    copy_str(e->error, sizeof(e->error), error);
    copy_str(e->url, sizeof(e->url), url);
    e->status_code = status_code;
}

void history_add_request(const char *api, const char *url, const char *method,
                         int response_status, long duration_ms) {
    int idx;
    if (g_app.api_request_count < MAX_API_REQUESTS) {
        idx = (g_app.api_request_start + g_app.api_request_count) %
              MAX_API_REQUESTS;
        g_app.api_request_count++;
    } else {
        idx = g_app.api_request_start;
        g_app.api_request_start =
            (g_app.api_request_start + 1) % MAX_API_REQUESTS;
    }

    ApiRequest *r = &g_app.api_requests[idx];
    memset(r, 0, sizeof(*r));
    r->timestamp = time(NULL);
    copy_str(r->api, sizeof(r->api), api);
    copy_str(r->url, sizeof(r->url), url);
    copy_str(r->method, sizeof(r->method), method ? method : "GET");
    r->response_status = response_status;
    r->duration_ms = duration_ms;
}

int history_error_count(void) {
    return g_app.api_error_count;
}

int history_request_count(void) {
    return g_app.api_request_count;
}

bool history_get_error(int index, ApiError *out) {
    if (index < 0 || index >= g_app.api_error_count) {
        return false;
    }
    int idx = (g_app.api_error_start + index) % MAX_API_ERRORS;
    *out = g_app.api_errors[idx];
    return true;
}

bool history_get_request(int index, ApiRequest *out) {
    if (index < 0 || index >= g_app.api_request_count) {
        return false;
    }
    int idx = (g_app.api_request_start + index) % MAX_API_REQUESTS;
    *out = g_app.api_requests[idx];
    return true;
}
