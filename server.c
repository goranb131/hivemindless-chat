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

#define PORT 12345
#define BUF_SIZE 1024
#define MAX_CLIENTS 100

// ANSI color codes
#define RED "\033[31m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define RESET "\033[0m"

// Structure to hold client information
typedef struct {
    int sock_fd;
    struct sockaddr_in addr;
    pthread_t thread;
    char nickname[BUF_SIZE];
} client_t;

// Global variables
char current_input[BUF_SIZE] = "";         // Server's current input
char server_nickname[BUF_SIZE] = "Server"; // Server nickname
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER; // Mutex for clients array
pthread_mutex_t input_mutex = PTHREAD_MUTEX_INITIALIZER;   // Mutex for input handling
client_t *clients[MAX_CLIENTS];                            // Array of client pointers
int server_fd;                                             // Server socket file descriptor

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

// Add client to clients array
void add_client(client_t *cl) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (!clients[i]) {
            clients[i] = cl;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

// Remove client from clients array
void remove_client(int sock_fd) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i]) {
            if (clients[i]->sock_fd == sock_fd) {
                clients[i] = NULL;
                break;
            }
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

// Check if a nickname is already taken
int is_nickname_taken(const char *nickname) {
    int taken = 0;
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i]) {
            if (strcmp(clients[i]->nickname, nickname) == 0) {
                taken = 1;
                break;
            }
        }
    }
    pthread_mutex_unlock(&clients_mutex);
    return taken;
}

// Broadcast message to all clients
void broadcast_message(char *message, int exclude_fd) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i]) {
            if (clients[i]->sock_fd != exclude_fd) {
                if (write(clients[i]->sock_fd, message, strlen(message)) == -1) {
                    perror("Broadcast failed");
                }
            }
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

// Thread function for handling client communication
void *handle_client(void *arg) {
    client_t *cli = (client_t *)arg;
    char buffer[BUF_SIZE];

    // Notify others about new client
    char join_message[BUF_SIZE];
    snprintf(join_message, BUF_SIZE, YELLOW "%s has joined the chat.\n" RESET, cli->nickname);
    printf("%s", join_message);
    broadcast_message(join_message, cli->sock_fd);

    while (1) {
        memset(buffer, 0, BUF_SIZE);
        ssize_t bytes_read = read(cli->sock_fd, buffer, BUF_SIZE - 1);
        if (bytes_read <= 0) {
            // Client disconnected
            printf(RED "\n%s has disconnected.\n" RESET, cli->nickname);
            // Notify others
            char leave_message[BUF_SIZE];
            snprintf(leave_message, BUF_SIZE, YELLOW "%s has left the chat.\n" RESET, cli->nickname);
            broadcast_message(leave_message, cli->sock_fd);
            close(cli->sock_fd);
            remove_client(cli->sock_fd);
            free(cli);
            break;
        }

        // Broadcast the message to other clients
        broadcast_message(buffer, cli->sock_fd);

        // Also print the message on the server console
        pthread_mutex_lock(&input_mutex);
        // Clear the current line
        printf("\33[2K\r");
        fflush(stdout);

        printf("%s\n", buffer);

        // Reprint the prompt and current input
        printf(GREEN "You: %s" RESET, current_input);
        fflush(stdout);
        pthread_mutex_unlock(&input_mutex);
    }

    pthread_detach(pthread_self());
    return NULL;
}

// Function to accept new clients
void *accept_clients(void *arg) {
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd == -1) {
            perror("Accept failed");
            continue;
        }

        // Handle nickname assignment
        char nickname[BUF_SIZE];
        while (1) {
            memset(nickname, 0, BUF_SIZE);
            // Receive client's nickname
            if (read(client_fd, nickname, BUF_SIZE - 1) <= 0) {
                perror("Failed to get client's nickname");
                close(client_fd);
                goto cleanup;
            }

            // Remove trailing newline
            nickname[strcspn(nickname, "\n")] = '\0';

            // Check if nickname is taken
            if (is_nickname_taken(nickname)) {
                // Send a message to client that nickname is taken
                char msg[BUF_SIZE] = "Nickname taken. Please enter a different nickname: ";
                if (write(client_fd, msg, strlen(msg)) <= 0) {
                    perror("Failed to send nickname rejection");
                    close(client_fd);
                    goto cleanup;
                }
            } else {
                // Nickname is unique, break out of loop
                break;
            }
        }

        // Create client structure
        client_t *cli = (client_t *)malloc(sizeof(client_t));
        cli->addr = client_addr;
        cli->sock_fd = client_fd;
        strncpy(cli->nickname, nickname, BUF_SIZE);

        // Add client to the list
        add_client(cli);

        // Send welcome message to the client
        char welcome_message[BUF_SIZE];
        snprintf(welcome_message, BUF_SIZE, "Welcome to the server, %s!", cli->nickname);
        if (write(cli->sock_fd, welcome_message, strlen(welcome_message)) <= 0) {
            perror("Failed to send welcome message");
        }

        // Create a thread for the client
        if (pthread_create(&cli->thread, NULL, handle_client, (void *)cli) != 0) {
            perror("Failed to create thread for client");
            close(client_fd);
            remove_client(client_fd);
            free(cli);
            continue;
        }

        continue;

    cleanup:
        close(client_fd);
        continue;
    }
    return NULL;
}

int main() {
    struct sockaddr_in server_addr;
    char buffer[BUF_SIZE];

    // Initialize clients array
    memset(clients, 0, sizeof(clients));

    // Display header message
    printf(YELLOW "Collaborate, Think Free. No Bloat. Just Talk >_\n" RESET);

    // Prompt for server nickname
    printf(GREEN "Enter your nickname (default is 'Server'): " RESET);
    fgets(server_nickname, BUF_SIZE, stdin);
    server_nickname[strcspn(server_nickname, "\n")] = '\0'; // Remove trailing newline
    if (strlen(server_nickname) == 0) {
        strcpy(server_nickname, "Server");
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
    if (listen(server_fd, 10) == -1) {
        error_exit("Listen failed");
    }
    printf(GREEN "Server listening on port %d...\n" RESET, PORT);

    // Enable raw mode for character-by-character input
    enable_raw_mode();

    // Create a thread to accept new clients
    pthread_t accept_thread;
    if (pthread_create(&accept_thread, NULL, accept_clients, NULL) != 0) {
        error_exit("Failed to create accept thread");
    }

    // Main thread handles server's own input
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

            if (strlen(current_input) == 0) {
                // Empty input, just print a new prompt
                printf("\n" GREEN "You: " RESET);
                fflush(stdout);
                index = 0;
                pthread_mutex_unlock(&input_mutex);
                continue;
            }

            // Prepare the message with nickname
            char message_with_nick[BUF_SIZE];
            snprintf(message_with_nick, BUF_SIZE, "[%s]: %s", server_nickname, current_input);

            // Broadcast the message to all clients
            broadcast_message(message_with_nick, -1); // -1 indicates server message

            // Also print the message on the server console
            // Clear the current line
            printf("\33[2K\r");
            fflush(stdout);

            printf("%s\n", message_with_nick);

            // Clear current_input
            index = 0;
            current_input[0] = '\0';

            // Print a new prompt
            printf(GREEN "You: " RESET);
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
    close(server_fd);
    return 0;
}