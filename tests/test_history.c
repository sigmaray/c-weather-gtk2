#include "test.h"

#include "app.h"
#include "history.h"

#include <string.h>

void test_history(void) {
    TEST_SUITE("history");

    memset(&g_app, 0, sizeof(g_app));
    EXPECT_EQ_INT(history_error_count(), 0);
    EXPECT_EQ_INT(history_request_count(), 0);

    ApiError err;
    ApiRequest req;
    EXPECT_FALSE(history_get_error(0, &err));
    EXPECT_FALSE(history_get_request(0, &req));

    history_add_error("api", "boom", "http://x", 500);
    EXPECT_EQ_INT(history_error_count(), 1);
    EXPECT_TRUE(history_get_error(0, &err));
    EXPECT_STREQ(err.api, "api");
    EXPECT_STREQ(err.error, "boom");
    EXPECT_STREQ(err.url, "http://x");
    EXPECT_EQ_INT(err.status_code, 500);

    history_add_request("api", "http://y", "GET", 200, 42);
    EXPECT_EQ_INT(history_request_count(), 1);
    EXPECT_TRUE(history_get_request(0, &req));
    EXPECT_STREQ(req.api, "api");
    EXPECT_STREQ(req.url, "http://y");
    EXPECT_STREQ(req.method, "GET");
    EXPECT_EQ_INT(req.response_status, 200);
    EXPECT_EQ_INT((int)req.duration_ms, 42);

    /* NULL method defaults to GET. */
    history_add_request("api2", "http://z", NULL, 201, 1);
    EXPECT_TRUE(history_get_request(1, &req));
    EXPECT_STREQ(req.method, "GET");

    /* Ring buffer drops oldest after MAX_API_ERRORS. */
    memset(&g_app, 0, sizeof(g_app));
    for (int i = 0; i < MAX_API_ERRORS + 5; i++) {
        char msg[32];
        snprintf(msg, sizeof(msg), "e%d", i);
        history_add_error("x", msg, "", i);
    }
    EXPECT_EQ_INT(history_error_count(), MAX_API_ERRORS);
    EXPECT_TRUE(history_get_error(0, &err));
    EXPECT_STREQ(err.error, "e5"); /* 0..4 dropped */
    EXPECT_EQ_INT(err.status_code, 5);
    EXPECT_TRUE(history_get_error(MAX_API_ERRORS - 1, &err));
    EXPECT_STREQ(err.error, "e24");
    EXPECT_FALSE(history_get_error(MAX_API_ERRORS, &err));
    EXPECT_FALSE(history_get_error(-1, &err));

    memset(&g_app, 0, sizeof(g_app));
    for (int i = 0; i < MAX_API_REQUESTS + 3; i++) {
        char url[32];
        snprintf(url, sizeof(url), "u%d", i);
        history_add_request("r", url, "GET", i, i);
    }
    EXPECT_EQ_INT(history_request_count(), MAX_API_REQUESTS);
    EXPECT_TRUE(history_get_request(0, &req));
    EXPECT_STREQ(req.url, "u3");
    EXPECT_TRUE(history_get_request(MAX_API_REQUESTS - 1, &req));
    EXPECT_STREQ(req.url, "u22");
}
