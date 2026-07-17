#ifndef SERVER_H
#define SERVER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pthread.h>
#include <time.h>

#define SERVER_PORT 4444           // Port to listen on for incoming connections
#define BUFFER_SIZE 1024           // Buffer size for receiving network data
#define PROGRAM_SUCCESS 0          // Standard success exit code
#define PROGRAM_FAILURE 1          // Standard failure exit code
#define LOG_DIR "client_logs"      // Directory to store log files


/**
 * @brief Structure containing client connection information.
 * 
 * This structure is used to package the client's socket file descriptor 
 * and IP address so they can be safely passed to a dedicated handler thread 
 * upon a successful connection.
 */
typedef struct {
    int socket_fd;
    char ip_address[INET_ADDRSTRLEN];
} ClientData;

/**
 * @brief Creates a new log file with a timestamped filename, including the client IP.
 * @param client_ip The IP address of the connected client.
 * @return FILE pointer to the created log file, or NULL on error.
 */
FILE* create_client_log_file(const char* client_ip);

/**
 * @brief Handles Backspace events by truncating the last written char in the log.
 * @param log_file The file pointer to modify.
 */
void handle_backspace(FILE *log_file);

/**
 * @brief Thread function to handle incoming data from a specific client.
 * @param arg Pointer to the ClientData structure.
 * @return NULL upon thread termination.
 */
void *handle_client(void *arg);

#endif