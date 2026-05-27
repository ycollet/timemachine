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

#include "gtkmeter.h"

#define METER_DEFAULT_WIDTH  10
#define METER_DEFAULT_LENGTH 100

static void gtk_meter_dispose                  (GObject          *object);
static void gtk_meter_snapshot                 (GtkWidget        *widget,
        GtkSnapshot      *snapshot);
static void gtk_meter_measure                  (GtkWidget        *widget,
        GtkOrientation    orientation,
        int               for_size,
        int              *minimum,
        int              *natural,
        int              *minimum_baseline,
        int              *natural_baseline);
static void gtk_meter_update                   (GtkMeter         *meter);
static void gtk_meter_adjustment_changed       (GtkAdjustment    *adjustment,
        gpointer          data);
static void gtk_meter_adjustment_value_changed (GtkAdjustment    *adjustment,
        gpointer          data);
static float iec_scale(float db);

G_DEFINE_TYPE(GtkMeter, gtk_meter, GTK_TYPE_WIDGET)

static void gtk_meter_class_init(GtkMeterClass *class)
{
    GObjectClass   *object_class = G_OBJECT_CLASS(class);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(class);

    object_class->dispose      = gtk_meter_dispose;
    widget_class->snapshot     = gtk_meter_snapshot;
    widget_class->measure      = gtk_meter_measure;

    gtk_widget_class_set_css_name(widget_class, "gtkmeter");
}

static void gtk_meter_init(GtkMeter *meter)
{
    meter->button       = 0;
    meter->direction    = GTK_METER_UP;
    meter->timer        = 0;
    meter->amber_level  = -6.0f;
    meter->amber_frac   = 0.0f;
    meter->iec_lower    = 0.0f;
    meter->iec_upper    = 0.0f;
    meter->peak         = 0.0f;
    meter->old_value    = 0.0;
    meter->old_lower    = 0.0;
    meter->old_upper    = 0.0;
    meter->adjustment   = NULL;
}

GtkWidget *gtk_meter_new(GtkAdjustment *adjustment, gint direction)
{
    GtkMeter *meter = g_object_new(GTK_TYPE_METER, NULL);

    if (!adjustment)
        adjustment = gtk_adjustment_new(0.0, 0.0, 0.0, 0.0, 0.0, 0.0);

    gtk_meter_set_adjustment(meter, adjustment);
    meter->direction = direction;

    return GTK_WIDGET(meter);
}

static void gtk_meter_dispose(GObject *object)
{
    GtkMeter *meter = GTK_METER(object);

    if (meter->adjustment) {
        g_signal_handlers_disconnect_by_data(meter->adjustment, meter);
        g_object_unref(meter->adjustment);
        meter->adjustment = NULL;
    }

    G_OBJECT_CLASS(gtk_meter_parent_class)->dispose(object);
}

GtkAdjustment *gtk_meter_get_adjustment(GtkMeter *meter)
{
    g_return_val_if_fail(meter != NULL, NULL);
    g_return_val_if_fail(GTK_IS_METER(meter), NULL);

    return meter->adjustment;
}

void gtk_meter_set_adjustment(GtkMeter *meter, GtkAdjustment *adjustment)
{
    g_return_if_fail(meter != NULL);
    g_return_if_fail(GTK_IS_METER(meter));

    if (meter->adjustment) {
        g_signal_handlers_disconnect_by_data(meter->adjustment, meter);
        g_object_unref(meter->adjustment);
    }

    meter->adjustment = adjustment;
    g_object_ref(meter->adjustment);

    g_signal_connect(adjustment, "changed",
                     G_CALLBACK(gtk_meter_adjustment_changed), meter);
    g_signal_connect(adjustment, "value-changed",
                     G_CALLBACK(gtk_meter_adjustment_value_changed), meter);

    meter->old_value  = gtk_adjustment_get_value(adjustment);
    meter->old_lower  = gtk_adjustment_get_lower(adjustment);
    meter->old_upper  = gtk_adjustment_get_upper(adjustment);
    meter->iec_lower  = iec_scale(gtk_adjustment_get_lower(adjustment));
    meter->iec_upper  = iec_scale(gtk_adjustment_get_upper(adjustment));
    meter->amber_frac = (iec_scale(meter->amber_level) - meter->iec_lower) /
                        (meter->iec_upper - meter->iec_lower);

    gtk_meter_update(meter);
}

static void gtk_meter_measure(GtkWidget *widget, GtkOrientation orientation,
                              int for_size,
                              int *minimum, int *natural,
                              int *minimum_baseline, int *natural_baseline)
{
    GtkMeter *meter = GTK_METER(widget);
    /* cross is always the thin dimension; along is always the long dimension.
     * Direction only decides which maps to horizontal vs vertical. */
    const int cross = METER_DEFAULT_WIDTH;
    const int along = METER_DEFAULT_LENGTH;

    if (orientation == GTK_ORIENTATION_HORIZONTAL)
        *minimum = *natural = (meter->direction == GTK_METER_UP ||
                               meter->direction == GTK_METER_DOWN) ? cross : along;
    else
        *minimum = *natural = (meter->direction == GTK_METER_UP ||
                               meter->direction == GTK_METER_DOWN) ? along : cross;

    if (minimum_baseline) *minimum_baseline = -1;
    if (natural_baseline) *natural_baseline = -1;
}

static void gtk_meter_snapshot(GtkWidget *widget, GtkSnapshot *snapshot)
{
    GtkMeter *meter = GTK_METER(widget);
    int width  = gtk_widget_get_width(widget);
    int height = gtk_widget_get_height(widget);
    float val, frac, peak_frac;
    int g_h, a_h, r_h;
    int length = 0, w = 0;

    graphene_rect_t bounds = GRAPHENE_RECT_INIT(0, 0, width, height);
    cairo_t *cr = gtk_snapshot_append_cairo(snapshot, &bounds);

    /* Dark background */
    cairo_set_source_rgb(cr, 0.15, 0.15, 0.15);
    cairo_rectangle(cr, 0, 0, width, height);
    cairo_fill(cr);

    if (!meter->adjustment) {
        cairo_destroy(cr);
        return;
    }

    switch (meter->direction) {
    case GTK_METER_UP:
    case GTK_METER_DOWN:
        length = height - 2;
        w      = width  - 2;
        break;
    case GTK_METER_LEFT:
    case GTK_METER_RIGHT:
        length = width  - 2;
        w      = height - 2;
        break;
    }

    val = iec_scale(gtk_adjustment_get_value(meter->adjustment));
    if (val > meter->peak)
        meter->peak = (val > meter->iec_upper) ? meter->iec_upper : val;

    frac      = (val         - meter->iec_lower) / (meter->iec_upper - meter->iec_lower);
    peak_frac = (meter->peak - meter->iec_lower) / (meter->iec_upper - meter->iec_lower);

    if (frac < meter->amber_frac) {
        g_h = frac * length;
        a_h = g_h;
        r_h = g_h;
    } else if (val <= 100.0f) {
        g_h = meter->amber_frac * length;
        a_h = frac * length;
        r_h = a_h;
    } else {
        g_h = meter->amber_frac * length;
        a_h = length * (100.0f - meter->iec_lower) /
              (meter->iec_upper - meter->iec_lower);
        r_h = frac * length;
    }

    if (a_h > length) a_h = length;
    if (r_h > length) r_h = length;

    /* Pixel position of the amber/yellow threshold along the bar */
    int amber_px = (int)(meter->amber_frac * length);

    switch (meter->direction) {
    case GTK_METER_RIGHT:
        /* Dim backlight — makes the meter visible even at silence */
        cairo_set_source_rgb(cr, 0.0, 0.1, 0.0);
        cairo_rectangle(cr, 1, 1, amber_px, w);
        cairo_fill(cr);
        cairo_set_source_rgb(cr, 0.1, 0.1, 0.0);
        cairo_rectangle(cr, 1 + amber_px, 1, length - amber_px, w);
        cairo_fill(cr);
        /* Signal bars */
        cairo_set_source_rgb(cr, 0.0, 0.9, 0.0);
        cairo_rectangle(cr, 1, 1, g_h, w);
        cairo_fill(cr);
        if (a_h > g_h) {
            cairo_set_source_rgb(cr, 0.75, 0.85, 0.0);
            cairo_rectangle(cr, 1 + g_h, 1, a_h - g_h, w);
            cairo_fill(cr);
        }
        if (r_h > a_h) {
            cairo_set_source_rgb(cr, 0.9, 0.0, 0.0);
            cairo_rectangle(cr, 1 + a_h, 1, r_h - a_h, w);
            cairo_fill(cr);
        }
        if (peak_frac > 0) {
            cairo_set_source_rgb(cr, 0.9, 0.9, 0.0);
            cairo_rectangle(cr, (int)(length * peak_frac), 1, 1, w);
            cairo_fill(cr);
        }
        break;

    case GTK_METER_UP:
    default:
        /* Dim backlight — makes the meter visible even at silence */
        cairo_set_source_rgb(cr, 0.1, 0.1, 0.0);
        cairo_rectangle(cr, 1, 1, w, length - amber_px);
        cairo_fill(cr);
        cairo_set_source_rgb(cr, 0.0, 0.1, 0.0);
        cairo_rectangle(cr, 1, length - amber_px + 1, w, amber_px);
        cairo_fill(cr);
        /* Signal bars */
        cairo_set_source_rgb(cr, 0.0, 0.9, 0.0);
        cairo_rectangle(cr, 1, length - g_h + 1, w, g_h);
        cairo_fill(cr);
        if (a_h > g_h) {
            cairo_set_source_rgb(cr, 0.75, 0.85, 0.0);
            cairo_rectangle(cr, 1, length - a_h + 1, w, a_h - g_h);
            cairo_fill(cr);
        }
        if (r_h > a_h) {
            cairo_set_source_rgb(cr, 0.9, 0.0, 0.0);
            cairo_rectangle(cr, 1, length - r_h + 1, w, r_h - a_h);
            cairo_fill(cr);
        }
        if (peak_frac > 0) {
            cairo_set_source_rgb(cr, 0.9, 0.9, 0.0);
            cairo_rectangle(cr, 1, (int)(length * (1.0f - peak_frac)) + 1, w, 1);
            cairo_fill(cr);
        }
        break;
    }

    cairo_destroy(cr);
}

static void gtk_meter_update(GtkMeter *meter)
{
    gfloat new_value;

    g_return_if_fail(meter != NULL);
    g_return_if_fail(GTK_IS_METER(meter));

    new_value = gtk_adjustment_get_value(meter->adjustment);

    if (new_value < gtk_adjustment_get_lower(meter->adjustment))
        new_value = gtk_adjustment_get_lower(meter->adjustment);
    if (new_value > gtk_adjustment_get_upper(meter->adjustment))
        new_value = gtk_adjustment_get_upper(meter->adjustment);

    if (new_value != gtk_adjustment_get_value(meter->adjustment))
        gtk_adjustment_set_value(meter->adjustment, new_value);

    gtk_widget_queue_draw(GTK_WIDGET(meter));
}

static void gtk_meter_adjustment_changed(GtkAdjustment *adjustment, gpointer data)
{
    GtkMeter *meter;

    g_return_if_fail(adjustment != NULL);
    g_return_if_fail(data != NULL);

    meter = GTK_METER(data);

    if ((meter->old_lower != gtk_adjustment_get_lower(adjustment)) ||
        (meter->old_upper != gtk_adjustment_get_upper(adjustment))) {
        meter->iec_lower = iec_scale(gtk_adjustment_get_lower(adjustment));
        meter->iec_upper = iec_scale(gtk_adjustment_get_upper(adjustment));

        gtk_meter_set_warn_point(meter, meter->amber_level);
        gtk_meter_update(meter);

        meter->old_value = gtk_adjustment_get_value(adjustment);
        meter->old_lower = gtk_adjustment_get_lower(adjustment);
        meter->old_upper = gtk_adjustment_get_upper(adjustment);
    } else if (meter->old_value != gtk_adjustment_get_value(adjustment)) {
        gtk_meter_update(meter);
        meter->old_value = gtk_adjustment_get_value(adjustment);
    }
}

static void gtk_meter_adjustment_value_changed(GtkAdjustment *adjustment, gpointer data)
{
    GtkMeter *meter;

    g_return_if_fail(adjustment != NULL);
    g_return_if_fail(data != NULL);

    meter = GTK_METER(data);

    if (meter->old_value != gtk_adjustment_get_value(adjustment)) {
        gtk_meter_update(meter);
        meter->old_value = gtk_adjustment_get_value(adjustment);
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
        def = (db + 60.0f) * 0.5f + 2.5f;
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

void gtk_meter_reset_peak(GtkMeter *meter)
{
    meter->peak = 0.0f;
}

void gtk_meter_set_warn_point(GtkMeter *meter, gfloat pt)
{
    meter->amber_level = pt;
    if (meter->direction == GTK_METER_LEFT || meter->direction == GTK_METER_DOWN) {
        meter->amber_frac = 1.0f - (iec_scale(meter->amber_level) - meter->iec_lower) /
                            (meter->iec_upper - meter->iec_lower);
    } else {
        meter->amber_frac = (iec_scale(meter->amber_level) - meter->iec_lower) /
                            (meter->iec_upper - meter->iec_lower);
    }
    gtk_widget_queue_draw(GTK_WIDGET(meter));
}
