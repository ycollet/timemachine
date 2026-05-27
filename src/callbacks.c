#include <gtk/gtk.h>

#include "main.h"
#include "callbacks.h"
#include "interface.h"
#include "support.h"
#include "gtkmeter.h"
#include "gtkmeterscale.h"
#include "threads.h"
#ifdef HAVE_LIBLO
#include <lo/lo.h>
#endif

static int button_pressed = 0;

void on_togglebutton1_clicked(GtkButton *button, gpointer user_data)
{
    GtkWidget *img = lookup_widget(main_window, "toggle_image");

    if (!gtk_widget_is_sensitive(img))
        return;

    button_pressed = !button_pressed;

    if (button_pressed) {
        recording_start();
        gtk_image_set_from_pixbuf(GTK_IMAGE(img), img_on);
    } else {
        recording_stop();
        gtk_widget_set_sensitive(img, FALSE);
        gtk_image_set_from_pixbuf(GTK_IMAGE(img), img_busy);
    }
}

gboolean on_window_close_request(GtkWindow *window, gpointer user_data)
{
    cleanup();
    return FALSE;
}

#ifdef HAVE_LIBLO
int osc_handler(const char *path, const char *types, lo_arg **argv, int argc,
                lo_message msg, void *user_data)
{
    GtkWidget *img = lookup_widget(main_window, "toggle_image");

    if (user_data) {
        recording_start();
        gtk_image_set_from_pixbuf(GTK_IMAGE(img), img_on);
    } else {
        recording_stop();
        gtk_widget_set_sensitive(img, FALSE);
        gtk_image_set_from_pixbuf(GTK_IMAGE(img), img_busy);
    }

    return 0;
}

int osc_handler_nox(const char *path, const char *types, lo_arg **argv,
                    int argc, lo_message msg, void *user_data)
{
    if (user_data)
        recording_start();
    else
        recording_stop();

    return 0;
}
#endif
