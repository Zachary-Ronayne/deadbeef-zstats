/* Copyright (c) 2026, Zachary Ronayne. All rights reserved.   */
/* Distributed under the terms of the 3-Clause BSD License. */
/* Full license text available in 'LICENSE' file.           */
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>

#include <deadbeef.h>

// Metadata name for play count
#define META_PLAY_COUNT "zstat_play_count"
// Metadata name for last played timestamp
#define META_LAST_PLAYED "zstat_last_played"
// Metadata name for last played timestamp as an epoch timestamp
#define META_LAST_PLAYED_EPOCH "zstat_last_played_epoch"

// Main instance for deadbeef
static DB_functions_t *deadbeef;

// Places the path in the given path pointer, returns 0 on success. Must free path if 0 is not returned
static int copySongPath(DB_playItem_t *track, char **path){
    // Deadbeef lock
    deadbeef->pl_lock();

    // Get the path to the song
    const char *songPath = deadbeef->pl_find_meta(track, ":URI");

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


typedef struct Zstat{
    int64_t play_count;
    int64_t last_played;
} zstat;

// Find the stats for the given value
static void findsStat(DB_playItem_t *track, zstat *stat){
    // Grab the path from the track
    char *songPath;
    int success = copySongPath(track, &songPath);

    // TODO use path to get stats from sql db?
    const int play_count = deadbeef->pl_find_meta_int(track, META_PLAY_COUNT, 0);
    deadbeef->pl_lock();
    const char *last_played_raw = deadbeef->pl_find_meta(track, META_LAST_PLAYED_EPOCH);
    deadbeef->pl_unlock();

    int last_played;
    if(!last_played_raw) last_played = 0;
    else last_played = (int64_t)strtoll(last_played_raw, NULL, 10);

    // Free the memory from the copy
    free(songPath);

    // Update stats on the stat struct
    stat->play_count = play_count;
    stat->last_played = last_played;
}

// Places a string representation of the given number in str
static void stringValue(int64_t number, char *str, size_t stringSize){
    snprintf(str, stringSize, "%" PRId64, number);
}

// Places the number into the deadbeef meta for the given name
static void updateIntMeta(int64_t number, DB_playItem_t *track, char *name){
    // Get the string value from the number
    char numberStr[32];
    stringValue(number, numberStr, sizeof(numberStr));
    // Store that string in the metadata
    deadbeef->pl_replace_meta(track, name, numberStr);

}

// Update the stats of the given track
static int updateStat(DB_playItem_t *track){
    zstat stat;
    findsStat(track, &stat);

    // TODO add to the readme explaining how to display these fields

    updateIntMeta(stat.play_count, track, META_PLAY_COUNT);

    time_t last_played = stat.last_played;
    
    struct tm *tm_info = localtime(&last_played);
    char numberStr[64];

    // Default to a dash when no value is present
    if(last_played == 0) strcpy(numberStr, "-");
    else{
        // Populate an actual timestamp
        strftime(
            numberStr,
            sizeof(numberStr),
            // TODO make this time format configurable in the plugin config screen
            "%Y-%m-%d %H:%M:%S",
            tm_info
        );
    }
    deadbeef->pl_replace_meta(
        track,
        META_LAST_PLAYED,
        numberStr
    );
    updateIntMeta(0, track, META_LAST_PLAYED_EPOCH);

    return 0;
}

// Update the stats of all records when the plugin loads
static int updateStats(void){
    DB_playItem_t *track = deadbeef->pl_get_first(PL_MAIN);

    while(track){
        updateStat(track);

        DB_playItem_t *next = deadbeef->pl_get_next(track, PL_MAIN);
        deadbeef->pl_item_unref(track);
        track = next;
    }

    return 0;
}

// Run when deadbeef connects the plugin
static int connect(void){
    // TODO is this doing anything?
    return updateStats();
}

// Run when deadbeef starts
static int start(void){
    return 0;
}

// Called when the plugin stops
static int stop(void) {
    deadbeef->log("zstats stop\n");
    return 0;
}

static int songFinished(char *songPath, DB_playItem_t *track){
    // TODO add an explanation of this to the readme for how to use the fields

    // TODO figure out if this works consistently
    const int play_count = deadbeef->pl_find_meta_int(track, META_PLAY_COUNT, 0);
    updateIntMeta(play_count + 1, track, META_PLAY_COUNT);

    // TODO figure out if this works consistently
    // TODO store both epoch and 
    // Uupdate last played
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char numberStr[64];
    // TODO make this time format configurable in the plugin config screen
    // TODO abstract out these common lines
    // TODO make last played default to "-" and the epoch to 0 if there is no last played
    // TODO make sure that the values are set to default when the song initially plays, and doesn't double count
    strftime(
        numberStr,
        sizeof(numberStr),
        "%Y-%m-%d %H:%M:%S",
        tm_info
    );
    deadbeef->pl_replace_meta(
        track,
        META_LAST_PLAYED,
        numberStr
    );

    updateIntMeta((int64_t)now, track, META_LAST_PLAYED_EPOCH);

    return 0;
}

// Called when deadbeef triggers an event
static int handle_event(uint32_t current_event, uintptr_t ctx, uint32_t p1, uint32_t p2){

    if(current_event == DB_EV_PLAYLISTCHANGED){
        DB_playItem_t *playlist_track = deadbeef->pl_get_first(PL_MAIN);
        if (playlist_track) {
            // TODO see if this updates too frequently
            // deadbeef->log("event update stat\n");
            updateStats();
        }
    }

    // Update stats when the track finishes playing
    // TODO make sure this only triggers when the song plays all the way to the end, not when it is skipped
    if(current_event == DB_EV_SONGFINISHED) {
        // Get the data about the event
        ddb_event_track_t *event = (ddb_event_track_t *)ctx;
        // Get the path from the metadata
        char *songPath;
        DB_playItem_t *track = event->track;
        int success = copySongPath(track, &songPath);

        // Exit on fail
        if(success != 0) return success;

        // Handle treating the song as played
        success = songFinished(songPath, track);

        // Free the memory used for the path
        free(songPath);

        // Return if the process was successful
        return success;
    }

    // Nothing to do, return success
    return 0;
}

// Define the plugin
static DB_misc_t plugin = {
    .plugin = {
        .type = DB_PLUGIN_MISC,

        .api_vmajor = 1,
        .api_vminor = 10,

        .version_major = 0,
        .version_minor = 1,

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
        .connect = connect,
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