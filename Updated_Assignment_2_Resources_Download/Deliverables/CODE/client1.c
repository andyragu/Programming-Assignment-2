#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mqueue.h>
#include <pthread.h>
#include <unistd.h>

#define SERVER_QUEUE "/server_queue"
#define CLIENT_QUEUE_PREFIX "/client_broadcast_"
#define MAX_MSG_SIZE 1024

char client_queue_name[50];  // Stores unique client queue name
mqd_t server_mq, client_mq;  // Message queues

// Function to listen for shutdown messages from the server
void* listen_for_shutdown(void* arg) {
    char buffer[MAX_MSG_SIZE];

    client_mq = mq_open(client_queue_name, O_RDONLY);
    if (client_mq == (mqd_t)-1) {
        perror("mq_open failed for client queue");
        pthread_exit(NULL);
    }

    while (1) {
        ssize_t bytes_read = mq_receive(client_mq, buffer, MAX_MSG_SIZE, NULL);
        if (bytes_read >= 0) {
            buffer[bytes_read] = '\0';
            printf("\n<client-1> [Main Thread]: Received shutdown message: %s\n", buffer);
            exit(0);  // Exit on shutdown message
        }
    }

    mq_close(client_mq);
    pthread_exit(NULL);
}

int main() {
    char command[MAX_MSG_SIZE];
    char prompt[MAX_MSG_SIZE] = "> "; //  Default prompt

    // Open connection to the server queue
    server_mq = mq_open(SERVER_QUEUE, O_WRONLY);
    if (server_mq == (mqd_t)-1) {
        perror("mq_open failed");
        exit(1);
    }

    // Generate a unique client queue name
    sprintf(client_queue_name, "%s%d", CLIENT_QUEUE_PREFIX, getpid());
    mq_unlink(client_queue_name); // Ensure no old queue exists
    struct mq_attr attr = {0, 10, MAX_MSG_SIZE, 0};
    client_mq = mq_open(client_queue_name, O_CREAT | O_RDONLY, 0666, &attr);
    if (client_mq == (mqd_t)-1) {
        perror("mq_open failed for client queue");
        exit(1);
    }

    // Print formatted intro message
    printf("--------------------------------------------------\n");
    printf("####### STUDENTS' REFERENCE SHELL CLIENT #######\n");
    printf("##################################################\n");
    printf("####### I am the Parent Process (PID: %d) running this Client #######\n", getpid());
    printf("--------------------------------------------------\n\n");

    // Send REGISTER command to the server
    sprintf(command, "REGISTER %d", getpid());
    mq_send(server_mq, command, strlen(command) + 1, 0);

    // Create a thread for listening to server shutdown messages
    pthread_t thread_ID;
    pthread_create(&thread_ID, NULL, listen_for_shutdown, NULL);

    // **Ensure Proper Thread ID Formatting**
    unsigned long main_thread_id = pthread_self() % 1000000000;  // 9-digit main thread ID
    unsigned long child_thread_id = (unsigned long)thread_ID;    // 15-digit child thread ID

    // Print startup messages with correctly formatted IDs
    printf("<client-1> [Main Thread -- %09lu]: I am the Client's Main Thread. My Parent Process is (PID: %d)...\n", main_thread_id, getpid());
    printf("\n<client-1> [Main Thread -- %09lu]: Created a Child Thread [%015lu] for listening to the server's SHUTDOWN broadcast message...\n", main_thread_id, child_thread_id);
    printf("\n<client-1> [Main Thread -- %09lu]: Client initialized. Enter commands (type 'EXIT' to quit)...\n", main_thread_id);

    // **Main input loop**
    while (1) {
        printf("\n%sEnter Command: ", prompt); //  Uses updated prompt
        fflush(stdout);  //  Ensures prompt is printed immediately

        fgets(command, MAX_MSG_SIZE, stdin);
        command[strcspn(command, "\n")] = 0; // Remove newline

        // **Handle CHPT new_prompt (Client Side)**
        if (strncmp(command, "CHPT ", 5) == 0) {
            char new_prompt[MAX_MSG_SIZE];
            sscanf(command + 5, "%1022s", new_prompt);  //  Ensures safe input handling
            snprintf(prompt, sizeof(prompt), "%.1021s ", new_prompt);
            printf("Prompt changed to '%s'\n", prompt);
            continue; //  Skip sending to server
        }
        

        if (strcmp(command, "EXIT") == 0) {
            printf("<client-1> [Main Thread -- %09lu]: Exiting...\n", main_thread_id);
            break;
        }

        mq_send(server_mq, command, strlen(command) + 1, 0);
    }

    // Cleanup
    mq_close(server_mq);
    mq_close(client_mq);
    mq_unlink(client_queue_name);

    return 0;
}