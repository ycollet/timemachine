#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include <gtk/gtk.h>

#include "support.h"

GtkWidget *lookup_widget(GtkWidget *widget, const gchar *widget_name)
{
    GtkWidget *parent, *found_widget;

    for (;;) {
        parent = gtk_widget_get_parent(widget);
        if (!parent)
            parent = g_object_get_data(G_OBJECT(widget), "GladeParentKey");
        if (parent == NULL)
            break;
        widget = parent;
    }

    found_widget = (GtkWidget *) g_object_get_data(G_OBJECT(widget), widget_name);
    if (!found_widget)
        g_warning("Widget not found: %s", widget_name);
    return found_widget;
}

static GList *pixmaps_directories = NULL;

void add_pixmap_directory(const gchar *directory)
{
    pixmaps_directories = g_list_prepend(pixmaps_directories, g_strdup(directory));
}

static gchar *find_pixmap_file(const gchar *filename)
{
    GList *elem = pixmaps_directories;
    while (elem) {
        gchar *pathname = g_strdup_printf("%s%s%s", (gchar *) elem->data,
                                          G_DIR_SEPARATOR_S, filename);
        if (g_file_test(pathname, G_FILE_TEST_EXISTS))
            return pathname;
        g_free(pathname);
        elem = elem->next;
    }
    return NULL;
}

GtkWidget *create_pixmap(GtkWidget *widget, const gchar *filename)
{
    gchar     *pathname;
    GtkWidget *image;

    if (!filename || !filename[0])
        return gtk_image_new();

    pathname = find_pixmap_file(filename);
    if (!pathname) {
        g_warning("Couldn't find pixmap file: %s", filename);
        return gtk_image_new();
    }

    image = gtk_image_new_from_file(pathname);
    g_free(pathname);
    return image;
}

GdkTexture *create_texture(const gchar *filename)
{
    gchar      *pathname;
    GdkTexture *texture;
    GError     *error = NULL;

    if (!filename || !filename[0])
        return NULL;

    pathname = find_pixmap_file(filename);
    if (!pathname) {
        g_warning("Couldn't find pixmap file: %s", filename);
        return NULL;
    }

    texture = gdk_texture_new_from_filename(pathname, &error);
    if (!texture) {
        fprintf(stderr, "Failed to load texture: %s: %s\n",
                pathname, error->message);
        g_error_free(error);
    }
    g_free(pathname);
    return texture;
}
