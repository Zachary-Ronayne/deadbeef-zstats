/* Copyright (c) 2026, Zachary Ronayne. All rights reserved.   */
/* Distributed under the terms of the 3-Clause BSD License. */
/* Full license text available in 'LICENSE' file.           */
#include <stdio.h>

#include <deadbeef.h>

// Main instance for deadbeef
static DB_functions_t *deadbeef;

// Called when deadbeef triggers an event
static int handle_event(uint32_t current_event, uintptr_t ctx, uint32_t p1, uint32_t p2){
    fprintf(stderr, "zstats got event\n");
    return 0;
}

// Run when deadbeef starts
static int start(void){
    fprintf(stderr, "zstats start\n");
    return 0;
}

// Called when the plugin stops
static int stop(void) {
    fprintf(stderr, "zstats stop\n");
    return 0;
}

// Define the plugin
static DB_misc_t plugin = {
    .plugin = {
        .type = DB_PLUGIN_MISC,

        .api_vmajor = 1,
        .api_vminor = 10,

        .version_major = 3,
        .version_minor = 0,

        .id = "zstat",

        .name = "zstat",

        .descr = "Tracks stats from deadbeef",

        .copyright =
            "BSD 3-Clause License\n\n"
            "Copyright (c) 2026, Zachary Ronayne\n"
            "All rights reserved.",

        .website = "https://github.com/Zachary-Ronayne/deadbeef-zstats",

        .command = NULL,

        .start = start,
        .stop = stop,
        .connect = NULL,
        .disconnect = NULL,

        .exec_cmdline = NULL,

        .get_actions = NULL,

        .message = handle_event,

        .configdialog = NULL
    }
};

// Export the plugin
extern DB_plugin_t *zstat_load(DB_functions_t *api) {
    deadbeef = api;
    return &plugin.plugin;
}