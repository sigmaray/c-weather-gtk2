#include "test.h"

#include "http.h"

void test_http(void) {
    TEST_SUITE("http / url_encode");

    char buf[64];
    EXPECT_TRUE(url_encode("Hello", buf, sizeof(buf)));
    EXPECT_STREQ(buf, "Hello");

    EXPECT_TRUE(url_encode("a b", buf, sizeof(buf)));
    EXPECT_STREQ(buf, "a+b");

    EXPECT_TRUE(url_encode("a/b", buf, sizeof(buf)));
    EXPECT_STREQ(buf, "a%2Fb");

    EXPECT_TRUE(url_encode("New York City", buf, sizeof(buf)));
    EXPECT_STREQ(buf, "New+York+City");

    EXPECT_TRUE(url_encode("cafe", buf, sizeof(buf)));
    EXPECT_STREQ(buf, "cafe");

    EXPECT_TRUE(url_encode("-_.~", buf, sizeof(buf)));
    EXPECT_STREQ(buf, "-_.~");

    /* Buffer too small for encoded form. */
    EXPECT_FALSE(url_encode(" ", buf, 1));
    EXPECT_FALSE(url_encode("%", buf, 3)); /* needs 3 chars + NUL */
    EXPECT_TRUE(url_encode("%", buf, 4));
    EXPECT_STREQ(buf, "%25");

    EXPECT_TRUE(url_encode("", buf, sizeof(buf)));
    EXPECT_STREQ(buf, "");
}
