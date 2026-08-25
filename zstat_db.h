#include <deadbeef.h>

// Holds the stats of a single track
typedef struct Zstat{
    int64_t play_count;
    int64_t last_played;
} zstat;

// Do initial setup for the db
int zstat_db_init(DB_functions_t *deadbeef_instance);

// Get the stats for a given track
int zstat_db_find(char *file_path, zstat* stat);

// Update the stats for the given track
int zstat_db_update(char *file_path, zstat stat);