#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <sys/stat.h>

#include <zstat_db.h>
#include <utils.h>
#include <sqlite3.h>

/*
TODO May want to optimize db operations to be in bulk, for now doing single operations.
2k songs loaded in 10-20 ms, good enough for me for now
*/

// TODO may want to investigate deadbeef being slow to close?


// TODO use the hash of the audio data to lookup the db record if the file path is not found

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

int zstat_db_find(char *file_path, zstat* stat){
    // Define query
    const char *sql =
        "SELECT play_count, last_played "
        "FROM zstat "
        "WHERE path = ?1 "
        ";";
    
    // Setup statement for path param
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);

    if(rc != SQLITE_OK){
        deadbeef->log("zstat: get stat prepare statement failed: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    // Bind path parameter
    rc = sqlite3_bind_text(stmt, 1, file_path, -1, SQLITE_TRANSIENT);
    // On fail, get rid of the statement
    if(rc != SQLITE_OK){
        deadbeef->log("zstat: get stat bind parameter failed: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return -1;
    }

    // Run the query
    rc = sqlite3_step(stmt);
    
    // Found a row
    if(rc == SQLITE_ROW){
        // Get the values
        int64_t play_count = sqlite3_column_int(stmt, 0);
        int64_t last_played = sqlite3_column_int(stmt, 1);
        stat->play_count = play_count;
        stat->last_played = last_played;
        rc = SQLITE_OK;
    }
    // Nothing found
    else if (rc == SQLITE_DONE) {
#ifdef DEBUG
        deadbeef->log("zstat: get stat, stat not found: %s\n", sqlite3_errmsg(db));
#endif
        rc = SQLITE_NOTFOUND;
    }

    // Delete the statement
    sqlite3_finalize(stmt);

    // Return whatever was found
    return -1;
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


