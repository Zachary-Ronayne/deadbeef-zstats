#include <sys/stat.h>
#include <errno.h>
#include <limits.h>
#include <string.h>

const int FILE_PATH_MAX = 4096;

int ensure_exists(char *file_path){
    // If the path is too big, fail
    char path[FILE_PATH_MAX];
    if(strlen(file_path) >= sizeof(path)) return -1;

    // Copy the file path initially
    strcpy(path, file_path);

    // Get the part of the path up to the last directory
    char *p;
    p = strrchr(path, '/');
    if (!p) return 0;
    // Get rid of anything after the last directory
    *p = '\0';

    // Create each directory
    for(p = path + 1; *p; p++){
        // Ignore if it's not gotten to a directory yet
        if(*p != '/') continue;

        // Treat the current position as the end of the directory
        *p = '\0';
        // Attempt to make the directory, if it fails, return failure
        if(mkdir(path, 0755) != 0 && errno != EEXIST) return -1;

        // Put the current position back to the directory value
        *p = '/';
    }

    // Make the final directory
    if (mkdir(path, 0755) != 0 && errno != EEXIST) {
        // Check that the path is a directory, if it's not, return failure
        struct stat st;
        if(stat(path, &st) != 0 || !S_ISDIR(st.st_mode)) return -1;
    }
    
    // Return success
    return 0;
}