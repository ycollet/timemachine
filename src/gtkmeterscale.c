/*
 *  Copyright (C) 2003 Steve Harris
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

#include <math.h>
#include <stdio.h>
#include <gtk/gtk.h>
#include <pango/pangocairo.h>

#include "gtkmeterscale.h"

#define METERSCALE_MAX_FONT_SIZE 8
#define METERSCALE_DEFAULT_LENGTH 100

static void gtk_meterscale_dispose     (GObject           *object);
static void gtk_meterscale_snapshot    (GtkWidget         *widget,
                                        GtkSnapshot       *snapshot);
static void gtk_meterscale_measure     (GtkWidget         *widget,
                                        GtkOrientation     orientation,
                                        int                for_size,
                                        int               *minimum,
                                        int               *natural,
                                        int               *minimum_baseline,
                                        int               *natural_baseline);
static float iec_scale(float db);
static void meterscale_draw_notch_label(GtkMeterScale *meterscale, cairo_t *cr,
                                        int widget_width, int widget_height,
                                        float db, int mark,
                                        PangoRectangle *last_label_rect);
static void meterscale_draw_notch      (GtkMeterScale *meterscale, cairo_t *cr,
                                        int widget_width, int widget_height,
                                        float db, int mark);

G_DEFINE_TYPE(GtkMeterScale, gtk_meterscale, GTK_TYPE_WIDGET)

static void gtk_meterscale_class_init(GtkMeterScaleClass *class)
{
    GObjectClass   *object_class = G_OBJECT_CLASS(class);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(class);

    object_class->dispose  = gtk_meterscale_dispose;
    widget_class->snapshot = gtk_meterscale_snapshot;
    widget_class->measure  = gtk_meterscale_measure;

    gtk_widget_class_set_css_name(widget_class, "gtkmeterscale");
}

static void gtk_meterscale_init(GtkMeterScale *meterscale)
{
    meterscale->direction    = 0;
    meterscale->iec_lower    = 0.0f;
    meterscale->iec_upper    = 0.0f;
    meterscale->cache        = NULL;
    meterscale->cache_width  = 0;
    meterscale->cache_height = 0;
}

static void gtk_meterscale_dispose(GObject *object)
{
    GtkMeterScale *meterscale = GTK_METERSCALE(object);

    if (meterscale->cache) {
        cairo_surface_destroy(meterscale->cache);
        meterscale->cache = NULL;
    }

    G_OBJECT_CLASS(gtk_meterscale_parent_class)->dispose(object);
}

GtkWidget *gtk_meterscale_new(gint direction, float min, float max)
{
    GtkMeterScale *meterscale = g_object_new(GTK_TYPE_METERSCALE, NULL);

    meterscale->direction = direction;
    meterscale->lower     = min;
    meterscale->upper     = max;
    meterscale->iec_lower = iec_scale(min);
    meterscale->iec_upper = iec_scale(max);

    return GTK_WIDGET(meterscale);
}

static void gtk_meterscale_measure(GtkWidget *widget, GtkOrientation orientation,
                                   int for_size,
                                   int *minimum, int *natural,
                                   int *minimum_baseline, int *natural_baseline)
{
    GtkMeterScale *meterscale = GTK_METERSCALE(widget);
    PangoContext       *pc;
    PangoLayout        *pl;
    PangoFontDescription *pfd;
    PangoRectangle      rect;

    pc  = gtk_widget_get_pango_context(widget);
    pfd = pango_font_description_new();
    pango_font_description_set_family(pfd, "sans");
    pango_font_description_set_size(pfd, METERSCALE_MAX_FONT_SIZE * PANGO_SCALE);
    pl  = pango_layout_new(pc);
    pango_layout_set_font_description(pl, pfd);
    pango_font_description_free(pfd);
    pango_layout_set_text(pl, "99", -1);
    pango_layout_get_pixel_extents(pl, &rect, NULL);
    g_object_unref(pl);

    if (orientation == GTK_ORIENTATION_HORIZONTAL) {
        if (meterscale->direction & (GTK_METERSCALE_LEFT | GTK_METERSCALE_RIGHT))
            *minimum = *natural = rect.width + METERSCALE_MAX_FONT_SIZE;
        else
            *minimum = *natural = METERSCALE_DEFAULT_LENGTH;
    } else {
        if (meterscale->direction & (GTK_METERSCALE_TOP | GTK_METERSCALE_BOTTOM))
            *minimum = *natural = rect.height + METERSCALE_MAX_FONT_SIZE + 1;
        else
            *minimum = *natural = METERSCALE_DEFAULT_LENGTH;
    }

    if (minimum_baseline) *minimum_baseline = -1;
    if (natural_baseline) *natural_baseline = -1;
}

static void gtk_meterscale_snapshot(GtkWidget *widget, GtkSnapshot *snapshot)
{
    GtkMeterScale *meterscale = GTK_METERSCALE(widget);
    int width  = gtk_widget_get_width(widget);
    int height = gtk_widget_get_height(widget);

    /* The scale content is static; rebuild the cache only when resized. */
    if (!meterscale->cache ||
        meterscale->cache_width  != width ||
        meterscale->cache_height != height) {

        if (meterscale->cache)
            cairo_surface_destroy(meterscale->cache);

        meterscale->cache = cairo_image_surface_create(
            CAIRO_FORMAT_ARGB32, width, height);
        meterscale->cache_width  = width;
        meterscale->cache_height = height;

        cairo_t *cr = cairo_create(meterscale->cache);
        PangoRectangle lr = {0, 0, 0, 0};
        float val;

        cairo_set_source_rgb(cr, 0.85, 0.85, 0.85);
        cairo_rectangle(cr, 0, 0, width, height);
        cairo_fill(cr);

        meterscale_draw_notch_label(meterscale, cr, width, height, 0.0f, 3, &lr);

        for (val = 5.0f; val < meterscale->upper; val += 5.0f)
            meterscale_draw_notch_label(meterscale, cr, width, height, val, 2, &lr);

        for (val = -5.0f; val > meterscale->lower; val -= 5.0f)
            meterscale_draw_notch_label(meterscale, cr, width, height, val, 2, &lr);

        for (val = -10.0f; val < 10.0f; val += 1.0f)
            meterscale_draw_notch(meterscale, cr, width, height, val, 1);

        cairo_destroy(cr);
    }

    /* Blit the cached surface into the GTK4 render tree. */
    graphene_rect_t bounds = GRAPHENE_RECT_INIT(0, 0, width, height);
    cairo_t *cr = gtk_snapshot_append_cairo(snapshot, &bounds);
    cairo_set_source_surface(cr, meterscale->cache, 0, 0);
    cairo_paint(cr);
    cairo_destroy(cr);
}

static void meterscale_draw_notch_label(GtkMeterScale *meterscale, cairo_t *cr,
                                        int widget_width, int widget_height,
                                        float db, int mark,
                                        PangoRectangle *last_label_rect)
{
    int length, width, pos;
    int vertical = 0;

    if (meterscale->direction & (GTK_METERSCALE_LEFT | GTK_METERSCALE_RIGHT)) {
        length   = widget_height - 2;
        width    = widget_width  - 2;
        pos      = length - length * (iec_scale(db) - meterscale->iec_lower) /
                   (meterscale->iec_upper - meterscale->iec_lower);
        vertical = 1;
    } else {
        length = widget_width  - 2;
        width  = widget_height - 2;
        pos    = length * (iec_scale(db) - meterscale->iec_lower) /
                 (meterscale->iec_upper - meterscale->iec_lower);
    }

    if (last_label_rect) {
        PangoLayout          *pl;
        PangoFontDescription *pfd;
        PangoRectangle        rect;
        char text[128];
        int x, y, size;

        size = 6 + length / 150;
        if (size > METERSCALE_MAX_FONT_SIZE)
            size = METERSCALE_MAX_FONT_SIZE;

        pfd = pango_font_description_new();
        pango_font_description_set_family(pfd, "sans");
        pango_font_description_set_size(pfd, size * PANGO_SCALE);

        PangoContext *pc = pango_cairo_create_context(cr);
        pl = pango_layout_new(pc);
        g_object_unref(pc);
        pango_layout_set_font_description(pl, pfd);
        pango_font_description_free(pfd);

        snprintf(text, 127, "%.0f", fabs(db));
        pango_layout_set_text(pl, text, -1);
        pango_layout_get_pixel_extents(pl, NULL, &rect);

        if (vertical) {
            x = width / 2 - rect.width / 2 + 1;
            y = pos - rect.height / 2;
            if (y < 1) y = 1;
        } else {
            x = pos - rect.width / 2 + 1;
            y = width / 2 - rect.height / 2 + 1;
            if (x < 1)
                x = 1;
            else if (x + rect.width > length)
                x = length - rect.width + 1;
        }

        /* Skip if overlapping previous label */
        if (vertical &&
            last_label_rect->y < y + rect.height + 2 &&
            last_label_rect->y + last_label_rect->height + 2 > y) {
            g_object_unref(pl);
            meterscale_draw_notch(meterscale, cr, widget_width, widget_height, db, mark);
            return;
        }
        if (!vertical &&
            last_label_rect->x < x + rect.width + 2 &&
            last_label_rect->x + last_label_rect->width + 2 > x) {
            g_object_unref(pl);
            meterscale_draw_notch(meterscale, cr, widget_width, widget_height, db, mark);
            return;
        }

        cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
        cairo_move_to(cr, x, y);
        pango_cairo_show_layout(cr, pl);
        g_object_unref(pl);

        last_label_rect->width  = rect.width;
        last_label_rect->height = rect.height;
        last_label_rect->x      = x;
        last_label_rect->y      = y;
    }

    meterscale_draw_notch(meterscale, cr, widget_width, widget_height, db, mark);
}

static void meterscale_draw_notch(GtkMeterScale *meterscale, cairo_t *cr,
                                  int widget_width, int widget_height,
                                  float db, int mark)
{
    int pos, length, width;

    if (meterscale->direction & (GTK_METERSCALE_LEFT | GTK_METERSCALE_RIGHT)) {
        length = widget_height - 2;
        width  = widget_width  - 2;
        pos    = length - length * (iec_scale(db) - meterscale->iec_lower) /
                 (meterscale->iec_upper - meterscale->iec_lower);
    } else {
        length = widget_width  - 2;
        width  = widget_height - 2;
        pos    = length * (iec_scale(db) - meterscale->iec_lower) /
                 (meterscale->iec_upper - meterscale->iec_lower);
    }

    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);

    if (meterscale->direction & GTK_METERSCALE_LEFT) {
        cairo_rectangle(cr, 0, pos, mark, 1);
        cairo_fill(cr);
    }
    if (meterscale->direction & GTK_METERSCALE_RIGHT) {
        cairo_rectangle(cr, width - mark + 1, pos, mark, 1);
        cairo_fill(cr);
    }
    if (meterscale->direction & GTK_METERSCALE_TOP) {
        cairo_rectangle(cr, pos + 1, 1, 1, mark);
        cairo_fill(cr);
    }
    if (meterscale->direction & GTK_METERSCALE_BOTTOM) {
        cairo_rectangle(cr, pos + 1, width - mark + 1, 1, mark);
        cairo_fill(cr);
    }
}

static float iec_scale(float db)
{
    float def = 0.0f;

    if (db < -70.0f) {
        def = 0.0f;
    } else if (db < -60.0f) {
        def = (db + 70.0f) * 0.25f;
    } else if (db < -50.0f) {
        def = (db + 60.0f) * 0.5f + 5.0f;
    } else if (db < -40.0f) {
        def = (db + 50.0f) * 0.75f + 7.5f;
    } else if (db < -30.0f) {
        def = (db + 40.0f) * 1.5f + 15.0f;
    } else if (db < -20.0f) {
        def = (db + 30.0f) * 2.0f + 30.0f;
    } else {
        def = (db + 20.0f) * 2.5f + 50.0f;
    }

    return def;
}
