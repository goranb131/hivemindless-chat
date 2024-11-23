#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h> // <-- This is the missing header

#define PORT 12345
#define BUF_SIZE 1024

void error_exit(const char *message) {
    perror(message);
    exit(EXIT_FAILURE);
}

int main() {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    char buffer[BUF_SIZE];
    socklen_t addr_len = sizeof(client_addr);

    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        error_exit("Socket creation failed");
    }

    // Bind socket to address
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY; // Accept connections on any IP
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        error_exit("Bind failed");
    }

    // Listen for connections
    if (listen(server_fd, 1) == -1) {
        error_exit("Listen failed");
    }
    printf("Server listening on port %d...\n", PORT);

    // Accept a client connection
    if ((client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len)) == -1) {
        error_exit("Accept failed");
    }
    printf("Client connected!\n");

    // Chat loop
    while (1) {
        memset(buffer, 0, BUF_SIZE);
        ssize_t bytes_read = recv(client_fd, buffer, BUF_SIZE - 1, 0);
        if (bytes_read <= 0) {
            printf("Client disconnected or error occurred.\n");
            break;
        }
        printf("Client: %s", buffer);

        printf("You: ");
        fgets(buffer, BUF_SIZE, stdin);

        if (send(client_fd, buffer, strlen(buffer), 0) == -1) {
            error_exit("Send failed");
        }
    }

    close(client_fd);
    close(server_fd);
    return 0;
}
