#include "icon.h"

#include <cairo.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static bool surface_to_png(cairo_surface_t *surface, const char *path) {
    cairo_status_t st = cairo_surface_write_to_png(surface, path);
    return st == CAIRO_STATUS_SUCCESS;
}

bool icon_write_temp_png(const char *path, const char *label) {
    const int size = 128;
    cairo_surface_t *surface =
        cairo_image_surface_create(CAIRO_FORMAT_ARGB32, size, size);
    cairo_t *cr = cairo_create(surface);

    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    bool muted = (strcmp(label, "--") == 0 || strcmp(label, "NA") == 0 || strcmp(label, "...") == 0);
    if (muted) {
        cairo_set_source_rgb(cr, 0.6, 0.6, 0.6); /* #999 */
    } else {
        cairo_set_source_rgb(cr, 0.259, 0.259, 0.259); /* #424242 */
    }

    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_BOLD);
    
    double font_size = size;
    cairo_text_extents_t ext;
    while (font_size > 5.0) {
        cairo_set_font_size(cr, font_size);
        cairo_text_extents(cr, label, &ext);
        if (ext.width <= size * 0.9 && ext.height <= size * 0.9) {
            break;
        }
        font_size -= 1.0;
    }

    double x = (size - ext.width) / 2.0 - ext.x_bearing;
    double y = (size - ext.height) / 2.0 - ext.y_bearing;
    cairo_move_to(cr, x, y);
    cairo_show_text(cr, label);

    cairo_destroy(cr);
    bool ok = surface_to_png(surface, path);
    cairo_surface_destroy(surface);
    return ok;
}

bool icon_write_weather_png(const char *path, int weathercode) {
    const int size = 128;
    cairo_surface_t *surface =
        cairo_image_surface_create(CAIRO_FORMAT_ARGB32, size, size);
    cairo_t *cr = cairo_create(surface);

    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    cairo_scale(cr, size / 32.0, size / 32.0);

    double cx = 16.0;
    double cy = 16.0;

    if (weathercode == 0 || weathercode == 1) {
        cairo_set_source_rgb(cr, 0.95, 0.75, 0.1);
        cairo_arc(cr, cx, cy, 8, 0, 2 * M_PI);
        cairo_fill(cr);
        if (weathercode == 1) {
            cairo_set_source_rgba(cr, 0.85, 0.85, 0.9, 0.9);
            cairo_arc(cr, cx + 6, cy + 4, 7, 0, 2 * M_PI);
            cairo_fill(cr);
        }
    } else if (weathercode == 2 || weathercode == 3 ||
               (weathercode >= 45 && weathercode <= 48)) {
        cairo_set_source_rgb(cr, 0.55, 0.58, 0.65);
        cairo_arc(cr, cx - 4, cy, 7, 0, 2 * M_PI);
        cairo_arc(cr, cx + 4, cy + 1, 8, 0, 2 * M_PI);
        cairo_arc(cr, cx, cy - 3, 6, 0, 2 * M_PI);
        cairo_fill(cr);
    } else if ((weathercode >= 51 && weathercode <= 67) ||
               (weathercode >= 80 && weathercode <= 82)) {
        cairo_set_source_rgb(cr, 0.55, 0.58, 0.65);
        cairo_arc(cr, cx, cy - 4, 8, 0, 2 * M_PI);
        cairo_fill(cr);
        cairo_set_source_rgb(cr, 0.2, 0.45, 0.85);
        cairo_set_line_width(cr, 2);
        for (int i = -1; i <= 1; i++) {
            cairo_move_to(cr, cx + i * 5, cy + 4);
            cairo_line_to(cr, cx + i * 5 - 1, cy + 12);
            cairo_stroke(cr);
        }
    } else if ((weathercode >= 71 && weathercode <= 77) ||
               (weathercode >= 85 && weathercode <= 86)) {
        cairo_set_source_rgb(cr, 0.7, 0.8, 0.95);
        cairo_arc(cr, cx, cy - 3, 8, 0, 2 * M_PI);
        cairo_fill(cr);
        cairo_set_source_rgb(cr, 0.85, 0.9, 1.0);
        for (int i = 0; i < 3; i++) {
            double x = cx - 6 + i * 6;
            cairo_move_to(cr, x, cy + 5);
            cairo_line_to(cr, x + 2, cy + 9);
            cairo_line_to(cr, x - 2, cy + 9);
            cairo_close_path(cr);
            cairo_fill(cr);
        }
    } else if (weathercode >= 95 && weathercode <= 99) {
        cairo_set_source_rgb(cr, 0.35, 0.35, 0.4);
        cairo_arc(cr, cx, cy - 4, 9, 0, 2 * M_PI);
        cairo_fill(cr);
        cairo_set_source_rgb(cr, 0.95, 0.85, 0.1);
        cairo_set_line_width(cr, 2.5);
        cairo_move_to(cr, cx + 2, cy + 2);
        cairo_line_to(cr, cx - 3, cy + 8);
        cairo_line_to(cr, cx + 1, cy + 8);
        cairo_line_to(cr, cx - 4, cy + 14);
        cairo_stroke(cr);
    } else {
        cairo_set_source_rgb(cr, 0.5, 0.5, 0.5);
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL,
                               CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, 16);
        cairo_text_extents_t ext;
        cairo_text_extents(cr, "?", &ext);
        cairo_move_to(cr, (32.0 - ext.width) / 2.0 - ext.x_bearing,
                      (32.0 - ext.height) / 2.0 - ext.y_bearing);
        cairo_show_text(cr, "?");
    }

    cairo_destroy(cr);
    bool ok = surface_to_png(surface, path);
    cairo_surface_destroy(surface);
    return ok;
}

bool icon_write_loading_png(const char *path) {
    const int size = 128;
    cairo_surface_t *surface =
        cairo_image_surface_create(CAIRO_FORMAT_ARGB32, size, size);
    cairo_t *cr = cairo_create(surface);

    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    cairo_scale(cr, size / 32.0, size / 32.0);

    double cx = 16.0;
    double cy = 16.0;
    double r = 10.0;

    cairo_set_line_width(cr, 3.0);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);

    cairo_set_source_rgba(cr, 0.65, 0.65, 0.65, 0.45);
    cairo_arc(cr, cx, cy, r, 0, 2 * M_PI);
    cairo_stroke(cr);

    cairo_set_source_rgb(cr, 0.2, 0.5, 0.85);
    cairo_arc(cr, cx, cy, r, -M_PI / 2.0, M_PI * 0.85);
    cairo_stroke(cr);

    cairo_destroy(cr);
    bool ok = surface_to_png(surface, path);
    cairo_surface_destroy(surface);
    return ok;
}
