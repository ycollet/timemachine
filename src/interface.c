#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include <gtk/gtk.h>

#include "main.h"
#include "callbacks.h"
#include "interface.h"
#include "support.h"
#include "gtkmeter.h"
#include "gtkmeterscale.h"

#define HOOKUP_OBJECT(component,widget,name) \
    g_object_set_data_full(G_OBJECT(component), name, \
        g_object_ref(widget), (GDestroyNotify) g_object_unref)

#define HOOKUP_OBJECT_NO_REF(component,widget,name) \
    g_object_set_data(G_OBJECT(component), name, widget)

GtkWidget *make_meter(float lower, float upper)
{
    GtkAdjustment *adjustment =
        gtk_adjustment_new(-60.0f, lower, upper, 0.0, 0.0, 0.0);
    return gtk_meter_new(adjustment, GTK_METER_RIGHT);
}

GtkWidget *create_window(const char *title)
{
    GtkWidget *window;
    GtkWidget *vbox1;
    GtkWidget *togglebutton1;
    GtkWidget *image1;
    GtkWidget *meter[MAX_PORTS];
    GtkWidget *scale;
    int i;

    window = gtk_window_new();
    gtk_widget_set_name(window, "window");
    gtk_window_set_title(GTK_WINDOW(window), title);

    vbox1 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_name(vbox1, "vbox1");
    gtk_window_set_child(GTK_WINDOW(window), vbox1);

    togglebutton1 = gtk_button_new();
    gtk_widget_set_name(togglebutton1, "togglebutton1");
    gtk_button_set_has_frame(GTK_BUTTON(togglebutton1), FALSE);
    gtk_box_append(GTK_BOX(vbox1), togglebutton1);

    image1 = create_pixmap(window, "off.png");
    gtk_widget_set_name(image1, "toggle_image");
    gtk_button_set_child(GTK_BUTTON(togglebutton1), image1);

    scale = gtk_meterscale_new(GTK_METERSCALE_BOTTOM, -60.0f, 6.0f);
    gtk_widget_set_name(scale, "scale_top");
    gtk_widget_set_size_request(scale, -1, 10);
    gtk_widget_set_focusable(scale, FALSE);
    gtk_box_append(GTK_BOX(vbox1), scale);

    for (i = 0; i < num_ports; i++) {
        char name[32];

        snprintf(name, 31, "meter%d", i);
        meter[i] = make_meter(-60, 6);
        gtk_widget_set_name(meter[i], name);
        gtk_meter_set_warn_point(GTK_METER(meter[i]), 0.0);
        gtk_widget_set_size_request(meter[i], -1, 10);
        gtk_widget_set_focusable(meter[i], FALSE);
        gtk_box_append(GTK_BOX(vbox1), meter[i]);
    }

    if (num_ports > 1) {
        scale = gtk_meterscale_new(GTK_METERSCALE_TOP, -60.0f, 6.0f);
        gtk_widget_set_name(scale, "scale_bottom");
        gtk_widget_set_size_request(scale, -1, 10);
        gtk_widget_set_focusable(scale, FALSE);
        gtk_box_append(GTK_BOX(vbox1), scale);
    }

    g_signal_connect(window, "close-request",
                     G_CALLBACK(on_window_close_request), NULL);
    g_signal_connect(togglebutton1, "clicked",
                     G_CALLBACK(on_togglebutton1_clicked), NULL);

    HOOKUP_OBJECT_NO_REF(window, window, "window");
    HOOKUP_OBJECT(window, vbox1, "vbox1");
    HOOKUP_OBJECT(window, togglebutton1, "togglebutton1");
    HOOKUP_OBJECT(window, image1, "toggle_image");
    for (i = 0; i < num_ports; i++) {
        char name[32];
        snprintf(name, 31, "meter%d", i);
        HOOKUP_OBJECT(window, meter[i], name);
    }

    return window;
}
