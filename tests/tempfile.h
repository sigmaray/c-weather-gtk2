#ifndef C_WEATHER_TEMPFILE_H
#define C_WEATHER_TEMPFILE_H

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Fill path with a unique template and create it via mkstemp.
 * On success returns fd (>=0); caller should close(). path must be writable. */
static inline int test_mkstemp(char *path, size_t pathsize, const char *prefix) {
    const char *dir = getenv("TMPDIR");
    if (!dir || !dir[0]) {
        dir = getenv("TEMP");
    }
    if (!dir || !dir[0]) {
        dir = getenv("TMP");
    }
    if (!dir || !dir[0]) {
        dir = "/tmp";
    }
    int n = snprintf(path, pathsize, "%s/%s-XXXXXX", dir, prefix);
    if (n < 0 || (size_t)n >= pathsize) {
        return -1;
    }
    return mkstemp(path);
}

#endif
