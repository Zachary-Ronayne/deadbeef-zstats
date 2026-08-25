/* Copyright (c) 2026, Zachary Ronayne. All rights reserved.   */
/* Distributed under the terms of the 3-Clause BSD License. */
/* Full license text available in 'LICENSE' file.           */
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <stdbool.h>
#include <glib.h>
#include <time.h>

#include <deadbeef.h>
#include <zstat_db.h>

// Metadata name for play count
#define META_PLAY_COUNT "zstat_play_count"
// Metadata name for last played timestamp
#define META_LAST_PLAYED "zstat_last_played"
// Metadata name for last played timestamp as an epoch timestamp
#define META_LAST_PLAYED_EPOCH "zstat_last_played_epoch"

// Main instance for deadbeef
static DB_functions_t *deadbeef;

// Timer to regularly update the current play time
static int timer_id = 0;

// Fields for tracking current playtime
static DB_playItem_t *current_track = NULL;
static float current_playpos = 0.0f;
static float current_duration = 0.0f;
static bool track_started = false;

// Update the timestamp the current track is playing
static void update_current_playtime(){
    // Do nothing if there is no track, or the track isn't playing yet
    if(!track_started || !current_track) return;

    // Find the current position of the track being played
    float playpos = deadbeef->streamer_get_playpos();

    // If that position is later than the current position, update the current position
    if (playpos > current_playpos) current_playpos = playpos;
}

static gboolean update_timer_callback(gpointer user_data){
    // User data is not needed
    (void)user_data;

    // Update the time
    update_current_playtime();

    // Tell the timer to keep going forever
    return G_SOURCE_CONTINUE;
}

// Allocate memory for the song in the given path pointer, returns 0 on success. Must free path if 0 is not returned
static int allocSongPath(DB_playItem_t *track, char **path){
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

// Get the stats for the given track
static zstat findsStat(DB_playItem_t *track){
    // Grab the path from the track
    char *songPath;
    int success = allocSongPath(track, &songPath);

#ifdef DEBUG
    deadbeef->log("Finding stats for %s\n", songPath);
#endif

    // Default stats to 0
    zstat stat;
    stat.play_count = 0;
    stat.last_played = 0;

    // Find the data
    zstat_db_find(songPath, &stat);

    // Free the memory from the copy
    free(songPath);

    // Return the data
    return stat;
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

// Update the last played timestamp string value for the given track to the given timestamp
static void updateMetaLastTimestamp(DB_playItem_t *track, time_t last_played){
    char numberStr[64];

    // Default to a dash when no value is present
    if(last_played == 0) strcpy(numberStr, "-");
    // Populate an actual timestamp
    else{
        // TODO make this time format a config
        struct tm *tm_info = localtime(&last_played);
        strftime(numberStr, sizeof(numberStr), "%Y-%m-%d %H:%M:%S", tm_info);
    }

    // Set the meta field
    deadbeef->pl_replace_meta(track, META_LAST_PLAYED, numberStr);
}

// Update the stats of the given track to the given values
static int update_deadbeef_meta(DB_playItem_t *track, zstat stat){
    // Update all values
    deadbeef->pl_lock();
    updateIntMeta(stat.play_count, track, META_PLAY_COUNT);
    updateMetaLastTimestamp(track, stat.last_played);
    updateIntMeta(stat.last_played, track, META_LAST_PLAYED_EPOCH);
    deadbeef->pl_unlock();

    // Return success
    return 0;
}

// Update the stats of all records when the plugin loads
static int updateStats(void){
    // Find the first track in the current playlist
    DB_playItem_t *track = deadbeef->pl_get_first(PL_MAIN);

#ifdef DEBUG
    // Track starting time
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
#endif

    int cnt = 0;

    // While there is still a track in the playlist, update it
    while(track){
        // Find the expected value of the stat
        zstat stat = findsStat(track);
        cnt++;

        // Update that track in deadbeef
        update_deadbeef_meta(track, stat);

        // Go to the next track
        DB_playItem_t *next = deadbeef->pl_get_next(track, PL_MAIN);
        deadbeef->pl_item_unref(track);
        track = next;
    }

#ifdef DEBUG
    // Log start up time
    clock_gettime(CLOCK_MONOTONIC, &end);
    long long elapsed_ms = (end.tv_sec - start.tv_sec) * 1000LL + (end.tv_nsec - start.tv_nsec) / 1000000LL;
    deadbeef->log("Took: %lld ms, cnt: %i\n", elapsed_ms, cnt);
#endif

    return 0;
}

// Run when deadbeef connects the plugin
static int connect(void){
    return 0;
}

// Run when deadbeef starts
static int start(void){
    // Initialize the db
    int db_success = zstat_db_init(deadbeef);
    if(db_success != 0){
        deadbeef->log("Failed to init zstat db\n");
        return db_success;
    }

    // Start up the timer, once a second, update the current playtime, only if the timer hasn't already been started
    if(timer_id == 0) timer_id = g_timeout_add(1000, update_timer_callback, NULL);

    // Return success
    return 0;
}

// Called when the plugin stops
static int stop(void) {
    // Clean up the timer
    if(timer_id != 0) {
        g_source_remove(timer_id);
        timer_id = 0;
    }

    // Return success
    return 0;
}

// A track has finished playing and the stats for that track need to be incremented
static int songFinished(char *songPath, DB_playItem_t *track){
    // Find data needed for updating the stat
    const int play_count = deadbeef->pl_find_meta_int(track, META_PLAY_COUNT, 0);
    time_t now = time(NULL);

    // Compute the new values of the stat
    zstat stat;
    stat.play_count = play_count + 1;
    stat.last_played = now;

#ifdef DEBUG
    deadbeef->log("Updating stats for %s\n", songPath);
#endif

    // Update the sql db
    zstat_db_update(songPath, stat);

    // Update the metadata in deadbeef
    update_deadbeef_meta(track, stat);

    // Return success
    return 0;
}

// Called when deadbeef triggers an event
static int handle_event(uint32_t current_event, uintptr_t ctx, uint32_t p1, uint32_t p2){

    // Different processing per event type
    switch(current_event){
        // If the playlist was changed, i.e. new songs were loaded, reload all stats
        case DB_EV_PLAYLISTCHANGED: {
            updateStats();
            return 0;
        }

        // If a song finished playing, update the stats
        case DB_EV_SONGFINISHED: {
            // Do nothing if the track hasn't started playing or there is no track
            if (!track_started || !current_track) return 0;

            // Update the values for when the track is played
            update_current_playtime();

            // If the track has played for some amount of time, find how long the track has been playing for
            if(current_duration > 0){
                // Duration remaining
                float remaining = current_duration - current_playpos;
                // Percentage still to play
                float percentage = current_playpos / current_duration;

                // TODO refine these conditions
                // If at least 95% of the song played, or there are less than 5 seconds left, count the song as played
                if(percentage >= 0.95f || remaining <= 5.0f) {
                   
                    // Get the path from the metadata
                    char *songPath;
                    int success = allocSongPath(current_track, &songPath);

                    // Exit on fail
                    if(success != 0) return success;

                    // Handle treating the song as played
                    success = songFinished(songPath, current_track);

                    // Free the memory used for the path
                    free(songPath);

                    // Reset for the next track
                    current_track = NULL;
                    current_playpos = 0.0f;
                    current_duration = 0.0f;
                    track_started = false;

                    return 0;
                }
            }
        }

        // When the song starts, update the current playing track
        case DB_EV_SONGSTARTED: {
            // Find the currently playing track
            ddb_event_track_t *event = (ddb_event_track_t *)ctx;
            if(!event || !event->track) return 0;

            // If there is a track playing, reset the current playback values
            current_track = event->track;
            current_playpos = 0.0f;
            current_duration = deadbeef->pl_get_item_duration(current_track);
            track_started = true;

            return 0;
        }
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

// Export the plugin so deadbeef can load it
extern DB_plugin_t *zstat_load(DB_functions_t *api) {
    deadbeef = api;
    return &plugin.plugin;
}