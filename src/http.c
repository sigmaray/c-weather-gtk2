#if !defined(_WIN32) || defined(__CYGWIN__)
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#endif

#include "http.h"
#include "compat.h"

#include <curl/curl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32) && !defined(__CYGWIN__)
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <unistd.h>
#else
#include <unistd.h>
#endif

typedef struct {
    char *data;
    size_t size;
} WriteBuffer;

/* curl CURLOPT_WRITEFUNCTION requires non-const char* */
static size_t write_cb(char *ptr, size_t size, size_t nmemb, void *userdata) {
    WriteBuffer *buf = userdata;
    size_t total = size * nmemb;
    char *p = realloc(buf->data, buf->size + total + 1);
    if (!p) {
        return 0;
    }
    buf->data = p;
    memcpy(buf->data + buf->size, ptr, total);
    buf->size += total;
    buf->data[buf->size] = '\0';
    return total;
}

static bool path_is_readable(const char *path) {
#if defined(_WIN32) && !defined(__CYGWIN__)
    DWORD attrs = GetFileAttributesA(path);
    return attrs != INVALID_FILE_ATTRIBUTES &&
           !(attrs & FILE_ATTRIBUTE_DIRECTORY);
#else
    return access(path, R_OK) == 0;
#endif
}

/* Fill dest with directory of the running executable (no trailing slash). */
static bool exe_dir(char *dest, size_t dest_size) {
#if defined(_WIN32) && !defined(__CYGWIN__)
    char path[MAX_PATH];
    DWORD n = GetModuleFileNameA(NULL, path, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) {
        return false;
    }
    char *slash = strrchr(path, '\\');
    char *slash2 = strrchr(path, '/');
    if (slash2 && (!slash || slash2 > slash)) {
        slash = slash2;
    }
    if (!slash) {
        return false;
    }
    *slash = '\0';
    if (strlen(path) + 1 > dest_size) {
        return false;
    }
    memcpy(dest, path, strlen(path) + 1);
    return true;
#elif defined(__APPLE__)
    char path[4096];
    uint32_t size = (uint32_t)sizeof(path);
    if (_NSGetExecutablePath(path, &size) != 0) {
        return false;
    }
    char resolved[4096];
    if (!realpath(path, resolved)) {
        snprintf(resolved, sizeof(resolved), "%s", path);
    }
    char *slash = strrchr(resolved, '/');
    if (!slash) {
        return false;
    }
    *slash = '\0';
    if (strlen(resolved) + 1 > dest_size) {
        return false;
    }
    memcpy(dest, resolved, strlen(resolved) + 1);
    return true;
#else
    char path[4096];
    ssize_t n = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (n < 0) {
        return false;
    }
    path[n] = '\0';
    char *slash = strrchr(path, '/');
    if (!slash) {
        return false;
    }
    *slash = '\0';
    if ((size_t)(slash - path) + 1 > dest_size) {
        return false;
    }
    memcpy(dest, path, (size_t)(slash - path) + 1);
    return true;
#endif
}

/* Prefer env / bundled CA; on Windows also enable the OS certificate store. */
static void http_configure_tls(CURL *curl) {
#ifdef CURLSSLOPT_NATIVE_CA
    curl_easy_setopt(curl, CURLOPT_SSL_OPTIONS, (long)CURLSSLOPT_NATIVE_CA);
#endif

    const char *env = getenv("CURL_CA_BUNDLE");
    if (env && env[0] && path_is_readable(env)) {
        curl_easy_setopt(curl, CURLOPT_CAINFO, env);
        return;
    }

    char dir[4096];
    if (!exe_dir(dir, sizeof(dir))) {
        return;
    }

    static const char *names[] = {"curl-ca-bundle.crt", "cacert.pem",
                                  "ca-bundle.crt"};
    char candidate[4096];
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
#if defined(_WIN32) && !defined(__CYGWIN__)
        int n = snprintf(candidate, sizeof(candidate), "%s\\%s", dir, names[i]);
#else
        int n = snprintf(candidate, sizeof(candidate), "%s/%s", dir, names[i]);
#endif
        if (n > 0 && (size_t)n < sizeof(candidate) &&
            path_is_readable(candidate)) {
            curl_easy_setopt(curl, CURLOPT_CAINFO, candidate);
            return;
        }
    }
}

void http_global_init(void) {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

void http_global_cleanup(void) {
    curl_global_cleanup();
}

void http_response_free(HttpResponse *resp) {
    if (!resp) {
        return;
    }
    free(resp->data);
    resp->data = NULL;
    resp->size = 0;
}

bool http_get(const char *url, const char *user_agent, HttpResponse *out) {
    memset(out, 0, sizeof(*out));
    out->status_code = -1;

    CURL *curl = curl_easy_init();
    if (!curl) {
        snprintf(out->error, sizeof(out->error), "curl_easy_init failed");
        return false;
    }

    WriteBuffer buf = {0};
    struct curl_slist *headers = NULL;

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    http_configure_tls(curl);
    if (user_agent && user_agent[0]) {
        curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agent);
    } else {
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "WeatherApp/1.0");
    }

    struct timespec start, end;
    int have_start = (clock_gettime(CLOCK_MONOTONIC, &start) == 0);
    CURLcode rc = curl_easy_perform(curl);
    int have_end = (clock_gettime(CLOCK_MONOTONIC, &end) == 0);

    if (have_start && have_end) {
        out->duration_ms = (end.tv_sec - start.tv_sec) * 1000L +
                           (end.tv_nsec - start.tv_nsec) / 1000000L;
    }

    if (rc != CURLE_OK) {
        snprintf(out->error, sizeof(out->error), "%s", curl_easy_strerror(rc));
        free(buf.data);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return false;
    }

    long code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
    out->status_code = (int)code;
    out->data = buf.data;
    out->size = buf.size;

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return true;
}

bool url_encode(const char *src, char *buf, size_t bufsize) {
    static const char *hex = "0123456789ABCDEF";
    size_t j = 0;
    for (size_t i = 0; src[i]; i++) {
        unsigned char c = (unsigned char)src[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' ||
            c == '~') {
            if (j + 1 >= bufsize) {
                return false;
            }
            buf[j++] = (char)c;
        } else if (c == ' ') {
            if (j + 1 >= bufsize) {
                return false;
            }
            buf[j++] = '+';
        } else {
            if (j + 3 >= bufsize) {
                return false;
            }
            buf[j++] = '%';
            buf[j++] = hex[c >> 4];
            buf[j++] = hex[c & 0xF];
        }
    }
    if (j >= bufsize) {
        return false;
    }
    buf[j] = '\0';
    return true;
}
