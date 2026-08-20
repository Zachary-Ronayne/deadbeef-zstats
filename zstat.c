/* Copyright (c) 2026, Zachary Ronayne. All rights reserved.   */
/* Distributed under the terms of the 3-Clause BSD License. */
/* Full license text available in 'LICENSE' file.           */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <deadbeef.h>

// Main instance for deadbeef
static DB_functions_t *deadbeef;

static int songFinished(char *songPath){
    deadbeef->log("song done %s", songPath);

    return 0;
}

// Places the path in the given path pointer, returns 0 on success. Must free path if 0 is not returned
static int copySongPath(ddb_event_track_t *event, char **path){
    // Deadbeef lock
    deadbeef->pl_lock();

    // Get the path to the song
    const char *songPath = deadbeef->pl_find_meta(event->track, ":URI");

    // If it wasn't obtained, unlock and fail
    if (!songPath) {
        deadbeef->pl_unlock();
        return -1;
    }

    // Copy the path out of the metadata
    char* pathCopy = strdup(songPath);
    *path = pathCopy;

    // Deadbeef unlock after grabbing the data
    deadbeef->pl_unlock();

    return 0;
}

// Called when deadbeef triggers an event
static int handle_event(uint32_t current_event, uintptr_t ctx, uint32_t p1, uint32_t p2){
    // Update stats when the track finishes playing
    if(current_event == DB_EV_SONGFINISHED) {
        // Get the data about the event
        ddb_event_track_t *event = (ddb_event_track_t *)ctx;
        // Get the path from the metadata
        char *songPath;
        int success = copySongPath(event, &songPath);

        // Exit on fail
        if(success != 0) return success;

        // Handle treating the song as played
        success = songFinished(songPath);

        // Free the memory used for the path
        free(songPath);

        // Return if the process was successful
        return success;
    }

    // Nothing to do, return success
    return 0;
}

// Run when deadbeef starts
static int start(void){
    deadbeef->log("zstats start\n");
    return 0;
}

// Called when the plugin stops
static int stop(void) {
    deadbeef->log("zstats stop\n");
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