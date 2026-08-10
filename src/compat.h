#ifndef C_WEATHER_COMPAT_H
#define C_WEATHER_COMPAT_H

#include <string.h>
#include <time.h>

#if defined(_WIN32) && !defined(__CYGWIN__)
#ifndef strcasecmp
#define strcasecmp _stricmp
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

static inline struct tm *localtime_r(const time_t *timep, struct tm *result) {
    if (localtime_s(result, timep) != 0) {
        return NULL;
    }
    return result;
}

#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 1
#endif

/*
 * XP-safe clock_gettime via QueryPerformanceCounter.
 * Do not use MinGW winpthread's clock_gettime: it imports GetTickCount64
 * (Vista+), which fails at load time on Windows XP. Newer MinGW declares
 * clock_gettime in pthread_time.h, so provide a distinct symbol and redirect.
 */
static inline int cw_clock_gettime(int clk_id, struct timespec *tp) {
    (void)clk_id;
    static LARGE_INTEGER freq;
    static BOOL have_freq = FALSE;
    LARGE_INTEGER count;

    if (!have_freq) {
        if (!QueryPerformanceFrequency(&freq) || freq.QuadPart == 0) {
            return -1;
        }
        have_freq = TRUE;
    }
    if (!QueryPerformanceCounter(&count)) {
        return -1;
    }
    tp->tv_sec = (time_t)(count.QuadPart / freq.QuadPart);
    tp->tv_nsec = (long)((count.QuadPart % freq.QuadPart) * 1000000000LL /
                         freq.QuadPart);
    return 0;
}

#undef clock_gettime
#define clock_gettime(clk, tp) cw_clock_gettime((clk), (tp))
#else
#include <strings.h>
#endif

#include <glib.h>

/* Thread/mutex helpers: GLib 2.32+ vs legacy (Windows XP / old MinGW GTK2). */
#if GLIB_CHECK_VERSION(2, 32, 0)
typedef GMutex CWMutex;

static inline void cw_mutex_init(CWMutex *m) {
    g_mutex_init(m);
}

static inline void cw_mutex_clear(CWMutex *m) {
    g_mutex_clear(m);
}

static inline void cw_mutex_lock(CWMutex *m) {
    g_mutex_lock(m);
}

static inline void cw_mutex_unlock(CWMutex *m) {
    g_mutex_unlock(m);
}

static inline void cw_thread_spawn(const char *name, GThreadFunc func,
                                   gpointer data) {
    GThread *t = g_thread_new(name, func, data);
    if (t) {
        g_thread_unref(t);
    }
}
#else
typedef struct {
    GMutex *handle;
} CWMutex;

static inline void cw_mutex_init(CWMutex *m) {
    m->handle = g_mutex_new();
}

static inline void cw_mutex_clear(CWMutex *m) {
    g_mutex_free(m->handle);
    m->handle = NULL;
}

static inline void cw_mutex_lock(CWMutex *m) {
    g_mutex_lock(m->handle);
}

static inline void cw_mutex_unlock(CWMutex *m) {
    g_mutex_unlock(m->handle);
}

static inline void cw_thread_spawn(const char *name, GThreadFunc func,
                                   gpointer data) {
    (void)name;
    if (!g_thread_supported()) {
        g_thread_init(NULL);
    }
    g_thread_create(func, data, FALSE, NULL);
}
#endif

#endif /* C_WEATHER_COMPAT_H */
