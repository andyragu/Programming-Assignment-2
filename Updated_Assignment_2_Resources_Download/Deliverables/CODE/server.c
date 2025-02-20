#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mqueue.h>
#include <pthread.h>
#include <unistd.h>

#define QUEUE_NAME "/server_queue"
#define MAX_MSG_SIZE 1024

void* handle_client(void* arg) {
    char command[MAX_MSG_SIZE];
    strcpy(command, (char*)arg);
    printf("Processing command: %s\n", command);

    // Run shell command
    if (fork() == 0) {
        execlp("/bin/sh", "sh", "-c", command, NULL);
        perror("execlp failed");
        exit(1);
    }

    pthread_exit(NULL);
}

int main() {
    mqd_t mq;
    struct mq_attr attr;
    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = MAX_MSG_SIZE;

    // Create message queue
    mq = mq_open(QUEUE_NAME, O_CREAT | O_RDONLY, 0666, &attr);
    if (mq == (mqd_t)-1) {
        perror("mq_open failed");
        exit(1);
    }

    printf("Server started. Waiting for messages...\n");

    while (1) {
        char buffer[MAX_MSG_SIZE];
        ssize_t bytes_read = mq_receive(mq, buffer, MAX_MSG_SIZE, NULL);
        if (bytes_read >= 0) {
            buffer[bytes_read] = '\0';
            pthread_t thread;
            pthread_create(&thread, NULL, handle_client, strdup(buffer));
            pthread_detach(thread);
        }
    }

    mq_close(mq);
    mq_unlink(QUEUE_NAME);
    return 0;
}
