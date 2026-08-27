#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <sys/stat.h>

#include <zstat_db.h>
#include <utils.h>
#include <sqlite3.h>

// Main instance for deadbeef
static DB_functions_t *deadbeef;

// Db instance
sqlite3 *db = NULL;

int zstat_db_init(DB_functions_t *deadbeef_instance){
    // Setup the deadbeef instance
    deadbeef = deadbeef_instance;

#ifdef DEBUG
    deadbeef->log("zstat: Initializing zstat db\n");
#endif

    // Try to use xdg home
    const char *home = getenv("XDG_DATA_HOME");

    // If that doesn't exist, use home
    if(home == NULL || home[0] == '\0') home = getenv("HOME");

    // If that still doesn't work, fail
     if(home == NULL || home[0] == '\0'){
        deadbeef->log("zstat: failed to find home path for zstat db");
        return -1;
     }

    // Build the actual file path for the db
    char db_path[FILE_PATH_MAX];
    snprintf(db_path, sizeof(db_path), "%s/deadbeef/zstat/zstat.db", home);

    // Make sure the path exists
    if(ensure_exists(db_path) != 0){
        deadbeef->log("error accessing directory for zstat db\n");
        return -1;
    }
    
#ifdef DEBUG
    deadbeef->log("zstat: db path: %s\n", db_path);
#endif

    // Setup the db file
    int rc = sqlite3_open(db_path, &db);
    char *error = NULL;
    if(rc != SQLITE_OK){
        deadbeef->log("zstat: error loading db: %s\n", error);
        return -1;
    }

    // Ensure the file has write permissions
    if(chmod(db_path, 0664) != 0){
        perror("chmod");
    }

    // Query to ensure the table exists
    const char *sql =
        "CREATE TABLE IF NOT EXISTS zstat ("
        "    path TEXT PRIMARY KEY,"
        "    play_count INTEGER NOT NULL DEFAULT 0,"
        "    last_played INTEGER NOT NULL DEFAULT 0, "
        "    hash BLOB"
        ");";

    // Create the table
    rc = sqlite3_exec(db, sql, NULL, NULL, &error);

    // Failed to create the table
    if(rc != SQLITE_OK){
        deadbeef->log("zstat: error initting db: %s\n", error);
        return -1;
    }

    // Return success
    return 0;
}

int zstat_db_find(int num_files, char *file_paths[], zstat stats[]){

    // Define the start of the query
    const char *start_query =
        "WITH requested(path_index, path) AS ( "
        "    VALUES ";

    // Leave slot for placeholders

    // Define the end of the query
    const char *end_query =
        " ) "
        "SELECT "
        "    requested.path, "
        "    COALESCE(zstat.play_count, 0) as play_count, "
        "    COALESCE(zstat.last_played, 0) as last_played, "
        "    CASE WHEN zstat.path IS NULL THEN 0 ELSE 1 END as found "
        "FROM requested "
        "LEFT JOIN zstat "
        "    ON zstat.path = requested.path "
        // Sort by the db keys to ensure the same sequence
        "ORDER BY requested.path_index ASC;";
    
    // Find the total size the query will need
    size_t query_size =
        // Size of the base query
        strlen(start_query) +
        // Six characters per placeholder
        (num_files * 6) +
        // Size of the base query
        strlen(end_query) +
        // Need space for the null terminator
        1;
    
    // Construct the query string
    char sql[query_size];
    // Add the the start
    int offset = snprintf(sql, sizeof(sql), "%s", start_query);

    // Add each placeholder
    for(size_t i = 0; i < num_files; i++){
        offset += snprintf(
            // The position of the array plus however far along the string the nex place is
            sql + offset,
            // The space remaining
            sizeof(sql) - offset,
            // Add the placeholder, and a comma if this is after the first placeholder
            "%s(?,?)", i > 0 ? "," : ""
        );
    }

    // Add the end of the query
    // Add the semicolon at the end
    snprintf(sql + offset, sizeof(sql) - offset, "%s", end_query);
    
    // Setup statement for path params
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);

    if(rc != SQLITE_OK){
        deadbeef->log("zstat: get stat prepare statement failed: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    // Bind path parameters
    for(size_t i = 0; i < num_files; i++){
        // Bind incrementing integer for clean sorting
        int param_index = 1 + i * 2;
        rc = sqlite3_bind_int(stmt, param_index, i);

        // On fail, get rid of the statement
        if(rc != SQLITE_OK){
            deadbeef->log("zstat: get stats bind int parameter failed: %s\n", sqlite3_errmsg(db));
            sqlite3_finalize(stmt);
            return -1;
        }

        // Bind path
        int param_path_index = 1 + i * 2 + 1;
        rc = sqlite3_bind_text(stmt, param_path_index, file_paths[i], -1, SQLITE_TRANSIENT);

        // On fail, get rid of the statement
        if(rc != SQLITE_OK){
            deadbeef->log("zstat: get stats bind path parameter failed: %s\n", sqlite3_errmsg(db));
            sqlite3_finalize(stmt);
            return -1;
        }

    }

    // Run the query and get all rows
    int next_index = 0;
    while(sqlite3_step(stmt) == SQLITE_ROW){
        // Find row data
        int64_t play_count = sqlite3_column_int(stmt, 1);
        int64_t last_played = sqlite3_column_int(stmt, 2);
        int exists = sqlite3_column_int(stmt, 3);

        // Nothing was found
        zstat *stat = &stats[next_index];
        if(exists == 0){
            stat->play_count = 0;
            stat->last_played = 0;
            stat->exists = 0;
        }
        // A row was found
        else{
            stat->play_count = play_count;
            stat->last_played = last_played;
            stat->exists = 1;
        }
        next_index++;
    }
    
    // Delete the statement
    sqlite3_finalize(stmt);

    // If not enough records were found, an error occurred
    if(next_index != num_files){
        deadbeef->log("zstat: get stat, expected %i rows, found only %i\n", num_files, next_index);
        return -1;
    }

    // Return success
    return 0;
}

int zstat_db_update(char *file_path, zstat stat){
    // Define query
    const char *sql =
        // Try to insert
        "INSERT INTO zstat (path, play_count, last_played) VALUES (?1, ?2, ?3) "
        // If the insert fails, set the row to the "excluded" field values that it tried to insert with
        "ON CONFLICT(path) DO UPDATE SET play_count = excluded.play_count, last_played = excluded.last_played; ";

    
    // Setup statement for parameters
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);

    // Failed to prepare the statement
    if(rc != SQLITE_OK){
        deadbeef->log("zstat: update stat, failed prepare: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    // Bind parameters
    sqlite3_bind_text(stmt, 1, file_path, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, (sqlite3_int64) stat.play_count);
    sqlite3_bind_int64(stmt, 3, (sqlite3_int64) stat.last_played);

    // Run the query
    rc = sqlite3_step(stmt);

    // Clean up the statement
    rc = sqlite3_finalize(stmt);
    
    // Query failed
    if(rc != SQLITE_OK){
        deadbeef->log("zstat: update stat, update failed: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    // Success
    return 0;
}

// TODO for the hash system, just return the struct containing the stats if it is missing or not, then separately grab all needed hashes and then update them