#include "test.h"

#include "app.h"

AppState g_app;
int g_tests_run;
int g_tests_failed;

int main(void) {
    printf("c-weather-gtk2 unit tests\n");

    test_weather();
    test_settings();
    test_history();
    test_http();
    test_icon();

    printf("\n%d checks, %d failed\n", g_tests_run, g_tests_failed);
    return g_tests_failed == 0 ? 0 : 1;
}
