#define _POSIX_C_SOURCE 200809L
#include <stdio.h>        // For printf, perror
#include <stdlib.h>       // For exit
#include <string.h>       // For memset, strcspn
#include <unistd.h>       // For close, read, write
#include <arpa/inet.h>    // For inet_pton, htons
#include <sys/socket.h>   // For socket, connect
#include <netinet/in.h>   // For sockaddr_in
#include <pthread.h>      // For pthread_create

#define BUF_SIZE 1024

void error_exit(const char *message) {
    perror(message);
    exit(EXIT_FAILURE);
}

// Client nickname
char client_nickname[BUF_SIZE] = "Client";

// Thread function for receiving messages
void *receive_messages(void *arg) {
    int sock_fd = *(int *)arg;
    char buffer[BUF_SIZE];
    while (1) {
        memset(buffer, 0, BUF_SIZE);
        ssize_t bytes_read = read(sock_fd, buffer, BUF_SIZE - 1);
        if (bytes_read <= 0) {
            printf("\nServer disconnected.\n");
            close(sock_fd);
            exit(0);
        }
        printf("\r%s\nYou: ", buffer); // Display incoming message with sender's nickname
        fflush(stdout);
    }
    return NULL;
}

int main() {
    int sock_fd;
    struct sockaddr_in server_addr;
    char server_ip[BUF_SIZE];
    char buffer[BUF_SIZE];

    // Prompt for server IP address
    printf("Enter server IP address: ");
    fgets(server_ip, BUF_SIZE, stdin);
    server_ip[strcspn(server_ip, "\n")] = '\0'; // Remove trailing newline

    // Prompt for client nickname
    printf("Enter your nickname (default is 'Client'): ");
    fgets(client_nickname, BUF_SIZE, stdin);
    client_nickname[strcspn(client_nickname, "\n")] = '\0'; // Remove trailing newline
    if (strlen(client_nickname) == 0) {
        strcpy(client_nickname, "Client"); // Default if nothing entered
    }

    // Create socket
    if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        error_exit("Socket creation failed");
    }

    // Define server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(12345);
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        error_exit("Invalid address or address not supported");
    }

    // Connect to server
    if (connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        error_exit("Connection failed");
    }
    printf("Connected to server.\n");

    // Create a thread to handle receiving messages
    pthread_t recv_thread;
    pthread_create(&recv_thread, NULL, receive_messages, &sock_fd);

    // Main thread handles sending messages
    while (1) {
        printf("You: ");
        fflush(stdout);
        memset(buffer, 0, BUF_SIZE);

        // Format message: [nickname]: message
        fgets(buffer, BUF_SIZE, stdin);
        buffer[strcspn(buffer, "\n")] = '\0'; // Remove trailing newline
        char message_with_nick[BUF_SIZE];
        snprintf(message_with_nick, BUF_SIZE, "[%s]: %s", client_nickname, buffer);

        if (write(sock_fd, message_with_nick, strlen(message_with_nick)) == -1) {
            error_exit("Send failed");
        }
    }

    close(sock_fd);
    return 0;
}