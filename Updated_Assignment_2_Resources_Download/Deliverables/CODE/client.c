#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mqueue.h>
#include <pthread.h>

#define QUEUE_NAME "/server_queue"
#define MAX_MSG_SIZE 1024
#define SEND_QUEUE "/server_queue"
#define RECEIVE_QUEUE "/client_queue"
 
void* thread_manager(void* arg)
{
    mqd_t mq_received;
    char buffer[MAX_MSG_SIZE]; //Store the messages. Storage designated to max size var

    mq_received = mq_open(RECEIVE_QUEUE, O_RDONLY);
    if (mq_received == (mqd_t)-1) {
        perror("mq_open failed to be received");
        pthread_exit(NULL); // Terminates current thread (but allows other threads to keep running)
        return NULL;
    }
}

int main() {
    mqd_t mq;
    char command[MAX_MSG_SIZE];

    mq = mq_open(QUEUE_NAME, O_WRONLY);
    if (mq == (mqd_t)-1) {
        perror("mq_open failed");
        exit(1);
    }

    receiver_thread()
    {
        pthread_t thread_ID = 001;
        int result = pthread_create(&thread_ID, NULL, thread_manager, NULL); 

        //Check for success
        if (result != 0)
        {
            perror("Failed to create thread");
            exit(1)
        }
    }

    while (1) {
        printf("Enter command: ");
        fgets(command, MAX_MSG_SIZE, stdin);
        command[strcspn(command, "\n")] = 0; // Remove newline

        if (strcmp(command, "EXIT") == 0) {
            printf("Exiting client...\n");
            break;
        }

        if (mq_send(mq, command, strlen(command) + 1, 0) == -1) {
            perror("mq_send failed");
        }
    }

    mq_close(mq);
    return 0;
}
