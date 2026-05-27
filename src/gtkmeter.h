/*
 *  Copyright (C) 2003 Steve Harris
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

#ifndef __GTK_METER_H__
#define __GTK_METER_H__

#include <gtk/gtk.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GTK_TYPE_METER            (gtk_meter_get_type())
#define GTK_METER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GTK_TYPE_METER, GtkMeter))
#define GTK_METER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GTK_TYPE_METER, GtkMeterClass))
#define GTK_IS_METER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GTK_TYPE_METER))
#define GTK_IS_METER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GTK_TYPE_METER))

#define GTK_METER_UP    0
#define GTK_METER_DOWN  1
#define GTK_METER_LEFT  2
#define GTK_METER_RIGHT 3

typedef struct _GtkMeter      GtkMeter;
typedef struct _GtkMeterClass GtkMeterClass;

struct _GtkMeter
{
    GtkWidget parent_instance;

    guint direction : 2;
    guint8 button;

    gfloat amber_level;
    gfloat amber_frac;

    gfloat iec_lower;
    gfloat iec_upper;

    gfloat peak;

    guint32 timer;

    gfloat old_value;
    gfloat old_lower;
    gfloat old_upper;

    GtkAdjustment *adjustment;
};

struct _GtkMeterClass
{
    GtkWidgetClass parent_class;
};

GType          gtk_meter_get_type       (void);
GtkWidget     *gtk_meter_new            (GtkAdjustment *adjustment, gint direction);
GtkAdjustment *gtk_meter_get_adjustment (GtkMeter *meter);
void           gtk_meter_set_adjustment (GtkMeter *meter, GtkAdjustment *adjustment);
void           gtk_meter_reset_peak     (GtkMeter *meter);
void           gtk_meter_set_warn_point (GtkMeter *meter, gfloat pt);

#ifdef __cplusplus
}
#endif

#endif /* __GTK_METER_H__ */
