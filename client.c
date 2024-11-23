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
#include <termios.h>
#include <fcntl.h>

#define BUF_SIZE 1024

// ANSI color codes
#define RED "\033[31m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define RESET "\033[0m"

char current_input[BUF_SIZE] = "";         // Save user's current input
char client_nickname[BUF_SIZE] = "Client"; // Client's nickname
pthread_mutex_t input_mutex = PTHREAD_MUTEX_INITIALIZER; // Mutex for thread-safe input handling

void error_exit(const char *message) {
    perror(message);
    exit(EXIT_FAILURE);
}

// Function to set terminal to raw mode
struct termios orig_termios;

void disable_raw_mode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void enable_raw_mode() {
    struct termios raw;

    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disable_raw_mode);

    raw = orig_termios;
    raw.c_lflag &= ~(ICANON | ECHO); // Disable canonical mode and echo
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
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

    // Send client nickname to server
    if(write(sock_fd, client_nickname, strlen(client_nickname)) <= 0){
        error_exit(RED "Failed to send nickname to server" RESET);
    }

    // Read server welcome message
    memset(buffer, 0, BUF_SIZE);
    read(sock_fd, buffer, BUF_SIZE - 1);
    printf(YELLOW "%s\n" RESET, buffer);

    // Create a thread to handle receiving messages
    pthread_t recv_thread;
    pthread_create(&recv_thread, NULL, receive_messages, &sock_fd);

    // Enable raw mode for character-by-character input
    enable_raw_mode();

    // Main thread handles sending messages
    char ch;
    int index = 0;

    printf(GREEN "You: " RESET);
    fflush(stdout);

    while (1) {
        ch = getchar();

        pthread_mutex_lock(&input_mutex);

        if (ch == '\n') {
            // User pressed Enter, send the message
            current_input[index] = '\0';

            // Prepare the message with nickname
            char message_with_nick[BUF_SIZE];
            snprintf(message_with_nick, BUF_SIZE, "[%s]: %s", client_nickname, current_input);

            // Send the message
            if (write(sock_fd, message_with_nick, strlen(message_with_nick)) == -1) {
                error_exit(RED "Send failed" RESET);
            }

            // Clear current_input
            index = 0;
            current_input[0] = '\0';

            // Print a new prompt
            printf("\n" GREEN "You: " RESET);
            fflush(stdout);
        } else if (ch == 127 || ch == '\b') {
            // Handle backspace
            if (index > 0) {
                index--;
                current_input[index] = '\0';

                // Move cursor back, overwrite character with space, and move cursor back again
                printf("\b \b");
                fflush(stdout);
            }
        } else {
            // Add character to current_input
            if (index < BUF_SIZE - 1) {
                current_input[index++] = ch;
                current_input[index] = '\0';

                // Echo the character
                putchar(ch);
                fflush(stdout);
            }
        }

        pthread_mutex_unlock(&input_mutex);
    }

    disable_raw_mode();
    close(sock_fd);
    return 0;
}