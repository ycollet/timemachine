/*
 *  Copyright (C) 2003 Steve Harris
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

#ifndef __GTK_METERSCALE_H__
#define __GTK_METERSCALE_H__

#include <gtk/gtk.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GTK_TYPE_METERSCALE            (gtk_meterscale_get_type())
#define GTK_METERSCALE(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GTK_TYPE_METERSCALE, GtkMeterScale))
#define GTK_METERSCALE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GTK_TYPE_METERSCALE, GtkMeterScaleClass))
#define GTK_IS_METERSCALE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GTK_TYPE_METERSCALE))
#define GTK_IS_METERSCALE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GTK_TYPE_METERSCALE))

#define GTK_METERSCALE_LEFT    1
#define GTK_METERSCALE_RIGHT   2
#define GTK_METERSCALE_TOP     4
#define GTK_METERSCALE_BOTTOM  8

typedef struct _GtkMeterScale      GtkMeterScale;
typedef struct _GtkMeterScaleClass GtkMeterScaleClass;

struct _GtkMeterScale {
    GtkWidget parent_instance;

    guint  direction;

    gfloat lower;
    gfloat upper;
    gfloat iec_lower;
    gfloat iec_upper;

    /* Render cache — rebuilt only when the widget is resized */
    cairo_surface_t *cache;
    int              cache_width;
    int              cache_height;
};

struct _GtkMeterScaleClass {
    GtkWidgetClass parent_class;
};

GType      gtk_meterscale_get_type (void);
GtkWidget *gtk_meterscale_new      (gint direction, gfloat min, gfloat max);

#ifdef __cplusplus
}
#endif

#endif /* __GTK_METERSCALE_H__ */
