#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mqueue.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

#define SERVER_QUEUE "/server_queue"
#define CLIENT_RESPONSE_QUEUE_PREFIX "/client2_broadcast_"
#define CLIENT_SHUTDOWN_QUEUE_PREFIX "/client2_shutdown_"
#define MAX_MSG_SIZE 1024

char client_response_queue_name[50];
char client_shutdown_queue_name[50];
mqd_t client_mq;  // Response queue descriptor

// Function to listen for SHUTDOWN messages on a dedicated queue
void* listen_for_shutdown(void* arg) {
    mqd_t shutdown_mq = *((mqd_t*) arg);
    free(arg);
    char buffer[MAX_MSG_SIZE];

    while (1) {
        ssize_t bytes_read = mq_receive(shutdown_mq, buffer, MAX_MSG_SIZE, NULL);
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            if (strcmp(buffer, "SHUTDOWN") == 0) {  
                printf("\n<client-2> [Main Thread]: Received shutdown message: %s\n", buffer);
                exit(0);
            } else {
                printf("\n[Debug] Ignoring non-shutdown message: %s\n", buffer);
            }
        }
    }

    mq_close(shutdown_mq);
    pthread_exit(NULL);
}

int main() {
    char command[MAX_MSG_SIZE];
    char prompt[MAX_MSG_SIZE] = "> ";
    struct mq_attr attr = {0, 10, MAX_MSG_SIZE, 0};

    // Open connection to the server queue
    mqd_t server_mq = mq_open(SERVER_QUEUE, O_WRONLY);
    if (server_mq == (mqd_t)-1) {
        perror("mq_open failed for server queue");
        exit(1);
    }

    // Create unique client response queue (used for command responses)
    sprintf(client_response_queue_name, "%s%d", CLIENT_RESPONSE_QUEUE_PREFIX, getpid());
    mq_unlink(client_response_queue_name);
    client_mq = mq_open(client_response_queue_name, O_CREAT | O_RDONLY | O_NONBLOCK, 0666, &attr);
    if (client_mq == (mqd_t)-1) {
        perror("mq_open failed for client response queue");
        exit(1);
    }

    // Create unique client shutdown queue (used exclusively for shutdown messages)
    sprintf(client_shutdown_queue_name, "%s%d", CLIENT_SHUTDOWN_QUEUE_PREFIX, getpid());
    mq_unlink(client_shutdown_queue_name);
    mqd_t shutdown_mq = mq_open(client_shutdown_queue_name, O_CREAT | O_RDONLY, 0666, &attr);
    if (shutdown_mq == (mqd_t)-1) {
        perror("mq_open failed for client shutdown queue");
        exit(1);
    }

    // Create a thread for listening to shutdown messages
    pthread_t thread_ID;
    mqd_t* shutdown_mq_ptr = malloc(sizeof(mqd_t));
    if (!shutdown_mq_ptr) {
        perror("malloc failed");
        exit(1);
    }
    *shutdown_mq_ptr = shutdown_mq;
    pthread_create(&thread_ID, NULL, listen_for_shutdown, shutdown_mq_ptr);

    // Ensure proper thread ID formatting and print startup messages only once
    unsigned long main_thread_id = pthread_self() % 1000000000;
    unsigned long child_thread_id = (unsigned long)thread_ID;
    printf("<client-2> [Main Thread -- %09lu]: I am the Client's Main Thread. My Parent Process is (PID: %d)...\n",
           main_thread_id, getpid());
    printf("<client-2> [Main Thread -- %09lu]: Created a Child Thread [%015lu] for listening to the server's SHUTDOWN broadcast message...\n",
           main_thread_id, child_thread_id);
    printf("<client-2> [Main Thread -- %09lu]: Client initialized. Enter commands (type 'EXIT' to quit)...\n",
           main_thread_id);

    // Send REGISTER command to the server (including our response queue name)
    {
        char register_command[MAX_MSG_SIZE];
        sprintf(register_command, "REGISTER %d %s", getpid(), client_response_queue_name);
        mq_send(server_mq, register_command, strlen(register_command) + 1, 0);
    }

    // Main input loop
    while (1) {
        printf("\n%sEnter Command: ", prompt);
        fflush(stdout);

        fgets(command, MAX_MSG_SIZE, stdin);
        command[strcspn(command, "\n")] = 0;

        // Handle CHPT command to change prompt
        if (strncmp(command, "CHPT ", 5) == 0) {
            char new_prompt[MAX_MSG_SIZE];
            sscanf(command + 5, "%1022s", new_prompt);
            snprintf(prompt, sizeof(prompt), "%.1021s ", new_prompt);
            printf("Prompt changed to '%s'\n", prompt);
            continue;
        }

        // Handle EXIT command
        if (strcmp(command, "EXIT") == 0) {
            printf("\n==================================================================\n");
            printf("<client-2> [Main Thread -- %09lu]: Gracefully exiting...\n", main_thread_id);
            printf("<client-2> [Main Thread -- %09lu]: Resource cleanup complete...\n", main_thread_id);
            printf("<client-2> [Main Thread -- %09lu]: Shutting down...\n", main_thread_id);
            break;
        }

        // For the LIST command, send the command with our PID and then print the response with the desired format
        if (strcmp(command, "LIST") == 0) {
            char list_command[MAX_MSG_SIZE];
            sprintf(list_command, "LIST %d", getpid());
            mq_send(server_mq, list_command, strlen(list_command) + 1, 0);

            char response[MAX_MSG_SIZE];
            printf("\n<client-2> [Main Thread -- %09lu]: Waiting for server response...\n", main_thread_id);

            struct timespec timeout;
            clock_gettime(CLOCK_REALTIME, &timeout);
            timeout.tv_sec += 2;  // Wait up to 2 seconds

            ssize_t bytes_read;
            while ((bytes_read = mq_timedreceive(client_mq, response, MAX_MSG_SIZE, NULL, &timeout)) == -1 && errno == EAGAIN) {
                printf("<client-2> [Main Thread -- %09lu]: No message yet, retrying...\n", main_thread_id);
                usleep(50000);
            }

            if (bytes_read > 0) {
                response[bytes_read] = '\0';
                printf("\n<client-2> [Main Thread -- %09lu]: Received server response\n", main_thread_id);
                printf("===============================================================\n");
                printf("%s\n", response);
            } else {
                perror("<client-2> Failed to receive LIST response");
            }
            continue;
        }

        // For other commands, send them as is
        mq_send(server_mq, command, strlen(command) + 1, 0);
    }

    // Cleanup resources before exiting
    mq_close(server_mq);
    mq_close(client_mq);
    mq_unlink(client_response_queue_name);
    mq_unlink(client_shutdown_queue_name);

    return 0;
}
