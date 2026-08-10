#ifndef C_WEATHER_UI_H
#define C_WEATHER_UI_H

#include <gtk/gtk.h>
#include <stdbool.h>

void ui_show_settings(GtkWindow *parent);
void ui_show_help(GtkWindow *parent);
void ui_show_errors(GtkWindow *parent);
void ui_show_requests(GtkWindow *parent);
void ui_show_weather_details(GtkWindow *parent);
void ui_show_message(GtkWindow *parent, const char *title, const char *message,
                     bool is_error);

#endif
