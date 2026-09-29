/*
 *  Copyright (C) 2004 Steve Harris
 *  Copyright (C) 2006 Garett Shulman
 *  Copyright (C) 2009 Adam Sampson
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <jack/jack.h>
#include <getopt.h>
#include <sndfile.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#ifdef HAVE_LIBREADLINE
#include <readline/readline.h>
#include <readline/history.h>
#endif

#ifdef HAVE_LIBLO
#include <lo/lo.h>
#endif

#include "threads.h"
#include "interface.h"
#include "meters.h"
#include "support.h"
#include "main.h"
#include "nsm.h"

#define DEBUG(lvl, txt...) \
    if (verbosity >= lvl) fprintf(stderr, PACKAGE ": " txt)

const static int verbosity = 0;

GtkWidget *main_window;

int num_ports = DEFAULT_NUM_PORTS;
unsigned int buf_length = DEFAULT_BUF_LENGTH;

char *client_name = DEFAULT_CLIENT_NAME;
char *prefix = DEFAULT_PREFIX;
char *format_name = DEFAULT_FORMAT;
int format_sf = 0;
int safe_filename = 0;
int auto_record = 0;
float auto_begin_threshold = 0.0;
float auto_end_threshold = 0.0;
unsigned int auto_end_time = DEFAULT_AUTO_END_TIME;

jack_port_t *ports[MAX_PORTS];
jack_client_t *client;

GdkTexture *img_on, *img_off, *img_busy;

#ifdef HAVE_LIBLO
int osc_handler(const char *path, const char *types, lo_arg **argv, int argc,
                lo_message msg, void *user_data);
int osc_handler_nox(const char *path, const char *types, lo_arg **argv,
                    int argc, lo_message msg, void *user_data);
char *osc_port = DEFAULT_OSC_PORT;
#endif

int main(int argc, char *argv[])
{
    unsigned int i;
    int opt;
    int help = 0;
    int console = 0;
    char port_name[32];
    pthread_t dt;

    bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);
    textdomain(GETTEXT_PACKAGE);

    auto_begin_threshold = db2lin(DEFAULT_AUTO_BEGIN_THRESHOLD);
    auto_end_threshold   = db2lin(DEFAULT_AUTO_END_THRESHOLD);

    while ((opt = getopt(argc, argv, "hic:t:n:p:f:sab:e:T:o:")) != -1) {
        switch (opt) {
        case 'h':
            help = 1;
            break;
        case 'i':
            console = 1;
            break;
        case 'c':
            num_ports = atoi(optarg);
            DEBUG(1, "ports: %d\n", num_ports);
            break;
        case 't':
            buf_length = atoi(optarg);
            DEBUG(1, "buffer: %ds\n", buf_length);
            break;
        case 'n':
            client_name = optarg;
            DEBUG(1, "client name: %s\n", client_name);
            break;
        case 'p':
            prefix = optarg;
            DEBUG(1, "prefix: %s\n", prefix);
            break;
        case 'f':
            format_name = optarg;
            break;
        case 's':
            safe_filename = 1;
            break;
        case 'a':
            auto_record = 1;
            break;
        case 'b':
            auto_begin_threshold = db2lin(atof(optarg));
            break;
        case 'e':
            auto_end_threshold = db2lin(atof(optarg));
            break;
        case 'T':
            auto_end_time = atoi(optarg);
            break;
        case 'o':
#ifdef HAVE_LIBLO
            osc_port = optarg;
#endif
            break;
        default:
            num_ports = 0;
            break;
        }
    }

    if (optind != argc)
        num_ports = argc - optind;

    if (num_ports < 1 || num_ports > MAX_PORTS || help) {
        fprintf(stderr, _("Usage %s: [-h] [-i] [-c channels] [-n jack-name]\n\t"
                          "[-t buffer-length] [-p file prefix] [-f format]\n\t"
                          "[-a] [-b begin-threshold] [-e end-threshold] [-T end-time]\n\t"
                          "[port-name ...]\n\n"), argv[0]);
        fprintf(stderr, _("\t-h\tshow this help\n"));
        fprintf(stderr, _("\t-i\tinteractive mode (console instead of X11) also enabled\n\t\tif DISPLAY is unset\n"));
        fprintf(stderr, _("\t-c\tspecify number of recording channels\n"));
        fprintf(stderr, _("\t-n\tspecify the JACK name timemachine will use\n"));
        fprintf(stderr, _("\t-t\tspecify the pre-recording buffer length\n"));
        fprintf(stderr, _("\t-p\tspecify the saved file prefix, may include path\n"));
        fprintf(stderr, _("\t-s\tuse safer characters in filename (windows compatibility)\n"));
        fprintf(stderr, _("\t-f\tspecify the saved file format\n"));
        fprintf(stderr, _("\t-a\tenable automatic sound-triggered recording\n"));
        fprintf(stderr, _("\t-b\tspecify threshold above which automatic recording will begin\n"));
        fprintf(stderr, _("\t-e\tspecify threshold below which automatic recording will end\n"));
        fprintf(stderr, _("\t-T\tspecify silence length before automatic recording ends\n"));
#ifdef HAVE_LIBLO
        fprintf(stderr, _("\t-o\tspecify the OSC port timemachine will listen on\n"));
#endif
        fprintf(stderr, "\n");
        fprintf(stderr, _("\tchannels must be in the range 1-8, default %d\n"),
                DEFAULT_NUM_PORTS);
        fprintf(stderr, _("\tjack-name, default \"%s\"\n"), DEFAULT_CLIENT_NAME);
        fprintf(stderr, _("\tfile-prefix, default \"%s\"\n"), DEFAULT_PREFIX);
        fprintf(stderr, _("\tbuffer-length, default %d secs\n"), DEFAULT_BUF_LENGTH);
        fprintf(stderr, _("\tformat, default '%s', options: wav, w64, flac\n"), DEFAULT_FORMAT);
        fprintf(stderr, _("\tbegin-threshold, default %.1f dB\n"), DEFAULT_AUTO_BEGIN_THRESHOLD);
        fprintf(stderr, _("\tend-threshold, default %.1f dB\n"), DEFAULT_AUTO_END_THRESHOLD);
        fprintf(stderr, _("\tend-time, default %d secs\n"), DEFAULT_AUTO_END_TIME);
#ifdef HAVE_LIBLO
        fprintf(stderr, _("\tosc-port, default %s\n"), DEFAULT_OSC_PORT);
#endif
        fprintf(stderr, "\n");
        fprintf(stderr, _("specifying port names to connect to on the command line overrides -c\n\n"));
        exit(1);
    }

    if (!strcasecmp(format_name, "wav"))
        format_sf = SF_FORMAT_WAV | SF_FORMAT_FLOAT;
#ifdef HAVE_W64
    if (!strcasecmp(format_name, "w64"))
        format_sf = SF_FORMAT_W64 | SF_FORMAT_FLOAT;
#endif
#ifdef HAVE_FLAC
    if (!strcasecmp(format_name, "flac"))
        format_sf = SF_FORMAT_FLAC | SF_FORMAT_PCM_24;
#endif

    if (format_sf == 0)
        fprintf(stderr, _("Unknown format '%s'\n"), format_name);

    if ((client = jack_client_open(client_name, 0, NULL)) == 0) {
        DEBUG(0, "jack server not running?\n");
        exit(1);
    }
    DEBUG(1, "registering as %s\n", client_name);

    process_init(buf_length);

    jack_set_process_callback(client, process, 0);

    if (jack_activate(client)) {
        DEBUG(0, "cannot activate JACK client");
        exit(1);
    }

    for (i = 0; i < num_ports; i++) {
        jack_port_t *port;

        snprintf(port_name, 31, "in_%d", i + 1);
        ports[i] = jack_port_register(client, port_name,
                                      JACK_DEFAULT_AUDIO_TYPE,
                                      JackPortIsInput, 0);
        if (optind != argc) {
            port = jack_port_by_name(client, argv[optind + i]);
            if (port == NULL) {
                fprintf(stderr, "Can't find port '%s'\n", port_name);
                continue;
            }
            if (jack_connect(client, argv[optind + i], jack_port_name(ports[i])))
                fprintf(stderr, "Cannot connect port '%s' to '%s'\n",
                        argv[optind + i], jack_port_name(ports[i]));
        }
    }

    pthread_create(&dt, NULL, (void *)&writer_thread, NULL);

#ifdef HAVE_LIBLO
    nsm_init("TimeMachine", "timemachine");
#endif

#ifdef HAVE_LIBREADLINE
    /* On macOS (Quartz backend) DISPLAY is never set but GTK still works.
     * On Linux Wayland, WAYLAND_DISPLAY is set instead of DISPLAY. */
    if (console
#ifndef __APPLE__
        || ((!getenv("DISPLAY") || getenv("DISPLAY")[0] == '\0') &&
            (!getenv("WAYLAND_DISPLAY") || getenv("WAYLAND_DISPLAY")[0] == '\0'))
#endif
       ) {
#ifdef HAVE_LIBLO
        lo_server_thread st = lo_server_thread_new(osc_port, NULL);
        if (st) {
            lo_server_thread_add_method(st, "/start", "", osc_handler_nox, (void *)1);
            lo_server_thread_add_method(st, "/stop",  "", osc_handler_nox, (void *)0);
            lo_server_thread_start(st);
            printf("Listening for OSC requests on osc.udp://localhost:%s\n", osc_port);
        }
#endif
        int done = 0;
        while (!done) {
            char *line = readline(_("TimeMachine> "));
            if (!line) {
                printf("EOF\n");
                break;
            }
            if (line && *line) {
                add_history(line);
                if (strncmp(line, "q", 1) == 0)             done = 1;
                else if (strncmp(line, "start", 3) == 0)    recording_start();
                else if (strncmp(line, "stop",  3) == 0)    recording_stop();
                else if (strncmp(line, "help",  3) == 0)    printf(_("Commands: start stop\n"));
                else                                         printf(_("Unknown command\n"));
            }
            free(line);
        }
    } else
#endif
    {
        gtk_init();

        /* Resolve pixmaps relative to the executable so the app can be
         * launched from any working directory (e.g. via Finder on macOS). */
        {
            gchar *exe_dir = g_path_get_dirname(argv[0]);
            gchar *p = g_build_filename(exe_dir, "..", "pixmaps", NULL);
            add_pixmap_directory(p);
            g_free(p);
            p = g_build_filename(exe_dir, "pixmaps", NULL);
            add_pixmap_directory(p);
            g_free(p);

            /* App icon: GTK4 dropped pixbuf-based gtk_window_set_icon();
             * the only remaining mechanism is an icon-theme name, which
             * requires a hicolor/<size>/apps/<name> layout. The build tree
             * has one next to the executable (see CMakeLists.txt); system
             * installs get one under the standard /usr/share/icons search
             * path automatically. This only takes effect on X11 — GTK4
             * silently ignores icon names on Wayland and macOS. */
            GtkIconTheme *icon_theme = gtk_icon_theme_get_for_display(gdk_display_get_default());
            gtk_icon_theme_add_search_path(icon_theme, exe_dir);
            g_free(exe_dir);
        }
        add_pixmap_directory(PACKAGE_DATA_DIR "/" PACKAGE "/pixmaps");
        add_pixmap_directory("pixmaps");
        add_pixmap_directory("../pixmaps");

        img_on   = create_texture("on.png");
        img_off  = create_texture("off.png");
        img_busy = create_texture("busy.png");

        main_window = create_window(client_name);
        gtk_window_set_icon_name(GTK_WINDOW(main_window), "timemachine");
        gtk_window_present(GTK_WINDOW(main_window));

        bind_meters();
        g_timeout_add(100, meter_tick, NULL);

#ifdef HAVE_LIBLO
        lo_server_thread st = lo_server_thread_new(osc_port, NULL);
        if (st) {
            lo_server_thread_add_method(st, "/start", "", osc_handler, (void *)1);
            lo_server_thread_add_method(st, "/stop",  "", osc_handler, (void *)0);
            lo_server_thread_start(st);
            printf("Listening for OSC requests on osc.udp://localhost:%s\n", osc_port);
        }
#endif

        GMainLoop *main_loop = g_main_loop_new(NULL, FALSE);
        g_main_loop_run(main_loop);
    }

    cleanup();

    return 0;
}

void cleanup(void)
{
#ifdef HAVE_LIBLO
    nsm_free();
#endif

    jack_client_close(client);

    recording_quit();

    while (!recording_done)
        usleep(1000);

    DEBUG(0, "exiting\n");
    fflush(stderr);

    exit(0);
}

/* vi:set ts=8 sts=4 sw=4: */
