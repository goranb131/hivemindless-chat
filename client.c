#define _POSIX_C_SOURCE 200809L
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUF_SIZE 1024

// ANSI color codes
#define RED "\033[31m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define RESET "\033[0m"

// Global variables
char current_input[BUF_SIZE] = "";         // Save user's current input
char client_nickname[BUF_SIZE] = "Client"; // Client's nickname
pthread_mutex_t input_mutex = PTHREAD_MUTEX_INITIALIZER; // Mutex for thread-safe input handling

void error_exit(const char *message) {
    perror(message);
    exit(EXIT_FAILURE);
}

// Thread function for receiving messages
void *receive_messages(void *arg) {
    int sock_fd = *(int *)arg;
    char buffer[BUF_SIZE];

    while (1) {
        memset(buffer, 0, BUF_SIZE);
        ssize_t bytes_read = read(sock_fd, buffer, BUF_SIZE - 1);
        if (bytes_read <= 0) {
            printf(RED "\nServer disconnected.\n" RESET);
            close(sock_fd);
            exit(0);
        }

        pthread_mutex_lock(&input_mutex);

        // Clear the current line
        printf("\33[2K\r"); // ANSI code to clear the line
        fflush(stdout);

        // Print the received message
        printf("%s\n", buffer);

        // Reprint the prompt and current input
        printf(GREEN "You: %s" RESET, current_input);
        fflush(stdout);

        pthread_mutex_unlock(&input_mutex);
    }
    return NULL;
}

int main() {
    int sock_fd;
    struct sockaddr_in server_addr;
    char server_ip[BUF_SIZE];
    char buffer[BUF_SIZE];

    // Display header message
    printf(YELLOW "Collaborate, Think Free. No Bloat. Just Talk >_\n" RESET);

    // Prompt for server IP address
    printf(GREEN "Enter server IP address: " RESET);
    fgets(server_ip, BUF_SIZE, stdin);
    server_ip[strcspn(server_ip, "\n")] = '\0'; // Remove trailing newline

    // Prompt for client nickname
    printf(GREEN "Enter your nickname (default is 'Client'): " RESET);
    fgets(client_nickname, BUF_SIZE, stdin);
    client_nickname[strcspn(client_nickname, "\n")] = '\0'; // Remove trailing newline
    if (strlen(client_nickname) == 0) {
        strcpy(client_nickname, "Client"); // Default nickname
    }

    // Create socket
    if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        error_exit(RED "Socket creation failed" RESET);
    }

    // Define server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(12345);
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        error_exit(RED "Invalid address or address not supported" RESET);
    }

    // Connect to server
    printf(GREEN "Connecting to %s...\n" RESET, server_ip);
    if (connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        error_exit(RED "Connection failed" RESET);
    }
    printf(GREEN "Connected to server.\n" RESET);

    // Read server welcome message
    memset(buffer, 0, BUF_SIZE);
    read(sock_fd, buffer, BUF_SIZE - 1);
    printf(YELLOW "%s\n" RESET, buffer);

    // Create a thread to handle receiving messages
    pthread_t recv_thread;
    pthread_create(&recv_thread, NULL, receive_messages, &sock_fd);

    // Main thread handles sending messages
    char input_buffer[BUF_SIZE];
    while (1) {
        // Print prompt
        pthread_mutex_lock(&input_mutex);
        printf(GREEN "You: " RESET);
        fflush(stdout);
        pthread_mutex_unlock(&input_mutex);

        // Read user input
        memset(input_buffer, 0, BUF_SIZE);
        if (fgets(input_buffer, BUF_SIZE, stdin) == NULL) {
            // Handle Ctrl+D or EOF gracefully
            printf(RED "\nClient shutting down.\n" RESET);
            close(sock_fd);
            exit(0);
        }

        // Remove trailing newline
        input_buffer[strcspn(input_buffer, "\n")] = '\0';

        // Update current_input under mutex
        pthread_mutex_lock(&input_mutex);
        strncpy(current_input, input_buffer, BUF_SIZE);
        pthread_mutex_unlock(&input_mutex);

        // Prepare the message with nickname
        char message_with_nick[BUF_SIZE];
        snprintf(message_with_nick, BUF_SIZE, "[%s]: %s", client_nickname, input_buffer);

        // Send the message
        if (write(sock_fd, message_with_nick, strlen(message_with_nick)) == -1) {
            error_exit(RED "Send failed" RESET);
        }

        // Clear current_input after sending
        pthread_mutex_lock(&input_mutex);
        current_input[0] = '\0';
        pthread_mutex_unlock(&input_mutex);
    }

    close(sock_fd);
    return 0;
}