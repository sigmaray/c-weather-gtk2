#include "test.h"

#include "weather.h"

void test_weather(void) {
    TEST_SUITE("weather");

    EXPECT_EQ_INT(convert_owm_to_wmo(200), 95);
    EXPECT_EQ_INT(convert_owm_to_wmo(299), 95);
    EXPECT_EQ_INT(convert_owm_to_wmo(300), 61);
    EXPECT_EQ_INT(convert_owm_to_wmo(500), 61);
    EXPECT_EQ_INT(convert_owm_to_wmo(511), 71);
    EXPECT_EQ_INT(convert_owm_to_wmo(600), 71);
    EXPECT_EQ_INT(convert_owm_to_wmo(701), 45);
    EXPECT_EQ_INT(convert_owm_to_wmo(800), 0);
    EXPECT_EQ_INT(convert_owm_to_wmo(801), 1);
    EXPECT_EQ_INT(convert_owm_to_wmo(802), 2);
    EXPECT_EQ_INT(convert_owm_to_wmo(803), 3);
    EXPECT_EQ_INT(convert_owm_to_wmo(804), 3);
    EXPECT_EQ_INT(convert_owm_to_wmo(999), 0);

    EXPECT_STREQ(weather_emoji(0), "☀️");
    EXPECT_STREQ(weather_emoji(1), "🌤️");
    EXPECT_STREQ(weather_emoji(2), "⛅");
    EXPECT_STREQ(weather_emoji(3), "☁️");
    EXPECT_STREQ(weather_emoji(45), "🌫️");
    EXPECT_STREQ(weather_emoji(61), "🌧️");
    EXPECT_STREQ(weather_emoji(71), "❄️");
    EXPECT_STREQ(weather_emoji(95), "⛈️");
    EXPECT_STREQ(weather_emoji(1234), "❓");

    EXPECT_STREQ(weather_description(0), "Ясно");
    EXPECT_STREQ(weather_description(2), "Облачно");
    EXPECT_STREQ(weather_description(45), "Туман");
    EXPECT_STREQ(weather_description(61), "Дождь");
    EXPECT_STREQ(weather_description(71), "Снег");
    EXPECT_STREQ(weather_description(80), "Ливень");
    EXPECT_STREQ(weather_description(85), "Снегопад");
    EXPECT_STREQ(weather_description(95), "Гроза");
    EXPECT_STREQ(weather_description(-1), "Неизвестно");
}
