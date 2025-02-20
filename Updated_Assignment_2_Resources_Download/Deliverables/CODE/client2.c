#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mqueue.h>
#include <unistd.h>

#define SERVER_QUEUE "/server_queue"
#define MAX_MSG_SIZE 1024

void listen_for_response() {
    mqd_t mq;
    char buffer[MAX_MSG_SIZE];

    mq = mq_open(SERVER_QUEUE, O_RDONLY);
    if (mq == (mqd_t)-1) {
        perror("mq_open failed");
        return;
    }

    while (1) {
        ssize_t bytes_read = mq_receive(mq, buffer, MAX_MSG_SIZE, NULL);
        if (bytes_read >= 0) {
            buffer[bytes_read] = '\0';
            printf("[Client2] Server Response: %s\n", buffer);
        }
    }

    mq_close(mq);
}

int main() {
    mqd_t mq;
    char command[MAX_MSG_SIZE];

    mq = mq_open(SERVER_QUEUE, O_WRONLY);
    if (mq == (mqd_t)-1) {
        perror("mq_open failed");
        exit(1);
    }

    sprintf(command, "REGISTER %d", getpid());
    mq_send(mq, command, strlen(command) + 1, 0);

    if (fork() == 0) {
        listen_for_response();
    }

    while (1) {
        printf("[Client2] Enter command: ");
        fgets(command, MAX_MSG_SIZE, stdin);
        command[strcspn(command, "\n")] = 0;

        if (strcmp(command, "EXIT") == 0) {
            printf("[Client2] Exiting...\n");
            break;
        }

        mq_send(mq, command, strlen(command) + 1, 0);
    }

    mq_close(mq);
    return 0;
}
