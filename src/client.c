// src/client.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define DEFAULT_IP "127.0.0.1"
#define DEFAULT_PORT 8080
#define MSG_MAX 1000
#define REPLY_MAX 1100

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

/* read one line (up to '\n') from the socket.
   tcp is a byte stream, so a reply can arrive in several pieces.
   returns 1 on success, 0 if the server closed the connection, -1 on error */
static int recv_line(int fd, char *out, size_t max) {
    size_t i = 0;
    while (i < max - 1) {
        char c;
        ssize_t n = recv(fd, &c, 1, 0);
        if (n < 0) {
            return -1;
        }
        if (n == 0) {
            return 0;
        }
        if (c == '\n') {
            break;
        }
        out[i++] = c;
    }
    out[i] = '\0';
    return 1;
}

int main(int argc, char *argv[]) {
    const char *server_ip = (argc > 1) ? argv[1] : DEFAULT_IP;
    int port = (argc > 2) ? atoi(argv[2]) : DEFAULT_PORT;
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Invalid port: %s\n", argv[2]);
        return 1;
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid IP address: %s\n", server_ip);
        close(sockfd);
        return 1;
    }

    /* tcp needs a connection first (the three-way handshake happens here) */
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        close(sockfd);
        return 1;
    }

    printf("Connected to %s:%d (type 'quit' to exit)\n", server_ip, port);

    char message[MSG_MAX];
    char reply[REPLY_MAX];
    while (1) {
        printf("> ");
        fflush(stdout);
        if (fgets(message, sizeof(message), stdin) == NULL) {
            break;
        }
        message[strcspn(message, "\n")] = '\0';

        if (strcmp(message, "quit") == 0) {
            break;
        }
        if (message[0] == '\0') {
            continue;
        }

        /* the newline marks the end of the message for the server */
        size_t len = strlen(message);
        message[len] = '\n';
        if (send_all(sockfd, message, len + 1) < 0) {
            perror("send");
            break;
        }

        int r = recv_line(sockfd, reply, sizeof(reply));
        if (r < 0) {
            perror("recv");
            break;
        }
        if (r == 0) {
            printf("Server closed the connection\n");
            break;
        }
        printf("Server replied: %s\n", reply);
    }

    close(sockfd);
    return 0;
}