#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mqueue.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

#define SERVER_QUEUE "/server_queue"
#define CLIENT_RESPONSE_QUEUE_PREFIX "/client_broadcast_"
#define CLIENT_SHUTDOWN_QUEUE_PREFIX "/client_shutdown_"
#define MAX_MSG_SIZE 1024

char client_response_queue_name[50];
char client_shutdown_queue_name[50];
mqd_t client_mq;  // Response queue descriptor

// Thread to listen for SHUTDOWN messages
void* listen_for_shutdown(void* arg) {
    mqd_t shutdown_mq = *((mqd_t*) arg);
    free(arg);
    char buffer[MAX_MSG_SIZE];
    unsigned long main_thread_id = pthread_self() % 1000000000;

    while (1) {
        ssize_t bytes_read = mq_receive(shutdown_mq, buffer, MAX_MSG_SIZE, NULL);
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            if (strcmp(buffer, "SHUTDOWN") == 0) {
                printf("\n----------------------------------------------------------------------\n");
                printf("<client-1> [Main Thread ** %09lu]: Gracefully exiting...\n", main_thread_id);
                
                // Ensure cleanup before exiting
                mq_close(shutdown_mq);
                mq_unlink(client_response_queue_name);
                mq_unlink(client_shutdown_queue_name);
                
                printf("<client-1> [Main Thread ** %09lu]: Resource cleanup complete...\n", main_thread_id);
                printf("<client-1> [Main Thread ** %09lu]: Shutting down...\n", main_thread_id);
                printf("----------------------------------------------------------------------\n");

                exit(0);
            }
        } else if (errno != EAGAIN) {
            perror("<client-1> Error receiving from shutdown queue");
        }
        usleep(100000);  // Prevent busy waiting
    }

    mq_close(shutdown_mq);
    pthread_exit(NULL);
}



int main() {
    char command[MAX_MSG_SIZE];
    char prompt[MAX_MSG_SIZE] = "> ";
    char final_command[MAX_MSG_SIZE];
    struct mq_attr attr = {0, 10, MAX_MSG_SIZE, 0};

    // Open server queue
    mqd_t server_mq = mq_open(SERVER_QUEUE, O_WRONLY);
    if (server_mq == (mqd_t)-1) {
        perror("mq_open failed for server queue");
        exit(1);
    }

    // Create client response queue
    sprintf(client_response_queue_name, "%s%d", CLIENT_RESPONSE_QUEUE_PREFIX, getpid());
    mq_unlink(client_response_queue_name);
    client_mq = mq_open(client_response_queue_name, O_CREAT | O_RDONLY | O_NONBLOCK, 0666, &attr);
    if (client_mq == (mqd_t)-1) {
        perror("mq_open failed for client response queue");
        exit(1);
    }

    // Create client shutdown queue
    sprintf(client_shutdown_queue_name, "%s%d", CLIENT_SHUTDOWN_QUEUE_PREFIX, getpid());
    mq_unlink(client_shutdown_queue_name);
    mqd_t shutdown_mq = mq_open(client_shutdown_queue_name, O_CREAT | O_RDONLY, 0666, &attr);
    if (shutdown_mq == (mqd_t)-1) {
        perror("mq_open failed for client shutdown queue");
        exit(1);
    }

    // Start shutdown listener thread
    pthread_t thread_ID;
    mqd_t* shutdown_mq_ptr = malloc(sizeof(mqd_t));
    if (!shutdown_mq_ptr) { perror("malloc failed"); exit(1); }
    *shutdown_mq_ptr = shutdown_mq;
    pthread_create(&thread_ID, NULL, listen_for_shutdown, shutdown_mq_ptr);

    // Print startup messages (only once)
    unsigned long main_thread_id = pthread_self() % 1000000000;
    unsigned long child_thread_id = (unsigned long)thread_ID;
    printf("<client-1> [Main Thread -- %09lu]: I am the Client's Main Thread. My Parent Process is (PID: %d)...\n",
           main_thread_id, getpid());
    printf("<client-1> [Main Thread -- %09lu]: Created a Child Thread [%015lu] for listening to the server's SHUTDOWN broadcast message...\n",
           main_thread_id, child_thread_id);
    printf("<client-1> [Main Thread -- %09lu]: Client initialized. Enter commands (type 'EXIT' to quit)...\n",
           main_thread_id);

    // Register with the server: "REGISTER <pid> <queue_name>"
    {
        char register_command[MAX_MSG_SIZE];
        snprintf(register_command, MAX_MSG_SIZE, "REGISTER %d %s", getpid(), client_response_queue_name);
        mq_send(server_mq, register_command, strlen(register_command) + 1, 0);
    }

    // Main input loop
while (1) {
    printf("\n%sEnter Command: ", prompt);
    fflush(stdout);
    if (!fgets(command, MAX_MSG_SIZE, stdin))
        continue;
    command[strcspn(command, "\n")] = 0;
    if (strlen(command) == 0 || strspn(command, " \t") == strlen(command)) {
        printf("Invalid input. Please enter a valid command.\n");
        continue;
    }

    // Handle CHPT locally before wrapping any command
    if (strncmp(command, "CHPT ", 5) == 0) {
        char new_prompt[MAX_MSG_SIZE];
        if (sscanf(command + 5, "%1022s", new_prompt) == 1) {
            snprintf(prompt, sizeof(prompt), "%.1021s ", new_prompt);
            printf("Prompt changed to '%s'\n", prompt);
        } else {
            printf("Invalid command. Usage: CHPT <new_prompt>\n");
        }
        continue;
    }

    // Prepare final_command by appending PID for commands the server expects
    if (strcmp(command, "LIST") == 0)
        snprintf(final_command, MAX_MSG_SIZE, "LIST %d", getpid());
    else if (strcmp(command, "HIDE") == 0)
        snprintf(final_command, MAX_MSG_SIZE, "HIDE %d", getpid());
    else if (strcmp(command, "UNHIDE") == 0)
        snprintf(final_command, MAX_MSG_SIZE, "UNHIDE %d", getpid());
    else if (strcmp(command, "EXIT") == 0)
        snprintf(final_command, MAX_MSG_SIZE, "EXIT %d", getpid());
    else if (strcmp(command, "exit") == 0)
        snprintf(final_command, MAX_MSG_SIZE, "exit %d", getpid());
    else if (strncmp(command, "SHELL ", 6) == 0) {
        char shell_body[MAX_MSG_SIZE];
        // Limit the shell command portion to prevent buffer overflow
        snprintf(shell_body, MAX_MSG_SIZE - 20, "%s", command + 6);
        snprintf(final_command, MAX_MSG_SIZE, "SHELL %d %.*s", getpid(), MAX_MSG_SIZE - 20, shell_body);
    } else {
        // For all other commands, assume it's a shell command and wrap it.
        int prefix_len = snprintf(NULL, 0, "SHELL %d ", getpid());
        int max_cmd_len = MAX_MSG_SIZE - prefix_len;
        snprintf(final_command, MAX_MSG_SIZE, "SHELL %d %.*s", getpid(), max_cmd_len, command);

    }

    // For commands that require a response (LIST, HIDE, UNHIDE, exit, SHELL, and EXIT)
    if ((strncmp(final_command, "LIST", 4) == 0) ||
        (strncmp(final_command, "HIDE", 4) == 0) ||
        (strncmp(final_command, "UNHIDE", 6) == 0) ||
        (strncmp(final_command, "exit", 4) == 0) ||
        (strncmp(final_command, "SHELL", 5) == 0) ||
        (strncmp(final_command, "EXIT", 4) == 0)) {
        mq_send(server_mq, final_command, strlen(final_command) + 1, 0);
        char response[MAX_MSG_SIZE];
    
        struct timespec timeout;
        clock_gettime(CLOCK_REALTIME, &timeout);
        timeout.tv_sec += 2;  // Wait up to 2 seconds
        ssize_t bytes_read = -1;
        while ((bytes_read = mq_timedreceive(client_mq, response, MAX_MSG_SIZE, NULL, &timeout)) == -1 && errno == EAGAIN) {
            usleep(50000);
        }
        if (bytes_read > 0) {
            response[bytes_read] = '\0';
            printf("\n<client-1> [Main Thread -- %09lu]: Received server response\n", main_thread_id);
            printf("===============================================================\n");
            printf("%s\n", response);
        } else {
            perror("<client-1> Failed to receive response");
        }
        // For uppercase EXIT, exit after receiving a response.
        if (strncmp(final_command, "EXIT", 4) == 0)
            break;
        continue;
    }

    // For any other commands, send them without waiting for a reply.
    mq_send(server_mq, final_command, strlen(final_command) + 1, 0);
}


    // Cleanup
    mq_close(server_mq);
    mq_close(client_mq);
    mq_unlink(client_response_queue_name);
    mq_unlink(client_shutdown_queue_name);
    return 0;
}
