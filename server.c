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

#define PORT 12345
#define BUF_SIZE 1024

// ANSI color codes
#define RED "\033[31m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define RESET "\033[0m"

char current_input[BUF_SIZE] = "";         // Save server user's current input
char server_nickname[BUF_SIZE] = "Server"; // Server nickname
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
    int client_fd = *(int *)arg;
    char buffer[BUF_SIZE];

    while (1) {
        memset(buffer, 0, BUF_SIZE);
        ssize_t bytes_read = read(client_fd, buffer, BUF_SIZE - 1);
        if (bytes_read <= 0) {
            printf(RED "\nClient disconnected.\n" RESET);
            close(client_fd);
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
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    char welcome_message[BUF_SIZE] = "Welcome to the server!";

    // Display header message
    printf(YELLOW "Collaborate, Think Free. No Bloat. Just Talk >_\n" RESET);

    // Prompt for server nickname
    printf(GREEN "Enter your nickname (default is 'Server'): " RESET);
    fgets(server_nickname, BUF_SIZE, stdin);
    server_nickname[strcspn(server_nickname, "\n")] = '\0'; // Remove trailing newline
    if (strlen(server_nickname) == 0) {
        strcpy(server_nickname, "Server");
    }

    // Prompt for optional welcome message
    printf(GREEN "Enter a welcome message for clients (optional): " RESET);
    fgets(welcome_message, BUF_SIZE, stdin);
    welcome_message[strcspn(welcome_message, "\n")] = '\0'; // Remove trailing newline
    if (strlen(welcome_message) == 0) {
        strcpy(welcome_message, "Welcome to the server!");
    }

    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        error_exit("Socket creation failed");
    }

    // Enable address reuse option
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        error_exit("setsockopt(SO_REUSEADDR) failed");
    }

    // Bind socket to address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        error_exit("Bind failed");
    }

    // Listen for connections
    if (listen(server_fd, 1) == -1) {
        error_exit("Listen failed");
    }
    printf(GREEN "Server listening on port %d...\n" RESET, PORT);

    // Accept a client connection
    if ((client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len)) == -1) {
        error_exit("Accept failed");
    }
    printf(GREEN "Client connected!\n" RESET);

    // Send welcome message to client
    if (write(client_fd, welcome_message, strlen(welcome_message)) <= 0) {
        error_exit("Failed to send welcome message");
    }

    // Create a thread to handle receiving messages
    pthread_t recv_thread;
    if (pthread_create(&recv_thread, NULL, receive_messages, &client_fd) != 0) {
        error_exit("Failed to create thread for receiving messages");
    }

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
            snprintf(message_with_nick, BUF_SIZE, "[%s]: %s", server_nickname, current_input);

            // Send the message
            if (write(client_fd, message_with_nick, strlen(message_with_nick)) == -1) {
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
    close(client_fd);
    close(server_fd);
    return 0;
}