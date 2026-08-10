#ifndef C_WEATHER_TEST_H
#define C_WEATHER_TEST_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int g_tests_run;
extern int g_tests_failed;

#define TEST_SUITE(name) printf("\n== %s ==\n", (name))

#define EXPECT(cond)                                                           \
    do {                                                                       \
        g_tests_run++;                                                         \
        if (!(cond)) {                                                         \
            g_tests_failed++;                                                  \
            fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);  \
        }                                                                      \
    } while (0)

#define EXPECT_EQ_INT(a, b)                                                    \
    do {                                                                       \
        int _a = (int)(a);                                                     \
        int _b = (int)(b);                                                     \
        g_tests_run++;                                                         \
        if (_a != _b) {                                                        \
            g_tests_failed++;                                                  \
            fprintf(stderr, "  FAIL %s:%d: %s (%d) != %s (%d)\n", __FILE__,    \
                    __LINE__, #a, _a, #b, _b);                                 \
        }                                                                      \
    } while (0)

#define EXPECT_STREQ(a, b)                                                     \
    do {                                                                       \
        const char *_a = (a);                                                  \
        const char *_b = (b);                                                  \
        g_tests_run++;                                                         \
        if (!_a || !_b || strcmp(_a, _b) != 0) {                               \
            g_tests_failed++;                                                  \
            fprintf(stderr, "  FAIL %s:%d: %s (\"%s\") != %s (\"%s\")\n",      \
                    __FILE__, __LINE__, #a, _a ? _a : "(null)", #b,            \
                    _b ? _b : "(null)");                                       \
        }                                                                      \
    } while (0)

#define EXPECT_TRUE(cond) EXPECT(cond)
#define EXPECT_FALSE(cond) EXPECT(!(cond))

void test_weather(void);
void test_settings(void);
void test_history(void);
void test_http(void);
void test_icon(void);

#endif
