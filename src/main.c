#include "app.h"
#include "compat.h"
#include "history.h"
#include "http.h"
#include "icon.h"
#include "log.h"
#include "settings.h"
#include "ui.h"
#include "weather.h"

#include <gtk/gtk.h>
#include <glib/gstdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#if defined(_WIN32) && !defined(__CYGWIN__)
#include <windows.h>
#endif

AppState g_app;

typedef struct {
    GtkStatusIcon *temp_icon;
    GtkStatusIcon *weather_icon;
    GtkWidget *menu;
    GtkWidget *item_temp;
    GtkWidget *item_weather;
    GtkWidget *item_updated;
    GtkWidget *item_coords;
    GtkWidget *item_lat;
    GtkWidget *item_lon;
    GtkWidget *item_location;
    GtkWidget *item_errors;
    GtkWidget *item_requests;
    char icon_dir[MAX_STR];
    unsigned icon_serial;
} TrayUI;

static TrayUI g_tray;
static CWMutex g_net_mutex;
static unsigned g_refresh_seq;
static unsigned g_apply_seq;

typedef struct {
    unsigned seq;
    bool ok;
    WeatherData data;
} RefreshResult;

typedef struct {
    unsigned seq;
    Settings backup;
} ApplyJob;

typedef struct {
    unsigned seq;
    Settings backup;
    bool loc_ok;
    bool weather_ok;
    WeatherData weather;
} ApplyResult;

static int round_temp(double temp) {
    if (temp >= 0) {
        return (int)(temp + 0.5);
    }
    return (int)(temp - 0.5);
}

static void format_temp_label(char *buf, size_t n, bool ok, double temp) {
    if (!ok) {
        snprintf(buf, n, "--");
        return;
    }
    snprintf(buf, n, "%d°", round_temp(temp));
}

static void status_icon_set_from_theme(GtkStatusIcon *icon, const char *dir,
                                       const char *name) {
    if (!icon || !name) {
        return;
    }
    if (dir && dir[0]) {
        gchar *file = g_strdup_printf("%s.png", name);
        gchar *path = g_build_filename(dir, file, NULL);
        g_free(file);
        if (g_file_test(path, G_FILE_TEST_IS_REGULAR)) {
            gtk_status_icon_set_from_file(icon, path);
            g_free(path);
            return;
        }
        g_free(path);
    }
    gtk_status_icon_set_from_icon_name(icon, name);
}

static void popup_tray_menu(GtkStatusIcon *status_icon, guint button,
                            guint32 activate_time) {
    if (!g_tray.menu || !status_icon) {
        return;
    }
    gtk_widget_show_all(g_tray.menu);
    gtk_menu_popup(GTK_MENU(g_tray.menu), NULL, NULL,
                   gtk_status_icon_position_menu, status_icon, button,
                   activate_time);
}

static void on_tray_popup(GtkStatusIcon *icon, guint button,
                          guint32 activate_time, gpointer data) {
    (void)data;
    popup_tray_menu(icon, button, activate_time);
}

static void on_tray_activate(GtkStatusIcon *icon, gpointer data) {
    (void)data;
    popup_tray_menu(icon, 1, gtk_get_current_event_time());
}

static void update_menu_labels(void) {
    char buf[MAX_STR * 2 + 64];
    if (g_app.weather.valid) {
        snprintf(buf, sizeof(buf), "Текущая температура: %.1f °C",
                 g_app.weather.temperature);
    } else {
        snprintf(buf, sizeof(buf), "Текущая температура: N/A");
    }
    gtk_menu_item_set_label(GTK_MENU_ITEM(g_tray.item_temp), buf);

    snprintf(buf, sizeof(buf), "Погода: %s",
             g_app.weather.valid
                 ? weather_description(g_app.weather.weathercode)
                 : "—");
    gtk_menu_item_set_label(GTK_MENU_ITEM(g_tray.item_weather), buf);

    if (g_app.has_last_update) {
        char tbuf[32];
        struct tm tm;
        localtime_r(&g_app.last_update_time, &tm);
        strftime(tbuf, sizeof(tbuf), "%H:%M:%S", &tm);
        snprintf(buf, sizeof(buf), "Обновлено: %s", tbuf);
    } else {
        snprintf(buf, sizeof(buf), "Обновлено: —");
    }
    gtk_menu_item_set_label(GTK_MENU_ITEM(g_tray.item_updated), buf);

    if (g_app.has_coords) {
        snprintf(buf, sizeof(buf), "Координаты: %.4f, %.4f", g_app.latitude,
                 g_app.longitude);
        gtk_menu_item_set_label(GTK_MENU_ITEM(g_tray.item_coords), buf);
        snprintf(buf, sizeof(buf), "LATITUDE: %.6f", g_app.latitude);
        gtk_menu_item_set_label(GTK_MENU_ITEM(g_tray.item_lat), buf);
        snprintf(buf, sizeof(buf), "LONGITUDE: %.6f", g_app.longitude);
        gtk_menu_item_set_label(GTK_MENU_ITEM(g_tray.item_lon), buf);
    }

    if (g_app.city_name[0] || g_app.country_name[0]) {
        snprintf(buf, sizeof(buf), "Местоположение: %s, %s", g_app.city_name,
                 g_app.country_name);
        gtk_menu_item_set_label(GTK_MENU_ITEM(g_tray.item_location), buf);
    }

    snprintf(buf, sizeof(buf), "Показать ошибки API (%d)", history_error_count());
    gtk_menu_item_set_label(GTK_MENU_ITEM(g_tray.item_errors), buf);
    snprintf(buf, sizeof(buf), "Показать последние API-запросы (%d)",
             history_request_count());
    gtk_menu_item_set_label(GTK_MENU_ITEM(g_tray.item_requests), buf);
}

static void update_tray_loading(void) {
    if (!g_tray.weather_icon || !g_tray.temp_icon) {
        return;
    }

    unsigned serial = ++g_tray.icon_serial;
    char weather_name[64];
    char temp_name[64];
    snprintf(weather_name, sizeof(weather_name), "c-weather-load-%u", serial);
    snprintf(temp_name, sizeof(temp_name), "c-weather-temp-%u", serial);

    gchar *weather_file = g_strdup_printf("%s.png", weather_name);
    gchar *temp_file = g_strdup_printf("%s.png", temp_name);
    gchar *weather_path =
        g_build_filename(g_tray.icon_dir, weather_file, NULL);
    gchar *temp_path = g_build_filename(g_tray.icon_dir, temp_file, NULL);
    g_free(weather_file);
    g_free(temp_file);

    if (!icon_write_loading_png(weather_path)) {
        log_errf("Не удалось записать иконку загрузки\n");
        g_free(weather_path);
        g_free(temp_path);
        return;
    }
    if (!icon_write_temp_png(temp_path, "...")) {
        log_errf("Не удалось записать иконку температуры\n");
    }

    status_icon_set_from_theme(g_tray.weather_icon, g_tray.icon_dir,
                               weather_name);
    status_icon_set_from_theme(g_tray.temp_icon, g_tray.icon_dir, temp_name);
    gtk_status_icon_set_tooltip_text(g_tray.temp_icon, "...");

    g_free(weather_path);
    g_free(temp_path);
}

static void update_tray_icons(void) {
    if (!g_tray.temp_icon || !g_tray.item_temp) {
        return;
    }

    char temp_label[16];
    format_temp_label(temp_label, sizeof(temp_label), g_app.weather.valid,
                      g_app.weather.temperature);

    unsigned serial = ++g_tray.icon_serial;
    char temp_name[64];
    char weather_name[64];
    snprintf(temp_name, sizeof(temp_name), "c-weather-temp-%u", serial);
    snprintf(weather_name, sizeof(weather_name), "c-weather-code-%u", serial);

    gchar *temp_file = g_strdup_printf("%s.png", temp_name);
    gchar *weather_file = g_strdup_printf("%s.png", weather_name);
    gchar *temp_path = g_build_filename(g_tray.icon_dir, temp_file, NULL);
    gchar *weather_path =
        g_build_filename(g_tray.icon_dir, weather_file, NULL);
    g_free(temp_file);
    g_free(weather_file);

    if (!icon_write_temp_png(temp_path, temp_label)) {
        log_errf("Не удалось записать иконку температуры\n");
    }
    int code = g_app.weather.valid ? g_app.weather.weathercode : -1;
    if (!icon_write_weather_png(weather_path, code)) {
        log_errf("Не удалось записать иконку погоды\n");
    }

    status_icon_set_from_theme(g_tray.temp_icon, g_tray.icon_dir, temp_name);
    status_icon_set_from_theme(g_tray.weather_icon, g_tray.icon_dir,
                               weather_name);

    char title[64];
    if (g_app.weather.valid) {
        snprintf(title, sizeof(title), "%.1f °C", g_app.weather.temperature);
    } else {
        snprintf(title, sizeof(title), "N/A");
    }
    gtk_status_icon_set_tooltip_text(g_tray.temp_icon, title);

    g_free(temp_path);
    g_free(weather_path);

    update_menu_labels();
}

static gboolean on_refresh_done(gpointer user_data) {
    RefreshResult *result = user_data;
    if (result->seq != g_refresh_seq) {
        g_free(result);
        return FALSE;
    }

    if (!result->ok) {
        log_errf("Ошибка получения погоды\n");
        g_app.weather.valid = false;
    } else {
        g_app.weather = result->data;
        g_app.last_update_time = time(NULL);
        g_app.has_last_update = true;
    }
    update_tray_icons();
    g_free(result);
    return FALSE;
}

static gpointer refresh_thread_fn(gpointer user_data) {
    unsigned seq = GPOINTER_TO_UINT(user_data);
    RefreshResult *result = g_new0(RefreshResult, 1);
    result->seq = seq;

    cw_mutex_lock(&g_net_mutex);
    result->ok = fetch_weather(&result->data);
    cw_mutex_unlock(&g_net_mutex);

    g_idle_add(on_refresh_done, result);
    return NULL;
}

void app_request_refresh(void) {
    update_tray_loading();
    g_refresh_seq++;
    cw_thread_spawn("weather-refresh", refresh_thread_fn,
                      GUINT_TO_POINTER(g_refresh_seq));
}

static gboolean on_update_timeout(gpointer user_data) {
    (void)user_data;
    app_request_refresh();
    return TRUE;
}

static void restart_update_timer(void) {
    if (g_app.update_timer_id != 0) {
        g_source_remove(g_app.update_timer_id);
        g_app.update_timer_id = 0;
    }
    guint seconds = (guint)g_app.settings.update_interval_seconds;
    if (seconds < 1) {
        seconds = 60;
    }
    g_app.update_timer_id = g_timeout_add_seconds(seconds, on_update_timeout, NULL);
}

static gboolean on_apply_done(gpointer user_data) {
    ApplyResult *result = user_data;
    if (result->seq != g_apply_seq) {
        g_free(result);
        return FALSE;
    }

    if (!result->loc_ok) {
        g_app.settings = result->backup;
        settings_save();
        update_tray_icons();
        ui_show_message(NULL, "Ошибка",
                        "Не удалось инициализировать местоположение", true);
        g_free(result);
        return FALSE;
    }

    restart_update_timer();
    if (!result->weather_ok) {
        log_errf("Ошибка получения погоды\n");
        g_app.weather.valid = false;
    } else {
        g_app.weather = result->weather;
        g_app.last_update_time = time(NULL);
        g_app.has_last_update = true;
    }
    update_tray_icons();
    ui_show_message(NULL, "Успех", "Настройки сохранены", false);
    g_free(result);
    return FALSE;
}

static gpointer apply_thread_fn(gpointer user_data) {
    ApplyJob *job = user_data;
    ApplyResult *result = g_new0(ApplyResult, 1);
    result->seq = job->seq;
    result->backup = job->backup;

    cw_mutex_lock(&g_net_mutex);
    result->loc_ok = initialize_location();
    if (result->loc_ok) {
        result->weather_ok = fetch_weather(&result->weather);
    }
    cw_mutex_unlock(&g_net_mutex);

    g_free(job);
    g_idle_add(on_apply_done, result);
    return NULL;
}

bool app_apply_settings_and_refresh(const Settings *new_settings,
                                    GtkWindow *parent) {
    Settings backup = g_app.settings;
    g_app.settings = *new_settings;

    if (!settings_save()) {
        g_app.settings = backup;
        ui_show_message(parent, "Ошибка", "Не удалось сохранить settings.json",
                        true);
        return false;
    }

    g_app.has_coords = false;
    g_app.city_name[0] = '\0';
    g_app.country_name[0] = '\0';

    update_tray_loading();
    g_refresh_seq++;
    g_apply_seq++;

    ApplyJob *job = g_new0(ApplyJob, 1);
    job->seq = g_apply_seq;
    job->backup = backup;
    cw_thread_spawn("weather-apply", apply_thread_fn, job);
    return true;
}

static void on_refresh(GtkMenuItem *item, gpointer data) {
    (void)item;
    (void)data;
    app_request_refresh();
}

static void on_details(GtkMenuItem *item, gpointer data) {
    (void)item;
    (void)data;
    ui_show_weather_details(NULL);
}

static void on_settings(GtkMenuItem *item, gpointer data) {
    (void)item;
    (void)data;
    ui_show_settings(NULL);
}

static void on_help(GtkMenuItem *item, gpointer data) {
    (void)item;
    (void)data;
    ui_show_help(NULL);
}

static void on_errors(GtkMenuItem *item, gpointer data) {
    (void)item;
    (void)data;
    ui_show_errors(NULL);
}

static void on_requests(GtkMenuItem *item, gpointer data) {
    (void)item;
    (void)data;
    ui_show_requests(NULL);
}

static void on_quit(GtkMenuItem *item, gpointer data) {
    (void)item;
    (void)data;
    gtk_main_quit();
}

static GtkWidget *add_insensitive_item(GtkWidget *menu, const char *label) {
    GtkWidget *item = gtk_menu_item_new_with_label(label);
    gtk_widget_set_sensitive(item, FALSE);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    return item;
}

static GtkWidget *add_action_item(GtkWidget *menu, const char *label,
                                  GCallback cb) {
    GtkWidget *item = gtk_menu_item_new_with_label(label);
    g_signal_connect(item, "activate", cb, NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    return item;
}

static void create_tray(void) {
    const char *tmpdir = g_get_tmp_dir();
    gchar *icon_dir =
        g_strdup_printf("%s/c-weather-gtk2-%d", tmpdir, (int)getpid());
    g_strlcpy(g_tray.icon_dir, icon_dir, sizeof(g_tray.icon_dir));
    g_free(icon_dir);
    g_mkdir_with_parents(g_tray.icon_dir, 0700);

    gchar *placeholder =
        g_build_filename(g_tray.icon_dir, "c-weather-temp-0.png", NULL);
    icon_write_temp_png(placeholder, "--");
    g_free(placeholder);
    placeholder =
        g_build_filename(g_tray.icon_dir, "c-weather-code-0.png", NULL);
    icon_write_weather_png(placeholder, -1);
    g_free(placeholder);

    gchar *temp0 =
        g_build_filename(g_tray.icon_dir, "c-weather-temp-0.png", NULL);
    gchar *code0 =
        g_build_filename(g_tray.icon_dir, "c-weather-code-0.png", NULL);
    g_tray.temp_icon = gtk_status_icon_new_from_file(temp0);
    g_tray.weather_icon = gtk_status_icon_new_from_file(code0);
    g_free(temp0);
    g_free(code0);

    g_signal_connect(g_tray.temp_icon, "popup-menu", G_CALLBACK(on_tray_popup),
                     NULL);
    g_signal_connect(g_tray.temp_icon, "activate", G_CALLBACK(on_tray_activate),
                     NULL);
    g_signal_connect(g_tray.weather_icon, "popup-menu",
                     G_CALLBACK(on_tray_popup), NULL);
    g_signal_connect(g_tray.weather_icon, "activate",
                     G_CALLBACK(on_tray_activate), NULL);

    gtk_status_icon_set_visible(g_tray.temp_icon, TRUE);
    gtk_status_icon_set_visible(g_tray.weather_icon, TRUE);
    gtk_status_icon_set_tooltip_text(g_tray.temp_icon, "Tray Weather");
    gtk_status_icon_set_tooltip_text(g_tray.weather_icon, "Tray Weather");

    g_tray.menu = gtk_menu_new();
    g_tray.item_temp = add_insensitive_item(g_tray.menu, "Текущая температура: —");
    g_tray.item_weather = add_insensitive_item(g_tray.menu, "Погода: —");
    gtk_menu_shell_append(GTK_MENU_SHELL(g_tray.menu),
                          gtk_separator_menu_item_new());
    g_tray.item_updated = add_insensitive_item(g_tray.menu, "Обновлено: —");
    gtk_menu_shell_append(GTK_MENU_SHELL(g_tray.menu),
                          gtk_separator_menu_item_new());
    g_tray.item_coords = add_insensitive_item(g_tray.menu, "Координаты: —");
    g_tray.item_lat = add_insensitive_item(g_tray.menu, "LATITUDE: —");
    g_tray.item_lon = add_insensitive_item(g_tray.menu, "LONGITUDE: —");
    gtk_menu_shell_append(GTK_MENU_SHELL(g_tray.menu),
                          gtk_separator_menu_item_new());
    g_tray.item_location =
        add_insensitive_item(g_tray.menu, "Местоположение: —");
    gtk_menu_shell_append(GTK_MENU_SHELL(g_tray.menu),
                          gtk_separator_menu_item_new());

    add_action_item(g_tray.menu, "Обновить сейчас", G_CALLBACK(on_refresh));
    add_action_item(g_tray.menu, "Подробная информация о погоде",
                    G_CALLBACK(on_details));
    add_action_item(g_tray.menu, "Настройки", G_CALLBACK(on_settings));
    add_action_item(g_tray.menu, "Как пользоваться", G_CALLBACK(on_help));
    g_tray.item_requests =
        add_action_item(g_tray.menu, "Показать последние API-запросы (0)",
                        G_CALLBACK(on_requests));
    g_tray.item_errors = add_action_item(
        g_tray.menu, "Показать ошибки API (0)", G_CALLBACK(on_errors));
    gtk_menu_shell_append(GTK_MENU_SHELL(g_tray.menu),
                          gtk_separator_menu_item_new());
    add_action_item(g_tray.menu, "Выйти", G_CALLBACK(on_quit));

    gtk_widget_show_all(g_tray.menu);
}

static void cleanup_icon_dir(void) {
    if (!g_tray.icon_dir[0]) {
        return;
    }
    GDir *dir = g_dir_open(g_tray.icon_dir, 0, NULL);
    if (dir) {
        const gchar *name;
        while ((name = g_dir_read_name(dir)) != NULL) {
            gchar *path = g_build_filename(g_tray.icon_dir, name, NULL);
            g_unlink(path);
            g_free(path);
        }
        g_dir_close(dir);
    }
    g_rmdir(g_tray.icon_dir);
}

/*
 * GUI binary is linked with -mwindows (no console on double-click / start).
 * If launched from an existing cmd.exe, attach to that console for logs.
 * Do not SetConsoleOutputCP(CP_UTF8): XP consoles mishandle UTF-8 bytes;
 * log_errf uses WriteConsoleW / OEM conversion instead.
 */
static void console_init(void) {
#if defined(_WIN32) && !defined(__CYGWIN__)
    if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
        return;
    }
    (void)freopen("CONOUT$", "w", stdout);
    (void)freopen("CONOUT$", "w", stderr);
#endif
}

int main(int argc, char **argv) {
    memset(&g_app, 0, sizeof(g_app));
    memset(&g_tray, 0, sizeof(g_tray));
    cw_mutex_init(&g_net_mutex);

    console_init();
    gtk_init(&argc, &argv);
    http_global_init();

    char cwd[MAX_STR];
    if (!getcwd(cwd, sizeof(cwd))) {
        snprintf(cwd, sizeof(cwd), ".");
    }
    gchar *path = g_build_filename(cwd, "settings.json", NULL);
    settings_set_path(path);
    g_free(path);
    settings_load();

    create_tray();
    atexit(cleanup_icon_dir);

    if (!initialize_location()) {
        log_errf("Ошибка инициализации местоположения\n");
        history_add_error("Инициализация",
                          "не удалось определить местоположение", "", -1);
        update_tray_icons();
    } else {
        app_request_refresh();
    }

    restart_update_timer();
    gtk_main();

    if (g_app.update_timer_id != 0) {
        g_source_remove(g_app.update_timer_id);
    }
    cw_mutex_clear(&g_net_mutex);
    http_global_cleanup();
    return 0;
}
