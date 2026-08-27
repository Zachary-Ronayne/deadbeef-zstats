#include <deadbeef.h>

// Holds the stats of a single track
typedef struct Zstat{
    int64_t play_count;
    int64_t last_played;
    int exists;
} zstat;

// Do initial setup for the db
int zstat_db_init(DB_functions_t *deadbeef_instance);

/*
Get the stats for a given track
@param num_files The size of both arrays
@param file_paths The path to each file to get stats for
@param stats An array of stats to place the results into. The size of this array must match with file_paths. The array index for file_paths will map one to one with the indexes in stats
@returns 0 on success
*/
int zstat_db_find(int num_files, char *file_paths[], zstat stats[]);

// Update the stats for the given track
int zstat_db_update(char *file_path, zstat stat);