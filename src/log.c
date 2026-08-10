#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#if defined(_WIN32) && !defined(__CYGWIN__)
#include <windows.h>

/* Write UTF-8 to a real console via Unicode; fall back to OEM/ACP bytes. */
static bool log_write_console_utf8(HANDLE h, const char *utf8) {
    DWORD mode = 0;
    if (h == INVALID_HANDLE_VALUE || h == NULL || !GetConsoleMode(h, &mode)) {
        return false;
    }

    int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
    if (wlen <= 0) {
        return false;
    }

    wchar_t *wbuf = (wchar_t *)malloc((size_t)wlen * sizeof(wchar_t));
    if (!wbuf) {
        return false;
    }
    if (MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wbuf, wlen) <= 0) {
        free(wbuf);
        return false;
    }

    DWORD written = 0;
    BOOL ok = WriteConsoleW(h, wbuf, (DWORD)(wlen - 1), &written, NULL);
    free(wbuf);
    return ok != 0;
}

static bool log_write_cp_utf8(FILE *stream, const char *utf8) {
    UINT cp = GetConsoleOutputCP();
    if (cp == 0 || cp == CP_UTF8) {
        cp = GetACP();
    }

    int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
    if (wlen <= 0) {
        return false;
    }
    wchar_t *wbuf = (wchar_t *)malloc((size_t)wlen * sizeof(wchar_t));
    if (!wbuf) {
        return false;
    }
    if (MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wbuf, wlen) <= 0) {
        free(wbuf);
        return false;
    }

    int alen =
        WideCharToMultiByte(cp, 0, wbuf, -1, NULL, 0, NULL, NULL);
    if (alen <= 0) {
        free(wbuf);
        return false;
    }
    char *abytes = (char *)malloc((size_t)alen);
    if (!abytes) {
        free(wbuf);
        return false;
    }
    if (WideCharToMultiByte(cp, 0, wbuf, -1, abytes, alen, NULL, NULL) <= 0) {
        free(wbuf);
        free(abytes);
        return false;
    }
    free(wbuf);
    fputs(abytes, stream);
    fflush(stream);
    free(abytes);
    return true;
}
#endif

void log_errf(const char *fmt, ...) {
    char buf[2048];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

#if defined(_WIN32) && !defined(__CYGWIN__)
    if (log_write_console_utf8(GetStdHandle(STD_ERROR_HANDLE), buf) ||
        log_write_console_utf8(GetStdHandle(STD_OUTPUT_HANDLE), buf) ||
        log_write_cp_utf8(stderr, buf)) {
        return;
    }
#endif
    fputs(buf, stderr);
    fflush(stderr);
}
