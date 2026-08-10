#include "ui.h"

#include "app.h"
#include "compat.h"
#include "history.h"
#include "settings.h"
#include "weather.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Declared in main.c */
bool app_apply_settings_and_refresh(const Settings *new_settings,
                                    GtkWindow *parent);
void app_request_refresh(void);

void ui_show_message(GtkWindow *parent, const char *title, const char *message,
                     bool is_error) {
    GtkWidget *dialog = gtk_message_dialog_new(
        parent, GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        is_error ? GTK_MESSAGE_ERROR : GTK_MESSAGE_INFO, GTK_BUTTONS_OK, "%s",
        message);
    gtk_window_set_title(GTK_WINDOW(dialog), title);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

static GtkWidget *make_scrolled_label(const char *text, int w, int h,
                                      bool selectable) {
    GtkWidget *label = gtk_label_new(text);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.0);
    gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
    gtk_label_set_selectable(GTK_LABEL(label), selectable);
#if GTK_CHECK_VERSION(2, 18, 0)
    if (selectable) {
        gtk_widget_set_can_focus(label, FALSE);
    }
#endif

    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    /* GtkLabel is not scrollable; viewport is required on GTK2. */
    gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scroll), label);
    gtk_widget_set_size_request(scroll, w, h);
    return scroll;
}

typedef struct {
    GtkWidget *city;
    GtkWidget *country;
    GtkWidget *lat;
    GtkWidget *lon;
    GtkWidget *interval;
    GtkWidget *provider;
    GtkWidget *api_key;
    GtkWidget *window;
} SettingsWidgets;

static void on_settings_save(GtkButton *button, gpointer user_data) {
    (void)button;
    SettingsWidgets *w = user_data;
    Settings s;
    memset(&s, 0, sizeof(s));

    const char *city = gtk_entry_get_text(GTK_ENTRY(w->city));
    const char *country = gtk_entry_get_text(GTK_ENTRY(w->country));
    const char *lat_s = gtk_entry_get_text(GTK_ENTRY(w->lat));
    const char *lon_s = gtk_entry_get_text(GTK_ENTRY(w->lon));
    const char *interval_s = gtk_entry_get_text(GTK_ENTRY(w->interval));
    const char *api_key = gtk_entry_get_text(GTK_ENTRY(w->api_key));
    gchar *provider = gtk_combo_box_get_active_text(GTK_COMBO_BOX(w->provider));

    while (*city == ' ') {
        city++;
    }
    while (*country == ' ') {
        country++;
    }

    if (city[0]) {
        snprintf(s.city, sizeof(s.city), "%s", city);
        s.has_city = true;
    }
    if (country[0]) {
        snprintf(s.country, sizeof(s.country), "%s", country);
        s.has_country = true;
    }

    if (lat_s[0]) {
        char *end = NULL;
        double v = strtod(lat_s, &end);
        if (end != lat_s) {
            s.latitude = v;
            s.has_latitude = true;
        }
    }
    if (lon_s[0]) {
        char *end = NULL;
        double v = strtod(lon_s, &end);
        if (end != lon_s) {
            s.longitude = v;
            s.has_longitude = true;
        }
    }

    s.update_interval_seconds = 60;
    if (interval_s[0]) {
        int v = atoi(interval_s);
        if (v >= 1) {
            s.update_interval_seconds = v;
        }
    }

    if (provider && provider[0]) {
        snprintf(s.api_provider, sizeof(s.api_provider), "%s", provider);
    } else {
        snprintf(s.api_provider, sizeof(s.api_provider), "open-meteo");
    }
    g_free(provider);

    if (api_key[0]) {
        snprintf(s.api_key, sizeof(s.api_key), "%s", api_key);
        s.has_api_key = true;
    }

    if (s.has_city && s.has_country) {
        s.has_latitude = false;
        s.has_longitude = false;
    }

    char err[256];
    if (!settings_validate(&s, err, sizeof(err))) {
        ui_show_message(GTK_WINDOW(w->window), "Ошибка", err, true);
        return;
    }

    if (app_apply_settings_and_refresh(&s, GTK_WINDOW(w->window))) {
        gtk_widget_destroy(w->window);
    }
}

static void on_quick_city(GtkButton *button, gpointer user_data) {
    SettingsWidgets *w = user_data;
    const char *data = g_object_get_data(G_OBJECT(button), "city-data");
    if (!data) {
        return;
    }
    char buf[MAX_STR * 2];
    snprintf(buf, sizeof(buf), "%s", data);
    char *sep = strchr(buf, '|');
    if (!sep) {
        return;
    }
    *sep = '\0';
    gtk_entry_set_text(GTK_ENTRY(w->city), buf);
    gtk_entry_set_text(GTK_ENTRY(w->country), sep + 1);
    gtk_entry_set_text(GTK_ENTRY(w->lat), "");
    gtk_entry_set_text(GTK_ENTRY(w->lon), "");
}

void ui_show_settings(GtkWindow *parent) {
    SettingsWidgets *w = g_malloc0(sizeof(*w));
    w->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(w->window), "Настройки");
    gtk_window_set_default_size(GTK_WINDOW(w->window), 560, 520);
    gtk_window_set_transient_for(GTK_WINDOW(w->window), parent);
    g_signal_connect_swapped(w->window, "destroy", G_CALLBACK(g_free), w);

    GtkWidget *box = gtk_vbox_new(FALSE, 8);
    gtk_container_set_border_width(GTK_CONTAINER(box), 12);
    gtk_container_add(GTK_CONTAINER(w->window), box);

    GtkWidget *info = gtk_label_new(
        "Укажите один из вариантов:\n"
        "• Город и Страна — координаты определятся автоматически\n"
        "• Широта и Долгота — координаты вручную");
    gtk_misc_set_alignment(GTK_MISC(info), 0.0, 0.0);
    gtk_box_pack_start(GTK_BOX(box), info, FALSE, FALSE, 0);

    GtkWidget *table = gtk_table_new(7, 2, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 6);
    gtk_table_set_col_spacings(GTK_TABLE(table), 8);
    gtk_box_pack_start(GTK_BOX(box), table, FALSE, FALSE, 0);

    w->city = gtk_entry_new();
    w->country = gtk_entry_new();
    w->lat = gtk_entry_new();
    w->lon = gtk_entry_new();
    w->interval = gtk_entry_new();
    w->provider = gtk_combo_box_new_text();
    w->api_key = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(w->api_key), FALSE);

    gtk_combo_box_append_text(GTK_COMBO_BOX(w->provider), "open-meteo");
    gtk_combo_box_append_text(GTK_COMBO_BOX(w->provider), "openweathermap");

    const Settings *s = &g_app.settings;
    bool city_mode = s->has_city && s->has_country;
    if (s->has_city) {
        gtk_entry_set_text(GTK_ENTRY(w->city), s->city);
    }
    if (s->has_country) {
        gtk_entry_set_text(GTK_ENTRY(w->country), s->country);
    }
    if (!city_mode && s->has_latitude) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%.6f", s->latitude);
        gtk_entry_set_text(GTK_ENTRY(w->lat), buf);
    }
    if (!city_mode && s->has_longitude) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%.6f", s->longitude);
        gtk_entry_set_text(GTK_ENTRY(w->lon), buf);
    }
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "%d", s->update_interval_seconds);
        gtk_entry_set_text(GTK_ENTRY(w->interval), buf);
    }
    if (strcmp(s->api_provider, "openweathermap") == 0) {
        gtk_combo_box_set_active(GTK_COMBO_BOX(w->provider), 1);
    } else {
        gtk_combo_box_set_active(GTK_COMBO_BOX(w->provider), 0);
    }
    if (s->has_api_key) {
        gtk_entry_set_text(GTK_ENTRY(w->api_key), s->api_key);
    }

    const char *labels[] = {"Город",
                            "Страна",
                            "Широта",
                            "Долгота",
                            "Интервал (сек)",
                            "API Провайдер",
                            "API Ключ"};
    GtkWidget *fields[] = {w->city,     w->country,  w->lat,    w->lon,
                           w->interval, w->provider, w->api_key};
    for (int i = 0; i < 7; i++) {
        GtkWidget *lab = gtk_label_new(labels[i]);
        gtk_misc_set_alignment(GTK_MISC(lab), 0.0, 0.5);
        gtk_table_attach(GTK_TABLE(table), lab, 0, 1, i, i + 1, GTK_FILL,
                         GTK_FILL, 0, 0);
        gtk_table_attach(GTK_TABLE(table), fields[i], 1, 2, i, i + 1,
                         GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
    }

    GtkWidget *quick = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(box), quick, FALSE, FALSE, 0);
    struct {
        const char *label;
        const char *data;
    } cities[] = {{"Warsaw", "Warsaw|Poland"},
                  {"Minsk", "Minsk|Belarus"},
                  {"Astana", "Astana|Kazakhstan"},
                  {"Berlin", "Berlin|Germany"},
                  {"Paris", "Paris|France"},
                  {"New York", "New York City|United States"}};
    for (size_t i = 0; i < sizeof(cities) / sizeof(cities[0]); i++) {
        GtkWidget *btn = gtk_button_new_with_label(cities[i].label);
        g_object_set_data_full(G_OBJECT(btn), "city-data",
                               g_strdup(cities[i].data), g_free);
        g_signal_connect(btn, "clicked", G_CALLBACK(on_quick_city), w);
        gtk_box_pack_start(GTK_BOX(quick), btn, FALSE, FALSE, 0);
    }

    GtkWidget *buttons = gtk_hbox_new(FALSE, 8);
    gtk_box_pack_end(GTK_BOX(box), buttons, FALSE, FALSE, 0);
    GtkWidget *save = gtk_button_new_with_label("Сохранить");
    GtkWidget *cancel = gtk_button_new_with_label("Отмена");
    gtk_box_pack_end(GTK_BOX(buttons), save, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(buttons), cancel, FALSE, FALSE, 0);
    g_signal_connect(save, "clicked", G_CALLBACK(on_settings_save), w);
    g_signal_connect_swapped(cancel, "clicked", G_CALLBACK(gtk_widget_destroy),
                             w->window);

    gtk_widget_show_all(w->window);
}

void ui_show_help(GtkWindow *parent) {
    const char *text =
        "Как пользоваться\n\n"
        "Приложение работает в фоне и показывает температуру в системном трее.\n"
        "Взаимодействие — через правый клик по иконке:\n"
        "• Обновить сейчас\n"
        "• Подробная информация о погоде\n"
        "• Настройки\n"
        "• История API-запросов и ошибок\n"
        "• Выйти\n\n"
        "Местоположение задаётся либо городом+страной, либо координатами.\n"
        "Провайдеры: open-meteo (по умолчанию) и openweathermap (нужен apiKey).\n"
        "Настройки хранятся в settings.json рядом с рабочей директорией.";

    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), "Как пользоваться");
    gtk_window_set_default_size(GTK_WINDOW(win), 640, 480);
    gtk_window_set_transient_for(GTK_WINDOW(win), parent);
    gtk_container_add(GTK_CONTAINER(win),
                      make_scrolled_label(text, 620, 460, false));
    gtk_widget_show_all(win);
}

void ui_show_errors(GtkWindow *parent) {
    int n = history_error_count();
    GString *text = g_string_new(NULL);
    if (n == 0) {
        g_string_append(text, "Ошибок не было");
    } else {
        for (int i = n - 1; i >= 0; i--) {
            ApiError e;
            if (!history_get_error(i, &e)) {
                continue;
            }
            char tbuf[32];
            struct tm tm;
            localtime_r(&e.timestamp, &tm);
            strftime(tbuf, sizeof(tbuf), "%H:%M:%S", &tm);
            g_string_append_printf(text, "[%s] %s\n%s\nURL: %s\n", tbuf, e.api,
                                   e.error, e.url);
            if (e.status_code >= 0) {
                g_string_append_printf(text, "HTTP Status: %d\n", e.status_code);
            }
            g_string_append(text, "\n");
        }
    }

    char title[64];
    snprintf(title, sizeof(title), "История ошибок API (%d)", n);
    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), title);
    gtk_window_set_default_size(GTK_WINDOW(win), 720, 480);
    gtk_window_set_transient_for(GTK_WINDOW(win), parent);
    gtk_container_add(GTK_CONTAINER(win),
                      make_scrolled_label(text->str, 700, 460, true));
    g_string_free(text, TRUE);
    gtk_widget_show_all(win);
}

void ui_show_requests(GtkWindow *parent) {
    int n = history_request_count();
    GString *text = g_string_new(NULL);
    if (n == 0) {
        g_string_append(text, "Запросов ещё не было");
    } else {
        for (int i = n - 1; i >= 0; i--) {
            ApiRequest r;
            if (!history_get_request(i, &r)) {
                continue;
            }
            char tbuf[32];
            struct tm tm;
            localtime_r(&r.timestamp, &tm);
            strftime(tbuf, sizeof(tbuf), "%H:%M:%S", &tm);
            g_string_append_printf(text, "[%s] %s %s\nURL: %s\n", tbuf, r.method,
                                   r.api, r.url);
            if (r.response_status >= 0) {
                g_string_append_printf(text, "Status: %d, %ld ms\n",
                                       r.response_status, r.duration_ms);
            }
            g_string_append(text, "\n");
        }
    }

    char title[64];
    snprintf(title, sizeof(title), "Последние API-запросы (%d)", n);
    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), title);
    gtk_window_set_default_size(GTK_WINDOW(win), 720, 480);
    gtk_window_set_transient_for(GTK_WINDOW(win), parent);
    gtk_container_add(GTK_CONTAINER(win),
                      make_scrolled_label(text->str, 700, 460, true));
    g_string_free(text, TRUE);
    gtk_widget_show_all(win);
}

void ui_show_weather_details(GtkWindow *parent) {
    char tbuf[32] = "Не обновлялось";
    if (g_app.has_last_update) {
        struct tm tm;
        localtime_r(&g_app.last_update_time, &tm);
        strftime(tbuf, sizeof(tbuf), "%H:%M:%S", &tm);
    }

    char lat[32] = "N/A", lon[32] = "N/A";
    if (g_app.has_coords) {
        snprintf(lat, sizeof(lat), "%.6f", g_app.latitude);
        snprintf(lon, sizeof(lon), "%.6f", g_app.longitude);
    }

    double temp = g_app.weather.valid ? g_app.weather.temperature : 0.0;
    int code = g_app.weather.valid ? g_app.weather.weathercode : -1;

    char text[MAX_STR * 2 + 512];
    snprintf(text, sizeof(text),
             "Подробная информация о погоде\n\n"
             "Местоположение\n"
             "Город: %s\n"
             "Страна: %s\n"
             "Координаты: %s, %s\n\n"
             "Текущая погода\n"
             "Температура: %.1f °C\n"
             "Погодные условия: %s %s\n\n"
             "Последнее обновление: %s",
             g_app.city_name[0] ? g_app.city_name : "—",
             g_app.country_name[0] ? g_app.country_name : "—", lat, lon, temp,
             weather_emoji(code), weather_description(code), tbuf);

    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), "Подробная информация о погоде");
    gtk_window_set_default_size(GTK_WINDOW(win), 480, 360);
    gtk_window_set_transient_for(GTK_WINDOW(win), parent);
    gtk_container_add(GTK_CONTAINER(win),
                      make_scrolled_label(text, 460, 340, true));
    gtk_widget_show_all(win);
}
