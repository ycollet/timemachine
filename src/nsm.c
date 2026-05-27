#ifdef HAVE_LIBLO

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <lo/lo.h>
#include <gtk/gtk.h>

#include "main.h"
#include "nsm.h"

static lo_server_thread nsm_st = NULL;
static lo_address nsm_addr = NULL;
static char *nsm_prefix = NULL;

static int open_handler(const char *path, const char *types, lo_arg **argv,
                        int argc, lo_message msg, void *user_data)
{
    const char *session_path = &argv[0]->s;
    const char *display_name = &argv[1]->s;
    const char *client_id    = &argv[2]->s;

    (void)display_name;
    (void)client_id;

    g_free(nsm_prefix);
    nsm_prefix = g_strdup_printf("%s/", session_path);
    prefix = nsm_prefix;

    lo_send(nsm_addr, "/reply", "ss", "/nsm/client/open", "OK");

    return 0;
}

static int save_handler(const char *path, const char *types, lo_arg **argv,
                        int argc, lo_message msg, void *user_data)
{
    lo_send(nsm_addr, "/reply", "ss", "/nsm/client/save", "OK");

    return 0;
}

static int session_loaded_handler(const char *path, const char *types,
                                  lo_arg **argv, int argc, lo_message msg,
                                  void *user_data)
{
    return 0;
}

static gboolean show_gui_idle(gpointer user_data)
{
    if (main_window)
        gtk_window_present(GTK_WINDOW(main_window));
    return FALSE;
}

static gboolean hide_gui_idle(gpointer user_data)
{
    if (main_window)
        gtk_widget_set_visible(main_window, FALSE);
    return FALSE;
}

static int show_gui_handler(const char *path, const char *types, lo_arg **argv,
                            int argc, lo_message msg, void *user_data)
{
    g_idle_add(show_gui_idle, NULL);
    return 0;
}

static int hide_gui_handler(const char *path, const char *types, lo_arg **argv,
                            int argc, lo_message msg, void *user_data)
{
    g_idle_add(hide_gui_idle, NULL);
    return 0;
}

int nsm_init(const char *app_name, const char *executable)
{
    const char *nsm_url = getenv("NSM_URL");
    if (!nsm_url || nsm_url[0] == '\0')
        return 0;

    nsm_addr = lo_address_new_from_url(nsm_url);
    if (!nsm_addr) {
        fprintf(stderr, "timemachine: could not parse NSM_URL '%s'\n", nsm_url);
        return -1;
    }

    nsm_st = lo_server_thread_new(NULL, NULL);
    if (!nsm_st) {
        lo_address_free(nsm_addr);
        nsm_addr = NULL;
        return -1;
    }

    lo_server_thread_add_method(nsm_st, "/nsm/client/open",
                                "sss", open_handler, NULL);
    lo_server_thread_add_method(nsm_st, "/nsm/client/save",
                                "", save_handler, NULL);
    lo_server_thread_add_method(nsm_st, "/nsm/client/session_is_loaded",
                                "", session_loaded_handler, NULL);
    lo_server_thread_add_method(nsm_st, "/nsm/client/show_optional_gui",
                                "", show_gui_handler, NULL);
    lo_server_thread_add_method(nsm_st, "/nsm/client/hide_optional_gui",
                                "", hide_gui_handler, NULL);

    lo_server_thread_start(nsm_st);

    const char *server_url = lo_server_thread_get_url(nsm_st);
    lo_send(nsm_addr, "/nsm/server/announce", "sssiii",
            app_name,
            ":optional-gui:",
            executable,
            1, 2,
            (int)getpid());

    printf("Announced to NSM at %s (reply URL: %s)\n", nsm_url, server_url);

    return 0;
}

void nsm_free(void)
{
    if (nsm_st) {
        lo_server_thread_stop(nsm_st);
        lo_server_thread_free(nsm_st);
        nsm_st = NULL;
    }
    if (nsm_addr) {
        lo_address_free(nsm_addr);
        nsm_addr = NULL;
    }
    g_free(nsm_prefix);
    nsm_prefix = NULL;
}

#endif /* HAVE_LIBLO */
