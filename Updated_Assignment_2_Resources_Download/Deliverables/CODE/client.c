#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mqueue.h>

#define QUEUE_NAME "/server_queue"
#define MAX_MSG_SIZE 1024

int main() {
    mqd_t mq;
    char command[MAX_MSG_SIZE];

    mq = mq_open(QUEUE_NAME, O_WRONLY);
    if (mq == (mqd_t)-1) {
        perror("mq_open failed");
        exit(1);
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
