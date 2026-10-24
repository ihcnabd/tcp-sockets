// src/server.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define DEFAULT_PORT 8080
#define BUF_SIZE 1024
#define BACKLOG 5

/* send the whole buffer, even if send() only accepts part of it */
static int send_all(int fd, const char *data, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = send(fd, data + sent, len - sent, MSG_NOSIGNAL);
        if (n < 0) {
            return -1;
        }
        sent += (size_t)n;
    }
    return 0;
}

/* talk to one client until it disconnects.
   tcp is a byte stream, so messages are separated by '\n'. */
static void handle_client(int client_fd, const char *ip, int port) {
    char buf[BUF_SIZE];
    size_t used = 0;

    while (1) {
        ssize_t n = recv(client_fd, buf + used, sizeof(buf) - used - 1, 0);
        if (n < 0) {
            perror("recv");
            return;
        }
        if (n == 0) {
            return; /* client closed the connection */
        }
        used += (size_t)n;
        buf[used] = '\0';

        /* process every complete line in the buffer */
        char *start = buf;
        char *nl;
        while ((nl = strchr(start, '\n')) != NULL) {
            *nl = '\0';
            printf("Received from %s:%d -> %s\n", ip, port, start);

            char reply[BUF_SIZE + 16];
            int len = snprintf(reply, sizeof(reply), "ACK: %s\n", start);
            if (send_all(client_fd, reply, (size_t)len) < 0) {
                perror("send");
                return;
            }
            start = nl + 1;
        }

        /* keep any incomplete line for the next recv */
        used = strlen(start);
        memmove(buf, start, used + 1);

        if (used >= sizeof(buf) - 1) {
            fprintf(stderr, "Line too long, discarding\n");
            used = 0;
        }
    }
}

int main(int argc, char *argv[]) {
    int port = (argc > 1) ? atoi(argv[1]) : DEFAULT_PORT;
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Invalid port: %s\n", argv[1]);
        return 1;
    }

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("socket");
        return 1;
    }

    /* allow quick restart without "address already in use" */
    int opt = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(listen_fd);
        return 1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);

    if (bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(listen_fd);
        return 1;
    }

    if (listen(listen_fd, BACKLOG) < 0) {
        perror("listen");
        close(listen_fd);
        return 1;
    }

    printf("TCP server listening on port %d\n", port);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }

        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, ip, sizeof(ip));
        int client_port = ntohs(client_addr.sin_port);

        printf("Client connected: %s:%d\n", ip, client_port);
        handle_client(client_fd, ip, client_port);
        printf("Client disconnected: %s:%d\n", ip, client_port);

        close(client_fd);
    }

    close(listen_fd);
    return 0;
}