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

#define SERVER_PORT 4444           // Port to listen on for incoming connections
#define BUFFER_SIZE 1024           // Buffer size for receiving network data
#define PROGRAM_SUCCESS 0          // Standard success exit code
#define PROGRAM_FAILURE 1          // Standard failure exit code
#define LOG_DIR "client_logs"      // Directory to store log files

/**
 * @brief Structure containing client connection information, updated to hold UUID metadata.
 * 
 * This structure packages the client's socket file descriptor, IP address, 
 * and unique machine identifier (UUID) for safe concurrent handling in threads.
 */
typedef struct {
    int socket_fd;
    char ip_address[INET_ADDRSTRLEN];
    char uuid[64]; 
} ExtendedClientData;

/**
 * @brief Creates or opens a persistent log file based strictly on the client's unique UUID.
 * @param uuid The unique identifier string provided by the client during handshake.
 * @return FILE pointer to the opened/created log file, or NULL on error.
 */
FILE* get_or_create_uuid_log(const char* uuid);

/**
 * @brief Thread function to handle incoming data from a specific client.
 * @param arg Pointer to the ExtendedClientData structure.
 * @return NULL upon thread termination.
 */
void *handle_client(void *arg);

#endif